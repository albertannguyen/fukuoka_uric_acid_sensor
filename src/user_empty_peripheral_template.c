/**
 ****************************************************************************************
 * @file user_empty_peripheral_template.c
 * @brief Empty peripheral template project source code with user code.
 * @addtogroup APP
 * @{
 * @author Albert Nguyen
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
#include "user_custs1_def.h"

// For PWM and sleep management
#include "arch_api.h"

/*
 ****************************************************************************************
 * DEFINITIONS
 ****************************************************************************************
 */

// Macro that clamps value to the range [min, max]
#define CLAMP(value, min, max) ((value) < (min) ? (min) : ((value) > (max) ? (max) : (value)))

// Define ADC pin based on preprocessor toggle in user_periph_setup.h for gpadc_init_se()
#if defined(BOARD_CUSTOM_PCB)
    #define ADC_ENUM_INPUT ADC_INPUT_SE_P0_6   // custom PCB ADC pin
#else
    #define ADC_ENUM_INPUT ADC_INPUT_SE_P0_6   // default USB devkit ADC pin
#endif

// Constants from datasheet
static const uint16_t MIN_PWM_DIV     = 2U;
static const uint16_t MAX_PWM_DIV     = 16383U;
static const uint32_t SYS_CLK_FREQ_HZ = 16000000U;
static const uint32_t LP_CLK_FREQ_HZ  = 32000U;

/*
----------------------------------
- Retained / Global variables
----------------------------------
*/

// These variables are retained across sleep cycles

// UVP (Undervoltage Protection) variables
timer_hnd uvp_timer __SECTION_ZERO("retention_mem_area0");
bool uvp_timer_initialized __SECTION_ZERO("retention_mem_area0");
uint16_t uvp_cccd_value __SECTION_ZERO("retention_mem_area0");
uint16_t uvp_adc_sample_raw __SECTION_ZERO("retention_mem_area0");
uint16_t uvp_adc_sample_mv __SECTION_ZERO("retention_mem_area0");
bool uvp_shutdown __SECTION_ZERO("retention_mem_area0");

// Sensor voltage variables
timer_hnd sensor_timer __SECTION_ZERO("retention_mem_area0");
uint16_t sensor_adc_sample_raw __SECTION_ZERO("retention_mem_area0");
uint16_t sensor_adc_sample_mv __SECTION_ZERO("retention_mem_area0");

/*
 ****************************************************************************************
 * UVP FUNCTIONS
 ****************************************************************************************
*/

void uvp_wireless_timer_cb(void)
{
	// Initialize and enable ADC for a single conversion of VBAT HIGH rail
	gpadc_init_se(ADC_INPUT_SE_VBAT_HIGH, 2, false, 0, ADC_INPUT_ATTN_4X, false, 0);
	adc_enable();
	
	// Read ADC and convert results to millivolts
	uvp_adc_sample_raw = gpadc_collect_sample();
	uvp_adc_sample_mv = gpadc_sample_to_mv(uvp_adc_sample_raw);
	
	// Compare ADC value to see if it is less than chosen UVP threshold (1900 mV)
	if(uvp_adc_sample_mv < (uint16_t)1900)
	{
		// GPIO enable signal is controlled by this flag in user_periph_setup.c
		uvp_shutdown = true;
		
		// FIXME: check if this redundant call breaks FW
		adc_disable();
		timer2_pwm_disable();
	}
	else
	{
		uvp_shutdown = false;
	}
	
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------\n\r");
	arch_printf("[UVP] Register Value: %u | Battery Voltage: %u mV \n\r", uvp_adc_sample_raw, uvp_adc_sample_mv);
	arch_printf("[UVP] System undervoltage shutdown status: %s \n\r", uvp_shutdown ? "true" : "false");
	#endif
	
	if (uvp_cccd_value == 0x0001 && (ke_state_get(TASK_APP) == APP_CONNECTED)) // notifications enabled and phone connected
	{
		#ifdef CFG_PRINTF
		arch_printf("[UVP] LSB: 0x%02X, MSB: 0x%02X\n\r", uvp_adc_sample_mv & 0xFF, (uvp_adc_sample_mv >> 8) & 0xFF);
		#endif
		
		// Create dynamic kernel message for notifications
		struct custs1_val_ntf_ind_req *req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_NTF_REQ,
																													prf_get_task_from_id(TASK_ID_CUSTS1),
																													TASK_APP,
																													custs1_val_ntf_ind_req,
																													DEF_SVC1_BATTERY_VOLTAGE_CHAR_LEN);
		
		// Populate the notification structure
		req->handle = SVC1_IDX_BATTERY_VOLTAGE_VAL;
		req->length = DEF_SVC1_BATTERY_VOLTAGE_CHAR_LEN;
		req->notification = true;
		
		// Copies UVP ADC value to notification payload
		memcpy(req->value, &uvp_adc_sample_mv, DEF_SVC1_BATTERY_VOLTAGE_CHAR_LEN);
		
		// Send structure to the kernel to be transmitted by the BLE stack
		KE_MSG_SEND(req);
	}
	
	// Disable ADC to conserve power
	adc_disable();
	
	/*
   ****************************************************************************************
   * UART test prints
   ****************************************************************************************
	 */
	
	// ChatGPT fetch code for DCDC converter, prints VBAT_LOW voltage
	syscntl_dcdc_level_t vbat_low = syscntl_dcdc_get_level();

	#ifdef CFG_PRINTF
	const char *voltage_str = NULL;

	switch (vbat_low) {
			case SYSCNTL_DCDC_LEVEL_1V025: voltage_str = "1.025 V"; break;
			case SYSCNTL_DCDC_LEVEL_1V05:  voltage_str = "1.050 V"; break;
			case SYSCNTL_DCDC_LEVEL_1V075: voltage_str = "1.075 V"; break;
			case SYSCNTL_DCDC_LEVEL_1V1:   voltage_str = "1.100 V"; break;
			case SYSCNTL_DCDC_LEVEL_1V125: voltage_str = "1.125 V"; break;
			case SYSCNTL_DCDC_LEVEL_1V150: voltage_str = "1.150 V"; break;
			case SYSCNTL_DCDC_LEVEL_1V175: voltage_str = "1.175 V"; break;
			case SYSCNTL_DCDC_LEVEL_1V2:   voltage_str = "1.200 V"; break;

			case SYSCNTL_DCDC_LEVEL_1V725: voltage_str = "1.725 V"; break;
			case SYSCNTL_DCDC_LEVEL_1V75:  voltage_str = "1.750 V"; break;
			case SYSCNTL_DCDC_LEVEL_1V775: voltage_str = "1.775 V"; break;
			case SYSCNTL_DCDC_LEVEL_1V8:   voltage_str = "1.800 V"; break;
			case SYSCNTL_DCDC_LEVEL_1V825: voltage_str = "1.825 V"; break;
			case SYSCNTL_DCDC_LEVEL_1V850: voltage_str = "1.850 V"; break;
			case SYSCNTL_DCDC_LEVEL_1V875: voltage_str = "1.875 V"; break;
			case SYSCNTL_DCDC_LEVEL_1V9:   voltage_str = "1.900 V"; break;

			case SYSCNTL_DCDC_LEVEL_2V425: voltage_str = "2.425 V"; break;
			case SYSCNTL_DCDC_LEVEL_2V45:  voltage_str = "2.450 V"; break;
			case SYSCNTL_DCDC_LEVEL_2V475: voltage_str = "2.475 V"; break;
			case SYSCNTL_DCDC_LEVEL_2V5:   voltage_str = "2.500 V"; break;
			case SYSCNTL_DCDC_LEVEL_2V525: voltage_str = "2.525 V"; break;
			case SYSCNTL_DCDC_LEVEL_2V550: voltage_str = "2.550 V"; break;
			case SYSCNTL_DCDC_LEVEL_2V575: voltage_str = "2.575 V"; break;
			case SYSCNTL_DCDC_LEVEL_2V6:   voltage_str = "2.600 V"; break;

			case SYSCNTL_DCDC_LEVEL_2V925: voltage_str = "2.925 V"; break;
			case SYSCNTL_DCDC_LEVEL_2V95:  voltage_str = "2.950 V"; break;
			case SYSCNTL_DCDC_LEVEL_2V975: voltage_str = "2.975 V"; break;
			case SYSCNTL_DCDC_LEVEL_3V0:   voltage_str = "3.000 V"; break;
			case SYSCNTL_DCDC_LEVEL_3V025: voltage_str = "3.025 V"; break;
			case SYSCNTL_DCDC_LEVEL_3V050: voltage_str = "3.050 V"; break;
			case SYSCNTL_DCDC_LEVEL_3V075: voltage_str = "3.075 V"; break;
			case SYSCNTL_DCDC_LEVEL_3V1:   voltage_str = "3.100 V"; break;

			default: voltage_str = "Unknown"; break;
	}

	arch_printf("[DCDC] VBAT_LOW voltage level: %s\n\r", voltage_str);
	#endif
	
	// Prints current sleep mode of system
	#ifdef CFG_PRINTF
	sleep_state_t current_mode = arch_get_sleep_mode();
	const char *mode_str;

	switch (current_mode) {
			case ARCH_SLEEP_OFF:
					mode_str = "ARCH_SLEEP_OFF (Active Mode)";
					break;
			case ARCH_EXT_SLEEP_ON:
					mode_str = "ARCH_EXT_SLEEP_ON (Extended Sleep)";
					break;
			case ARCH_EXT_SLEEP_OTP_COPY_ON:
					mode_str = "ARCH_EXT_SLEEP_OTP_COPY_ON";
					break;
			default:
					mode_str = "UNKNOWN MODE";
					break;
	}

	// Print the status using the retrieved string and raw numeric value
	arch_printf("[SLEEP] Current Mode: %s (Value: %u) \n\r", mode_str, (uint8_t)current_mode);
	#endif
	
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------\n\r");
	#endif
	
	// Restart this function every 0.5 second
	uvp_timer = app_easy_timer(50, uvp_wireless_timer_cb);
}

/*
 ****************************************************************************************
 * ADC FUNCTIONS
 ****************************************************************************************
*/

void gpadc_wireless_timer_cb(void)
{
	// Initialize and enable ADC for a single conversion of sensor voltage
	gpadc_init_se(ADC_ENUM_INPUT, 2, false, 0, ADC_INPUT_ATTN_4X, false, 0);
	adc_enable();
	
	// Read ADC and convert results to millivolts
	sensor_adc_sample_raw = gpadc_collect_sample();
	sensor_adc_sample_mv = gpadc_sample_to_mv(sensor_adc_sample_raw);
	
	// Create dynamic kernel message for notifications
	struct custs1_val_ntf_ind_req *req = KE_MSG_ALLOC_DYN(CUSTS1_VAL_NTF_REQ,
																												prf_get_task_from_id(TASK_ID_CUSTS1),
																												TASK_APP,
																												custs1_val_ntf_ind_req,
																												DEF_SVC1_SENSOR_VOLTAGE_CHAR_LEN);
	
	// Populate the notification structure
	req->handle = SVC1_IDX_SENSOR_VOLTAGE_VAL;
	req->length = DEF_SVC1_SENSOR_VOLTAGE_CHAR_LEN;
	req->notification = true;
	
	// Copies sensor voltage ADC value to notification payload
	memcpy(req->value, &sensor_adc_sample_mv, DEF_SVC1_SENSOR_VOLTAGE_CHAR_LEN);
	
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------\n\r");
	arch_printf("[ADC] Register Value: %u | Sensor Voltage: %u mV \n\r", sensor_adc_sample_raw, sensor_adc_sample_mv);
	arch_printf("[ADC] LSB: 0x%02X, MSB: 0x%02X\n\r", sensor_adc_sample_mv & 0xFF, (sensor_adc_sample_mv >> 8) & 0xFF);
	arch_printf("---------------------------------------------------------------------\n\r");
	#endif
	
	// Send structure to the kernel to be transmitted by the BLE stack
	KE_MSG_SEND(req);
	
	// Disable ADC to conserve power
	adc_disable();
	
	// If phone is still connected, restart this function every 1 second
	if (ke_state_get(TASK_APP) == APP_CONNECTED)
	{
			sensor_timer = app_easy_timer(100, gpadc_wireless_timer_cb);
	}
}

// TODO: read datasheet and calculate manual mode settings that gives highest sampling rate and accuracy
void gpadc_init_se(adc_input_se_t input, uint8_t smpl_time_mult, bool continuous, uint8_t interval_mult, adc_input_attn_t input_attenuator, bool chopping, uint8_t oversampling)
{
	// Build ADC config structure for single-ended measurement
	adc_config_t adc_config_struct =
	{
		// ADC fixed to single-ended due to PCB layout
		.input_mode = ADC_INPUT_MODE_SINGLE_ENDED,

		// Adjustable values from ADC parameters
		.input = input,
		.smpl_time_mult = smpl_time_mult,
		.continuous = continuous,
		.interval_mult = interval_mult,
		.input_attenuator = input_attenuator,
		.chopping = chopping,
		.oversampling = oversampling
	};
	
	// Initialize ADC with the configuration structure
	adc_init(&adc_config_struct);
	
	// Disable input shifter (not needed)
	adc_input_shift_disable();
	
	// Disable die temperature sensor (may lower noise)
	adc_temp_sensor_disable();
	
	// Set ADC startup delay after LDO powers on
	adc_delay_set(64); // for 16 Mhz system clock, 16 us delay is recommended by datasheet
	
	// Enable load current for ADC's LDO to improve stability during conversion
	adc_ldo_const_current_enable();

	// WIP: Perform offset calibration to remove DC offsets before sampling, needs more bench testing
	adc_reset_offsets();
	adc_offset_calibrate(ADC_INPUT_MODE_SINGLE_ENDED);
}

uint16_t gpadc_collect_sample(void)
{
	// Fetch raw ADC sample then apply SDK correction routine (config-dependent)
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

void timer2_pwm_set_frequency(tim0_2_clk_div_t clk_div, tim2_clk_src_t clk_src, uint16_t pwm_div)
{
	// Create config structures for clock division and timer2 config
	tim0_2_clk_div_config_t clk_cfg = {
		.clk_div = clk_div
	};
	
	tim2_config_t tmr_cfg =
	{
		.clk_source = clk_src,
    .hw_pause = TIM2_HW_PAUSE_OFF // WIP: Keep hardware pause off for now
	};
	
	// Apply clock division and timer config
	timer0_2_clk_div_set(&clk_cfg);
	timer2_config(&tmr_cfg);
	
	// Compute input clock frequency from selected clock source and divider
	uint8_t clk_div_int = 1 << clk_div;
	uint32_t clk_freq = (clk_src == TIM2_CLK_SYS) ? SYS_CLK_FREQ_HZ : LP_CLK_FREQ_HZ,
					 input_freq = clk_freq / clk_div_int;

	// Clamp pwm_div to datasheet allowed range
	pwm_div = CLAMP(pwm_div, MIN_PWM_DIV, MAX_PWM_DIV);
	
	#ifdef CFG_PRINTF
	arch_printf("[PWM FREQ] pwm_div (clamped value) = %u (0x%04X)\n\r", pwm_div, pwm_div);
	#endif

	// Set PWM frequency based on formula from datasheet for Timer 2
	timer2_pwm_freq_set(input_freq / pwm_div, input_freq); // pwm_div promoted to 32 bits
}

void timer2_pwm_set_dc_and_offset(uint8_t dc_pwm2, uint8_t offset_pwm2, uint8_t dc_pwm3, uint8_t offset_pwm3)
{
	// Clamp input values if outside of valid 0-100% range
	dc_pwm2 = CLAMP(dc_pwm2, 0, 100);
	offset_pwm2 = CLAMP(offset_pwm2, 0, 100);
	dc_pwm3 = CLAMP(dc_pwm3, 0, 100);
	offset_pwm3 = CLAMP(offset_pwm3, 0, 100);
	
	// Create config structures for PWM signals
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
	
	// Apply configuration to PWM signals
	timer2_pwm_signal_config(&pwm2_cfg);
	timer2_pwm_signal_config(&pwm3_cfg);
}

void timer2_pwm_enable(void)
{
	// Enable timer input clock
	timer0_2_clk_enable();
	
	// Enable PWM outputs
	timer2_start();
	
	// Prevent SoC from sleeping
	arch_set_sleep_mode(ARCH_SLEEP_OFF);
}

void timer2_pwm_disable(void)
{
	// Disable PWM outputs
	timer2_stop();

	// Disable timer input clock
	timer0_2_clk_disable();
	
	// Return sleep mode to default
	arch_set_sleep_mode(ARCH_EXT_SLEEP_ON);
}

/*
 ****************************************************************************************
 * USER CALLBACK FUNCTIONS
 ****************************************************************************************
*/

void user_on_connection(uint8_t connection_idx, struct gapc_connection_req_ind const *param)
{
	#ifdef CFG_PRINTF
	arch_printf("[BLE] Phone connected to DA14531. \n\r");
	#endif
	
	default_app_on_connection(connection_idx, param);
}

void user_on_disconnect(struct gapc_disconnect_ind const *param )
{
	#ifdef CFG_PRINTF
	arch_printf("[BLE] Phone disconnected from DA14531. \n\r");
	#endif
	
	default_app_on_disconnect(param);
}

void user_catch_rest_hndl(ke_msg_id_t const msgid,
													void const *param,
													ke_task_id_t const dest_id,
													ke_task_id_t const src_id)
{
	switch(msgid)
	{
		// Checks for case when phone writes data to a custom characteristic value
		case CUSTS1_VAL_WRITE_IND:
		{
			// Param is generic constant so must change type to expected struct by casting it (SDK line)
			struct custs1_val_write_ind const *msg_param = (struct custs1_val_write_ind const *)(param);
			
			// Determine which characteristic was written to by checking its handle and jumping to the respective handler
			switch (msg_param->handle)
			{
				case SVC1_IDX_SENSOR_VOLTAGE_NTF_CFG:
					user_svc1_sensor_voltage_cfg_ind_handler(msgid, msg_param, dest_id, src_id);
					break;

				case SVC1_IDX_PWM_FREQ_VAL:
					user_svc1_pwm_freq_wr_ind_handler(msgid, msg_param, dest_id, src_id);
					break;
				
				case SVC1_IDX_PWM_DC_AND_OFFSET_VAL:
					user_svc1_pwm_dc_and_offset_wr_ind_handler(msgid, msg_param, dest_id, src_id);
					break;
				
				case SVC1_IDX_PWM_STATE_VAL:
					user_svc1_pwm_state_wr_ind_handler(msgid, msg_param, dest_id, src_id);
					break;
				
				case SVC1_IDX_BATTERY_VOLTAGE_NTF_CFG:
					user_svc1_battery_voltage_cfg_ind_handler(msgid, msg_param, dest_id, src_id);
					break;
				
				default:
					break;
			}
		} break;
	
		// Checks for case when client reads a custom characteristic value
		case CUSTS1_VALUE_REQ_IND:
		{
			struct custs1_value_req_ind const *msg_param = (struct custs1_value_req_ind const *) param;
			
			switch (msg_param->att_idx)
			{
				case SVC1_IDX_SENSOR_VOLTAGE_VAL:
					user_svc1_read_sensor_voltage_handler(msgid, msg_param, dest_id, src_id);
					break;
				
				case SVC1_IDX_BATTERY_VOLTAGE_VAL:
					user_svc1_read_battery_voltage_handler(msgid, msg_param, dest_id, src_id);
					break;

				default: // default read case is an SDK code snippet
				{
						// Send Error message
						struct custs1_value_req_rsp *rsp = KE_MSG_ALLOC(CUSTS1_VALUE_REQ_RSP,
																														src_id,
																														dest_id,
																														custs1_value_req_rsp);

						// Provide the connection index.
						rsp->conidx  = app_env[msg_param->conidx].conidx;
						// Provide the attribute index.
						rsp->att_idx = msg_param->att_idx;
						// Force current length to zero.
						rsp->length = 0;
						// Set Error status
						rsp->status  = ATT_ERR_APP_ERROR;
						// Send message
						KE_MSG_SEND(rsp);
				} break;
			}
		} break;
		
		// Code snippet given and required by SDK
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

void user_svc1_sensor_voltage_cfg_ind_handler(ke_msg_id_t const msgid,
                                         struct custs1_val_write_ind const *param,
                                         ke_task_id_t const dest_id,
                                         ke_task_id_t const src_id)
{
	// Check UVP status
	if(uvp_shutdown)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Prevented characteristic change and forced exit of handler function \n\r");
		#endif
		
		return;
	}
	
	// Copy CCCD value written by the phone into a local variable
	uint16_t cccd_value = 0; // set to zero for safe memcpy
	memcpy(&cccd_value, param->value, param->length);
	
	#ifdef CFG_PRINTF
	arch_printf("[SENSOR VOLTAGE] cccd_value = %u \n\r", cccd_value);
	#endif

	if (cccd_value == 0x0001 && (ke_state_get(TASK_APP) == APP_CONNECTED)) // notifications enabled and phone connected
	{
		#ifdef CFG_PRINTF
    arch_printf("[SENSOR VOLTAGE] Starting the ADC. \n\r");
    #endif
		
		// Start ADC conversions on 1 second timer
		sensor_timer = app_easy_timer(100, gpadc_wireless_timer_cb);
	}
	else if (cccd_value == 0x0000) // notifications disabled
	{
		#ifdef CFG_PRINTF
    arch_printf("[SENSOR VOLTAGE] Stopping the ADC. \n\r");
    #endif
		
		// Stop ADC conversions
		if (sensor_timer != EASY_TIMER_INVALID_TIMER) // prevents cancel when timer does not exist
		{
			// Cancels existing timer
			app_easy_timer_cancel(sensor_timer);
			sensor_timer = EASY_TIMER_INVALID_TIMER;
		}
	}
}

void user_svc1_pwm_freq_wr_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id)
{
	// Check UVP status
	if(uvp_shutdown)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Prevented characteristic change and forced exit of handler function \n\r");
		#endif
		
		return;
	}
	
	// Validate length of characteristic value written by the phone
	if (param->length != DEF_SVC1_PWM_FREQ_CHAR_LEN)
	{
    #ifdef CFG_PRINTF
    arch_printf("[PWM FREQ] Invalid packet byte length: %u (expected %u) \n\r", param->length, DEF_SVC1_PWM_FREQ_CHAR_LEN);
    #endif
		
    return; // ignore incomplete write
	}
	
	// Parse byte array into expected values
	// Byte order is [clk_div, clk_src, pwm_div_MSB, pwm_div_LSB]
	uint8_t clk_div = param->value[0];
	uint8_t clk_src = param->value[1];
	uint16_t pwm_div = ((param->value[2] << 8) | param->value[3]);
	
	#ifdef CFG_PRINTF
	arch_printf("[PWM FREQ] Bytes received. \n\r");
	arch_printf("[PWM FREQ] clk_div value = %u (0x%02X) \n\r", clk_div, clk_div);
	arch_printf("[PWM FREQ] clk_src value = %u (0x%02X) \n\r", clk_src, clk_src);
	arch_printf("[PWM FREQ] pwm_div value = %u (0x%04X) \n\r", pwm_div, pwm_div);
	#endif

	// Validate inputs with enum int literals from timer headers
	if (clk_div > TIM0_2_CLK_DIV_8 || clk_src > TIM2_CLK_SYS) // literals are auto-promoted to uint8 for comparison
	{
		#ifdef CFG_PRINTF
    arch_printf("[PWM FREQ] Invalid clk_div or clk_src write (first 2 bytes), input is ignored. \n\r");
    #endif
		
		return; // ignore invalid write
	}

	// Apply values to function
	timer2_pwm_set_frequency((tim0_2_clk_div_t)clk_div, (tim2_clk_src_t)clk_src, pwm_div); // note that pwm_div is clamped in this function already
	
	#ifdef CFG_PRINTF
	arch_printf("[PWM FREQ] SUCCESS on setting config. \n\r");
	#endif
}

void user_svc1_pwm_dc_and_offset_wr_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id)
{
	// Check UVP status
	if(uvp_shutdown)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Prevented characteristic change and forced exit of handler function \n\r");
		#endif
		
		return;
	}
	
	// Validate length of characteristic value written by the phone
	if (param->length != DEF_SVC1_PWM_DC_AND_OFFSET_CHAR_LEN)
	{
		#ifdef CFG_PRINTF
		arch_printf("[PWM DC AND OFFSET] Invalid packet byte length: %u (expected %u) \n\r", param->length, DEF_SVC1_PWM_DC_AND_OFFSET_CHAR_LEN);
		#endif
		
    return; // ignore incomplete write
	}
	
	// Parse byte array into expected values
	// Byte order is [dc_pwm2, offset_pwm2, dc_pwm3, offset_pwm3]
	uint8_t dc_pwm2 = param->value[0];
	uint8_t offset_pwm2 = param->value[1];
	uint8_t dc_pwm3 = param->value[2];
	uint8_t offset_pwm3 = param->value[3];
	
	#ifdef CFG_PRINTF
	arch_printf("[PWM DC AND OFFSET] Bytes received. \n\r");
	arch_printf("[PWM DC AND OFFSET] dc_pwm2 value = %u (0x%02X) \n\r", dc_pwm2, dc_pwm2);
	arch_printf("[PWM DC AND OFFSET] offset_pwm2 value = %u (0x%02X) \n\r", offset_pwm2, offset_pwm2);
	arch_printf("[PWM DC AND OFFSET] dc_pwm3 value = %u (0x%02X) \n\r", dc_pwm3, dc_pwm3);
	arch_printf("[PWM DC AND OFFSET] offset_pwm3 value = %u (0x%02X) \n\r", offset_pwm3, offset_pwm3);
	#endif
	
	// Apply values to function (which already has clamp protection for all inputs)
	timer2_pwm_set_dc_and_offset(dc_pwm2, offset_pwm2, dc_pwm3, offset_pwm3);
	
	#ifdef CFG_PRINTF
	arch_printf("[PWM DC AND OFFSET] SUCCESS on setting config. \n\r");
	#endif
}

void user_svc1_pwm_state_wr_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id)
{	
	// Check UVP status
	if(uvp_shutdown)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Prevented characteristic change and forced exit of handler function \n\r");
		#endif
		
		return;
	}
	
	// Copy single byte value to variable
	uint8_t state = 0;
	memcpy(&state, param->value, param->length);
	
	#ifdef CFG_PRINTF
	arch_printf("[PWM STATE] Byte received. \n\r");
	arch_printf("[PWM STATE] state value = %u (0x%02X) \n\r", state, state);
	#endif
	
	// Turns on or off PWM output based on value
	if(state > 1)
	{
		#ifdef CFG_PRINTF
		arch_printf("[PWM STATE] Invalid input. \n\r");
		#endif
		
		return; // ignore invalid write
	} 
	else if(state == 1)
	{
		timer2_pwm_enable(); // turn on output
		
		#ifdef CFG_PRINTF
		arch_printf("[PWM STATE] Output ON. \n\r");
		#endif
	}
	else if(state == 0)
	{
		timer2_pwm_disable(); // turn off output
		
		#ifdef CFG_PRINTF
		arch_printf("[PWM STATE] Output OFF. \n\r");
		#endif
	}
}

void user_svc1_battery_voltage_cfg_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id)
{
	// Check UVP status
	if(uvp_shutdown)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Prevented characteristic change and forced exit of handler function \n\r");
		#endif
		
		return;
	}
	
	// Copy CCCD value written by the phone into a local variable
	uint16_t cccd_value = 0;
	memcpy(&cccd_value, param->value, param->length);
	
	// Copy local variable into retained variable for notification logic in uvp_wireless_timer_cb
	uvp_cccd_value = cccd_value;
	
	#ifdef CFG_PRINTF
	arch_printf("[BATTERY VOLTAGE] cccd_value = %u \n\r", cccd_value);
	#endif

	if (uvp_cccd_value == 0x0001) // notifications enabled
	{
		#ifdef CFG_PRINTF
    arch_printf("[BATTERY VOLTAGE] Starting notifications. \n\r");
    #endif
	}
	else if (uvp_cccd_value == 0x0000) // notifications disabled
	{	
		#ifdef CFG_PRINTF
    arch_printf("[BATTERY VOLTAGE] Stopping notifications. \n\r");
    #endif
	}
}

void user_svc1_read_sensor_voltage_handler(ke_msg_id_t const msgid,
                                           struct custs1_value_req_ind const *param,
                                           ke_task_id_t const dest_id,
                                           ke_task_id_t const src_id)
{
	// Check UVP status
	if(uvp_shutdown)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Prevented characteristic change and forced exit of handler function \n\r");
		#endif
		
		return;
	}
	
	#ifdef CFG_PRINTF
	arch_printf("[READ SENSOR VOLTAGE] READING last saved value of sensor voltage. \n\r");
	arch_printf("[READ SENSOR VOLTAGE] Register Value: %u | Sensor Voltage: %u mV \n\r", sensor_adc_sample_raw, sensor_adc_sample_mv);
	arch_printf("[READ SENSOR VOLTAGE] LSB: 0x%02X, MSB: 0x%02X\n\r", sensor_adc_sample_mv & 0xFF, (sensor_adc_sample_mv >> 8) & 0xFF);
	#endif
	
	// Create dynamic kernel message for read response
	struct custs1_value_req_rsp *rsp = KE_MSG_ALLOC_DYN(CUSTS1_VALUE_REQ_RSP,
																											prf_get_task_from_id(TASK_ID_CUSTS1),
																											TASK_APP,
																											custs1_value_req_rsp,
																											DEF_SVC1_SENSOR_VOLTAGE_CHAR_LEN);

	// Fill response fields with expected values by the SDK
	rsp->conidx  = app_env[param->conidx].conidx; // connection index
	rsp->att_idx = param->att_idx; // attribute index
	rsp->length  = sizeof(sensor_adc_sample_mv); // current length that will be returned
	rsp->status  = ATT_ERR_NO_ERROR; // ATT error code
	
	// Copy last saved sensor voltage value to response payload
	memcpy(&rsp->value, &sensor_adc_sample_mv, rsp->length);
	
	// Send structure to the kernel to be transmitted by the BLE stack
	KE_MSG_SEND(rsp);
}

void user_svc1_read_battery_voltage_handler(ke_msg_id_t const msgid,
                                           struct custs1_value_req_ind const *param,
                                           ke_task_id_t const dest_id,
                                           ke_task_id_t const src_id)
{
	// Check UVP status
	if(uvp_shutdown)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Prevented characteristic change and forced exit of handler function \n\r");
		#endif
		
		return;
	}
	
	#ifdef CFG_PRINTF
	arch_printf("[READ BATTERY VOLTAGE] READING last saved value of battery. \n\r");
	arch_printf("[READ BATTERY VOLTAGE] Register Value: %u | Battery Voltage: %u mV \n\r", uvp_adc_sample_raw, uvp_adc_sample_mv);
	arch_printf("[READ BATTERY VOLTAGE] LSB: 0x%02X, MSB: 0x%02X\n\r", uvp_adc_sample_mv & 0xFF, (uvp_adc_sample_mv >> 8) & 0xFF);
	#endif
	
	// Create dynamic kernel message for read response
	struct custs1_value_req_rsp *rsp = KE_MSG_ALLOC_DYN(CUSTS1_VALUE_REQ_RSP,
																											prf_get_task_from_id(TASK_ID_CUSTS1),
																											TASK_APP,
																											custs1_value_req_rsp,
																											DEF_SVC1_BATTERY_VOLTAGE_CHAR_LEN);

	// Fill response fields with expected values by the SDK
	rsp->conidx  = app_env[param->conidx].conidx; // connection index
	rsp->att_idx = param->att_idx; // attribute index
	rsp->length  = sizeof(uvp_adc_sample_mv); // current length that will be returned
	rsp->status  = ATT_ERR_NO_ERROR; // ATT error code
	
	// Copy last saved battery voltage value to response payload
	memcpy(&rsp->value, &uvp_adc_sample_mv, rsp->length);
	
	// Send structure to the kernel to be transmitted by the BLE stack
	KE_MSG_SEND(rsp);
}

arch_main_loop_callback_ret_t user_app_on_system_powered(void)
{
	wdg_freeze(); // freeze watchdog timer
	
	// Initiates UVP timer only once
	if(!uvp_timer_initialized){
		uvp_timer = app_easy_timer(50, uvp_wireless_timer_cb);
		uvp_timer_initialized = true;
	}
	
	wdg_resume(); // resume watchdog timer
	
	return GOTO_SLEEP; // returning KEEP_POWERED hardfaults to nmi_handler.c, likely due to how SDK handles sleep mode
}

void user_app_on_init(void)
{
	// Initialize user retained variables to safe defaults
	uvp_shutdown = false;
	uvp_timer_initialized = false;
	uvp_cccd_value = 0;
	uvp_adc_sample_raw = 0;
	uvp_adc_sample_mv = 0;
	
	sensor_adc_sample_raw = 0;
	sensor_adc_sample_mv = 0;
	
	// Start the default initialization process for BLE user application
	// SDK doc states that this should be the last line called in this function
	default_app_on_init();
}

/// @} APP
