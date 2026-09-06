#include <Arduino.h>

#include "app_controller.hpp"

static ecu::AppController app;

void setup() {
    app.begin();
}

void loop() {
    app.loop();
}
