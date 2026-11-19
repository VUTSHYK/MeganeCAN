# ESP32-C5 WeActStudio port — work in progress

Target: WeActStudio CANFDDevBoard V1.0 (ESP32-C5), Renault Samsung SM5, classic CAN at 500 kbit/s.

## Confirmed hardware (manufacturer README)
- On-chip TWAI CAN FD controller; CAN TX GPIO23, CAN RX GPIO24.
- OBD-II CAN-H pin 6, CAN-L pin 14, ground pins 4/5, vehicle power pin 16.
- WS2812 LED GPIO27.
- Manufacturer example: `Examples/Arduino/CAN_TWAI/CAN_TWAI.ino`, uses `esp_twai.h` and `esp_twai_onchip.h`.

## Safety requirements for initial firmware
- Hardware listen-only / silent mode; never send a CAN frame or ACK.
- 500000 bit/s nominal bitrate; classic CAN frames (up to 8 data bytes).
- Accept all standard and extended IDs; preserve frame ID, DLC, payload, and timestamps.
- Do not enable MeganeCAN display registration, joystick injection, heartbeat, or any other TX path.
- Vehicle CAN termination: leave the board's switchable 120-ohm resistor OFF on an already terminated bus; verify actual wiring before connecting.

## Existing MeganeCAN integration points
- `platformio.ini`: existing builds use `collin80/esp32_can` and target ESP32-C3 or ESP32.
- `src/bus/HwCanBus.{h,cpp}`: central conversion from driver `CAN_FRAME` to portable `Frame`, with receive taps.
- `src/utils/CanUtils.{h,cpp}`: legacy `CAN_FRAME` API still used by other modules.
- `src/main.cpp`: `CAN0.setCANPins(GPIO_NUM_3, GPIO_NUM_4)`, `CAN0.begin(CAN_BPS_500K)`, `CAN0.setGeneralCallback(gotFrame)`, `CAN0.watchFor()`.
- `src/utils/CanLog.{h,cpp}` and `data/wire.html`: existing logger and web UI; reuse after port.

## Implementation sequence
1. Verify a PlatformIO toolchain with ESP32-C5 and the new TWAI on-chip API; don't assume the default `espressif32` platform supports it.
2. Introduce a target-specific TWAI adapter and convert received `twai_frame_t` to MeganeCAN's portable `Frame`.
3. Gate out all transmit functionality at the hardware driver level and start the controller in supported listen-only mode.
4. Add a dedicated build environment and exclude incompatible modules if necessary.
5. Compile, then test on the bench with a known CAN source before attaching to the car.
6. Validate `/wire` captures and dropped-frame counters.

## References
- https://github.com/WeActStudio/WeActStudio.CANFDDevBoardV1_ESP32C5
- https://github.com/WeActStudio/WeActStudio.CANFDDevBoardV1_ESP32C5/blob/master/Examples/Arduino/CAN_TWAI/CAN_TWAI.ino
- https://github.com/Barbedx/MeganeCAN

Status: planning and source audit only. No ESP32-C5 build or hardware test has yet been performed.
