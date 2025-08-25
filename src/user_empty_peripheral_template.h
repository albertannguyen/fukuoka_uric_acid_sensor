/**
 ****************************************************************************************
 * @file user_empty_peripheral_template.h
 * @brief Empty peripheral template project header file.
 * @note Albert Nguyen: use "__BKPT(0);" for debugging with LTO compile
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

/*
 ****************************************************************************************
 * FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief app_on_init callback function rerouted to user space.
 * @note See user_callback_config.h for reroutes.
 ****************************************************************************************
*/
void user_app_on_init(void);

/**
 ****************************************************************************************
 * @brief app_on_system_powered callback function rerouted to user space.
 ****************************************************************************************
*/
arch_main_loop_callback_ret_t user_app_on_system_powered(void);

/**
 ****************************************************************************************
 * @brief Callback function for timer-driven UVP logic.
 ****************************************************************************************
*/
void uvp_timer_cb(void);

/**
 ****************************************************************************************
 * @brief Initializes the ADC with a given configuration.
 * @param[in] smpl_time_mult     Sample time multiplier (1-15).
 * @param[in] continuous         Enable or disable continuous conversion mode.
 * @param[in] interval_mult      Interval multiplier for continuous mode (0-255).
 * @param[in] input_attenuator   Attenuation factor for the ADC input.
 * @param[in] chopping           Enable or disable chopping.
 * @param[in] oversampling       Oversampling mode (0-7).
 * @note ADC input mode is fixed in this function (single-ended, P0_6).
 *			 ANY CHANGES TO ADC CONFIG MUST BE APPLIED WHEN ADC IS OFF.
 * @sa adc_init, adc_set_sample_time, adc_set_interval, adc_set_oversampling
 ****************************************************************************************
 */
void gpadc_init(uint8_t input, uint8_t smpl_time_mult, bool continuous, uint8_t interval_mult, adc_input_attn_t input_attenuator, bool chopping, uint8_t oversampling);

/**
 ****************************************************************************************
 * @brief Starts an ADC conversion, reads register value, and corrects the results.
 * @sa adc_get_sample
 * @sa adc_correct_sample
 * @return conversion result
 ****************************************************************************************
*/
uint16_t gpadc_collect_sample(void);

/**
 ****************************************************************************************
 * @brief Returns the raw conversion of ADC in millivolts.
 * @param[in] sample        Raw conversion result of the ADC.
 * @note This is a code snippet given by Renesas.
 * @return sample in millivolts 
 ****************************************************************************************
*/
uint16_t gpadc_sample_to_mv(uint16_t sample);

/**
 ****************************************************************************************
 * @brief Callback function for timer-driven measurements.
 ****************************************************************************************
*/
void gpadc_timer_cb(void);

/**
 ****************************************************************************************
 * @brief Callback function for interrupt-driven measurements.
 ****************************************************************************************
*/
void gpadc_interrupt(void);

/**
 ****************************************************************************************
 * @brief Initializes Timer 2 and the PWM output.
 * @param[in] clk_div       Clock divider for Timer 2.
 * @param[in] clk_src       Clock source for Timer 2.
 * @param[in] hw_pause      Enable or disable hardware pause for Timer 2.
 * @param[in] pwm_div       PWM frequency divider for PWM output (2 to (2^14 - 1)).
 ****************************************************************************************
*/
void timer2_pwm_init(tim0_2_clk_div_t clk_div, tim2_clk_src_t clk_src, tim2_hw_pause_t hw_pause, uint16_t pwm_div);

/**
 ****************************************************************************************
 * @brief Enables the PWM output.
 * @param[in] dc_pwm2       Duty cycle of PWM2 (0% - 100%).
 * @param[in] offset_pwm2   Offset of PWM2, delays the first rising edge (0% - 100%).
 * @param[in] dc_pwm3       Duty cycle of PWM3 (0% - 100%).
 * @param[in] offset_pwm3   Offset of PWM3, delays the first rising edge (0% - 100%).
 ****************************************************************************************
*/
void timer2_pwm_enable(uint8_t dc_pwm2, uint8_t offset_pwm2, uint8_t dc_pwm3, uint8_t offset_pwm3);

/**
 ****************************************************************************************
 * @brief Disables the PWM output.
 ****************************************************************************************
*/
void timer2_pwm_disable(void);

/**
 ****************************************************************************************
 * @brief Connection function.
 * @param[in] conidx        Connection Id index
 * @param[in] param         Pointer to GAPC_CONNECTION_REQ_IND message
 * @note Yellow warning only shows up after building the project (they can be ignored for now).
 *			 Default callback function given by template.
 ****************************************************************************************
*/
void user_on_connection(const uint8_t conidx, struct gapc_connection_req_ind const *param);

/**
 ****************************************************************************************
 * @brief Disconnection function.
 * @param[in] param         Pointer to GAPC_DISCONNECT_IND message
 * @note Default callback function given by template.
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
 * @note Default callback function given by template.
 ****************************************************************************************
*/
void user_catch_rest_hndl(ke_msg_id_t const msgid, void const *param, ke_task_id_t const dest_id, ke_task_id_t const src_id);

/// @} APP

#endif