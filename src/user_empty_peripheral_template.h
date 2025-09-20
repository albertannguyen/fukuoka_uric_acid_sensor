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
 *          3. Shuts down or enables amplifiers via GPIO based on result of comparison.
 *          4. Reports ADC sample and comparison result to UART for debugging.
 *      Timer is automatically restarted after each call.
 * @note Needs work, in final firmware must send this information to client.
 * @sa gpadc_init, gpadc_collect_sample, gpadc_sample_to_mv, GPIO_SetActive, GPIO_SetInactive, app_easy_timer
 ****************************************************************************************
*/
void uvp_uart_timer_cb(void);

/**
 ****************************************************************************************
 * @brief Timer-driven callback for ADC conversions and BLE notifications.
 * @details
 *      This function periodically:
 *          1. Initializes ADC for a specific input (i.e., P0_6).
 *          2. Collects ADC sample and converts it to millivolts.
 *          3. Sends the value over BLE notifications using the custom service.
 *          4. Optionally prints values to UART for debugging on the devkit.
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

// FIXME: all PWM function comments
/**
 ****************************************************************************************
 * @brief Initializes Timer2 frequency for PWM generation.
 * @param[in] clk_div  Clock divider for Timer2 input.
 * @param[in] clk_src  Clock source for Timer2.
 * @param[in] pwm_div  Divider to set PWM frequency (2–16383).
 * @details Sets up Timer2 with PWM frequency and clock source. Must call
 *          timer2_pwm_set_duty_offsets() and then timer2_pwm_enable() afterward to start outputs.
 * @sa timer0_2_clk_div_set, timer2_config, timer2_pwm_freq_set
 ****************************************************************************************
*/
void timer2_pwm_set_frequency(tim0_2_clk_div_t clk_div, tim2_clk_src_t clk_src, uint16_t pwm_div);

void timer2_pwm_set_duty_offsets(uint8_t dc_pwm2, uint8_t offset_pwm2, uint8_t dc_pwm3, uint8_t offset_pwm3);

void timer2_pwm_enable(void);

/**
 ****************************************************************************************
 * @brief Disable all Timer2 PWM outputs.
 * @details Stops PWM signals and disables the timer input clock.
 * @sa timer2_stop, timer0_2_clk_disable
 ****************************************************************************************
*/
void timer2_pwm_disable(void);

/**
 ****************************************************************************************
 * @brief Connection function.
 * @param[in] conidx        Connection Id index
 * @param[in] param         Pointer to GAPC_CONNECTION_REQ_IND message
 * @note Starts ADC sampling and PWM outputs upon connection.
 * @sa default_app_on_connection, gpadc_wireless_timer_cb, timer2_pwm_init, timer2_pwm_enable
 ****************************************************************************************
*/
void user_on_connection(const uint8_t conidx, struct gapc_connection_req_ind const *param);

/**
 ****************************************************************************************
 * @brief Disconnection function.
 * @param[in] param         Pointer to GAPC_DISCONNECT_IND message
 * @note Stops PWM outputs to save power after disconnection.
 * @sa default_app_on_disconnect, timer2_pwm_disable
 ****************************************************************************************
*/
void user_on_disconnect(struct gapc_disconnect_ind const *param);

/**
 ****************************************************************************************
 * @brief Handles the messages that are not handled by the SDK internal mechanisms.
 * @param[in] msgid   Id of the message received.
 * @param[in] param   Pointer to the parameters of the message.
 * @param[in] dest_id ID of the receiving task instance.
 * @param[in] src_id  ID of the sending task instance.
 * @note Required to avoid GATT timeouts for unhandled messages.
 *			 This is a function given by Renesas.
 * @sa KE_MSG_ALLOC, KE_MSG_SEND
 ****************************************************************************************
*/
void user_catch_rest_hndl(ke_msg_id_t const msgid, void const *param, ke_task_id_t const dest_id, ke_task_id_t const src_id);

// FIXME: comments on all handler functions
// runs on both subscribe and unsubscribe
void user_svc1_sensor_voltage_cfg_ind_handler(ke_msg_id_t const msgid,
                                         struct custs1_val_write_ind const *param,
                                         ke_task_id_t const dest_id,
                                         ke_task_id_t const src_id);
																				 
void user_svc1_pwm_freq_wr_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id);
															 
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
