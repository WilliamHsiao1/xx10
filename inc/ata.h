/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *
 *
 *****************************************************************************
 * 
 * 3610		2010/04/09	Odin		Initial version
 * 3610		2010/04/27	Odin		USB2.0 BOT Debugging
 *
 *****************************************************************************/

#ifndef ATA_H
#define ATA_H

#undef  Ex
#ifdef ATA_C
	#define Ex
#else
	#define Ex extern
#endif

//Ex u8 ATA_WrCache;
Ex u8 extended_descriptor;
Ex u8 ata_passth_prepared;


extern u8 ata_ReadID(u8 sata_ch_flag);
	#define SATA_CH0		0x01
	#define SATA_CH1		0x02	
	#define SATA_TWO_CHs	0x03
#define ATA_ID_DATA_LEN		512
extern u8 ata_init(PSATA_OBJ  pSob);
extern u8 read_QueuedErrLog(u8 sata_ch_flag);

//extern u8 init_HDDs(void);
extern u8 ataInit_quick_enum();
	#define QUICK_ENUM_STATUS_SUCCESS				0x00
	#define QUICK_ENUM_STATUS_DOUBLE_ENUM			0x01
	#define QUICK_ENUM_STATUS_HDD0_INIT_FAIL		0x02
	#define QUICK_ENUM_STATUS_HDD1_INIT_FAIL		0x03
	#define QUICK_ENUM_STATUS_ATA_TIMEOUT			0x04

extern u32 ata_SetDMAXferMode(PSATA_OBJ  pSob, u32 mode);

extern u32 ata_ExecUSBNoDataCmd(PSATA_OBJ  pSob, PCDB_CTXT pCtxt, u8 command);
extern u32 ata_ExecNoDataCmd(PSATA_OBJ  pSob, u32 cmdAndFeature, u32 fis_lba, u8 timeout_tick);
extern u32 ata_ExecNoDataCmd_TwoHDDs(u32 cmdAndFeature, u32 fis_lba, u8 timeout_tick);

#ifdef WDC
extern u8 ata_startup_hdd(u8 sync_flag, u8 sata_ch_flag);
extern void ata_shutdown_hdd(u8 power_off, u8 sata_ch_flag);
extern void ata_hdd_standby(void);
extern void ata_load_fis_from_rw16Cdb(PCDB_CTXT pCtxt, u8 xfr_type);
#define DMA_TYPE    		0
#define FPDMA_TYPE    	1
#define LOG_TYPE		2

extern u8 ata_checksum8(const u8 *buf);
#endif
extern u8 ata_enable_wr_cache(PSATA_OBJ  pSob);
extern u8 ata_en_smart_operation(PSATA_OBJ pSob);
extern u8 ata_en_AAM(PSATA_OBJ pSob);


#define MIN_HDD_SIZE			2097152			// 1 GiB


///////////////////////////////////////////////////////////////
// ATA Shadow Registers
///////////////////////////////////////////////////////////////
// Bit definition of ATA STATUS Register  
#define   ATA_STATUS_BSY  0x80
#define   ATA_STATUS_DRDY 0x40
#define   ATA_STATUS_DSC  0x10
#define   ATA_STATUS_DRQ  0x08
#define   ATA_STATUS_CORR 0x04
#define   ATA_STATUS_IDX  0x02
#define   ATA_STATUS_CHECK  0x01

// Bit definition of ATA ERROR Register
#define   ATA_ERR_ICRC    (1 << 7)
#define   ATA_ERR_UNC     (1 << 6)
#define   ATA_ERR_MC      (1 << 5)
#define   ATA_ERR_IDNF    (1 << 4)
#define   ATA_ERR_MCR     (1 << 3)
#define   ATA_ERR_ABRT    (1 << 2)
//#define   ATA_ERR_NM      (1 << 1)
//#define   ATA_ERR_MED     (1 << 0)

//Bit definition of ATA FEATURES Register
#define   FEATURES_DMA        0x01

//definition of ATA Device Register
#define MASTER              0xA0
//	#define   DRV_HEAD_DFT        0xA0


// Bit definition of ATA DEVICE Control Register
#define   DEVICE_CRTL_DFT     0x08
#define   DEVICE_CRTL_SRST    0x04
#define   DEVICE_CRTL_IENT    0x02


#define NO_ERROR					0x00
#define ERROR_ILL_OP				0x01
#define ERROR_ILL_CDB				0x02
#define ERROR_ILL_PARAM			0x03
#define ERROR_UA_NOT_READY		0x04
#define ERROR_UA_BECOMING_READY 	0x05
#define ERROR_UA_NO_MEDIA			0x06
#define ERROR_UA_NOT_READY_INIT	0x07
#define LBA_OUT_RANGE				0x08
#define ERROR_MEDIA_CHANGED		0x09
#define ERROR_WRITE_PROTECT		0x0A
//#define ERROR_COMMAND_ABORTED	0x0A
#define LOGICAL_UNIT_NOT_CONFIGURED		0x0B
#define ERROR_ROUNDED_PARAMETER 	0x0C
#define ERROR_PARAM_LIST_LENGTH	0x0D
#define ERROR_ASSERT_FAILED		0x0E
#define ERROR_HW_FAIL_INTERNAL	0x0F
#define ERROR_UNSUPPORTED_SES_FUNCTION	0x10
#define ERROR_UNLOCK_EXCEED				0x11
#define ERROR_INCORRECT_SECURITY_STATE	0x12
#define ERROR_AUTHENTICATION_FAILED		0x13
#define ERROR_ATA_MEMORIZED				0x14
#define ERROR_LBA_OUT_RANGE				0x15
#define ERROR_MEDIUM_LOCKED				0x16
#define ERROR_ILLEGAL_TRACK_MODE			0x17
#define ERROR_NONE							0x18
#define ERROR_DIAG_FAIL_COMPONENT		0x19
#define ERROR_INVALID_FLASH				0x20
#define ERROR_INVALID_CHIP_VER				0x21
#define ERROR_HDD_FAIL_INITIALIZATION		0x22
#define ERROR_PERMANENT_WRITE_PROTECT	0x23
#define ERROR_INVALID_PCBA					0x24
#define ERROR_INVALID_LUN					0x25
#define ERROR_COMMAND_SEQUENCE			0x26
#define ERROR_DATA_PHASE_ABORTED			0x34
#define ATA_PASSTHR_INFOR_AVAILABLE		0x35


#define WRITE_CACHE_DISABLE	0x00        // write cache support,but disable
#define WRITE_CACHE_ENABLE	0x01	 // write cache support,and enable
#define WRITE_CACHE_UNSUPPORT	0xFF // write cache unsupport

#define AAM_STATUS_UNSUPPORT	0x00 // AAM unsupport
#define AAM_STATUS_DISABLE	0x01        // write cache support,but disable
#define AAM_STATUS_ENABLE	0x02	 // AAM support,and enable
#define AAM_ISMAX_PERFORMANCE	0x04	 // AAM support,and enable

#define SMART_STATUS_DISABLE	0x00        // SMART support,but disable
#define SMART_STATUS_ENABLE	0x01	 // SMART support,and enable
#define SMART_STATUS_UNSUPPORT	0xFF // SMART unsupport
//struct DEV_INFO
//{
//	u8 class;
//	u8 mediaType;
//	u8 deviceType;
//	u8 reserved;
//};

//struct X_DEV_INFO
//{
//	u32 sectorLBA_high;
//	u32 sectorLBA;
//	u16 ioMode;
//	u8 modelText[40];
//};

typedef          /* Data type with only possible values - ATA-6 command codes */
enum {
    	ATA6_CHECK_POWER_MODE			= 0xE5 ,
	DOOR_LOCK							= 0xDE ,  DOOR_UNLOCK                = 0xDF ,
	DOWNLOAD_MICROCODE				= 0x92 ,
    	ATA6_EXECUTE_DEVICE_DIAGNOSTICS	= 0x90 ,
	ATA6_FLUSH_CACHE					= 0xE7 ,
	ATA6_FLUSH_CACHE_EXT				= 0xEA ,
	ATA6_GET_MEDIA_STATUS			= 0xDA ,
	ATA6_IDENTIFY_DEVICE				= 0xEC ,
	ATA6_IDENTIFY_PACKET_DEVICE		= 0xA1 ,
	ATA6_IDLE							= 0xE3,
	ATA6_IDLE_IMMEDIATE             		= 0xE1 ,
	ATA6_MEDIA_EJECT					= 0xED ,
	ATA6_NOP							= 0,
	ATA6_PACKET						= 0xA0 ,
	ATA6_READ_BUFFER					= 0xE4 ,
	ATA6_READ_DMA					= 0xC8 ,
	ATA6_READ_DMA_EXT				= 0x25 ,
	READ_MULTIPLE						= 0xC4 ,
	ATA6_READ_SECTORS				= 0x20 ,
	ATA6_READ_VERIFY_SECTORS			= 0x40 ,
	ATA6_READ_VERIFY_SECTORS_EXT	= 0x42 ,
	ATA6_READ_LOG_EXT				= 0x2F,
	ATA6_READ_LOG_DMA_EXT			= 0x47,
    	SECURITY_SET_PASSWORD			= 0xF1,   SECURITY_DISABLE_PASSWORD  = 0xF6 ,
    	SECURITY_ERAZE_PREPARE			= 0xF3 ,  SECURITY_ERAZE_UNIT        = 0xF4 ,
    	SECURITY_FREEZE_LOCK				= 0xF5 ,  SECURITY_UNLOCK            = 0xF2 ,
 	ATA6_SEEK							= 0x70 ,
	ATA6_SET_MULTIPLE_MODE			= 0xC6 ,
	ATA6_SET_FEATURES				= 0xEF ,
	ATA6_SLEEP							= 0xE6 ,
	ATA6_SMART						= 0xB0 ,
	ATA6_STANDBY						= 0xE2 ,
	ATA6_STANDBY_IMMEDIATE			= 0xE0 ,
	ATA6_WRITE_BUFFER				= 0xE8 ,
	ATA6_WRITE_DMA					= 0xCA ,
	ATA6_WRITE_DMA_EXT				= 0x35 ,
	ATA6_WRITE_MULTIPLE				= 0xC5 ,
	ATA6_WRITE_SECTORS				= 0x30 ,
	ATA6_WRITE_LOG_EXT				= 0x3F ,
	ATA6_WRITE_LOG_DMA_EXT			= 0x57 ,
	ATA6_RD_FPDMA_QUEUED                    = 0x60 ,
	ATA6_WR_FPDMA_QUEUED                   = 0x61 ,
    	ATA6_WRITE_SECTORS_EXT			= 0x34
}  ATA6_OpCode ; /*ATA-6 command codes - end */
	
#ifdef WDC
// Power Off parameter values for  ata_shutdown_hdd()
enum {
	KEEP_POWER_ON		= 0,
	TURN_POWER_OFF		= 1
};
// SMART Feature field values
//
enum SmartFeatureCodes {
	SMART_READ_DATA				= 0xD0,
	SMART_EXEC_OFF_LINE_IM		= 0xD4,
	SMART_READ_LOG				= 0xD5,
	SMART_WRITE_LOG				= 0xD6,
	SMART_ENABLE_OPERATIONS		= 0xD8,
	SMART_DISABLE_OPERATIONS	= 0xD9,
	SMART_RETURN_STATUS			= 0xDA
};

enum SmartSignature {
	SMART_SIGNATURE_LOW			= 0x4F,
	SMART_SIGNATURE_HIGH		= 0xC2
};

enum AAMFeature_subCmds{
	ENABLE_AAM_FEATURE = 0x42,
	DSIABLE_AAM_FEATURE = 0xC2
};

enum AAMLevels
{
	AAM_MIN_ACOSTIC_EMANATION = 0x80,
	AAM_MAX_PERFORMANCE = 0xFE	
};


// IDENTIFY DEVICE data field offset and lengths.
// These are 16-bit word offsets just like in the ATA specification.
//
enum AtaIdDataFields {
	ATA_ID_SPECIFIC_CONFIG		=   2,	// See enum AtaSpecificConfiguration
	ATA_ID_SERIAL_NUMBER		=  10,
	ATA_ID_SERIAL_NUMBER_SIZEW	=  10,
	ATA_ID_FIRMWARE_REV			=  23,
	ATA_ID_FIRMWARE_REV_SIZEW	=   4,
	ATA_ID_MODEL_NUMBER			=  27,
	ATA_ID_MODEL_NUMBER_SIZEW	=  20,
	ATA_ID_SECTOR_SIZES			= 106,	// Physical and logical sector sizes
	ATA_ID_BLOCK_ALIGNMENT		= 209,
	ATA_ID_INTEGRITY_WORD		= 255	// Checksum and validity indicator
};

enum FeatureCodes {
	FEATURE_ENABLE_AAM			= 0x42,	// Enable Auto Acoustic Management
	FEATURE_DISABLE_AAM			= 0xC2,
	FEATURE_ENABLE_WRITE_CACHE	= 0x02,
	FEATURE_DISABLE_WRITE_CACHE	= 0x82,
	FEATURE_ENABLE_PUIS			= 0x06,	// Enable Power Up In Standby
	FEATURE_DISABLE_PUIS		= 0x86,
	FEATURE_PUIS_SPINUP			= 0x07,
	FEATURE_ENABLE_READ_AHEAD	= 0xAA,
	FEATURE_DISABLE_READ_AHEAD	= 0x55,
	FEATURE_SET_TRANSFER_MODE	= 0x03
};

enum AtaSpecificConfiguration {
	CFG_NEED_SPINUP__ID_INCOMPLETE	= 0x37C8,
	CFG_NEED_SPINUP__ID_COMPLETE	= 0x738C,
	CFG_NO_SPINUP__ID_INCOMPLETE	= 0x8C73,
	CFG_NO_SPINUP__ID_COMPLETE		= 0xC837
};
#endif

#endif
