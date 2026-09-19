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

// Names come back from the card as "/20260910_1732_04.emulog"; the page and
// the URLs use them without the leading slash.
const char* bareName(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash != nullptr ? slash + 1 : path;
}

// Rejects anything that could walk out of the card's root. The only names this
// server will open are plain log filenames.
bool safeName(const String& name) {
    if (name.length() == 0 || name.length() > 60) {
        return false;
    }
    if (name.indexOf('/') >= 0 || name.indexOf('\\') >= 0 || name.indexOf("..") >= 0) {
        return false;
    }
    return isLogName(name.c_str());
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

bool ServicePage::writable() {
    const bool running = ctx_.model != nullptr &&
                         ctx_.model->snapshot().rpm >= kEngineRunningRpm;
    if (!running) {
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
    appendJsonString(out, log.ready() ? bareName(log.fileName()) : "");
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

void ServicePage::handleLogs() {
    String out;
    out.reserve(1024);
    out += '{';

    // The file being written is named so the page can grey it out. It is never
    // offered for download or deletion: it is still open and incomplete.
    out += "\"active\":";
    appendJsonString(out, ctx_.log->ready() ? bareName(ctx_.log->fileName()) : "");
    out += ",\"files\":[";

    File root = SD.open("/");
    bool first = true;
    uint32_t total = 0;
    if (root) {
        for (File entry = root.openNextFile(); entry; entry = root.openNextFile()) {
            const char* name = bareName(entry.name());
            if (entry.isDirectory() || !isLogName(name)) {
                entry.close();
                continue;
            }
            if (!first) {
                out += ',';
            }
            first = false;
            out += "{\"name\":";
            appendJsonString(out, name);
            out += ",\"size\":" + String(static_cast<uint32_t>(entry.size()));
            out += '}';
            total += static_cast<uint32_t>(entry.size());
            entry.close();
        }
        root.close();
    }

    out += "],\"totalBytes\":" + String(total);
    out += ",\"cardBytes\":" + String(static_cast<uint32_t>(SD.totalBytes() / 1024));
    out += ",\"usedBytes\":" + String(static_cast<uint32_t>(SD.usedBytes() / 1024));
    out += '}';

    server_->send(200, "application/json", out);
}

bool ServicePage::sendFile(const char* name) {
    char path[72];
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

    char path[72];
    snprintf(path, sizeof(path), "/%s", name.c_str());
    File probe = SD.open(path, FILE_READ);
    if (!probe) {
        server_->send(404, "text/plain", "No such file on the card.");
        return;
    }
    const uint32_t size = static_cast<uint32_t>(probe.size());
    probe.close();

    server_->setContentLength(size);
    server_->sendHeader("Content-Disposition",
                        String("attachment; filename=\"") + name + "\"");
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
        char path[72];
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

    server_->setContentLength(total);
    server_->sendHeader("Content-Disposition", "attachment; filename=\"emulogs.tar\"");
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

        char path[72];
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
                             ? bareName(ctx_.log->fileName())
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

        char path[72];
        snprintf(path, sizeof(path), "/%s", name.c_str());
        if (SD.remove(path)) {
            ++deleted;
            Serial.print(F("service: deleted "));
            Serial.println(name);
        } else {
            ++refused;
        }
    }

    String out = "{\"deleted\":" + String(deleted) + ",\"refused\":" + String(refused) + "}";
    server_->send(200, "application/json", out);
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
