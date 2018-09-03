/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/

#define UAS_C

#include "general.h"
#ifdef UAS
/****************************************\
   uas_init()
\****************************************/
void uas_init()
{
	curMscMode = MODE_UAS;
	usb_ctxt.curCtxt = NULL;
	usb_ctxt.ctxt_que = NULL;
	usb_ctxt.ctxt_que_tail = NULL;
	usb_ctxt.newCtxt= NULL;
	usb_ctxt.post_dout = 0;

	uas_ci_paused = 0;

	uas_ci_paused_ctxt = NULL;
	uas_qenum_pending_ctxt = NULL;
	new_dispatched_CIU = NULL;
	uas_abort_sata_ch = 0xFF;
	uas_cmd_state = UAS_CMD_STATE_IDLE;
#ifdef UAS_READ_PREFECTH
	prefetched_rdCmd_ctxt = NULL;
#endif
}

/****************************************\
   Dn
\****************************************/
void uas_device_no_data(PCDB_CTXT pCtxt)
{
	if (dbg_next_cmd)
		MSG("Dn\n");
//	pCtxt->CTXT_FLAG = CTXT_FLAG_U2B;		
#ifdef FAST_CONTEXT
	pCtxt->CTXT_No_Data = MSC_TX_CTXT_NODATA;
#else
	pCtxt->CTXT_No_Data = (MSC_TX_CTXT_VALID|MSC_TX_CTXT_NODATA) >> 8;
#endif
	pCtxt->CTXT_CONTROL |= CTXT_CTRL_DIR;
	pCtxt->CTXT_DbufIndex = SEG_NULL;
	pCtxt->CTXT_XFRLENGTH = 0;
	usb_exec_que_ctxt();
	return;
}

/****************************************\
   Di or Dn
\****************************************/
void uas_device_data_in(PCDB_CTXT pCtxt, u32 byteCnt)
{
	u32 i, tmp32, *p32;
	if (dbg_next_cmd)
		MSG("Di\n");

	pCtxt->CTXT_FLAG = CTXT_FLAG_U2B;
	pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
	pCtxt->CTXT_LunStatus = LUN_STATUS_GOOD;
	pCtxt->CTXT_XFRLENGTH = byteCnt;
	pCtxt->CTXT_CONTROL = CTXT_CTRL_DIR; // dir in
	//DBG("PCtxt: %lx, len: %lx", pCtxt, pCtxt->CTXT_XFRLENGTH);
	//DBG("\txfrLen: %lx", pCtxt->CTXT_XFRLENGTH);


	// Hi
	// input port of TX buffer Seg 3  is CPU Write
	// Output port of TX buffer Seg 3  is USB Read
	

	pCtxt->CTXT_DbufIndex = DBUF_SEG_U2BR;
//	pCtxt->CTXT_DBUF_IN_OUT = (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_CPU_W_PORT;
	
//	Tx_Dbuf->dbuf_Seg[pCtxt->CTXT_DbufIndex].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_NULL << 4) | TX_DBUF_CPU_W_PORT; 
	set_dbufSource(pCtxt->CTXT_DbufIndex);
	tmp32 = byteCnt >> 2;
	p32 = (u32 *)mc_buffer;
	for (i = 0; i < tmp32; i++)
	{
		Tx_Dbuf->dbuf_DataPort = *p32++;
	}
		
	if (byteCnt & 0x2)		// byte3, byte2
	{
		u16 *p16;	
		p16 = (u16 *)p32;				
		*((u16 *)&Tx_Dbuf->dbuf_DataPort) = *p16++;
		if (byteCnt & 0x1)		// byte3
			*(((u8 *)&Tx_Dbuf->dbuf_DataPort)+1) = *((u8 *)p16);
	}
	else if (byteCnt & 0x1)	// byte1
		*((u8 *)&Tx_Dbuf->dbuf_DataPort) = *((u8 *)p32);
						
	//DBG("Segment DONE!\t");
	Delay(1);
	set_dbufDone(pCtxt->CTXT_DbufIndex);

	if (dbg_next_cmd)
		MSG("seg dn\n");
	
//		*usb_Msc0TxCtxt = MSC_TX_CTXT_VALID|site;
#ifdef FAST_CONTEXT
	pCtxt->CTXT_No_Data = 0;
#else
	pCtxt->CTXT_No_Data = MSC_TX_CTXT_VALID >> 8;
#endif
	usb_exec_que_ctxt();
//		DBG("ok\n");
	return;
}



/****************************************\
   Do or Dn
\****************************************/
void uas_device_data_out(PCDB_CTXT pCtxt, u32 byteCnt)
{
	pCtxt->CTXT_FLAG = CTXT_FLAG_U2B;
	pCtxt->CTXT_Status = CTXT_STATUS_PENDING;
//	pCtxt->CTXT_LunStatus = LUN_STATUS_GOOD;
	pCtxt->CTXT_CONTROL = 0;

	pCtxt->CTXT_XFRLENGTH = byteCnt;

	// USB Data Out

	// input port of RX buffer Seg 4  is CPU Read
	// Output port of RX buffer Seg 4  is USB Write
	pCtxt->CTXT_DbufIndex = DBUF_SEG_U2BW;
//	pCtxt->CTXT_DBUF_IN_OUT = (RX_DBUF_CPU_R_PORT << 4) | RX_DBUF_USB_W_PORT; 

			
//	*usb_Msc0RxCtxt = MSC_RX_CTXT_VALID|site;
#ifdef FAST_CONTEXT
	pCtxt->CTXT_No_Data = 0;
#else
	pCtxt->CTXT_No_Data = (MSC_RX_CTXT_VALID) >> 8;
#endif
	usb_exec_que_ctxt();
}
#endif

void uas_abort_ctxt(PCDB_CTXT pCtxt)
{
	*usb_CtxtFreeFIFO = pCtxt->CTXT_Index;
	*usb_Msc0LunCtxtUsed = 0xffff;
}

#ifdef UAS
u8 uas_abort_que_ctxt(void)
{
	PCDB_CTXT pCtxt = usb_ctxt.ctxt_que;
	if (pCtxt == NULL)
	{
		uas_ci_paused = 0;
		uas_abort_sata_ch = 0xFF;
	}
	else
	{
		if (arrayMode == JBOD)
		{// if it's JBOD mode, the other SATA which is not reseted can work fine
			if (uas_abort_sata_ch != pCtxt->CTXT_LUN)
				return 1;
		}
		usb_ctxt.ctxt_que = pCtxt->CTXT_Next;
		pCtxt->CTXT_Next = NULL;
		MSG("AB C %bx, l %bx, t %bx\n", pCtxt->CTXT_CDB[0], pCtxt->CTXT_LUN, (u8)pCtxt->CTXT_ITAG);	
		pCtxt->CTXT_LunStatus = LUN_STATUS_BUSY;	
		pCtxt->CTXT_Status = CTXT_STATUS_ERROR;
		uas_device_no_data(pCtxt);
	}
	return 0;
}

void uas_abort_all(void)
{
// abort all the received CIU and Task IU
	PCDB_CTXT pCtxt = usb_ctxt.curCtxt;
	if (pCtxt)
	{
		// abort the executing CMD
		if (pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR)
		{
			// reset the DIN EP
			*usb_Msc0DICtrl = MSC_DATAIN_RESET;
		}
		else
		{
			// reset the DOUT EP
			*usb_Msc0DOutCtrl = MSC_DOUT_RESET;
		}
		pCtxt ->CTXT_ccmIndex = CCM_NULL; 
		uas_abort_ctxt(pCtxt);
		usb_ctxt.curCtxt = NULL;
		rw_flag = 0; // go back to idle
	}
	
	if (uas_qenum_pending_ctxt)
	{
		uas_abort_ctxt(uas_qenum_pending_ctxt);
		uas_qenum_pending_ctxt = NULL;
		fetch_Ctxt_mem = 0;
	}
	sata_abort_all(&sata0Obj);
	sata_abort_all(&sata1Obj);
}

static void reschedule_USB_ctxt(u8 uasQueSituation)
{
	// if SATA is resetted, shall reschedule the SATA commands which have been sent to SATA and follow the previous sequence
	PCDB_CTXT pAbCtxt = usb_ctxt.ctxt_que;
	PCDB_CTXT reschedule_startCTXT = NULL;
	
	if (pAbCtxt == NULL)	return;
	
	while (pAbCtxt != usb_ctxt.newCtxt)
	{
		if ((pAbCtxt->CTXT_usb_state > CTXT_USB_STATE_RECEIVE_CIU)
		&& ((pAbCtxt->CTXT_FLAG & CTXT_FLAG_U2B) == 0))
		{
			if (((uasQueSituation & (ABORT_CTXT_INVOLVE_SATA0 |ABORT_CTXT_INVOLVE_SATA1)) == (ABORT_CTXT_INVOLVE_SATA0 |ABORT_CTXT_INVOLVE_SATA1) )// both two SATA drives will be resetted, and the command involve SATA shall be reschedule
			|| (((uasQueSituation & (ABORT_CTXT_INVOLVE_SATA0 |ABORT_CTXT_INVOLVE_SATA1)) == ABORT_CTXT_INVOLVE_SATA0 ) && (pAbCtxt->CTXT_CONTROL & CTXT_CTRL_SATA0_EN)) // SATA drive 0 will be resetted, and the command involve drive0 shall be reschedule
			|| (((uasQueSituation & (ABORT_CTXT_INVOLVE_SATA0 |ABORT_CTXT_INVOLVE_SATA1)) == ABORT_CTXT_INVOLVE_SATA1 ) && (pAbCtxt->CTXT_CONTROL & CTXT_CTRL_SATA1_EN)))  // SATA drive 1 will be resetted, and the command involve drive1 shall be reschedule
			{
				if (reschedule_startCTXT == NULL)
				{
					reschedule_startCTXT = pAbCtxt;
				}
				
				if (pAbCtxt->CTXT_usb_state > CTXT_USB_STATE_AWAIT_SATA_DMASETUP)
				{
					// if the Ctxt has allocated the Dbuf resource, and only wait for USB resource, 
					// the dbuf resource shall be released, and be reallocated again when the DMASETUP is received after reschedule
					pAbCtxt->CTXT_usb_state = CTXT_USB_STATE_RECEIVE_CIU; 
					if (pAbCtxt->CTXT_DbufIndex != SEG_NULL)
					{
						reset_dbuf_seg(pAbCtxt->CTXT_DbufIndex);
						free_dbufConnection(pAbCtxt->CTXT_DbufIndex);
					}
				}
			}
		}
		pAbCtxt = pAbCtxt->CTXT_Next;
		if (pAbCtxt == NULL) break;
	}
	if (reschedule_startCTXT)
	{
		reschedule_startCTXT->CTXT_usb_state = CTXT_USB_STATE_RECEIVE_CIU;
		usb_ctxt.newCtxt = reschedule_startCTXT;
	}
}
#ifdef UAS_CLEAR_FEAT_ABORT
void uas_abort_cmd_by_clr_feature(u8 uas_abort_dir)
{
	if (usb_ctxt.curCtxt)
	{
		// reset the EP and stop the transfer because the host tries to terminate the transfer by clear feature halt in WIn8
		PCDB_CTXT pCtxt = usb_ctxt.curCtxt;
		
		// if the clear feature halt is to clear halt on DOUT EP, and the present transfer is on DIN EP. Then do not abort this command
		// if the clear feature halt is to clear halt on DIN EP, and the present transfer is on DOUT EP. Then do not abort this command
		if ((pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR) && (uas_abort_dir == UAS_ABORT_BY_CLEAR_FEATURE_DOUT_HALT))
		{
			return;
		}
		else if (((pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR) == 0) && (uas_abort_dir == UAS_ABORT_BY_CLEAR_FEATURE_DIN_HALT))
		{
			return;
		}
		if (((pCtxt->CTXT_No_Data & BIT_6) == 0) && ((pCtxt->CTXT_FLAG & CTXT_FLAG_U2B) == 0)) // no data transfer
		{
			if (pCtxt->CTXT_usb_state >= CTXT_USB_STATE_CTXT_SETUP_DONE)
			{
				MSG("ClrF Halt\n");
				//dbg_next_cmd = 5;
				if (pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR)
				{
					rst_din(pCtxt);
					*usb_Msc0DICtrl = MSC_DATAIN_RESET;
					pCtxt->CTXT_XFRLENGTH = 0;
					*usb_Msc0TxCtxt = MSC_TX_CTXT_VALID|MSC_TX_CTXT_NODATA|pCtxt->CTXT_Index;										
				}
				else
				{
					rst_dout(pCtxt);
					*usb_Msc0DOutCtrl = MSC_DOUT_RESET;
					pCtxt->CTXT_XFRLENGTH = 0;
					*usb_Msc0RxCtxt = MSC_RX_CTXT_VALID|MSC_RX_CTXT_NODATA|pCtxt->CTXT_Index;
				}
				pCtxt ->CTXT_ccmIndex = CCM_NULL; 
				hdd_err_2_sense_data(pCtxt, ERROR_DATA_PHASE_ABORTED);
				uas_ci_paused |= UAS_PAUSE_WAIT_FOR_USB_RESOURCE;
				uas_ci_paused_ctxt = pCtxt;
				usb_ctxt_send_status(pCtxt);	
				u8 uasQueSituation = 0;
				if (pCtxt->CTXT_CONTROL & CTXT_CTRL_SATA0_EN)
				{
					if (sata0Obj.sobj_State > SATA_READY)
					{
						sata_Reset(&sata0Obj, SATA_HARD_RST);
						uas_ci_paused |= UAS_PAUSE_SATA0_NOT_READY;
						uasQueSituation |= ABORT_CTXT_INVOLVE_SATA0;
					}
				}
				if (pCtxt->CTXT_CONTROL & CTXT_CTRL_SATA1_EN)
				{
					if (sata1Obj.sobj_State > SATA_READY)
					{
						sata_Reset(&sata1Obj, SATA_HARD_RST);
						uas_ci_paused |= UAS_PAUSE_SATA0_NOT_READY;
						uasQueSituation |= ABORT_CTXT_INVOLVE_SATA1;
					}
				}
				reschedule_USB_ctxt(uasQueSituation);
			}
		}
	}
}
#endif

/****************************************\
	UAS_Task_Manage_IU
\****************************************/
#define RIU_TRANSMITION_NO_TIU_SITE	0x80
/**********************************************************
Firmware can transmit the RIU w/ or w/o associated TIU 
if the flag == 0x80, it transmit RIU w/o TIU, it's like the case that there's overlapped CIUs, 
and firmware aboat the CIU by RIU with response code OVERLAPPED TAG 
if flag = 0~15, it's the site number of the associated TIU
***********************************************************/
void uas_Respond_RIU(u16 tag, u8 response_code, u8 flag)
{
	*usb_Msc0StatCtrlClr = MSC_STAT_RESP_RUN;  //clear it	
	if (*usb_Msc0IntStatus_shadow & MSC_TASK_AVAIL_INT)
		*usb_Msc0IntStatus = MSC_TASK_AVAIL_INT;	
	if (*usb_IntStatus & CDB_AVAIL_INT)
		*usb_IntStatus = CDB_AVAIL_INT;
	if (flag == RIU_TRANSMITION_NO_TIU_SITE)
	{
		*usb_Msc0Tag = MSC_TASK_NEW|tag;
		uas_RIU_active = 1;
	}
	else
	{
		*usb_Msc0Tag = ((u32)flag<<16);
	}
	DBG("msc0_tag %lx\n", *usb_Msc0Tag);
	*usb_Msc0Response = (response_code << 24);
	//now send it
	*usb_Msc0StatCtrl = MSC_STAT_RESP_RUN;
}

void uas_abort_TIU_list(void)
{
	// in normal case, it shall not happen.
	// if there's overlapped TIU or CIU, it shall happen
	PTASK_CTXT pTaskCtxt = taskCtxtList.curTaskCtxt; 
	if (pTaskCtxt)
	{
		*usb_Msc0StatCtrlClr = MSC_STAT_RESP_RUN; 
		*usb_Msc0StatCtrl = MSC_STAT_RESET;
		MSG("Fr tiu %bx\n", pTaskCtxt->TASK_CTXT_SITE);
		*usb_Msc0TaskFreeFIFO = pTaskCtxt->TASK_CTXT_SITE;		
		taskCtxtList.curTaskCtxt = NULL; 		
	}
	
	if ((pTaskCtxt = taskCtxtList.taskCtxt_que) != NULL)
	{// free the uas task management IU lis 
		while (taskCtxtList.taskCtxt_que)
		{
			taskCtxtList.taskCtxt_que = pTaskCtxt->taskCtxt_Next;
			MSG("F tiu %bx\n", pTaskCtxt->TASK_CTXT_SITE);
			*usb_Msc0TaskFreeFIFO = taskCtxtList.taskCtxt_que->TASK_CTXT_SITE;	
			pTaskCtxt->taskCtxt_Next = NULL;
		}
	}
}

void uas_delete_ctxt(PCDB_CTXT pCtxt)
{
	DBG("del ctxt s %bx, t %bx\n", pCtxt->CTXT_Index, (u8)pCtxt->CTXT_ITAG);
	if (pCtxt == new_dispatched_CIU)
	{
		new_dispatched_CIU = NULL;
		uas_cmd_state = UAS_CMD_STATE_IDLE;
	}
	if (pCtxt == uas_ci_paused_ctxt)
	{
		if (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN)
		{
			uas_ci_paused &= ~UAS_PAUSE_RAID_RW;
		}
	}
	
	if (pCtxt == usb_ctxt.newCtxt)
	{// new Ctxt is going to be deleted, and it's not dispatched yet, move the point to next CTXT
		usb_ctxt.newCtxt = pCtxt->CTXT_Next;
	}
#ifdef UAS_READ_PREFECTH
	if (prefetched_rdCmd_ctxt == pCtxt)
	{
		prefetched_rdCmd_ctxt = NULL;
	}
#endif
	usb_deQue_ctxt(pCtxt);
	uas_abort_ctxt(pCtxt);
}

#if 1
u8 chk_valid_tagID(u16 tagID)
{
	if ((tagID > 0) && (tagID < 0xFFFD))
	{
		return 0;
	}
	else
	{
		MSG("iv TID\n");
		return 1;
	}
}
#endif

// create this function to save the code size
static u8 chkCtxtAndDelCtxt(PCDB_CTXT pCtxt)
{
	u8 rc = 0;
	// for the command which does not involve the SATA data transfer(like Inquiry) 
	// or media access command but it's not sent to SATA yet, delete the node in USB Que
	if ((pCtxt->CTXT_usb_state > CTXT_USB_STATE_AWAIT_SATA_READY) && 
		((pCtxt->CTXT_FLAG & CTXT_FLAG_U2B) == 0))
	{
		rc = 1;
	}
	if (pCtxt->CTXT_DbufIndex != SEG_NULL)
	{
		if (pCtxt->CTXT_DbufIndex != SEG_NULL)
		{
			reset_dbuf_seg(pCtxt->CTXT_DbufIndex);
			free_dbufConnection(pCtxt->CTXT_DbufIndex);
		}
	}
	uas_delete_ctxt(pCtxt);	
	return rc;
}

/* 	return value: 
	1: means that the abort CTXT is the active ctxt, shall reset USB EP
	2: means the active CTXT involve SATA transfer, and the SATA DISK will be resetted because the abort CTXT also involve SATA
*/
static void chk_USB_EP_need_reset(u8 usbQueState)
{
	u8 val = 0;
	if (usbQueState & ABORT_CTXT_IS_ACTIVE_CTXT)
	{
		val = 1;
	}
	else if (((usbQueState & ACTIVE_CTXT_INVOLVE_SATA0) && (usbQueState & ABORT_CTXT_INVOLVE_SATA0))
	||((usbQueState & ACTIVE_CTXT_INVOLVE_SATA1) && (usbQueState & ABORT_CTXT_INVOLVE_SATA1)))
	{
		val = 2;
	}

	if (val)
	{
		PCDB_CTXT pAbCtxt = usb_ctxt.curCtxt;
		reset_Endpoint(pAbCtxt);
		usb_ctxt.curCtxt = NULL;
		if (val == 2)
		{
			pAbCtxt->CTXT_usb_state = CTXT_USB_STATE_CTXT_SETUP_DONE;
			hdd_err_2_sense_data(pAbCtxt, ERROR_DATA_PHASE_ABORTED);
			usb_device_no_data(pAbCtxt);
		}
	}
}

// return value: 1 -> only happens in Abort task function beause there's no CIU with the Tag = task Tag in TIU
u8 uas_abort_function(PTASK_CTXT pTaskCtxt)
{
	u8 rc = 1;
	// for usb Active Ctxt, it might has different cases and it shall be handled differently
	// case 1: C1, C2, C3, T4: C1 is active command, and abort C2 which is in USB que, just remove C2
	// case 2: C1, C2, C3, T4: C1 is active command, and abort C1, reset the USB and remove C1.
	// case 3: C1, C2, C3, T4: C1,C2 has been sent to SATA, C1 is active command, and abort C2, reset USB, and abort C1 with abort sense
	// case 4: C1, C2, C3, T4: C1,C2 has been sent to SATA, C2 is active commands, and abort C2, reset USB, and re-issue C1
	u8 usbCtxtQueSituation = 0;

	MSG("TIU f%bx\n", pTaskCtxt->TASK_CTXT_FUNCTION);

	PCDB_CTXT pQCtxt = usb_ctxt.ctxt_que;
	PCDB_CTXT pAbCtxt = NULL;
	if (usb_ctxt.curCtxt)
	{
		if ((usb_ctxt.curCtxt->CTXT_FLAG & CTXT_FLAG_U2B) == 0)
		{
			if (usb_ctxt.curCtxt->CTXT_CONTROL & CTXT_CTRL_SATA0_EN)
			{
				usbCtxtQueSituation |= ACTIVE_CTXT_INVOLVE_SATA0;
			}
			if (usb_ctxt.curCtxt->CTXT_CONTROL & CTXT_CTRL_SATA1_EN)
			{
				usbCtxtQueSituation |= ACTIVE_CTXT_INVOLVE_SATA1;
			}
		}
	}
	switch(pTaskCtxt->TASK_CTXT_FUNCTION)
	{
		case ABORT_TASK:
			// C1 C2 C3 T4(abort C2)
			while (pQCtxt != NULL)
			{
				if ((pQCtxt->CTXT_ITAG == pTaskCtxt->TASK_CTXT_ITAG) && (pTaskCtxt->TASK_CTXT_LUN == pQCtxt->CTXT_LUN))
				{// find CIU, and abort it
					if (pQCtxt == usb_ctxt.curCtxt)
					{// it's the current active USB command
						usbCtxtQueSituation |= ABORT_CTXT_IS_ACTIVE_CTXT;
					}
					if (chkCtxtAndDelCtxt(pQCtxt))
					{
						if (pQCtxt->CTXT_CONTROL & CTXT_CTRL_SATA0_EN)
						{
							usbCtxtQueSituation |= ABORT_CTXT_INVOLVE_SATA0;
						}
						if (pQCtxt->CTXT_CONTROL & CTXT_CTRL_SATA1_EN)							
						{
							usbCtxtQueSituation |= ABORT_CTXT_INVOLVE_SATA1;
						}
					}
					rc = 0;
					break;
				}
				pQCtxt = pQCtxt->CTXT_Next;
			}

			if (rc == 0)
			{// the aborted Tag is in the Task set, and has been aborted		
				chk_USB_EP_need_reset(usbCtxtQueSituation);
				// if SATA is resetted, shall reschedule the SATA commands which have been sent to SATA and follow the previous sequence
				if (usbCtxtQueSituation & (ABORT_CTXT_INVOLVE_SATA0 | ABORT_CTXT_INVOLVE_SATA1))
				{
					reschedule_USB_ctxt(usbCtxtQueSituation);
				}
			}
			break;
			
		case ABORT_TASK_SET:
		case CLEAR_TASK_SET:
		case LOGICAL_UNIT_RESET:
			// LUN 0: C1 C2 C4
			// LUN 1: C3
			// LUN 2: C5
			// TIU: T6 abort LUN 0
			if (usb_ctxt.curCtxt != NULL)
			{
				if ( usb_ctxt.curCtxt->CTXT_LUN == pTaskCtxt->TASK_CTXT_LUN)
				{// the active USB command is required to be aborted
					usbCtxtQueSituation |= ABORT_CTXT_IS_ACTIVE_CTXT;
				}
			}
			
			while (pQCtxt != NULL)
			{
				if (pQCtxt->CTXT_LUN == pTaskCtxt->TASK_CTXT_LUN)
				{
					// search the CIU list, abort the CIU with LUN field set to taskLUN
					if (chkCtxtAndDelCtxt(pQCtxt))
					{
						if (pQCtxt->CTXT_CONTROL & CTXT_CTRL_SATA0_EN)
						{
							usbCtxtQueSituation |= ABORT_CTXT_INVOLVE_SATA0;
						}
						if (pQCtxt->CTXT_CONTROL & CTXT_CTRL_SATA1_EN)							
						{
							usbCtxtQueSituation |= ABORT_CTXT_INVOLVE_SATA1;
						}
					}
				}
				pQCtxt = pQCtxt->CTXT_Next;
			}

			chk_USB_EP_need_reset(usbCtxtQueSituation);
			if (usbCtxtQueSituation & (ABORT_CTXT_INVOLVE_SATA0 | ABORT_CTXT_INVOLVE_SATA1))
			{
				reschedule_USB_ctxt(usbCtxtQueSituation);
			}
			rc = 0;
			break;

		case I_T_NEXUS_RESET:
			if (usb_ctxt.curCtxt != NULL)
			{
				usbCtxtQueSituation |= ABORT_CTXT_IS_ACTIVE_CTXT;
			}
			// abort all CIUs
			while (pQCtxt != NULL)
			{
				if (chkCtxtAndDelCtxt(pQCtxt))
				{
					if (pQCtxt->CTXT_CONTROL & CTXT_CTRL_SATA0_EN)
					{
						usbCtxtQueSituation |= ABORT_CTXT_INVOLVE_SATA0;
					}
					if (pQCtxt->CTXT_CONTROL & CTXT_CTRL_SATA1_EN)							
					{
						usbCtxtQueSituation |= ABORT_CTXT_INVOLVE_SATA1;
					}
				}
				pQCtxt = pQCtxt->CTXT_Next;
			}
			rc = 0;
			if (usbCtxtQueSituation & ABORT_CTXT_IS_ACTIVE_CTXT)
			{
				reset_Endpoint(usb_ctxt.curCtxt);
				rw_flag = 0;
				usb_ctxt.curCtxt = NULL;
			}
			break;
	}

	if (rc == 0)
	{
		if (usbCtxtQueSituation & (ABORT_CTXT_INVOLVE_SATA0 | ABORT_CTXT_INVOLVE_SATA1))
		{
			if (usbCtxtQueSituation & ABORT_CTXT_INVOLVE_SATA0)
			{
				sata_Reset(&sata0Obj, SATA_HARD_RST);
				uas_ci_paused |= UAS_PAUSE_SATA0_NOT_READY;
			}
			if (usbCtxtQueSituation & ABORT_CTXT_INVOLVE_SATA1)
			{
				sata_Reset(&sata1Obj, SATA_HARD_RST);
				uas_ci_paused |= UAS_PAUSE_SATA1_NOT_READY;
			}
		}
	}
	return rc;
}

void uas_exec_TIU_ctxt(PTASK_CTXT pTaskCtxt)
{
	switch(pTaskCtxt->TASK_CTXT_FUNCTION)
	{
		case  ABORT_TASK:
			if (uas_abort_function(pTaskCtxt))
			{
				pTaskCtxt->TASK_RESPONSE = TaskManageFunctionFailed;			// TASK MANAGEMENT FUNCTION FAILED
			}
			break;

		case  ABORT_TASK_SET:
		case  CLEAR_TASK_SET:
		case  LOGICAL_UNIT_RESET:
		case  I_T_NEXUS_RESET:
			uas_abort_function(pTaskCtxt);
			// clear all the commands
			break;

		case  CLEAR_ACA:					
		case  QUERY_TASK:
		case  QUERY_TASK_SET:
		case  QUERY_ASYC_EVENT:
			break;
			
		default:
			pTaskCtxt->TASK_RESPONSE = TaskManageNotSupported;			// TASK MANAGEMENT FUNCTION NOT SUPPORTED
			break;		
	}
	uas_Respond_RIU(pTaskCtxt->TASK_CTXT_ITAG, pTaskCtxt->TASK_RESPONSE, pTaskCtxt->TASK_CTXT_SITE);	
}

void uas_append_TIU_ctxt(PTASK_CTXT pTaskCtxt)
{
	// if there's executed Task CTXT in processing, push the
	pTaskCtxt->TASK_RESPONSE = TaskManageComplete;
	if (pTaskCtxt->TASK_CTXT_FLAG & (TASK_CTXT_FLAG_LUN_ERR | TASK_CTXT_FLAG_OVERLAP_TASK | TASK_CTXT_FLAG_OVERLAP_CMD))
	{
		u16 iTag = pTaskCtxt->TASK_CTXT_ITAG;
		u8 rspCode = OVERLAPPED_TAG_ATTEMPTED;
		*usb_Msc0StatCtrlClr = MSC_STAT_RESP_RUN;  //clear it	
		*usb_Msc0StatCtrl = MSC_STAT_RESET;
		if (pTaskCtxt->TASK_CTXT_FLAG & TASK_CTXT_FLAG_LUN_ERR)
		{
			pTaskCtxt->TASK_RESPONSE = INCORRECT_LOGICAL_UNIT_NUMBER;
			rspCode = INCORRECT_LOGICAL_UNIT_NUMBER;
		}
		else
		{
			MSG("Ovlap TIU, t%bx, flg %bx, ot %bx\n", (u8)pTaskCtxt->TASK_CTXT_ITAG, pTaskCtxt->TASK_CTXT_FLAG, pTaskCtxt->TASK_CTXT_OSITE);
			DBG("00: %bx, %bx\n", (u8)pTaskCtxt->TASK_CTXT_ITAG, pTaskCtxt->TASK_RESPONSE);
			// abort all the command/task Context 
			TASK_CTXT l_task_ctxt;
			l_task_ctxt.TASK_CTXT_FUNCTION = I_T_NEXUS_RESET;
			uas_abort_function(&l_task_ctxt);
			uas_abort_TIU_list();
			pTaskCtxt->TASK_RESPONSE = OVERLAPPED_TAG_ATTEMPTED;
		}
		DBG("2:%bx, %bx\n", (u8)pTaskCtxt->TASK_CTXT_ITAG, pTaskCtxt->TASK_RESPONSE);
		DBG("3:%bx, %bx\n", (u8)pTaskCtxt->TASK_CTXT_ITAG, pTaskCtxt->TASK_RESPONSE);
		uas_Respond_RIU(iTag, rspCode, RIU_TRANSMITION_NO_TIU_SITE);
		*usb_Msc0TaskFreeFIFO = pTaskCtxt->TASK_CTXT_SITE;
		return;
	}
	else if (pTaskCtxt->TASK_CTXT_FLAG & (TASK_CTXT_FLAG_SIZE_ERR | TASK_CTXT_FLAG_TAG_ZERO))
	{
		pTaskCtxt->TASK_RESPONSE = InvalidInformationUnit;
	}

	// shall wait for the clean up of TASK MANAGEMENT function
	uas_ci_paused |= UAS_PAUSE_TASK_IU_RECEIVED;
	if ( taskCtxtList.curTaskCtxt == NULL)
	{
		// execute the task context
		uas_exec_TIU_ctxt(pTaskCtxt);
		taskCtxtList.curTaskCtxt = pTaskCtxt;
	}
	else
	{
		// find the tail of the que
		PTASK_CTXT pTask_Que =  taskCtxtList.taskCtxt_que;
		pTaskCtxt->taskCtxt_Next = NULL;
		if (pTask_Que == NULL)
		{
			 taskCtxtList.taskCtxt_que= pTaskCtxt;
		}
		else
		{
			while (pTask_Que->taskCtxt_Next)
			{
				pTask_Que = pTask_Que->taskCtxt_Next;
			}
			pTask_Que->taskCtxt_Next = pTaskCtxt;
		}
	}
}
void uas_exec_TIU_Que(void)
{
	PTASK_CTXT pTaskCtxt = taskCtxtList.taskCtxt_que;
	if (pTaskCtxt)
	{
		taskCtxtList.taskCtxt_que = pTaskCtxt->taskCtxt_Next;
		pTaskCtxt->taskCtxt_Next = NULL;
		taskCtxtList.curTaskCtxt = pTaskCtxt;
		uas_exec_TIU_ctxt(pTaskCtxt);
	}
	else
	{
		// if the uas pause is set when receive TIU, clear it
		if (uas_ci_paused & UAS_PAUSE_TASK_IU_RECEIVED)
		{
			uas_ci_paused &= ~UAS_PAUSE_TASK_IU_RECEIVED;
		}
	}
}

// merge the din_send_SIU & dou_send_SIU to save the code size 
void uas_send_Sense_IU(PCDB_CTXT pCtxt)
{
	// Reseet Sense Data Pointer
	u8 volatile *usb_mscStatus;
	u32 volatile  *usb_mscSensePort;
	if (pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR)
	{
		*usb_Msc0TxSenseAddress = 0;
		usb_mscStatus = usb_Msc0DIStatus;
		usb_mscSensePort = usb_Msc0TxSensePort;
	}
	else
	{
		*usb_Msc0RxSenseAddress = 0;
		usb_mscStatus = usb_Msc0DOStatus;
		usb_mscSensePort = usb_Msc0RxSensePort;
	}
	
	// check status byte
  	if (pCtxt->CTXT_Status != CTXT_STATUS_GOOD)
	{
		u8 senseBuffer[24];
		u32  *u32ptr, i;
		u8 lun_status = pCtxt->CTXT_LunStatus;
		
		*usb_mscStatus = lun_status;		

		if (lun_status == LUN_STATUS_CHKCOND)
		{	//check condition
			xmemset(senseBuffer, 0, 24);
			senseBuffer[0] = 0x70;
			senseBuffer[7] = 10;		// additional Sense Length

			if (pCtxt->CTXT_CDB[12] == 22)
			{	// extended Descriptor
				senseBuffer[0] = 0x72;		// response code
				senseBuffer[7] = 14;		// additional Sense Length

				extended_descriptor = 0;
				if (pCtxt->CTXT_CDB[0] == SCSI_ATA_PASS_THR16)
				{
					extended_descriptor = pCtxt->CTXT_CDB[1] & 0x01;		// extend bit
				}
				Ata_return_descriptor_sense(pCtxt, &senseBuffer[8]);
			}
			else if (pCtxt->CTXT_CDB[12] == 18)
			{
				senseBuffer[2] =  pCtxt->CTXT_CDB[13];		// sense code
				senseBuffer[12] = pCtxt->CTXT_CDB[14];	// ASC
				senseBuffer[13] = pCtxt->CTXT_CDB[15];	// ASCQ
			}

			u32ptr = ((u32 *)senseBuffer);
	
			//copy20 byte sense data 
			for (i=0; i < 5 ; i++)
			{
				if ((pCtxt->CTXT_CDB[12] == 18) && (i == 4))
					*((u16 *)usb_mscSensePort) = *((u16 *)u32ptr);
				else
					*usb_mscSensePort = *u32ptr++;
			}
			// copy 4 more bytes
			if (pCtxt->CTXT_CDB[12] == 22)
			{
				*((u16 *)usb_mscSensePort) = *((u16 *)u32ptr);
			}
		}
	}
	else  
	{
		*usb_mscStatus = LUN_STATUS_GOOD; 	
	}
	DBG("S SIU\n");
	if (pCtxt->CTXT_Status == CTXT_STATUS_ERROR)
	{
		if (raid_rb_state == RAID_RB_IDLE)
		{
			if (raid1_error_detection(pCtxt)) return;
		}
	}
	usb_deQue_ctxt(usb_ctxt.curCtxt);

	//this instruction will cause hardware to send sense IU	
	if (pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR)
	{
		*usb_Msc0StatCtrl = MSC_STAT_TXSENSE_RUN;
	}
	else
	{
		*usb_Msc0StatCtrl = MSC_STAT_RXSENSE_RUN;
	}
}

#endif

void uas_Chk_pending_SIU(PCDB_CTXT	pCtxt)
{
	if (dbg_next_cmd)
	{
		DBG("Chk PSIU\n");
	}
	if ((pCtxt->CTXT_CONTROL & CTXT_CTRL_SEND_SIU_DONE) == 0)
	{
		pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
		if (pCtxt->CTXT_CONTROL & CTXT_CTRL_SEND_SIU)
		{
			usb_ctxt_send_status(pCtxt);
			pCtxt->CTXT_CONTROL &= ~CTXT_CTRL_SEND_SIU;
		}
		else
		{
			// for the USB to SATA command, the next DMASetup or setDevBit has already received
			if ((pCtxt->CTXT_FLAG & (CTXT_FLAG_B2S|CTXT_FLAG_U2B)) == 0)
			{
				pCtxt->CTXT_CONTROL |= CTXT_CTRL_SEND_SIU;
				pCtxt->CTXT_ccmIndex = CCM_NULL;
			}
		}
	}
}

#ifdef DBG_FUNCTION
void dbg_uas_flow_ctrl(void)
{
	static u8 uas_deadtime_debug_count = 0;
	if (++uas_deadtime_debug_count == 5)
	{
		dump_all_regs(NULL);
		*usb_DevCtrlClr = USB_ENUM;	
		while (1)
		{
			if (USB_VBUS_OFF())
				return;
		}
	}
}
#endif

void uas_CIU_pending(PCDB_CTXT pCtxt)
{
	pCtxt->CTXT_usb_state = CTXT_USB_STATE_AWAIT_SATA_READY;
	DBG("pd c %bx\n", (u8)pCtxt->CTXT_ITAG);
//	dbg_next_cmd = 4;
	if ((logicalUnit == HDD0_LUN) && (sata0Obj.sobj_device_state != SATA_NO_DEV))
		uas_ci_paused |= UAS_PAUSE_SATA0_NOT_READY;//SATA_CH1
	else
		uas_ci_paused |= UAS_PAUSE_SATA1_NOT_READY;//SATA_CH0

	uas_ci_paused_ctxt = pCtxt;

	usb_ctxt.newCtxt = pCtxt;
}

// this function is used to hadle the MSC interrupt for UAS mode
static u8 uas_MSC_isr(void)
{
	u32	 msc_int_status = *usb_Msc0IntStatus_shadow;

	//check if management_IU posted
	if (msc_int_status & MSC_TASK_AVAIL_INT)
	{
		u8 task_IU_site = *usb_Msc0TaskAvailableFIFO;
		PTASK_CTXT pTaskCtxt = (PTASK_CTXT)(TASK_CTXT_MEMORY_ADDR + (task_IU_site << 4));
		pTaskCtxt->TASK_CTXT_SITE = task_IU_site;
//		MSG("Tiu t %bx, s %bx, flg %bx, func %bx, osite %bx\n", (u8)pTaskCtxt->TASK_CTXT_ITAG,task_IU_site, pTaskCtxt->TASK_CTXT_FLAG, pTaskCtxt->TASK_CTXT_FUNCTION, pTaskCtxt->TASK_CTXT_OSITE);
		MSG("Tiu t %bx\n", (u8)pTaskCtxt->TASK_CTXT_ITAG);
		*usb_Msc0IntStatus = MSC_TASK_AVAIL_INT;	
		uas_append_TIU_ctxt(pTaskCtxt);
	}

	if (msc_int_status & (MSC_ST_TIMEOUT_INT|MSC_DI_TIMEOUT_INT|MSC_DO_TIMEOUT_INT))
	{
		if ((usb_ctxt.curCtxt == NULL) && (taskCtxtList.curTaskCtxt == NULL) && (uas_RIU_active == 0))
		{
			*usb_Msc0IntStatus = MSC_ST_TIMEOUT_INT|MSC_DI_TIMEOUT_INT|MSC_DO_TIMEOUT_INT;
		}
		else
		{
#if 0//def DBG_FUNCTION
			dbg_uas_flow_ctrl();
#endif
			if (msc_int_status & MSC_ST_TIMEOUT_INT)
			{
				*usb_Msc0IntStatus = MSC_ST_TIMEOUT_INT;
				if ((*usb_Msc0StatCtrl & (STATUS_ACKE|STATUS_ACKR))
				&& (*usb_Msc0StatCtrl & (MSC_STAT_TXSENSE_RUN|MSC_STAT_RXSENSE_RUN|MSC_STAT_RESP_RUN)))
				{
					MSG("S_F\n");
					*usb_Msc0StatCtrl = MSC_STAT_FLOW_CTRL;
				}
				else
					*usb_Msc0StatCtrlClr = MSC_STAT_FLOW_CTRL;// clear the Flow control
			}

			if (usb_ctxt.curCtxt)
			{
				if (msc_int_status & MSC_DI_TIMEOUT_INT)
				{
					*usb_Msc0IntStatus = MSC_DI_TIMEOUT_INT;
					if ((*usb_Msc0DIXferStatus & MSC_DATAIN_HOST_WAIT) >= ACKE)
					{
						if ((*usb_Msc0DICtrl & MSC_DIN_FLOW_CTRL) == 0)
						{
							MSG("Di_F %bx, %bx\n", *usb_Msc0DIXferStatus, *usb_Msc0DICtrl);
							*usb_Msc0DICtrl = MSC_DIN_FLOW_CTRL;
							//dbg_uas_flow_ctrl();
						}
					}
				}
				
				if (msc_int_status & MSC_DO_TIMEOUT_INT)
				{
					*usb_Msc0IntStatus = MSC_DO_TIMEOUT_INT;
					if ((*usb_Msc0DOXferStatus & MSC_DATAOUT_HOST_WAIT) >= DPE)
					{
						MSG("Do_F %bx\n", *usb_Msc0DOXferStatus);
						*usb_Msc0DOutCtrl = MSC_DOUT_FLOW_CTRL;
					}
				}
			}
#ifdef USING_HW_TIMEOUT						
			if (msc_int_status & (MSC_DATAIN_HUNG|MSC_DATAOUT_HUNG))
			{
				MSG("h %lx\n", msc_int_status);
				*usb_Msc0IntStatus = MSC_DATAIN_HUNG|MSC_DATAOUT_HUNG;
				usb_timeout_handle();
			}
#endif
		}
	}	

	if (msc_int_status & (MSC_RESP_DONE_INT| MSC_TXSENSE_DONE_INT |MSC_RXSENSE_DONE_INT|MSC_TX_DONE_INT|MSC_RX_DONE_INT|MSC_DIN_HALT_INT|MSC_DOUT_HALT_INT))
	{
		usb_msc_isr();
		// when the Sense IU of this command is sent out
		if (re_enum_flag == FORCE_OUT_RE_ENUM)
			return 1;
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
#ifdef UAS
void usb_uas()
{
	// xlate SCSI Read/Wite command to SATA NCQ command 
	u8 site = 0;
	init_usb_registers(MODE_UAS);
	MSG("*UAS*\n");
	uas_init();
	taskCtxtList.curTaskCtxt = NULL;
	taskCtxtList.taskCtxt_que = NULL;
	uas_RIU_active = 0;
	
	DBG("%lx, %lx\n", *usb_CoreCtrl, *usb_LinkCtrl);

	// Enable "Setup Device Request" interrupt of EP 0
	*usb_Msc0IntEn = (MSC_TASK_AVAIL_INT|MSC_RESP_DONE_INT|MSC_TXSENSE_DONE_INT|MSC_RXSENSE_DONE_INT|MSC_TX_DONE_INT|MSC_RX_DONE_INT|MSC_DIN_HALT_INT|MSC_DOUT_HALT_INT| MSC_ST_TIMEOUT_INT|MSC_DO_TIMEOUT_INT);

	if (sata0Obj.sobj_device_state != SATA_NO_DEV)
	{
		sata_ch_reg = sata0Obj.pSata_Ch_Reg;
		sata_ch_reg->sata_FrameIntEn |= SETDEVBITSIEN|DMASETUPIEN|D2HFISIEN;
		sata_ch_reg->sata_BlkCtrl &= ~AUTO_COMMAND_EXEC;
		sata_ch_reg->sata_build_Opts = AUTO_COMMAND_LOAD;
		DBG("%lx, %lx\n", sata_ch_reg->sata_build_Opts, sata_ch_reg->sata_BlkCtrl);	
		DBG("tx %bx\n", *usb_Msc0DICtrl);
	}

	if (sata1Obj.sobj_device_state != SATA_NO_DEV)
	{
		sata_ch_reg = sata1Obj.pSata_Ch_Reg;
		sata_ch_reg->sata_FrameIntEn |= SETDEVBITSIEN|DMASETUPIEN|D2HFISIEN;
		sata_ch_reg->sata_BlkCtrl &= ~AUTO_COMMAND_EXEC;
		sata_ch_reg->sata_build_Opts = AUTO_COMMAND_LOAD;
		DBG("%lx, %lx\n", sata_ch_reg->sata_build_Opts, sata_ch_reg->sata_BlkCtrl);	
		DBG("tx %bx\n", *usb_Msc0DICtrl);
	}
	*usb_IntStatus = USB_SUSPEND;
	
#if (PWR_SAVING)
	turn_on_pwr = 1;
#endif

	u32 temp_timer = 0;
	while(1)
	{
		if (*cpu_wake_up & CPU_USB_SUSPENDED)
		{
			if ( usb_suspend() )
				return;
			continue;
		}
		
		if ((power_off_commanded)
#ifdef HARDWARE_RAID
			|| ((raid_config_button_pressed) && (!hw_raid_enum_check()))
#endif			
#ifdef SUPPORT_HR_HOT_PLUG
			|| (hot_plug_enum)
#endif
			)
		{
			DBG("power off\n");
			return;
		}
		
		if (mscInterfaceChange)
		{
			return;
		}

		if (*chip_IntStaus & USB_INT)
		{
			u32 usb_int_Status = *usb_IntStatus_shadow;		

			// check USB VBUS
			//if (usb_int_Status & VBUS_OFF)
			if (USB_VBUS_OFF())
			{
				return;
			}

			// USB 2.0 BUS_RST--------------
			if ((usb_int_Status & (HOT_RESET|WARM_RESET|USB_BUS_RST)) || (*usb_Msc0IntStatus_shadow & BOT_RST_INT))
			{
				MSG("RST\n");
#ifdef LA_DUMP				
				if (usb_int_Status & WARM_RESET);
					dump_U3_PM_LA();
#endif					
				return;
			}


			if (usb_int_Status & CTRL_INT)
			{	
				// EP0_SETUP----------------
				if (*usb_Ep0Ctrl & EP0_SETUP)
				{	
					DBG("Setup\n");
					usb_control();
					continue;
				}
			}

			if (usb_int_Status & CDB_AVAIL_INT)
			{// fetch out the CDB as soon as possible
#ifdef DBG_PERFORMANCE
				*gpioDataOut &= ~(GPIO8_PIN|GPIO9_PIN|GPIO10_PIN|GPIO11_PIN);
#endif
#ifdef USB2_L1_PATCH
				reject_USB2_L1_IN_IO_transfer();
#endif
				site = *usb_CtxtAvailFIFO;
				PCDB_CTXT pCtxt = (PCDB_CTXT)(HOST_CONTEXT_ADDR + ((u32)site << 6));
				pCtxt->CTXT_Index = site;

				pCtxt->CTXT_usb_state = CTXT_USB_STATE_RECEIVE_CIU;
				*usb_IntStatus = CDB_AVAIL_INT;

				if (chk_valid_tagID(pCtxt->CTXT_ITAG))
				{// throw away the Ctxt because there's no valid TagID to respond Status
					uas_abort_ctxt(pCtxt);
					continue; 
				}
				
				// in UAS mode, the word definition is as below:
				// 15:8 is the overlapped site number
				// bit 2 is Overlapped command
				// bit_1 is overlapped Tag	
				// bit_0 is CDBXFERLENGTH_ZERO
				DBG("c %bx,s %bx, t %bx, f %bx\n", pCtxt->CTXT_CDB[0], site,(u8)pCtxt->CTXT_ITAG, pCtxt->CTXT_SAT_FLAG);
				if (pCtxt->CTXT_SAT_FLAG & CTXT_SAT_FLAG_OVERLAP)
				{
					MSG("PhasC %x\n", pCtxt->CTXT_PHASE_CASE);
					// search the 
					u8 overlap_flag = 0;
					if (pCtxt->CTXT_PHASE_CASE & (CTXT_TAG_OVERLAP_CMD|CTXT_TAG_OVERLAP_TASK))
					{
						overlap_flag = 1;
					}
					else
					{
						if (taskCtxtList.curTaskCtxt)
						{
							if (pCtxt->CTXT_ITAG == taskCtxtList.curTaskCtxt->TASK_CTXT_ITAG)
							{
								overlap_flag = 1;
							}
						}

						PTASK_CTXT pTaskCtxt = taskCtxtList.taskCtxt_que;
						MSG("ch ovl\n");
						while (pTaskCtxt)
						{
							if (pCtxt->CTXT_ITAG == pTaskCtxt->TASK_CTXT_ITAG)
							{	
								overlap_flag = 1;
							}
							pTaskCtxt = pTaskCtxt->taskCtxt_Next;
						}
				
					}
					if (overlap_flag)
					{
						// there's an open issue that the hardware set the phase case error without the overlap CMD or Task set
						TASK_CTXT l_task_ctxt;
						l_task_ctxt.TASK_CTXT_FUNCTION = I_T_NEXUS_RESET;
						uas_abort_function(&l_task_ctxt);
						uas_abort_ctxt(pCtxt);
						uas_abort_TIU_list();
						uas_Respond_RIU(pCtxt->CTXT_ITAG, OVERLAPPED_TAG_ATTEMPTED, RIU_TRANSMITION_NO_TIU_SITE);
						continue;
					}
				}
				// Que the new arrived CIU to USB Que
#ifdef DBG_PERFORMANCE
				*gpioDataOut = *gpioDataOut & ~(GPIO9_PIN|GPIO10_PIN|GPIO11_PIN) | GPIO8_PIN;
#endif
				usb_que_ctxt(pCtxt);	
//				if (pCtxt->CTXT_CDB[0] == SCSI_SYNCHRONIZE_CACHE)
//					dbg_next_cmd = 3;
				DBG("c %bx, s %bx, t %bx\n", pCtxt->CTXT_CDB[0], site, (u8)pCtxt->CTXT_ITAG);

				if (usb_ctxt.newCtxt == NULL)
					usb_ctxt.newCtxt = pCtxt; //new CIU has not been executed
			}
#ifdef DBG_PERFORMANCE
			*gpioDataOut = *gpioDataOut & ~(GPIO8_PIN|GPIO9_PIN | GPIO10_PIN) | GPIO11_PIN;
#endif
		}	// if (chip_IntStaus & USB_INT

		if (*usb_IntStatus_shadow & MSC0_INT)
		{
			if (uas_MSC_isr())
				return;
		}
		
		if (*chip_IntStaus & SATA0_INT)
		{	
			sata_isr(&sata0Obj);
		}

		if (*usb_IntStatus_shadow & MSC0_INT)
		{
			if (uas_MSC_isr())
				return;
		}

		if (*chip_IntStaus & SATA1_INT)
		{	
			sata_isr(&sata1Obj);
		}

		switch (uas_cmd_state)
		{
		case UAS_CMD_STATE_IDLE:
			if (uas_ci_paused == 0)
			{
				if (usb_ctxt.newCtxt != NULL)
				{
#ifdef DBG_PERFORMANCE
					*gpioDataOut = *gpioDataOut & ~(GPIO8_PIN|GPIO9_PIN | GPIO11_PIN) | GPIO10_PIN;
#endif
					usb_pwr_func();

					if ((sata0Obj.sobj_init == 0) || (sata1Obj.sobj_init == 0))
					{
						if (usb_QEnum())
							goto _CHK_SATA_INT;
					}
					
					new_dispatched_CIU = usb_ctxt.newCtxt;
					usb_ctxt.newCtxt = new_dispatched_CIU->CTXT_Next;
					
					if (new_dispatched_CIU->CTXT_usb_state == CTXT_USB_STATE_RECEIVE_CIU)
					{// usb ctxt state is not initialized
						new_dispatched_CIU->CTXT_ccmIndex = CCM_NULL;
						*((u16 *)(&(new_dispatched_CIU->CTXT_Status))) = (LUN_STATUS_GOOD << 8)|(CTXT_STATUS_PENDING);

						// dditional CDB_Length is CIU byte 6 [7:2]
						// additional CDB_Length shall be not great than 0
						if (new_dispatched_CIU->CTXT_ADDITIONAL_CDBLENGTH & 0xFC)  
							new_dispatched_CIU->CTXT_FLAG |= CTXT_SAT_FLAG_INVALID_IU;

						new_dispatched_CIU->CTXT_DbufIndex = SEG_NULL;
						new_dispatched_CIU->CTXT_CONTROL = 0;
					}
					
					if (new_dispatched_CIU->CTXT_SAT_FLAG & (CTXT_SAT_FLAG_INVALID_IU|CTXT_SAT_FLAG_LUN_ERR))
					{
#ifdef UAS_READ_PREFECTH
						if (prefetched_rdCmd_ctxt == new_dispatched_CIU)
						{
							prefetched_rdCmd_ctxt = NULL;
						}
#endif

						usb_deQue_ctxt(new_dispatched_CIU);
						if (new_dispatched_CIU->CTXT_SAT_FLAG & CTXT_SAT_FLAG_INVALID_IU)
						{
							MSG("Inv IU\n");
							uas_Respond_RIU(new_dispatched_CIU->CTXT_ITAG, InvalidInformationUnit, RIU_TRANSMITION_NO_TIU_SITE);
							uas_abort_ctxt(new_dispatched_CIU);
						}
						else if (new_dispatched_CIU->CTXT_SAT_FLAG & CTXT_SAT_FLAG_LUN_ERR)
						{
							MSG("Inv LUN\n");
							uas_Respond_RIU(new_dispatched_CIU->CTXT_ITAG, INCORRECT_LOGICAL_UNIT_NUMBER, RIU_TRANSMITION_NO_TIU_SITE);
							uas_abort_ctxt(new_dispatched_CIU);
						}
						continue;
					}	
					
					if (
#ifdef WDC_CHECK	
						(product_model == ILLEGAL_BOOT_UP_PRODUCT) || 
#endif
#ifdef DATABYTE_RAID
						(cfa_active == 1) ||
#endif
						(arrayMode == NOT_CONFIGURED) || 
						((logicalUnit == HDD0_LUN) && ((array_status_queue[0] == AS_NOT_CONFIGURED) || (array_status_queue[0] == AS_BROKEN))) ||
						((logicalUnit == HDD1_LUN) && ((array_status_queue[1] == AS_NOT_CONFIGURED) || (array_status_queue[1] == AS_BROKEN))))
					{
						if (check_IILegal_boot(new_dispatched_CIU))
							continue;
					}

#ifdef DBG_FUNCTION
					if (dbg_next_cmd)
					{
						dbg_next_cmd--;
						MSG("c %bx, l %bx, t %bx\n", new_dispatched_CIU->CTXT_CDB[0], new_dispatched_CIU->CTXT_LUN, (u8)new_dispatched_CIU->CTXT_ITAG);
					}
#endif
#ifdef WIN8_UAS_PATCH
					if (intel_SeqNum_monitor)
					{
						if (intel_SeqNum_monitor & INTEL_SEQNUM_CHECK_CONDITION)
						{
							if (--intel_SeqNum_Monitor_Count == 0)
								intel_SeqNum_monitor = 0;
							else
								intel_SeqNum_monitor &= ~INTEL_SEQNUM_CHECK_CONDITION;
						}
					}
#endif
					uas_cmd_state = UAS_CMD_STATE_CDB_CHECK;
				}
			}
			else
			{
				uas_cmd_state = UAS_CMD_STATE_CIU_PAUSED;
			}
			break;
			
		case UAS_CMD_STATE_CDB_CHECK:
			uas_cmd_state = UAS_CMD_STATE_IDLE;
			logicalUnit = new_dispatched_CIU->CTXT_LUN; 
			if (new_dispatched_CIU->CTXT_usb_state == CTXT_USB_STATE_RECEIVE_CIU)
			{
				new_dispatched_CIU->CTXT_usb_state = CTXT_USB_STATE_CTXT_SETUP_DONE;
			}

			if ((logicalUnit == HDD0_LUN) || (logicalUnit == HDD1_LUN))
			{
				switch (new_dispatched_CIU->CTXT_CDB[0])
				{
					case SCSI_READ6:
					case SCSI_READ10:
					case SCSI_READ12:
						if (new_dispatched_CIU->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
						{
							translate_rw_cmd_to_rw16(new_dispatched_CIU);
						}
					
					/****************************************\
					SCSI_READ16
					\****************************************/
					case SCSI_READ16:
					{
						DBG("process read\n");
						if (new_dispatched_CIU->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
						{
							if (scsi_chk_valid_rw_cmd(new_dispatched_CIU))
							{
								uas_cmd_state = UAS_CMD_STATE_IDLE;
								break;
							}
						}
#ifdef AES_EN	
						if (vital_data[logicalUnit].cipherID != CIPHER_NONE)
						{
							if ((vital_data[logicalUnit].cipherID == CIPHER_AES_256_XTS) || (vital_data[logicalUnit].cipherID == CIPHER_AES_128_XTS))
							{
								new_dispatched_CIU->CTXT_FLAG = CTXT_FLAG_AES|CTXT_FLAG_XTS;
							}
							else
							{
								new_dispatched_CIU->CTXT_FLAG = CTXT_FLAG_AES;
							}
							new_dispatched_CIU->CTXT_DbufIndex = ((logicalUnit == HDD0_LUN) && (sata0Obj.sobj_device_state != SATA_NO_DEV)) ? DBUF_SEG_U2S0R_AES : DBUF_SEG_U2S1R_AES;
						}
						else
#endif
						{
							new_dispatched_CIU->CTXT_FLAG = 0;
							new_dispatched_CIU->CTXT_DbufIndex = ((logicalUnit == HDD0_LUN) && (sata0Obj.sobj_device_state != SATA_NO_DEV)) ? DBUF_SEG_U2S0R : DBUF_SEG_U2S1R;
						}

						new_dispatched_CIU->CTXT_CONTROL = CTXT_CTRL_DIR;			// Data-in
						new_dispatched_CIU->CTXT_Prot = PROT_FPDMAIN;
						new_dispatched_CIU->CTXT_ccm_cmdinten = 0; 
						new_dispatched_CIU->CTXT_Status = CTXT_STATUS_PENDING;
						rw_flag = READ_FLAG;
						if (usbMode == CONNECT_USB3)
						{
							rw_flag |= RW_IN_PROCESSING;
							rw_time_out = 80;
						}
						uas_cmd_state = UAS_CMD_STATE_CIU_DISPATCH;
						new_dispatched_CIU->CTXT_usb_state = CTXT_USB_STATE_AWAIT_SATA_READY;
					}
					break;
				

					/****************************************\
						SCSI_WRITE6
					\****************************************/
					case SCSI_WRITE6:
					case SCSI_WRITE10:
					case SCSI_WRITE12:
						if (new_dispatched_CIU->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
						{
							translate_rw_cmd_to_rw16(new_dispatched_CIU);
						}
					case SCSI_WRITE16:
						if (new_dispatched_CIU->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
						{
							if (scsi_chk_valid_rw_cmd(new_dispatched_CIU))
							{
								break;
							}
						}
#ifdef AES_EN	
						if (vital_data[logicalUnit].cipherID != CIPHER_NONE)
						{
							if ((vital_data[logicalUnit].cipherID == CIPHER_AES_256_XTS) || (vital_data[logicalUnit].cipherID == CIPHER_AES_128_XTS))
							{
								new_dispatched_CIU->CTXT_FLAG		= CTXT_FLAG_AES|CTXT_FLAG_XTS;
							}
							else
							{
								new_dispatched_CIU->CTXT_FLAG		= CTXT_FLAG_AES;
							}
							new_dispatched_CIU->CTXT_DbufIndex = ((logicalUnit == HDD0_LUN) && (sata0Obj.sobj_device_state != SATA_NO_DEV)) ? DBUF_SEG_U2S0W_AES : DBUF_SEG_U2S1W_AES;
							*usb_Msc0DOutCtrl = MSCn_DATAOUT_SDONE_DISABLE;
						}
						else
#endif
						{
							new_dispatched_CIU->CTXT_FLAG = 0;
							new_dispatched_CIU->CTXT_DbufIndex = ((logicalUnit == HDD0_LUN) && (sata0Obj.sobj_device_state != SATA_NO_DEV)) ? DBUF_SEG_U2S0W : DBUF_SEG_U2S1W;
						}
						// Data-Out
						new_dispatched_CIU->CTXT_Prot = PROT_FPDMAOUT;
						new_dispatched_CIU->CTXT_ccm_cmdinten = 0; 
						new_dispatched_CIU->CTXT_Status = CTXT_STATUS_PENDING;
						uas_cmd_state = UAS_CMD_STATE_CIU_DISPATCH;
						new_dispatched_CIU->CTXT_usb_state = CTXT_USB_STATE_AWAIT_SATA_READY;
						break;
					
					case SCSI_WDC_SECURITY_CMD:
					case SCSI_SEND_DIAGNOSTIC:
					case SCSI_RCV_DIAG_RESULTS:
					case SCSI_LOG_SENSE:
					case SCSI_MODE_SELECT6:
					case SCSI_MODE_SELECT10:
					case SCSI_WDC_FACTORY_CONFIG:
					case SCSI_SYNCHRONIZE_CACHE:
					case SCSI_SYNCHRONIZE_CACHE16:
                    case SCSI_ATA_PASS_THR:
                    case SCSI_ATA_PASS_THR16:
					case SCSI_START_STOP_UNIT:
						DBG("chk st %lx\n", pSataObj->sobj_State);
						if ((sata0Obj.sobj_State > SATA_READY) || (sata1Obj.sobj_State > SATA_READY) || (usb_ctxt.curCtxt))
						{
							uas_cmd_state = UAS_CMD_STATE_CIU_PAUSED;
							uas_CIU_pending(new_dispatched_CIU);
							break;
						}

						/****************************************\
						other commands
						READ6, WRITE6, VERIFY, INQUIRY, READ_CAPACITY,
						REQUEST_SENSE, MODE_SENSE6, MODE_SELECT6, WRITE_BUFFER10, READ_BUFFER10,
						TEST_UNIT_READY, SYNCHRONIZE_CACHE, REZERO, PREVENT_MEDIUM_REMOVAL,
						START_STOP_UNIT, and FORMAT_UNIT
						\****************************************/

					default:
						if ((logicalUnit == HDD0_LUN) && (sata0Obj.sobj_device_state != SATA_NO_DEV))
							pSataObj = &sata0Obj;
						else
							pSataObj = &sata1Obj;	
						uas_cmd_state = UAS_CMD_STATE_CIU_PAUSED;
						scsi_StartAtaCmd(pSataObj, new_dispatched_CIU);
	//					*usb_IntStatus = CDB_AVAIL_INT|MSC0_INT;
				}	// switch (pCtxt->CTXT_CDB[0])
			}
#ifdef WDC_VCD
			else if (logicalUnit == VCD_LUN)
			{
				switch (new_dispatched_CIU->CTXT_CDB[0])
				{
					case SCSI_RCV_DIAG_RESULTS:
					case SCSI_MODE_SELECT6:
					case SCSI_MODE_SELECT10:
					case SCSI_WDC_READ_VIRTUAL_CD: 
					case SCSI_WDC_WRITE_VIRTUAL_CD:
					/*case SCSI_READ_CD:		// --->v0.07_14.22_0327_A */
					case SCSI_READ12:
					case SCSI_READ10:
					case SCSI_SYNCHRONIZE_CACHE:
					case SCSI_SYNCHRONIZE_CACHE16:
					case SCSI_START_STOP_UNIT:
						if ((sata0Obj.sobj_State > SATA_READY) || (sata1Obj.sobj_State > SATA_READY) || (usb_ctxt.curCtxt))
						{
							uas_cmd_state = UAS_CMD_STATE_CIU_PAUSED;
							uas_CIU_pending(new_dispatched_CIU);
							break;
						}

					default: 
						uas_cmd_state = UAS_CMD_STATE_CIU_PAUSED;
						vcd_start_command(new_dispatched_CIU);
						break;
				}
			}
#endif
#ifdef WDC_SES
			else if (logicalUnit == SES_LUN)
			{
				switch (new_dispatched_CIU->CTXT_CDB[0])
				{
					case SCSI_WDC_SECURITY_CMD:
					case SCSI_SYNCHRONIZE_CACHE:
					case SCSI_SYNCHRONIZE_CACHE16:
					case SCSI_START_STOP_UNIT:
					case SCSI_SEND_DIAGNOSTIC:
					case SCSI_RCV_DIAG_RESULTS:
					case SCSI_LOG_SENSE:
					case SCSI_MODE_SELECT6:
					case SCSI_MODE_SELECT10:							
					case SCSI_WDC_READ_VIRTUAL_CD:
					case SCSI_WDC_WRITE_VIRTUAL_CD:
					case SCSI_WDC_WRITE_HANDY_STORE:
					case SCSI_WDC_READ_HANDY_STORE:
					//case SCSI_FORMAT_DISK:
						if ((sata0Obj.sobj_State > SATA_READY) || (sata1Obj.sobj_State > SATA_READY) || (usb_ctxt.curCtxt))
						{
							uas_CIU_pending(new_dispatched_CIU);
							uas_cmd_state = UAS_CMD_STATE_CIU_PAUSED;
							break;
						}

					default: 
						uas_cmd_state = UAS_CMD_STATE_CIU_PAUSED;
						ses_start_command(new_dispatched_CIU);
						break;
				}
			}
#endif
			else
			{
				scsi_cmd_unsupport_lun(new_dispatched_CIU);
				uas_cmd_state = UAS_CMD_STATE_CIU_PAUSED;
			}				
			break;
			
		case UAS_CMD_STATE_CIU_DISPATCH:
			logicalUnit = new_dispatched_CIU->CTXT_LUN; 
			uas_cmd_state = UAS_CMD_STATE_IDLE;
			new_dispatched_CIU->CTXT_usb_state = CTXT_USB_STATE_CTXT_SETUP_DONE;
			switch(new_dispatched_CIU->CTXT_CDB[0])
			{
			case SCSI_READ6:
			case SCSI_READ10:
			case SCSI_READ12:
			case SCSI_READ16:
                if ((arrayMode == RAID0_STRIPING_DEV)
        #ifdef SUPPORT_SPAN
                    || (arrayMode == SPANNING)
        #endif
                    )
                {
        #ifndef DATASTORE_LED
					new_dispatched_CIU->CTXT_FLAG |= CTXT_FLAG_RAID_EN;
					u32 val = raid_exec_ctxt(new_dispatched_CIU);
					if (val == CTXT_STATUS_ERROR)
					{
						uas_ci_paused |= UAS_PAUSE_WAIT_FOR_USB_RESOURCE;
						uas_ci_paused_ctxt = new_dispatched_CIU;
						new_dispatched_CIU->CTXT_LunStatus = LUN_STATUS_BUSY;
						new_dispatched_CIU->CTXT_Status = CTXT_STATUS_ERROR;
						usb_device_no_data(new_dispatched_CIU);
					}
					else if (val == CTXT_STATUS_PAUSE)
					{
						uas_cmd_state = UAS_CMD_STATE_CIU_PAUSED;
						break;
					}
					else
					{
						new_dispatched_CIU->CTXT_usb_state = CTXT_USB_STATE_AWAIT_SATA_DMASETUP;
					}
        #endif //DATASTORE_LED
				}
				else
				{
#if 1   
        
					if (arrayMode == RAID1_MIRROR)
					{	// use the ping pong read/write
        #ifdef DATASTORE_LED
						if (array_status_queue[0] == AS_GOOD)
						{
							pSataObj = &sata0Obj;
						 	// but does not toggle to the sata1 always to avoid to disturb the sequential read/write
							if ( raid1_active_timer == 0 )
							{
								if (sata1Obj.sobj_device_state == SATA_DEV_READY)
								{
									pSataObj = &sata1Obj;
									new_dispatched_CIU->CTXT_DbufIndex |= DBUF_SATA1_CHANNEL;
								}
							}
						}
						else
						{

							if (raid1_use_slot == 0) 
								pSataObj = &sata0Obj;
							else
							{
								pSataObj = &sata1Obj;
								new_dispatched_CIU->CTXT_DbufIndex |= DBUF_SATA1_CHANNEL;
							}
						}
        #endif //DATASTORE_LED                       
					}
					else
#endif		
					{//JBOD
						if ((logicalUnit == HDD0_LUN) && (sata0Obj.sobj_device_state != SATA_NO_DEV))
						{
							pSataObj = &sata0Obj;
						}
						else
						{
							pSataObj = &sata1Obj;
						}
       
					}
					if (sata_exec_ncq_ctxt(pSataObj, new_dispatched_CIU) == CTXT_STATUS_ERROR)
					{	// sense Sense IU if error return 
						if (pSataObj->sobj_sata_ch_seq)
						{
							uas_ci_paused |= UAS_PAUSE_SATA1_NCQ_NO_ENOUGH_TAGS;
						}
						else
						{
							uas_ci_paused |= UAS_PAUSE_SATA0_NCQ_NO_ENOUGH_TAGS;
						}
						uas_ci_paused_ctxt = new_dispatched_CIU;
						new_dispatched_CIU->CTXT_Status = CTXT_STATUS_ERROR;
						new_dispatched_CIU->CTXT_LunStatus = LUN_STATUS_BUSY;
						usb_device_no_data(new_dispatched_CIU);
					}
					else
					{
						new_dispatched_CIU->CTXT_usb_state = CTXT_USB_STATE_AWAIT_SATA_DMASETUP;
					}
  
				}

				break;
				
			case SCSI_WRITE6:
			case SCSI_WRITE10:
			case SCSI_WRITE12:
			case SCSI_WRITE16:
				if (arrayMode != JBOD)
				{
					new_dispatched_CIU->CTXT_FLAG |= CTXT_FLAG_RAID_EN;
					u32 val = raid_exec_ctxt(new_dispatched_CIU);
					if (val == CTXT_STATUS_ERROR)
					{
						uas_ci_paused |= UAS_PAUSE_WAIT_FOR_USB_RESOURCE;
						uas_ci_paused_ctxt = new_dispatched_CIU;
						new_dispatched_CIU->CTXT_LunStatus = LUN_STATUS_BUSY;
						new_dispatched_CIU->CTXT_Status = CTXT_STATUS_ERROR;
//								usb_append_rx_que_ctxt(pCtxt);
						usb_device_no_data(new_dispatched_CIU);
					}
					else if (val == CTXT_STATUS_PAUSE)
					{
						uas_cmd_state = UAS_CMD_STATE_CIU_PAUSED;
						break;
					}
					else
					{
						new_dispatched_CIU->CTXT_usb_state = CTXT_USB_STATE_AWAIT_SATA_DMASETUP;
					}
				}
				else
				{
					if ((logicalUnit == HDD0_LUN) && (sata0Obj.sobj_device_state != SATA_NO_DEV))
						pSataObj = &sata0Obj;
					else
						pSataObj = &sata1Obj;
					if (sata_exec_ncq_ctxt(pSataObj, new_dispatched_CIU) == CTXT_STATUS_ERROR)
					{	// sense Sense IU if error return 
						if (pSataObj->sobj_sata_ch_seq)
						{
							uas_ci_paused |= UAS_PAUSE_SATA1_NCQ_NO_ENOUGH_TAGS;
						}
						else
						{
							uas_ci_paused |= UAS_PAUSE_SATA0_NCQ_NO_ENOUGH_TAGS;
						}
						uas_ci_paused_ctxt = new_dispatched_CIU;
						new_dispatched_CIU->CTXT_LunStatus = LUN_STATUS_BUSY;
						new_dispatched_CIU->CTXT_Status = CTXT_STATUS_ERROR;
						usb_device_no_data(new_dispatched_CIU);
					}
					else
					{
						new_dispatched_CIU->CTXT_usb_state = CTXT_USB_STATE_AWAIT_SATA_DMASETUP;
					}
				}
				break;
			}
			break;	

		case UAS_CMD_STATE_CIU_PAUSED:
			if (uas_ci_paused & (UAS_PAUSE_SATA0_NOT_READY | UAS_PAUSE_SATA1_NOT_READY | UAS_PAUSE_WAIT_FOR_USB_RESOURCE))
			{
				if (uas_ci_paused & UAS_PAUSE_WAIT_FOR_USB_RESOURCE)
				{
					if ((( sata0Obj.sobj_State == SATA_READY) || (sata0Obj.sobj_device_state == SATA_NO_DEV)) && ((sata1Obj.sobj_State == SATA_READY) || (sata1Obj.sobj_device_state == SATA_NO_DEV))&& (usb_ctxt.curCtxt == NULL))
					{
						uas_ci_paused &= ~(UAS_PAUSE_SATA0_NOT_READY |UAS_PAUSE_SATA1_NOT_READY | UAS_PAUSE_WAIT_FOR_USB_RESOURCE);
					}
				}
				else
				{
					if (( sata0Obj.sobj_State == SATA_READY) || (sata0Obj.sobj_device_state == SATA_NO_DEV))
					{
						uas_ci_paused &= ~UAS_PAUSE_SATA0_NOT_READY;
					}
					if ((sata1Obj.sobj_State == SATA_READY) || (sata1Obj.sobj_device_state == SATA_NO_DEV))
					{
						uas_ci_paused &= ~UAS_PAUSE_SATA1_NOT_READY;
					}
				}
			}
			else if (uas_ci_paused & (UAS_PAUSE_SATA0_NCQ_NO_ENOUGH_TAGS | UAS_PAUSE_SATA1_NCQ_NO_ENOUGH_TAGS))
			{
				if (sata0Obj.cFree != 0)
				{	
					uas_ci_paused &= ~UAS_PAUSE_SATA0_NCQ_NO_ENOUGH_TAGS;
				}
				if (sata1Obj.cFree != 0)
				{
					uas_ci_paused &= ~UAS_PAUSE_SATA1_NCQ_NO_ENOUGH_TAGS;
				}
			}	
			if (uas_ci_paused == 0)
			{
				uas_cmd_state = UAS_CMD_STATE_IDLE;
			}
			break;
		}

_CHK_SATA_INT:
		if (*usb_IntStatus_shadow & MSC0_INT)
		{
			if (uas_MSC_isr())
				return;
		}
		
		if (*chip_IntStaus & SATA0_INT)
		{	
			sata_isr(&sata0Obj);
		}

		if (*usb_IntStatus_shadow & MSC0_INT)
		{
			if (uas_MSC_isr())
				return;
		}

		if (*chip_IntStaus & SATA1_INT)
		{	
			sata_isr(&sata1Obj);
		}	
#ifdef DBG_PERFORMANCE		
		*gpioDataOut = *gpioDataOut & (~GPIO11_PIN) | (GPIO8_PIN|GPIO9_PIN|GPIO10_PIN);
#endif
		usb_chk();
#ifdef DBG_PERFORMANCE
		*gpioDataOut = *gpioDataOut |(GPIO8_PIN|GPIO9_PIN|GPIO10_PIN |GPIO11_PIN);
#endif
	} // End of while loop
}
#endif

