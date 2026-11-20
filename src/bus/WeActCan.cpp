#include "WeActCan.h"
#if defined(CONFIG_IDF_TARGET_ESP32C5)
#include <Arduino.h>
#include <esp_timer.h>
#include <esp_twai.h>
#include <esp_twai_onchip.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
constexpr size_t QUEUE_LEN = 256;
struct Slot {
    twai_frame_t frame;
    uint8_t data[TWAIFD_FRAME_MAX_LEN];
    uint64_t received_us;
};
Slot slots[QUEUE_LEN];
SemaphoreHandle_t freeSlots = nullptr;
SemaphoreHandle_t readySlots = nullptr;
volatile size_t writePos = 0;
size_t readPos = 0;
volatile uint32_t lost = 0;
uint32_t skipped = 0;
twai_node_handle_t node = nullptr;
WeActCan::ReceiveFn receiver = nullptr;
void* receiverCtx = nullptr;

bool IRAM_ATTR onReceive(twai_node_handle_t handle,
                         const twai_rx_done_event_data_t*, void*) {
    BaseType_t wake = pdFALSE;
    if (xSemaphoreTakeFromISR(freeSlots, &wake) != pdTRUE) {
        ++lost;
        return wake == pdTRUE;
    }
    Slot& s = slots[writePos];
    if (twai_node_receive_from_isr(handle, &s.frame) == ESP_OK) {
        s.received_us = static_cast<uint64_t>(esp_timer_get_time());
        writePos = (writePos + 1) % QUEUE_LEN;
        xSemaphoreGiveFromISR(readySlots, &wake);
    } else {
        ++lost;
        xSemaphoreGiveFromISR(freeSlots, &wake);
    }
    return wake == pdTRUE;
}
}

namespace WeActCan {
bool begin(ReceiveFn callback, void* context) {
    if (!callback || node) return false;
    freeSlots = xSemaphoreCreateCounting(QUEUE_LEN, QUEUE_LEN);
    readySlots = xSemaphoreCreateCounting(QUEUE_LEN, 0);
    if (!freeSlots || !readySlots) return false;
    for (auto& s : slots) {
        s.frame.buffer = s.data;
        s.frame.buffer_len = sizeof(s.data);
    }
    receiver = callback;
    receiverCtx = context;
    twai_onchip_node_config_t config = {};
    config.io_cfg.tx = GPIO_NUM_23;
    config.io_cfg.rx = GPIO_NUM_24;
    config.io_cfg.quanta_clk_out = GPIO_NUM_NC;
    config.io_cfg.bus_off_indicator = GPIO_NUM_NC;
    config.bit_timing.bitrate = 500000;
    config.bit_timing.sp_permill = 875;
    config.data_timing.bitrate = 5000000;
    config.data_timing.sp_permill = 625;
    config.flags.enable_listen_only = true;
    if (twai_new_node_onchip(&config, &node) != ESP_OK) {
        node = nullptr;
        return false;
    }
    twai_event_callbacks_t callbacks = {};
    callbacks.on_rx_done = onReceive;
    if (twai_node_register_event_callbacks(node, &callbacks, nullptr) != ESP_OK ||
        twai_node_enable(node) != ESP_OK) {
        twai_node_delete(node);
        node = nullptr;
        return false;
    }
    return true;
}

unsigned poll(unsigned maxFrames) {
    unsigned consumed = 0;
    if (!node) return 0;
    while (consumed < maxFrames && xSemaphoreTake(readySlots, 0) == pdTRUE) {
        Slot& s = slots[readPos];
        const twai_frame_t& raw = s.frame;
        if (!raw.header.fdf && raw.header.dlc <= 8) {
            Frame f{};
            f.id = raw.header.id;
            f.extended = raw.header.ide;
            f.len = raw.header.dlc;
            f.source = Frame::SRC_MM_CAN;
            for (unsigned i = 0; i < f.len; ++i) f.data[i] = s.data[i];
            receiver(f, s.received_us, receiverCtx);
        } else {
            ++skipped;
        }
        ++consumed;
        readPos = (readPos + 1) % QUEUE_LEN;
        xSemaphoreGive(freeSlots);
    }
    return consumed;
}
uint32_t dropped() { return lost; }
uint32_t unsupported() { return skipped; }
}
#else
namespace WeActCan {
bool begin(ReceiveFn, void*) { return false; }
unsigned poll(unsigned) { return 0; }
uint32_t dropped() { return 0; }
uint32_t unsupported() { return 0; }
}
#endif
