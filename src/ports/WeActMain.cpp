// Minimal ESP32-C5 firmware entry point using MeganeCAN's portable Frame.
// Safe first-stage target: hardware CAN listen-only, serial log, no CAN TX.
#include <Arduino.h>
#include "bus/WeActCan.h"

static uint32_t seen = 0;
static void received(const Frame& frame, void*) {
    ++seen;
    Serial.printf("@RX %lu %s %08lX %u", (unsigned long)millis(),
                  frame.extended ? "EXT" : "STD",
                  (unsigned long)frame.id, (unsigned)frame.len);
    for (uint8_t i = 0; i < frame.len; ++i)
        Serial.printf(" %02X", frame.data[i]);
    Serial.println();
}

void setup() {
    Serial.begin(921600);
    delay(300);
    Serial.println("# MeganeCAN ESP32-C5 SM5 passive target; 500 kbit/s");
    if (!WeActCan::begin(received)) {
        Serial.println("# ERROR: TWAI initialization failed");
        while (true) delay(1000);
    }
    Serial.println("# TWAI listen-only active; CAN transmit disabled");
}

void loop() {
    WeActCan::poll(128);
    static uint32_t last = 0;
    if (millis() - last >= 5000) {
        last = millis();
        Serial.printf("# frames=%lu dropped=%lu\n",
                      (unsigned long)seen, (unsigned long)WeActCan::dropped());
    }
    delay(1);
}
