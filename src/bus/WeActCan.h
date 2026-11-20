#pragma once
// ESP32-C5 on-chip TWAI passive receiver. No transmit API is exposed.
#include <stdint.h>
#include "bus/Frame.h"

namespace WeActCan {
using ReceiveFn = void (*)(const Frame&, uint64_t received_us, void* context);

// Start the hardware in silent/listen-only mode.
bool begin(ReceiveFn callback, void* context = nullptr);
// Drain at most maxFrames queued records; returns records consumed.
unsigned poll(unsigned maxFrames = 64);
// Queue overflow or hardware receive failures.
uint32_t dropped();
// Received frames skipped because they are not classic CAN.
uint32_t unsupported();
}
