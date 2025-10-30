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
extern bool flag_gpio_uvp;

/*
 ****************************************************************************************
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */
 
/**
 ****************************************************************************************
 * @brief UVP (for battery voltage) periodic timer callback.
 * @details
 *  - Samples the VBAT_HIGH ADC channel (single-shot).
 *  - Converts raw sample to millivolts.
 *  - Compares to a chosen undervoltage threshold (1900 mV).
 *  - Toggles UVP_MAX_SHDN GPIO to disable/enable amplifiers accordingly.
 *  - If phone notifications are enabled and the app is connected,
 *		a BLE notification is built and sent containing the 16-bit
 *    battery voltage (mV) in **little-endian** byte order (LSB first).
 *
 * @note app_easy_timer ticks are 10 ms each in SDK6; this callback reschedules
 *       itself every 500 ms or 0.5 s.
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
 * @param[in] continuous       		Enable or disable continuous sampling mode.
 * @param[in] interval_mult    		Interval multiplier for continuous mode (0–255).
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
void gpadc_init_se(adc_input_se_t input, uint8_t smpl_time_mult, bool continuous, uint8_t interval_mult, adc_input_attn_t input_attenuator, bool chopping, uint8_t oversampling);

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
 * @brief Set PWM duty cycle and offset for TIM2 channels 2 and 3.
 *
 * @param[in] dc_pwm2     	 Duty cycle for PWM2 in percent (0 to 100).
 * @param[in] offset_pwm2    Offset for PWM2 in percent (0 to 100).
 * @param[in] dc_pwm3     	 Duty cycle for PWM3 in percent (0 to 100).
 * @param[in] offset_pwm3 	 Offset for PWM3 in percent (0 to 100).
 *
 * @details Values are clamped to 0 to 100 and loaded into the timer via timer2_pwm_signal_config().
 *          This function does not start the timer — call timer2_pwm_enable() to begin outputs.
 * @sa timer2_pwm_signal_config, timer2_pwm_enable
 ****************************************************************************************
 */
void timer2_pwm_set_dc_and_offset(uint8_t dc_pwm2, uint8_t offset_pwm2, uint8_t dc_pwm3, uint8_t offset_pwm3);
 
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

// TODO: see if connection or disconnection function need to be used for device operation
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
 * @brief Handle client writes to PWM duty cycle and offset characteristic.
 *
 * @param[in] msgid   Message ID (CUSTS1_VAL_WRITE_IND).
 * @param[in] param   Pointer to custs1_val_write_ind.
 * @param[in] dest_id Receiver task id.
 * @param[in] src_id  Sender task id.
 *
 * @details
 *  - Expected byte array payload: [dc_pwm2, offset_pwm2, dc_pwm3, offset_pwm3] where each is 0 to 100%.
 *  - Validates byte array length and ignores incomplete writes.
 *  - Calls timer2_pwm_set_dc_and_offset(...) which clamps all inputs to [0,100].
 * @sa timer2_pwm_set_dc_and_offset, CLAMP, timer2_pwm_enable
 ****************************************************************************************
 */
void user_svc1_pwm_dc_and_offset_wr_ind_handler(ke_msg_id_t const msgid,
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
