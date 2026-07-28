# smart_glass_firmware

Firmware scaffold for the AI-Powered Smart Glasses project (see
`vision_glasses/docs/` in the parent repo for the full spec: SRS-001,
SW-ARCH-001, HW-ARCH-001, AI-ARCH-001, NAV-ARCH-001, VOICE-ARCH-001,
COMM-ARCH-001, DB-ARCH-001, DEC-ENG-001, SDD-001, TEST-001).

This scaffold follows the directory layout, message bus design, task
table, and startup sequence exactly as specified in `SW-ARCH-001.md`
sections 18–20.

## What's implemented (real logic, not stubs)

- **`middleware/message_bus.{h,c}`** — pub/sub over a FreeRTOS priority
  queue, fixed-size payload envelope, subscriber table with a dispatcher
  task. All message types from `SW-ARCH-001.md`'s message bus section are
  defined in `middleware/include/message_types.h`.
- **`middleware/task_manager.{h,c}`** — static task table matching
  section 19.1 exactly (names, priorities, stack sizes), creates every
  task and starts the scheduler.
- **`middleware/system_health.{h,c}`** — per-task watchdog tracking,
  boot-failure reporting, bus-drop counting.
- **`hal/imu_hal.{h,c}`** + **`drivers/i2c_imu_drv.{h,c}`** — full
  init → self-test → 1kHz read → publish `MSG_IMU_DATA` pipeline. Driver
  is a stub (no real I2C transactions yet) but the HAL/task/bus wiring
  is real and follows the intended pattern for every other sensor.
- **`hal/camera_hal.{h,c}`** + **`drivers/usb_camera_drv.{h,c}`** — same
  pattern for the 30fps depth camera task.
- **`main.c`** — implements the startup sequence from section 20 stage
  by stage, with each unimplemented stage clearly marked `TODO`.

## What's stubbed (compiles and runs, no real behavior yet)

Every other task in the table (`gps_task`, `audio_in_task`,
`audio_out_task`, `ai_inference_task`, `ai_postprocess_task`, `nav_task`,
`decision_task`, `voice_task`, `safety_task`, `battery_task`,
`wifi_task`, `db_task`, `log_task`) has a placeholder entry point in
`middleware/task_stubs.c` that starts, feeds its watchdog, and does
nothing else. Each stub's comment names the module that should own its
real implementation, matching the directory structure in
`SW-ARCH-001.md` section 18 (e.g. `nav_task_entry` → move into
`navigation/nav_manager.c` once written).

## Suggested build order

Given the dependency graph in the architecture docs, a reasonable next
sequence is:

1. **Drivers for one full sensor path** — pick IMU or camera (already
   scaffolded) and replace the stub driver with real register-level
   code once hardware is on the bench.
2. **`database/db_manager.c`** — needed early since `system_config.yaml`
   and `schema.sql` (in the parent repo) define config/calibration data
   several other modules depend on (e.g. IMU calibration offsets).
3. **`ai/ai_manager.c`** — Edge Impulse model loading, referenced by
   `AI-ARCH-001.md`; `ai_inference_task` is currently the emptiest stub
   and blocks the whole perception → decision → nav chain.
4. **`decision/decision_engine.c`** — per `DEC-ENG-001.md`, this is the
   hazard-prioritization hub that most other modules publish to/consume
   from; worth building against synthetic `MSG_OBSTACLE_ALERT` /
   `MSG_FALL_DETECTED` test messages before real sensors are ready.

## Build

Two build paths are scaffolded; neither is fully wired to a toolchain
yet since the exact Arduino UNO Q board target/FreeRTOS port needs
confirming during hardware bring-up (see `TODO`s in `platformio.ini`
and `CMakeLists.txt`).

```sh
# PlatformIO (once board id + FreeRTOS lib_dep are filled in)
pio run -e smart_glass_uno_q

# CMake (once FREERTOS_DIR is provided)
cmake -B build -DFREERTOS_DIR=/path/to/FreeRTOS
cmake --build build
```
