/**
 ****************************************************************************************
 * @file user_empty_peripheral_template.c
 * @brief Empty peripheral template project source code.
 * @addtogroup APP
 * @{
 * @note Albert Nguyen
 ****************************************************************************************
 */

/*
 ****************************************************************************************
 * INCLUDE FILES
 ****************************************************************************************
 */
 
#include "rwip_config.h" // SW configuration
#include "gattc_task.h"
#include "app_api.h"
#include "user_empty_peripheral_template.h"

// For GPIO settings
#include "gpio.h"
#include "user_periph_setup.h"

// For UART serial port debugging
#include "arch_console.h"

// For ADC functions
#include "adc.h"
#include "adc_531.h"

// For timer 2 functions
#include "timer0_2.h"
#include "timer2.h"

// For DCDC converter debug
#include "syscntl.h"

// For BLE notifications
#include "custs1_task.h"
#include "user_custs1_def.h"

/*
 ****************************************************************************************
 * DEFINITIONS
 ****************************************************************************************
 */

// Clamp macro
#define CLAMP(value, min, max) ((value) < (min) ? (min) : ((value) > (max) ? (max) : (value)))

// Constants from datasheet
static const uint16_t MIN_PWM_DIV     = 2U;
static const uint16_t MAX_PWM_DIV     = 16383U;
static const uint32_t SYS_CLK_FREQ_HZ = 16000000U;
static const uint32_t LP_CLK_FREQ_HZ  = 32000U;

// UVP variables
timer_hnd uvp_timer __SECTION_ZERO("retention_mem_area0");
bool uvp_timer_initialized __SECTION_ZERO("retention_mem_area0");

// ADC variables
timer_hnd adc_timer __SECTION_ZERO("retention_mem_area0");
timer_hnd adc_timer_UART __SECTION_ZERO("retention_mem_area0");
uint16_t adc_sample_raw __SECTION_ZERO("retention_mem_area0");
uint16_t adc_sample_mv __SECTION_ZERO("retention_mem_area0");

/*
 ****************************************************************************************
 * UVP FUNCTIONS
 ****************************************************************************************
*/

void uvp_uart_timer_cb(void)
{
	// Declared for UART printout
	bool uvp_result;
	
	// Intialize and enable ADC
	// FIXME: need to initialize ADC pin in user_periph_setup.c and .h before flashing code to custom PCB
	gpadc_init(ADC_INPUT_SE_VBAT_HIGH, 2, false, 0, ADC_INPUT_ATTN_4X, false, 0);
	adc_enable();
	
	// Read ADC and convert results to millivolts
	adc_sample_raw = gpadc_collect_sample();
	adc_sample_mv = gpadc_sample_to_mv(adc_sample_raw);
	
	// Compare ADC value to see if it is less than 1900 mV
	if(adc_sample_mv < (uint16_t)1900){
		GPIO_SetInactive(UVP_MAX_SHDN_PORT, UVP_MAX_SHDN_PIN); // shutdown amplifiers
		uvp_result = true;
	}
	else
	{
		GPIO_SetActive(UVP_MAX_SHDN_PORT, UVP_MAX_SHDN_PIN); // enable amplifiers
		uvp_result = false;
	}
	
	// Print output to UART for debugging
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------\n\r");
	arch_printf("[UVP] Register Value: %d | Battery Voltage: %d mV \n\r", adc_sample_raw, adc_sample_mv);
	arch_printf("[UVP] System undervoltage status: %s \n\r", uvp_result ? "YES" : "NO");
	arch_printf("---------------------------------------------------------------------\n\r");
	#endif
	
	// Disable ADC
	adc_disable();
	
	// Restart this function every 0.5 seconds
	uvp_timer = app_easy_timer(50, uvp_uart_timer_cb);
}

/*
 ****************************************************************************************
 * ADC FUNCTIONS
 ****************************************************************************************
*/

void gpadc_wireless_timer_cb(void)
{
	// Intialize and enable ADC
	gpadc_init(ADC_INPUT_SE_P0_6, 2, false, 0, ADC_INPUT_ATTN_4X, false, 0);
	adc_enable();
	
	// Read ADC and convert results to millivolts
	adc_sample_raw = gpadc_collect_sample();
	adc_sample_mv = gpadc_sample_to_mv(adc_sample_raw);
	
	// Create dynamic kernel messsage for BLE notifications
	struct custs1_val_ntf_ind_req *req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_NTF_REQ,
																												prf_get_task_from_id(TASK_ID_CUSTS1),
																												TASK_APP,
																												custs1_val_ntf_ind_req,
																												DEF_SVC1_SENSOR_VOLTAGE_CHAR_LEN);
	
	// Set the parameters of the struct defined above
	req->handle = SVC1_IDX_SENSOR_VOLTAGE_VAL;
	req->length = DEF_SVC1_SENSOR_VOLTAGE_CHAR_LEN;
	req->notification = true;
	
	// Copies ADC value to notification payload
	memcpy(req->value, &adc_sample_mv, DEF_SVC1_SENSOR_VOLTAGE_CHAR_LEN);
	
	// Print output to UART for debugging
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------\n\r");
	arch_printf("[GPADC] Register Value: %d | Sensor Voltage: %d mV \n\r", adc_sample_raw, adc_sample_mv);
	// Note that BLE print is in little-endian order and must be converted to big-endian order before turning into a decimal.
	arch_printf("[BLE] LSB: 0x%02X, MSB: 0x%02X\n\r",
							adc_sample_mv & 0xFF,
							(adc_sample_mv >> 8) & 0xFF);
	arch_printf("---------------------------------------------------------------------\n\r");
	#endif
	
	// Sends notification to BLE stack
	KE_MSG_SEND(req);
	
	// Disable ADC
	adc_disable();
	
	// If phone is still connected, restart timer and callback function (SDK line)
	if (ke_state_get(TASK_APP) == APP_CONNECTED)
	{
			adc_timer = app_easy_timer(100, gpadc_wireless_timer_cb); // Restart this function every 1 second
	}
}

// TODO: read datasheet and calculate manual mode settings that gives highest sampling rate and accuracy
void gpadc_init(uint8_t input, uint8_t smpl_time_mult, bool continuous, uint8_t interval_mult, adc_input_attn_t input_attenuator, bool chopping, uint8_t oversampling)
{
	// ADC config structure
	adc_config_t adc_config_struct =
	{
		// HW fixed for current setup
		.input_mode = ADC_INPUT_MODE_SINGLE_ENDED,

		// SW adjustable
		.input = input,
		.smpl_time_mult = smpl_time_mult,
		.continuous = continuous,
		.interval_mult = interval_mult,
		.input_attenuator = input_attenuator,
		.chopping = chopping,
		.oversampling = oversampling
	};
	
	// Initialize ADC with structure
	adc_init(&adc_config_struct);
	
	// Disable input shifter
	adc_input_shift_disable();
	
	// FIXME: determine if this line needs to be included
	// Disable die temperature sensor
	// adc_temp_sensor_disable();

	// Perform offset calibration of the ADC
	adc_reset_offsets();
	adc_offset_calibrate(ADC_INPUT_MODE_SINGLE_ENDED);
	
	// Consider using adc_ldo_const_current_enable() if getting noisy readings at lower voltage
}

uint16_t gpadc_collect_sample(void)
{
	// Raw sample is processed using ADC parameters and corrected conversion is returned
	uint16_t sample = adc_correct_sample(adc_get_sample());
	
	return (sample);
}

uint16_t gpadc_sample_to_mv(uint16_t sample)
{
	// Effective resolution of ADC sample based on oversampling rate	
	uint32_t adc_resolution = 10 + ((6 < adc_get_oversampling()) ? 6 : adc_get_oversampling());

	// Reference voltage is 900mv but is scaled based on input attenation
	uint32_t ref_mv = 900 * (GetBits16(GP_ADC_CTRL2_REG, GP_ADC_ATTN) + 1);

	// Returns mV value read by the ADC
	return (uint16_t)((((uint32_t)sample) * ref_mv) >> adc_resolution);
}

/*
 ****************************************************************************************
 * PWM FUNCTIONS
 ****************************************************************************************
*/

void timer2_pwm_init(tim0_2_clk_div_t clk_div, tim2_clk_src_t clk_src, tim2_hw_pause_t hw_pause, uint16_t pwm_div)
{
	// Define timer parameters in struct
	tim0_2_clk_div_config_t clk_cfg = {
		.clk_div = clk_div
	};
	
	tim2_config_t tmr_cfg =
	{
		.clk_source = clk_src,
    .hw_pause = hw_pause
	};
	
	// Set timer parameters using structs above
	timer0_2_clk_div_set(&clk_cfg);
	timer2_config(&tmr_cfg);
	
	// Define PWM parameters
	uint8_t clk_div_int = 1 << clk_div;
	uint32_t clk_freq = (clk_src == TIM2_CLK_SYS) ? SYS_CLK_FREQ_HZ : LP_CLK_FREQ_HZ,
					 input_freq = clk_freq / clk_div_int;

	// Clamp pwm_div if beyond datasheet range of 2 to (2^14 - 1)
	pwm_div = CLAMP(pwm_div, MIN_PWM_DIV, MAX_PWM_DIV);

	// Set PWM frequency based on datasheet for Timer 2
	timer2_pwm_freq_set(input_freq / pwm_div, input_freq);
}

void timer2_pwm_enable(uint8_t dc_pwm2, uint8_t offset_pwm2, uint8_t dc_pwm3, uint8_t offset_pwm3)
{
	// Clamp input values if outside of valid 0-100% range
	dc_pwm2 = CLAMP(dc_pwm2, 0, 100);
	offset_pwm2 = CLAMP(offset_pwm2, 0, 100);
	dc_pwm3 = CLAMP(dc_pwm3, 0, 100);
	offset_pwm3 = CLAMP(offset_pwm3, 0, 100);
	
	// Define PWM parameters in struct
	tim2_pwm_config_t pwm2_cfg = {
		.pwm_signal = TIM2_PWM_2,
		.pwm_dc = dc_pwm2,
		.pwm_offset = offset_pwm2
	};
	
	tim2_pwm_config_t pwm3_cfg = {
		.pwm_signal = TIM2_PWM_3,
		.pwm_dc = dc_pwm3,
		.pwm_offset = offset_pwm3
	};
	
	// Set PWM parameters
	timer2_pwm_signal_config(&pwm2_cfg);
	timer2_pwm_signal_config(&pwm3_cfg);
	
	// Enable timer input clock
	timer0_2_clk_enable();
	
	// Enable PWM signal
	timer2_start();
}

void timer2_pwm_disable(void)
{
	// Disable PWM signal
	timer2_stop();

	// Disable timer input clock
	timer0_2_clk_disable();
}

/*
 ****************************************************************************************
 * USER CALLBACK FUNCTIONS
 ****************************************************************************************
*/

void user_on_connection(uint8_t connection_idx, struct gapc_connection_req_ind const *param)
{
	default_app_on_connection(connection_idx, param);
	
	// FIXME: temporary mode of operation
	gpadc_wireless_timer_cb();
	
	// Run PWM
	// Max voltage is 3.3 V on LP clock source, min is 0 V
	// This is because GPIO is supplied by VBAT_HIGH or the 3.3 V LDO on devkit
	timer2_pwm_init(TIM0_2_CLK_DIV_8, TIM2_CLK_LP, TIM2_HW_PAUSE_OFF, 0xFFFF);
	timer2_pwm_enable(50, 0, 25, 0);
}

void user_on_disconnect(struct gapc_disconnect_ind const *param )
{
	default_app_on_disconnect(param);
	
	// Stop PWM
	timer2_pwm_disable();
}

void user_catch_rest_hndl(ke_msg_id_t const msgid, void const *param, ke_task_id_t const dest_id, ke_task_id_t const src_id)
{
    switch(msgid)
    {
        case GATTC_EVENT_REQ_IND:
        {
            // Confirm unhandled indication to avoid GATT timeout
            struct gattc_event_ind const *ind = (struct gattc_event_ind const *) param;
            struct gattc_event_cfm *cfm = KE_MSG_ALLOC(GATTC_EVENT_CFM, src_id, dest_id, gattc_event_cfm);
            cfm->handle = ind->handle;
            KE_MSG_SEND(cfm);
        } break;

        default:
            break;
    }
}

arch_main_loop_callback_ret_t user_app_on_system_powered(void)
{
	wdg_freeze(); // freeze watchdog timer
	
	// Initiates UVP timer only once
	if(!uvp_timer_initialized){
		uvp_timer = app_easy_timer(50, uvp_uart_timer_cb);
		uvp_timer_initialized = true;
	}
	
	wdg_resume(); // resume watchdog timer
	
	return GOTO_SLEEP; // returning KEEP_POWERED hardfaults to nmi_handler.c, likely due to how SDK handles sleep
}

void user_app_on_init(void)
{
	// Initialize user retained variables
	uvp_timer_initialized = false;
	adc_sample_raw = 0;
	adc_sample_mv = 0;
	
	// TODO: make changes to DCDC converter and observe how it changes output of GPIOs
	// syscntl_dcdc_level_t vdd = syscntl_dcdc_get_level();
	
	// Start the default initialization process for BLE user application
	// SDK doc states that this should be the last line called in the callback function
	default_app_on_init();
}

/// @} APP
