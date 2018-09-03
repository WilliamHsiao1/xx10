/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/

#define USB_HDD_C

#include "general.h"
#ifdef LA_DUMP
u32 rewind_cnt;
u32 msc0_proto_retry_LA_ADR;
u16 retry_cnt;
u32 dword0, dword1, dword2;
#endif

// this is shared by BOT & UAS, to reduce code size(32 bytes)
u8 check_IILegal_boot(PCDB_CTXT pCtxt)
{
	if ((CmdBlk(0) == SCSI_INQUIRY)
		|| (CmdBlk(0) == SCSI_REQUEST_SENSE) || (CmdBlk(0) == SCSI_REPORT_LUNS) 
		||((CmdBlk(0) == SCSI_READ_BUFFER10) || (CmdBlk(0) == SCSI_WRITE_BUFFER10))
		|| (( /* just hdd lun supported cmd*/
#ifdef WDC_CHECK
			(CmdBlk(0) == SCSI_WDC_FACTORY_CONFIG) || (CmdBlk(0) == SCSI_WDC_MAINTENANCE_IN) 
			|| (CmdBlk(0) == SCSI_WDC_MAINTENANCE_OUT) || (CmdBlk(0) == SCSI_START_STOP_UNIT)
			|| (CmdBlk(0) == SCSI_SEND_DIAGNOSTIC) || (CmdBlk(0) == SCSI_RCV_DIAG_RESULTS)
			|| (CmdBlk(0) == SCSI_LOG_SELECT) || (CmdBlk(0) == SCSI_LOG_SENSE)
#ifdef DBG_FUNCTION
			|| (CmdBlk(0) == SCSI_WDC_ENCRYPTION_STATUS)	
			|| (CmdBlk(0) == SCSI_WDC_SECURITY_CMD)
#endif	
			/*|| (CmdBlk(0) == SCSI_TEST_UNIT_READY) */
			|| 
#endif
			(CmdBlk(0) == SCSI_ATA_PASS_THR) || (CmdBlk(0) == SCSI_ATA_PASS_THR16)) 
			&& ((logicalUnit == HDD0_LUN) || (logicalUnit == HDD1_LUN))))
	{
		// supported command
		return 0;
	}
	else
	{
		if (flash_type == UNKNOWN_FLASH)
		{
			hdd_err_2_sense_data(pCtxt, ERROR_INVALID_FLASH);
		}
#ifdef DATABYTE_RAID
		else if (cfa_active == 1)
		{
			hdd_err_2_sense_data(pCtxt, ERROR_UA_NO_MEDIA);
		}
#endif
		else if (((logicalUnit == HDD0_LUN) && (array_status_queue[0] == AS_NOT_CONFIGURED)) 
			|| ((logicalUnit == HDD1_LUN) && (array_status_queue[1] == AS_NOT_CONFIGURED)))
		{
			// In not config mode, we don't set vcd & ses lun, so we don't need cosider them
			hdd_err_2_sense_data(pCtxt, LOGICAL_UNIT_NOT_CONFIGURED);
		}
		else if (((logicalUnit == HDD0_LUN) && (array_status_queue[0] == AS_BROKEN))
			|| ((logicalUnit == HDD1_LUN) && (array_status_queue[1] == AS_BROKEN)))
		{
			hdd_err_2_sense_data(pCtxt, ERROR_HDD_FAIL_INITIALIZATION);
		}
#ifdef WDC_CHECK		
		else if (unsupport_boot_up & UNSUPPORT_CHIP_REV_BOOT)
		{
			hdd_err_2_sense_data(pCtxt, ERROR_INVALID_CHIP_VER);
		}
		else if (unsupport_boot_up & UNSUPPORT_HW_STRAPPING_BOOT)
		{
			hdd_err_2_sense_data(pCtxt, ERROR_INVALID_PCBA);
		}
#endif		
		else
			return 0; // goto EXEC command
		pCtxt->CTXT_usb_state = CTXT_USB_STATE_CTXT_SETUP_DONE;
		usb_device_no_data(pCtxt);
		return 1; // continue						
	}
}

#ifdef DBG_FUNCTION
#ifdef LA_DUMP
#if 0
void dump_SM_LA(void)
{
	// dump the SM LA
	msc0_proto_retry_LA_ADR = *usb_MSC0_SM_LA_ADR;
	MSG("SM LA Adr %lx\n", msc0_proto_retry_LA_ADR);
	*usb_MSC0_SM_LA_ADR = (msc0_proto_retry_LA_ADR & ~(LA_ADR|LA_ENABLE)) | ((msc0_proto_retry_LA_ADR & LA_ADR) - 8);
	retry_cnt = (u16)((msc0_proto_retry_LA_ADR & LA_CNT) >> 16);
	for (int i = 0; i < (retry_cnt * 2); i++)
	{
		if ((i & 0x01) == 0x01)
		{
			dword1 = *usb_MSC0_SM_LA_DATA;
			MSG("T=%x C=%x\t", (u16)(dword1>>16), (u16)(dword1));
			Delayus(100);
			MSG("TXRX=%bx TX=%bx ", (u8)(dword0>>24), (u8)(dword0>>16));
			Delayus(50);
			MSG("RX2=%bx RX1=%bx\n", (u8)(dword0>>8), (u8)(dword0));
		}
		else
		{
			dword0 = *usb_MSC0_SM_LA_DATA;
		}

		if ((i & 0x3) == 0x3)
			Delay(1);
	}
	MSG("\n");	
}
void dump_Conn_LA(void)
{
	msc0_proto_retry_LA_ADR = *usb_Connection_LA_ADR;
	MSG("Connection_LA_ADR %lx\n", msc0_proto_retry_LA_ADR);
	retry_cnt = (u16)((msc0_proto_retry_LA_ADR & LA_CNT) >> 16);
	*usb_Connection_LA_ADR = (msc0_proto_retry_LA_ADR & ~(LA_ADR|LA_ENABLE)) |((msc0_proto_retry_LA_ADR & LA_ADR) - 8);
	for (int i = 0; i < (retry_cnt * 2); i++)
	{
		if ((i & 0x01) == 0x01)
		{
			dword1 = *usb_Connection_LA_DATA;
			MSG("%lx\t", dword1);
			Delayus(50);
			MSG("%lx\n", dword0);
			Delayus(50);
		}
		else
		{
			dword0 = *usb_Connection_LA_DATA;
		}
		if ((i & 0x3) == 0x3)
			Delayus(200);
	}
	MSG("\n");
}
#endif
void dump_U3_PM_LA(void)
{
	MSG("RXELECIDLE_FILTER %bx\n", *usb_USB3_Rxelecidle_Filter); 
	msc0_proto_retry_LA_ADR = *usb_PM_LA_ADR;
	MSG("USB3_PM_LA_ADR %lx\n", msc0_proto_retry_LA_ADR);
	retry_cnt = (u16)((msc0_proto_retry_LA_ADR & LA_CNT) >> 16);
	*usb_PM_LA_ADR = (msc0_proto_retry_LA_ADR & ~(LA_RESET|LA_ENABLE)) |((msc0_proto_retry_LA_ADR & LA_ADR) - 8);
#if 0
	for (int i = 0; i < (retry_cnt * 2); i++)
	{
		if ((i & 0x01) == 0x01)
		{
			dword0 = *usb_PM_LA_DATA_low;
			dword1 = *usb_PM_LA_DATA_high;
			MSG("%lx\t", dword0);
			Delayus(50);
			MSG("%lx\n", dword1);
			Delayus(50);
		}
		else
		{
			dword0 = *usb_PM_LA_DATA_low;
			dword1 = *usb_PM_LA_DATA_high;
			MSG("%lx\t", dword0);
			Delayus(50);
			MSG("%lx\n", dword1);
			Delayus(50);
		}
		if ((i & 0x3) == 0x3)
			Delayus(200);
	}
#else
	for (u16 i=(retry_cnt * 3); i>0;i-=3)
	{
		dword0 = *usb_PM_LA_DATA_low;
		dword1 = *usb_PM_LA_DATA_low;
		dword2 = *usb_PM_LA_DATA_low;
		MSG("DW2 %lx\t", dword2);
		Delayus(400);
		MSG("DW1 %x, %x\t", (u16)(dword1>>16), (u16)dword1);
		Delayus(500);
		MSG("DW0 %lx\n", dword0);
		Delayus(400);
	}

#endif
	MSG("\n");
}
#endif

void dump_all_regs(PCDB_CTXT pCtxt)
{
	MSG("u fifo cnt %lx, RxXferCount %lx, DOutCtrl %bx, DOutClr %bx, DOXferStatus %bx, XferStatus %bx, CmdCtrl %bx, StatCtrl %bx, RxAvailStatus %bx\n", 
		*usb_Msc0_FIFO_CNT, *usb_Msc0RxXferCount, *usb_Msc0DOutCtrl, *usb_Msc0DOutCtrlClr, *usb_Msc0DOXferStatus, *usb_Msc0XferStatus,*usb_Msc0CmdCtrl,*usb_Msc0StatCtrl, *usb_Msc0RxAvail_status);
	MSG("RxCtxt %x, DICtrl %bx, DICtrlClr %bx, DIXferStatus %bx, TxAvailStatus %bx, Msc0Ctrl %x, RecoveryError %bx\n", *usb_Msc0RxCtxt, *usb_Msc0DICtrl, *usb_Msc0DICtrlClr, *usb_Msc0DIXferStatus, *usb_Msc0RxAvail_status, *usb_Msc0Ctrl,*usb_RecoveryError);
	MSG("TxXferCount %lx, TxCtxt %x, ct used %x\n", *usb_Msc0TxXferCount, *usb_Msc0TxCtxt, *usb_Msc0CtxtUsed);
	// dump raid configuration
	MSG("RaidConfig %lx, array_status %bx %bx\n", *raid_config, array_status_queue[0], array_status_queue[1]);
	if (raid_mirror_status)
	{
		MSG("mirrStatus %bx, rbState %bx, raid1ActiveTm %bx, raid1RbStatus %bx, instRB %bx\n", raid_mirror_status, raid_rb_state, raid1_active_timer, raid_rebuild_verify_status, instant_rebuild_flag);
	}
	MSG("chipInt %lx\n", *chip_IntStaus);
	if (curMscMode == MODE_UAS)
	{
		MSG("UAS: uas_pause %x, sm %bx\n", uas_ci_paused, uas_cmd_state);
		if (pCtxt)
		{
			MSG("iTag %bx\n", (u8)pCtxt->CTXT_ITAG);
		}
	}
	else
	{
		MSG("BotActive %bx\n", bot_active);
	}
	dump_stdTimer();
	MSG("Sata 0: ");
	sata_ch_reg = sata_Ch0Reg;
	MSG("Status %bx, Cfree %lx, DataTxStat %x, DataRxStat %x, TCnt %lx, sobjState %bx, subState%bx, ssubState %bx, devState %bx, RxFIFOCtrl %bx\n",sata_ch_reg->sata_Status, sata0Obj.cFree, sata_ch_reg->sata_DataTxStat, sata_ch_reg->sata_DataRxStat, sata_ch_reg->sata_TCnt, sata0Obj.sobj_State, sata0Obj.sobj_subState, sata0Obj.sobj_subsubState, sata0Obj.sobj_device_state, (u8)sata_ch_reg->sata_RxFFCtrl);
	if (sata_ch_reg->sata_Status & ATA_STATUS_CHECK)
		MSG("sataErr %bx\n", sata_ch_reg->sata_Error);
	MSG("sataInt %x, FramInt %x\n", sata_ch_reg->sata_IntStatus, sata_ch_reg->sata_FrameInt);
	MSG("sataIntEn %x, FramIntEn %x\n", sata_ch_reg->sata_IntEn, sata_ch_reg->sata_FrameIntEn);
	MSG("sata_phyIntEn: %lx sata_phyStat: %x sata_phyInt: %lx\n", sata_ch_reg->sata_PhyIntEn, sata_ch_reg->sata_PhyStat, sata_ch_reg->sata_PhyInt);
	sata_ch_reg->sata_SMCtrl = 0x01;
	MSG("pcl st %x\n", sata_ch_reg->sata_SMSTAT);
	sata_ch_reg->sata_SMCtrl = 0x40;
	MSG("ccm st %x\n", sata_ch_reg->sata_SMSTAT);
	MSG("ERR Cnt %lx, %lx, %lx, %lx\n", sata_ch_reg->sata_UNKNFISCnt, sata_ch_reg->sata_CRCErrCnt, sata_ch_reg->sata_DECErrCnt, sata_ch_reg->sata_DSPErrCnt);

	MSG("Sata 1: ");
	sata_ch_reg = sata_Ch1Reg;
	MSG("Status %bx, Cfree %lx, DataTxStat: %x, DataRxStat %x, TCnt %lx, sobjState %bx, subState%bx, ssubState %bx, devState %bx, RxFIFOCtrl %bx\n",sata_ch_reg->sata_Status, sata1Obj.cFree, sata_ch_reg->sata_DataTxStat, sata_ch_reg->sata_DataRxStat, sata_ch_reg->sata_TCnt, sata1Obj.sobj_State, sata1Obj.sobj_subState, sata1Obj.sobj_subsubState,sata1Obj.sobj_device_state, (u8)sata_ch_reg->sata_RxFFCtrl);
	if (sata_ch_reg->sata_Status & ATA_STATUS_CHECK)
		MSG("sataErr %bx\n", sata_ch_reg->sata_Error);
	MSG("sataInt %x, FramInt %x\n", sata_ch_reg->sata_IntStatus, sata_ch_reg->sata_FrameInt);
	MSG("sataIntEn %x, FramIntEn %x\n", sata_ch_reg->sata_IntEn, sata_ch_reg->sata_FrameIntEn);
	MSG("sata_phyIntEn: %lx sata_phyStat: %x sata_phyInt: %lx\n", sata_ch_reg->sata_PhyIntEn, sata_ch_reg->sata_PhyStat, sata_ch_reg->sata_PhyInt);
	sata_ch_reg->sata_SMCtrl = 0x01;
	MSG("pcl st %x\n", sata_ch_reg->sata_SMSTAT);
	sata_ch_reg->sata_SMCtrl = 0x40;
	MSG("ccm st %x\n", sata_ch_reg->sata_SMSTAT);
	MSG("ERR Cnt %lx, %lx, %lx, %lx\n", sata_ch_reg->sata_UNKNFISCnt, sata_ch_reg->sata_CRCErrCnt, sata_ch_reg->sata_DECErrCnt, sata_ch_reg->sata_DSPErrCnt);

	MSG("aes_Ctrl %lx\n", *aes_control);

	#ifdef SATARXFIFO_CTRL1
	MSG("rxfifo22 %lx\n", sata_ch_reg->sata_PhyCtrl); 
	#endif					

	//sata_ch_reg->sata_DataFIFOCtrl &= ~RXDATAFIFOBB;
	MSG("usb_IntStatus %lx, Msc0IntStatus %lx, L3 %x\n", *usb_IntStatus, *usb_Msc0IntStatus, *usb_USB3LTSSM_shadow);
	MSG("control ctrl %bx\n", *usb_Ep0Ctrl);
	{
		*usb_USB3StateSelect = 0x0A;
		u8 state_mc = *usb_USB3StateCtrl;
		MSG("Do state %bx\n", state_mc);
		*usb_USB3StateSelect = 0x09;
		state_mc = *usb_USB3StateCtrl;
		MSG("Di state %bx\n", state_mc);
		*usb_USB3StateSelect = MSC0_CMD_BUF;
		*usb_USB3StateSelect;
		state_mc = *usb_USB3StateCtrl;
		MSG("CmdBuf State %bx\n", state_mc);	
		*usb_USB3StateSelect = MSC0_STATUS_CONTROL;
		*usb_USB3StateSelect;
		state_mc = *usb_USB3StateCtrl;
		MSG("StatusCtrl State %bx\n", state_mc);	
	}
	if (pCtxt)
	{
		MSG("ctxtXferLen %lx, dbufIdx %bx, ", pCtxt->CTXT_XFRLENGTH, pCtxt->CTXT_DbufIndex);
		MSG("Flag %bx, Ctrl %bx, status %bx\n", pCtxt->CTXT_FLAG, pCtxt->CTXT_CONTROL, pCtxt->CTXT_Status);
	}

	dbuf_dbg(DBUF_SEG_U2BR); // dump Dbuf Seg0 
	dbuf_dbg(DBUF_SEG_DUMP_TXSEG1234);// dump Dbuf Seg 1, 2, 3, 4
	dbuf_dbg(DBUF_SEG_U2BW);
	dbuf_dbg(DBUF_SEG_DUMP_RXSEG1234);

#ifdef LA_DUMP
#ifdef REWIND_DUMP
	// dump information of rewind
	rewind_cnt = *usb_rewind_cnt;
	MSG("rewind_cnt %bx\n", rewind_cnt);
	for (int i = 0; i < (rewind_cnt * 16); i++)
	{
		MSG("%lx\t", *usb_rewind_port);
		if ((i & 0x01) == 0x01)
			MSG("\n");
	}
	MSG("\n");
	
	// dump rewinded datas
	if (CmdBlk(0) != 0x2A)
	{
		msc0_proto_retry_LA_ADR = *usb_MSC0_Proto_Retry_LA_ADR;
		MSG("proRtyLA_ADR %lx\n", msc0_proto_retry_LA_ADR);
		retry_cnt = (u16)((msc0_proto_retry_LA_ADR & LA_CNT) >> 16);
		*usb_MSC0_Proto_Retry_LA_ADR = (*usb_MSC0_Proto_Retry_LA_ADR & ~(LA_ADR|LA_ENABLE)) |((msc0_proto_retry_LA_ADR & LA_ADR) - 8);
		for (i = 0; i < (retry_cnt * 2); i++)
		{
			if ((i & 0x01) == 0x01)
			{
				dword1 = *usb_MSC0_Proto_Retry_LA;
				MSG("%lx\t", dword1);
				Delayus(50);
				MSG("%lx\n", dword0);
				Delayus(50);
			}
			else
			{
				dword0 = *usb_MSC0_Proto_Retry_LA;
			}
			if ((i & 0x3) == 0x3)
				Delayus(200);
		}
		MSG("\n");
	}
#endif	
	// dump the SM LA
	//dump_SM_LA();
	dump_U3_PM_LA();
#if 0	
	msc0_proto_retry_LA_ADR = *usb_Connection_LA_ADR;
	MSG("Connection_LA_ADR %lx\n", msc0_proto_retry_LA_ADR);
	retry_cnt = (u16)((msc0_proto_retry_LA_ADR & LA_CNT) >> 16);
	*usb_Connection_LA_ADR = (msc0_proto_retry_LA_ADR & ~(LA_ADR|LA_ENABLE)) |((msc0_proto_retry_LA_ADR & LA_ADR) - 8);
	for (i = 0; i < (retry_cnt * 2); i++)
	{
		if ((i & 0x01) == 0x01)
		{
			dword1 = *usb_Connection_LA_DATA;
			MSG("%lx\t", dword1);
			Delayus(50);
			MSG("%lx\n", dword0);
			Delayus(50);
		}
		else
		{
			dword0 = *usb_Connection_LA_DATA;
		}
		if ((i & 0x3) == 0x3)
			Delayus(200);
	}
	MSG("\n");
#endif
#endif
#if 0
	for (int i = 0; i < 3; i++)
	{
		dbuf_dbg(i);
	}

	for (int j = 0; j < 16; j++)
	{
		sata_ch_reg->sata_BlkCtrl |= CCMREAD;
		MSG("Dump CCM %bx:\n", j);
		MSG("ccm_prot %lx, xfer_len %lx\n", sata_ch_reg->sata_Ccm[j].ccm_prot, sata_ch_reg->sata_Ccm[j].ccm_xfercnt);
		for (int i = 0; i < 16; i++)
		{
			MSG("%bx\t", sata_ch_reg->sata_Ccm[j].ccm_fis[i]);
			if ((i & 0x07) == 0x07)
				MSG("\n");
		}
		MSG("\n");
		sata_ch_reg->sata_BlkCtrl &= ~CCMREAD;
	}
#endif
}
#endif
#ifdef DBG_FUNCTION
void dump_phy_regs(void)
{
	u8 temp8;
	MSG("U3 PMA:\n");
	for (u8 i = 0; i < 0x0E; i++)
	{
		temp8 = spi_phy_rd(PHY_SPI_U3PMA, i);
		MSG("%bx: %bx\n", i, temp8);
	}
	MSG("U3 PCS:\n");
	for (i = 0; i < 0x50; i++)
	{
		temp8 = spi_phy_rd(PHY_SPI_U3PCS, i);
		MSG("%bx: %bx\n", i, temp8);
	}
	MSG("SATA0:\n");
	for (i = 0; i < 0x20; i++)
	{
		temp8 = spi_phy_rd(PHY_SPI_SATA0, i);
		MSG("%bx: %bx\n", i, temp8);
	}
	MSG("SATA1:\n");
	for (i = 0; i < 0x20; i++)
	{
		temp8 = spi_phy_rd(PHY_SPI_SATA1, i);
		MSG("%bx: %bx\n", i, temp8);
	}
}
#endif
void switch_regs_setting(u8 reg_mode)
{
	u8 temp8;
	
	if (usb3_test_mode_enable == 0)
	{
		if (reg_mode & COMPLIANCE_TEST_MODE)
		{
			temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x03);
			temp8 = (temp8 & ~0x0F) | 0x09; // set SSC PPM to 4500ppm
			spi_phy_wr_retry(PHY_SPI_U3PMA, 0x03, temp8);
			spi_phy_wr_retry(PHY_SPI_U3PMA, 0x07, 0x80);

			spi_phy_wr_retry(PHY_SPI_U3PCS, 0x1C, 0x01); // disable HW SSC function
			spi_phy_wr_retry(PHY_SPI_U3PCS, 0x1C, 0x00); 
			start_HW_cal_SSC = 0;
			usb3_test_mode_enable = 1;
		}

		if (reg_mode & LOOPBACK_TEST_MODE)
		{
			temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x08); 
			temp8 = (temp8 & ~0xF0) | 0x90; // pcap is 0x10, RTUNE 0x01
			spi_phy_wr_retry(PHY_SPI_U3PMA, 0x08, temp8); 
			
			temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x09);	
			temp8 = (temp8 & ~0x3F) | 0x1D|0x80; // cdr_lp_gn[5:0], CDR second order filter tracking ranger up to 15K PPM enable: bit_7
			spi_phy_wr_retry(PHY_SPI_U3PMA, 0x09, temp8);

			temp8 = spi_phy_rd(PHY_SPI_U3PCS, 0x29); // set zcap value to 0x0 
			temp8 = (temp8 & ~0xF0) | 0x70;
			spi_phy_wr_retry(PHY_SPI_U3PCS, 0x29, temp8);			
			usb3_test_mode_enable = 1;
		}
	}
	else
	{
		if (reg_mode == NORMAL_MODE)
		{
			temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x03);
			temp8 &= ~0x0F; // set SSC PPM equal 0
			spi_phy_wr_retry(PHY_SPI_U3PMA, 0x03, temp8);
#ifndef LONG_CABLE
			temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x08); 
			#define PCAP_IN		0x40 // pma_8[7:6]: AFE equalization: low pole, less peaking
			temp8 = (temp8 & ~0xC0) |PCAP_IN;
			spi_phy_wr_retry(PHY_SPI_U3PMA, 0x08, temp8); 
			
			temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x09);	
			temp8 = (temp8 & ~0x3F) | 0x1E|0x80; // cdr_lp_gn[5:0], CDR second order filter tracking ranger up to 15K PPM enable: bit_7
			spi_phy_wr_retry(PHY_SPI_U3PMA, 0x09, temp8);

			temp8 = spi_phy_rd(PHY_SPI_U3PCS, 0x29); // set zcap value to 0x0 
			temp8 = (temp8 & ~0xF0) | 0x40;
			spi_phy_wr_retry(PHY_SPI_U3PCS, 0x29, temp8);
#else
			temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x08); 
			temp8 = (temp8 & ~0xF0) | 0x90; // pcap is 0x10, RTUNE 0x01
			spi_phy_wr_retry(PHY_SPI_U3PMA, 0x08, temp8); 
			
			temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x09);	
			temp8 = (temp8 & ~0x3F) | 0x1D|0x80; // cdr_lp_gn[5:0], CDR second order filter tracking ranger up to 15K PPM enable: bit_7
			spi_phy_wr_retry(PHY_SPI_U3PMA, 0x09, temp8);

			temp8 = spi_phy_rd(PHY_SPI_U3PCS, 0x29); // set zcap value to 0x0 
			temp8 = (temp8 & ~0xF0) | 0x70;
			spi_phy_wr_retry(PHY_SPI_U3PCS, 0x29, temp8);	
#endif
			
			start_HW_cal_SSC = 0;
			usb3_test_mode_enable = 0;
		}
	}
}

void usb_timeout_handle()
{
	if (rw_flag & RW_IN_PROCESSING)
	{
		rw_flag = (rw_flag & ~RW_IN_PROCESSING) | RW_WAIT_TMOUT_RESET_FLAG;
		_disable();
		// 3secs timer
		rw_time_out = 10; 
		_enable();
		check_usb_active_timer = 2;
		saved_usb_cnt = *usb_Msc0Residue;
		MSG("u hang\n");
	}
	else if (rw_flag & RW_WAIT_TMOUT_RESET_FLAG)
	{
		if ((saved_usb_cnt == *usb_Msc0Residue) && (check_usb_active_timer == 0))
		{
			rw_flag &= ~(RW_WAIT_TMOUT_RESET_FLAG|RW_IN_PROCESSING);
			MSG("tm out\n");
			
			PCDB_CTXT pCtxt;
			pCtxt = usb_ctxt.curCtxt;
			if (pCtxt)
			{
				DBG("HW error/tm out\n");
				MSG("cdb %bx\n", pCtxt->CTXT_CDB[0]);
			}

#ifdef DBG_FUNCTION					
			dump_all_regs(pCtxt);
#endif

		}
		else
		{
			// if the USB TX/RX is really in processing very slowly, F/W will count another 1 secs 
			// until the tx/rx is stoped or host resets bus
			if (check_usb_active_timer)
				check_usb_active_timer--;
			saved_usb_cnt = *usb_Msc0Residue;
			DBG("cnt tm %bx\n", check_usb_active_timer);
			_disable();
			rw_time_out = 10;
			_enable();
		}
	}	
}

void usb_rsp_unit_b_rdy(void)
{
	if (usb_ctxt.newCtxt != NULL)
	{
		PCDB_CTXT pCtxt = usb_ctxt.newCtxt;
		hdd_err_2_sense_data(pCtxt, ERROR_UA_BECOMING_READY);
		pCtxt->CTXT_usb_state = CTXT_USB_STATE_CTXT_SETUP_DONE; 
		dbg_next_cmd = 2;
		usb_device_no_data(pCtxt);
		usb_ctxt.newCtxt = pCtxt->CTXT_Next;	
		MSG("N\n");
		rsp_unit_not_ready = 0;
	}
}

// merge the main check in BOT.c & UAS.c to save code size
void usb_chk(void)
{	
#ifdef USB2_L1_PATCH
	if (usbMode == CONNECT_USB3)
#else
	if ((usbMode == CONNECT_USB2) ||(usbMode == CONNECT_USB3))
#endif
	{
		if (( usb_ctxt.curCtxt != NULL) || 
		((sata0Obj.sobj_State >= SATA_ACTIVE)
		|| (sata1Obj.sobj_State >= SATA_ACTIVE)))
		{
#ifdef USB2_L1_PATCH
			if (usbMode == CONNECT_USB2)
			{
				usb2_L1_reject_timer = 8;	
			}
			else
#endif
			{
				U1U2_ctrl_timer = 3;
			}
		}
	}
	
	if (arrayMode != JBOD)
	{
		if (raid_xfer_status & RAID_XFER_IN_RPOGRESS)
		{
			// check the RAID_RD_DONE & RAID_WR_DONE
			if (raid_xfer_status & RAID_XFER_DONE)
			{
				raid_done_isr();
			}
		}	
		
		if (raid_mirror_status) // when the drives requires rebuilding or verifying
		{
			raid_rebuild();
		}
	}

#ifndef USING_HW_TIMEOUT	
	if (rw_time_out == 0)
		usb_timeout_handle();
#endif

#ifdef LINK_RECOVERY_MONITOR		
	if (rw_flag & (READ_FLAG | WRITE_FLAG))
	{
		// 
		if (*usb_USB3LTSSM_shadow & LTSSM_RECOVERY)
		{
			rw_flag |= LINK_RETRY_FLAG;
		}
	}
#endif

	if (rsp_unit_not_ready)
	{
		usb_rsp_unit_b_rdy();
	}
	
	if (usb_ctxt.post_dout)
	{
		PCDB_CTXT pCtxt = usb_ctxt.curCtxt;
		if (pCtxt)
		{
			 scsi_post_data_out(pCtxt);
		}
	}
	if (--timer_count == 0)
	{
		timer_count = 20; // slow down the polling frequency for better performance
		if ((sata0Obj.sobj_State >= SATA_ACTIVE) // when SATA is busy, led blinks
		|| (sata1Obj.sobj_State >= SATA_ACTIVE))
		{
			// Show the disk activity and restart the standby timer.
			tickle_activity();
			reset_standby_timer();
			if ((raid_rebuild_verify_status & RRVS_IN_PROCESS) == 0)
			{
				raid1_active_timer = RAID_REBUILD_VERIFY_ONLINE_ACTIVE_TIMER;
			}
		}
	}
			
#if (PWR_SAVING)	
#if WDC_HDD_TEMPERATURE_POLL_TICKS		//check temperature
	if (check_temp_flag)
	{
		if ( !(disable_upkeep & (UPKEEP_NO_TEMP_POLL_MASK_S0 | UPKEEP_NO_TEMP_POLL_MASK_S1)) &&
			 ((sata0Obj.sobj_State == SATA_READY) && (sata1Obj.sobj_State == SATA_READY)))
		{
			// if there's pending UAS SATA command and USB command, finished it firstly, then get the temperature
			if ((((sata0Obj.cFree == sata0Obj.default_cFree) && (sata1Obj.cFree == sata1Obj.default_cFree)) || (curMscMode == MODE_BOT))
				&& (usb_ctxt.curCtxt == NULL))
			{
			#ifndef LA_DUMP	
				MSG("chk temp\n");
			#endif
				ignore_tickles = 1;
				diag_check_hdd_temperature();

				ignore_tickles = 0;
				_disable(); 
				expect_temperatureTimer32 = WDC_HDD_TEMPERATURE_POLL_TICKS;
				check_temp_flag = 0;
				_enable();
				uas_ci_paused = 0;
			}
			else
			{
				if (curMscMode == MODE_UAS)
				{
					if (uas_ci_paused == 0)
					{
						uas_ci_paused |= (UAS_PAUSE_SATA0_NOT_READY |UAS_PAUSE_SATA1_NOT_READY);
					}
				}
			}
		}
	}
#endif	// WDC_HDD_TEMPERATURE_POLL_TICKS

#if FAN_CTRL_EXEC_TICKS			
	if (fan_ctrl_exec_flag)
	{
		fan_ctrl_exec_flag = 0;
		fan_ctrl_exec_ticker = FAN_CTRL_EXEC_TICKS;
		fan_ctrl_exec();
	}
#endif
	// Spin down the HDD if the inactivity (standby) timer expired
	// and background activities would not be disrupted.
	ata_hdd_standby();
#endif

#ifdef SUPPORT_HR_HOT_PLUG
	sata_status_chk_on_HR();
#endif

}

void init_usb_registers(u8 mscMode)
{
	raid_xfer_status = 0;
	raid1_active_timer = RAID_REBUILD_VERIFY_ONLINE_ACTIVE_TIMER;
	fetch_Ctxt_mem = 0;
	_disable(); 
#if WDC_SHORT_HDD_TEMPERATURE_POLL_TICKS 	
	expect_temperatureTimer32 = WDC_SHORT_HDD_TEMPERATURE_POLL_TICKS;
	check_temp_flag = 0;
#endif	
#if FAN_CTRL_EXEC_TICKS
	fan_ctrl_exec_ticker = FAN_CTRL_EXEC_TICKS;
	fan_ctrl_exec_flag = 0;
#endif	
	_enable();

	if (DISABLE_U1U2() == 0)
	{
		*usb_DevCtrlClr = (USB3_U1_REJECT | USB3_U2_REJECT);	
		u1U2_reject_state = U1U2_ACCEPT;
	}
	else
	{
		*usb_DevCtrl = (USB3_U1_REJECT | USB3_U2_REJECT);	
		u1U2_reject_state = U1U2_REJECT;
	}

	u8 temp = 0; // 512B/s
	if (vital_data[HDD0_LUN].user_bs_log2m9 <4) //1024B/s, 2048B/s, 4096B/s
		temp = vital_data[HDD0_LUN].user_bs_log2m9;
	
	temp = temp << 5;
	if (mscMode == MODE_BOT)
		temp |= 0x01;
	else
		temp |= 0x04;	
	
	*usb_Msc0LunCtrl = (*usb_Msc0LunCtrl & 0x0F) | 0x00; // index to LUN 0
	*usb_Msc0LunSATCtrl = (*usb_Msc0LunSATCtrl & ~(SAT_CMD | HOST_BLK_SIZE)) | temp;

	DBG("%x\n", *usb_Msc0LunSATCtrl);
	if (HDD1_LUN != UNSUPPORTED_LUN)
	{
		// set the SAT transfer for HDD1 lun
		temp = 0; // 512B/s
		if (vital_data[HDD1_LUN].user_bs_log2m9 <4) //1024B/s, 2048B/s, 4096B/s
			temp = vital_data[HDD1_LUN].user_bs_log2m9;
		
		temp = temp << 5;
		if (mscMode == MODE_BOT)
			temp |= 0x01;
		else
			temp |= 0x04;
		
		*usb_Msc0LunCtrl = (*usb_Msc0LunCtrl & 0x0F) | 0x10; // set the LUN ctrl index to 1
		*usb_Msc0LunSATCtrl = (*usb_Msc0LunSATCtrl & ~(SAT_CMD | HOST_BLK_SIZE)) | temp;
	}
	
	*usb_DeviceRequestEn |= USB_GET_DESCR_STD_UTILITY;		
	*usb_U3IdleTimeout = 0xFA;
}
#if (PWR_SAVING)
// power off USB2 in USB3 mode, power off USB3 in USB2 mode to save power
// when USB 2 bus reset or USB3 warm reset will wake the power-off logical (USB2/USB3)
void save_usb_power(void)
{
	if(usbMode == CONNECT_USB3)
	{
		*usb_DevCtrl = USB2_FORCE_SUSP;	// turn off USB2 power
		MSG("turn off U2 PLL\n");
	}
	else
    	{
	        //turn off usb3 phy
	 	spi_phy_wr_retry(PHY_SPI_U3PCS, 0x13, 0xF0|PD_USB3_TX|PD_USB3_RX|PD_USB3_PLL);
		Delayus(2);
		spi_phy_wr_retry(PHY_SPI_U3PCS, 0x13, 0xF0|PD_USB3_TX|PD_USB3_RX|PD_USB3_VREG|PD_USB3_PLL);
		MSG("turn off U3 PLL\n");
      	}
}

void turn_on_USB23_pwr(void)
{
	//turn on USB 2.0 PLL
	*usb_DevCtrl = USB2_PLL_FREERUN;	// enable U2 PLL Freerun
	*usb_DevCtrlClr = USB2_FORCE_SUSP;
       //turn on usb3 PLL
       _disable();
       u8 temp8 = spi_phy_rd(PHY_SPI_U3PCS, 0x13);	
	temp8 &= ~0x0F;
	spi_phy_wr_retry(PHY_SPI_U3PCS, 0x13, temp8);	
	_enable();
}
#endif

void check_usb_mode(void)
{
	if (usbMode == CONNECT_UNKNOWN)
	{
		u16 devstatus = *usb_DevStatus_shadow;
		
		MSG("USB");
		if (devstatus &  USB3_MODE )
		{
			MSG("3\n");
			usbMode = CONNECT_USB3;
		}
		else if (devstatus &  USB2_MODE )
		{
			if (*usb_DevStatus & USB2_HS_MODE)
			{
				MSG("2\n");
				usbMode = CONNECT_USB2;
			}
			else
			{
				MSG("1\n");
				usbMode = CONNECT_USB1;
			}
		}
	}
}

void usb_pwr_func(void)
{
	if (usbMode == CONNECT_USB3)
	{
		if (u1U2_reject_state == U1U2_ACCEPT)
		{
			*usb_DevCtrl = (USB3_U1_REJECT | USB3_U2_REJECT);
			if ((*usb_USB3LTSSM == LTSSM_U1) || (*usb_USB3LTSSM == LTSSM_U2))
				*usb_DevCtrl = USB3_U1U2_EXIT;
			u1U2_reject_state = U1U2_REJECT;	
			U1U2_ctrl_timer = 3;
		}
	}

#if (PWR_SAVING)
	if (turn_on_pwr)
	{
		if ((usb_active) && (usb_reenum_flag == 0) && 
		((sata0Obj.sobj_init == 1) && (sata1Obj.sobj_init == 1)))
		{
			save_usb_power();
			turn_on_pwr = 0;
		}
	}
#endif
}

void chk_power_Off(void)
{
	if (pwr_off_enable == 1)
	{
		power_off_commanded = 1; // use this command to return to main function
		if (pwr_off_enable)
		{
			Delay(250);
			pwr_off_enable = 0;
		}
	}
}

#ifdef USB2_L1_PATCH
void reject_USB2_L1_IN_IO_transfer(void)
{
	// because hardware does not support the L1 resume automatically, firmware does the patch to reject L1 in IO transfer
	if (usbMode == CONNECT_USB2)
	{
		_disable();
		*usb_DevCtrl = USB2_L1_DISABLE;
		*cpu_wake_up = (*cpu_wake_up & 0x00FFFF00) | USB2_WAKEUP_REQ;
		usb2_L1_reject_timer = 8;
		_enable();
	}
}
#endif

void translate_rw_cmd_to_rw16(PCDB_CTXT pCtxt)
{
	switch(CmdBlk(0))
	{
	case SCSI_READ6:
	case SCSI_WRITE6:
		// control bit
		CmdBlk(15) = CmdBlk(5);
		
		//Transfer Length
		CmdBlk(13) = CmdBlk(4);

		// LBA
		CmdBlk(7) = CmdBlk(1) & 0x1F;
		CmdBlk(8) = CmdBlk(2);
		CmdBlk(9) = CmdBlk(3);
		
		CmdBlk(2) = 0;
		CmdBlk(3) = 0;
		CmdBlk(4) = 0;
		CmdBlk(5) = 0;
		CmdBlk(6) = 0;
		break;

	case SCSI_READ10:
	case SCSI_WRITE10:
		// control bit
		CmdBlk(15) = CmdBlk(9);
		
		//Transfer Length
		CmdBlk(10) = 0;
		CmdBlk(11) = 0;
		CmdBlk(12) = CmdBlk(7);
		CmdBlk(13) = CmdBlk(8);	
		
		// LBA
		CmdBlk(6) = CmdBlk(2);
		CmdBlk(7) = CmdBlk(3);
		CmdBlk(8) = CmdBlk(4);
		CmdBlk(9) = CmdBlk(5);
		CmdBlk(2) = 0;
		CmdBlk(3) = 0;
		CmdBlk(4) = 0;
		CmdBlk(5) = 0;
		break;
		
	case SCSI_READ12:	
	case SCSI_WRITE12:	
		// control bit
		CmdBlk(15) = CmdBlk(11);
		
		//Transfer Length
		CmdBlk(10) = CmdBlk(6);
		CmdBlk(11) = CmdBlk(7);
		CmdBlk(12) = CmdBlk(8);
		CmdBlk(13) = CmdBlk(9);
		
		// LBA
		CmdBlk(6) = CmdBlk(2);
		CmdBlk(7) = CmdBlk(3);
		CmdBlk(8) = CmdBlk(4);
		CmdBlk(9) = CmdBlk(5);
		CmdBlk(2) = 0;
		CmdBlk(3) = 0;
		CmdBlk(4) = 0;
		CmdBlk(5) = 0;
		break;
	}
}

void usb_hdd()
{
	rdCapCounter = 0;
#ifdef RX_DETECTION
	u32	rx_detection_time = 0;
#endif
	dbg_next_cmd = 0;
	pwr_off_enable = 0;
	r_recover_counter = 0;
	w_recover_counter = 0;
	
	power_on_flag = 1;
	usb_reenum_flag = 0;
	usb3_ts1_timer = 90;

	usb3_test_mode_enable = 0;
	start_HW_cal_SSC = 0;
	u8 start_chk_host_mode = 1;
	u8 host_mode = HOST_MODE_USB3;

	usb_inactive_count = 0;
#ifdef DEBUG_USB3_ERR_CNT	
//	linkErrCnt = *usb_LinkErrorCount;
	decErrCnt = *usb_DecodeErrorCount;
//	disparityErrCnt = *usb_DisparityErrorCount;
	MSG("PWR on U ErrCnt %lx\n", decErrCnt);
#endif
	usb_active = 1;
	UsbExec_Init();

	while(1)
	{
begin:
		if (*usb_DevState_shadow == 0x80)
		{
			*usb_DevCtrlClr = USB_ENUM;
			*usb_DevCtrl = (USB_FW_RDY | USB_ENUM);
			MSG("toggle Enum\n");
		}

		if (*usb_USB3LTSSM_shadow == LTSSM_POLLING)
		{
			if (start_HW_cal_SSC)
			{
				_disable();
				if (spi_phy_rd(PHY_SPI_U3PCS, 0x43) & BIT_3) // USB RX ready
				{
					spi_phy_wr_retry(PHY_SPI_U3PCS, 0x1C, 0x0B); // enable HW calculate SSC
					MSG("EN Adap\n");
#ifdef DEBUG_USB3_ERR_CNT
					fout_value = (u8)((*usb3_PHY_control & USB3_PHY_FOUT) >> 16);
					MSG("Fout %bx\n", fout_value);
#endif
					start_HW_cal_SSC = 0;
				}
				_enable();
			}
		}

		if (*cpu_wake_up & CPU_USB_SUSPENDED)
		{
			if ( usb_suspend() )
				return;
			continue;
		}	

		if (USB_VBUS_OFF() || power_off_commanded)
		{
			usb_active = 0;
			return;
		}

		u32 usb_int_Status = *usb_IntStatus_shadow;

		if (usb_int_Status & (HOT_RESET|WARM_RESET|USB_BUS_RST))
		{
			if (usb_int_Status & USB_BUS_RST)
			{
				if (rx_detect_count == RX_DETECT_COUNT8)
				{
					rx_detect_count = RX_DETECT_COUNT1;
					*usb2_Timing_Control0 = (*usb2_Timing_Control0 & ~USB2_PWR_ATTACH_MIN) | USB2_PWR_ATTACH_MIN_8MS;
					*usb_link_debug_param = (*usb_link_debug_param & ~USB_RX_DETECT_CNT) | USB_RX_DETECT_CNT_EQ_1;
					if (*usb_USB3LTSSM_shadow == LTSSM_RXDETECT) // patch the issue that the RX-detection CNT can't be changed in the state of RX-detect
						*usb_DevCtrl = EXIT_SS_DISABLE;
				}
			}
			else
			{
				if ((rx_detect_count == RX_DETECT_COUNT1) && (host_mode != HOST_MODE_USB2))
				{
					rx_detect_count = RX_DETECT_COUNT8;
					*usb2_Timing_Control0 = (*usb2_Timing_Control0 & ~USB2_PWR_ATTACH_MIN) | USB2_PWR_ATTACH_MIN_80MS;
					*usb_link_debug_param = (*usb_link_debug_param & ~USB_RX_DETECT_CNT) | USB_RX_DETECT_CNT_EQ_8;
				}
			}
			MSG("U Rst %bx\n", (u8)usb_int_Status);
			DBG("Rx det cnt %lx\n", *usb_link_debug_param);

			usb_host_rst();
			_disable();			
			if (((sata0Obj.sobj_State == SATA_RESETING) || (sata0Obj.sobj_State == SATA_PHY_RDY))
			|| ((sata1Obj.sobj_State == SATA_RESETING) || (sata1Obj.sobj_State == SATA_PHY_RDY)))
			{
				chk_fis34_timer = 30;
				spinup_timer = 3;
			}
			_enable();
			continue;	// to check the RST & SUSP again			
		}
#ifdef DEBUG_USB3_ERR_CNT		
		if (decErrCnt != *usb_DecodeErrorCount)
		{
//			linkErrCnt = *usb_LinkErrorCount;
			decErrCnt = *usb_DecodeErrorCount;
//			disparityErrCnt = *usb_DisparityErrorCount;
			MSG("U ErrCnt %lx\n", decErrCnt);
		}
		if (start_HW_cal_SSC == 0)
		{
			tmp_fout_value = (u8)((*usb3_PHY_control & USB3_PHY_FOUT) >> 16);
			if (fout_value != tmp_fout_value)
			{
				fout_value = tmp_fout_value;
				MSG("fout %bx\n", fout_value);
			}
		}
#endif
		
		if ((*usb_Msc0IntStatus_shadow & BOT_RST_INT))
		{

			*usb_Ep0CtrlClr = EP0_HALT;
			*usb_RecoveryError = 0x00;                       //for usb hot rest
			*usb_Msc0DICtrlClr = MSC_DIN_DONE | MSC_CSW_RUN | MSC_DI_HALT ;
			*usb_Msc0DOutCtrlClr = MSC_DOUT_RESET | MSC_DOUT_HALT ;
			*usb_Msc0IntStatus = BOT_RST_INT;	

			MSG("bRst\n");

			continue;	// to check the RST & SUSP again			
		}

		if (1)//(*usb_IntStatus & CTRL_INT)
		{	
			// EP0_SETUP----------------
			if (*usb_Ep0Ctrl & EP0_SETUP)
			{	
				check_usb_mode();
				_disable();
				switch_regs_setting(NORMAL_MODE);
				_enable();
				
#ifdef SLOW_INSERTION
				if ((power_on_flag) && (usbMode != CONNECT_USB3))
				{
					DBG("enum ag\n");
					*usb_Ep0CtrlClr = EP0_SETUP;	
					*usb_DevCtrlClr = USB_ENUM;		//disable USB enumeration
					Delay(200);
					*usb_DevCtrl = USB_ENUM;		//do USB enumeration
					usbMode = CONNECT_UNKNOWN;
					power_on_flag = 0;
					continue;
				}
#endif
				power_on_flag = 0;
				if (start_chk_host_mode)
				{
					start_chk_host_mode = 0;
					if (usbMode == CONNECT_USB3)
					{
						host_mode = HOST_MODE_USB3;
					}
					else
					{
						host_mode = HOST_MODE_USB2;
						if (rx_detect_count == RX_DETECT_COUNT8)
						{
							rx_detect_count = RX_DETECT_COUNT1;
							*usb2_Timing_Control0 = (*usb2_Timing_Control0 & ~USB2_PWR_ATTACH_MIN) | USB2_PWR_ATTACH_MIN_8MS;
							*usb_link_debug_param = (*usb_link_debug_param & ~USB_RX_DETECT_CNT) | USB_RX_DETECT_CNT_EQ_1;
						}
						*usb_usb2_FS_connection_timer = 0x000661a7;
						*usb_USB2_connect_timer = 0x00061a7;
					}
				}				
				usb_control();
				// in NEC host, it will not issue the bus reset, then change the protocol
//				curMscMode = check_usbProt_mode();
				continue;
			}
		}
		
		//Delay(1);
#if 0//ndef FPGA
		if (*usb_DevState_shadow & USB_CONFIGURED)
#endif
		{
			if (*chip_IntStaus & SATA0_INT)
			{
				sata_isr(&sata0Obj);
			}
			if (*chip_IntStaus & SATA1_INT)
			{
				sata_isr(&sata1Obj);
			}
		}
		
		if ((usb_int_Status & CDB_AVAIL_INT) || (*usb_Msc0IntStatus_shadow & MSC_TASK_AVAIL_INT))
		{
			saved_usb_cnt = 0;
			check_usb_active_timer = 0;
			mscInterfaceChange = 0;
			timer_count = 20;
			check_usb_mode(); // for the case that the renesis UAS driver switch to UAS mode w/o reset
#ifdef USB2_L1_PATCH
			reject_USB2_L1_IN_IO_transfer();
#endif
			if (usbMode != CONNECT_USB2)
			{
				auxreg_w(REG_CONTROL1, 0x0); // disable timer 1
			}
#ifdef UAS
			if(*usb_Msc0Ctrl & MSC_UAS_MODE)
			{
				//enable dead timer 1 sec. (x10000)
				*usb_DeadTimeout = 200;// 10ms
#ifdef WIN8_UAS_PATCH
				intel_host_flag = 0;
				intel_SeqNum_Monitor_Count = 3;
				intel_SeqNum_monitor = INTEL_SEQNUM_START_MONITOR;
#endif
				usb_uas();
			}
			else
#endif
			{
				//disable dead timer
				*usb_DeadTimeout = 0;
				reset_clock(CPU_CLOCK_BOT);
				*cpu_Clock_restore = (*cpu_Clock_restore & ~CPU_CLK_DIV_RESTORE) | CPU_CLOCK_BOT;
				usb_bot();
			}
#if (PWR_SAVING)
			turn_on_USB23_pwr();
#endif
#ifdef WIN8_UAS_PATCH
			intel_SeqNum_monitor = 0;
			intel_host_flag = 0;
#endif
			if ((usb_ctxt.curCtxt != NULL) || (sata0Obj.sobj_State > SATA_READY) || (sata1Obj.sobj_State > SATA_READY))
			{
				rst_all_seg();
			}
			
			if ((sata0Obj.sobj_State == SATA_PWR_DWN) || (sata1Obj.sobj_State == SATA_PWR_DWN))
			{
				sata_phy_pwr_ctrl(SATA_PHY_PWR_UP, SATA_CH0);
				sata_phy_pwr_ctrl(SATA_PHY_PWR_UP, SATA_CH1);
			}
			
			if (*chip_IntStaus & USB2_PLL_RDY)
			{
				init_timer1_interrupt(100, BEST_CLOCK);
			}
			
			_disable();
			*usb_DevCtrlClr = USB2_L1_DISABLE;
			_enable();
			
			reset_clock(CPU_CLOCK_UAS);
			*cpu_Clock_restore = (*cpu_Clock_restore & ~CPU_CLK_DIV_RESTORE) | CPU_CLOCK_UAS;
			// check VBus
			//if (*usb_IntStatus & VBUS_OFF)
			if (USB_VBUS_OFF() || power_off_commanded || (re_enum_flag == FORCE_OUT_RE_ENUM)
#ifdef HARDWARE_RAID
				|| (raid_config_button_pressed)
#endif			
#ifdef SUPPORT_HR_HOT_PLUG
				|| (hot_plug_enum)
#endif
				)
			{
				return;
			}
			if (re_enum_flag == RECHECK_USB)
				re_enum_flag = 0;
			usb_reenum_flag = 0;
			usbMode = CONNECT_UNKNOWN;
			curMscMode = MODE_UNKNOWN;
#ifdef RX_DETECTION
			rx_detection_time = 0;
#endif
			*usb_CtxtSize = 0xC000 | MAX_UAS_CTXT_SITE;
			goto begin;
		}	// 		if (*usb_IntStatus & CDB_AVAIL_INT)
	} // End of while loop
}
