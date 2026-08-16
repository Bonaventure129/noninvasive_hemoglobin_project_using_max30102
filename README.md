# Non-Invasive Hemoglobin (Hb) Monitor

![C++](https://img.shields.io/badge/Language-C%2B%2B-blue)
![Python](https://img.shields.io/badge/Language-Python-yellow)
![Platform](https://img.shields.io/badge/Platform-ESP32--C3-lightgrey)

## Overview
This repository contains the firmware and signal processing scripts for a prototype Non-Invasive Hemoglobin (Hb) Monitor. By utilizing photoplethysmography (PPG), the system captures raw Red and Infrared (IR) optical absorption data via a GY-MAX30102 sensor. 

An ESP32-C3 microprocessor buffers the live telemetry, extracts the pulsatile (AC) and baseline (DC) components, and applies a custom linear regression calibration model to estimate Hemoglobin levels (g/dl) in real-time, displaying the results on an I2C OLED screen.

## Hardware Architecture
*   **Microcontroller:** ESP32-C3
*   **Optical Sensor:** GY-MAX30102 Pulse Oximetry Module
*   **Display:** 0.96" I2C OLED Display
*   **Communication:** Inter-Integrated Circuit (I2C)

### Wiring Guide

| Component Pin | ESP32-C3 Pin (Default I2C) |
| :--- | :--- |
| **VIN / VCC** | 3.3V |
| **GND** | GND |
| **SDA** | GPIO 8 |
| **SCL** | GPIO 9 |
| **INT**| Not connected (Polling used for data acquisition) |

## Software Architecture

### 1. Embedded Firmware (C/C++)
Developed in the Arduino IDE, the firmware operates by collecting a continuous sample buffer of raw optical data. Once the buffer is filled, the code calculates the Peak-to-Peak (AC) and Average (DC) values for both the Red and IR light bands to compute the Ratio of Ratios ($R$):

$$R = \frac{AC_{red} / DC_{red}}{AC_{ir} / DC_{ir}}$$

The calculated $R$ value is then passed through the derived calibration equation to yield the final Hemoglobin concentration.

### 2. Calibration Model (Python)
The repository includes a Python data science pipeline (`hb_calibration.py`) used to build the mathematical model. It processes raw CSV logs against clinical ground-truth data by:
1. Applying a digital bandpass filter (0.5 Hz - 3.0 Hz) using `scipy.signal` to isolate human heartbeats and reject motion artifacts.
2. Calculating the clinical Ratio of Ratios ($R$).
3. Executing a linear regression analysis via `pandas` and `scipy.stats` to generate the final calibration formula.

**Current Calibration Equation:**
$$Hb = 47.50 \cdot R - 5.31$$

## Installation & Setup

### Microcontroller Firmware
1. Open `Hemoglobin_project_main.ino` in the Arduino IDE.
2. Install the required libraries via the Library Manager:
   * `SparkFun MAX3010x Pulse and Proximity Sensor Library`
   * `Adafruit SSD1306`
   * `Adafruit GFX Library`
3. Select your specific ESP32-C3 board and flash the firmware.

### Python Data Processing Environment
To run the calibration script and generate your own regression models safely on a Linux environment (PEP 668 compliant):
```bash
# 1. Create a virtual environment
python3 -m venv .venv

# 2. Activate the environment
source .venv/bin/activate

# 3. Install dependencies
pip install pandas numpy scipy matplotlib

# 4. Run the calibration script
python hb_calibration.py
