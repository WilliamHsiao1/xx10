/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2004-2013, All Rights Reserved
 *
 *   This file contains confidential and proprietary information
 *   which is the property of Initio Corporation.
 *
 *******************************************************************************/


#define SATA_C
#include	"general.h"

// allocate & release mechanism
// when the bit is 1, it's free CCM, otherwise, it has been released
u32 sata_AllocateCCM(PSATA_OBJ  pSob)
{
	u32			i, bit;

	DBG("alloc cFree: %LX\n", pSob->cFree);

	for (i= 0, bit = 1; bit != 0; i++, bit= bit<< 1)
	{
		if((pSob->cFree & bit))
		{
			pSob->cFree &= ~bit;
			return i;
		}
	}
	ERR("get ccm fail");
	return CCM_NULL;
}

void sata_DetachCCM(PSATA_OBJ  pSob, u32 SActive)
{
	u32			bit;
	PEXT_CCM	pScm;

	pScm = &(pSob->sobj_Scm[0]);
	DBG("free tag sactive: %LX\n", SActive);
	for (bit = 1; bit != 0; bit= bit<< 1, pScm++)
	{
		if (SActive & bit)
		{
			pScm->scm_next = NULL;
			pScm->scm_pCdbCtxt = NULL;
//			pScm->scm_site = NULL;
//			pScm->scm_prot = NULL;
			pSob->cFree |= bit;
			SActive &= ~bit;
			if (SActive == 0)
				return;
		}
	}
}
void sata_Cleanup_NCQCMD(PSATA_OBJ  pSob, u32 SActive)
{
	u32			bit;
	PEXT_CCM	pScm;

	pScm = &(pSob->sobj_Scm[0]);
	for (bit = 1; bit != 0; bit= bit<< 1, pScm++)
	{
		if (SActive & bit)
		{
			if (pScm != NULL)
			{
				PCDB_CTXT pCtxt = pScm->scm_pCdbCtxt;
				if (pCtxt)
				{// find the USB command, and abort them
					MSG("c %bx, s %bx, t %bx sbsy%lx\n", pCtxt->CTXT_CDB[0], pCtxt->CTXT_Index, (u8)pCtxt->CTXT_ITAG, pSob->sobj_sBusy);
					pCtxt->CTXT_usb_state = CTXT_USB_STATE_CTXT_SETUP_DONE;
					hdd_err_2_sense_data(pCtxt, ERROR_DATA_PHASE_ABORTED);
					usb_device_no_data(pCtxt);
				}
				pScm->scm_next = NULL;
				pSob->cFree |= bit;
				SActive &= ~bit;
				if (SActive == 0)
					return;
			}
		}
	}	
}
static void delete_curSCM(PSATA_OBJ  pSob)
{
	PEXT_CCM pScm = pSob->sobj_curScm;
	if (pScm != NULL)
	{
		u32 bit32 = 1 << pScm->scm_ccmIndex;
		pSob->sobj_sBusy &= ~bit32;
		sata_DetachCCM(pSob, bit32);						
	}
	pSob->sobj_curScm = NULL;
}

/****************************************\
 sata_abort_ctxt

\****************************************/
u32 sata_abort_ctxt(PSATA_OBJ  pSob, u16 tid)
{
	PEXT_CCM pScm, pPrevScm = NULL;

	// search non-NCQ queue
	if ((pScm = pSob->sobj_que))
	{
		PCDB_CTXT pCtxt;
		while (pScm)
		{
			if ((pCtxt = pScm->scm_pCdbCtxt))
			{
				// found match TID
				if (pCtxt->CTXT_ITAG == tid)
				{	// un-queue pScm
					if ((pScm == pSob->sobj_que))
					{
						pSob->sobj_que = pScm->scm_next ;
					}
					else
					{
			
						pPrevScm->scm_next = pScm->scm_next;
					}
					pScm->scm_pCdbCtxt = NULL;
					sata_DetachCCM(pSob, 1 << pScm->scm_ccmIndex); //

//					pScm->scm_pCdbCtxt = NULL;
					uas_abort_ctxt(pCtxt);
					return 1;
				}
			}
			pPrevScm = pScm;
			pScm = pScm->scm_next;
		}
	}

	// search NCQ queue
	if ((pScm = pSob->sobj_que))
	{
		PCDB_CTXT pCtxt;
		while (pScm)
		{
			if ((pCtxt = pScm->scm_pCdbCtxt))
			{
				// found match TID
				if (pCtxt->CTXT_ITAG == tid)
				{	// un-queue pScm
					if ((pScm == pSob->sobj_que))
					{
						pSob->sobj_que = pScm->scm_next ;
					}
					else
					{
						pPrevScm->scm_next = pScm->scm_next;
					}
					pScm->scm_pCdbCtxt = NULL;
					sata_DetachCCM(pSob, 1 << pScm->scm_ccmIndex); //

					//	pCtxt->CTXT_ccmIndex = CCM_NULL;
					// abort pCtxt 
					uas_abort_ctxt(pCtxt);
					return 1;
				}
			}
			pPrevScm = pScm;
			pScm = pScm->scm_next;
		}
	}
	return 0;
}

u32 sata_abort_all(PSATA_OBJ  pSob)
{
	PEXT_CCM pScm;
	u32		ret = 0;
	MSG("S Ab\n");
	// abort non-NCQ queue
	if ((pScm = pSob->sobj_que))
	{
		ret = 1;

		while (pScm)
		{
			PCDB_CTXT pCtxt;

			DBG("pScm->scm_ccmIndex %bx\n", pScm->scm_ccmIndex);

			if ((pCtxt = pScm->scm_pCdbCtxt))
			{
				pScm->scm_pCdbCtxt = NULL;
				uas_abort_ctxt(pCtxt);
			}
			sata_DetachCCM(pSob, 1 << pScm->scm_ccmIndex); //
			pScm = pScm->scm_next;
		}
		pSob->sobj_que = NULL;
	}

	// abort NCQ queue
	if ((pScm = pSob->sobj_ncq_que))
	{
		ret = 1;

		while (pScm)
		{
			PCDB_CTXT pCtxt;
			DBG("pScm->scm_ccmIndex %bx\n", pScm->scm_ccmIndex);
			// need abort host ctxt, otherwise the pScm->scm_pCdbCtxt will be NULL, can'r release the resource rightly
			if ((pCtxt = pScm->scm_pCdbCtxt))
			{
				pScm->scm_pCdbCtxt = NULL;
				// abort pCtxt 
				uas_abort_ctxt(pCtxt);
			}
			sata_DetachCCM(pSob, 1 << pScm->scm_ccmIndex); //
			pScm = pScm->scm_next;
		}
	}
	if (pSob->sobj_sBusy)
	{
		ret = 1;
		sata_Reset(pSob, SATA_HARD_RST);
	}
	
	return ret;

}

/****************************************\
   sata_exec_scm
\****************************************/
void sata_exec_scm(PSATA_OBJ  pSob, PEXT_CCM pScm)
{
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;
	u32 SegIndex;
	PCDB_CTXT pCtxt;
	if (dbg_next_cmd)
		MSG("exec scm: ccmIndex: %BX, segIndex: %BX\n", 
			pScm->scm_ccmIndex, pScm->scm_SegIndex);
	
	pCtxt = pScm->scm_pCdbCtxt;
	if (pCtxt == NULL)
	{
		MSG("scm ept\n");
		return;
	}

	SegIndex = pScm->scm_SegIndex;

	if ((raid_rebuild_verify_status & RRVS_IN_PROCESS) == 0)  // block the timer in rebuilding or verifing
	{
		raid1_active_timer = RAID_REBUILD_VERIFY_ONLINE_ACTIVE_TIMER;
	}

	if (pCtxt->CTXT_FLAG & CTXT_FLAG_B2S)
	{
		if (SegIndex != SEG_NULL)
		{
			set_dbufConnection(SegIndex);
		}
		sata_ch_reg->sata_EXQHINP = pScm->scm_ccmIndex;	
	}
	else
	{
		if (pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR)
		{
			if (SegIndex != SEG_NULL)
			{	
				// rout Data to Dbuffer 
				set_dbufSource(SegIndex);
//				*((u8 *) &(Tx_Dbuf->dbuf_Seg[SegIndex].dbuf_Seg_INOUT)) =  pScm->scm_Seg_INOUT & 0x0F; // only connect the source 
			}
			sata_ch_reg->sata_EXQHINP = pScm->scm_ccmIndex;

			if ((SegIndex != SEG_NULL) && (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN))
			{
				// wait for another channel is also setted up
				if (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_SATA_REGS_SET_UP_DONE)
				{
					usb_exec_que_ctxt();
				}
			}
			else
			{
				if (rw_flag & READ_FLAG)
				{
					sata_ch_reg->sata_Status |= ATA_STATUS_BSY;
				}
				usb_exec_que_ctxt();
			}
		}
		else
		{
			sata_ch_reg->sata_EXQHINP = pScm->scm_ccmIndex;
			if ((SegIndex != SEG_NULL) && (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN))
			{
				// wait for another channel is also setted up
				if (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_SATA_REGS_SET_UP_DONE)
				{
//					*raid_write_ctrl = RAID_WR_START;
					if (dbg_next_cmd)
						MSG("r %bx\n", raid_xfer_status);
					usb_exec_que_ctxt();
				}
			}
			else
			{
				usb_exec_que_ctxt();
			}
		}
	}

	pScm->scm_next = NULL;
	pSob->sobj_curScm = pScm;
	pSob->sobj_sBusy |= (1 << pScm->scm_ccmIndex);
	if (pSob->sobj_State != SATA_PRE_READY)
		pSob->sobj_State = SATA_ACTIVE;				
}


void sata_append_scm(PSATA_OBJ  pSob, PEXT_CCM pScm)
{
	PEXT_CCM pQScm;
//	if (dbg_next_cmd)
//		MSG("appn scm\n");

	if ((pSob->sobj_State == SATA_STANDBY) ||
		(pSob->sobj_State == SATA_PRE_READY) ||
		(pSob->sobj_State == SATA_READY) ||	
		// issue Read log command to get the failed NCQ Error log
		((pSob->sobj_State >= SATA_NCQ_FLUSH) && (pSob->sobj_subState == SATA_SUB_STATE_NCQ_ERROR)))
	{
		sata_exec_scm(pSob, pScm);
		return;
	}
	if (dbg_next_cmd)
	{
		MSG("appQue\n");
	}
	if (pSob->sobj_State == SATA_NCQ_ACTIVE)
	{
		pSob->sobj_State = SATA_NCQ_FLUSH;
	}	

	// append to non-ncq SCM que
	pScm->scm_next = NULL;
	if ((pQScm = pSob->sobj_que) == NULL)
	{
		pSob->sobj_que = pScm;
	}
	else
	{
		while (pQScm->scm_next)
		{
			pQScm = pQScm->scm_next;
		}
		pQScm->scm_next = pScm;
	}
}

void sata_exec_ncq_scm(PSATA_OBJ  pSob, PEXT_CCM pScm)
{
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;

	sata_ch_reg->sata_EXQHINP = pScm->scm_ccmIndex;
	pScm->scm_next = NULL;
	
	pSob->sobj_sBusy |= (1 << pScm->scm_ccmIndex);
	DBG("ExQH st: %bx\n", sata_ch_reg->sata_EXQHStat);
	if (dbg_next_cmd)
		MSG("exQ %bx\n", pSob->sobj_sata_ch_seq);
	if ((pSob->sobj_State == SATA_READY) || (pSob->sobj_State == SATA_STANDBY))
		pSob->sobj_subState = SATA_SUB_STATE_NCQ_IDLE;
	
	pSob->sobj_State = SATA_NCQ_ACTIVE;	
	raid1_active_timer = RAID_REBUILD_VERIFY_ONLINE_ACTIVE_TIMER;
}

void sata_append_ncq_scm(PSATA_OBJ  pSob, PEXT_CCM pScm)
{
	PEXT_CCM pQScm;
	
	DBG("append_ncq_scm %bx\n", pSob->sobj_State);
	pScm->scm_next = NULL;

	if ((pSob->sobj_State == SATA_STANDBY) ||
		(pSob->sobj_State == SATA_READY) ||
		(pSob->sobj_State == SATA_NCQ_ACTIVE) )
	{
		sata_exec_ncq_scm(pSob, pScm);
		return;
	}
	MSG("appSQ\n");
	if ((pQScm = pSob->sobj_ncq_que) == NULL)
	{
		pSob->sobj_ncq_que = pScm;
	}
	else
	{
		while (pQScm->scm_next)
		{
			pQScm = pQScm->scm_next;
		}
		pQScm->scm_next = pScm;
	}			
}

void sata_return_tfr(PSATA_OBJ  pSob, PCDB_CTXT pCtxt)
{
	*(((u16 *) &(pCtxt->CTXT_FIS[FIS_STATUS]))) =  *((u16 *)(&pSob->pSata_Ch_Reg->sata_Status));
	*(((u32 *) &(pCtxt->CTXT_FIS[FIS_LBA_LOW]))) =  *((u32 *)(&pSob->pSata_Ch_Reg->sata_LbaL));
	*(((u32 *) &(pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT]))) =  *((u32 *)(&pSob->pSata_Ch_Reg->sata_LbaLH));
	*(((u16 *) &(pCtxt->CTXT_FIS[FIS_SEC_CNT]))) =  *((u16 *)(&pSob->pSata_Ch_Reg->sata_SectCnt));
}

u8 sata_chk_status(PSATA_OBJ  pSob, PCDB_CTXT pCtxt, u16 ata_error_status)
{
	if ((ata_error_status & ATA_STATUS_CHECK) && ((ata_error_status & ATA_STATUS_BSY) == 0))
	{
		*(((u16 *) &(pCtxt->CTXT_FIS[FIS_STATUS]))) =  ata_error_status;
		
		dbg_next_cmd = 1;
		MSG("sc Ata");
		if (pSob->sobj_sata_ch_seq)
			MSG("1");
		else
			MSG("0");
		MSG(" err %bx, %bx\n", pCtxt->CTXT_FIS[FIS_STATUS], pCtxt->CTXT_FIS[FIS_ERROR]);
		// generate SCSI Sense code
		if ((pCtxt->CTXT_FLAG & CTXT_FLAG_B2S) == 0)
			hdd_ata_err_2_sense_data(pCtxt);
		return 1;
	}
	else
	{
		if (pCtxt->CTXT_FLAG & CTXT_FLAG_RET_TFR)
		{
			sata_return_tfr(pSob, pCtxt);
			if ((pCtxt->CTXT_FLAG & CTXT_FLAG_B2S) == 0)
				hdd_ata_return_tfr(pCtxt);					
		}					
	}
	return 0;
}

void pop_SCM_usb_Que(PSATA_OBJ  pSob, PEXT_CCM pScm)
{
	PCDB_CTXT pCtxt = pScm->scm_pCdbCtxt;
	pSob->sobj_sBusy &= ~(1 << pScm->scm_ccmIndex);
	MSG("Push Cmd %bx, l %bx, t %bx\n", pCtxt->CTXT_CDB[0], pCtxt->CTXT_LUN, (u8)pCtxt->CTXT_ITAG);
	if (pCtxt != NULL)
	{
		pCtxt->CTXT_Next = NULL;
		// pending que for USB TX Contxt
		PCDB_CTXT pQCtxt = usb_ctxt.ctxt_que;
		if (pQCtxt == NULL)
		{
			 usb_ctxt.ctxt_que = pCtxt;
		}
		else
		{
			while (pQCtxt->CTXT_Next)
			{
				pQCtxt = pQCtxt->CTXT_Next;
			}
			pQCtxt->CTXT_Next = pCtxt;
		}
	}
}

void sata_exec_next_scm(PSATA_OBJ  pSob)
{
	PEXT_CCM	pScm;

	// any pending non-NCQ command ?
	if ((pScm = pSob->sobj_que))
	{
		pSob->sobj_que = pScm->scm_next;
		pScm->scm_next = NULL;
		sata_exec_scm(pSob, pScm);
	}
	else if ((pScm = pSob->sobj_ncq_que))
	{
		pSob->sobj_ncq_que = pScm->scm_next;
		pScm->scm_next = NULL;
		sata_exec_ncq_scm(pSob, pScm);
	}
}

static void sata_recovery_NCQ_error(PSATA_OBJ pSob)
{
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;
	// reset the SATA port & DBUF
	disconnect_SataDbufConnection(pSob->sobj_sata_ch_seq);
	//dbuf_dbg(DBUF_SEG_U2BR);
	//dbuf_dbg(DBUF_SEG_DUMP_TXSEG1234);
	sata_ch_reg->sata_BlkCtrl |= (TXSYNCFIFORST|RXSYNCFIFORST); //reset sata RX  fifo
	sata_ch_reg->sata_BlkCtrl |= DDONEFLUSH_DIS;

	if(pSob->sobj_sata_ch_seq)
		read_QueuedErrLog(SATA_CH1);//  read id 
	else
		read_QueuedErrLog(SATA_CH0);//  read id 

	pSob->sobj_State = SATA_NCQ_FLUSH;
}

static void sata_update_RAID_done_status(PSATA_OBJ  pSob, PCDB_CTXT pCtxt, PEXT_CCM pScm)
{
	if (((pCtxt->CTXT_CONTROL & CTXT_CTRL_SATA0_DONE) == 0) || ((pCtxt->CTXT_CONTROL & CTXT_CTRL_SATA1_DONE) == 0))
	{		
		if (pScm)
		{
			if (pScm->scm_control & SCM_CONTROL_SATA0_EN)
			{
				pCtxt->CTXT_CONTROL |= CTXT_CTRL_SATA0_DONE;
				if (dbg_next_cmd)
					MSG("r d0\n");
			}
			else if (pScm->scm_control & SCM_CONTROL_SATA1_EN)
			{
				pCtxt->CTXT_CONTROL |= CTXT_CTRL_SATA1_DONE;
				if (dbg_next_cmd)
					MSG("r d1\n");
			}

			pScm->scm_SegIndex = SEG_NULL;

			if ((pScm->scm_prot & 0x0C) != 0x08)
			{// if it's DMA transfer, clear the SATA CCM, otherwise, the SCM will be cleared after receive SATA setDevBit
				u32 bit32 = 1 << pScm->scm_ccmIndex;
				sata_DetachCCM(pSob, bit32); //
				pSob->sobj_sBusy &= (~bit32);
				pSob->sobj_State = SATA_READY;
			}
			
			pSob->sobj_curScm = NULL;
		}
	}
	
	if ((pCtxt->CTXT_CONTROL & CTXT_CTRL_SATA0_DONE) && (pCtxt->CTXT_CONTROL & CTXT_CTRL_SATA1_DONE))
	{
		if ( raid_xfer_status & (RAID_XFER_SATA0_NCQ_ERR | RAID_XFER_SATA1_NCQ_ERR))
		{
			if (raid_xfer_status & RAID_XFER_SATA0_NCQ_ERR)
			{
				sata_recovery_NCQ_error(&sata0Obj);
				raid_xfer_status &= ~RAID_XFER_SATA0_NCQ_ERR;
			}
			if (raid_xfer_status & RAID_XFER_SATA1_NCQ_ERR)
			{
				sata_recovery_NCQ_error(&sata1Obj);
				raid_xfer_status &= ~RAID_XFER_SATA1_NCQ_ERR;
			}
		}
		else
		{
			raid_xfer_status |= RAID_XFER_DONE;
		}
	}
	return;
}

u8 cal_count_bits(u32 data)
{
	u8 c = 0;
	for (; data; c++)
	{
		data &= data - 1;
	}
	return c;
}

static void read_QUEUE_ERROR_log(PSATA_OBJ pSob)
{
	// data fields of QUEUED ERROR LOG
	// byte 0: NQ bit 7, Tag bit 4:0
	// byte 2: Status, byte 3: Error
	// respond status with sense information to host
	if (mc_buffer[0] & BIT_7)
	{// NQ is set, 
		MSG("NQ ER\n");
	}
	else
	{
		u8 tag = mc_buffer[0] & 0x1F;
		MSG("tag %bx\n", tag);
		// find the USB command
		PEXT_CCM pScm = &(pSob->sobj_Scm[tag]);
		if (pScm)
		{
			PCDB_CTXT pCtxt = pScm->scm_pCdbCtxt;
			if (pCtxt)
			{
				MSG("iTag %bx\n", (u8)pCtxt->CTXT_ITAG);
				pCtxt->CTXT_FIS[FIS_STATUS] =  mc_buffer[2];
				pCtxt->CTXT_FIS[FIS_ERROR] =  mc_buffer[3];
				
				if ((pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN) == 0)
				{
					// generate SCSI Sense code
					hdd_ata_err_2_sense_data(pCtxt);
					pCtxt->CTXT_ccmIndex = NULL; // send SIU with check condition in USB ISR
					pCtxt->CTXT_usb_state = CTXT_USB_STATE_CTXT_SETUP_DONE;
					
					if ((usb_ctxt.curCtxt == NULL) || (usb_ctxt.curCtxt == pCtxt))
					{
						reset_Endpoint(pCtxt);
					}

					if (usb_ctxt.curCtxt == pCtxt)
					{
						usb_ctxt_send_status(pCtxt);
					}
					else
					{
						usb_device_no_data(pCtxt);
					}
					Delay(1);
				}
				else
				{
					if (usb_ctxt.curCtxt == pCtxt)
					{
						if (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN)
						{
							sata_update_RAID_done_status(pSob, pCtxt, pScm);
						}
					}
					pCtxt->CTXT_FLAG |= CTXT_FLAG_UAS_ERR_ABORT;
				}
			}
						
			pSob->sobj_sBusy &= ~(1 << tag);
			sata_DetachCCM(pSob, (1 << tag));
		}
		else
		{
			MSG("N-SCM\n");
		}
	}
}

static sata_NCQ_error_handle(PSATA_OBJ pSob)
{
	dbg_next_cmd = 5;
	// recovery from the error state
	if ((raid_xfer_status & RAID_XFER_IN_RPOGRESS) == 0)
	{
		MSG("Raid");
		sata_recovery_NCQ_error(pSob);
	}
	else
	{
		PCDB_CTXT pCtxt = usb_ctxt.curCtxt;
		if (pSob->sobj_sata_ch_seq)
		{
			raid_xfer_status |= RAID_XFER_SATA1_NCQ_ERR;
			pCtxt->CTXT_CONTROL |= CTXT_CTRL_SATA1_DONE;
		}
		else
		{
			raid_xfer_status |= RAID_XFER_SATA0_NCQ_ERR;
			pCtxt->CTXT_CONTROL |= CTXT_CTRL_SATA0_DONE;
		}
		MSG("Update RaidSta\n");
		sata_update_RAID_done_status(pSob, pCtxt, NULL);
	}
}

static void reset_resource_on_sata_DMATX_done(PSATA_OBJ pSob)
{
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;
	PEXT_CCM	pScm = pSob->sobj_curScm;
	PCDB_CTXT pCtxt = pScm->scm_pCdbCtxt;
	u8 segIndex = pScm->scm_SegIndex;
	if ((pCtxt->CTXT_FLAG & CTXT_FLAG_B2S) == 0)
	{
		// Reset on USB RX FIFO will propagate to DBUFF & 
		// it is not necssary to reset DBUF again
		usb_rx_fifo_rst();					
	}

#ifdef AES_EN
	if (pCtxt->CTXT_FLAG & CTXT_FLAG_AES)
	{
		*aes_control &= ~(AES_ENCRYP_EN | AES_ENCRYP_KEYSET_SEL);
		*aes_control;
	}
#endif

	if (pScm->scm_prot == PROT_FPDMAOUT)
	{//NCQ protocol
		reset_dbuf_seg(segIndex);

#ifndef FPGA // it seems that the SATA TX FIFO is not required to reset
		sata_ch_reg->sata_BlkCtrl |= TXSYNCFIFORST;	//reset sata TX  fifo
		sata_ch_reg->sata_BlkCtrl;
#endif
	}

	free_dbufConnection(segIndex);
#ifdef AES_EN
	if (pCtxt->CTXT_FLAG & CTXT_FLAG_AES)
	{
		*aes_control &= ~(AES_ENCRYP_EN | AES_ENCRYP_KEYSET_SEL);
		*aes_control;
	}
#endif
	if ((pCtxt->CTXT_FLAG & CTXT_FLAG_B2S) == 0)
	{
		usb_rx_fifo_rst();
	}
}

static void ProcessSataDMASetup(PSATA_OBJ pSob)
{
	PCDB_CTXT	pCtxt = NULL;
	PEXT_CCM	pScm = NULL;
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;
	switch (pSob->sobj_subsubState)
	{
		case NCQ_IDLE_CHK_PENDING_SIU:
#ifdef DBG_PERFORMANCE
			*gpioDataOut = *gpioDataOut & ~(GPIO8_PIN|GPIO10_PIN|GPIO11_PIN) | (GPIO9_PIN);
#endif
			// release the status of the active USB command
			pScm = pSob->sobj_curScm;
			if (pSob->sobj_curScm != NULL)
			{
				pCtxt = pScm->scm_pCdbCtxt;
				if (pCtxt != NULL)
				{
					uas_Chk_pending_SIU(pCtxt);
					pScm->scm_pCdbCtxt = NULL;
				}
				pSob->sobj_curScm = NULL; 
			}
			pSob->sobj_subsubState = NCQ_IDLE_CHK_AVAIL_DBUF;

		case NCQ_IDLE_CHK_AVAIL_DBUF:
			u8 segIndex;			
			u8 cur_tag = sata_ch_reg->sata_RxTag & 0x1F;   	  // we need get from sata protocal:(rigester)
			pScm = &pSob->sobj_Scm[cur_tag];
			DBG("CurT %bx\n", cur_tag);

			u8 dbuf_control = pScm->scm_SegIndex;
			if (pScm->scm_prot == PROT_FPDMAIN)
			{
				if((segIndex = TX_DBUF_GetIdleSeg_UAS(dbuf_control)) == SEG_NULL)
					return;
			}
			else
			{
				if((segIndex = RX_DBUF_GetIdleSeg_UAS(dbuf_control)) == SEG_NULL)
					return;
			}
			pCtxt = pScm->scm_pCdbCtxt;	
			sata_ch_reg->sata_FrameInt = DMASETUPI;
			sata_ch_reg->sata_CurrTag = cur_tag;

			pScm->scm_SegIndex = segIndex;
			pCtxt->CTXT_usb_state = CTXT_USB_STATE_CTXT_SETUP_DONE;
			pCtxt->CTXT_DbufIndex = segIndex;
			// current Active SATA command with CCM & SCM
			pSob->sobj_curScm = pScm;

			//DBG("\n\nDmaSetup:  Seg: %lx  sataTag: %lx  usbTag: %lx", segIndex, cur_tag, pScm->pCdbCtxt->CTXT_ITAG);			
			if (pScm->scm_prot == PROT_FPDMAIN)
			{	
				// rout Data to Dbuffer 
				set_dbufSource(segIndex);			
				pSob->sobj_subState = SATA_SUB_STATE_NCQ_RX_XFER;

			}
			else
			{	
				pSob->sobj_subState = SATA_SUB_STATE_NCQ_TX_XFER;
			}	

			// check there's pending SIU is ready to be sent to host when next DMASETUP is assert
			pSob->sobj_subsubState = NCQ_IDLE_CHK_PENDING_SIU; 
			
			if (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN)
			{
				// for RAID, the usb ctxt should be setted once
				if ((pCtxt->CTXT_CONTROL & CTXT_CTRL_RAID_USB_START) == 0)
				{
					pCtxt->CTXT_CONTROL |= CTXT_CTRL_RAID_USB_START;	
				}	
				else
				{
					return;
				}
			}

			usb_exec_que_ctxt();	

#ifdef DBG_PERFORMANCE
			*gpioDataOut = *gpioDataOut & ~(GPIO8_PIN|GPIO10_PIN) | (GPIO9_PIN | GPIO11_PIN);
#endif
			return;	
	}
}

static void ProcessSataSetDevBit(PSATA_OBJ pSob)
{
	//u32 fis3210, fis7654;
#ifdef DBG_PERFORMANCE
	*gpioDataOut = *gpioDataOut & (~(GPIO9_PIN | GPIO10_PIN |GPIO11_PIN)) | GPIO8_PIN;
#endif
	PEXT_CCM	pScm;
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;
	u32 setDevBitFisDW0 = sata_ch_reg->sata_FISRCV0[0];
	DBG("SetD\n");
	if ((setDevBitFisDW0 & 0xff) != SET_DEVICE_BITS_FIS)
	{
		sata_ch_reg->sata_FrameInt = SETDEVBITSI;
		return;
	}	
	
	pScm = pSob->sobj_curScm;
	
	//FW can not get the original SActive from HD, 
	//so fetch the SETDEVBITS FIS from Receive FIFO
	switch (pSob->sobj_subState)
	{
	case SATA_SUB_STATE_NCQ_IDLE:
		if(pScm != NULL)
		{
			if (sata_ch_reg->sata_IntStatus & (DATATXDONE|RXDATARLSDONE|RXRLSDONE|DATARXDONE))
			{
				DBG("ct\n");
				return;
			}
			else
			{// it's normal case because the HDD fails the command with SETDEVBIT FIS
//					pSob->sobj_curScm = NULL;
				ERR("error: Setdevbit with active scm\n");
			}
		}
		// continue to do process the SetDevBit Fis

	case SATA_SUB_STATE_NCQ_ERROR:
		u32 setDevBitFisDW1 = sata_ch_reg->sata_FISRCV0[1];
		sata_ch_reg->sata_FrameInt = SETDEVBITSI;
		// Set Device Bits - Device to Host
		// firmware shall record the tags with successful transfer, 
		// it shall cover the case that the error bit is set, but the ACT field all set the successful transfer Tag
		if (setDevBitFisDW1)
		{
			if (pSob->sobj_subState == SATA_SUB_STATE_NCQ_ERROR)
			{
				pSob->sobj_subState = SATA_SUB_STATE_NCQ_CLEANUP;
			}
			else
			{
				//check if the status of the active USB command has been received
				if (pScm != NULL)
				{
					PCDB_CTXT pCtxt = pScm->scm_pCdbCtxt;
					if (pCtxt != NULL)
					{
						DBG("dw1 %lx, %bx\n", setDevBitFisDW1, pCtxt->CTXT_ccmIndex);
						if ((1 << pCtxt->CTXT_ccmIndex) & setDevBitFisDW1)
						{
							if (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN)
							{
								sata_update_RAID_done_status(pSob, pCtxt, pScm);
							}
							else
							{
								uas_Chk_pending_SIU(pCtxt); 
								pScm->scm_pCdbCtxt = NULL;
								pSob->sobj_curScm = NULL; 
							}
						}
					}
				}

				sata_DetachCCM(pSob, setDevBitFisDW1); //
				if ((pSob->sobj_sBusy &= (~setDevBitFisDW1)) == 0)
				{
					pSob->sobj_State = SATA_READY;
					sata_exec_next_scm(pSob);
				}
			}
		}
		
		if (setDevBitFisDW0 & BIT_16)//error bit is set
		{	// NCQ commands Fail
			MSG("N F %bx\n", pSob->sobj_sata_ch_seq);
			dbg_next_cmd = 3;
			if(pSob->sobj_sata_ch_seq)
			{
				uas_ci_paused |= UAS_PAUSE_SATA1_NCQ_ERROR_CLEANUP;//SATA_CH1
			}
			else
			{
				uas_ci_paused |= UAS_PAUSE_SATA0_NCQ_ERROR_CLEANUP;//SATA_CH0
			}
			
			pSob->sobj_subState = SATA_SUB_STATE_NCQ_ERROR;
		}	
		break;
	}

#ifdef DBG_PERFORMANCE
	*gpioDataOut = *gpioDataOut & (~(GPIO9_PIN | GPIO10_PIN)) | (GPIO8_PIN|GPIO11_PIN);
#endif
}

static void ProcessDataRXDONE(PSATA_OBJ pSob)
{// DIN FPDMA transfer
	PEXT_CCM	pScm;
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;

	DBG("rxl dn: tag: %BX\n", sata_ch_reg->sata_CurrTag);
	sata_ch_reg->sata_CurrTag = 0xFF;			//must reset CurrTag	 // in v8.24
	sata_ch_reg->sata_IntStatus = (RXDATARLSDONE | DATARXDONE | RXRLSDONE);
	if (dbg_next_cmd)
		MSG("SRX\n");
	if((pScm = pSob->sobj_curScm) != NULL)
	{
		u32 segIndex;
		
		if((segIndex = pScm->scm_SegIndex) != SEG_NULL)
		{
			// disconnect input port
			DBG("rst strx\n");
			if (pScm->scm_prot == PROT_PIOIN)
			{
				pSob->sobj_Init_Failure_Reason = HDD_INIT_NO_FAILURE;
				if ((sata_ch_reg->sata_Status & ATA_STATUS_CHECK) == 0)
				{
					#define QUEUED_ERROR_LOG_DATA_LEN 	512
					read_data_from_cpu_port(QUEUED_ERROR_LOG_DATA_LEN,DIR_TX, 1);
				}
				else
				{
					pSob->sobj_Init_Failure_Reason = HDD_INIT_FAIL_ATA_INIT_ERROR;	
				}
			}
			else
			{
				PCDB_CTXT pCtxt = pScm->scm_pCdbCtxt;
				if (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN)
				{// the data buffer reset shall be setted in RAID_DONE ISR
					DBG("%bx\n", pSob->sobj_sata_ch_seq);
					pSob->sobj_subState = SATA_SUB_STATE_NCQ_IDLE;
					return;
				}
				free_SourcedbufConnection(segIndex);
			}
			pScm->scm_SegIndex = SEG_NULL;	
			sata_ch_reg->sata_BlkCtrl |= RXSYNCFIFORST; //reset sata RX  fifo
			sata_ch_reg->sata_BlkCtrl;
			
			if (pScm->scm_prot == PROT_PIOIN)
			{
				reset_dbuf_seg(segIndex);
				// reset Dbuf
				free_dbufConnection(segIndex);
				if (dbg_next_cmd)
					MSG("sbsy %lx, %bx\n", pSob->sobj_sBusy, pScm->scm_ccmIndex);
				delete_curSCM(pSob);
#ifdef UAS				
				if (pSob->sobj_State >= SATA_NCQ_FLUSH)
				{
					if (pSob->sobj_subState == SATA_SUB_STATE_NCQ_CLEANUP)
					{
						if (pSob->sobj_Init_Failure_Reason == HDD_INIT_NO_FAILURE)
						{
							if (dbg_next_cmd)
								MSG("rd errLg\n");
							sata_ch_reg->sata_Status |= ATA_STATUS_CHECK;
							pSob->sobj_subState = SATA_SUB_STATE_NCQ_IDLE;	
							pSob->sobj_State = SATA_READY;
							read_QUEUE_ERROR_log(pSob);
							sata_Cleanup_NCQCMD(pSob, 0xFFFFFFFF);
							if(pSob->sobj_sata_ch_seq)
							{
								uas_ci_paused &= ~UAS_PAUSE_SATA1_NCQ_ERROR_CLEANUP;//SATA_CH1
							}
							else
							{
								uas_ci_paused &= ~UAS_PAUSE_SATA0_NCQ_ERROR_CLEANUP;//SATA_CH0 
							}
							return;
						}
						else
						{// HDD does not support read Queue Error log command, abort all the pending commands
							uas_abort_all();
							pSob->sobj_subState = SATA_SUB_STATE_NCQ_IDLE;
							return;
						}
					}
				}
				else
#endif					
				{
					if (pSob->sobj_subState == SATA_ATA_READ_ID)
					{
						// check the validate of ID command or ID data
						if (pSob->sobj_Init_Failure_Reason)
						{
							//pSob->sobj_Init_Failure_Reason = HDD_INIT_FAIL_ATA_INIT_ERROR;
							pSob->sobj_subState = SATA_INITIALIZED_FAIL;
						}
						else
						{
							if (ata_init(pSob))
								pSob->sobj_subState = SATA_INITIALIZED_FAIL;
							//issue the spin up command
							ata_ExecNoDataCmd(pSob, ATA6_IDLE_IMMEDIATE, 0, 0); 
							rw_time_out = 200;
							pSob->sobj_subState = SATA_SPIN_UP_HDD;
						}
					}
					
					if ((pSob->sobj_State == SATA_PRE_READY) && (pSob->sobj_subState == SATA_INITIALIZED_FAIL))
					{
						pSob->sobj_State = SATA_READY;
						return;
					}					
				}
			}
			else
			{
				if (dbg_next_cmd)
				{
					MSG("SRXD\n");
				}
				pSob->sobj_subState = SATA_SUB_STATE_NCQ_IDLE;
				return;
			}
		}
	}
}

static void ProcessDataTXDONE(PSATA_OBJ pSob)
{// DOut FPDMA transfer
	u32	segIndex;
	PCDB_CTXT pCtxt;
	PEXT_CCM	pScm = pSob->sobj_curScm;
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;
	
	if (pScm != NULL)
	{
		if ((pCtxt = pScm->scm_pCdbCtxt) != NULL)
		{
			if (pCtxt->CTXT_Status != CTXT_STATUS_XFER_DONE)
			{
				DBG("wait USB rx done\n");
				return;
			}
			
			if (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN)
			{
				sata_ch_reg->sata_CurrTag = 0xFF;	
				sata_ch_reg->sata_IntStatus = DATATXDONE;
				pSob->sobj_subState = SATA_SUB_STATE_NCQ_IDLE;
				return;
			}
			
			if((segIndex = pScm->scm_SegIndex) != SEG_NULL)
			{					
				reset_resource_on_sata_DMATX_done(pSob);
			}

			// Any releated USB CDB
			// break association between pCtxt & pScm
//			pScm->scm_pCdbCtxt = NULL;
//			pCtxt->CTXT_ccmIndex = CCM_NULL;
//			pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
#ifdef LINK_RECOVERY_MONITOR
			if (rw_flag & LINK_RETRY_FLAG)
			{
				w_recover_counter++;
				MSG("W recvry %x\n", w_recover_counter);
			}
#endif
			if (dbg_next_cmd)
			{
				MSG("STXD\n");
			}
			pCtxt->CTXT_CONTROL |= CTXT_CTRL_SEND_SIU;
			pSob->sobj_subState = SATA_SUB_STATE_NCQ_IDLE;
		}
	}
	sata_ch_reg->sata_CurrTag = 0xFF;	
	sata_ch_reg->sata_IntStatus = DATATXDONE;
}

static void ProcessDMADATARXDONE(PSATA_OBJ pSob)
{// DIN DMA transfer
	PEXT_CCM	pScm = pSob->sobj_curScm;
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;

	if (pScm)
	{		
		PCDB_CTXT	pCtxt;

		if ((pCtxt = pScm->scm_pCdbCtxt) == NULL)
		{
			DBG("rxdn: ctxt NULL\n");
			sata_ch_reg->sata_IntStatus = RXDATARLSDONE;
			pSob->sobj_curScm = NULL;
			return;
		}
		
		if (pCtxt->CTXT_Status != CTXT_STATUS_XFER_DONE)
		{
			DBG("wait USB tx done\n");
			return;
		}
		
		u32 segIndex = pScm->scm_SegIndex;
		if ((segIndex != SEG_NULL) && (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN))
		{// should be removed as well
			sata_update_RAID_done_status(pSob, pCtxt, pScm);
			sata_ch_reg->sata_FrameInt = D2HFISI;
			sata_ch_reg->sata_IntStatus = (RXDATARLSDONE|DATARXDONE|RXRLSDONE|FRAMEINTPEND);
			return;
		}

		if ((pCtxt = pScm->scm_pCdbCtxt))
		{
			u16 ata_error_status =  *((u16 *)(&sata_ch_reg->sata_Status));	
			if (ata_error_status & ATA_STATUS_CHECK)
			{
				*(((u16 *) &(pCtxt->CTXT_FIS[FIS_STATUS]))) =  ata_error_status;
				dbg_next_cmd = 1;
				MSG("dr Ata");
				if (pScm->scm_control & SCM_CONTROL_SATA0_EN)
					MSG("0");
				else if (pScm->scm_control & SCM_CONTROL_SATA1_EN)
					MSG("1");
//						MSG("%bx, %bx, ctxt flag %bx\n", pCtxt->CTXT_FIS[FIS_STATUS], pCtxt->CTXT_FIS[FIS_ERROR], pCtxt->CTXT_FLAG);
				MSG(" err %bx, %bx\n", pCtxt->CTXT_FIS[FIS_STATUS], pCtxt->CTXT_FIS[FIS_ERROR]);
				// generate SCSI Sense code
				if ((pCtxt->CTXT_FLAG & CTXT_FLAG_B2S) == 0)
					hdd_ata_err_2_sense_data(pCtxt);
			}
			else
			{
				if (pCtxt->CTXT_FLAG & CTXT_FLAG_RET_TFR)
				{
					sata_return_tfr(pSob, pCtxt);
					if ((pCtxt->CTXT_FLAG & CTXT_FLAG_B2S) == 0)
						hdd_ata_return_tfr(pCtxt);					
				}					
				else
				{
					pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
				}
			}

			pCtxt->CTXT_ccmIndex = CCM_NULL;
		}
//				MSG("s dn");
		if (segIndex != SEG_NULL)
		{	
			// Disconnect input port
//					*((u8 *) &(Tx_Dbuf->dbuf_Seg[segIndex].dbuf_Seg_INOUT)) &= 0xf0;
			DBG("rst ST\n");
			free_SourcedbufConnection(segIndex);
			pScm->scm_SegIndex = SEG_NULL;	
			sata_ch_reg->sata_BlkCtrl |= RXSYNCFIFORST;	//reset sata RX  fifo	
			sata_ch_reg->sata_BlkCtrl;
		}

		delete_curSCM(pSob);
		pSob->sobj_State = SATA_READY;
		// the following is for UAS only 
		sata_exec_next_scm(pSob);
	}
	sata_ch_reg->sata_FrameInt = D2HFISI;
	sata_ch_reg->sata_IntStatus = 0xff;
	return;
}

static void ProcessDMADATATXDONE(PSATA_OBJ pSob)
{// DOut DMA transfer
	PEXT_CCM	pScm = pSob->sobj_curScm;
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;
	
	DBG("<W%X>", sata_ch_reg->sata_DataTxStat);
	if (dbg_next_cmd)
	{
		MSG("DATATXI %x\n", sata_ch_reg->sata_IntStatus);
	}

	if (pScm)
	{
		
		PCDB_CTXT	pCtxt = pScm->scm_pCdbCtxt;
		u16			ata_error_status;

		if (pCtxt == NULL)
		{
			DBG("sata_isr: pCtxt NULL in DATATXDONE\n");
			sata_ch_reg->sata_IntStatus = DATATXDONE;
			pSob->sobj_curScm = NULL;
			return;
		}
//				if (dbg_next_cmd)
//					MSG("Ctxt status %bx\n", pCtxt->CTXT_Status);
		u32 segIndex = pScm->scm_SegIndex;

		if ((pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN) == 0)
		{
			if (pCtxt->CTXT_Status != CTXT_STATUS_XFER_DONE)
			{
				if (dbg_once_flag == 0)
				{
					MSG("w U rx dn\n");
					MSG("msc0Int %lx\n", *usb_Msc0IntStatus);
					dbg_once_flag = 1;
					dbg_next_cmd = 2;
				}
				// for the special case that the USB write is done but the SATA done is finished because of the check condition w/o data tx done
				// FW will halt the dout bulk, when the USB halt done is recieved, FW sets the CTXT status to XFR_DONE
				if (sata_ch_reg->sata_TCnt && (pCtxt->CTXT_Status != CTXT_STATUS_HALT)) 
				{
					*usb_Msc0DOutCtrl = MSC_DOUT_HALT;
					pCtxt->CTXT_Status = CTXT_STATUS_HALT;	
					dbg_next_cmd = 2;
					MSG("s ht\n");
				}
				return;
			}
		}
		
		if (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN)
		{
			sata_update_RAID_done_status(pSob, pCtxt, pScm);
			sata_ch_reg->sata_FrameInt = D2HFISI;
			sata_ch_reg->sata_IntStatus = (DATATXDONE|FRAMEINTPEND);
			return;
		}
		// everthing is ok for sata_isr() to process DATATXDONE interrupt
		dbg_once_flag = 0;

		if (segIndex != SEG_NULL)
		{	
			
			reset_resource_on_sata_DMATX_done(pSob);
		}

		ata_error_status =  *((u16 *)(&sata_ch_reg->sata_Status));
		if (ata_error_status & ATA_STATUS_CHECK)
		{
			*(((u16 *) &(pCtxt->CTXT_FIS[FIS_STATUS]))) =  ata_error_status;
			
			dbg_next_cmd = 1;
			MSG("dt Ata ");
			if (pScm->scm_control & SCM_CONTROL_SATA0_EN)
				MSG("0");
			else
				MSG("1");
			MSG(" err %bx, %bx\n", pCtxt->CTXT_FIS[FIS_STATUS], pCtxt->CTXT_FIS[FIS_ERROR]);
			// generate SCSI Sense code
			if ((pCtxt->CTXT_FLAG & CTXT_FLAG_B2S) == 0)
				hdd_ata_err_2_sense_data(pCtxt);
		}
		else
		{
			if (pCtxt->CTXT_FLAG & CTXT_FLAG_RET_TFR)
			{
				sata_return_tfr(pSob, pCtxt);
				if ((pCtxt->CTXT_FLAG & CTXT_FLAG_B2S) == 0)
					hdd_ata_return_tfr(pCtxt);					
			}					
			else
			{
				pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
			}
		}
		
//				Delay(1);
		if ((dbg_next_cmd) && ((pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR) == 0))
		{
			MSG("fl %bx\n", pCtxt->CTXT_FLAG);
		}
		if (!(pCtxt->CTXT_FLAG & CTXT_FLAG_B2S))
		{
			pCtxt->CTXT_ccmIndex = CCM_NULL;

			//  send status back to USB Host
#ifdef LINK_RECOVERY_MONITOR
			if (rw_flag & LINK_RETRY_FLAG)
			{
				w_recover_counter++;
				MSG("W recvry %x\n", w_recover_counter);
			}
#endif
			if (dbg_next_cmd)
				MSG("s csw\n");
			usb_ctxt_send_status(pCtxt);
		}

		delete_curSCM(pSob);

		pSob->sobj_State = SATA_READY;
		// the following is for UAS only 
		sata_exec_next_scm(pSob);
	}
	sata_ch_reg->sata_FrameInt = D2HFISI;
	sata_ch_reg->sata_IntStatus = (DATATXDONE|FRAMEINTPEND);
	return;
}

static void ProcessDMAStatusFis(PSATA_OBJ pSob)
{
	PEXT_CCM	pScm = pSob->sobj_curScm;
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;
	u8 segIndex = SEG_NULL;
	if (dbg_next_cmd)
	{
		DBG("Fis34I %x\n", sata_ch_reg->sata_IntStatus);
	}

	if (pScm)
	{
		if (raid_rebuild_verify_status & RRVS_IN_PROCESS)
		{
			if ((raid_rebuild_verify_status & RRVS_IO_DONE) != RRVS_IO_DONE)
			{
				if (pScm->scm_control & SCM_CONTROL_SATA0_EN)
				{
					DBG("D0 D\n");
					raid_rebuild_verify_status |= RRVS_D0_IO_DONE;
				}
				if (pScm->scm_control & SCM_CONTROL_SATA1_EN)
				{
					DBG("D1 D\n");
					raid_rebuild_verify_status |= RRVS_D1_IO_DONE;
				}
				
				if ((raid_rebuild_verify_status & RRVS_IO_DONE) != RRVS_IO_DONE)
				{
					return;
				}
			}
		}
		else
		{
			PCDB_CTXT	pCtxt = pScm->scm_pCdbCtxt;

			if (pCtxt)
			{
				if ((pCtxt->CTXT_FLAG & CTXT_FLAG_B2S) == 0)
				{
					if (pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR)
					{
						if (pCtxt->CTXT_Status != CTXT_STATUS_XFER_DONE) return; // wait for USB transfer done
					}
					else
					{
						if (pCtxt->CTXT_Status != CTXT_STATUS_XFER_DONE)
						{
							if ((pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN) == 0)
							{
								// for the special case that the USB write is done but the SATA done is finished because of the check condition w/o data tx done
								// FW will halt the dout bulk, when the USB halt done is recieved, FW sets the CTXT status to XFR_DONE
								if (sata_ch_reg->sata_TCnt && (pCtxt->CTXT_Status != CTXT_STATUS_HALT)) 
								{
									*usb_Msc0DOutCtrl = MSC_DOUT_HALT;
									pCtxt->CTXT_Status = CTXT_STATUS_HALT;	
									dbg_next_cmd = 2;
									MSG("s ht\n");
								}
							}
							return;
						}
					}
				}

				if (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN)
				{
					sata_update_RAID_done_status(pSob, pCtxt, pScm);
					sata_ch_reg->sata_FrameInt = D2HFISI;			
					sata_ch_reg->sata_IntStatus = FRAMEINTPEND;
					return;
				}
				
				// break association between pCtxt & pScm
				segIndex = pScm->scm_SegIndex;
				pScm->scm_pCdbCtxt = NULL;
				pCtxt->CTXT_ccmIndex = CCM_NULL;

				u16 ata_error_status =  *((u16 *)(&sata_ch_reg->sata_Status));
				if (ata_error_status & ATA_STATUS_CHECK)
				{
					*(((u16 *) &(pCtxt->CTXT_FIS[FIS_STATUS]))) =  ata_error_status;

					// generate SCSI Sense code
					if ((pCtxt->CTXT_FLAG & CTXT_FLAG_B2S) == 0)
						hdd_ata_err_2_sense_data(pCtxt);
				}
				else
				{
					if (pCtxt->CTXT_FLAG & CTXT_FLAG_RET_TFR)
					{
						sata_return_tfr(pSob, pCtxt);
						if ((pCtxt->CTXT_FLAG & CTXT_FLAG_B2S) == 0)
							hdd_ata_return_tfr(pCtxt);					
					}					
					else
					{
						pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
					}
				}

				if (pCtxt->CTXT_FLAG & CTXT_FLAG_U2B)
				{
					//usb_append_tx_ctxt_que(pCtxt);
					 usb_device_no_data(pCtxt);
				}
				else if ((pCtxt->CTXT_FLAG & CTXT_FLAG_B2S) == 0)
				{
					if (pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR)
					{
						// reset resource
						if (segIndex != SEG_NULL)
						{
							free_SourcedbufConnection(segIndex);
							pScm->scm_SegIndex = SEG_NULL;	
							sata_ch_reg->sata_BlkCtrl;
						}
					}
					else
					{
						//reset resource and send out status
						if (segIndex != SEG_NULL)
						{
							reset_resource_on_sata_DMATX_done(pSob);
						}
						
						if (!(pCtxt->CTXT_FLAG & CTXT_FLAG_B2S))
						{
							//  send status back to USB Host
#ifdef LINK_RECOVERY_MONITOR
							if (rw_flag & LINK_RETRY_FLAG)
							{
								w_recover_counter++;
								MSG("W recvry %x\n", w_recover_counter);
							}
#endif
							if (dbg_next_cmd)
								MSG("s csw\n");
							usb_ctxt_send_status(pCtxt);
						}
					}
				}
				sata_ch_reg->sata_BlkCtrl |= RXSYNCFIFORST;	//reset sata RX  fifo	
			}
		}
		if (raid_rebuild_verify_status & RRVS_IN_PROCESS)
		{
			if ( pSob->sobj_sata_ch_seq)
			{
				raid_rebuild_verify_status |= RRVS_D1_DONE;
			}
			else
			{
				raid_rebuild_verify_status |= RRVS_D0_DONE;
			}
		}
		delete_curSCM(pSob);
		pSob->sobj_State = SATA_READY;
		// the following is for UAS only 
		sata_exec_next_scm(pSob);
	}
	sata_ch_reg->sata_FrameInt = D2HFISI;

	// clear SATA Frame interrupt			
	sata_ch_reg->sata_IntStatus = FRAMEINTPEND;
	if ((segIndex != SEG_NULL) || (raid_rebuild_verify_status & RRVS_IN_PROCESS))
	{//
		sata_ch_reg->sata_IntStatus = (RXDATARLSDONE|RXRLSDONE|DATARXDONE | DATATXDONE);
	}
	DBG("I %x\n", sata_ch_reg->sata_IntStatus);
}
						
/****************************************\
	sata_isr
\****************************************/
void sata_isr(PSATA_OBJ  pSob)
{
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;

	u32 sata_frame_int = sata_ch_reg->sata_FrameInt;
	u16 sata_int_status = sata_ch_reg->sata_IntStatus;
	DBG("st[%bx] isr %lx, %x\n", pSob->sobj_sata_ch_seq, sata_frame_int, sata_int_status);
	if (pSob->sobj_State >= SATA_NCQ_FLUSH)
	{
		switch(pSob->sobj_subState)
		{
			case SATA_SUB_STATE_NCQ_IDLE:
				// Sata is in SATA_NCQ_ACTIVE or SATA_NCQ_FLUSH state
				if (sata_frame_int & DMASETUPI)
				{
					ProcessSataDMASetup(pSob);
				}	// 	if (sata_ch_reg->sata_FrameInt & DMASETUPI)
				else if (sata_frame_int & SETDEVBITSI)
				{
					ProcessSataSetDevBit(pSob);
				}
				else if (sata_frame_int & D2HFISI)
				{
					if (sata_ch_reg->sata_Status  & ATA_STATUS_CHECK)
					{
						// move to error state
						pSob->sobj_subState = SATA_SUB_STATE_NCQ_ERROR;
					}
					sata_ch_reg->sata_FrameInt = D2HFISI;
				}
				if (pSob->sobj_subState == SATA_SUB_STATE_NCQ_ERROR)
				{
					sata_NCQ_error_handle(pSob);
				}
				break;
				
			case SATA_SUB_STATE_NCQ_TX_XFER:
#ifdef DBG_PERFORMANCE
				*gpioDataOut = *gpioDataOut & ~(GPIO8_PIN|GPIO11_PIN) | (GPIO9_PIN | GPIO10_PIN);
#endif
				if (sata_int_status & DATATXDONE )
				{	// Data-out Done for NCQ command
					ProcessDataTXDONE(pSob);
				}
				if (sata_frame_int & SETDEVBITSI)
				{
					if (sata_ch_reg->sata_Status  & ATA_STATUS_CHECK)
					{// the error case shall be covered
						pSob->sobj_subState = SATA_SUB_STATE_NCQ_ERROR;
						sata_NCQ_error_handle(pSob);
						goto _RECEIVE_SETDEVBITFIS;
					}
				}
				break;
			case SATA_SUB_STATE_NCQ_ERROR:
				if (sata_frame_int & SETDEVBITSI)
				{
_RECEIVE_SETDEVBITFIS:
					ProcessSataSetDevBit(pSob);
				}
				break;
			case SATA_SUB_STATE_NCQ_CLEANUP:
			case SATA_SUB_STATE_NCQ_RX_XFER:
#ifdef DBG_PERFORMANCE
				*gpioDataOut = *gpioDataOut & (~GPIO8_PIN) | (GPIO9_PIN | GPIO10_PIN |GPIO11_PIN);
#endif
				if ( sata_int_status & RXDATARLSDONE )
				{
					ProcessDataRXDONE(pSob);
					if (pSob->sobj_subState == SATA_SUB_STATE_NCQ_CLEANUP)
					{
						sata_ch_reg->sata_BlkCtrl &= ~DDONEFLUSH_DIS;
					}
				}
				if (sata_frame_int & SETDEVBITSI)
				{
					if (sata_ch_reg->sata_Status  & ATA_STATUS_CHECK)
					{// the error case shall be covered
						pSob->sobj_subState = SATA_SUB_STATE_NCQ_ERROR;
						sata_NCQ_error_handle(pSob);
						goto _RECEIVE_SETDEVBITFIS;
					}
				}
				break;				
		}
	}	//  if (pSob->sobj_State >= SATA_NCQ_FLUSH)
	else  if (pSob->sobj_State == SATA_ACTIVE)
	{
		if (sata_frame_int & (SETDEVBITSI|DMASETUPI))
		{
			sata_ch_reg->sata_FrameInt = (SETDEVBITSI|DMASETUPI);
			// clear SATA Frame interrupt
			sata_ch_reg->sata_IntStatus = FRAMEINTPEND;
		}

		if (raid_rebuild_verify_status & RRVS_IN_PROCESS)		
		{
			DBG("R %x, %lx\n", sata_int_status, sata_frame_int);
			goto _CHK_D2HFISI;
		}
		// SATA non-NCQ command (Data-in) 
		if ( sata_int_status & RXDATARLSDONE )
		{
			ProcessDMADATARXDONE(pSob);
		}

		// SATA non-NCQ command (Data-out) 
		if ( sata_int_status & DATATXDONE )
		{
			ProcessDMADATATXDONE(pSob);
		}
		
_CHK_D2HFISI:
		// SATA non-NCQ command (No Data) 
		if (sata_frame_int & D2HFISI)
		{
			ProcessDMAStatusFis(pSob);
		}
		return;
	}	// else  if (pSob->sobj_State == SATA_ACTIVE)
	else if (pSob->sobj_State == SATA_RESETING)
	{
		if (sata_ch_reg->sata_PhyInt & PHYRDYI)
		{
			// Disable SATA PHY Ready Interrupt
			sata_ch_reg->sata_PhyIntEn &= ~PHYRDYIEN;

			// clear SATA PHY Ready & Down interrupt
			sata_ch_reg->sata_PhyInt = (PHYDWNI|PHYRDYI|DEVDETI);

			// clear SATA PHY interrupt
			sata_ch_reg->sata_IntStatus = 0xCC;

			// Enable SATA PHY Down Interrupt
			sata_ch_reg->sata_PhyIntEn |= PHYDWNIEN;

			// 	Enable D2HFIS Interrupt for Signature FIS
			sata_ch_reg->sata_FrameIntEn |= D2HFISIEN;

			pSob->sobj_State = SATA_PHY_RDY;
			MSG("S%bx PhyRDY\n", pSob->sobj_sata_ch_seq);
			
			// For simplicity, just assume the HDD is spinning now.
			disable_upkeep &= ~((pSob->sobj_sata_ch_seq) ? UPKEEP_HDD_SPUN_DOWN_S1 : UPKEEP_HDD_SPUN_DOWN_S0);

			// In most cases, the standby condition timer should be enabled.
			// Caller must disable it explicitly if it should not be.
			disable_upkeep &= ~((pSob->sobj_sata_ch_seq) ? UPKEEP_HOST_CONTROLS_POWER_CONDITION_S1 : UPKEEP_HOST_CONTROLS_POWER_CONDITION_S0);
			// once the fis34 is not received in 1 sec, issue the comrst once again	
		}
		return;
	}
	else if (pSob->sobj_State == SATA_PHY_RDY)
	{
		if (sata_ch_reg->sata_FrameInt & D2HFISI)
		{
			
			if ((sata_ch_reg->sata_Status & ATA_STATUS_CHECK) == 0)
			{
				if ((sata_ch_reg->sata_SectCnt == 0x01) &&
					(sata_ch_reg->sata_LbaL == 0x01))
				{
					if ((sata_ch_reg->sata_LbaM == 0x14) &&
						(sata_ch_reg->sata_LbaH == 0xEB))
					{	
						//ATAPI Device
						pSob->sobj_State = SATA_READY;
						pSob->sobj_Init_Failure_Reason = HDD_INIT_FAIL_ATA_INIT_ERROR;
						return;
					}
				}
			}
			if (chk_TX_DBUF_idleSeg_B2S())
			{// check if there's available DBUF to hold the data
				return;
			}
			MSG("rcv%bx Fis34\n", pSob->sobj_sata_ch_seq);

			if (curMscMode != MODE_UAS)
				sata_ch_reg->sata_FrameIntEn &= ~D2HFISIEN;

			// clear D2HFIS interrupt
			sata_ch_reg->sata_FrameInt = D2HFISI;

			// clear SATA Frame interrupt
			sata_ch_reg->sata_IntStatus = FRAMEINTPEND;
			sata_ch_reg->sata_BlkCtrl |= TXSYNCFIFORST;
#if 1
			pSob->sobj_State = SATA_PRE_READY; // firmware will initialize the HDD
			pSob->sobj_subState = SATA_ATA_READ_ID;
			if(pSob->sobj_sata_ch_seq)
			{
				count_no_fis34_failure_sata1 = 0;
				ata_ReadID(SATA_CH1);//  read id 
			}
			else
			{
				count_no_fis34_failure_sata0 = 0;
				ata_ReadID(SATA_CH0);//  read id 
			}
			rw_time_out = 50;
#else

			pSob->sobj_State = SATA_READY;
			// Disable D2HFIS Interrupt
			_disable(); 
			if (((sata0Obj.sobj_State >= SATA_READY) || ((HDDs_status & SATA0_READY) == 0))
			&& ((sata1Obj.sobj_State >= SATA_READY) || ((HDDs_status & SATA1_READY) == 0)))
			{
				chk_fis34_timer = 0;
				// when hdd resume from standby state, F/W will set uas_ci_paused flag -> for this case, F/W code will clear the flag here 
				// or the uas is really pending since for the single thread purpose like a non-ncq is received after que command
#ifdef UAS
				uas_ci_paused = 0;
#endif
				rsp_unit_not_ready = 0;
			}
			_enable();
			Delay(1);

			// any pending SATA command ?
			sata_exec_next_scm(pSob);
#endif
		}
		return;
	}
	else if (pSob->sobj_State == SATA_PRE_READY)
	{// wait for the SATA RX_release done
		switch (pSob->sobj_subState)
		{
			case SATA_ATA_READ_ID:
				if (sata_int_status & RXDATARLSDONE)
				{
					ProcessDataRXDONE(pSob);
				}
				break;

			case SATA_SPIN_UP_HDD:
			case SATA_ENABLE_WRITE_CACHE:
			case SATA_ENABLE_AAM:
			case SATA_ENABLE_SMART_OPERATION:
				if (sata_frame_int & D2HFISI)
				{// 
					sata_ch_reg->sata_FrameInt = D2HFISI;
					sata_ch_reg->sata_IntStatus = FRAMEINTPEND;
					sata_ch_reg->sata_BlkCtrl |= TXSYNCFIFORST;
					delete_curSCM(pSob);
					if (pSob->sobj_subState == SATA_SPIN_UP_HDD)
					{
						if (ata_enable_wr_cache(pSob))
						{// write cache is already enable, then issue command to enable smart operation
_ENABLE_AAM_OPERATION:
							if (ata_en_AAM(pSob))
							{
_ENABLE_SMART_OPERATION:
								if (ata_en_smart_operation(pSob))
								{
									goto _ENABLE_SMART_OP_DONE;
								}
							}
						}
					}			
					else if (pSob->sobj_subState == SATA_ENABLE_WRITE_CACHE)
					{
						goto _ENABLE_AAM_OPERATION;
					}
					else if (pSob->sobj_subState == SATA_ENABLE_AAM)
					{
						goto _ENABLE_SMART_OPERATION;
					}
					else if (pSob->sobj_subState == SATA_ENABLE_SMART_OPERATION)
					{	
_ENABLE_SMART_OP_DONE:
						pSob->sobj_device_state = SATA_DEV_READY;
						pSob->sobj_subState = SATA_INITIALIZED_COMPLETE;
						pSob->sobj_State = SATA_READY;
						
						if (((sata0Obj.sobj_State > SATA_PRE_READY) || (sata0Obj.sobj_device_state == SATA_NO_DEV)) 
						&& ((sata1Obj.sobj_State > SATA_PRE_READY) || (sata1Obj.sobj_device_state == SATA_NO_DEV)))
						{
							chk_fis34_timer = 0;
							spinup_timer = 0;
						}
						MSG("SRDY\n");
						sata_exec_next_scm(pSob);
					}
				}
				break;

			default:
				break;
				
		}
	}
	DBG("sata_isr done\n");
}

/****************************************\
 ata_exec_ctxt()
    Execute a SATA non-NCQ command to seecified SATA OBJ
\****************************************/
u32 sata_exec_ctxt(PSATA_OBJ  pSob, PCDB_CTXT pCtxt)
{
	SATA_CCM volatile	*pCCM;
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;
	u32		CCMIndex;
	PEXT_CCM pScm;
	//DBG("\nsata_PhyCtrl:%lx\n",sata_ch_reg->sata_PhyCtrl);
	if (dbg_next_cmd)
		DBG("s exec ctxt\n");

	CCMIndex = sata_AllocateCCM(pSob);

	if (CCMIndex == CCM_NULL)
	{
		//CCM full 
		MSG("ccm f %bx\n", pSob->sobj_sata_ch_seq);
		pCtxt->CTXT_DbufIndex = SEG_NULL;
		pCtxt->CTXT_Status = CTXT_STATUS_ERROR;
		pCtxt->CTXT_LunStatus = LUN_STATUS_BUSY;
		return CTXT_STATUS_ERROR;
	}

	pCtxt->CTXT_ccmIndex = CCMIndex;
	pCtxt->CTXT_Status = CTXT_STATUS_PENDING;
	pCtxt->CTXT_CONTROL |= pSob->sobj_sata_ch_seq ? CTXT_CTRL_SATA1_EN: CTXT_CTRL_SATA0_EN;

	pScm = &pSob->sobj_Scm[CCMIndex];
	pScm->scm_next = NULL;
	pScm->scm_pCdbCtxt = pCtxt;


	pScm->scm_ccmIndex = CCMIndex;
	pScm->scm_prot = pCtxt->CTXT_Prot;
	pScm->scm_control = pSob->sobj_sata_ch_seq ? SCM_CONTROL_SATA1_EN : SCM_CONTROL_SATA0_EN;
	pScm->scm_TimeOutTick = pCtxt->CTXT_TimeOutTick;
	pScm->scm_SegIndex = pCtxt->CTXT_DbufIndex;

	pCCM = &(sata_ch_reg->sata_Ccm[CCMIndex]);
	pCCM->ccm_prot		= FLAGS_V|pScm->scm_prot;
	pCCM->ccm_cmdinten = (u32)pCtxt->CTXT_ccm_cmdinten; 

	if ((pCtxt->CTXT_DbufIndex != SEG_NULL) && (pCtxt->CTXT_DbufIndex & DBUF_RAID_FUNCTION_ENABLE) && ((raid_rebuild_verify_status & RRVS_IN_PROCESS) ==0))
	{
		pCCM->ccm_xfercnt 	= raid_xfer_length;
	}
	else
	{
		pCCM->ccm_xfercnt = pCtxt->CTXT_XFRLENGTH;
	}

#ifdef ERR_RB_DBG  // for instant_RB here
	if ((err_rb_dbg) && (arrayMode == RAID1_MIRROR) && 
		(pCtxt->CTXT_FIS[FIS_COMMAND] == ATA6_READ_DMA_EXT) &&
		(!(raid_rebuild_verify_status & RRVS_IN_PROCESS)))
	{
		// 28 00 00 00 10 00 00 xx xx 00  will cause a FAIL
		if ((*((u32 *)&pCtxt->CTXT_FIS[FIS_LBA_LOW]) & 0x00FFFFFF) == 0x1000)
		{
			MSG("insRBdbg FAIL entrance\n");
			pCtxt->CTXT_FIS[FIS_LBA_HIGH_EXT] = 0x1;
		}
	}
#endif
	//xmemcpy(pCtxt->CTXT_FIS, (u8 *)pCCM->ccm_fis, 16);
	{
		u32		fis[4];
		fis[0] = *((u32 *)&pCtxt->CTXT_FIS[0]);
		fis[1] = *((u32 *)&pCtxt->CTXT_FIS[4]);
		fis[2] = *((u32 *)&pCtxt->CTXT_FIS[8]);
		fis[3] = *((u32 *)&pCtxt->CTXT_FIS[12]);

		*((u32 *)(&pCCM->ccm_fis[0])) = fis[0];
		*((u32 *)(&pCCM->ccm_fis[4])) = fis[1];
		*((u32 *)(&pCCM->ccm_fis[8])) = fis[2];
		*((u32 *)(&pCCM->ccm_fis[12])) = fis[3];
	}

	sata_append_scm(pSob, pScm);

	pCtxt->CTXT_LunStatus = LUN_STATUS_GOOD;

	pCtxt->CTXT_Status = CTXT_STATUS_PENDING;
	if (!(pCtxt->CTXT_FLAG & CTXT_FLAG_SYNC))
	{
		DBG("SYN return\n");
		return CTXT_STATUS_PENDING;
	}

	while (1)
	{
		if (pScm->scm_prot != PROT_ND)
		{
			if ((*usb_IntStatus_shadow & CTRL_INT ) && (*usb_Ep0Ctrl & EP0_SETUP))
			{
				// in the SATA sync mode, there's possible that the USB setup is asserted
				usb_control();
			}
		}
		if ((pScm->scm_prot == PROT_PIOIN) || (pScm->scm_prot == PROT_DMAIN))
		{
			if (sata_ch_reg->sata_IntStatus & RXDATARLSDONE)
			{
				u32	segIndex;				
				if ((segIndex = pScm->scm_SegIndex) != SEG_NULL)
				{
					// Disconnect input port
//					*((u8 *) &(Tx_Dbuf->dbuf_Seg[segIndex].dbuf_Seg_INOUT)) &= 0xf0;
					// fetch the data by CPU when there's no error happens
					if ((sata_ch_reg->sata_Status & ATA_STATUS_CHECK) == 0)
					{
#if 0
						dbuf_dbg(DBUF_SEG_DUMP_TXSEG1234);
						if (pCtxt->CTXT_FLAG & CTXT_FLAG_AES)
							MSG("aes_Ctrl %lx\n", *aes_control);
						Delay(1);
#endif
						read_data_from_cpu_port(pCtxt->CTXT_XFRLENGTH, DIR_TX, 1);
					}
#ifdef AES_EN		// disable the AES firstly
					if (pCtxt->CTXT_FLAG & CTXT_FLAG_AES)
					{
						*aes_control &= ~(AES_DECRYP_EN|AES_DECRYP_KEYSET_SEL);
					}
#endif					
					// reset sata FIFO
					sata_ch_reg->sata_BlkCtrl |= RXSYNCFIFORST;	//reset sata RX  fifo
					sata_ch_reg->sata_BlkCtrl;
					
					reset_dbuf_seg(segIndex);
					// reset Dbuf
					free_dbufConnection(segIndex);
					
					pScm->scm_SegIndex = SEG_NULL;
				}
				sata_ch_reg->sata_IntStatus = 0xFFFF;	// Write 1 clear
				sata_ch_reg->sata_FrameInt= D2HFISI;
				break;
			}
		}
		else if ((pScm->scm_prot == PROT_PIOOUT) || (pScm->scm_prot == PROT_DMAOUT))
		{
			if (sata_ch_reg->sata_IntStatus & DATATXDONE )
			{
				sata_ch_reg->sata_IntStatus = DATATXDONE;	
				reset_dbuf_seg(pCtxt->CTXT_DbufIndex);
				sata_ch_reg->sata_BlkCtrl |= TXSYNCFIFORST;	//reset sata TX  fifo

				// Disconnect both input & output port
				free_dbufConnection(pCtxt->CTXT_DbufIndex);
#ifdef AES_EN
				if (pCtxt->CTXT_FLAG & CTXT_FLAG_AES)
				{
					*aes_control &= ~(AES_ENCRYP_EN | AES_ENCRYP_KEYSET_SEL);	 // Disable Encryption
				}
#endif
				break;
			}
		}
		else
		{
			if (sata_ch_reg->sata_FrameInt & D2HFISI)
			{
				sata_ch_reg->sata_FrameInt= D2HFISI;

				// clear SATA Frame interrupt
				sata_ch_reg->sata_IntStatus = FRAMEINTPEND;
				sata_ch_reg->sata_BlkCtrl |= TXSYNCFIFORST;
				break;
			}
		}
		if (pScm->scm_TimeOutTick == 0)
		{
			sata_Reset(pSob, SATA_HARD_RST);
			return CTXT_STATUS_ERROR;
		}
	}

	delete_curSCM(pSob);
	pSob->sobj_State = SATA_READY;
	DBG("cmd done\n");
	if (pCtxt->CTXT_FLAG & CTXT_FLAG_RET_TFR)
	{
		sata_return_tfr(pSob, pCtxt);
	}

	// Check Status register of ATA TFR
	if (sata_ch_reg->sata_Status & ATA_STATUS_CHECK)
	{
		*(((u16 *) &(pCtxt->CTXT_FIS[FIS_STATUS]))) =  *((u16 *)(&sata_ch_reg->sata_Status));
		// generate SCSI Sense code
		hdd_ata_err_2_sense_data(pCtxt);
		return CTXT_STATUS_ERROR;
	}
	else
	{
		pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
		return CTXT_STATUS_GOOD;
	}
}


/****************************************\
 ata_exec_ncq_ctxt
   Execute a SATA NCQ command to seecified SATA OBJ
\****************************************/
u32 sata_exec_ncq_ctxt(PSATA_OBJ  pSob, PCDB_CTXT pCtxt)
{
	SATA_CCM volatile	*pCCM;
	EXT_CCM				*pScm;
	SATA_CH_REG volatile *sata_ch_reg;
	u32		CCMIndex;
	//DBG("\nsata_PhyCtrl:%lx\n",sata_ch_reg->sata_PhyCtrl);
	DBG("exec ncq\n");
	
	CCMIndex = sata_AllocateCCM(pSob);

	if (CCMIndex == CCM_NULL)
	{
		//CCM full 
		MSG("ccm full\n");
		pCtxt->CTXT_ccmIndex = CCM_NULL;
		pCtxt->CTXT_LunStatus = LUN_STATUS_BUSY;
		pCtxt->CTXT_Status = CTXT_STATUS_ERROR;
		return CTXT_STATUS_ERROR;
	}

	pCtxt->CTXT_ccmIndex = CCMIndex;
	pCtxt->CTXT_CONTROL |= pSob->sobj_sata_ch_seq ? CTXT_CTRL_SATA1_EN: CTXT_CTRL_SATA0_EN;

	pScm = &pSob->sobj_Scm[CCMIndex];
	pScm->scm_next = NULL;
	pScm->scm_pCdbCtxt = pCtxt;
//	pScm->scm_site = pCtxt->CTXT_Index; // comment in v0.02
	pScm->scm_prot = pCtxt->CTXT_Prot;
	pScm->scm_control = pSob->sobj_sata_ch_seq ? SCM_CONTROL_SATA1_EN : SCM_CONTROL_SATA0_EN;
	pScm->scm_SegIndex = pCtxt->CTXT_DbufIndex;

	sata_ch_reg = pSob->pSata_Ch_Reg;
	sata_ch_reg->sata_CCMSITEINDEX = (u8)CCMIndex;
	pCCM = &sata_ch_reg->sata_Ccm[CCMIndex];
	
	pCCM->ccm_prot		= FLAGS_V|FLAGS_X|pScm->scm_prot;
	pCCM->ccm_cmdinten	= SETDEVBITSIEN|DMASETUPIEN;//D2HFISIEN;
	if ((pCtxt->CTXT_DbufIndex & DBUF_RAID_FUNCTION_ENABLE) && ((raid_rebuild_verify_status & RRVS_IN_PROCESS) ==0))
		pCCM->ccm_xfercnt 	= raid_xfer_length;
	else
		pCCM->ccm_xfercnt 	= pCtxt->CTXT_XFRLENGTH;	

#ifdef ERR_RB_DBG  // for instant_RB here
	if (//(err_rb_dbg) && (arrayMode == RAID1_MIRROR) && 
		(pCtxt->CTXT_FIS[FIS_COMMAND] == ATA6_WR_FPDMA_QUEUED) &&
		(!(raid_rebuild_verify_status & RRVS_IN_PROCESS)))
	{
		// 28 00 00 00 10 00 00 xx xx 00  will cause a FAIL
		if (cal_count_bits(pSob->cFree) < 32)//((*((u32 *)&pCtxt->CTXT_FIS[FIS_LBA_LOW]) & 0x00FFFFFF) == 0x1000)
		{
			//MSG("insRBdbg FAIL entrance\n");
			//MSG("ctl %bx\n",pCtxt->CTXT_CONTROL);
			pCtxt->CTXT_FIS[FIS_LBA_HIGH_EXT] = 0x1;
		}
	}
#endif
//	xmemcpy(pCtxt->CTXT_FIS, (u8 *)pCCM->ccm_fis, 16);
	u32		fis[4];
	fis[0] = *((u32 *)&pCtxt->CTXT_FIS[0]);
	fis[1] = *((u32 *)&pCtxt->CTXT_FIS[4]);
	fis[2] = *((u32 *)&pCtxt->CTXT_FIS[8]);
	fis[3] = *((u32 *)&pCtxt->CTXT_FIS[12]);

	*((u32 *)(&pCCM->ccm_fis[0])) = fis[0];
	*((u32 *)(&pCCM->ccm_fis[4])) = fis[1];
	*((u32 *)(&pCCM->ccm_fis[8])) = fis[2];
	*((u32 *)(&pCCM->ccm_fis[12])) = fis[3];
	
	//set TAG at ccm_fis[12] by TAG_NUM << 3
	//ref. SATA spec. 2.6 Gold: p.336, p.456
	pCCM->ccm_fis[12] = CCMIndex << 3;

	sata_append_ncq_scm(pSob, pScm);

	pCtxt->CTXT_LunStatus = LUN_STATUS_GOOD;

	pCtxt->CTXT_Status = CTXT_STATUS_PENDING;
	DBG("site: %BX, tag: %BX\n", pCtxt->CTXT_Index, pCtxt->CTXT_ccmIndex);
	return CTXT_STATUS_PENDING;
}


/****************************************\
	sata_HardReset
\****************************************/
void sata_HardReset(PSATA_OBJ  pSob)
{
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;

	// Disable SATA PHY not ready Interrupt
	sata_ch_reg->sata_PhyIntEn &= ~PHYDWNIEN;

	MSG("sRst\n");
	sata_ch_reg->sata_BlkCtrl |= SBLKRESET;
	Delayus(1);
	sata_reg_init(pSob);

	sata_ch_reg->sata_PhyCtrl |= PHYRST;
	pSob->sobj_init = 0; 
	pSob->sobj_device_state = SATA_DEV_UNKNOWN;
#ifdef SATA2_ONLY
	sata_ch_reg->sata_PhyCtrl = (sata_ch_reg->sata_PhyCtrl & ~BIT_3) | BIT_4; // gen 2 only
#endif
#ifdef SATA1_ONLY
	sata_ch_reg->sata_PhyCtrl = (sata_ch_reg->sata_PhyCtrl & ~BIT_4) | BIT_3; // gen 1 only
#endif
#ifdef FORCE_NO_SATA3
	sata_ch_reg->sata_PhyCtrl = (sata_ch_reg->sata_PhyCtrl & (~PHYGEN3))
				| (PHYGEN2|PHYGEN1);
#endif
	sata_ch_reg->sata_BlkCtrl = (sata_ch_reg->sata_BlkCtrl & ~(DDONEFLUSH_DIS |DTXDDONEDIS|DTXSDONEDIS)) | (TXZERO_DIS |RXSYNCFIFORST | TXSYNCFIFORST);
//	sata_ch_reg->sata_PhyCtrl = (sata_ch_reg->sata_PhyCtrl & ~BIT_10) | BIT_9 | BIT_8; // force 3G
#ifdef SUPPORT_SATA_PM
	sata_ch_reg->sata_PhyCtrl |= PHYCMDWAKE;
#endif

#if WDC_HDD_TEMPERATURE_POLL_TICKS
	check_temp_flag = 0;
	expect_temperatureTimer32 = WDC_HDD_TEMPERATURE_POLL_TICKS;
#endif	
	// Enable SATA PHY ready Interrupt

	// clear all interuupt registers of SATA IP 
	sata_ch_reg->sata_PhyInt = 0xFFFFFFFF;
	sata_ch_reg->sata_FrameInt = 0xFFFFFFFF;
	sata_ch_reg->sata_IntStatus = 0xFFFF; // clr 
	sata_ch_reg->sata_UNKNFISCnt = 0;
	sata_ch_reg->sata_CRCErrCnt = 0;
	sata_ch_reg->sata_DECErrCnt = 0;
	sata_ch_reg->sata_DSPErrCnt = 0;

	// Enable SATA PHY ready Interrupt
	sata_ch_reg->sata_PhyIntEn |= PHYRDYIEN;
	sata_ch_reg->sata_FrameIntEn |= D2HFISIEN;
	sata_ch_reg->sata_HoldLv = 0x60;

	sata_ch_reg->sata_IntEn = RXDATARLSDONEEN|DATATXDONEEN|FRAMEINTPENDEN|PHYINTPENDEN;
#ifdef UAS
	if (ncq_supported)
	{
		sata_ch_reg->sata_FrameIntEn |= (SETDEVBITSIEN | DMASETUPIEN);
	}
#endif
	//sata_ch_reg->sata_FrameIntEn |= D2HFISIEN;

	pSob->sobj_State = SATA_RESETING;
	if (unsupport_boot_up & (UNSUPPORT_HDD0_INITIALIZED_FAIL<<pSob->sobj_sata_ch_seq))
	{
		unsupport_boot_up &= ~(UNSUPPORT_HDD0_INITIALIZED_FAIL<<pSob->sobj_sata_ch_seq);
	}
}

void sata_timeout_reset(PSATA_OBJ  pSob)
{
	if (pSob->sobj_curScm)
	{
		u8 SegIndex = pSob->sobj_curScm->scm_SegIndex;
		if (SegIndex != SEG_NULL)
		{
			free_dbufConnection(SegIndex);
		}
	}
	sata_HardReset(pSob);
	sata_InitSataObj(pSob);
}

/****************************************\
	sata_Reset
\****************************************/
void sata_Reset(PSATA_OBJ  pSob, u32 rst_cmd)
{
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;

	switch (rst_cmd)
	{
		case USB_SATA_RST:
			if (pSob->sobj_State > SATA_READY)
			{	// sata obj is not in SATA_ACTIVE, SATA_NCQ_FLUSH, or SATA_NCQ_ACTIVE state
				DBG("S2\n");
				sata_HardReset(pSob);
				sata_InitSataObj(pSob);
				return;
			}
			sata_InitSataObj(pSob);
			return;

		case SATA_HARD_RST:
			DBG("S3\n");
			sata_HardReset(pSob);
			_disable();
			chk_fis34_timer = CHECK_FIS34_TIMER;
			spinup_timer = 3; 
			_enable();

			//sata_abort_all_usb_io(pSob);

			sata_InitSataObj(pSob);

			break;

		case SATA_SOFT_RST:
			sata_ch_reg->sata_DEVCTRL = DEVICE_CRTL_SRST;
			Delay(20);
			sata_ch_reg->sata_DEVCTRL = 0;

			// 	clear SATA Frame Interrupt
			sata_ch_reg->sata_FrameInt = 0xffffffff;

			// clear SATA interrupt
			sata_ch_reg->sata_IntStatus = 0xffff;

			// 	Enable D2HFIS Interrupt for Signature FIS
			sata_ch_reg->sata_FrameIntEn |= D2HFISIEN;

			pSob->sobj_State = SATA_PHY_RDY;

			//sata_abort_all_usb_io(pSob);

			sata_InitSataObj(pSob);

			break;

	}
}


u8 chk_fis34(PSATA_OBJ  pSob)
{
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;
	u8 val = sata_ch_reg->sata_Status;
	if (val != 0xFF)
	{	
		// RX FIS 34
		// chk Busy Bit of TFR Status Register
		if ((val & ATA_STATUS_BSY) == 0)
		{	
			// chk Error Bit of TFR Status Register
			if ((val & ATA_STATUS_CHECK) == 0)
			{
				if ((sata_ch_reg->sata_SectCnt == 0x01) &&
					(sata_ch_reg->sata_LbaL == 0x01))
				{
					if ((sata_ch_reg->sata_LbaM == 0x14) &&
						(sata_ch_reg->sata_LbaH == 0xEB))
					{	// ATAPI device
						// doesn't support ODD
						return 0;
					}
					else if ((sata_ch_reg->sata_LbaM == 0x00) &&
							 (sata_ch_reg->sata_LbaH == 0x00))
					{
						pSob->sobj_class = DEVICECLASS_ATA;
						pSob->sobj_State = SATA_PRE_READY; // firmware will initialize the HDD
						MSG("ATA%bx DEV\n", pSob->sobj_sata_ch_seq);
						return 0;
					}
			
				}
			}
			return 0;
		}
		else
			return 1;
	}
	else
		return 1;
}


/****************************************\
	scan_sata_device
\****************************************/
void scan_sata_device(u8 sata_ch_flag)
{
	MSG("Scan SATA %bx\n", sata_ch_flag);
	
	u8 sata2_retry_count = 5;
	SATA_CH_REG volatile *sata_ch_reg = NULL;
	u16 i, ii = 0;
	for (i = 0; i < 2; i++)
	{
		if ((sata_ch_flag & (1 << i)) == 0)
		{
			continue;
		}
			
		// reset 2 SATA channel
		sata_ch_reg = i ? sata_Ch1Reg : sata_Ch0Reg;
		sata_ch_reg->sata_PhyCtrl &= ~(PHYPWRUP|PHYGEN3|PHYGEN2|PHYGEN1);
		//check gen1 or gen2
#ifdef FORCE_NO_SATA3
		sata_ch_reg->sata_PhyCtrl = (sata_ch_reg->sata_PhyCtrl & (~PHYGEN3))
				| (PHYPWRUP|PHYGEN2|PHYGEN1);
#else		
		sata_ch_reg->sata_PhyCtrl |= (PHYPWRUP|PHYGEN3|PHYGEN2|PHYGEN1);	
#endif
		sata_ch_reg->sata_FrameIntEn &= ~D2HFISIEN;
		sata_ch_reg->sata_Status = 0xFF;
		sata_ch_reg->sata_PhyInt = 0xFFFFFFFF;
		sata_ch_reg->sata_FrameInt = 0xFFFFFFFF;
		sata_ch_reg->sata_IntStatus = 0xFFFF;
	}
	if (sata_ch_flag & SATA_CH0)
		sata0Obj.sobj_class = DEVICECLASS_NONE;
	if (sata_ch_flag & SATA_CH1)
		sata1Obj.sobj_class = DEVICECLASS_NONE;
_CHK_PHY_RDY:	
	for (i = 0; i < 3; i++)
	{
		// check the SATA phy ready for 3 times, everytime 2.5secs
		// the PHY ready should be asserted soon, regularly less than 2secs.
		if (((sata0Obj.sobj_State < SATA_PHY_RDY) ||(sata0Obj.sobj_State > SATA_READY)) && (sata_ch_flag & SATA_CH0))
		{
			sata_HardReset(&sata0Obj);
		}
		if (((sata1Obj.sobj_State < SATA_PHY_RDY) ||(sata1Obj.sobj_State > SATA_READY)) && (sata_ch_flag & SATA_CH1))
		{
			sata_HardReset(&sata1Obj);
		}
		// check if SATA Phy ready (OOB passed)
//		for (ii= 0; ii < 2500; ii++)
		for (ii= 0; ii < 6000; ii++)
		{	
			tickle_activity();
			Delay(1);	
			if (((sata0Obj.sobj_State == SATA_PHY_RDY) || !(sata_ch_flag & SATA_CH0)) 
			&& ((sata1Obj.sobj_State == SATA_PHY_RDY) || !(sata_ch_flag & SATA_CH1)))
				break;
			else
			{
				if ((sata0Obj.sobj_State != SATA_PHY_RDY) && (sata_ch_flag & SATA_CH0))
				{
					sata_ch_reg = sata0Obj.pSata_Ch_Reg;
					if (sata_ch_reg->sata_PhyStat & PHYRDY)
					{
						sata0Obj.sobj_State = SATA_PHY_RDY;
						sata0Obj.sobj_class = DEVICECLASS_UNKNOWN;
						MSG("S0 PhyRdy\n");
					}
				}
				if ((sata1Obj.sobj_State != SATA_PHY_RDY) && (sata_ch_flag & SATA_CH1))
				{
					sata_ch_reg = sata1Obj.pSata_Ch_Reg;
					if (sata_ch_reg->sata_PhyStat & PHYRDY)
					{
						sata1Obj.sobj_State = SATA_PHY_RDY;
						sata1Obj.sobj_class = DEVICECLASS_UNKNOWN;
						MSG("S1 PhyRdy\n");
					}
				}
			}
		}	
	}
	
// if 1/2 channels are PHY ready, check Fis34
// otherwise return
	if (((sata0Obj.sobj_State == SATA_PHY_RDY) && (sata_ch_flag & SATA_CH0))
	|| ((sata1Obj.sobj_State == SATA_PHY_RDY) && (sata_ch_flag & SATA_CH1)))
	{
		// check the Fis34
		for (ii= 0; ii < 300; ii++)
		{
			tickle_activity();
			// one channel has received FIS34, and the other channel receives Fis34 also or it's not PHY ready
			// the channels scan should finish
			if ((sata0Obj.sobj_State == SATA_PHY_RDY) && (sata_ch_flag & SATA_CH0))
			{
				if ((chk_fis34(&sata0Obj) == 0) && (sata1Obj.sobj_State != SATA_PHY_RDY))
					break;
			}
			if ((sata1Obj.sobj_State == SATA_PHY_RDY) && (sata_ch_flag & SATA_CH1))
			{
				if ((chk_fis34(&sata1Obj) == 0) && (sata0Obj.sobj_State != SATA_PHY_RDY))
					break;
			}
			
			MSG(".");
			Delay(10);
		}
		sata2_retry_count--;
		for (i = 0; i < 2; i++)
		{
			// reset 2 SATA channel
			if ((sata_ch_flag & (1 << i)) == 0)
			{
				continue;
			}
			sata_ch_reg = i ? sata_Ch1Reg : sata_Ch0Reg;
			sata_ch_reg->sata_FrameIntEn &= ~D2HFISIEN;
			sata_ch_reg->sata_PhyInt = 0xFFFFFFFF;
			sata_ch_reg->sata_FrameInt = 0xFFFFFFFF;
			sata_ch_reg->sata_IntStatus = 0xFFFF;
		}
		// for the case that the phy ready is set, but the Fis34 is not received or the unknow Fis34, F/W should reset the SATA channel agains
		if ((((sata0Obj.sobj_State == SATA_PHY_RDY) && (sata_ch_flag & SATA_CH0))
		|| ((sata1Obj.sobj_State == SATA_PHY_RDY) && (sata_ch_flag & SATA_CH1))) && (sata2_retry_count != 0))
		{
			MSG("Reset again\n");
			goto _CHK_PHY_RDY;
		}
	}
	else
	{
		MSG("No SATA Dev\n");
		return;
	}
}

void sata_reg_init(PSATA_OBJ  pSob)
{
	SATA_CH_REG volatile *sata_ch_reg = pSob->pSata_Ch_Reg;
#ifdef FORCE_NO_SATA3
	sata_ch_reg->sata_PhyCtrl = (sata_ch_reg->sata_PhyCtrl & (~PHYGEN3))
				| (PHYPWRUP|PHYGEN2|PHYGEN1);
#else	
	sata_ch_reg->sata_PhyCtrl	= PHYPWRUP|PHYGEN3|PHYGEN2|PHYGEN1;
#endif
	sata_ch_reg->sata_CurrTag	= 0xFF;
	sata_ch_reg->sata_EXQCtrl	= (EXQHSTRT|EXQNSTRT);

	// Disable auto reset of SATA BLK  TXSYNCFIFO & RXSYNCFIFO
	sata_ch_reg->sata_BlkCtrl  &= ~(TXSYNCFIFOAUTO |RXSYNCFIFOAUTO);
	sata_ch_reg->sata_Rsvd168[0]	|=	(BIT_3|BIT_2|BIT_1|BIT_0); // fowllowing ASIC's instruction in 12/29/10 for compliance test

#if 0 //def NCQ
	// if set BIT5, HW auto fill CurrTag with RXTag while CurrTag is 0xFF when HD send DMA Setup
	//sata_ch_reg->sata_BlkCtrl |= BIT_5;
	
	//FrameIntEn must enable, even for polling usage.
	sata_ch_reg->sata_FrameIntEn = (SETDEVBITSIEN | DMASETUPIEN);
#endif //NCQ


}

void sata_active_scm_count_down(PSATA_OBJ  pSob)
{
	EXT_CCM *pScm = pSob->sobj_curScm;

	if (pScm)
	{
		if (pScm->scm_TimeOutTick)
		{
			pScm->scm_TimeOutTick--;	
		}
	}

}

void sata_InitSataObj(PSATA_OBJ  pSob)
{
	u32 i;
	EXT_CCM *pScm = pSob->sobj_Scm;
	for(i=0; i<32; i++, pScm++)
	{
		pScm->scm_next = NULL;
		pScm->scm_pCdbCtxt = NULL;
//		pScm->scm_site = NULL;
//		pScm->scm_prot = NULL;
		pScm->scm_ccmIndex = i;
	}

	// assume SATA device w/o NCQ
	pSob->cFree = pSob->default_cFree;

	pSob->sobj_sBusy = 0;
	pSob->sobj_que = NULL;
	pSob->sobj_ncq_que = NULL;
	pSob->sobj_curScm = NULL;

//	pSataObj->sataStatus = 0;
//	pSataObj->AllocateCCM = sata_AllocateCCM;
//	pSataObj->DetachCCM = sata_DetachCCM;
}

void pwr_on_sata_init(void)
{

#ifndef FPGA
	// power up SATA PLL to make sure PLL is on
	spi_phy_wr(PHY_SPI_SATA0, 0x20, 0); 
	spi_phy_wr(PHY_SPI_SATA1, 0x20, 0); 
	sata_wait_PLL_rdy();
#endif

	sata_ch_reg = sata_Ch0Reg;
	sata0Obj.pSata_Ch_Reg = sata_Ch0Reg;
	sata0Obj.sobj_sata_ch_seq = 0;
	sata0Obj.sobj_State = SATA_RESETING;
	sata0Obj.default_cFree = 0x01;
	sata1Obj.pSata_Ch_Reg = sata_Ch1Reg;
	sata1Obj.sobj_sata_ch_seq = 1;
	sata1Obj.sobj_State = SATA_RESETING;
	sata1Obj.default_cFree = 0x01;
	pSataObj = (PSATA_OBJ)(&sata0Obj);

	sata_InitSataObj(pSataObj);	
	sata_reg_init(pSataObj);	
#ifdef RAID
	pSataObj = (PSATA_OBJ)(&sata1Obj);
	sata_InitSataObj(pSataObj);	
	sata_reg_init(pSataObj);
#endif	
}

u8 sata_await_fis34(u8 flag)
{
MSG("aw fis34\n");
	for (sata_spinup_timer = 0; sata_spinup_timer < 7000; sata_spinup_timer++) // it's always very quick
	{
		for (u8 i = 0; i < 2; i++)
		{
			if (flag == SATA_CH1)
				i = 1;
				
			pSataObj = ((SATA_OBJ *)&sata0Obj + i);
			sata_ch_reg = pSataObj->pSata_Ch_Reg;
			// check for SATA phy ready
			if ((sata_ch_reg->sata_PhyInt & PHYRDYI) && (pSataObj->sobj_State< SATA_PHY_RDY))
			{
				// Disable SATA PHY Ready Interrupt
				sata_ch_reg->sata_PhyIntEn &= ~PHYRDYIEN;

				// clear SATA PHY Ready & Down interrupt
				sata_ch_reg->sata_PhyInt = (PHYDWNI|PHYRDYI);

				// clear SATA PHY interrupt
				sata_ch_reg->sata_IntStatus = PHYINTPEND;

				// Enable SATA PHY Down Interrupt
				sata_ch_reg->sata_PhyIntEn |= PHYDWNIEN;

				pSataObj->sobj_State = SATA_PHY_RDY;
				MSG("Phy%bx RDY\n", pSataObj->sobj_sata_ch_seq);
			}
			if (((sata_spinup_timer & 0x1FF) == 0x1FF) && (pSataObj->sobj_State< SATA_PHY_RDY))// every 500ms, if PHY is not ready, reset once
				sata_HardReset(pSataObj);
			
			if (pSataObj->sobj_State == SATA_PHY_RDY)
			{
				u8 val = sata_ch_reg->sata_Status;
				if (val != 0xFF)
				{
					if ((val & ATA_STATUS_BSY) == 0)
					{
						if ((val & ATA_STATUS_CHECK) == 0)
						{
							if ((sata_ch_reg->sata_LbaM == 0x00) && (sata_ch_reg->sata_LbaH == 0x00))
							{
								MSG("s Rdy%x\n", sata_spinup_timer);
								sata_ch_reg->sata_FrameIntEn &= ~D2HFISIEN;
								// clear D2HFIS interrupt
								sata_ch_reg->sata_FrameInt = D2HFISI;
								// clear SATA Frame interrupt
								sata_ch_reg->sata_IntStatus = FRAMEINTPEND;
								pSataObj->sobj_State = SATA_READY;
							}
						}
					}
				}
			}
			
			if (flag == SATA_CH0)
				break;
		}
		if (flag == SATA_TWO_CHs)
		{
			if ((sata0Obj.sobj_State == SATA_READY) && (sata1Obj.sobj_State == SATA_READY))
				return 0;
		}
		else
		{
			if (pSataObj->sobj_State == SATA_READY)
				return 0;
		}
		Delay(1);
	}
	return 1;
}

void sata_wait_PLL_rdy(void)
{
	while (1) // it need to be debugged in ASIC
	{
		if (*chip_IntStaus & SATA_PLL_RDY)
		{
			break;
		}
	}
}

void sata_phy_pwr_ctrl(u8 pwr_on_off, u8 sata_ch_flag)
{
	if (*chip_Revision >= 0x03)
	{
		u8 tmp = 0;
		u8 pwr_up_PLL = 0;
		#define PD_CDR		BIT_7
		#define PD_SER		BIT_6
		#define PD_TXDR		BIT_5
		#define PD_PLL		BIT_4
		#define PD_AFE_VREG	BIT_3
		#define PD_AFE_BIAS	BIT_2
		#define PD_INBUF	BIT_1
		#define PD_SQDET	BIT_0

		
		if (pwr_on_off == SATA_PHY_PWR_DWN)
		{
			/* 
			PWR_SAVING_DOWN___SATA3 -1____00005
			PWR_SAVING_UP___XYZ9 - 8___SATA3 -1____00005
			0x00:Power up  / 0xFF: Power down
			CDR module<b7> & serial module<b6> & TX driver<b5> & PLL<b4> & voltage regulator<b3> 
			Power analog front end bias<b2> & input buffer<b1> & squelch detection module<b0> 
			*/
			tmp = 0xFF; 
		}
		else
		{// power on PLL first because Two SATA channels share the PLL
			if (spi_phy_rd(PHY_SPI_SATA0, 0x20) & PD_PLL)
			{
				pwr_up_PLL = 1;
			}
			spi_phy_wr_retry(PHY_SPI_SATA0, 0x20, (spi_phy_rd(PHY_SPI_SATA0,0x20) & 0xEF));
		}

		if (sata_ch_flag & SATA_CH0)
		{

			#ifdef DBG_PWR_SAVING
			MSG("PWR CTL SLOT0: %bx  STATUS: %bx\n", tmp,sata0Obj.sobj_State);
			#endif

			if((sata1Obj.sobj_State != SATA_PWR_DWN) && (pwr_on_off == SATA_PHY_PWR_DWN))
			{
				tmp = 0xEF;	//make sure only slot 1  condition , sata still have PLL.
			}
			
			spi_phy_wr_retry(PHY_SPI_SATA0, 0x20, tmp);// SATA PLL power	
		}
		else
		{
			#ifdef DBG_PWR_SAVING
			MSG("PWR CTL SLOT1: %bx  STATUS: %bx\n", tmp,sata0Obj.sobj_State);
			#endif

			spi_phy_wr_retry(PHY_SPI_SATA1, 0x20, tmp);
			
			if ((sata0Obj.sobj_State == SATA_PWR_DWN) && (pwr_on_off == SATA_PHY_PWR_DWN))
			{//channel 0 has been shutdown, so shut down the PLL
				spi_phy_wr_retry(PHY_SPI_SATA0, 0x20, 0xFF);
			}
			
		}

		if (pwr_on_off == SATA_PHY_PWR_UP)
		{
			if (pwr_up_PLL)
			{
				sata_wait_PLL_rdy();
			}
			MSG("RST ST TX\n");
			u8 temp8 = spi_phy_rd(PHY_SPI_SATA0, 0x10);
			Delayus(200);
			if (sata_ch_flag == SATA_CH1)
			{
				spi_phy_wr_retry(PHY_SPI_SATA0, 0x10, (temp8 & ~0x10));
				MSG("ST[0x10] %bx\n", spi_phy_rd(PHY_SPI_SATA0, 0x10));
				spi_phy_wr_retry(PHY_SPI_SATA0, 0x10, temp8);
				MSG("ST[0x10] %bx\n", spi_phy_rd(PHY_SPI_SATA0, 0x10));
			}
			else
			{
				spi_phy_wr_retry(PHY_SPI_SATA0, 0x10, (temp8 & ~0x08));
				MSG("ST[0x10] %bx\n", spi_phy_rd(PHY_SPI_SATA0, 0x10));
				spi_phy_wr_retry(PHY_SPI_SATA0, 0x10, temp8);
				MSG("ST[0x10] %bx\n", spi_phy_rd(PHY_SPI_SATA0, 0x10));		
			}
		}
	}
}
