/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/

#ifndef UAS_H
#define UAS_H

//----------------------
#undef  Ex
#ifdef UAS_C
	#define Ex
#else
	#define Ex extern
#endif

//IU identify
#define	Command_IU		   0x01
#define	Sense_IU			   0x03
#define   Response_IU      		   0x04
#define   TaskManagement_IU    0x05
#define   ReadReady_IU               0x06
#define   WriteReady_IU              0x07




//Task attribute code
#define   Task_Simple                0x00
#define   Task_HeadOfQueue     0x01
#define   Task_Ordered              0x02
#define   Task_ACA                     0x04

//#define RAID_EXEC_DMA_CMD

#define TASK_ATTRIBUTE    0x07
#define TASK_PRIORITY       0x78

#define MAX_UAS_STREAM 	5 // Max 32 stream
#define MAX_UAS_CTXT_SITE	0x1F // max 32 Ctxt sites
#define MAX_UAS_BURST_SIZE 7// 8K size

#define UAS_READ_PREFECTH

Ex u8 uas_abort_sata_ch;
Ex u8 uas_RIU_active;
Ex u16 uas_ci_paused;
enum uas_PauseReason
{
	UAS_NO_PAUSE = 0,
	UAS_PAUSE_SATA0_NOT_READY = 0x01,
	UAS_PAUSE_SATA1_NOT_READY = 0x02,
	UAS_PAUSE_SATA0_NCQ_ERROR_CLEANUP= 0x04,
	UAS_PAUSE_SATA1_NCQ_ERROR_CLEANUP= 0x08,
	UAS_PAUSE_SATA0_NCQ_NO_ENOUGH_TAGS = 0x10,
	UAS_PAUSE_SATA1_NCQ_NO_ENOUGH_TAGS = 0x20,
	UAS_PAUSE_WAIT_FOR_USB_RESOURCE = 0x40,
	UAS_PAUSE_TASK_IU_RECEIVED = 0x80,
	UAS_PAUSE_RAID_RW			= 0x100, // the read/write command which cross the RAID engine shall be executed in single thread
	UAS_PAUSE_RAID_RB			= 0x200
};

enum uas_cmd_dispatch_states
{
	UAS_CMD_STATE_IDLE = 0,
	UAS_CMD_STATE_CDB_CHECK = 1,
	UAS_CMD_STATE_CIU_DISPATCH = 2,
	UAS_CMD_STATE_CIU_PAUSED = 3
};

enum {
	ABORT_CTXT_IS_ACTIVE_CTXT = BIT_0,
	ABORT_CTXT_INVOLVE_SATA0 = BIT_1,
	ABORT_CTXT_INVOLVE_SATA1 = BIT_2,
	ACTIVE_CTXT_INVOLVE_SATA0 = BIT_3,
	ACTIVE_CTXT_INVOLVE_SATA1 = BIT_4
};

//Task management response code
#define   TaskManageComplete    		0x00
#define   InvalidInformationUnit 		0x02
#define   TaskManageNotSupported     	0x04
#define   TaskManageFunctionFailed     0x05
#define   TaskManageFunctionSucceed  0x08
#define	INCORRECT_LOGICAL_UNIT_NUMBER	0x09
#define	OVERLAPPED_TAG_ATTEMPTED		0x0A

typedef struct _TASK_CTXT
{
	u8 	TASK_CTXT_FLAG;
	u8 	TASK_CTXT_FUNCTION;
	u16  TASK_CTXT_ITAG;
	u16  TASK_CTXT_CTAG;
	u8 	TASK_CTXT_LUN;
	u8 	TASK_CTXT_OSITE;
	struct _TASK_CTXT *taskCtxt_Next;
	u8 	TASK_RESPONSE; // this field is filled by firmware
	u8	TASK_CTXT_SITE;
	u8	reserved[2];
}TASK_CTXT, *PTASK_CTXT;

typedef struct
{
	PTASK_CTXT curTaskCtxt;
	PTASK_CTXT taskCtxt_que;
}TASK_CTXT_LIST;

Ex TASK_CTXT_LIST	taskCtxtList;

#define  ABORT_TASK   		0x01
#define  ABORT_TASK_SET    	0x02
#define  CLEAR_TASK_SET        0x04
#define  LOGICAL_UNIT_RESET  0x08
#define  I_T_NEXUS_RESET       0x10
#define  CLEAR_ACA                  0x40
#define  QUERY_TASK                0x80
#define  QUERY_TASK_SET        0x81
#define  QUERY_ASYC_EVENT    0x82

#define TASK_CTXT_FLAG_TAG_ZERO		BIT_4	// this is set if the Tag field is 0, 0xFFFE, 0xFFFF
#define TASK_CTXT_FLAG_OVERLAP_TASK	BIT_3	// this is set if the tag of TIU the same as one of the active ones in the task context
#define TASK_CTXT_FLAG_OVERLAP_CMD	BIT_2	// this is set if the tag of TIU the same as one of the active ones in the command context
#define TASK_CTXT_FLAG_SIZE_ERR		BIT_1 	// this is set if the TIU's size is not 16bytes
#define TASK_CTXT_FLAG_LUN_ERR		BIT_0	// this is set if the LUN is not correct


Ex PCDB_CTXT uas_ci_paused_ctxt;
Ex PCDB_CTXT uas_qenum_pending_ctxt;
Ex PCDB_CTXT new_dispatched_CIU;
#ifdef UAS_READ_PREFECTH
Ex PCDB_CTXT prefetched_rdCmd_ctxt; // CTXT which is used for the prefetched read command
#endif
Ex u8 uas_cmd_state;
#ifdef UAS_CLEAR_FEAT_ABORT
Ex u8 uas_abort_cmd_flag;
	#define UAS_ABORT_BY_CLEAR_FEATURE_DIN_HALT 1 // clear feature halt for DIN EP
	#define UAS_ABORT_BY_CLEAR_FEATURE_DOUT_HALT 2 // clear feature halt for DOUT EP
#endif

#ifdef WIN8_UAS_PATCH
Ex u8 intel_SeqNum_monitor;
	#define INTEL_SEQNUM_START_MONITOR	BIT_0
	#define INTEL_SEQNUM_CHECK_CONDITION	BIT_1
	#define INTEL_SEQNUM_CLEAR_FEATURE_HALT	BIT_2
Ex u8 intel_SeqNum_Monitor_Count;
Ex u8 intel_host_flag;
#endif

#ifdef UAS
extern void uas_init();
extern void uas_device_no_data(PCDB_CTXT pCtxt);
extern void uas_device_data_in(PCDB_CTXT pCtxt, u32 byteCnt);
extern void uas_device_data_out(PCDB_CTXT pCtxt, u32 byteCnt);
#endif
extern u8 uas_abort_que_ctxt(void);
extern void uas_abort_ctxt(PCDB_CTXT pCtxt);
#ifdef UAS_CLEAR_FEAT_ABORT
extern void uas_abort_cmd_by_clr_feature(u8 uas_abort_dir);
#endif
extern void uas_abort_all(void);
extern void uas_Chk_pending_SIU(PCDB_CTXT	pCtxt);
#ifdef UAS
extern void UAS_Task_Manage_IU();
#if 0
extern void uas_din_send_sense_iu(PCDB_CTXT	pCtxt);
extern void uas_dout_send_sense_iu(PCDB_CTXT	pCtxt);
#else
extern void uas_send_Sense_IU(PCDB_CTXT pCtxt);
#endif
extern void usb_uas();
extern void uas_exec_TIU_Que(void);

#endif

#endif

