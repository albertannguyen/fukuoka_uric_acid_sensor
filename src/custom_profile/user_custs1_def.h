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

// Ignore red build error here
#include "attm_db_128.h"

/*
 ****************************************************************************************
 * DEFINES
 ****************************************************************************************
 */
 
// Define service 1 UUID (custom service for ADC data)
#define DEF_SVC1_UUID_128 {0x72, 0x0E, 0x9F, 0xE8, 0xDE, 0xEC, 0x12, 0x4D, 0x99, 0xA5, 0xED, 0x64, 0xF3, 0xC4, 0x21, 0xB4}

// Define UUID for sensor voltage characteristic
#define DEF_SVC1_SENSOR_VOLTAGE_UUID_128 {0x04, 0x03, 0x08, 0x32, 0x72, 0x9D, 0x42, 0x93, 0x61, 0x4D, 0x48, 0xF6, 0x92, 0x2C, 0xD2, 0x41}

// Define characteristic length = 2 bytes (16 bits), which stores 10-bit ADC result
#define DEF_SVC1_SENSOR_VOLTAGE_CHAR_LEN 2

// Defines user-friendly description as a string
#define DEF_SVC1_SENSOR_VOLTAGE_USER_DESC   "Sensor voltage (mV)"

// WIP: Define standard bluetooth battery service to report battery voltage level to app below

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
		
		// Saves total number of enumeration (SDK line)
    CUSTS1_IDX_NB
};

/// @} USER_CONFIG

#endif // _USER_CUSTS1_DEF_H_
