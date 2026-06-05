# Audio-Silence-Monitor-Lite

A smart, network-connected audio silence monitor built on the Seeed Studio XIAO ESP32-C3. This device monitors a live line-level audio feed (e.g., broadcast automation, background music servers, radio streams) and automatically sends an email alert via SMTP2GO if the audio drops out for more than 30 seconds.

## Features
* **2-Stage Guitar-Tuner Calibration:** Visual LED feedback makes it incredibly easy to set the perfect input volume and dynamic noise floor threshold.
* **Email Alerts:** Secure alerts sent directly to your inbox via Wi-Fi using SMTP2GO.
* **Self-Healing:** Automatically sends a "Clear" notification once sustained audio returns for 10 seconds.
* **Compact Hardware:** Minimal components required, powered over a standard USB-C connection.

---

## Bill of Materials (BOM)

| Component | Qty | Description | Notes |
| :--- | :---: | :--- | :--- |
| **XIAO ESP32-C3** | 1 | Microcontroller with Wi-Fi/BLE | Main processor |
| **Capacitor 1µF (105)** | 1 | Ceramic or Electrolytic | Blocks external DC offset |
| **Resistor 10kΩ** | 2 | 1/4W Resistors | Creates the 1.65V DC bias for the ADC |
| **Resistor 330Ω** | 2 | 1/4W Resistors | Current limiting for LEDs |
| **LED (Red)** | 1 | 3mm or 5mm LED | Status & Alarm indicator |
| **LED (Green)** | 1 | 3mm or 5mm LED | Status & Normal indicator |
| **Tactile Push Button** | 1 | Momentary Button | Initiates calibration routine |
| **3.5mm Audio Jack** | 1 | Panel mount or breakout | Audio input interface |

---

## Hardware Wiring Guide

* **Audio Input:** * Audio Signal Source $\rightarrow$ `1µF Capacitor` $\rightarrow$ `Pin D0`
  * `Pin D0` $\rightarrow$ `10kΩ Resistor` to `3.3V`
  * `Pin D0` $\rightarrow$ `10kΩ Resistor` to `GND`
  * Audio Ground $\rightarrow$ `GND`
* **Control & Indicators:**
  * **Button:** `Pin D7` $\rightarrow$ Momentary Button $\rightarrow$ `GND`
  * **Red LED:** `Pin D8` $\rightarrow$ `330Ω Resistor` $\rightarrow$ Anode (+), Cathode (-) to `GND`
  * **Green LED:** `Pin D9` $\rightarrow$ `330Ω Resistor` $\rightarrow$ Anode (+), Cathode (-) to `GND`

---

## Setup & Operation Guide

### 1. Software Configuration
Before flashing the code using the Arduino IDE, update the configuration block at the top of the sketch:
* Set your `WIFI_SSID` and `WIFI_PASSWORD`.
* Sign up for a free account at [SMTP2GO](https://www.smtp2go.com/) and input your `AUTHOR_EMAIL`, `AUTHOR_PASSWORD`, and `RECIPIENT_EMAIL`.

### 2. First-Time Calibration Instruction
To ensure the ESP32 accurately reads your audio feed without clipping or picking up background hum, follow this two-stage process:

1. **Start Calibration:** While your audio feed (music or talk) is playing actively, **press and hold the button for 3+ seconds**. Both LEDs will flash twice to indicate calibration mode is active.
2. **Stage 1 (Level Tuning):** The device works like a guitar tuner:
   * **Flashing Red:** Input volume is too low. Turn up your audio source feed.
   * **Flashing Green:** Input volume is too high. Lower your audio source feed.
   * **Solid Red + Green:** Perfect level. 
3. **Lock & Profile:** Once both LEDs are solid, **press the button once**. 
4. **Stage 2 (Auto-Threshold):** The LEDs will alternate flashing for 30 seconds while the system builds a dynamic noise-floor profile of your audio. Do not alter the volume during this time. 

Once both LEDs flash twice a final time, the device is armed and actively monitoring your audio stream!
