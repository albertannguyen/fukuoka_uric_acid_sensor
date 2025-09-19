/**
 ****************************************************************************************
 * @file user_custs1_def.c
 * @brief Custom Server 1 (CUSTS1) profile database definitions.
 * @note Albert Nguyen
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @defgroup USER_CONFIG
 * @ingroup USER
 * @brief Custom server 1 (CUSTS1) profile database definitions.
 *
 * @{
 ****************************************************************************************
 */

/*
 ****************************************************************************************
 * INCLUDE FILES
 ****************************************************************************************
 */

#include <stdint.h>
#include "co_utils.h"
#include "prf_types.h"
#include "attm_db_128.h"
#include "user_custs1_def.h"

/*
 ****************************************************************************************
 * LOCAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
 
// Define service 1 UUID
static const att_svc_desc128_t custs1_svc1                      = DEF_SVC1_UUID_128;
 
// Define UUID for sensor voltage characteristic
static const uint8_t SVC1_SENSOR_VOLTAGE_UUID_128[ATT_UUID_128_LEN] = DEF_SVC1_SENSOR_VOLTAGE_UUID_128;

// Define general attribute specifications (SDK code snippet, refer to att.h)
static const uint16_t att_decl_svc       = ATT_DECL_PRIMARY_SERVICE;
static const uint16_t att_decl_char      = ATT_DECL_CHARACTERISTIC;
static const uint16_t att_desc_cfg       = ATT_DESC_CLIENT_CHAR_CFG;
static const uint16_t att_desc_user_desc = ATT_DESC_CHAR_USER_DESCRIPTION;

/*
 ****************************************************************************************
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

// Define array of services from enum database
const uint8_t custs1_services[]  = {SVC1_IDX_SVC, CUSTS1_IDX_NB};

// Define size of the array above (SDK line)
const uint8_t custs1_services_size = ARRAY_LEN(custs1_services) - 1;

// Define total number of services and attributes combined (SDK line)
const uint16_t custs1_att_max_nb = CUSTS1_IDX_NB;

/// Full CUSTS1 Database Description - Used to add attributes into the database (SDK line, refer to attm_db_128.h)
const struct attm_desc_128 custs1_att_db[CUSTS1_IDX_NB] =
{
	// Add service 1 in database
	// Use indexing from enum database in header file
	[SVC1_IDX_SVC] = {
		(uint8_t*)&att_decl_svc,	// Pointer to attribute service declaration and cast it as uint8_t
		ATT_UUID_128_LEN, 			 	// UUID size
		PERM(RD, ENABLE), 			  // Enable permission to read (refer to attm.h)
		sizeof(custs1_svc1), 		  // Attribute max length
		sizeof(custs1_svc1), 		  // Attribute current length
		(uint8_t*)&custs1_svc1 	  // Pointer to value of attribute (UUID) and cast it as uint8_t
	},
	
	// Add sensor voltage characteristic declaration, refer to SDK examples
	[SVC1_IDX_SENSOR_VOLTAGE_CHAR] = {
		(uint8_t*)&att_decl_char,
		ATT_UUID_16_LEN, // refer to att.h
		PERM(RD, ENABLE),
		0,
		0,
		NULL
	},
	
	// Add sensor voltage characteristic value
	[SVC1_IDX_SENSOR_VOLTAGE_VAL] = {
		SVC1_SENSOR_VOLTAGE_UUID_128,
		ATT_UUID_128_LEN,
		PERM(NTF, ENABLE),
		DEF_SVC1_SENSOR_VOLTAGE_CHAR_LEN,
		0,
		NULL
	},
	
	// Add sensor voltage characteristic value configuration descriptor for notifications
	[SVC1_IDX_SENSOR_VOLTAGE_NTF_CFG] = {
		(uint8_t*)&att_desc_cfg,
		ATT_UUID_16_LEN,
		PERM(RD, ENABLE) | PERM(WR, ENABLE) | PERM(WRITE_REQ, ENABLE),
		sizeof(uint16_t),
		0,
		NULL
	},

	// Add sensor voltage characteristic user description
	[SVC1_IDX_SENSOR_VOLTAGE_USER_DESC] = {
		(uint8_t*)&att_desc_user_desc,
		ATT_UUID_16_LEN,
		PERM(RD, ENABLE),
		sizeof(DEF_SVC1_SENSOR_VOLTAGE_USER_DESC) - 1,
		sizeof(DEF_SVC1_SENSOR_VOLTAGE_USER_DESC) - 1,
		(uint8_t *) DEF_SVC1_SENSOR_VOLTAGE_USER_DESC
	}
};

/// @} USER_CONFIG
