/**
 ****************************************************************************************
 * @file user_custs1_def.h
 * @brief Custom Server 1 (CUSTS1) profile database definitions.
 * @note Albert Nguyen
 ****************************************************************************************
 */

#ifndef _USER_CUSTS1_DEF_H_
#define _USER_CUSTS1_DEF_H_

/**
 ****************************************************************************************
 * @defgroup USER_CONFIG
 * @ingroup USER
 * @brief Custom Server 1 (CUSTS1) profile database definitions.
 *
 * @{
 ****************************************************************************************
 */

/*
 ****************************************************************************************
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "attm_db_128.h"

/*
 ****************************************************************************************
 * DEFINES
 ****************************************************************************************
 */

// Define service 1 UUID (custom service for ADC data)
#define DEF_SVC1_UUID_128 {0x72, 0x0E, 0x9F, 0xE8, 0xDE, 0xEC, 0x12, 0x4D, 0x99, 0xA5, 0xED, 0x64, 0xF3, 0xC4, 0x21, 0xB4}

/*
----------------------------------
- Characteristics
----------------------------------
*/

// Each one needs UUID, length, and user description

// Define sensor voltage
#define DEF_SVC1_SENSOR_VOLTAGE_UUID_128 {0x04, 0x03, 0x08, 0x32, 0x72, 0x9D, 0x42, 0x93, 0x61, 0x4D, 0x48, 0xF6, 0x92, 0x2C, 0xD2, 0x41}
#define DEF_SVC1_SENSOR_VOLTAGE_CHAR_LEN 2 // 2 bytes for 10-bit ADC
#define DEF_SVC1_SENSOR_VOLTAGE_USER_DESC "Sensor Voltage (little-endian bytes)"

// Define PWM freq
#define DEF_SVC1_PWM_FREQ_UUID_128 {0xC8, 0x69, 0x15, 0x55, 0x2C, 0x2B, 0xB8, 0x4D, 0xAB, 0x36, 0x66, 0x42, 0x52, 0x6E, 0x5A, 0x4A}
#define DEF_SVC1_PWM_FREQ_CHAR_LEN 4
#define DEF_SVC1_PWM_FREQ_USER_DESC "Timer2 PWM Frequency Config"

// WIP: Define standard bluetooth battery service to report battery voltage level to app below, ideally in a separate service

/// Custom1 Service Data Base Characteristic enum
enum
{
    // Service 1
		// Adds service 1 index as starting point (SDK line)
    SVC1_IDX_SVC = 0,
		
		// Adds service 1 attributes
		SVC1_IDX_SENSOR_VOLTAGE_CHAR,
		SVC1_IDX_SENSOR_VOLTAGE_VAL,
		SVC1_IDX_SENSOR_VOLTAGE_NTF_CFG,
		SVC1_IDX_SENSOR_VOLTAGE_USER_DESC,
	
		SVC1_IDX_PWM_FREQ_CHAR,
		SVC1_IDX_PWM_FREQ_VAL,
		SVC1_IDX_PWM_FREQ_USER_DESC,
	
		// Saves total number of enumeration (SDK line)
    CUSTS1_IDX_NB
};

/// @} USER_CONFIG

#endif // _USER_CUSTS1_DEF_H_
