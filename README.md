# Fukuoka Uric Acid Sensor - Production Firmware

![C](https://img.shields.io/badge/Language-C-blue.svg)
![SoC](https://img.shields.io/badge/SoC-DA14531--00-orange.svg)
![SDK](https://img.shields.io/badge/SDK-Dialog--6.0.24.1464-green.svg)
![Build](https://img.shields.io/badge/Build-Keil--uVision--5-lightgrey.svg)
![License](https://img.shields.io/badge/License-MIT-yellow.svg)

## 📖 Introduction
This repository contains the full and finalized firmware for the **Fukuoka Uric Acid Sensor**, targeting the **DA14531 SmartBond TINY™ SoC**. 

### The Evolution
Building upon the initial proof-of-concept, this version introduces a robust event-driven architecture designed for long-term battery operation. Key advancements include a **Battery-Aware Control Loop** for sensor excitation, a high-reliability **Undervoltage Protection (UVP)** state machine, and full utilization of the SoC's **Retention RAM** to preserve system state during deep sleep cycles.

---

## 🚀 Advanced Features & Implementation

### 1. Battery-Aware PWM Compensation
To ensure electrochemical sensing accuracy as the battery discharges, the firmware implements a feed-forward compensation algorithm. It monitors the battery voltage (Vbat) and updates the PWM duty cycle in real-time to maintain a precise target voltage for the bias signal (Vtarget).

* **The Logic:** Implemented in `timer2_pwm_dc_control`, the pulse width is calculated using the following compensation formula:
  > **PulseWidth = (Period / 2) - [(5 * Vtarget * Period) / (7 * Vbat)]**
* **Precision:** Utilizes `GPIO_ConfigurePinPower` set to the **1V rail** for reduced current driving strength, increasing the stability and accuracy of the ±1V sensor excitation signal.

### 2. Under-Voltage Protection (UVP) with Hysteresis
A dedicated software state machine prevents erratic system reboots and battery damage during low-power states.
* **Hysteresis logic:** Implemented with a shutdown threshold of **1850 mV** and a restart threshold of **1900 mV**.
* **Safety Protocol:** Upon trigger, the system disables all PWM excitation and sensor sampling timers, entering a low-power "safe mode" while maintaining BLE telemetry for error reporting and diagnostics.

### 3. Power Optimization & Memory Retention
* **Extended Sleep:** Configured for `ARCH_EXT_SLEEP_ON` with an average current draw optimized for wearable longevity.
* **Retention RAM:** Critical system variables (UVP status, PWM offsets, and BLE CCCD configurations) are stored in `retention_mem_area0`. This ensures the device "remembers" its state and target bias voltages after waking from deep sleep cycles.
* **Buck Mode:** The DCDC converter is optimized for efficiency using `syscntl_dcdc_turn_on_in_buck(SYSCNTL_DCDC_LEVEL_1V1)`.

---

## 📡 BLE Service Definition (GATT)
The firmware implements a custom 128-bit UUID service (`D6E0...`) for full sensor control via a smartphone or gateway.

| Characteristic | Data Format | Description |
| :--- | :--- | :--- |
| **Sensor Voltage** | 16-bit uint | Real-time electrochemical data (mV) sent via Notifications. |
| **PWM Frequency** | 4-byte Array | Configures `clk_div`, `clk_src`, and `pwm_div` dynamically. |
| **Vbias & Offset** | 10-byte Array | Sets target bias and Op-Amp rail zero-calibration. |
| **PWM State** | 8-bit uint | Software ON/OFF toggle (1 = ON, 0 = OFF). |
| **Battery Level** | 16-bit uint | Real-time Vbat telemetry (mV) with UVP status. |

---

## 🛠 Tech Stack
* **Microcontroller:** Renesas (Dialog) DA14531-00 SmartBond TINY™
* **Core:** ARM Cortex-M0+
* **SDK:** Dialog SmartBond SDK6 (v6.0.24.1464)
* **Peripherals:** * **GPADC:** Configured with oversampling and chopping for low-noise sensing.
    * **Timer 2:** Utilized for high-precision PWM generation.
    * **SysCntl:** Buck-mode DCDC management for optimized power efficiency.
* **Development:** Keil uVision 5 with ARM Compiler 6.

---

## 📂 Project Structure
This repository follows the Dialog SDK6 design patterns, with application-specific logic decoupled from the hardware abstraction layer.

* **`user_empty_peripheral_template.c/.h`**
  * The **System Coordinator**. Manages the main event loop, UVP state machine, and the mathematical control logic for PWM compensation.
* **`user_periph_setup.c/.h`**
  * The **Hardware Abstraction Layer (HAL)**. Implements a dual-configuration setup using the `BOARD_CUSTOM_PCB` preprocessor toggle to switch between the **DA14531 USB DevKit** and the final **Custom PCB** pinout.
* **`user_custs1_def.c/.h`**
  * The **BLE Database**. Defines the 128-bit custom service and characteristic structures for the Uric Acid sensor telemetry.
* **`user_config.h`**
  * **System Configuration**. Centralizes settings for the `ARCH_EXT_SLEEP_ON` power state and high-performance advertising intervals (687.5ms).

---

## ⚙️ Getting Started

### Prerequisites
* **Keil uVision 5**.
* **Dialog SmartBond SDK6** (v6.0.24.1464).
* **DA14531 Development Kit** or custom hardware based on the DA14531-00.

### Installation & Build
1. Clone the repository:
   ```bash
   git clone [https://github.com/albertannguyen/fukuoka_uric_acid_sensor.git](https://github.com/albertannguyen/fukuoka_uric_acid_sensor.git)
   ```
2. Open the project file `*.uvprojx` in Keil uVision.
3. **Configuration:** To target the custom sensor hardware, ensure `#define BOARD_CUSTOM_PCB` is enabled in `user_periph_setup.h`.
4. Build the target and flash using a **J-Link** debugger.

---

## 📝 Technical Notes & Optimization
* **NMI Hardfault Mitigation:** During development, returning `KEEP_POWERED` in the system-powered callback led to NMI hardfaults. This was resolved by implementing a `GOTO_SLEEP` return, allowing the SDK kernel to manage power-state transitions safely.
* **Low-Power Accuracy:** By configuring PWM pins with reduced current driving strength (`GPIO_POWER_RAIL_1V`), the system achieves higher stability for the ±1V sensing rails required for electrochemical analysis.
* **Watchdog Management:** The Watchdog Timer is explicitly frozen during sensitive UVP (Undervoltage Protection) logic checks and resumed immediately after, ensuring system reliability without risking unintended resets during critical battery telemetry processing.
