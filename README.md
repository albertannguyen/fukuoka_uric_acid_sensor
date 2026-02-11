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

2. **Internal UVP with Software Hysteresis:** Replaced external hardware voltage supervisors with an internal ADC-driven **Undervoltage Protection (UVP)** state machine. By sampling `VBAT_HIGH` and implementing a dual-threshold hysteresis (1850 mV/1900 mV), the firmware manages battery health and system stability entirely in software, saving valuable PCB space.

3. **Dynamic PWM Control Loop:** Implemented a feed-forward compensation algorithm to maintain sensor excitation stability. The firmware dynamically calculates and adjusts PWM duty cycles based on real-time battery fluctuations, ensuring the electrochemical sensor receives a precise ±1 V bias signal regardless of battery state.

4. **Defensive Architecture & System Robustness:** The firmware incorporates strict input clamping, Op-Amp rail offset compensation via remote sensor calibration, and strategic power management. The safeguards ensure a stable boot-up sequence and prevent system hard faults even under complex asynchronous BLE event handling. The other improvements increase sensor runtime and accuracy in the final measurements.

---

## 🚀 Advanced Features & Implementation

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

### 2. Internal UVP with Software Hysteresis
The firmware replaces the external hardware voltage supervisor from the initial prototype with an integrated UVP routine that takes advantage of the DA14531's ability to measure its supply rails using the ADC.

* **The Logic:** The system periodically samples the internal `VBAT_HIGH` ADC channel to monitor battery voltage. Based on these readings, the firmware manages a GPIO pin that serves as the hardware enable signal for the SoC and all external peripherals.

* **Hysteresis Control:** To prevent the system from rapidly power-cycling due to minor battery voltage fluctuations near the cutoff point, the firmware implements a dual-threshold state machine:
    * **At Shutdown (1850 mV):** The GPIO is pulled low, shutting down the analog front-end and putting the SoC into sleep mode.
    * **At Restart (1900 mV):** Normal operation only resumes once the battery has recovered sufficiently, ensuring a stable boot-up process.

* **Engineering Impact:** Migrating this supervisor logic from a dedicated IC to firmware reduced the Bill of Materials (BOM) and saved critical PCB space, which is essential for the device's compact wearable form factor.

### 3. Remote Calibration & Memory Retention
To ensure measurement accuracy, the firmware incorporates a calibration routine that compensates for non-ideal hardware behavior, specifically targeting voltage offsets in the analog front-end.

* **Charge Pump Offset Compensation:** Due to non-ideal performance of the hardware charge pump providing the negative rail for the Op-Amps, the center point (0 V) of the excitation signal can shift.

* **Calibration Process:** Using a Digital Multimeter (DMM), the real-world offset is measured and sent to the device via a dedicated BLE characteristic. The characteristic handler function subtracts this `zero_cal` value from the target voltage in real-time, centering the excitation rails and ensuring the sensor receives the intended bias.

* **State Retention & Sleep Management:** To maximize battery life, the SoC utilizes **Extended Sleep Mode**. Critical system variables such as ADC samples and target bias voltages are stored in a designated retention section of the RAM (`retention_mem_area0`). This hardware-level data retention ensures that when the chip wakes up from a sleep cycle, it immediately resumes operation with the correct values without requiring a re-sync from the mobile app.

---

## 📡 BLE Service Definition (GATT)
The firmware implements a custom 128-bit UUID service for sensor control and telemetry via a mobile device. All data is transmitted in **Little-Endian** format due to hardware architecture. This requires the mobile application to flip the byte order before converting from **Hex to Decimal** to obtain the correct millivolt reading.

| Characteristic | Properties | Length | User Description |
| :--- | :--- | :--- | :--- |
| **Sensor Voltage** | Read/Notify | 2 Bytes | Sensor Voltage (little-endian bytes to mV) |
| **PWM Frequency** | Write | 4 Bytes | Timer2 PWM Frequency Config |
| **PWM Vbias & Offset** | Write | 10 Bytes | Timer2 PWM2 and PWM3 Vbias and Offsets |
| **PWM State** | Write | 1 Byte | Timer2 PWM State On/Off |
| **Battery Voltage** | Read/Notify | 2 Bytes | Battery Voltage (little-endian bytes to mV) |

---

## 🛠 Tech Stack
**Microcontroller:** Dialog Semiconductor (Renesas) DA14531-00 (Base variant)
**Development Environment:** Keil uVision 5
**SDK:** Dialog SmartBond SDK6 (v6.0.24.1464)
**Communication:** Bluetooth Low Energy (BLE 5.1)

---

## 📂 Project Structure
This firmware leverages the `empty_peripheral_template` project framework provided by the Dialog SDK. While the skeletal structure follows the SDK’s design patterns, the core application logic and peripheral driver integrations are custom implementations tailored for the final hardware.

### 🛠 Hardware & System Configuration
* **`user_periph_setup.c/.h`**: Defines the GPIO pinmuxing and hardware initialization for the SoC.
* **`user_callback_config.h`**: Reroutes the SDK main loop by implementing user callback functions to manage autonomous system execution.
* **`user_config.h`**: Configures the sleep mode and defines the device advertising parameters.
* **`user_modules_config.h`**: Configures the inclusion of SDK software modules. Enables the BLE module that holds the custom service.
* **`da14531_config_basic.h`**: Configures UART output to establish a serial debugging interface for hardware development.

### 🧠 User Application
* **`user_empty_peripheral_template.c/.h`**: The primary user application layer.

### 📡 BLE & GATT Implementation
* **`user_custs1_def.c/.h`**: Defines the structure of the custom GATT database. It specifies the 128-bit UUIDs, attributes, indexing, and permissions for the user-defined characteristics. This file acts as the primary interface between the firmware and any central BLE device.

---

## ⚙️ Getting Started

### Prerequisites
* **Keil uVision 5** with ARM Compiler support.
* **Dialog SmartBond SDK6** (v6.0.24.1464 or compatible).
* **DA14531 Development Kit** or custom hardware based on the DA14531-00.

### Installation & Build
1. To maintain the relative include paths, clone this repository into your local SDK directory at: `...\DA145xx_SDK\6.0.24.1464\projects\target_apps\template\fukuoka_uric_acid_sensor`
2. Navigate to the `Keil_5` folder within the project and launch the `*.uvprojx` file in Keil uVision.
3. Build the target and flash using a **J-Link** debugger.

---

## 📝 Technical Notes & Optimization

* **Experimental Rail Optimization:** Through hardware validation using a Digital Multimeter (DMM), it was determined that configuring the PWM GPIOs with reduced current drive significantly improved output accuracy. This configuration minimizes switching noise and brings the physical sensor excitation voltage into closer alignment with the theoretical ±1V model, ensuring higher data integrity for the electrochemical front-end.
