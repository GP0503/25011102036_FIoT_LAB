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
└── 8051/
    ├── CAT2/   README.md · circuit.png · main.c, delay.c, delay.h · STARTUP.A51 · LED_BLINK.uvproj
    ├── CAT3/   README.md · circuit.png · led_pattern_controller.c · STARTUP.A51 · .uvproj · Objects/*.hex
    ├── CAT4/   README.md · circuit.png · keypad_7seg_hmi.c · STARTUP.A51 · .uvproj · Objects/*.hex
    ├── CAT5/   README.md · circuit.png · led_fade_pwm.c · STARTUP.A51 · .uvproj
    ├── CAT6/   README.md · circuit.png · uart_echo.c · STARTUP.A51 · .uvproj · Objects/*.hex
    └── CAT7/   README.md · circuit.png · lcd_driver.c · STARTUP.A51 · .uvproj · Objects/*.hex
```

The STM32 experiments (CAT8 onwards) will be added under `STM32/`.

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

## 4. Opening and Building a Project

1. Open the experiment's `.uvproj` file in Keil µVision (C51). The target device is the AT89S52.
2. Build the target (F7). The firmware is written to `Objects/<project>.hex`.
3. Program the hex file into the AT89S52 on the development board through its ISP programmer.

Only source, project, startup and firmware files are tracked. Keil user settings, listings and intermediate build files are excluded by [`.gitignore`](.gitignore).
