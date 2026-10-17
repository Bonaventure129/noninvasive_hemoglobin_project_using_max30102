# 🩸 Non-Invasive Hemoglobin Estimator using TinyML

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![TensorFlow Lite](https://img.shields.io/badge/TensorFlow%20Lite-Micro-orange.svg)](https://www.tensorflow.org/lite/microcontrollers)
[![Platform](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)

An end-to-end TinyML (Machine Learning on Microcontrollers) system designed to estimate blood hemoglobin (Hb) levels non-invasively. This project uses a MAX30102/MAX30105 optical sensor to capture Red and Infrared (IR) light absorption through a fingertip, processes this data through an embedded Artificial Neural Network (ANN), and displays the predicted Hemoglobin concentration in g/dL on an OLED screen.

---

## 📑 Table of Contents
- [About the Project](#-about-the-project)
- [Hardware Requirements](#-hardware-requirements)
- [Circuit Connections](#-circuit-connections)
- [Project Structure](#-project-structure)
- [The Neural Network (TinyML)](#-the-neural-network-tinyml)
- [Getting Started](#-getting-started)
  - [Phase 1: Data Collection](#phase-1-data-collection)
  - [Phase 2: Training the AI](#phase-2-training-the-ai)
  - [Phase 3: Deployment](#phase-3-deployment)
- [Usage](#-usage)

---

## 💡 About the Project

Traditional hemoglobin testing requires drawing blood. This project aims to provide a pain-free, rapid estimation alternative using photoplethysmography (PPG) and AI. By measuring how much Red and IR light is absorbed by the blood pulsating in a finger, we can feed these optical characteristics into a custom-trained Neural Network running directly on an ESP32 microcontroller to predict Hemoglobin levels in real-time.

---

## ⚡ Hardware Requirements

*   **Microcontroller:** ESP32 (e.g., ESP32-WROOM, ESP32-C3)
*   **Sensor:** MAX30102 or MAX30105 Pulse Oximeter and Heart-Rate Sensor
*   **Display:** 0.96" SSD1306 OLED Display (I2C)
*   **Misc:** Breadboard, Jumper Wires, 3.3V Power Supply

---

## 🔌 Circuit Connections

Both the MAX30105 sensor and the SSD1306 OLED communicate using the I2C protocol. This means their data lines can be wired in parallel to the exact same pins on the ESP32.

| ESP32 Pin | OLED Pin | MAX30105 Pin | Description |
| :--- | :--- | :--- | :--- |
| **3.3V** | VCC | VIN / VCC | Power supply for peripherals |
| **GND** | GND | GND | Common ground |
| **GPIO 8** | SDA | SDA | I2C Data line |
| **GPIO 9** | SCL | SCL | I2C Clock line |

> **Note:** The code is configured for `I2C_SDA = 8` and `I2C_SCL = 9`. You can change these in `Hemoglobin_project_main.ino` if your specific ESP32 board uses different default I2C pins. The OLED uses I2C address `0x3C` and the MAX30105 uses `0x57`.

---

## 📂 Project Structure

```text
Hemoglobin_project/
├── Hemoglobin_project_calibration/
│   └── Hemoglobin_project_calibration_ir_and_red_light.ino  # Data extraction code
├── training/
│   ├── train_hb_model.py                                    # Neural Network training script
│   ├── Clean_Hemoglobin_Dataset.csv                         # Generated combined dataset
│   └── *.csv                                                # Raw patient data files
└── Hemoglobin_project_main/
    ├── Hemoglobin_project_main.ino                          # Main TinyML C++ application
    └── model.h                                              # Compiled TensorFlow Lite AI brain
```

---

## 🧠 The Neural Network (TinyML)

The core of this project is a Feedforward Artificial Neural Network (ANN) that learns the mathematical relationship between light absorption and hemoglobin concentration.

1.  **Architecture:** The model is built using TensorFlow/Keras with two inputs (Red, IR), two hidden layers (16 neurons and 8 neurons with ReLU activation), and a single linear output layer for the Hb prediction.
2.  **Preprocessing:** Raw sensor data (which can range from 50k to over 100k) is normalized to a `0.0` - `1.0` scale using a `MinMaxScaler`.
3.  **Inference:** Once trained, the model is converted to a C-byte array using TensorFlow Lite for Microcontrollers. The ESP32 allocates a 4KB Tensor Arena to run this model natively, updating predictions on the fly.

---

## 🚀 Getting Started

### Prerequisites
*   [Arduino IDE](https://www.arduino.cc/en/software) with the ESP32 Board Manager installed.
*   Arduino Libraries: `Adafruit GFX`, `Adafruit SSD1306`, `SparkFun MAX3010x`, and an ESP32 compatible `TensorFlowLite` library.
*   Python 3.8+ installed on your PC.

### Phase 1: Data Collection (Calibration)
Before the AI can work, it needs to learn from real-world data correlated with medical lab results.
1. Flash `Hemoglobin_project_calibration_ir_and_red_light.ino` to your ESP32.
2. Place a patient's finger firmly on the sensor.
3. Open the Arduino Serial Monitor (115200 baud). You will see comma-separated values (Red, IR).
4. Copy this raw data and save it as a `.csv` file in the `training/` folder, named after the patient (e.g., `amuzie_chibuike.csv`).

### Phase 2: Training the AI
1. Open a terminal and navigate to the `training/` directory.
2. Install the required Python packages:
   ```bash
   pip install pandas numpy tensorflow scikit-learn
   ```
3. Open `train_hb_model.py` and update the `ground_truth_map` with your collected CSV files and their corresponding actual Hemoglobin lab results (in g/dL).
4. Run the training script:
   ```bash
   python train_hb_model.py
   ```
5. **CRITICAL:** The script will output **C++ Calibration Constants** (`RED_MIN`, `RED_MAX`, etc.). Copy these values! The script will also automatically generate a new `model.h` file.

### Phase 3: Deployment
1. Move the newly generated `model.h` file from the `training/` folder into the `Hemoglobin_project_main/` folder.
2. Open `Hemoglobin_project_main.ino` in the Arduino IDE.
3. Locate the `CALIBRATION CONSTANTS` section and paste the values you copied during Phase 2.
4. Compile and upload the code to your ESP32.

---

## 🏥 Usage

1. Power the device. The OLED will initialize and display: **"Waiting for Patient. Place finger firmly."**
2. Place a finger flat and still on the MAX30105 sensor.
3. The device will automatically detect the finger and begin an **8-second countdown**. It takes exactly 8 seconds to build a stable average of your heart's optical pulse.
4. Keep your finger completely still during the countdown.
5. Once complete, the TinyML model will run, and your estimated Hemoglobin level will be displayed on the screen in **g/dL**.
6. Remove your finger to reset the device for the next reading.
