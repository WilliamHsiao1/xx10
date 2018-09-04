/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************
 * 
 * 3610		2012/01/20	Bill		Initial version
 *
 *****************************************************************************/

#ifndef RAID_H
#define RAID_H

//----------------------
#undef  Ex
#ifdef RAID_C
	#define Ex
#else
	#define Ex extern
#endif

//#ifdef HDR2526_GPIO_HIGH_PRIORITY
Ex u8 raid_mode;		// indicate real raid mode, just for read
//#endif
Ex u8 arrayMode;		// indicate work mode
	#define RAID0_STRIPING_DEV		0x00
	#define RAID1_MIRROR			0x01
#ifdef SUPPORT_SPAN
	#define SPANNING				0x02
#endif	
	#define JBOD						0x0F
	#define NOT_CONFIGURED			0x10

#ifdef HARDWARE_RAID	//GPIO5:GPIO6
#ifdef DATABYTE_RAID
#define GPIO_RAID0	0x01	// DATABYTE_RAID_MAPPING
#define GPIO_RAID1	0x03	// DATABYTE_RAID_MAPPING
#define GPIO_SPAN	0x02	// DATABYTE_RAID_MAPPING
#define GPIO_JBOD	0x00	// DATABYTE_RAID_MAPPING
#else
#define GPIO_RAID0	0x03
#define GPIO_RAID1	0x02
#define GPIO_SPAN	0x00
#define GPIO_JBOD	0x01
#endif
#endif


#define SATA_NO_DEV			0x00
#define SATA_DEV_READY			0x01
#define SATA_DEV_UNKNOWN			0xFF

//Ex u8 sata_slot_is_swapped;



//Please note that WD currently does not want to support the auto-rebuild feature, 
//but we may want it in the future.  
//The enable_auto_rebuild flag shall be set to the constant value of false
Ex u8 enable_auto_rebuild;
Ex u8 id_compare_value;
Ex u8 num_arrays;
Ex u8 num_DiskLuns_created;
Ex u64 rebuildLBA; // it's the watermark
Ex u32 rebuildLen; // it's the rebuild sector count in every slice
Ex u8 instant_rebuild_flag;  // to indicate it's a instant rebuild
Ex u8 write_fail_dev;
	#define FAIL_SLOT0	0x01
	#define FAIL_SLOT1  	0x02


//RAID Enumeration Global Variables.
Ex RAID_PARMS ram_rp[MAX_SATA_CH_NUM];
Ex RAID_PARMS tmp_rp;
Ex u8 cur_status[MAX_SATA_CH_NUM];
#define SLOT0	0
#define SLOT1	1
//#define SUPPORTED_LUN		0x04
Ex u8 report_status[MAX_SATA_CH_NUM][MAX_SATA_CH_NUM];
Ex u8 report_slot[MAX_SATA_CH_NUM];
Ex u8 need_to_store_slot[MAX_SATA_CH_NUM];
Ex u8 slot_to_nvm[MAX_SATA_CH_NUM];
Ex u8 array_status_queue[MAX_SATA_CH_NUM];

#define COPY_INVALID			0	// copy invalid
#define COPY1_DISK_GOOD		1	// 1st Copy from HDD  is good
#define COPY2_DISK_GOOD		2	// 2nd Copy from HDD  is good
#define COPY3_NVM_GOOD		3	// 3rd Copy from NVM  is good



enum ArrayStatus {
	AS_GOOD = 0x00,
	AS_DEGRADED = 0x01,
	AS_REBUILDING = 0x02,
	AS_REBUILD_FAILED = 0x03,
	AS_DATA_LOST = 0x04,
	AS_NOT_CONFIGURED = 0x10,
	AS_BROKEN = 0x11
};


enum SlotStatus {
	SS_GOOD = 0x00,
	SS_REBUILDING,
	SS_EMPTY,
	SS_MISSING,
	SS_BLANK,
	SS_ID_MISMATCH,
	SS_HARD_REJECTED,
	SS_SOFT_REJECTED,
	SS_FAILED,
	SS_SPARE,
	SS_NOT_PARTICIPATING
};	

enum sourceIDs
{
	SOURCE_ID_SLOT0 = 0,
	SOURCE_ID_SLOT1 = 1,
	SOURCE_ID_PHYSICAL = 0xFF
};

typedef struct _VOL_DESC
{
	u8 array_status_cmd;		// 0
	u8 arrayMode;				// 1
	u8 stripe_size;				// 2
	u8 rebuild_percentage_complete;	// 3
	u64 start_lba;				// 4~11
	u64 numOfBlocks;			// 12~19
	u8 slot0Status;			// 20
	u8 slot0OrgOrder;			// 21
	u8 slot1Status;			// 22
	u8 slot1OrgOrder;		// 23
}VOL_DESC, *PVOL_DESC;

typedef struct _RAID_STATUS_CONFIG_PARAM
{
	u8 signature;				// 0
	u8 reserved[2];				// 1~2
	u8 metadataStatus;			// 3
	u8 reserved2;				// 4
	u8 numOfSlot;				// 5
	u8 maxVolPerHDD;			// 6
	u8 numOfVolSetDesc;			// 7
	VOL_DESC volDesc[MAX_SATA_CH_NUM];	// 8~55
	//vol_descriptor vol1_Desc;	// 32~55
}RAID_STATUS_CONFIG_PARAM;

//Ex u8 array_cmd;
#define AC_NO_OP			0x80
#define AC_CREATE			0x81
#define AC_REBUILD			0x82
	


// use to setup dbuf connection, disconnection and reset
Ex u8 raid_mirror_operation;
	#define RAID1_REBUILD0_1	BIT_1 // SATA0 -> SATA1 copy
	#define RAID1_REBUILD1_0	BIT_2 // SATA1 -> SATA0 Copy
	#define RAID1_VERIFY	BIT_3

// indicate to do the rebuild/verify at spare time or offline	
Ex u8 raid_mirror_status;
	#define RAID1_STATUS_REBUILD 	0x01
	#define RAID1_STATUS_VERIFY	0x02

Ex u8 raid_rb_state;
enum
{
	RAID_RB_IDLE = 0, 	// in IDLE state, the rebuild can execute the next slice
	RAID_RB_IN_PROCESS, // the instant rebuild/rebuild will move to this state, and it will wait for the rebuild done or rebuild error
	RAID_RB_DONE_UPDATE, // if there's error happens, will determine the error case to see if there's action need to do or wait for complete
};

enum 
{
	RB_RC_OK = 0,		// rebuild is not done yet
	RB_RC_DONE_ERR,	// rebuild is done with error case
	RB_RC_DONE			// rebuild is done with good status
};

Ex PCDB_CTXT pCtxt_INST_RB;
	
	
Ex u8 raid_rebuild_verify_status;
	//#define RRVS_IDLE					BIT_0		// obsolete
	#define RRVS_IN_PROCESS			BIT_1
	// done & faill just occured in process state, so when process is finished, remembe to clear them.
	#define RRVS_DONE					(BIT_2|BIT_3)
	#define RRVS_D0_DONE				BIT_2
	#define RRVS_D1_DONE				BIT_3
	#define RRVS_IO_DONE				(BIT_4|BIT_5)
	#define RRVS_D0_IO_DONE			BIT_4
	#define RRVS_D1_IO_DONE			BIT_5
	#define RRVS_ONLINE					BIT_7

Ex u8 raid1_active_timer;
	#define RAID_REBUILD_VERIFY_ONLINE_ACTIVE_TIMER	50 // 5secs

Ex u32 raid_xfer_length;

Ex u64 verify_watermark;
//Ex u64 raid_mirror_size;
Ex u8 raid1_use_slot;
Ex void update_raid1_read_use_slot(void);

Ex void raid_init(void);
Ex u32 raid_load_FIS_execute(PCDB_CTXT pCtxt, u8 control);
Ex u32 raid_exec_ctxt(PCDB_CTXT pCtxt);
Ex u8 attribute_raid_reg_setup_done(u8 dir);
	#define STATUS_TWO_CHANNELS_SET_UP	0x03
	#define STATUS_CHANNEL0_SET_UP		0x01
	#define STATUS_CHANNEL1_SET_UP		0x02
	#define STATUS_ERROR					0xFF
Ex void raid_done_isr();
Ex void raid_rebuild(void);
Ex void raid_verify(void);
Ex u8 instant_rebuild(PCDB_CTXT pCtxt);
Ex void ini_init(void);
Ex void raid_wdc_maintenance_in_cmd(PCDB_CTXT pCtxt);
Ex void raid_wdc_maintenance_out_cmd(PCDB_CTXT pCtxt);
Ex void update_array_data_area_size(void);

Ex void raid_enumeration(u8 load_flag);
Ex u8 raid1_error_detection(PCDB_CTXT pCtxt);

#ifdef HARDWARE_RAID
#ifdef HDR2526_GPIO_HIGH_PRIORITY
Ex u8 hard_raid_mapping(void);
#endif
Ex u8 hw_raid_enum_check(void);
Ex u8 hw_raid_config(u8 am_cond);
#ifdef SUPPORT_HR_HOT_PLUG
//Ex u8 sata_unrdy_cnt;
//#define SATA_UNRDY_DELAY		11
Ex u8 chk_sata_active_status(void);
Ex void sata_status_chk_on_HR(void);
#endif
#endif
#ifdef HDD_CLONE
#define CS_CLONING					0x01
#define CS_CLONE_LOST_DEV			0xFE
#define CS_CLONE_FAIL				0xFF
#define CS_CLONE_DONE_HOLD		0x03
Ex u8 clone_state;
Ex void clone_init(void);
#endif

#define DIR_IN 		BIT_7
#define SATA0_CH  	BIT_6
Ex u8 raid_xfer_status;
	#define RAID_XFER_IN_RPOGRESS			0x01
	#define RAID_XFER_READ_FLAG			0x02
	#define RAID_XFER_WRITE_FLAG			0x04
	#define RAID_XFER_DONE					0x08
	// bit[5:4] are used in NCQ command only, because the device service requires the Read NCQ ERROR Log to clean up the drive
	#define RAID_XFER_SATA0_NCQ_ERR		0x10 
	#define RAID_XFER_SATA1_NCQ_ERR		0x20
// for be called both in SCSI and INI func
#define TARGET_BOTH_SLOTS		0x03
#define TARGET_SLOT0			0x01
#define TARGET_SLOT1			0x02
#endif
