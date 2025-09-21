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
#define DEF_SVC1_UUID_128 {0xee,0x8f,0x7a,0x37,0xcc,0x56,0x5d,0xaa,0xed,0x40,0xa0,0xf0,0xc2,0x94,0xe0,0xd6}

/*
----------------------------------
- Characteristics
----------------------------------
*/

// Each one needs UUID, byte length, and user description

// Define sensor voltage
#define DEF_SVC1_SENSOR_VOLTAGE_UUID_128 {0x35,0x8c,0x68,0x30,0xbb,0x00,0xec,0x89,0x46,0x4b,0x67,0xf2,0xc4,0xa7,0xf4,0xfe}
#define DEF_SVC1_SENSOR_VOLTAGE_CHAR_LEN 2 // 2 bytes for 10-bit ADC
#define DEF_SVC1_SENSOR_VOLTAGE_USER_DESC "Sensor Voltage (little-endian bytes)"

// Define PWM freq
#define DEF_SVC1_PWM_FREQ_UUID_128 {0xf0,0x5d,0x4d,0x6f,0xb6,0x6a,0x9b,0x98,0x33,0x42,0xb3,0xf2,0xf6,0x0f,0x2f,0xc8}
#define DEF_SVC1_PWM_FREQ_CHAR_LEN 4
#define DEF_SVC1_PWM_FREQ_USER_DESC "Timer2 PWM Frequency Config"

// Define PWM dc and offset
#define DEF_SVC1_PWM_DC_AND_OFFSET_UUID_128 {0x72,0x50,0x8a,0x2d,0xca,0xcf,0x9c,0xb6,0x51,0x4f,0x07,0xbc,0xf6,0x0d,0xff,0x3a}
#define DEF_SVC1_PWM_DC_AND_OFFSET_CHAR_LEN 4
#define DEF_SVC1_PWM_DC_AND_OFFSET_USER_DESC "Timer2 PWM2 and PWM3 Duty Cycles and Offsets"

// Define PWM state
#define DEF_SVC1_PWM_STATE_UUID_128 {0x42,0x2d,0x1b,0x1c,0x46,0x02,0x26,0xb4,0xd0,0x4a,0x1e,0xd5,0x25,0xf9,0xc3,0x2d}
#define DEF_SVC1_PWM_STATE_CHAR_LEN 1
#define DEF_SVC1_PWM_STATE_USER_DESC "Timer2 PWM State On/Off"

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
	
		SVC1_IDX_PWM_DC_AND_OFFSET_CHAR,
		SVC1_IDX_PWM_DC_AND_OFFSET_VAL,
		SVC1_IDX_PWM_DC_AND_OFFSET_USER_DESC,
	
		SVC1_IDX_PWM_STATE_CHAR,
		SVC1_IDX_PWM_STATE_VAL,
		SVC1_IDX_PWM_STATE_USER_DESC,
	
		// Saves total number of enumeration (SDK line)
    CUSTS1_IDX_NB
};

/// @} USER_CONFIG

#endif // _USER_CUSTS1_DEF_H_
