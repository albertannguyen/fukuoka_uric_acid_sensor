/**
 ****************************************************************************************
 * @file user_periph_setup.c
 * @brief Peripherals setup and initialization.
 * @author Albert Nguyen
 * @note This file run multiple times due to wake up from sleep.
 ****************************************************************************************
 */

/*
 ****************************************************************************************
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "user_periph_setup.h"
#include "datasheet.h"
#include "system_library.h"
#include "rwip_config.h"
#include "gpio.h"
#include "uart.h"
#include "syscntl.h"

// Needed for uvp_shutdown
#include "user_empty_peripheral_template.h"

/*
 ****************************************************************************************
 * GPIO RESERVATIONS
 ****************************************************************************************
		i.e. to reserve P0_1 as Generic Purpose I/O:
		RESERVE_GPIO(DESCRIPTIVE_NAME, GPIO_PORT_0, GPIO_PIN_1, PID_GPIO);
 */

#if DEVELOPMENT_DEBUG

void GPIO_reservations(void)
{
	#if defined (CFG_PRINTF_UART2)
			RESERVE_GPIO(UART2_TX, UART2_TX_PORT, UART2_TX_PIN, PID_UART2_TX);
	#endif

	#if !defined (__DA14586__)
			RESERVE_GPIO(SPI_EN, SPI_EN_PORT, SPI_EN_PIN, PID_SPI_EN);
	#endif

	// reserve UVP pins as GPIO
	RESERVE_GPIO(UVP_EN_OUTPUT, UVP_EN_OUTPUT_PORT, UVP_EN_OUTPUT_PIN, PID_GPIO);
	
	// reserve ADC pins
	RESERVE_GPIO(ADC_INPUT, ADC_INPUT_PORT, ADC_INPUT_PIN, PID_ADC);
	
	// reserve PWM pins
	RESERVE_GPIO(PWM2_OUTPUT, PWM2_OUTPUT_PORT, PWM2_OUTPUT_PIN, PID_PWM2);
	RESERVE_GPIO(PWM3_OUTPUT, PWM3_OUTPUT_PORT, PWM3_OUTPUT_PIN, PID_PWM3);
}

#endif

/*
 ****************************************************************************************
 * GPIO CONFIGURATION
 ****************************************************************************************
		i.e. to set P0_1 as Generic purpose Output:
		GPIO_ConfigurePin(GPIO_PORT_0, GPIO_PIN_1, OUTPUT, PID_GPIO, false);

		last argument sets initial level of digital output, true is high and false is low
		last argument is ignored if pin is configured as an input
		this info can be found in gpio.c and gpio.h
 */

void set_pad_functions(void)
{
	#if defined (__DA14586__)
			// Disallow spontaneous DA14586 SPI Flash wake-up
			GPIO_ConfigurePin(GPIO_PORT_2, GPIO_PIN_3, OUTPUT, PID_GPIO, true);
	#else
			// Disallow spontaneous SPI Flash wake-up
			GPIO_ConfigurePin(SPI_EN_PORT, SPI_EN_PIN, OUTPUT, PID_SPI_EN, true);
	#endif

	#if defined (CFG_PRINTF_UART2)
			// Configure UART2 TX Pad
			GPIO_ConfigurePin(UART2_TX_PORT, UART2_TX_PIN, OUTPUT, PID_UART2_TX, false);
	#endif
	
	// set pin 9 (enable) as digital output high or low based on flag in user_empty_peripheral_template.c
	if(uvp_shutdown)
	{
		GPIO_ConfigurePin(UVP_EN_OUTPUT_PORT, UVP_EN_OUTPUT_PIN, OUTPUT, PID_GPIO, false); // enable output low
	}
	else
	{
		GPIO_ConfigurePin(UVP_EN_OUTPUT_PORT, UVP_EN_OUTPUT_PIN, OUTPUT, PID_GPIO, true); // enable output high
	}
	
	// set ADC pin as input
	GPIO_ConfigurePin(ADC_INPUT_PORT, ADC_INPUT_PIN, INPUT, PID_ADC, false);
	
	// set PWM pins
	GPIO_ConfigurePin(PWM2_OUTPUT_PORT, PWM2_OUTPUT_PIN, OUTPUT, PID_PWM2, false);
	GPIO_ConfigurePin(PWM3_OUTPUT_PORT, PWM3_OUTPUT_PIN, OUTPUT, PID_PWM3, false);
	
	// set GPIO outputs to reduced current driving strength (except for UART pin)
	// PWM low current is more accurate when testing with +/- 1V
	GPIO_ConfigurePinPower(UVP_EN_OUTPUT_PORT, UVP_EN_OUTPUT_PIN, GPIO_POWER_RAIL_1V);
	GPIO_ConfigurePinPower(PWM2_OUTPUT_PORT, PWM2_OUTPUT_PIN, GPIO_POWER_RAIL_1V);
	GPIO_ConfigurePinPower(PWM3_OUTPUT_PORT, PWM3_OUTPUT_PIN, GPIO_POWER_RAIL_1V);
}

#if defined (CFG_PRINTF_UART2)
// Configuration struct for UART2
static const uart_cfg_t uart_cfg = {
    .baud_rate = UART2_BAUDRATE,
    .data_bits = UART2_DATABITS,
    .parity = UART2_PARITY,
    .stop_bits = UART2_STOPBITS,
    .auto_flow_control = UART2_AFCE,
    .use_fifo = UART2_FIFO,
    .tx_fifo_tr_lvl = UART2_TX_FIFO_LEVEL,
    .rx_fifo_tr_lvl = UART2_RX_FIFO_LEVEL,
    .intr_priority = 2,
};
#endif

void periph_init(void)
{
	#if defined (__DA14531__)
	// Template code (old)
	// In Boost mode enable the DCDC converter to supply VBAT_HIGH for the used GPIOs
	// Assumption: The connected external peripheral is powered by 3V
	// syscntl_dcdc_turn_on_in_boost(SYSCNTL_DCDC_LEVEL_3V0);
	
	// Set converter to buck mode and generate VBAT_LOW = 1.1V typical value
	syscntl_dcdc_turn_on_in_buck(SYSCNTL_DCDC_LEVEL_1V1);
	
	#else
			// Power up peripherals' power domain
			SetBits16(PMU_CTRL_REG, PERIPH_SLEEP, 0);
			while (!(GetWord16(SYS_STAT_REG) & PER_IS_UP));
			SetBits16(CLK_16M_REG, XTAL16_BIAS_SH_ENABLE, 1);
	#endif

	// ROM patch
	patch_func();

	// Initialize peripherals
	#if defined (CFG_PRINTF_UART2)
			// Initialize UART2
			uart_initialize(UART2, &uart_cfg);
	#endif

	// Set pad functionality
	set_pad_functions();

	// Enable the pads
	GPIO_set_pad_latch_en(true);
}