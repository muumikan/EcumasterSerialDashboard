#include "service_page.hpp"

#include <Arduino.h>
#include <SD.h>
#include <WebServer.h>
#include <lvgl.h>
#include <string.h>

#include "ecu_link.hpp"
#include "setup_items.hpp"

namespace ecu {

extern const char kServicePageHtml[];

namespace {

// Read from the card and handed to the client in blocks this size. Big enough
// that the per-block overhead disappears, small enough that the UI gets a turn
// often - at SPI speeds this is a few milliseconds of card time.
constexpr size_t kBlockSize = 4096;

constexpr char kEmuLogSuffix[] = ".emulog";

bool isLogName(const char* name) {
    const size_t len = strlen(name);
    const size_t suffix = sizeof(kEmuLogSuffix) - 1;
    return len > suffix && strcmp(name + len - suffix, kEmuLogSuffix) == 0;
}

// Paths come back from the card as "/20260910/1732_04.emulog"; the page and
// the URLs use them relative to the root, so the day folder stays part of the
// name and one string identifies a log everywhere.
const char* cardName(const char* path) {
    return path != nullptr && path[0] == '/' ? path + 1 : path;
}

bool isDayFolder(const char* name) {
    if (strlen(name) != 8) {
        return false;
    }
    for (uint8_t i = 0; i < 8; ++i) {
        if (name[i] < '0' || name[i] > '9') {
            return false;
        }
    }
    return true;
}

// Rejects anything that could walk out of the card's root. Two shapes are
// accepted and nothing else: a log in the root, left over from before logs
// were filed by day, and a log inside a day folder. In particular the folder
// half has to be eight digits, so no request can name a directory this
// dashboard did not create.
bool safeName(const String& name) {
    if (name.length() == 0 || name.length() > 70) {
        return false;
    }
    if (name.indexOf('\\') >= 0 || name.indexOf("..") >= 0) {
        return false;
    }

    const int slash = name.indexOf('/');
    if (slash >= 0) {
        if (slash != 8 || name.indexOf('/', slash + 1) >= 0) {
            return false;   // one folder deep, and the folder is a date
        }
        char day[9];
        for (uint8_t i = 0; i < 8; ++i) {
            day[i] = name.charAt(i);
        }
        day[8] = '\0';
        if (!isDayFolder(day)) {
            return false;
        }
    }

    // slash is -1 for a root-level log, which puts this back at the start of
    // the string - the shape that has to keep working for what is already on
    // the card.
    return isLogName(name.c_str() + slash + 1);
}

void appendJsonString(String& out, const char* value) {
    out += '"';
    for (const char* p = value; *p != '\0'; ++p) {
        if (*p == '"' || *p == '\\') {
            out += '\\';
        }
        out += *p;
    }
    out += '"';
}

// ---- tar ----------------------------------------------------------------
// Plain ustar. The files are already gzip, so there is nothing to gain by
// compressing the bundle and a lot to lose by needing a second compressor in
// RAM beside the logger's.

constexpr size_t kTarBlock = 512;

void octal(char* field, size_t width, uint32_t value) {
    // Width includes the trailing NUL that ustar expects on these fields.
    for (size_t i = width - 1; i > 0; --i) {
        field[i - 1] = static_cast<char>('0' + (value & 7));
        value >>= 3;
    }
    field[width - 1] = '\0';
}

void tarHeader(uint8_t* block, const char* name, uint32_t size) {
    memset(block, 0, kTarBlock);
    strncpy(reinterpret_cast<char*>(block), name, 99);
    octal(reinterpret_cast<char*>(block + 100), 8, 0644);       // mode
    octal(reinterpret_cast<char*>(block + 108), 8, 0);          // uid
    octal(reinterpret_cast<char*>(block + 116), 8, 0);          // gid
    octal(reinterpret_cast<char*>(block + 124), 12, size);
    octal(reinterpret_cast<char*>(block + 136), 12, 0);         // mtime
    block[156] = '0';                                           // regular file
    memcpy(block + 257, "ustar", 5);
    memcpy(block + 263, "00", 2);

    // The checksum is computed with its own field read as spaces.
    memset(block + 148, ' ', 8);
    uint32_t sum = 0;
    for (size_t i = 0; i < kTarBlock; ++i) {
        sum += block[i];
    }
    octal(reinterpret_cast<char*>(block + 148), 7, sum);
    block[155] = ' ';
}

}  // namespace

ServicePage::ServicePage() = default;

ServicePage::~ServicePage() {
    delete server_;
}

void ServicePage::begin(const ServiceContext& context) { ctx_ = context; }

void ServicePage::loop(bool apServing) {
    if (apServing && !running_) {
        if (server_ == nullptr) {
            server_ = new WebServer(80);
            install();
        }
        server_->begin();
        running_ = true;
        Serial.println(F("service: page at http://192.168.4.1/"));
    } else if (!apServing && running_) {
        server_->stop();
        running_ = false;
    }

    if (running_) {
        server_->handleClient();
    }
}

void ServicePage::install() {
    server_->on("/", HTTP_GET, [this]() { handleIndex(); });
    server_->on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    server_->on("/api/logs", HTTP_GET, [this]() { handleLogs(); });
    server_->on("/log", HTTP_GET, [this]() { handleDownload(); });
    server_->on("/all.tar", HTTP_GET, [this]() { handleTar(); });
    server_->on("/api/delete", HTTP_POST, [this]() { handleDelete(); });
    server_->on("/api/clock", HTTP_POST, [this]() { handleClock(); });
    server_->on("/api/session", HTTP_POST, [this]() { handleSession(); });
    server_->on("/api/settings", HTTP_GET, [this]() { handleSettingsGet(); });
    server_->on("/api/settings", HTTP_POST, [this]() { handleSettingsPost(); });
    server_->on("/api/events", HTTP_GET, [this]() { handleEvents(); });
    server_->on("/api/reboot", HTTP_POST, [this]() { handleReboot(); });
    server_->onNotFound([this]() { handleNotFound(); });
}

// Every endpoint that changes something on the dashboard goes through here.
//
// The test is engineStopped(), not the raw rpm field. Cutting the ignition
// cuts the ECU's power mid-frame, so the snapshot keeps reporting the last rpm
// it ever saw - and a dashboard that had been idling at 850 rpm when the key
// turned then refused every delete and every setting until it was rebooted,
// while cheerfully serving this page. See engine_data_model.hpp.
bool ServicePage::writable() {
    const bool stopped =
        ctx_.model == nullptr || engineStopped(*ctx_.model, millis());
    if (stopped) {
        return true;
    }
    server_->send(409, "text/plain",
                  "The engine is running. Stop it before changing anything.");
    return false;
}

void ServicePage::handleIndex() {
    server_->sendHeader("Cache-Control", "no-store");
    server_->send_P(200, "text/html", kServicePageHtml);
}

void ServicePage::handleNotFound() {
    server_->send(404, "text/plain", "No such page on this dashboard.");
}

void ServicePage::handleStatus() {
    const uint32_t nowMs = millis();
    const EmuLog& log = *ctx_.log;

    String out;
    out.reserve(768);
    out += '{';

    out += "\"build\":";
    appendJsonString(out, __DATE__ " " __TIME__);

    out += ",\"link\":";
    appendJsonString(out, toString(ctx_.model->linkState(nowMs)));
    out += ",\"linkName\":";
    appendJsonString(out, kEcuLinkName);
    out += ",\"baud\":" + String(kEcuLinkBaud);
    out += ",\"rpm\":" + String(ctx_.model->snapshot().rpm);
    out += ",\"batteryV\":" + String(ctx_.model->snapshot().batteryV, 1);
    out += ",\"updates\":" + String(ctx_.model->updateCount());
    out += ",\"badFrames\":" + String(ctx_.model->badFrames());

    out += ",\"logging\":";
    out += log.ready() ? "true" : "false";
    out += ",\"logFile\":";
    appendJsonString(out, log.ready() ? cardName(log.fileName()) : "");
    out += ",\"logFailure\":";
    appendJsonString(out, log.failure() != nullptr ? log.failure() : "");
    out += ",\"frames\":" + String(log.framesWritten());
    out += ",\"logBytes\":" + String(log.bytesWritten());
    out += ",\"dropped\":" + String(log.framesDropped());
    out += ",\"session\":";
    appendJsonString(out, log.sessionName());

    char stamp[9] = "--:--:--";
    if (ctx_.rtc->present()) {
        formatHms(ctx_.rtc->now(), stamp, sizeof(stamp));
    }
    out += ",\"clock\":";
    appendJsonString(out, stamp);
    out += ",\"rtc\":";
    out += ctx_.rtc->present() ? "true" : "false";

    out += ",\"clients\":" + String(ctx_.ap->clients());
    out += ",\"idleLeft\":" + String(ctx_.ap->idleSecondsLeft(nowMs));
    out += ",\"uptime\":" + String(nowMs / 1000);
    out += ",\"heap\":" + String(ESP.getFreeHeap());
    out += ",\"psram\":" + String(ESP.getFreePsram());

    // LVGL's own pool, which is a fixed array and not part of the heap above.
    // It is the one that has actually run out on this board.
    lv_mem_monitor_t lv = {};
    lv_mem_monitor(&lv);
    out += ",\"lvFree\":" + String(static_cast<uint32_t>(lv.free_size));
    out += ",\"lvTotal\":" + String(static_cast<uint32_t>(lv.total_size));
    out += ",\"lvUsedPct\":" + String(lv.used_pct);
    out += '}';

    server_->send(200, "application/json", out);
}

// Appends one file's entry and keeps the reply moving out in pieces. A season
// of logs is several hundred entries, and building all of it in one String
// would be tens of kilobytes of heap held while the radio is up.
void ServicePage::appendLog(String& out, const char* name, uint32_t size,
                            bool& first, uint32_t& total) {
    if (!first) {
        out += ',';
    }
    first = false;
    out += "{\"name\":";
    appendJsonString(out, name);
    out += ",\"size\":" + String(size);
    out += '}';
    total += size;

    if (out.length() >= kChunkFlush) {
        server_->sendContent(out);
        out = "";
    }
}

void ServicePage::handleLogs() {
    server_->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_->send(200, "application/json", "");

    String out;
    out.reserve(kChunkFlush + 256);
    out += '{';

    // The file being written is named so the page can grey it out. It is never
    // offered for download or deletion: it is still open and incomplete.
    out += "\"active\":";
    appendJsonString(out, ctx_.log->ready() ? cardName(ctx_.log->fileName()) : "");
    out += ",\"files\":[";

    bool first = true;
    uint32_t total = 0;

    // Two levels, and deliberately only two: the day folders this dashboard
    // writes, plus whatever sits in the root from before it did. A general
    // recursive walk would follow anything a laptop had left on the card.
    File root = SD.open("/");
    if (root) {
        for (File entry = root.openNextFile(); entry; entry = root.openNextFile()) {
            if (entry.isDirectory()) {
                if (isDayFolder(entry.name())) {
                    for (File file = entry.openNextFile(); file;
                         file = entry.openNextFile()) {
                        if (!file.isDirectory() && isLogName(file.name())) {
                            appendLog(out, cardName(file.path()),
                                      static_cast<uint32_t>(file.size()), first, total);
                        }
                        file.close();
                    }
                }
            } else if (isLogName(entry.name())) {
                appendLog(out, cardName(entry.path()),
                          static_cast<uint32_t>(entry.size()), first, total);
            }
            entry.close();
            if (ctx_.pump != nullptr) {
                ctx_.pump(ctx_.context);
            }
        }
        root.close();
    }

    out += "],\"totalBytes\":" + String(total);
    out += ",\"cardBytes\":" + String(static_cast<uint32_t>(SD.totalBytes() / 1024));
    out += ",\"usedBytes\":" + String(static_cast<uint32_t>(SD.usedBytes() / 1024));
    out += '}';

    server_->sendContent(out);
    server_->sendContent("");
}

bool ServicePage::sendFile(const char* name) {
    char path[80];
    snprintf(path, sizeof(path), "/%s", name);

    File file = SD.open(path, FILE_READ);
    if (!file) {
        return false;
    }

    uint8_t block[kBlockSize];
    WiFiClient client = server_->client();
    while (file.available() > 0 && client.connected()) {
        const int read = file.read(block, sizeof(block));
        if (read <= 0) {
            break;
        }
        client.write(block, static_cast<size_t>(read));
        if (ctx_.pump != nullptr) {
            ctx_.pump(ctx_.context);
        }
    }
    file.close();
    return true;
}

void ServicePage::handleDownload() {
    const String name = server_->arg("name");
    if (!safeName(name)) {
        server_->send(400, "text/plain", "Not a log filename.");
        return;
    }

    char path[80];
    snprintf(path, sizeof(path), "/%s", name.c_str());
    File probe = SD.open(path, FILE_READ);
    if (!probe) {
        server_->send(404, "text/plain", "No such file on the card.");
        return;
    }
    const uint32_t size = static_cast<uint32_t>(probe.size());
    probe.close();

    // The name carries its day folder, and a slash in Content-Disposition is
    // either refused or silently cut back to the leaf - which would put half a
    // dozen files called 1732_04.emulog in one download folder. Flattened to
    // an underscore it is the name these files had before they were filed by
    // day, which is also what the laptop wants to see.
    String download = name;
    download.replace('/', '_');

    server_->setContentLength(size);
    server_->sendHeader("Content-Disposition",
                        String("attachment; filename=\"") + download + "\"");
    server_->send(200, "application/octet-stream", "");
    sendFile(name.c_str());
}

// Several files in one download. The browser can only be given one file at a
// time, and ten selected logs would otherwise be ten separate saves.
void ServicePage::handleTar() {
    const String names = server_->arg("names");
    if (names.length() == 0) {
        server_->send(400, "text/plain", "Nothing selected.");
        return;
    }

    // Two passes: the length has to be known before the first byte goes out,
    // because chunked encoding here would mean holding the whole bundle.
    uint32_t total = 0;
    int start = 0;
    while (start < static_cast<int>(names.length())) {
        int comma = names.indexOf(',', start);
        if (comma < 0) {
            comma = names.length();
        }
        const String name = names.substring(start, comma);
        start = comma + 1;
        if (!safeName(name)) {
            continue;
        }
        char path[80];
        snprintf(path, sizeof(path), "/%s", name.c_str());
        File entry = SD.open(path, FILE_READ);
        if (!entry) {
            continue;
        }
        const uint32_t size = static_cast<uint32_t>(entry.size());
        entry.close();
        total += kTarBlock + ((size + kTarBlock - 1) / kTarBlock) * kTarBlock;
    }
    total += 2 * kTarBlock;   // the end-of-archive marker

    // Named for the moment it was fetched, not for its contents. A tuning day
    // means fetching the card several times, and half a dozen files all called
    // emulogs.tar in one download folder is a puzzle nobody wants to solve
    // later. The dashboard's clock is the one both ends have agreed on - the
    // page sets it from the laptop - so it is the one that names the bundle.
    char bundle[40] = "emulogs.tar";
    if (ctx_.rtc != nullptr && ctx_.rtc->present()) {
        const DateTime now = ctx_.rtc->now();
        if (now.valid) {
            snprintf(bundle, sizeof(bundle), "emulogs_%04u%02u%02u_%02u%02u.tar",
                     now.year, now.month, now.day, now.hour, now.minute);
        }
    }

    server_->setContentLength(total);
    server_->sendHeader("Content-Disposition",
                        String("attachment; filename=\"") + bundle + "\"");
    server_->send(200, "application/x-tar", "");

    WiFiClient client = server_->client();
    uint8_t block[kTarBlock];

    start = 0;
    while (start < static_cast<int>(names.length()) && client.connected()) {
        int comma = names.indexOf(',', start);
        if (comma < 0) {
            comma = names.length();
        }
        const String name = names.substring(start, comma);
        start = comma + 1;
        if (!safeName(name)) {
            continue;
        }

        char path[80];
        snprintf(path, sizeof(path), "/%s", name.c_str());
        File entry = SD.open(path, FILE_READ);
        if (!entry) {
            continue;
        }
        const uint32_t size = static_cast<uint32_t>(entry.size());
        entry.close();

        tarHeader(block, name.c_str(), size);
        client.write(block, kTarBlock);

        sendFile(name.c_str());

        const uint32_t padding = (kTarBlock - (size % kTarBlock)) % kTarBlock;
        if (padding > 0) {
            memset(block, 0, padding);
            client.write(block, padding);
        }
    }

    memset(block, 0, kTarBlock);
    client.write(block, kTarBlock);
    client.write(block, kTarBlock);
}

// Deletion takes the names to delete, one by one. There is deliberately no
// "delete everything" form: the page asks the driver to confirm a list, and a
// list is what the server acts on.
void ServicePage::handleDelete() {
    if (!writable()) {
        return;
    }

    const String names = server_->arg("names");
    const char* active = ctx_.log != nullptr && ctx_.log->ready()
                             ? cardName(ctx_.log->fileName())
                             : "";

    uint16_t deleted = 0;
    uint16_t refused = 0;
    int start = 0;
    while (start < static_cast<int>(names.length())) {
        int comma = names.indexOf(',', start);
        if (comma < 0) {
            comma = names.length();
        }
        const String name = names.substring(start, comma);
        start = comma + 1;

        if (!safeName(name)) {
            ++refused;
            continue;
        }
        // The file being written is still open. Deleting it would leave the
        // logger writing to a handle with no directory entry.
        if (name == active) {
            ++refused;
            continue;
        }

        char path[80];
        snprintf(path, sizeof(path), "/%s", name.c_str());
        if (SD.remove(path)) {
            ++deleted;
            Serial.print(F("service: deleted "));
            Serial.println(name);
        } else {
            ++refused;
        }
    }

    if (deleted > 0) {
        pruneEmptyDays();
    }

    String out = "{\"deleted\":" + String(deleted) + ",\"refused\":" + String(refused) + "}";
    server_->send(200, "application/json", out);
}

// A day folder whose last log has been deleted is litter, and the page has no
// way to remove one - it only ever names files. Emptying one is the only thing
// that can leave it behind, so this runs after a delete and nowhere else.
void ServicePage::pruneEmptyDays() {
    File root = SD.open("/");
    if (!root) {
        return;
    }

    // Collected first, removed after: rmdir inside the walk would be deleting
    // the entry the directory handle is standing on.
    char empty[kMaxPrune][12];
    uint8_t count = 0;

    for (File entry = root.openNextFile(); entry && count < kMaxPrune;
         entry = root.openNextFile()) {
        if (entry.isDirectory() && isDayFolder(entry.name())) {
            File first = entry.openNextFile();
            const bool bare = !first;
            if (first) {
                first.close();
            }
            if (bare) {
                snprintf(empty[count], sizeof(empty[count]), "/%s", entry.name());
                ++count;
            }
        }
        entry.close();
    }
    root.close();

    for (uint8_t i = 0; i < count; ++i) {
        if (SD.rmdir(empty[i])) {
            Serial.print(F("service: removed empty folder "));
            Serial.println(empty[i]);
        }
    }
}

void ServicePage::handleClock() {
    if (!writable()) {
        return;
    }

    DateTime t;
    t.year = static_cast<uint16_t>(server_->arg("year").toInt());
    t.month = static_cast<uint8_t>(server_->arg("month").toInt());
    t.day = static_cast<uint8_t>(server_->arg("day").toInt());
    t.hour = static_cast<uint8_t>(server_->arg("hour").toInt());
    t.minute = static_cast<uint8_t>(server_->arg("minute").toInt());
    t.second = static_cast<uint8_t>(server_->arg("second").toInt());
    t.valid = t.year >= 2024 && t.year < 2100 && t.month >= 1 && t.month <= 12 &&
              t.day >= 1 && t.day <= 31 && t.hour < 24 && t.minute < 60 && t.second < 60;

    if (!t.valid) {
        server_->send(400, "text/plain", "That is not a time this clock can hold.");
        return;
    }
    if (!ctx_.rtc->setTime(t)) {
        server_->send(500, "text/plain", "The clock did not accept the time.");
        return;
    }

    Serial.println(F("service: clock set from the browser"));
    server_->send(200, "application/json", "{\"ok\":true}");
}

void ServicePage::handleSession() {
    if (!writable()) {
        return;
    }
    ctx_.log->setSessionName(server_->arg("name").c_str());

    String out = "{\"session\":";
    appendJsonString(out, ctx_.log->sessionName());
    out += '}';
    server_->send(200, "application/json", out);
}

// The form is generated from the same table the panel reads, so there is one
// list of what a driver may change rather than two that drift apart.
void ServicePage::handleSettingsGet() {
    const DashSettings& live = *ctx_.settings;
    const DashSettings defaults = defaultDashSettings();

    String out;
    out.reserve(4096);
    out += "{\"categories\":[";

    for (uint8_t c = 0; c < kSetupCategoryCount; ++c) {
        const SetupCategory& group = kSetupCategories[c];
        if (c > 0) {
            out += ',';
        }
        out += "{\"name\":";
        appendJsonString(out, group.name);
        out += ",\"items\":[";

        for (uint8_t i = 0; i < group.count; ++i) {
            const SetupItem& item = group.items[i];
            if (i > 0) {
                out += ',';
            }
            out += "{\"key\":";
            appendJsonString(out, item.name);
            out += ",\"unit\":";
            appendJsonString(out, item.unit);
            out += ",\"bool\":";
            out += item.type == SetupType::Bool ? "true" : "false";
            out += ",\"value\":" + String(readSetting(live, item), static_cast<int>(item.decimals));
            out += ",\"default\":" + String(readSetting(defaults, item), static_cast<int>(item.decimals));
            out += ",\"min\":" + String(item.minValue, static_cast<int>(item.decimals));
            out += ",\"max\":" + String(item.maxValue, static_cast<int>(item.decimals));
            out += ",\"step\":" + String(item.step, static_cast<int>(item.decimals));
            out += ",\"decimals\":" + String(item.decimals);
            out += '}';
        }
        out += "]}";
    }
    out += "]}";

    server_->send(200, "application/json", out);
}

void ServicePage::handleSettingsPost() {
    if (!writable()) {
        return;
    }

    // Addressed by category and item index rather than by name: the indices
    // come from the table this server just served, and a name would have to be
    // matched against that same table anyway.
    const uint8_t category = static_cast<uint8_t>(server_->arg("category").toInt());
    const uint8_t index = static_cast<uint8_t>(server_->arg("index").toInt());

    if (category >= kSetupCategoryCount || index >= kSetupCategories[category].count) {
        server_->send(400, "text/plain", "No such setting.");
        return;
    }

    const SetupItem& item = kSetupCategories[category].items[index];

    // Clamped by the item's own range, exactly as a button press is. A value
    // typed into a form is no more trusted than one stepped on the panel.
    const float value = clampSetting(item, server_->arg("value").toFloat());
    writeSetting(*ctx_.settings, item, value);

    if (ctx_.onSettingsChanged != nullptr) {
        ctx_.onSettingsChanged(ctx_.context);
    }

    String out = "{\"value\":" + String(value, static_cast<int>(item.decimals)) + '}';
    server_->send(200, "application/json", out);
}

void ServicePage::handleEvents() {
    String out;
    out.reserve(1024);
    out += '{';

    if (ctx_.alarms == nullptr) {
        // The alarm engine lives with the UI. On a dashboard whose display
        // failed there is nothing to report, and saying so beats an empty list
        // that looks like a clean run.
        out += "\"available\":false,\"events\":[]}";
        server_->send(200, "application/json", out);
        return;
    }

    const EventLog& events = ctx_.alarms->events();
    out += "\"available\":true,\"total\":" + String(events.total());
    out += ",\"capacity\":" + String(EventLog::kCapacity);
    out += ",\"events\":[";

    for (uint8_t i = 0; i < events.count(); ++i) {
        const AlarmEvent& e = events.at(i);
        if (i > 0) {
            out += ',';
        }
        char stamp[9];
        formatHms(e.time, stamp, sizeof(stamp));

        out += "{\"time\":";
        appendJsonString(out, stamp);
        out += ",\"label\":";
        appendJsonString(out, e.label);
        out += ",\"value\":";
        appendJsonString(out, e.value);
        out += ",\"rpm\":" + String(e.rpm);
        out += ",\"active\":";
        out += e.active ? "true" : "false";
        out += ",\"severity\":";
        appendJsonString(out, e.severity == AlarmSeverity::Critical ? "crit"
                              : e.severity == AlarmSeverity::Warning ? "warn"
                                                                     : "info");
        out += ",\"seconds\":" +
               String(e.active ? 0 : (e.endMs - e.startMs) / 1000);
        out += '}';
    }
    out += "]}";

    server_->send(200, "application/json", out);
}

void ServicePage::handleReboot() {
    if (!writable()) {
        return;
    }
    server_->send(200, "application/json", "{\"ok\":true}");

    // Let the response reach the browser before the board goes away.
    delay(200);
    ESP.restart();
}

}  // namespace ecu
