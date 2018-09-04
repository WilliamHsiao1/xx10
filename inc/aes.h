/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2008-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/

#ifndef AES_H
#define AES_H

#undef  Ex
#ifdef AES_C
	#define Ex
#else
	#define Ex extern
#endif

// The following enumerations and data structures are defined by the
// Artemis Firmware Functional specification, except where noted.

#define MAX_UNLOCK_ALLOWED	5

// Cipher IDs
enum {
	CIPHER_NONE				= 0x00,
	CIPHER_AES_128_ECB		= 0x10,
	CIPHER_AES_256_ECB		= 0x20,
	CIPHER_AES_128_XTS		= 0x18,
	CIPHER_AES_256_XTS		= 0x28,
	CIPHER_AES_FDE			= 0x30,
	CIPHER_NOT_CONFIGURED	= 0xff		// Not defined by Apollo FW Specification.
};

// Security States enumeration
// Used only by the ENCRYPTION STATUS command
enum {
	ESSS_SECURITY_OFF		= 0,
	ESSS_LOCKED				= 1,
	ESSS_UNLOCKED			= 2,
	ESSS_UNLOCKS_EXCEEDED	= 6,
	ESSS_NOT_CONFIGURED		= 7
};

// Data returned by the ENCRYPTION STATUS command
typedef struct _EncryptionSatusData {

	u8  Signature;
	u8  reserve1[2];
	u8  En_Security_State;
	u8  Current__Cipher_ID;
	u8  reserve2[1];
	u8  Password_length[2];
	u8  Key_Reset_Enabler[4];
	u8  reserve3[3]; 
	u8  Number_Of_Ciphers;
	u8  Cipher[4];
} EncryptionSatusData;

// Valid values for the SECURITY command's Action field.
enum {
	UNLOCK_ENCRYPTION		= 0xE1,
	CHANGE_PASSWORD		= 0xE2,
	RESET_DEK				= 0xE3
	//FORCE_RESET_DEK		= 0xE4
};

Ex u8  unlock_counter[2];
Ex u32 dek_reset_enabler[2];

#define IS_LOCKED(slot_num)				(security_state[slot_num] & SS_LOCKED)

#define IS_USER_PASSWORD_SET(slot_num)	(security_state[slot_num] & SS_USER_SET_PASSWORD)

#define IS_LOCKED_OR_UNCONFIGED(slot_num)	(security_state[slot_num] & (SS_LOCKED | SS_NOT_CONFIGURED))


extern void aes_encryption_status_cmd(PCDB_CTXT pCtxt);
extern void aes_security_cmd(PCDB_CTXT pCtxt);
#endif	//  AES_H

