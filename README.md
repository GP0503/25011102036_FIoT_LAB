# Foundations of IoT – Lab Experiments

**Course:** 25CS2741 Fundamentals of Internet of Things + Lab

**Student:** Guruprasad Baskar · Reg. No. 25011102036 · II Year B.Tech CSE (IoT), Section IoT-A

**Institution:** Shiv Nadar University, Chennai

## 1. About

This repository contains my laboratory experiment submissions for 25CS2741. Experiments are organised by hardware platform, continuous assessment (CAT) and task. Each task has its own folder with a `README.md`, a circuit diagram and the project files needed to open, build and reproduce it.

## 2. Repository Structure

Experiments are grouped as `<Platform>/CAT<n>/TASK<n><X>/`. Every task folder is a complete project on its own, with its `README.md` and `circuit.png`.

```text
25011102036_FIoT_LAB/
│
├── README.md
├── .gitignore
│
├── 8051/
│   ├── CAT2/TASK2B/    README.md · circuit.png · main.c, delay.c, delay.h · STARTUP.A51 · LED_BLINK.uvproj · Objects/*.hex
│   ├── CAT3/TASK3A/    README.md · circuit.png · led_pattern_controller.c · STARTUP.A51 · *.uvproj · Objects/*.hex
│   ├── CAT4/TASK4A/    README.md · circuit.png · keypad_7seg_hmi.c · STARTUP.A51 · *.uvproj · Objects/*.hex
│   ├── CAT5/TASK5A/    README.md · circuit.png · led_fade_pwm.c · STARTUP.A51 · *.uvproj · Objects/*.hex
│   ├── CAT6/TASK6B/    README.md · circuit.png · uart_echo.c · STARTUP.A51 · *.uvproj · Objects/*.hex
│   └── CAT7/TASK7A/    README.md · circuit.png · lcd_driver.c · STARTUP.A51 · *.uvproj · Objects/*.hex
│
└── STM32/
    ├── CAT8/   TASK8A/ · TASK8B/ · TASK8C/
    ├── CAT9/   TASK9A/
    ├── CAT10/  TASK10A/ · TASK10B/ · TASK10C/
    ├── CAT11/  TASK11A/ · TASK11B/ · TASK11C/
    ├── CAT12/  TASK12A/ · TASK12B/ · TASK12C/
    ├── CAT13/  TASK13/
    ├── CAT14/  TASK14/
    └── CAT15/  TASK15/   (+ Node-RED flow .json and dashboard.png)
        each:   README.md · circuit.png · <project>.ioc · .mxproject · Core/ · Drivers/ · MDK-ARM/ (*.uvprojx, startup_*.s)
```

Each STM32 task folder is a complete STM32CubeMX + Keil MDK project: the `.ioc` configuration, `Core/` (application and generated code), `Drivers/` (CMSIS + HAL) and `MDK-ARM/` (Keil project, startup file and, where built, the firmware `.hex`).

## 3. Experiments

### 8051 (AT89S52 @ 11.0592 MHz, Keil µVision C51)

| EXP | Date | Name of Experiment | Task | Folder |
|---|---|---|---|---|
| CAT2 | 20/07/2026 | 8051 Tool Chain Setup – Development Board – AT89S52 | 2B Bare-Metal LED Blink with a Customised Delay Library (includes the board bring-up sheet) | [8051/CAT2/TASK2B](8051/CAT2/TASK2B) |
| CAT3 | 27/07/2026 | Timer-Based LED Pattern Generator and Button Interrupt Controller | 3A Four-State LED Control System | [8051/CAT3/TASK3A](8051/CAT3/TASK3A) |
| CAT4 | 03/08/2026 | Human-Machine Interface using Matrix Keypad, LEDs, and Seven-Segment Display | 4A Keypad-Controlled Seven-Segment Display and LED Functions | [8051/CAT4/TASK4A](8051/CAT4/TASK4A) |
| CAT5 | 10/08/2026 | Hardware Timer and Interrupt-Based Timing for LED Fading, Pulse Generation, and Stepper Motor Control | 5A LED Fading using Software PWM | [8051/CAT5/TASK5A](8051/CAT5/TASK5A) |
| CAT6 | 17/08/2026 | UART Driver and Serial Debugging | 6B UART Echo | [8051/CAT6/TASK6B](8051/CAT6/TASK6B) |
| CAT7 | 24/08/2026 | 16×2 LCD Driver Development and Dynamic Display Control | 7A LCD Initialization and Character/String Display | [8051/CAT7/TASK7A](8051/CAT7/TASK7A) |

### STM32 (STM32CubeMX + Keil MDK, HAL)

| EXP | Date | Name of Experiment | Task | Board | Folder |
|---|---|---|---|---|---|
| CAT8 | 18/09/2026 | STM32 GPIO Bring-Up and Switch-Controlled LED | 8A GPIO Input/Output | Blue Pill (STM32F103C8T6) | [STM32/CAT8/TASK8A](STM32/CAT8/TASK8A) |
| | | | 8B Event-Based Control | | [STM32/CAT8/TASK8B](STM32/CAT8/TASK8B) |
| | | | 8C Short Press vs Long Press | | [STM32/CAT8/TASK8C](STM32/CAT8/TASK8C) |
| CAT9 | 28/09/2026 | STM32 UART Command Controlled LED | 9A Basic UART Communication (Transmit a Character) | Blue Pill (STM32F103C8T6) | [STM32/CAT9/TASK9A](STM32/CAT9/TASK9A) |
| CAT10 | 05/10/2026 | STM32–Wemos Wi-Fi Modem Interface Using AT Commands | 10A Establish STM32 ↔ Wemos UART Communication | Blue Pill + Wemos D1 Mini | [STM32/CAT10/TASK10A](STM32/CAT10/TASK10A) |
| | | | 10B Explore the ESP8266 AT Command Interface | | [STM32/CAT10/TASK10B](STM32/CAT10/TASK10B) |
| | | | 10C Connect Wemos to Wi-Fi | | [STM32/CAT10/TASK10C](STM32/CAT10/TASK10C) |
| CAT11 | 14/10/2026 | STM32F401RE Cortex-M4: GPIO and User Input Programming | 11A On-board LED Bring-Up | NUCLEO-F401RE | [STM32/CAT11/TASK11A](STM32/CAT11/TASK11A) |
| | | | 11B On-board Switch Control with State Remembrance | | [STM32/CAT11/TASK11B](STM32/CAT11/TASK11B) |
| | | | 11C Short-Press and Long-Press Detection | | [STM32/CAT11/TASK11C](STM32/CAT11/TASK11C) |
| CAT12 | 23/10/2026 | Sensor Interfacing (BME280) – STM32 (Debug) – ESP8266 (Wi-Fi) HTTP – Node Analytics | 12A BME280 over I²C with the Bosch Sensor API | NUCLEO-F401 + BME280 | [STM32/CAT12/TASK12A](STM32/CAT12/TASK12A) |
| | | | 12B ESP8266 HTTP Server Bring-Up | NUCLEO-F401 + Wemos D1 Mini | [STM32/CAT12/TASK12B](STM32/CAT12/TASK12B) |
| | | | 12C Raw HTTP Response with BME280 Data | NUCLEO-F401 + BME280 + Wemos D1 Mini | [STM32/CAT12/TASK12C](STM32/CAT12/TASK12C) |
| CAT13 | 27/10/2026 | Sensor Interfacing (BME280) – (MQTT) – Node Analytics | 13 BME280 Telemetry Publishing over MQTT | NUCLEO-F401 + BME280 + Wemos D1 Mini | [STM32/CAT13/TASK13](STM32/CAT13/TASK13) |
| CAT14 | 04/11/2026 | IoT Integration Platform – Node-RED: Two-Player Competitive IoT Snake Game (End Semester Lab) | 14 Level 1 – Player node: joystick → MQTT direction commands | Blue Pill + joystick + Wemos D1 Mini · Node-RED | [STM32/CAT14/TASK14](STM32/CAT14/TASK14) |
| CAT15 | 12/11/2026 | Sensor Interfacing (BME280) – MQTT – Node-RED: Live Weather Monitoring Dashboard with Node Analytics | 15 Live weather dashboard with four inferences (BME280 + OpenWeatherMap) | NUCLEO-F401 + BME280 + Wemos D1 Mini · Node-RED | [STM32/CAT15/TASK15](STM32/CAT15/TASK15) |

The CAT10 lab sheet prints its tasks as 9A–9C; they are numbered 10A–10C here, following the coursework numbering. One firmware covers all three CAT10 tasks, so the same project is placed in each of those task folders.

## 4. Opening and Building a Project

**8051**

1. Open the task's `.uvproj` file in Keil µVision (C51). The target device is the AT89S52.
2. Build the target (F7). The firmware is written to `Objects/<project>.hex`.
3. Program the hex file into the AT89S52 on the development board through its ISP programmer.

**STM32**

1. Open the `.ioc` file in STM32CubeMX to see the pin, clock and peripheral configuration.
2. Open `MDK-ARM/<project>.uvprojx` in Keil MDK-ARM and build (F7). The firmware is written to `MDK-ARM/<project>/<project>.hex`.
3. Flash and debug through the ST-LINK (external ST-LINK V2 for the Blue Pill, on-board ST-LINK for the Nucleo).

Only source, project, startup and firmware files are tracked. Keil user settings, listings and intermediate build files are excluded by [`.gitignore`](.gitignore).
