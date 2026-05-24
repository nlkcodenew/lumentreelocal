# ESP32-C3 Experimental First Boot

Date: 2026-05-24

## Scope

Validate whether a newly purchased `ESP32-C3 Super Mini` with `4MB` embedded
flash can at least:

1. enumerate correctly on the host,
2. build the current firmware under a dedicated target,
3. flash successfully,
4. and boot into the application image.

This was intentionally done without changing the stable `ESP32-S3` production
line.

## Hardware detected

Host runtime verification showed:

- USB device: `303A:1001 Espressif USB JTAG/serial debug unit`
- serial port: `/dev/ttyACM0`
- chip: `ESP32-C3`
- flash: `4MB`
- mode: `USB-Serial/JTAG`
- MAC: `9c:cc:01:c0:a3:38`

## Firmware target added

New experimental `PlatformIO` target:

- `esp32-c3-4mb-experimental`

Key settings:

- board: `esp32-c3-devkitm-1`
- flash size: `4MB`
- partition table: `single_app_4MB_experimental.csv`
- CPU: `160MHz`
- upload speed: `460800`
- build flags:
  - `ARDUINO_USB_CDC_ON_BOOT=1`
  - `ARDUINO_USB_MODE=1`
  - firmware version suffix: `0.15.1-exp-c3-4mb`

## Build result

Command:

```bash
cd /home/mrlinh/esp32-lumentree/firmware
pio run -e esp32-c3-4mb-experimental
```

Result:

- `SUCCESS`
- RAM: `64204 / 327680` (`19.6%`)
- Flash: `1768020 / 3014656` (`58.6%`)

So the firmware fits comfortably in the custom single-app `4MB` layout.

## Flash result

Command:

```bash
cd /home/mrlinh/esp32-lumentree/firmware
pio run -e esp32-c3-4mb-experimental -t upload --upload-port /dev/ttyACM0
```

Result:

- `SUCCESS`
- full image written and verified

## First boot result

Serial boot capture showed:

- ROM boot banner
- normal load from flash
- app entry reached
- NVS keys missing, which is expected for a fresh blank board:
  - `wifi_ssid NOT_FOUND`
  - `wifi_pass NOT_FOUND`
  - `api_url NOT_FOUND`
  - `api_token NOT_FOUND`
  - `device_id NOT_FOUND`
  - `gateway_id NOT_FOUND`
  - `target_mac NOT_FOUND`

This is enough to say:

- the board is not bricked,
- the image is valid,
- and the app starts on real `ESP32-C3` hardware.

## What is verified vs not yet verified

Verified:

- host USB enumeration
- chip detection
- build success
- flash success
- first boot into application image

Not yet verified:

- AP mode visibility from a client device
- local portal responsiveness
- Wi-Fi onboarding
- BLE scan and inverter pairing
- upload path to the production API
- read/write token flow
- long-running stability

## Practical conclusion

`ESP32-C3 4MB` has now crossed the first meaningful threshold:

- it is no longer just a planned target on paper,
- it is a real experimental target that builds, flashes, and boots.

However, it must still remain **experimental** until onboarding, BLE, upload,
and runtime stability are tested end-to-end.
