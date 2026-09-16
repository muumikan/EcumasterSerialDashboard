#pragma once

#include <stdint.h>

#include "alarm_engine.hpp"
#include "dash_settings.hpp"
#include "emu_log.hpp"
#include "engine_data_model.hpp"
#include "rtc_clock.hpp"
#include "service_ap.hpp"

class WebServer;

namespace ecu {

// Everything the service page is allowed to touch, wired once by the
// controller. A struct rather than eight constructor arguments, and a struct
// of borrowed pointers rather than a back-reference to AppController: what is
// on this list is what the page can reach.
struct ServiceContext {
    EmuLog* log = nullptr;
    const EngineDataModel* model = nullptr;
    RtcClock* rtc = nullptr;
    const ServiceAp* ap = nullptr;

    // Null when the display failed to come up, since the alarm engine lives
    // with the UI. The page drops the alarm tab rather than inventing one.
    const AlarmEngine* alarms = nullptr;

    DashSettings* settings = nullptr;

    // Called after the page edits the settings, so the owner applies and saves
    // them by exactly the same route the panel uses.
    void (*onSettingsChanged)(void* context) = nullptr;

    // Called between blocks of a file download. Sending a few megabytes is
    // seconds of work, and without this the screen would be frozen for all of
    // it - so the owner gets to run LVGL while the card is being read.
    void (*pump)(void* context) = nullptr;

    void* context = nullptr;
};

// The maintenance page the dashboard serves while its access point is up.
//
// Read-mostly: the logs, the state the console would print, and the same
// settings the panel edits. Writes are POSTs and are refused while the engine
// is running - which cannot happen anyway, since the access point is down by
// then, but the server does not depend on that to be safe.
class ServicePage {
public:
    ServicePage();
    ~ServicePage();

    void begin(const ServiceContext& context);

    // Starts and stops with the access point.
    void loop(bool apServing);

private:
    void install();

    void handleIndex();
    void handleStatus();
    void handleLogs();
    void handleDownload();
    void handleTar();
    void handleDelete();
    void handleClock();
    void handleSession();
    void handleSettingsGet();
    void handleSettingsPost();
    void handleEvents();
    void handleReboot();
    void handleNotFound();

    // True when a write may proceed. Refuses with 409 and explains if not.
    bool writable();

    // Sends one file from the card, in blocks, pumping the UI between them.
    // Returns false if the file could not be opened.
    bool sendFile(const char* name);

    ServiceContext ctx_;
    WebServer* server_ = nullptr;
    bool running_ = false;
};

}  // namespace ecu
