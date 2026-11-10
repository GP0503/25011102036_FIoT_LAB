# Foundations of IoT – Lab Experiments

**Course:** 25CS2741 Fundamentals of Internet of Things + Lab

**Student:** Krish K (Krish Kalaiselvan) · Reg. No. 25011102049 · II Year B.Tech CSE (IoT), Section IoT-A

**Institution:** Shiv Nadar University, Chennai

## 1. About

This repository contains my laboratory experiment submissions for 25CS2741. Experiments are organised by hardware platform. Each experiment has its own folder with a `README.md`, a circuit diagram and the project files needed to open, build and reproduce it.

## 2. Repository Structure

```text
25011102049_FIoT_LAB/
│
├── README.md
├── .gitignore
│
├── 8051/
│   ├── CAT2/   README.md · circuit.png · main.c, delay.c, delay.h · STARTUP.A51 · LED_BLINK.uvproj
│   ├── CAT3/   README.md · circuit.png · led_pattern_controller.c · STARTUP.A51 · .uvproj · Objects/*.hex
│   ├── CAT4/   README.md · circuit.png · keypad_7seg_hmi.c · STARTUP.A51 · .uvproj · Objects/*.hex
│   ├── CAT5/   README.md · circuit.png · led_fade_pwm.c · STARTUP.A51 · .uvproj
│   ├── CAT6/   README.md · circuit.png · uart_echo.c · STARTUP.A51 · .uvproj · Objects/*.hex
│   └── CAT7/   README.md · circuit.png · lcd_driver.c · STARTUP.A51 · .uvproj · Objects/*.hex
│
└── STM32/
    ├── CAT9/    README.md · circuit.png · <project>.ioc · Core/ · Drivers/ · MDK-ARM/
    ├── CAT10/   README.md · circuit.png · <project>.ioc · Core/ · Drivers/ · MDK-ARM/
    ├── CAT12/   README.md
    │   ├── TASKA_BME280_I2C_UART/      circuit.png · <project>.ioc · Core/ · Drivers/ · MDK-ARM/
    │   └── TASKB_HTTP_JSON_RESPONSE/   circuit.png · <project>.ioc · Core/ · Drivers/ · MDK-ARM/
    └── CAT13/   README.md · circuit.png · <project>.ioc · Core/ · Drivers/ · MDK-ARM/
```

Each STM32 folder is a complete STM32CubeMX + Keil MDK project: the `.ioc` configuration, `Core/` (application and generated code), `Drivers/` (CMSIS + HAL) and `MDK-ARM/` (Keil project, startup file and, where built, the firmware `.hex`).

## 3. Experiments

### 8051 (AT89S52 @ 11.0592 MHz, Keil µVision C51)

| EXP | Date | Name of Experiment | Task(s) | Folder |
|---|---|---|---|---|
| CAT2 | 20/07/2026 | 8051 Tool Chain Setup – Development Board – AT89S52 | 2A Bare-Metal LED Blink · 2B Customised Library Function for Delay | [8051/CAT2](8051/CAT2) |
| CAT3 | 27/07/2026 | Timer-Based LED Pattern Generator and Button Interrupt Controller | 3A Four-State LED Control System | [8051/CAT3](8051/CAT3) |
| CAT4 | 03/08/2026 | Human-Machine Interface using Matrix Keypad, LEDs, and Seven-Segment Display | 4A Keypad-Controlled Seven-Segment Display and LED Functions | [8051/CAT4](8051/CAT4) |
| CAT5 | 10/08/2026 | Hardware Timer and Interrupt-Based Timing for LED Fading, Pulse Generation, and Stepper Motor Control | 5A LED Fading using Software PWM | [8051/CAT5](8051/CAT5) |
| CAT6 | 17/08/2026 | UART Driver and Serial Debugging | 6B UART Echo | [8051/CAT6](8051/CAT6) |
| CAT7 | 24/08/2026 | 16×2 LCD Driver Development and Dynamic Display Control | 7A LCD Initialization and Character/String Display | [8051/CAT7](8051/CAT7) |

### STM32 (STM32CubeMX + Keil MDK, HAL)

| EXP | Date | Name of Experiment | Task(s) | Board | Folder |
|---|---|---|---|---|---|
| CAT9 | 28/09/2026 | STM32 UART Command Controlled LED | 9A Basic UART Communication (Transmit a Character) | Blue Pill (STM32F103C8T6) | [STM32/CAT9](STM32/CAT9) |
| CAT10 | 05/10/2026 | STM32–Wemos Wi-Fi Modem Interface Using AT Commands | 9A STM32 ↔ Wemos UART · 9B AT Command Interface · 9C Connect Wemos to Wi-Fi | Blue Pill + Wemos D1 Mini | [STM32/CAT10](STM32/CAT10) |
| CAT12 | 23/10/2026 | BME280 Sensor Interfacing Using I²C and Bosch Sensor API (ESP8266 HTTP) | 12A BME280 over I²C (Bosch API) · 12B Raw HTTP Response with BME280 Data | NUCLEO-F401 + BME280 + Wemos D1 Mini | [STM32/CAT12](STM32/CAT12) |
| CAT13 | 27/10/2026 | BME280 Sensor Interfacing Using I²C and MQTT | 13A BME280 Telemetry Publishing over MQTT | NUCLEO-F401 + BME280 + Wemos D1 Mini | [STM32/CAT13](STM32/CAT13) |

## 4. Opening and Building a Project

**8051**

1. Open the experiment's `.uvproj` file in Keil µVision (C51). The target device is the AT89S52.
2. Build the target (F7). The firmware is written to `Objects/<project>.hex`.
3. Program the hex file into the AT89S52 on the development board through its ISP programmer.

**STM32**

1. Open the `.ioc` file in STM32CubeMX to see the pin, clock and peripheral configuration.
2. Open `MDK-ARM/<project>.uvprojx` in Keil MDK-ARM and build (F7). The firmware is written to `MDK-ARM/<project>/<project>.hex`.
3. Flash and debug through the ST-LINK (external ST-LINK V2 for the Blue Pill, on-board ST-LINK for the Nucleo).

Only source, project, startup and firmware files are tracked. Keil user settings, listings and intermediate build files are excluded by [`.gitignore`](.gitignore).
