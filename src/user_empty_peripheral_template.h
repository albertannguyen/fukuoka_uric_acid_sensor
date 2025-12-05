/**
 ****************************************************************************************
 * @file user_empty_peripheral_template.h
 * @brief Empty peripheral template project header file.
 * @author Albert Nguyen
 ****************************************************************************************
 */

#ifndef _USER_EMPTY_PERIPHERAL_TEMPLATE_H_
#define _USER_EMPTY_PERIPHERAL_TEMPLATE_H_

/**
 ****************************************************************************************
 * @addtogroup APP
 * @ingroup RICOW
 * @brief 
 * @{
 ****************************************************************************************
 */

/*
 ****************************************************************************************
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "rwble_config.h"
#include "app_task.h"                  // application task
#include "gapc_task.h"                 // gap functions and messages
#include "gapm_task.h"                 // gap functions and messages
#include "app.h"                       // application definitions
#include "co_error.h"                  // error code definitions

/****************************************************************************
Add here supported profiles' application header files.
i.e.
#if (BLE_DIS_SERVER)
#include "app_dis.h"
#include "app_dis_task.h"
#endif
*****************************************************************************/

// For user callback functions
#include "arch_api.h"

// For ADC functions
#include "adc_531.h"

// For PWM functions
#include "timer0_2.h"
#include "timer2.h"

// For BLE handler functions
#include "gapc_task.h" // gap functions and messages
#include "gapm_task.h" // gap functions and messages
#include "custs1_task.h"

// For user_periph_setup.c
#include <stdbool.h>
extern bool uvp_shutdown;

/*
 ****************************************************************************************
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */

 /**
 ****************************************************************************************
 * @brief UVP (for battery voltage) periodic timer callback.
 *
 * @details
 * - Samples the VBAT_HIGH ADC channel (single-shot).
 * - Converts raw sample to millivolts.
 * - Compares to a chosen undervoltage shutdown threshold (1825 mV) and a restart threshold (1875 mV) using hysteresis logic.
 * - If shutdown is triggered, it disables the PWM VBIAS and the sensor sampling timer.
 * - If phone notifications are enabled and the app is connected,
 * a BLE notification is built and sent containing the 16-bit
 * battery voltage (mV) in **little-endian** byte order (LSB first).
 *
 * @note app_easy_timer ticks are 10 ms each in SDK6; this callback reschedules
 * itself every 500 ms or 0.5 s via app_easy_timer(50, ...).
 * @sa gpadc_init_se, gpadc_collect_sample, gpadc_sample_to_mv, KE_MSG_ALLOC_DYN, KE_MSG_SEND
 ****************************************************************************************
 */
void uvp_wireless_timer_cb(void);

/**
 ****************************************************************************************
 * @brief Sensor Voltage periodic timer callback.
 * @details
 *  - Initializes ADC for the sensor input (hardware-dependant).
 *  - Performs a single ADC conversion and converts the result to millivolts.
 *  - Builds and sends a BLE notification with the 16-bit sensor voltage (mV),
 *    sent in little-endian byte order (LSB first).
 *  - Disables ADC to reduce power once sample and notification are done.
 *  - If the BLE connection remains active, it restarts the timer every 1 s.
 *
 * @note Client must wrie to CCCD to enable sensor notifications and for sampling/timers to run.
 * @sa gpadc_init_se, gpadc_collect_sample, gpadc_sample_to_mv, KE_MSG_ALLOC_DYN, KE_MSG_SEND, ke_state_get
 ****************************************************************************************
 */
void gpadc_wireless_timer_cb(void);
 
/**
 ****************************************************************************************
 * @brief Initialize the ADC in single-ended mode with given parameters.
 *
 * @param[in] input            		ADC input channel (e.g., P0_6, VBAT_HIGH).
 * @param[in] smpl_time_mult   		Sample time multiplier (1–15).
 * @param[in] input_attenuator 		Attenuation factor for the ADC input.
 *														 		Must be one of the values from adc_input_attn_t enum.
 * @param[in] chopping         		Enable or disable chopping.
 * @param[in] oversampling     		Oversampling setting (0–7).
 *
 * @details This helper wraps adc_init() and performs additional recommended
 *          setup calls:
 *            - disable input shift
 *            - disable die temperature sensor
 *            - set ADC startup delay (adc_delay_set(64) for 16 MHz system)
 *            - enable ADC's LDO load current
 *            - reset and calibrate ADC offset
 *
 * @note Input mode is fixed to single-ended in the implementation.
 *       ADC should be disabled before re-configuring.
 * @sa adc_init, adc_input_shift_disable, adc_temp_sensor_disable, adc_delay_set, adc_offset_calibrate
 ****************************************************************************************
 */
void gpadc_init_se(adc_input_se_t input, uint8_t smpl_time_mult, adc_input_attn_t input_attenuator, bool chopping, uint8_t oversampling);

/**
 ****************************************************************************************
 * @brief Collect a corrected raw ADC sample.
 *
 * @return Corrected raw ADC sample (already processed with adc_correct_sample()).
 *
 * @details This function calls adc_get_sample() and then applies the SDK
 *          correction helper adc_correct_sample() to return a usable raw sample.
 * @sa adc_get_sample, adc_correct_sample
 ****************************************************************************************
 */
uint16_t gpadc_collect_sample(void);

/**
 ****************************************************************************************
 * @brief Convert a corrected ADC sample to a millivolt value.
 *
 * @param[in] sample		Corrected ADC sample value from gpadc_collect_sample().
 * @return Corrected ADC sample value in millivolts (mV).
 *
 * @details The conversion accounts for:
 *   - base ADC resolution (10 bits) plus any oversampling contribution,
 *   - the ADC internal reference (900 mV) scaled by the input attenuator setting.
 *
 * @note The returned uint16_t represents millivolts. When transmitted over BLE
 *       the two payload bytes are placed LSB-first (little-endian).
 * @sa adc_get_oversampling, GetBits16(GP_ADC_CTRL2_REG, GP_ADC_ATTN)
 ****************************************************************************************
 */
uint16_t gpadc_sample_to_mv(uint16_t sample);
 
 /**
 ****************************************************************************************
 * @brief Periodic timer callback that runs the control loop to adjust the PWM Duty Cycle (DC) to compensate for battery voltage (VBAT) changes.
 *
 * @details This function is executed periodically (every 50ms).
 * It sequentially calls the main control function, `timer2_pwm_dc_control`, for each active PWM channel
 * (`TIM2_PWM_2` and `TIM2_PWM_3`) to update their Duty Cycles.
 * * **Control Sequence:**
 * 1. Restarts the timer (`app_easy_timer`).
 * 2. Reads the global battery voltage (VBAT) from the last ADC reading.
 * 3. Calls `timer2_pwm_dc_control` to calculate the new PWM pulse width.
 * 4. Writes the result to the PWM hardware register.
 *
 * @sa timer2_pwm_dc_control, gpadc_collect_sample
 ****************************************************************************************
 */
void timer2_pwm_dc_control_timer_cb(void);
 
 /**
 ****************************************************************************************
 * @brief Calculates and updates the PWM Duty Cycle (DC) value based on a target output voltage and the current battery voltage (VBAT).
 *
 * @param[in] target_vbias_mv	  The desired DC target voltage (in mV) for the channel.
 * @param[in] channel           The Timer2 PWM channel to be controlled (`TIM2_PWM_CHANNEL1` to `TIM2_PWM_CHANNEL7`).
 *
 * @details This function implements the core control logic, calculating the precise Duty Cycle required
 * to maintain the target output voltage despite fluctuations in the battery voltage (VBAT). It is typically called
 * by the periodic timer (`timer2_pwm_dc_control_timer_cb()`).
 *
 * * **Control Logic:** This function reads VBAT, applies it to the compensation formula, calculates the necessary PWM pulse width,
 * and writes the resulting `END_CYCLE` value to the hardware register for the specified channel.
 *
 * @sa timer2_pwm_dc_control_timer_cb, timer2_pwm_set_offset
 ****************************************************************************************
 */
void timer2_pwm_dc_control(int16_t target_vbias_mv, tim2_pwm_t channel);
 
 /**
 ****************************************************************************************
 * @brief Directly sets the PWM offset (`START_CYCLE`) as a fixed Duty Cycle (DC) percentage for a channel.
 *
 * @param[in] offset_percentage	The desired PWM Duty Cycle (DC) offset, expressed as a percentage (0-100).
 * @param[in] channel           The Timer2 PWM channel to configure (`TIM2_PWM_CHANNEL1` to `TIM2_PWM_CHANNEL7`).
 *
 * @details This function is used to apply a fixed Duty Cycle (DC) offset to the PWM signal
 * by writing the corresponding timer count value directly to the channel's `START_CYCLE` register.
 * This function is separate from the main VBAT compensation control loop entirely.
 *
 * @note The calculated `START_CYCLE` value depends on the configured Timer2 period.
 *
 * @sa timer2_pwm_set_dc_and_offset
 ****************************************************************************************
 */
void timer2_pwm_set_offset(uint8_t offset_percentage, tim2_pwm_t channel);

/**
 ****************************************************************************************
 * @brief Configure Timer2 PWM frequency.
 *
 * @param[in] clk_div    Clock divider enum (tim0_2_clk_div_t). Encoded as shift (1 << clk_div).
 * @param[in] clk_src 	 Timer2 clock source enum (tim2_clk_src_t): LP (32 kHz) or SYS (32 MHz).
 * @param[in] pwm_div	 	 PWM divider (2 to 16383). This function clamps out-of-range values.
 *
 * @details
 *   - Computes input_freq = clk_freq / (1 << clk_div).
 *   - Configures timer0_2 clock division and timer2 configuration structs.
 *   - Calls timer2_pwm_freq_set(input_freq / pwm_div, input_freq) to program the timer.
 *
 * @note After setting frequency, duty cycle and offsets are configured separately
 *       via timer2_pwm_set_dc_and_offset() and the output enabled with timer2_pwm_enable().
 * @sa timer0_2_clk_div_set, timer2_config, timer2_pwm_freq_set
 ****************************************************************************************
 */
void timer2_pwm_set_frequency(tim0_2_clk_div_t clk_div, tim2_clk_src_t clk_src, uint16_t pwm_div);

/**
 ****************************************************************************************
 * @brief Enable Timer2 PWM outputs.
 *
 * @details Enables timer input clock via timer0_2_clk_enable() and starts Timer2 via
 *          timer2_start(). Ensure PWM frequency, duty cycles, and offsets are configured prior to enabling.
 * @sa timer0_2_clk_enable, timer2_start, timer2_pwm_set_frequency, timer2_pwm_set_dc_and_offset
 ****************************************************************************************
 */
void timer2_pwm_enable(void);

/**
 ****************************************************************************************
 * @brief Disable Timer2 PWM outputs.
 *
 * @details Stops Timer2 and disables the timer input clock to save power. Per-channel
 *          PWM settings are retained in registers and will take effect again when enabled.
 * @sa timer2_stop, timer0_2_clk_disable
 ****************************************************************************************
 */
void timer2_pwm_disable(void);

/**
 ****************************************************************************************
 * @brief Called when a BLE central connects to the device.
 *
 * @param[in] connection_idx    Connection index assigned by the stack.
 * @param[in] param           	Pointer to GAPC_CONNECTION_REQ_IND data provided by the stack.
 *
 * @sa default_app_on_connection
 ****************************************************************************************
 */
void user_on_connection(const uint8_t conidx, struct gapc_connection_req_ind const *param);

/**
 ****************************************************************************************
 * @brief Called when a BLE central disconnects.
 *
 * @param[in] param    Pointer to GAPC_DISCONNECT_IND provided by the stack.
 *
 * @sa default_app_on_disconnect
 ****************************************************************************************
 */
void user_on_disconnect(struct gapc_disconnect_ind const *param);

/**
 ****************************************************************************************
 * @brief Handles unprocessed messages (not handled by the SDK) and dispatches custom handlers.
 *
 * @param[in] msgid   	 Message ID of the incoming message.
 * @param[in] param   	 Pointer to the message parameters.
 * @param[in] dest_id    Task ID of the receiver.
 * @param[in] src_id  	 Task ID of the sender.
 *
 * @details
 *  - Routes write indications to the appropriate user handlers.
 *  - Routes read requests to read handlers or replies with ATT_ERR_APP_ERROR.
 *  - Confirms GATTC_EVENT_REQ_IND events to avoid GATT timeouts.
 *
 * @sa CUSTS1_VAL_WRITE_IND, CUSTS1_VALUE_REQ_IND, GATTC_EVENT_REQ_IND, KE_MSG_ALLOC, KE_MSG_SEND
 ****************************************************************************************
 */
void user_catch_rest_hndl(ke_msg_id_t const msgid, void const *param, ke_task_id_t const dest_id, ke_task_id_t const src_id);

/**
 ****************************************************************************************
 * @brief Handle CCCD writes to the Sensor Voltage characteristic.
 *
 * @param[in] msgid   Message ID (CUSTS1_VAL_WRITE_IND).
 * @param[in] param   Pointer to custs1_val_write_ind.
 * @param[in] dest_id Receiver task id.
 * @param[in] src_id  Sender task id.
 *
 * @details Copies the 2-byte CCCD into a local variable and:
 *    - if CCCD = 0x0001 and connected, starts the periodic sensor voltage ADC timer (1 s).
 *    - if CCCD = 0x0000, cancels the sensor voltage timer.
 * @sa app_easy_timer, gpadc_wireless_timer_cb
 ****************************************************************************************
 */
void user_svc1_sensor_voltage_cfg_ind_handler(ke_msg_id_t const msgid,
                                         struct custs1_val_write_ind const *param,
                                         ke_task_id_t const dest_id,
                                         ke_task_id_t const src_id);

/**
 ****************************************************************************************
 * @brief Handle client writes to PWM Frequency characteristic.
 *
 * @param[in] msgid   Message ID (CUSTS1_VAL_WRITE_IND).
 * @param[in] param   Pointer to custs1_val_write_ind.
 * @param[in] dest_id Receiver task id.
 * @param[in] src_id  Sender task id.
 *
 * @details
 *  - Expected byte array payload: [clk_div, clk_src, pwm_div_MSB, pwm_div_LSB].
 *  - Validates byte array length and ignores incomplete writes.
 *  - Validates clk_div and clk_src within enum ranges and invalid writes.
 *  - Calls timer2_pwm_set_frequency(...) which clamps pwm_div to datasheet range.
 * @sa timer2_pwm_set_frequency, TIM0_2_CLK_DIV_*, TIM2_CLK_*, CLAMP
 ****************************************************************************************
 */
void user_svc1_pwm_freq_wr_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id);

/**
 ****************************************************************************************
 * @brief BLE Write Indication Handler for the PWM VBIAS and Offset Characteristic.
 *
 * @param[in] msgid         Message ID (CUSTS1_VAL_WRITE_IND).
 * @param[in] param         Pointer to custs1_val_write_ind structure containing the characteristic value.
 * @param[in] dest_id       Receiver task id.
 * @param[in] src_id        Sender task id.
 *
 * @details This function is executed when a BLE central device (e.g., a phone) writes to the custom
 * PWM VBIAS and Offset characteristic. It is responsible for parsing the 10-byte payload
 * and immediately applying the requested VBIAS target voltages and PWM offsets for
 * channels `TIM2_PWM_2` and `TIM2_PWM_3`.
 *
 * The handler performs the following critical steps:
 * 1. **Guard Check:** Prevents changes if the Under Voltage Protection (UVP) shutdown is active.
 * 2. **Validation:** Checks if the received packet length is exactly 10 bytes.
 * 3. **Data Parsing:** Extracts two sets of `vbias_mv` (target voltage in mV), `zero_cal` (zero-voltage calibration value), and `offset` (Duty Cycle percentage).
 * 4. **Compensation:** Subtracts the `zero_cal` value from the raw `vbias_mv` targets to compensate for Op Amp rail offsets.
 * 5. **Clamping:** Clamps the resulting target voltages to the hardware-safe range (±1000 mV).
 * 6. **PWM Configuration:** Calls `timer2_pwm_set_offset` for the new `START_CYCLE` value, and `timer2_pwm_dc_control` to set the initial PWM Duty Cycle (DC) based on the new targets.
 * 7. **Global Update:** Updates the global `target_vbias_1_mv` and `target_vbias_2_mv` variables, which the periodic compensation loop (`timer2_pwm_dc_control_timer_cb`) uses for subsequent Duty Cycle updates.
 *
 * @sa timer2_pwm_set_offset, timer2_pwm_dc_control, timer2_pwm_dc_control_timer_cb
 ****************************************************************************************
 */		
void user_svc1_pwm_vbias_and_offset_wr_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id);
															 
/**
 ****************************************************************************************
 * @brief Handle client write to PWM State characteristic (single-byte ON/OFF).
 *
 * @param[in] msgid   Message ID (CUSTS1_VAL_WRITE_IND).
 * @param[in] param   Pointer to custs1_val_write_ind.
 * @param[in] dest_id Receiver task id.
 * @param[in] src_id  Sender task id.
 *
 * @details
 *  - Reads a single byte state: 0 = OFF, 1 = ON.
 *  - If ON: calls timer2_pwm_enable() to enable Timer2 clock and start outputs.
 *  - If OFF: calls timer2_pwm_disable() to stop outputs and disable timer clock.
 *  - Ignores invalid writes (values > 1).
 *
 * @note
 *  - Ensure PWM frequency and duty are configured before enabling output to avoid unexpected signals.
 *  - timer2_pwm_disable() does not clear configurations; re-enable preserves settings.
 * @sa timer2_pwm_enable, timer2_pwm_disable
 ****************************************************************************************
 */
void user_svc1_pwm_state_wr_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id);

/**
 ****************************************************************************************
 * @brief Handle CCCD writes for Battery Voltage characteristic.
 *
 * @param[in] msgid   Message ID (CUSTS1_VAL_WRITE_IND).
 * @param[in] param   Pointer to custs1_val_write_ind.
 * @param[in] dest_id Receiver task id.
 * @param[in] src_id  Sender task id.
 *
 * @details
 *  - Copies the 2-byte CCCD into a local variable and writes it to retained memory.
 *  - uvp_wireless_timer_cb() checks uvp_cccd_value and the connection state to decide whether to
 *    build and send periodic battery notifications.
 *
 * @note The handler itself does not start/stop the UVP timer,
 *			 it only determines whether notifications will be sent
 * @sa uvp_wireless_timer_cb, KE_MSG_ALLOC_DYN, app_easy_timer
 ****************************************************************************************
 */
void user_svc1_battery_voltage_cfg_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id);

/**
 ****************************************************************************************
 * @brief Handle read request for Sensor Voltage characteristic and respond with last-known value.
 *
 * @param[in] msgid   Message ID (CUSTS1_VALUE_REQ_IND).
 * @param[in] param   Pointer to custs1_value_req_ind.
 * @param[in] dest_id Receiver task id.
 * @param[in] src_id  Sender task id.
 *
 * @details
 *  - Allocates a dynamic message response.
 *  - Copies sensor voltage into payload (2 bytes, little-endian) and sends message.
 *
 * @note Uses retained sensor_adc_sample_mv so reads succeed even if sampling timer is stopped.
 * @sa KE_MSG_ALLOC_DYN, KE_MSG_SEND, app_env
 ****************************************************************************************
 */
void user_svc1_read_sensor_voltage_handler(ke_msg_id_t const msgid,
                                           struct custs1_value_req_ind const *param,
                                           ke_task_id_t const dest_id,
                                           ke_task_id_t const src_id);
																					 
/**
 ****************************************************************************************
 * @brief Handle read request for Battery Voltage characteristic and respond with last-known value.
 *
 * @param[in] msgid   Message ID (CUSTS1_VALUE_REQ_IND).
 * @param[in] param   Pointer to custs1_value_req_ind.
 * @param[in] dest_id Receiver task id.
 * @param[in] src_id  Sender task id.
 *
 * @details
 *  - Allocates a dynamic message response.
 *  - Copies battery voltage into payload (2 bytes, little-endian) and sends message.
 *
 * @sa KE_MSG_ALLOC_DYN, KE_MSG_SEND, uvp_wireless_timer_cb
 ****************************************************************************************
 */
void user_svc1_read_battery_voltage_handler(ke_msg_id_t const msgid,
                                           struct custs1_value_req_ind const *param,
                                           ke_task_id_t const dest_id,
                                           ke_task_id_t const src_id);

/**
 ****************************************************************************************
 * @brief User callback when the system is powered on.
 *
 * @return arch_main_loop_callback_ret_t  GOTO_SLEEP to allow SDK to manage sleep mode.
 *
 * @details
 *   - Freezes watchdog, starts UVP timer once, resumes watchdog,
 *     and returns GOTO_SLEEP (KEEP_POWERED caused hardfault during debugging).
 * @sa wdg_freeze, wdg_resume, app_easy_timer
 ****************************************************************************************
 */
arch_main_loop_callback_ret_t user_app_on_system_powered(void);

/**
 ****************************************************************************************
 * @brief User initialization callback executed at boot.
 *
 * @details Sets retained variables to safe defaults and calls default_app_on_init()
 *          as the last action to allow the SDK to finalize initialization.
 * @sa default_app_on_init
 ****************************************************************************************
 */
void user_app_on_init(void);

/// @} APP

#endif
