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

// Constant ADC offset from GND measurement
// FIXME: do some more testing here with bench power supply
static const uint16_t adc_software_offset_mv = 34U;

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

// PWM variables
timer_hnd pwm_dc_control_timer __SECTION_ZERO("retention_mem_area0");
int16_t target_vbias_1_mv __SECTION_ZERO("retention_mem_area0");
int16_t target_vbias_2_mv __SECTION_ZERO("retention_mem_area0");
uint32_t pulse_width_1 __SECTION_ZERO("retention_mem_area0");
uint32_t pulse_width_2 __SECTION_ZERO("retention_mem_area0");
uint32_t period_width __SECTION_ZERO("retention_mem_area0");

/*
 ****************************************************************************************
 * UVP FUNCTIONS
 ****************************************************************************************
*/

void uvp_wireless_timer_cb(void)
{
	// Initialize ADC for a single conversion of VBAT HIGH rail
	gpadc_init_se(ADC_INPUT_SE_VBAT_HIGH, 3, ADC_INPUT_ATTN_4X, true, 4);
	
	// Read ADC and convert results to millivolts
	adc_enable();
	uvp_adc_sample_raw = gpadc_collect_sample();
	uvp_adc_sample_mv = gpadc_sample_to_mv(uvp_adc_sample_raw);
	uvp_adc_sample_mv -= adc_software_offset_mv;
	adc_disable();
	
	// Compare ADC value to see if it is less than chosen UVP threshold (1800 mV)
	if(uvp_adc_sample_mv < (uint16_t)1800)
	{
		// GPIO enable signal is controlled by this flag in user_periph_setup.c
		uvp_shutdown = true;

		// Stop sensor voltage peripheral and timer
		if (sensor_timer != EASY_TIMER_INVALID_TIMER) // prevents cancel when timer does not exist
		{
			// Cancels existing timer
			app_easy_timer_cancel(sensor_timer);
			sensor_timer = EASY_TIMER_INVALID_TIMER;
		}
		
		// Stop vbias peripheral and timer
		timer2_pwm_disable();
	}
	else
	{
		uvp_shutdown = false;
	}
	
	if (uvp_cccd_value == 0x0001 && (ke_state_get(TASK_APP) == APP_CONNECTED)) // notifications enabled and phone connected
	{
		#ifdef CFG_PRINTF
		arch_printf("[UVP] Battery Voltage: %u mV \n\r", uvp_adc_sample_mv);
		arch_printf("[UVP] System undervoltage shutdown status: %s \n\r", uvp_shutdown ? "true" : "false");
		arch_printf("[UVP] LSB: 0x%02X, MSB: 0x%02X \n\r", uvp_adc_sample_mv & 0xFF, (uvp_adc_sample_mv >> 8) & 0xFF);
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
	
	// Restart this function every 0.5 second
	uvp_timer = app_easy_timer(50, uvp_wireless_timer_cb);
	
	/*
   ****************************************************************************************
   * UART test prints
   ****************************************************************************************
	 */

	#ifdef CFG_PRINTF
	// Prints VBAT_LOW voltage
	syscntl_dcdc_level_t vbat_low = syscntl_dcdc_get_level();
	
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

	arch_printf("[DCDC] VBAT_LOW voltage level: %s \n\r", voltage_str);
	
	// Prints current sleep mode of system
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

	arch_printf("[SLEEP] Current Mode: %s (Value: %u) \n\r", mode_str, (uint8_t)current_mode);
	
	// Print PWM parameters
	arch_printf("[PWM DUTY] Pulse Width 1: %lu \n\r", pulse_width_1);
	arch_printf("[PWM DUTY] Pulse Width 2: %lu \n\r", pulse_width_2);
	arch_printf("[PWM DUTY] Period Width: %lu \n\r", period_width);
	#endif
}

/*
 ****************************************************************************************
 * ADC FUNCTIONS
 ****************************************************************************************
*/

void gpadc_wireless_timer_cb(void)
{
	// Initialize ADC for a single conversion of sensor voltage
	gpadc_init_se(ADC_ENUM_INPUT, 3, ADC_INPUT_ATTN_4X, true, 4);
	
	// Read ADC and convert results to millivolts
	adc_enable();
	sensor_adc_sample_raw = gpadc_collect_sample();
	sensor_adc_sample_mv = gpadc_sample_to_mv(sensor_adc_sample_raw);
	sensor_adc_sample_mv -= adc_software_offset_mv;
	adc_disable();
	
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
	
	// Send structure to the kernel to be transmitted by the BLE stack
	KE_MSG_SEND(req);
	
	// If phone is still connected, restart this function every 1 second
	if (ke_state_get(TASK_APP) == APP_CONNECTED)
	{
			sensor_timer = app_easy_timer(100, gpadc_wireless_timer_cb);
	}
	
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	arch_printf("[ADC] Sensor Voltage: %u mV \n\r", sensor_adc_sample_mv);
	arch_printf("[ADC] LSB: 0x%02X, MSB: 0x%02X \n\r", sensor_adc_sample_mv & 0xFF, (sensor_adc_sample_mv >> 8) & 0xFF);
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
}

void gpadc_init_se(adc_input_se_t input, uint8_t smpl_time_mult, adc_input_attn_t input_attenuator, bool chopping, uint8_t oversampling)
{
	// Build ADC config structure for single-ended measurement
	adc_config_t adc_config_struct =
	{
		// ADC options that are fixed due to hardware and implementation
		.input_mode = ADC_INPUT_MODE_SINGLE_ENDED,
		.continuous = false,
		.interval_mult = 0,

		// Adjustable ADC options for SE mode only
		.input = input,
		.smpl_time_mult = smpl_time_mult,
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

	// Perform offset calibration to remove DC offsets before sampling
	// BUG: ADC still reads 34 mV for GND, compensated for this offset in SW during reads
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

// BUG: PWM registers do not retain value or can be wrote too when sleep mode is on

void timer2_pwm_dc_control_timer_cb(void)
{
	// Update duty cycles based on VBAT ADC reading for a select channel
	timer2_pwm_dc_control(target_vbias_1_mv, TIM2_PWM_2);
	timer2_pwm_dc_control(target_vbias_2_mv, TIM2_PWM_3);
	
	// Restart this function every 0.5 second
	pwm_dc_control_timer = app_easy_timer(50, timer2_pwm_dc_control_timer_cb);
}

void timer2_pwm_dc_control(int16_t target_vbias_mv, tim2_pwm_t channel)
{
	// Prevent SoC from sleeping
	arch_set_sleep_mode(ARCH_SLEEP_OFF);
	
	// Read Timer 2 period counter register
	uint32_t period_count = GetWord16(TRIPLE_PWM_FREQUENCY) + 1u;
	
	// Declare variables used to read existing offset
	volatile uint16_t *start_reg;
	volatile uint16_t *end_reg;
	
	// Select the correct START and END cycle registers based on the channel
	switch (channel)
	{
		case TIM2_PWM_2:
				start_reg = (volatile uint16_t *)PWM2_START_CYCLE;
				end_reg = (volatile uint16_t *)PWM2_END_CYCLE;
				break;
		case TIM2_PWM_3:
				start_reg = (volatile uint16_t *)PWM3_START_CYCLE;
				end_reg = (volatile uint16_t *)PWM3_END_CYCLE;
				break;
		case TIM2_PWM_4:
				start_reg = (volatile uint16_t *)PWM4_START_CYCLE;
				end_reg = (volatile uint16_t *)PWM4_END_CYCLE;
				break;
		#if defined (__DA14531__)
		case TIM2_PWM_5:
				start_reg = (volatile uint16_t *)PWM5_START_CYCLE;
				end_reg = (volatile uint16_t *)PWM5_END_CYCLE;
				break;
		case TIM2_PWM_6:
				start_reg = (volatile uint16_t *)PWM6_START_CYCLE;
				end_reg = (volatile uint16_t *)PWM6_END_CYCLE;
				break;
		case TIM2_PWM_7:
				start_reg = (volatile uint16_t *)PWM7_START_CYCLE;
				end_reg = (volatile uint16_t *)PWM7_END_CYCLE;
				break;
		#endif
		default:
				return;
	}
	
	// Read existing offset from correct START_CYCLE register
	uint32_t offset_count = GetWord16(start_reg);
	
	// Read battery voltage
	uint16_t vbat_mv = uvp_adc_sample_mv;
	
	// Guard against division near zero
	if(vbat_mv < 100)
	{
		#ifdef CFG_PRINTF
		arch_printf("[ERROR] Vbat divisor near zero (<100mV) \n\r");
		#endif
		
		return;
	}
	
	// Construct (duty cycle * period) function as function of vbias and vbat
	int32_t half_period = (int32_t)period_count / 2;
	int32_t numerator   = 5 * (int32_t)target_vbias_mv * (int32_t)period_count;
	int32_t denominator = 7 * (int32_t)vbat_mv;
	int32_t second_term = numerator / denominator;
	int32_t pulse_width_raw = half_period - second_term;
	
	// Clamp pulse width as safety, not using CLAMP macro due to uint and int comparison
	uint32_t pulse_width;
	if (pulse_width_raw < 0)
	{
		pulse_width = 0;
	}
	else if (pulse_width_raw > (int32_t)period_count)
	{
		pulse_width = period_count;
	}
	else
	{
		pulse_width = (uint32_t)pulse_width_raw;
	}
	
	// Add offset to pulse width to create END_CYCLE value
	uint32_t end_cycle_value_raw = pulse_width + offset_count;
	uint32_t end_cycle_value;
	
	// Clamp END_CYCLE value via wrap-around logic
	if (end_cycle_value_raw >= period_count)
	{
			end_cycle_value = end_cycle_value_raw - period_count;
	}
	else
	{
			end_cycle_value = end_cycle_value_raw;
	}
	
	// Write the new END_CYCLE value directly to the register
	SetWord16(end_reg, end_cycle_value);
	
	// BUG: UART prints will cause CPU SW reset if function is called from BLE handler
	/*
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	arch_printf("[PWM CONTROL CH%u] Read target vbias: %ld mV \n\r", channel + 1, (int32_t)target_vbias_mv);
	arch_printf("[PWM CONTROL CH%u] Read period count: %lu \n\r", channel + 1, period_count);
	arch_printf("[PWM CONTROL CH%u] Read offset: %lu counts \n\r", channel + 1, offset_count);
	arch_printf("[PWM CONTROL CH%u] Calculated Pulse Width: %lu counts \n\r", channel + 1, pulse_width);
	arch_printf("[PWM CONTROL CH%u] Calculated END_CYCLE: %lu counts \n\r", channel + 1, end_cycle_value);
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
	*/
	
	// Save period width and pulse width to global variable instead of UART printout
	period_width = period_count;
	switch (channel)
	{
		case TIM2_PWM_2:
				pulse_width_1 = pulse_width;
				break;
		case TIM2_PWM_3:
				pulse_width_2 = pulse_width;
				break;
		case TIM2_PWM_4:
				break;
		#if defined (__DA14531__)
		case TIM2_PWM_5:
				break;
		case TIM2_PWM_6:
				break;
		case TIM2_PWM_7:
				break;
		#endif
		default:
				return;
	}
}

void timer2_pwm_set_offset(uint8_t offset_percentage, tim2_pwm_t channel)
{
	// Prevent SoC from sleeping
	arch_set_sleep_mode(ARCH_SLEEP_OFF);
	
	// Clamp input percentage
	uint8_t offset_clamped = CLAMP(offset_percentage, 0, 100);
	
	// Read Timer 2 period counter register
	uint32_t period_count = GetWord16(TRIPLE_PWM_FREQUENCY) + 1u;
	
	// Calculate the offset value in timer counts (32-bit)
	uint32_t offset_count = (period_count * offset_clamped) / 100u;
	
	// Determine the correct register address based on the channel
	volatile uint16_t *start_reg;
	
	switch (channel)
	{
			case TIM2_PWM_2: start_reg = (volatile uint16_t *)PWM2_START_CYCLE; break;
			case TIM2_PWM_3: start_reg = (volatile uint16_t *)PWM3_START_CYCLE; break;
			case TIM2_PWM_4: start_reg = (volatile uint16_t *)PWM4_START_CYCLE; break;
			#if defined (__DA14531__)
			case TIM2_PWM_5: start_reg = (volatile uint16_t *)PWM5_START_CYCLE; break;
			case TIM2_PWM_6: start_reg = (volatile uint16_t *)PWM6_START_CYCLE; break;
			case TIM2_PWM_7: start_reg = (volatile uint16_t *)PWM7_START_CYCLE; break;
			#endif
			default: return;
	}
	
	// Write the offset to the register
	SetWord16(start_reg, offset_count);
	
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	arch_printf("[PWM OFFSET CH%u] Target Offset: %u percent \n\r", channel + 1, offset_clamped);
	arch_printf("[PWM OFFSET CH%u] Read period count: %lu \n\r", channel + 1, period_count);
	arch_printf("[PWM CONTROL CH%u] Calculated offset counts: %lu counts \n\r", channel + 1, offset_count);
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
}

void timer2_pwm_set_frequency(tim0_2_clk_div_t clk_div, tim2_clk_src_t clk_src, uint16_t pwm_div)
{
	// Prevent SoC from sleeping
	arch_set_sleep_mode(ARCH_SLEEP_OFF);
	
	// Create config structures for clock division and timer2 config
	tim0_2_clk_div_config_t clk_cfg = {
		.clk_div = clk_div
	};
	
	tim2_config_t tmr_cfg =
	{
		.clk_source = clk_src,
    .hw_pause = TIM2_HW_PAUSE_OFF
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
	
	// Calculate PWM output frequency based on formula from datasheet for Timer 2
	uint32_t output_freq = input_freq / (uint32_t)pwm_div;

	// Set PWM output frequency
	timer2_pwm_freq_set(output_freq, input_freq);
	
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	arch_printf("[PWM FREQ] clk_div value = %u (0x%02X) \n\r", clk_div, clk_div);
	arch_printf("[PWM FREQ] clk_src value = %u (0x%02X) \n\r", clk_src, clk_src);
	arch_printf("[PWM FREQ] pwm_div (clamped value) = %u (0x%04X) \n\r", pwm_div, pwm_div);
	arch_printf("[PWM FREQ] Input clock freq: %lu Hz \n\r", input_freq);
	arch_printf("[PWM FREQ] Output PWM freq: %lu Hz \n\r", output_freq);
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
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
	// Prevent SoC from sleeping
	arch_set_sleep_mode(ARCH_SLEEP_OFF);
	
	// Enable timer input clock
	timer0_2_clk_enable();
	
	// Start PWM duty cycle updates
	pwm_dc_control_timer = app_easy_timer(50, timer2_pwm_dc_control_timer_cb);
	
	// Enable PWM outputs
	timer2_start();
	
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	arch_printf("[PWM ENABLE] PWM output on. \n\r");
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
}

void timer2_pwm_disable(void)
{
	// Stop PWM duty cycle updates
	if (pwm_dc_control_timer != EASY_TIMER_INVALID_TIMER) // prevents cancel when timer does not exist
	{
		// Cancels existing timer
		app_easy_timer_cancel(pwm_dc_control_timer);
		pwm_dc_control_timer = EASY_TIMER_INVALID_TIMER;
	}
	
	// Disable PWM outputs
	timer2_stop();

	// Disable timer input clock
	timer0_2_clk_disable();
	
	// Return sleep mode to default
	arch_set_sleep_mode(ARCH_EXT_SLEEP_ON);
	
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	arch_printf("[PWM DISABLE] PWM output off. \n\r");
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
}

/*
 ****************************************************************************************
 * USER CALLBACK FUNCTIONS
 ****************************************************************************************
*/

void user_on_connection(uint8_t connection_idx, struct gapc_connection_req_ind const *param)
{
	default_app_on_connection(connection_idx, param);
	
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	arch_printf("[BLE] Phone connected to DA14531. \n\r");
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
}

void user_on_disconnect(struct gapc_disconnect_ind const *param )
{
	default_app_on_disconnect(param);
	
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	arch_printf("[BLE] Phone disconnected from DA14531. \n\r");
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
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
				
				case SVC1_IDX_PWM_VBIAS_AND_OFFSET_VAL:
					user_svc1_pwm_vbias_and_offset_wr_ind_handler(msgid, msg_param, dest_id, src_id);
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
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
	
	// Check UVP status
	if(uvp_shutdown)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Prevented characteristic change and forced exit of handler function \n\r");
		arch_printf("---------------------------------------------------------------------------------------- \n\r");
		#endif
		
		return;
	}
	
	// Copy CCCD value written by the phone into a local variable
	uint16_t cccd_value = 0; // set to zero for safe memcpy
	memcpy(&cccd_value, param->value, param->length);
	
	#ifdef CFG_PRINTF
	arch_printf("[BLE - SENSOR VOLTAGE] cccd_value = %u \n\r", cccd_value);
	#endif

	if (cccd_value == 0x0001 && (ke_state_get(TASK_APP) == APP_CONNECTED)) // notifications enabled and phone connected
	{
		#ifdef CFG_PRINTF
    arch_printf("[BLE - SENSOR VOLTAGE] Starting the ADC. \n\r");
    #endif
		
		// Start ADC conversions on 1 second timer
		sensor_timer = app_easy_timer(100, gpadc_wireless_timer_cb);
	}
	else if (cccd_value == 0x0000) // notifications disabled
	{
		#ifdef CFG_PRINTF
    arch_printf("[BLE - SENSOR VOLTAGE] Stopping the ADC. \n\r");
    #endif
		
		// Stop ADC conversions
		if (sensor_timer != EASY_TIMER_INVALID_TIMER) // prevents cancel when timer does not exist
		{
			// Cancels existing timer
			app_easy_timer_cancel(sensor_timer);
			sensor_timer = EASY_TIMER_INVALID_TIMER;
		}
	}
	
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
}

void user_svc1_pwm_freq_wr_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id)
{
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif	
	
	// Check UVP status
	if(uvp_shutdown)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Prevented characteristic change and forced exit of handler function \n\r");
		arch_printf("---------------------------------------------------------------------------------------- \n\r");
		#endif
		
		return;
	}
	
	// Validate length of characteristic value written by the phone
	if (param->length != DEF_SVC1_PWM_FREQ_CHAR_LEN)
	{
    #ifdef CFG_PRINTF
    arch_printf("[WARNING] Invalid packet byte length: %u (expected %u) \n\r", param->length, DEF_SVC1_PWM_FREQ_CHAR_LEN);
		arch_printf("---------------------------------------------------------------------------------------- \n\r");
    #endif
		
    return; // ignore incomplete write
	}
	
	// Parse byte array into expected values
	// Byte order is [clk_div, clk_src, pwm_div_MSB, pwm_div_LSB]
	uint8_t clk_div = param->value[0];
	uint8_t clk_src = param->value[1];
	uint16_t pwm_div = ((param->value[2] << 8) | param->value[3]);

	// Validate inputs with enum int literals from timer headers
	if (clk_div > TIM0_2_CLK_DIV_8 || clk_src > TIM2_CLK_SYS) // literals are auto-promoted to uint8 for comparison
	{
		#ifdef CFG_PRINTF
    arch_printf("[WARNING] Invalid clk_div or clk_src write (first 2 bytes), input is ignored. \n\r");
		arch_printf("---------------------------------------------------------------------------------------- \n\r");
    #endif
		
		return; // ignore invalid write
	}

	// Apply values to function
	timer2_pwm_set_frequency((tim0_2_clk_div_t)clk_div, (tim2_clk_src_t)clk_src, pwm_div); // note that pwm_div is clamped in this function already
	
	#ifdef CFG_PRINTF
	arch_printf("[BLE - PWM FREQ] SUCCESS on setting config. \n\r");
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
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
		arch_printf("[WARNING] Invalid packet byte length: %u (expected %u) \n\r", param->length, DEF_SVC1_PWM_DC_AND_OFFSET_CHAR_LEN);
		#endif
		
    return; // ignore incomplete write
	}
	
	// Parse byte array into expected values
	// Byte order is [dc_pwm2, offset_pwm2, dc_pwm3, offset_pwm3]
	uint8_t dc_pwm2 = param->value[0];
	uint8_t offset_pwm2 = param->value[1];
	uint8_t dc_pwm3 = param->value[2];
	uint8_t offset_pwm3 = param->value[3];
	
	// Apply values to function (which already has clamp protection for all inputs)
	timer2_pwm_set_dc_and_offset(dc_pwm2, offset_pwm2, dc_pwm3, offset_pwm3);
	
	#ifdef CFG_PRINTF
	arch_printf("[PWM DC AND OFFSET] Bytes received. \n\r");
	arch_printf("[PWM DC AND OFFSET] dc_pwm2 value = %u (0x%02X) \n\r", dc_pwm2, dc_pwm2);
	arch_printf("[PWM DC AND OFFSET] offset_pwm2 value = %u (0x%02X) \n\r", offset_pwm2, offset_pwm2);
	arch_printf("[PWM DC AND OFFSET] dc_pwm3 value = %u (0x%02X) \n\r", dc_pwm3, dc_pwm3);
	arch_printf("[PWM DC AND OFFSET] offset_pwm3 value = %u (0x%02X) \n\r", offset_pwm3, offset_pwm3);
	arch_printf("[PWM DC AND OFFSET] SUCCESS on setting config. \n\r");
	#endif
}

void user_svc1_pwm_vbias_and_offset_wr_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id)
{
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
	
	// Check UVP status
	if(uvp_shutdown)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Prevented characteristic change and forced exit of handler function \n\r");
		arch_printf("---------------------------------------------------------------------------------------- \n\r");
		#endif
		
		return;
	}
	
	// Validate length of characteristic value written by the phone
	if (param->length != DEF_SVC1_PWM_VBIAS_AND_OFFSET_CHAR_LEN)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Invalid packet byte length: %u (expected %u) \n\r", param->length, DEF_SVC1_PWM_VBIAS_AND_OFFSET_CHAR_LEN);
		arch_printf("---------------------------------------------------------------------------------------- \n\r");
		#endif
		
    return; // ignore incomplete write
	}
	
	// Parse byte array into expected values
	// Byte order is [vbias_1_mv_msb, vbias_1_mv_lsb, zero_cal_1_msb, zero_cal_1_lsb, offset_1,
	// vbias_2_mv_msb, vbias_2_mv_lsb, zero_cal_2_msb, zero_cal_2_lsb, offset_2]
	int16_t vbias_1_mv = ((param->value[0] << 8)| param->value[1]);
	int16_t zero_cal_1 = ((param->value[2] << 8)| param->value[3]); // vbias voltage value measured when target vbias is 0V
	uint8_t offset_1 = param->value[4];
	int16_t vbias_2_mv = ((param->value[5] << 8) | param->value[6]);
	int16_t zero_cal_2 = ((param->value[7] << 8) | param->value[8]);
	uint8_t offset_2 = param->value[9];
	
	// Compensate for uncentered op amp rails via subtraction of zero_cal values
	vbias_1_mv -= zero_cal_1;
	vbias_2_mv -= zero_cal_2;
	
	// Clamp target vbias voltages from -1V to 1V (HW specific)
	vbias_1_mv = CLAMP(vbias_1_mv, -1000, 1000);
	vbias_2_mv = CLAMP(vbias_2_mv, -1000, 1000);
	
	// Set offsets for PWM2 and PWM3
	timer2_pwm_set_offset(offset_1, TIM2_PWM_2);
	timer2_pwm_set_offset(offset_2, TIM2_PWM_3);
	
	// Set initial duty cycle for PWM2 and PWM3
	timer2_pwm_dc_control(vbias_1_mv, TIM2_PWM_2);
	timer2_pwm_dc_control(vbias_2_mv, TIM2_PWM_3);
	
	// Update retained value in memory for timer function when PWM enable
	target_vbias_1_mv = vbias_1_mv;
	target_vbias_2_mv = vbias_2_mv;
	
	#ifdef CFG_PRINTF
	arch_printf("[BLE - PWM VBIAS] Bytes received. \n\r");
	arch_printf("[BLE - PWM VBIAS] vbias_1_mv = %ld (0x%04X) \n\r", (int32_t)vbias_1_mv, (uint16_t)vbias_1_mv);
	arch_printf("[BLE - PWM VBIAS] zero_cal_1 = %ld (0x%04X) \n\r", (int32_t)zero_cal_1, (uint16_t)zero_cal_1);
	arch_printf("[BLE - PWM VBIAS] offset_1 = %u (0x%02X) \n\r", offset_1, offset_1);
	arch_printf("[BLE - PWM VBIAS] vbias_2_mv = %ld (0x%04X) \n\r", (int32_t)vbias_2_mv, (uint16_t)vbias_2_mv);
	arch_printf("[BLE - PWM VBIAS] zero_cal_2 = %ld (0x%04X) \n\r", (int32_t)zero_cal_2, (uint16_t)zero_cal_2);
	arch_printf("[BLE - PWM VBIAS] offset_2 = %u (0x%02X) \n\r", offset_2, offset_2);
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
}

void user_svc1_pwm_state_wr_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id)
{	
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
	
	// Check UVP status
	if(uvp_shutdown)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Prevented characteristic change and forced exit of handler function \n\r");
		arch_printf("---------------------------------------------------------------------------------------- \n\r");
		#endif
		
		return;
	}
	
	// Copy single byte value to variable
	uint8_t state = 0;
	memcpy(&state, param->value, param->length);
	
	#ifdef CFG_PRINTF
	arch_printf("[BLE - PWM STATE] Byte received. \n\r");
	arch_printf("[BLE - PWM STATE] state value = %u (0x%02X) \n\r", state, state);
	#endif
	
	// Turns on or off PWM output based on value
	if(state > 1)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Invalid input. \n\r");
		arch_printf("---------------------------------------------------------------------------------------- \n\r");
		#endif
		
		return; // ignore invalid write
	} 
	else if(state == 1)
	{
		timer2_pwm_enable(); // turn on output
	}
	else if(state == 0)
	{
		timer2_pwm_disable(); // turn off output
	}
	
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
}

void user_svc1_battery_voltage_cfg_ind_handler(ke_msg_id_t const msgid,
                               struct custs1_val_write_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id)
{
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
	
	// Check UVP status
	if(uvp_shutdown)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Prevented characteristic change and forced exit of handler function \n\r");
		arch_printf("---------------------------------------------------------------------------------------- \n\r");
		#endif
		
		return;
	}
	
	// Copy CCCD value written by the phone into a local variable
	uint16_t cccd_value = 0;
	memcpy(&cccd_value, param->value, param->length);
	
	// Copy local variable into retained variable for notification logic in uvp_wireless_timer_cb
	uvp_cccd_value = cccd_value;
	
	#ifdef CFG_PRINTF
	arch_printf("[BLE - BATTERY VOLTAGE] cccd_value = %u \n\r", cccd_value);
	#endif

	if (uvp_cccd_value == 0x0001) // notifications enabled
	{
		#ifdef CFG_PRINTF
    arch_printf("[BLE - BATTERY VOLTAGE] Starting notifications. \n\r");
    #endif
	}
	else if (uvp_cccd_value == 0x0000) // notifications disabled
	{	
		#ifdef CFG_PRINTF
    arch_printf("[BLE - BATTERY VOLTAGE] Stopping notifications. \n\r");
    #endif
	}
	
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
}

void user_svc1_read_sensor_voltage_handler(ke_msg_id_t const msgid,
                                           struct custs1_value_req_ind const *param,
                                           ke_task_id_t const dest_id,
                                           ke_task_id_t const src_id)
{
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
	
	// Check UVP status
	if(uvp_shutdown)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Prevented characteristic change and forced exit of handler function \n\r");
		arch_printf("---------------------------------------------------------------------------------------- \n\r");
		#endif
		
		return;
	}
	
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
	
	#ifdef CFG_PRINTF
	arch_printf("[BLE - SENSOR VOLTAGE] READING last saved value of sensor voltage. \n\r");
	arch_printf("[BLE - SENSOR VOLTAGE] Sensor Voltage: %u mV \n\r", sensor_adc_sample_mv);
	arch_printf("[BLE - SENSOR VOLTAGE] LSB: 0x%02X, MSB: 0x%02X\n\r", sensor_adc_sample_mv & 0xFF, (sensor_adc_sample_mv >> 8) & 0xFF);
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
}

void user_svc1_read_battery_voltage_handler(ke_msg_id_t const msgid,
                                           struct custs1_value_req_ind const *param,
                                           ke_task_id_t const dest_id,
                                           ke_task_id_t const src_id)
{
	#ifdef CFG_PRINTF
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
	
	// Check UVP status
	if(uvp_shutdown)
	{
		#ifdef CFG_PRINTF
		arch_printf("[WARNING] Prevented characteristic change and forced exit of handler function \n\r");
		arch_printf("---------------------------------------------------------------------------------------- \n\r");
		#endif
		
		return;
	}
	
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
	
	#ifdef CFG_PRINTF
	arch_printf("[BLE - BATTERY VOLTAGE] READING last saved value of battery. \n\r");
	arch_printf("[BLE - BATTERY VOLTAGE] Battery Voltage: %u mV \n\r", uvp_adc_sample_mv);
	arch_printf("[BLE - BATTERY VOLTAGE] LSB: 0x%02X, MSB: 0x%02X\n\r", uvp_adc_sample_mv & 0xFF, (uvp_adc_sample_mv >> 8) & 0xFF);
	arch_printf("---------------------------------------------------------------------------------------- \n\r");
	#endif
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
	uvp_timer_initialized = false;
	uvp_cccd_value = 0;
	uvp_adc_sample_raw = 0;
	uvp_adc_sample_mv = 0;
	uvp_shutdown = false;
	
	sensor_adc_sample_raw = 0;
	sensor_adc_sample_mv = 0;
	
	target_vbias_1_mv = 0;
	target_vbias_2_mv = 0;
	pulse_width_1 = 0;
	pulse_width_2 = 0;
	period_width = 0;
	
	// Start the default initialization process for BLE user application
	// SDK doc states that this should be the last line called in this function
	default_app_on_init();
}

/// @} APP
