/**
 ****************************************************************************************
 * @file user_empty_peripheral_template.h
 * @brief Empty peripheral template project header file.
 * @note Albert Nguyen
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

// for user callback functions
#include "arch_api.h"

// for ADC functions
#include "adc_531.h"

// for PWM functions
#include "timer0_2.h"
#include "timer2.h"

// for BLE handler functions
#include "gapc_task.h" // gap functions and messages
#include "gapm_task.h" // gap functions and messages
#include "custs1_task.h"

/*
 ****************************************************************************************
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Timer-driven callback for undervoltage protection (UVP) logic.
 * @details
 *      This function is called periodically by a timer. It:
 *          1. Reads battery voltage using the ADC.
 *          2. Compares voltage to a threshold (i.e. 1.9 V).
 *          3. Shuts down or enables external peripherals via GPIO based on result of comparison.
 *      Timer is automatically restarted after each call.
 * @sa gpadc_init, gpadc_collect_sample, gpadc_sample_to_mv, GPIO_SetActive, GPIO_SetInactive, app_easy_timer
 ****************************************************************************************
 */
void uvp_uart_timer_cb(void);

/**
 ****************************************************************************************
 * @brief Timer-driven callback for ADC conversions and BLE notifications.
 * @details
 *      This function periodically:
 *          1. Initializes and enables ADC for a specific input (i.e., P0_6).
 *          2. Collects ADC sample and converts it to millivolts.
 *          3. Sends the value over BLE notifications using the custom service.
 *					4. Disables the ADC
 * @note Timer is automatically restarted as long as a BLE connection is active.
 * @sa gpadc_init, gpadc_collect_sample, gpadc_sample_to_mv, KE_MSG_ALLOC_DYN, KE_MSG_SEND, ke_state_get, app_easy_timer
 ****************************************************************************************
 */
void gpadc_wireless_timer_cb(void);
 
 /**
 ****************************************************************************************
 * @brief Initialize ADC with specific configuration parameters.
 * @param[in] input            ADC input channel (e.g., P0_6, VBAT_HIGH).
 * @param[in] smpl_time_mult   Sample time multiplier (1–15).
 * @param[in] continuous       Enable or disable continuous sampling mode.
 * @param[in] interval_mult    Interval multiplier for continuous mode (0–255).
 * @param[in] input_attenuator Attenuation factor for the ADC input.
 *														 Must be one of the values from adc_input_attn_t enum.
 * @param[in] chopping         Enable or disable chopping.
 * @param[in] oversampling     Oversampling setting (0–7).
 * @note Input mode is fixed to single-ended. ADC must be off before changing config.
 * @sa adc_init, adc_input_shift_disable, adc_reset_offsets, adc_offset_calibrate, adc_temp_sensor_disable
 ****************************************************************************************
 */
void gpadc_init_se(adc_input_se_t input, uint8_t smpl_time_mult, bool continuous, uint8_t interval_mult, adc_input_attn_t input_attenuator, bool chopping, uint8_t oversampling);

/**
 ****************************************************************************************
 * @brief Collect a single ADC sample and apply corrections.
 * @details Reads raw ADC register, applies corrections based on current ADC config, and returns.
 * @return Corrected ADC sample as a 16-bit unsigned integer.
 * @sa adc_get_sample, adc_correct_sample
 ****************************************************************************************
 */
uint16_t gpadc_collect_sample(void);

/**
 ****************************************************************************************
 * @brief Converts raw ADC sample to millivolts.
 * @param[in] sample Raw ADC register value.
 * @return Voltage in millivolts.
 * @note Reference voltage and attenuation are accounted for.
 *			 This is a function given by Renesas.
 * @sa adc_get_oversampling
 ****************************************************************************************
 */
uint16_t gpadc_sample_to_mv(uint16_t sample);
 
/**
 ****************************************************************************************
 * @brief Sets the PWM frequency of Timer2.
 * @param[in] clk_div  Clock divider for Timer2 input (1, 2, 4, or 8). Must be one of
 *										 the values from tim0_2_clk_div_t enum.
 * @param[in] clk_src  Clock source selection for Timer2 (LP 32 kHz or SYS 16 MHz clock).
 *										 Must be one of the values from tim2_clk_src_t enum.
 * @param[in] pwm_div  Divider for PWM output (valid range: 2–16383).
 * @details
 *      Configures Timer2 with the desired input clock and PWM divider.
 *      The PWM frequency is calculated as: pwm_freq = input_freq / pwm_div,
 *      where input_freq = clk_freq / clk_div_int. Values are clamped to the valid range.
 *      After calling this function, duty cycle and offset must be configured separately
 *      using timer2_pwm_set_dc_and_offset(), and the output enabled via timer2_pwm_enable().
 * @sa timer0_2_clk_div_set, timer2_config, timer2_pwm_freq_set, CLAMP
 ****************************************************************************************
 */
void timer2_pwm_set_frequency(tim0_2_clk_div_t clk_div, tim2_clk_src_t clk_src, uint16_t pwm_div);

/**
 ****************************************************************************************
 * @brief Sets Timer2 PWM duty cycle and offset for two channels.
 * @param[in] dc_pwm2       Duty cycle for PWM2 (0–100%).
 * @param[in] offset_pwm2   Offset for PWM2 (0–100%).
 * @param[in] dc_pwm3       Duty cycle for PWM3 (0–100%).
 * @param[in] offset_pwm3   Offset for PWM3 (0–100%).
 * @details Clamps values to 0-100% and applies to Timer2 PWM outputs.
 *          Must call timer2_pwm_enable() afterward to start PWM.
 * @sa timer2_pwm_set_frequency, timer2_pwm_enable, CLAMP
 ****************************************************************************************
 */
void timer2_pwm_set_dc_and_offset(uint8_t dc_pwm2, uint8_t offset_pwm2, uint8_t dc_pwm3, uint8_t offset_pwm3);

/**
 ****************************************************************************************
 * @brief Enables Timer2 PWM outputs.
 * @details
 *      This function enables the Timer2 input clock and starts all configured PWM channels.
 *      Must call timer2_pwm_set_frequency() and timer2_pwm_set_dc_and_offset() before enabling
 *      to ensure the PWM operates with the desired frequency and duty cycle.
 * @sa timer2_pwm_set_frequency, timer2_pwm_set_dc_and_offset, timer0_2_clk_enable, timer2_start
 ****************************************************************************************
 */
void timer2_pwm_enable(void);

// WIP: validate ChatGPT comment
/**
 ****************************************************************************************
 * @brief Disables Timer2 PWM outputs and stops the timer input clock.
 * @details
 *      Stops PWM generation by calling the timer stop routine and then disables the
 *      Timer2 input clock to reduce power. This function does **NOT** clear per-channel
 *      PWM configuration (duty/offset) — those settings remain in the timer registers
 *      and will take effect again when the timer is re-enabled via timer2_pwm_enable().
 *      It is safe to call this function repeatedly; calling it when the timer is already
 *      stopped has no adverse effect.
 * @sa timer2_stop, timer0_2_clk_disable, timer2_pwm_enable, timer2_pwm_set_dc_and_offset
 ****************************************************************************************
 */
void timer2_pwm_disable(void);

// WIP: see if connection or disconnection function need to be used for device operation
/**
 ****************************************************************************************
 * @brief Connection function.
 * @param[in] conidx        Connection Id index
 * @param[in] param         Pointer to GAPC_CONNECTION_REQ_IND message
 * @sa default_app_on_connection, gpadc_wireless_timer_cb, timer2_pwm_init, timer2_pwm_enable
 ****************************************************************************************
 */
void user_on_connection(const uint8_t conidx, struct gapc_connection_req_ind const *param);

/**
 ****************************************************************************************
 * @brief Disconnection function.
 * @param[in] param         Pointer to GAPC_DISCONNECT_IND message
 * @sa default_app_on_disconnect, timer2_pwm_disable
 ****************************************************************************************
 */
void user_on_disconnect(struct gapc_disconnect_ind const *param);

/**
 ****************************************************************************************
 * @brief Handles unprocessed messages and dispatches custom handlers.
 * @param[in] msgid   Message ID of the incoming message.
 * @param[in] param   Pointer to the message parameters.
 * @param[in] dest_id Task ID of the receiver.
 * @param[in] src_id  Task ID of the sender.
 * @details
 *      This function handles messages not automatically processed by the SDK, including:
 *          - Dispatching write indications for custom characteristics to their respective
 *            user-defined handler functions.
 *          - Responding to GATTC_EVENT_REQ_IND messages to prevent GATT timeouts. The
 *            confirmation code for this case is provided directly by the Renesas SDK.
 * @note Only the handling of GATTC_EVENT_REQ_IND is provided by the SDK. Other cases
 *       (custom characteristic writes) are implemented by the user.
 * @sa KE_MSG_ALLOC, KE_MSG_SEND, user_svc1_sensor_voltage_cfg_ind_handler,
 *     user_svc1_pwm_freq_wr_ind_handler, user_svc1_pwm_dc_and_offset_wr_ind_handler,
 *     user_svc1_pwm_state_wr_ind_handler
 ****************************************************************************************
 */
void user_catch_rest_hndl(ke_msg_id_t const msgid, void const *param, ke_task_id_t const dest_id, ke_task_id_t const src_id);

/**
 ****************************************************************************************
 * @brief Handles client configuration writes to the Sensor Voltage characteristic.
 * @param[in] msgid   Message ID.
 * @param[in] param   Pointer to custs1_val_write_ind structure.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id  ID of the sending task instance.
 * @details Enables or disables BLE notifications for the sensor voltage characteristic
 *          based on the CCCD value written by the client. Starts/stops ADC timer accordingly.
 * @sa gpadc_wireless_timer_cb, app_easy_timer
 ****************************************************************************************
 */
void user_svc1_sensor_voltage_cfg_ind_handler(ke_msg_id_t const msgid,
                                         struct custs1_val_write_ind const *param,
                                         ke_task_id_t const dest_id,
                                         ke_task_id_t const src_id);

/**
 ****************************************************************************************
 * @brief Handles client writes to the PWM frequency characteristic.
 * @param[in] msgid   Message ID.
 * @param[in] param   Pointer to custs1_val_write_ind structure.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id  ID of the sending task instance.
 * @details Parses the 4-byte payload [clk_div, clk_src, pwm_div_MSB, pwm_div_LSB],
 *          validates values, and sets Timer2 PWM frequency.
 * @sa timer2_pwm_set_frequency, CLAMP
 ****************************************************************************************
 */																				 
void user_svc1_pwm_freq_wr_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id);

/**
 ****************************************************************************************
 * @brief Handles client writes to the PWM duty cycle and offset characteristic.
 * @param[in] msgid   Message ID.
 * @param[in] param   Pointer to custs1_val_write_ind structure.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id  ID of the sending task instance.
 * @details Parses the 4-byte payload [dc_pwm2, offset_pwm2, dc_pwm3, offset_pwm3],
 *          validates values, and sets Timer2 PWM duty cycle and offset for both channels.
 * @sa timer2_pwm_set_dc_and_offset, CLAMP
 ****************************************************************************************
 */
void user_svc1_pwm_dc_and_offset_wr_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id);
			
/**
 ****************************************************************************************
 * @brief Handles client writes to the PWM state characteristic.
 * @param[in] msgid   Message ID.
 * @param[in] param   Pointer to custs1_val_write_ind structure.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id  ID of the sending task instance.
 * @details Single-byte payload (0 = OFF, 1 = ON) to enable/disable Timer2 PWM output.
 *          Ignores invalid values > 1.
 * @sa timer2_pwm_enable, timer2_pwm_disable
 ****************************************************************************************
 */								 
void user_svc1_pwm_state_wr_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id);

/**
 ****************************************************************************************
 * @brief User callback when the system is powered on.
 * @return GOTO_SLEEP to allow the system to enter low-power mode after execution.
 * @note Called repeatedly by the SDK main loop.
 * @sa wdg_freeze, wdg_resume, app_easy_timer, uvp_uart_timer_cb
 ****************************************************************************************
 */
arch_main_loop_callback_ret_t user_app_on_system_powered(void);

/**
 ****************************************************************************************
 * @brief User initialization callback.
 * @note Initializes retained variables and starts default SDK initialization.
 * @details Should call default_app_on_init() as the last line.
 * @sa default_app_on_init
 ****************************************************************************************
 */
void user_app_on_init(void);

/// @} APP

#endif
