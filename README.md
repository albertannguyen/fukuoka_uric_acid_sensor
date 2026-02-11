# Fukuoka Uric Acid Sensor - Production Firmware

![C](https://img.shields.io/badge/Language-C-blue.svg)
![SoC](https://img.shields.io/badge/SoC-DA14531--00-orange.svg)
![SDK](https://img.shields.io/badge/SDK-Dialog--6.0.24.1464-green.svg)
![Build](https://img.shields.io/badge/Build-Keil--uVision--5-lightgrey.svg)
![License](https://img.shields.io/badge/License-MIT-yellow.svg)

## 📖 Introduction
This repository contains the finalized firmware for the **Fukuoka Uric Acid Sensor**, developed on the **DA14531 SmartBond TINY™ SoC**. While the initial prototype established basic operation, this codebase represents a fully integrated system designed for high-accuracy sensing and long-term battery reliability.

### 🔄 Key Advancements & Evolution
This version introduces four major system-level upgrades over the initial prototype:

1. **Full Custom BLE GATT Server:** Created a custom 128-bit UUID service. This includes asynchronous notification handlers, dynamic read/write indications, and a structured GATT database for real-time telemetry and remote configuration of the hardware.

2. **Internal UVP with Software Hysteresis:** Replaced external hardware voltage supervisors with an internal ADC-driven Undervoltage Protection (UVP) state machine. By sampling `VBAT_HIGH` and implementing a dual-threshold hysteresis (1850 mV/1900 mV), the firmware manages battery health and system stability entirely in software, saving valuable PCB space.

3. **Dynamic PWM Control Loop:** Implemented a feed-forward compensation algorithm to maintain sensor excitation stability. The firmware dynamically calculates and adjusts PWM duty cycles based on real-time battery fluctuations, ensuring the electrochemical sensor receives a precise ±1V bias regardless of battery state.

4. **Defensive Architecture & System Robustness:** The firmware incorporates strict input clamping, Op-Amp rail offset compensation via remote calibration, and strategic power management. The safeguards ensure a stable boot-up sequence and prevent system hard faults even under complex asynchronous BLE event handling. The other improvements increase sensor runtime and accuracy in the final measurements.

---

## 🚀 Advanced Features & Implementation

### 2. Under-Voltage Protection (UVP) with Hysteresis
A dedicated software state machine prevents erratic system reboots and battery damage during low-power states.
* **Hysteresis logic:** Implemented with a shutdown threshold of **1850 mV** and a restart threshold of **1900 mV**.
* **Safety Protocol:** Upon trigger, the system disables all PWM excitation and sensor sampling timers, entering a low-power "safe mode" while maintaining BLE telemetry for error reporting and diagnostics.

### 3. Power Optimization & Memory Retention
* **Extended Sleep:** Configured for `ARCH_EXT_SLEEP_ON` with an average current draw optimized for wearable longevity.
* **Retention RAM:** Critical system variables (UVP status, PWM offsets, and BLE CCCD configurations) are stored in `retention_mem_area0`. This ensures the device "remembers" its state and target bias voltages after waking from deep sleep cycles.
* **Buck Mode:** The DCDC converter is optimized for efficiency using `syscntl_dcdc_turn_on_in_buck(SYSCNTL_DCDC_LEVEL_1V1)`.

## 🚀 Advanced Features & Implementation

### 1. Battery-Aware PWM Compensation
To maintain electrochemical sensing accuracy as the battery discharges, the firmware implements a dynamic feed-forward compensation algorithm. This ensures the sensor excitation remains stable even as the supply voltage fluctuates.

* **The Compensation Formula:**
    The pulse width (PulseWidth) is dynamically adjusted based on the real-time battery voltage ($V_{bat}$) to maintain a precise target bias ($V_{target}$) on a specific part of the sensor:
    > **PulseWidth = (Period / 2) - (5 * $V_{target}$ * Period) / (7 * $V_{bat}$)**
* **Implementation:** Handled within `timer2_pwm_dc_control`, where the result is clamped and written to the `TRIPLE_PWM` registers.
* **Hardware Optimization:** Utilizes the DA14531 **1V Rail** configuration (`GPIO_POWER_RAIL_1V`) to reduce driving strength, resulting in a cleaner and more accurate ±1V signal.

### 1. Battery-Aware PWM Compensation
To maintain electrochemical sensing accuracy as the battery discharges, the firmware implements a dynamic feed-forward compensation algorithm. This ensures the sensor excitation remains stable even as the supply voltage fluctuates.

* **The Compensation Formula:**
  The pulse width is calculated as a fraction of the total timer period. In the DA14531 hardware, this count directly determines the duty cycle of the Timer 2 PWM signal:

$$PulseWidth = \frac{Period}{2} - \frac{5 \cdot V_{target} \cdot Period}{7 \cdot V_{bat}}$$

* **Variable Breakdown:**
    * **PulseWidth:** The calculated time where the PWM signal is toggled high in **timer ticks**. This is written to the `PWMx_END_CYCLE` register.
    * **Period:** The total duration of a PWM cycle in **timer ticks**. This value is read directly from the `TRIPLE_PWM_FREQUENCY` register.
    * **V_target:** The desired bias voltage in millivolts.
    * **V_bat:** The battery voltage sampled from the internal ADC in millivolts.

* **Integer Math & System Stability:**
The original control law was derived from circuit analysis and contained floating-point values. However, because the ARM Cortex-M0+ architecture lacks a dedicated Floating Point Unit (FPU), using floating-point variables led to firmware instability and system-wide crashes. 

To resolve this, the formula was refactored into **fixed-point integer math**. By utilizing 32-bit intermediate variables (`int32_t`) to prevent overflow during calculations, the equation remains highly accurate to the original model while using data types that the hardware and SDK are natively compatible with. This ensures the DA14531 can perform stable, high-speed duty cycle updates every 500 ms without the risk of total failure.

### 2. Under-Voltage Protection (UVP) with Hysteresis
To prevent erratic behavior and hardware brownouts, the firmware replaces external voltage supervisors with an internal ADC-driven state machine.

* **Hysteresis Logic:**
    * **Shutdown:** 1850 mV (System enters safe mode; all PWM and sensing timers disabled).
    * **Restart:** 1900 mV (System resumes normal operation).
* **Reliability:** By sampling `VBAT_HIGH` and applying software hysteresis, the system avoids "chatter" (rapidly toggling ON/OFF) when the battery level is near the threshold.

### 3. Remote Calibration & Offset Compensation
The system accounts for non-ideal analog components (such as Op-Amp input offsets) through a remote calibration routine.

* **Zero-Calibration:** A dedicated BLE characteristic allows the user to send a `zero_cal` value. The firmware subtracts this from the raw $V_{target}$ to perfectly center the excitation rails.
* **State Retention:** Calibration offsets and user configurations are stored in the SoC's **Retention RAM** (`retention_mem_area0`), ensuring settings persist across deep sleep cycles.

### 4. Power Domain Management
Optimized for the SmartBond™ architecture, the system utilizes the internal DCDC converter in **Buck Mode** (`SYSCNTL_DCDC_LEVEL_1V1`). This configuration maximizes efficiency during `ARCH_EXT_SLEEP_ON` periods while providing stable power to the analog front-end.

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
