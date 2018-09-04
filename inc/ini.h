/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 1998-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *
 *******************************************************************************/

#ifndef INI_H
#define INI_H

#undef  Ex
#ifdef INI_C
	#define Ex
#else
	#define Ex extern
#endif


// The Data Encryption Key (DEK) is either 16 or 32 bytes (128- or 256-bit)
#define MAX_KEY_SIZE			32  // bytes

// Size of passwords when encryption is disabled (cleartext mode)
// This must be less than or equal to the max. password and key lengths.
#define CIPHER_NONE_PW_SIZE		32  // bytes
#define MAX_PASSWORD_SIZE		32  // bytes

// Size of the Firmware Private area (aka the Init data area)
//#define FW_PRIVATE_AREA_SIZE	4096	// 2 MiB

// FW private area uses page 0 in log address 0x80 to 0x9F
#define VCD_CONTENT_AREA_LOG_ADDR		0xCF
#define FW_PRIVATE_AREA_BASE_LOG_ADDR	0x80

#define INI_VCD_SIZE			61440		// 30MB for all product
#define INI_PRIVATE_SIZE		64

// LBA alignment of the pages (blocks) in the FW Private area.
// This is a shift count, so value of 3 means alignment is 8 blocks (4K)
// 8 block alignment is useful with HDDs that have 4K physical sectors.
#define FW_PRIVATE_ALIGNMENT_LOG2	3

// Magic numbers (signatures)
#define INI_DATA_SIG					0x57440114
#define PWD_PROTECT_SIG				0x35BA5D27
#define WDC_UNIT_PARAMS_SIG				0x57440348
#define WDC_FACTORY_PARA_SIG			0x57440122
#define WDC_VID_PRODUCT_INFO_SIG		0x57441120
#define WDC_QUICKENUM_DATA_SIG		0x57441121
#ifdef INITIO_STANDARD_FW
#define WDC_RAID_PARAM_SIG			0x21494E49
#define WDC_VITAL_PARAM_SIG			0x20494E49
#define WDC_OTHER_PARAM_SIG			0x22494E49
#else
#define WDC_RAID_PARAM_SIG			0x30505249
#define WDC_VITAL_PARAM_SIG			0x30505649
#define WDC_OTHER_PARAM_SIG			0x30504F49
#endif
#define WDC_IDEK_SIG					0x4B454449
#define CHIPS_SIGNATURE				0x25c93610
#define WDC_FW_SIGNATURE				0x03030505
// The Init and password-protected data are saved together in one sector.
// The Init data is at the beginning of the sector (offset 0) and is not
// encrypted; the password-protected data is is at  PWD_PROTECT_OFFSET
// and is encrypted by either the user's password or the Default Password.
//
#define PWD_PROTECT_OFFSET	200
#define MAX_SATA_CH_NUM		2

#define DISABLE_U1U2()	(nvm_quick_enum.dev_config & DEVCFG_DISABLE_U1U2)

typedef struct _METADATA_HEADER
{
	u32 	signature;
	u32 	payload_len;
	u16	payload_crc;
	u16	header_crc;
}METADATA_HEADER;

typedef struct _DISK_INFO
{
	u8 	medium_rotation_rate;
	u8	serialNum[20];	
	u8	user_bs_log2m9;
	u8	reserved[2];
	u64	numOfSectors; 		
}DISK_INFO;

typedef struct _QUICK_ENUM
{
	METADATA_HEADER header;
	u8  	dev_config; // dev configuration	
	u8 	numOfDiskLUNs;
	u8	slowEnumOnce;
	u8	reserved;
	DISK_INFO	disk0Info;
	DISK_INFO	disk1Info;
}QUICK_ENUM, *PQUICK_ENUM;




typedef struct _RAID_DISK_INFO
{
	u8 disk_bus_type;	// 32	RAID DISK0 HDD'S BUS TYPE = 0x61 (i.e. lower case 'a') 
	u8 disk_model_num[40]; // 33:72		RAID DISK0 HDD'S MODEL NUMBER (40 bytes)
	u8 disk_sn[20];			// 73:92		RAID DISK0 HDD'S SERIAL NUMBER (20 bytes)
	u8 NT_disk_id;			// 93		NULL TERMINATOR FOR RAID DISK0 ID STRING = 0X00 
	u8 disk_status;			// 94
	u8 reserved;			// 95
	
	u64 usableCapcity;		//0x96:103
	u16  blockLength;		//104:105
	u16 mediumRotationRate;	//106:107
	u8 reserved_1;			//108
	u8 nominalFormFactor;		//109
	u8 reserved_2[2];		//110-111
}RAID_DISK_INFO;

typedef struct _RAID_PARAMS
{
	// header
	METADATA_HEADER header;
	
	u32 vcd_size;		// 12:15	(MSB) VIRTUAL CD SIZE (in 512 byte sectors)
	u32 rserved_16;		// 16:19	Reserved (4 bytes for VCD expansion)
	u8 config_VID[2];	// 20:21	(MSB) CONFIGURED BY VID 				
	u8 config_PID[2];	// 22:23	(MSB) CONFIGURED BY PID 
	u32 raidGenNum;		// 24:27	(MSB) RAID GENERATION NUMBER
	u8 array_mode;		// 28	RAID MODE
	u8 stripe_size;		// 29	RAID STRIPE SIZE EXPONENT 
	u8 disk_num;		// 30	RAID DISK NUMBER
	u8 reserved_31;		// 31	RESERVED

	RAID_DISK_INFO disk[MAX_SATA_CH_NUM];
	u64	array_size;			// 160:167	(MSB) ARRAY SIZE (8 bytes)
}RAID_PARMS, *PRAID_PARMS;


// it shall be saved in each HDD, and flash
// saved address in HDD: page addr 0x01, log addr 0x80 & 0x9F
// saved address in flash: 0x1000
typedef struct _VITAL_PARAM
{
	METADATA_HEADER header;
	u8	user_bs_log2m9;
	u8	cipherID;
	u8	reserved[2];
	u32 	DEK_signature;
	u16 	DEK_CRC;
	u8	reserved2[2];
	// DEK[0/1]
	u8	random1[4];
	u8	DEK_partA[12];
	u8	random2[4];
	u8	DEK_partB[12];
	u8	random3[4];
	u8	DEK_partC[12];
	u8	random4[4];
	u8	DEK_partD[12];
	u8	random5[4];
	u8	DEK_partE[12];
	u8	random6[4];
	u8	DEK_partF[4];
	u64	lun_capacity;
}VITAL_PARMS, *PVITAL_PARMS;

typedef struct _PROD_INFO
{
	METADATA_HEADER header;
	u8 	new_VID[2];
	u8 	new_PID[2];	
	u16	wwn_actions;
	u8	disk_wwn[8];
	u8	disk1_wwn[8];
	u8	vcd_wwn[8];	
	u8	ses_wwn[8];
	u8	container_ID[16];
	u8	iSerial[20];
	u8	factory_freezed;
	u8	reserved[3];	
}PROD_INFO, *PPROD_INFO;

// shall be saved in each HDD, and only saved in HDD
// Other Parms
typedef struct _OTHER_PARMS
{
	METADATA_HEADER header;
	u64	raid_rebuildCheckPoint;
	u8	vcd_media;	// only bit1
	u8	reserved[3];
}OTHER_PARMS;

// Password-protected data.
// The most important protected data is the Data Encryption Key (DEK)
//
typedef struct _CIPHER_DEK_INFO
{
	u8  	DEK0[MAX_KEY_SIZE]; // it can be used to save the user password as well, in the case that the Cipher ID is NONE
	u8  	DEK1[MAX_KEY_SIZE];
}CIPHER_DEK_INFO;

typedef struct _PASSWORD_DISK
{
	u8 user_psw[MAX_PASSWORD_SIZE];
}PASSWORD_DISK;

typedef struct PWD_PROTECT {
	CIPHER_DEK_INFO cipher_dek_disk[2];
	PASSWORD_DISK psw_disk[2];
} PWD_PROTECT;

typedef struct _UNIT_PARMS {
	METADATA_HEADER header;
	mode_page_save_data  mode_save; // save the standby timer and led brightness
} UNIT_PARMS;

#define SIZE_QUICK_ENUM	sizeof(QUICK_ENUM)
#define SIZE_UNIT_PARMS	sizeof(UNIT_PARMS)
#define SIZE_RAID_PARMS	sizeof(RAID_PARMS)
#define SIZE_VITAL_PARMS	sizeof(VITAL_PARMS)
#define SIZE_PROD_INFO		sizeof(PROD_INFO)


Ex QUICK_ENUM nvm_quick_enum;
Ex UNIT_PARMS nvm_unit_parms;
Ex RAID_PARMS nvm_rp[MAX_SATA_CH_NUM];
Ex VITAL_PARMS vital_data[MAX_SATA_CH_NUM];
Ex VITAL_PARMS tmp_vital_data;
Ex OTHER_PARMS other_Params[MAX_SATA_CH_NUM];
Ex OTHER_PARMS tmp_other_Params;

Ex PROD_INFO prodInfo;
Ex PWD_PROTECT PwdData;


Ex u8 is_chip_for_wd;

#define HEADER_LENGTH		10 //used to check the header crc
#define WDEK_CRC_LENGTH	90 //used to check the wDek CRC

Ex u16 wwn_action; 
Ex u8 productInfo_freeze;
enum WWN_actions
{
	WWN_NOT_VALID = 0x00,
	WWN_NO_CHANGE	 = 0x01,
	WWN_IS_ERASED = 0x02,
	WWN_FROM_FLASH = 0x08,
	WWN_NO_CHANGE_ALL_LUNS = 0x1111
};

enum USN_actions
{
	USN_NOT_VALID = 0,
	USN_NO_CHANGE,
	USN_IS_ERASED,
	USN_IS_VALID
};
 // we need very be carefull to write flash size, erase every sector is 4k
 // if erase some of the data ,we must re-write it to flash!!
enum FLASH_MAPPING
{
	WDC_QUICK_ENUM_DATA_ADDR = 0x0,
	WDC_UNIT_PARAMS_ADDR = 0x400,
	WDC_PRODUCT_INFO_ADDR = 0x1000,
	WDC_RAID_PARAM_ADDR	= 0x2000,
	WDC_VITAL_PARAM_ADDR = 0x2200
};//WDC_UNIT_PARAMS_ADDR use which address?

// raid use 168*2 --0x200
// vital use 120*2 --0x100
#define RAID_VITAL_TOTAL_SIZE		0x300

// Location of various data in the firmware-private area.
// Make sure these do not overlap the handy store!
//Log address
/* 							 SMART LOG LAYOUT
____________________________________________________________________________
|Log Address | Page Address 0 | Page Address 1 | Page Address 2| Page Address 3 to 15 |
----------------------------------------------------------------------------
| 	0x00      | Log Directory     |	 Reserved         | Reserved          | Reserved                  |
-----------------------------------------------------------------------------
|0x01 --0x7F |				ATA Defined Logs									|
-----------------------------------------------------------------------------
|0x80 --0x9F |		Host Vendor Specific (more details below)						|
-----------------------------------------------------------------------------
|	0x80	|RAID  Parms 1st | Vital Parms 1st | Other Parms 1st| Reserved			|
-----------------------------------------------------------------------------
|0x81 --0x87 |				Reserved											|
-----------------------------------------------------------------------------
|	0x88	|		Handy Store for first split volume								|
-----------------------------------------------------------------------------
|0x89 --0x90 |		Reserved for Handy Store used by future split volumes			|
----------------------------------------------------------------------------
|0x91 --0x9E |				Reserved											|
----------------------------------------------------------------------------
|	0x9F		|RAID  Parms 2st | Vital Parms 2st | Other Parms 2st| Reserved			|
-----------------------------------------------------------------------------
|0xA0 -0xDF  |				Device vendor Specific								|
-----------------------------------------------------------------------------
|0xE0 --0xE1 |				SCT												|
-----------------------------------------------------------------------------
|0xE2 --0xFF |				Reserved											|
-----------------------------------------------------------------------------
*/
enum {
#ifdef WDC_RAID
	WD_LOG_ADDRESS				= 0x80,
#else
	WD_LOG_ADDRESS				= 31,
#endif	
	INI_DATA_LOG_ADDRESS			= 0x81,
	INI_UNUSE_LOG_ADDRESS		= 0x85,
	INI_UNIT_PARAMS_LOG_ADDR	= 0x9E,
#ifdef WDC_RAID
	WD_BACKUP_LOG_ADDRESS		= 0x9F,
#else
	WD_BACKUP_LOG_ADDRESS		= 63,
#endif
};
//WD Page address
enum {
	WD_RAID_PARAM_PAGE			= 0x00,
	WD_VITAL_PARAM_PAGE			= 0x01,
	WD_OTHER_PARAM_PAGE			= 0x02,
};


#define AES_NONE				0x0
#define AES_ENCRYPT_MODE		0x1 // encrypt or decrypt data w/ AES key
#define AES_INTRINSIC_MODE		0x2
#define AES_PSW_PROTECT_MODE		0x3 // protect AES key by password

// Option flags for  ReadWritePrivatePage()
// ket set flag
#define PRIVATE_KEYSET_NUM			0x01
	#define 	PRIVATE_KEYSET2		0x01
	#define 	PRIVATE_KEYSET1		0x00

#define PRIVATE_ENCRYPT_MASK		0x02  	// Enable encryption when reading/writing private data
	#define	PRIVATE_ENCRYPT_EN	0x02
	#define	PRIVATE_ENCRYPT_DIS	0x00
	
#define PRIVATE_RW_MASK			0x04
	#define PRIVATE_WRITE_DATA	0x00	// Write private data
	#define PRIVATE_READ_DATA		0x04	// Read private data

#define PRIVATE_IN_SATA1			0x08	// read/write in sata1
#ifdef WDC_RAID
#define PRIVATE_LOG_BACKUP			0x10	// Read/write from smart log backup sector
#else
#define PRIVATE_LOG_BACKUP			0x00	// Read/write from HDD LBA backup sector
#endif

// Return codes of various functions.
enum {
	INI_RC_OK = 0,
	INI_RC_IO_ERROR,
	INI_RC_CRC_ERROR,
	INI_RC_DATA_NOT_SET,
	INI_RC_BLANK_DATA,
	INI_RC_WRONG_PASSWORD,
	INI_RC_VBUS_RESET,
	INI_RC_NO_DEV,
	INI_RC_LOCAL_CMD_TIMEOUT = 0xFC
};



Ex u8 metadata_status;
	#define MS_GOOD					0x00
	#define MS_CONFLICT_ARRAYS		0x01
	#define MS_STH_NOT_VALID		0x02
	#define MS_NO_VALID				0x03


Ex u64 fw_private_base;
Ex u64 user_data_area_size[MAX_SATA_CH_NUM];
Ex u64 array_data_area_size;// use this parameter to record the 
Ex u8   current_aes_key;
Ex u8   current_aes_key2; // this is used for JBOD mode second disk channel
Ex u8  user_password[MAX_PASSWORD_SIZE];

Ex u8 need_to_sync_vital;
Ex u8   security_state[2];		// 0: lun0 , 1: lun1
// following the security state
enum SecurityStateBits {
	SS_ENCRYPTION_ENABLED		= 0x01,	// User's data is encrypted.
	SS_USER_SET_PASSWORD		= 0x02,	// User has set a password.
	SS_LOCKED						= 0x04,	// The drive is locked.
	SS_NOT_CONFIGURED			= 0x10,	// Not configured (was No DEK)
};
// copyMaster parameter values for  ini_update_init_data()
enum {
	UPDATE_INDEPENDENTLY		= 0,
	COPY_MASTER_TO_BACKUP		= 1
};

enum {
	SECURITY_USER_PASSWORD = 0,
	SECURITY_MASTER_PASSWORD = 1
};

enum
{
	SYNC_VITAL_DATA_SAME_PRIORITY = 0,
	SYNC_VITAL_DATA_DISK0_HIGH_PRIORITY = 1,
	SYNC_VITAL_DATA_DISK0_LOW_PRIORITY = 2
};

Ex u8 IS_LOCKED(u8 slot_num);
Ex u8 IS_LOCKED_OR_UNCONFIGED(u8 slot_num);
Ex u8 IS_USER_PASSWORD_SET(u8 slot_num);

Ex u8 sync_vital_data(u8 tar_slot);
Ex void check_disks_vital_data(u8 priority);

Ex void init_reset_default_nvm_unit_parms(void);
Ex void init_security_Params(void);
Ex void init_update_params(void);
Ex void update_user_data_area_size(void);
Ex void ini_fill_default_password(u8 slot_num,u8   *pw);
//extern void ini_init(void);
Ex void ini_reset_defaults(u8 target);
Ex u8   ini_save_passworded_data(u8 target);
Ex u8	CheckPassword(u8 slot_num);
Ex void ClearUserPassword(u8 slot_num);
Ex void SetUserPassword(const u8 *pwd, u8 len,u8 slot_num);
Ex void ClearPwdData(u8 slot_num);
Ex u32 generate_RandNum_DW(void);
Ex void seal_drive(void);
Ex u8 Read_Disk_MetaData(u8 slotNum,u8 page_addr, u8 rlog_addr);
Ex u8 Write_Disk_MetaData(u8 slotNum,u8 page_addr,u8 save_option);
		#define FIRST_BACKUP	0x01
		#define SECOND_BACKUP	0x02
Ex void switch_to_cryptMode(u8 KeySet);
Ex void enable_aes_key(u8 *key, u8 KeySet);
Ex void fill_vitalParam(u8 slot_num);
Ex void switch_to_pswProtMode(PCDB_CTXT pCtxt, u8 options);
Ex void initio_vendor_debug_clean_matadata();
#endif  // INI_H
