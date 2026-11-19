# WeActStudio ESP32-C5: first passive capture target

The first implementation lives in [SM5PassiveSniffer/SM5PassiveSniffer.ino](SM5PassiveSniffer/SM5PassiveSniffer.ino).

This is a **standalone Arduino sketch**, not yet a complete port of the MeganeCAN firmware. It is intended to validate the hardware and receive-only TWAI driver before integrating the existing `/wire` web UI.

## Prerequisites
- WeActStudio CANFDDevBoard V1.0 ESP32-C5.
- Arduino ESP32-C5 board support with the **new** on-chip TWAI headers `esp_twai.h` and `esp_twai_onchip.h` (as used in the manufacturer's example).
- Board selection `ESP32C5 Dev Module`.
- USB serial monitor at **921600 baud**.

## Wiring
- Board OBD-II pin 6: CAN-H; pin 14: CAN-L; pins 4/5: ground.
- Do not assume that the SM5 multimedia CAN bus is on the vehicle's standard OBD-II CAN pins. Verify the wiring diagram and bus with a meter/scope first.
- Switchable 120-ohm termination: OFF when joining an already correctly terminated CAN network.
- Power and grounding must match the board's specifications.

## Behavior
- TWAI nominal bitrate 500000; GPIO24 RX, GPIO23 TX.
- Hardware `enable_listen_only=true`; **no transmit function** exists in this sketch.
- No CAN acceptance mask is installed.
- Classic CAN payloads up to 8 bytes, standard and extended IDs.
- USB serial lines formatted as `@RX <timestamp_us> <STD|EXT> <hex_id> <dlc> <hex_data...>`.
- Prints a `# dropped=<count>` diagnostic every 5 seconds.
- Uses a 256-slot queue; USB serial throughput can still cause dropped frames. The timestamp is captured when dequeuing from the hardware RX callback, not at the exact physical bus edge.

## Important limitations
- Not compiled or hardware-tested yet.
- This sketch is **not** MeganeCAN's `/wire` WebSocket protocol; the serial log format is provisional.
- Does not decode Renault frames or control the radio.
- Does not yet integrate `HwCanBus`, `CanLog`, or the MeganeCAN web UI.
- The default `platformio.ini` of MeganeCAN targets ESP32-C3/ESP32 and is not automatically compatible with ESP32-C5.
- If the manufacturer TWAI API changes between ESP-IDF/Arduino releases, the sketch may need adjustment.

## Next steps
1. Compile using the manufacturer's supported Arduino/ESP-IDF toolchain.
2. Validate receive-only mode with a controlled CAN source.
3. Add a dedicated ESP32-C5 PlatformIO environment after identifying a compatible platform version.
4. Adapt the received frames into MeganeCAN `Frame` and expose them through the existing `/wire` UI.
5. Run automated builds and a real-world test.
