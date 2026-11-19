#pragma once
// ESP32-C5 on-chip TWAI receiver adapter for MeganeCAN's portable Frame.
// Target: WeActStudio CANFDDevBoard V1.0; nominal 500 kbit/s.
// Must be built against Arduino-ESP32 / ESP-IDF with esp_twai_onchip.h.
#include <stdint.h>
#include "bus/Frame.h"

namespace WeActCan {
using ReceiveFn = void (*)(const Frame&, void* context);

// Initializes hardware in silent/listen-only mode; never transmits.
bool begin(ReceiveFn callback, void* context = nullptr);

// Call regularly from an application task, never from an ISR.
// Returns number of CAN frames delivered to callback.
unsigned poll(unsigned maxFrames = 64);

// Number of frames discarded when the ISR ring buffer was full.
uint32_t dropped();
}
