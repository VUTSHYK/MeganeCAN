// WeActStudio ESP32-C5 passive CAN capture for Renault SM5.
// Based on the manufacturer's CAN_TWAI_Listen example.
// Build with a recent Arduino-ESP32/ESP-IDF exposing esp_twai_onchip.h.
// This target deliberately has no CAN transmission code.
#include <Arduino.h>
#include <esp_twai.h>
#include <esp_twai_onchip.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
constexpr gpio_num_t RX_PIN = GPIO_NUM_24;
constexpr gpio_num_t TX_PIN = GPIO_NUM_23;
constexpr uint32_t BITRATE = 500000;
constexpr size_t POOL_SIZE = 256;

struct Slot {
    twai_frame_t frame;
    uint8_t bytes[TWAIFD_FRAME_MAX_LEN];
    uint64_t received_us;
};
struct RxContext {
    Slot slots[POOL_SIZE];
    SemaphoreHandle_t free_slots = nullptr;
    SemaphoreHandle_t ready_slots = nullptr;
    volatile size_t write_index = 0;
    size_t read_index = 0;
    volatile uint32_t dropped = 0;
} rx;
twai_node_handle_t node = nullptr;

bool IRAM_ATTR onRx(twai_node_handle_t handle,
                     const twai_rx_done_event_data_t*, void* user_ctx) {
    auto* ctx = static_cast<RxContext*>(user_ctx);
    BaseType_t wake = pdFALSE;
    if (xSemaphoreTakeFromISR(ctx->free_slots, &wake) != pdTRUE) {
        ctx->dropped++;
        return wake == pdTRUE;
    }
    Slot& slot = ctx->slots[ctx->write_index];
    if (twai_node_receive_from_isr(handle, &slot.frame) == ESP_OK) {
        slot.received_us = esp_timer_get_time();
        ctx->write_index = (ctx->write_index + 1) % POOL_SIZE;
        xSemaphoreGiveFromISR(ctx->ready_slots, &wake);
    } else {
        ctx->dropped++;
        xSemaphoreGiveFromISR(ctx->free_slots, &wake);
    }
    return wake == pdTRUE;
}

void printFrame(const Slot& slot) {
    const auto& f = slot.frame;
    // The initial target is classic CAN; ignore FD payloads.
    if (f.header.fdf || f.header.dlc > 8) return;
    Serial.printf("@RX %llu %s %08lX %u",
                  (unsigned long long)slot.received_us,
                  f.header.ide ? "EXT" : "STD",
                  (unsigned long)f.header.id,
                  (unsigned)f.header.dlc);
    for (unsigned i = 0; i < f.header.dlc; ++i)
        Serial.printf(" %02X", slot.bytes[i]);
    Serial.println();
}
}

void setup() {
    Serial.begin(921600);
    delay(500);
    rx.free_slots = xSemaphoreCreateCounting(POOL_SIZE, POOL_SIZE);
    rx.ready_slots = xSemaphoreCreateCounting(POOL_SIZE, 0);
    if (!rx.free_slots || !rx.ready_slots) abort();
    for (auto& slot : rx.slots) {
        slot.frame.buffer = slot.bytes;
        slot.frame.buffer_len = sizeof(slot.bytes);
    }
    twai_onchip_node_config_t cfg = {};
    cfg.io_cfg.tx = TX_PIN;
    cfg.io_cfg.rx = RX_PIN;
    cfg.io_cfg.quanta_clk_out = GPIO_NUM_NC;
    cfg.io_cfg.bus_off_indicator = GPIO_NUM_NC;
    cfg.bit_timing.bitrate = BITRATE;
    cfg.bit_timing.sp_permill = 875;
    cfg.data_timing.bitrate = 5000000;
    cfg.data_timing.sp_permill = 625;
    cfg.flags.enable_listen_only = true;
    ESP_ERROR_CHECK(twai_new_node_onchip(&cfg, &node));
    // No mask filter: capture all IDs supported by the controller.
    twai_event_callbacks_t callbacks = {};
    callbacks.on_rx_done = onRx;
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node, &callbacks, &rx));
    ESP_ERROR_CHECK(twai_node_enable(node));
    Serial.println("# SM5 CAN passive RX; 500000 bit/s; no TX");
}

void loop() {
    if (xSemaphoreTake(rx.ready_slots, pdMS_TO_TICKS(100)) == pdTRUE) {
        const Slot& slot = rx.slots[rx.read_index];
        printFrame(slot);
        rx.read_index = (rx.read_index + 1) % POOL_SIZE;
        xSemaphoreGive(rx.free_slots);
    }
    static uint32_t lastReport = 0;
    if (millis() - lastReport >= 5000) {
        lastReport = millis();
        Serial.printf("# dropped=%lu\n", (unsigned long)rx.dropped);
    }
}
