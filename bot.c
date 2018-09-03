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
 * bot.c		2010/08/15	Winston 	Initial version
 *				2010/10/06	Ted			VBUS OFF detect modify
 *
 *****************************************************************************/

#define BOT_C

#include "general.h"


/****************************************\
   Dn
\****************************************/
void bot_init()
{
	curMscMode = MODE_BOT;
	usb_ctxt.curCtxt = NULL;
	usb_ctxt.ctxt_que = NULL;
	usb_ctxt.ctxt_que_tail = NULL;
	usb_ctxt.post_dout = 0;

	bot_active = 0;

}


/****************************************\
   Dn
\****************************************/
void bot_device_no_data(PCDB_CTXT pCtxt)
{
//	pCtxt->CTXT_FLAG = CTXT_FLAG_U2B;

	// Is Hn ? 
	if (pCtxt->CTXT_XFRLENGTH == 0)
	{	// Set to USB Data In
		pCtxt->CTXT_CONTROL |= CTXT_CTRL_DIR;
	}

	if (pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR)
	{	// Hi > Dn(case 4) or Hn = Dn(Case 1)


		if (pCtxt->CTXT_XFRLENGTH == 0)
		{	// case 1
			// set to case 1 (Hn == Dn)			????? case bit should bit symbolic constant
			pCtxt->CTXT_PHASE_CASE = 0x0002;
		}
		else
		{	// case 4 (Hi > Dn)
			pCtxt->CTXT_PHASE_CASE = 0x0010;
		}
		//send no data
		

		
//		*usb_Msc0TxCtxt = MSC_TX_CTXT_VALID|MSC_TX_CTXT_NODATA|site;
#ifdef FAST_CONTEXT
		pCtxt->CTXT_No_Data = MSC_TX_CTXT_NODATA;
#else
		pCtxt->CTXT_No_Data = (MSC_TX_CTXT_VALID|MSC_TX_CTXT_NODATA) >> 8;
#endif
	}
	else
	{	// Ho > Dn (CASE 9)
		if (pCtxt->CTXT_PHASE_CASE == 0)
			pCtxt->CTXT_PHASE_CASE = 0x0200;

		if (pCtxt->CTXT_XFRLENGTH) 
		{
			pCtxt->CTXT_PHASE_CASE = 0x0200;
		}

//		*usb_Msc0RxCtxt = MSC_RX_CTXT_VALID|MSC_RX_CTXT_NODATA|site;
#ifdef FAST_CONTEXT
		pCtxt->CTXT_No_Data = MSC_RX_CTXT_NODATA;
#else
		pCtxt->CTXT_No_Data = (MSC_RX_CTXT_VALID|MSC_RX_CTXT_NODATA) >> 8;
#endif
	}
	usb_exec_que_ctxt();
	return;
}

/****************************************\
   Di or Dn
\****************************************/
void bot_device_data_in(PCDB_CTXT pCtxt, u32 byteCnt)
{
//	u32 i, tmp32, *p32;

	DBG("bot d_in\n");

	pCtxt->CTXT_FLAG = CTXT_FLAG_U2B;
	pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
	pCtxt->CTXT_LunStatus = LUN_STATUS_GOOD;

	// input port of TX buffer Seg 3  is CPU Write
	// Output port of TX buffer Seg 3  is USB Read

	//DBG("PCtxt: %lx, len: %lx", pCtxt, pCtxt->CTXT_XFRLENGTH);
	//DBG("\txfrLen: %lx", pCtxt->CTXT_XFRLENGTH);
	if (byteCnt==0)
	{	// Dn
		usb_device_no_data(pCtxt);
		return;
	}

	// Is Hn ? 
	if (pCtxt->CTXT_XFRLENGTH == 0)
	{	// Set to USB Data In / 3610 TX Dbuff
		pCtxt->CTXT_CONTROL |= CTXT_CTRL_DIR;
	}
	if (pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR)
	{	// Hi or Hn
		if (pCtxt->CTXT_XFRLENGTH == 0)
		{	// case 2 (Hn < Di)
			DBG("Hn\n");
			pCtxt->CTXT_PHASE_CASE = 0x0004;	

//				*usb_Msc0TxCtxt = MSC_TX_CTXT_VALID|MSC_TX_CTXT_NODATA|site;
#ifdef FAST_CONTEXT
			pCtxt->CTXT_No_Data = MSC_TX_CTXT_NODATA;
#else
			pCtxt->CTXT_No_Data = (MSC_TX_CTXT_VALID|MSC_TX_CTXT_NODATA) >> 8;
#endif
			pCtxt->CTXT_Status = CTXT_STATUS_PHASE;	// phase Error
			usb_exec_que_ctxt();
			return;
		}
		// input port of TX buffer Seg 3  is CPU Write
		// Output port of TX buffer Seg 3  is USB Read	
		pCtxt->CTXT_DbufIndex = DBUF_SEG_U2BR;
//			pCtxt->CTXT_DBUF_IN_OUT = (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_CPU_W_PORT;
		set_dbufSource(pCtxt->CTXT_DbufIndex);
		write_data_by_cpu_port(byteCnt, DIR_TX);
						
		{	// Hi
			if (pCtxt->CTXT_XFRLENGTH == byteCnt)
			{	// case 6 (Hi = Di)
				pCtxt->CTXT_PHASE_CASE = 0x0040;
			}
			else if (pCtxt->CTXT_XFRLENGTH < byteCnt)
			{	// case 7 (Hi < Di)
				// overrun case is generated by F/W, skip this case
				// send the data following usb Hi
				//pCtxt->CTXT_PHASE_CASE = 0x0080;
				//pCtxt->CTXT_Status = 2;	// phase Error
				pCtxt->CTXT_PHASE_CASE = 0x0040;
			}
			else //if (pCtxt->CTXT_XFRLENGTH < byteCnt)
			{	// case 5 (Hi > Di)
				pCtxt->CTXT_PHASE_CASE = 0x0020;
			}
		}

		//DBG("Segment DONE!\t");
//			Rx_Dbuf->dbuf_Seg[pCtxt->CTXT_DbufIndex].dbuf_Seg_Control = SEG_DONE;
		set_dbufDone(pCtxt->CTXT_DbufIndex);
		if (dbg_next_cmd)
			MSG("Seg Dn\n");
//			*usb_Msc0TxCtxt = MSC_TX_CTXT_VALID|site;
#ifdef FAST_CONTEXT
		pCtxt->CTXT_No_Data = 0;
#else
		pCtxt->CTXT_No_Data = MSC_TX_CTXT_VALID >> 8;
#endif
	}
	else
	{	// case 10 (Ho <> Di)
		pCtxt->CTXT_PHASE_CASE = 0x0400;

		// phase Error
		pCtxt->CTXT_Status = CTXT_STATUS_PHASE;

//			*usb_Msc0RxCtxt = MSC_RX_CTXT_VALID|MSC_RX_CTXT_NODATA|site;
#ifdef FAST_CONTEXT
		pCtxt->CTXT_No_Data = MSC_TX_CTXT_NODATA;
#else
		pCtxt->CTXT_No_Data = (MSC_TX_CTXT_VALID|MSC_TX_CTXT_NODATA) >> 8;
#endif
	}
	usb_exec_que_ctxt();
	return;
}


/****************************************\
   Do or Dn
\****************************************/
void bot_device_data_out(PCDB_CTXT pCtxt, u32 byteCnt)
{
	pCtxt->CTXT_FLAG = CTXT_FLAG_U2B;
	pCtxt->CTXT_Status = CTXT_STATUS_PENDING;
	pCtxt->CTXT_LunStatus = LUN_STATUS_GOOD;
//	pCtxt->CTXT_CONTROL = 0;

	if (!byteCnt)
	{
		pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
		usb_device_no_data(pCtxt);
		return;
	}

	if (pCtxt->CTXT_XFRLENGTH == 0)
	{	// Set to USB Data In / 3610 TX Dbuff
		pCtxt->CTXT_CONTROL |= CTXT_CTRL_DIR;
	}

	if (pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR)
	{	// Hi or Hn
		
		if (pCtxt->CTXT_XFRLENGTH == 0)
		{	// case 3 (Hn < Do)
			pCtxt->CTXT_PHASE_CASE = 0x0008;
			pCtxt->CTXT_Status = CTXT_STATUS_PHASE;	// phase Error
		}
		else
		{	// case 8 (Hi <> D0)
			pCtxt->CTXT_PHASE_CASE = 0x0100;
			pCtxt->CTXT_Status = CTXT_STATUS_PHASE;	// phase Error
		}
		pCtxt->CTXT_DbufIndex = SEG_NULL;

//			*usb_Msc0TxCtxt = MSC_TX_CTXT_VALID|MSC_TX_CTXT_NODATA|site;
#ifdef FAST_CONTEXT
		pCtxt->CTXT_No_Data = MSC_TX_CTXT_NODATA;
#else
		pCtxt->CTXT_No_Data = (MSC_TX_CTXT_VALID|MSC_TX_CTXT_NODATA) >> 8;
#endif
	}
	else
	{	// Ho
		// USB Data Out

		// input port of RX buffer Seg 4  is CPU Read
		// Output port of RX buffer Seg 4  is USB Write
		pCtxt->CTXT_DbufIndex = DBUF_SEG_U2BW;

		if (pCtxt->CTXT_XFRLENGTH == byteCnt)
		{	// case 12 (Ho = Do)
			pCtxt->CTXT_PHASE_CASE = 0x1000;
		}
		else if (pCtxt->CTXT_XFRLENGTH < byteCnt)
		{	// case 13 (Ho < Do)
			pCtxt->CTXT_PHASE_CASE = 0x2000;
			pCtxt->CTXT_Status = CTXT_STATUS_PHASE;	// phase Error
			usb_device_no_data(pCtxt);
			return;
		}
		else //if (pCtxt->CTXT_XFRLENGTH > byteCnt)
		{	// case 11 (Ho > Do)
//				pCtxt->CTXT_PHASE_CASE = 0x0800;
			// FW get the bytecnt from cdb allocation length or param len, the updated smartware has resolved the issue that the 
			// the alloc or param len doesn't match the ctxt_xfer_length
			// there's a case that the reset dek tool has the issue also, i think it should be fixed in later wd's reset-dek tool.
			pCtxt->CTXT_PHASE_CASE = 0x1000; 
		}
		
//			*usb_Msc0RxCtxt = MSC_RX_CTXT_VALID|site;
#ifdef FAST_CONTEXT
		pCtxt->CTXT_No_Data = 0;
#else
		pCtxt->CTXT_No_Data = (MSC_RX_CTXT_VALID >> 8);
#endif
	}
	usb_exec_que_ctxt();
	return;
}


/////////////////////////////////////////////////////////////////////////////////////

void usb_bot()
{
	u8 site = 0;
	init_usb_registers(MODE_BOT);
	MSG("*BOT*\n");
	DBG("u core %lx\n", *usb_CoreCtrl);

	sata_ch_reg = sata_Ch1Reg;
	sata_ch_reg->sata_FrameIntEn &= ~D2HFISIEN;
	sata_ch_reg = sata_Ch0Reg;
	sata_ch_reg->sata_FrameIntEn &= ~D2HFISIEN;

	bot_init();

	*usb_Msc0IntEn = (MSC_CSW_DONE_INT|MSC_TX_DONE_INT|MSC_RX_DONE_INT|MSC_DIN_HALT_INT|MSC_DOUT_HALT_INT);

#if (PWR_SAVING)
	turn_on_pwr = 1;
#endif

	while(1)
	{

begin:

		if (*cpu_wake_up & CPU_USB_SUSPENDED)
		{
			if ( usb_suspend())
				return;
			continue;
		}

		// check USB VBUS
		//if (usb_int_Status & VBUS_OFF)
		if (USB_VBUS_OFF())
		{
			ERR("vbus off\n");
//			if (hdd_power)
//				usb_host_rst();
			return;
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
		
		if ((*chip_IntStaus & USB_INT) ||(usb_ctxt.newCtxt != NULL))
		{
			u32 usb_int_Status = *usb_IntStatus_shadow;

			// USB 2.0 BUS_RST--------------
			// USB 3.0 Hot or Warm Reset--------------
			if (usb_int_Status & (HOT_RESET|WARM_RESET|USB_BUS_RST))
			{
				MSG("RST 1\n");
				return;
			}
			
			if (*usb_Msc0IntStatus_shadow & BOT_RST_INT)
			{
bot_bulk_reset:	
				MSG("Bk RST\n");
				if (bot_active)
				{	// load the context site number of Bad CDB context into the Context Free FIFO
					*usb_CtxtFreeFIFO = site;
					*usb_Msc0CtxtUsed = 0;
				}

				*usb_Msc0IntStatus = BOT_RST_INT|MSC_DIN_HALT_INT|MSC_DOUT_HALT_INT;

				*usb_Ep0CtrlClr = EP0_HALT;
				*usb_RecoveryError = 0x00;                       //for usb hot rest
				*usb_Msc0DICtrlClr = MSC_DIN_DONE | MSC_CSW_RUN | MSC_DI_HALT;
				*usb_Msc0DOutCtrlClr = MSC_DOUT_RESET | MSC_DOUT_HALT;
				raid_xfer_status = 0;

				// we shall also check whether the quene is NULL -> TBI(to be improved)	
				if (sata0Obj.sobj_device_state == SATA_DEV_READY)
					sata_Reset(&sata0Obj, USB_SATA_RST);
				if (sata1Obj.sobj_device_state == SATA_DEV_READY)
					sata_Reset(&sata1Obj, USB_SATA_RST);

				rst_all_seg();

				bot_init();
				continue;
			}


			if (usb_int_Status & CTRL_INT)
			{	
				// EP0_SETUP----------------
				if (*usb_Ep0Ctrl & EP0_SETUP)
				{	
					DBG("ep_stp\n");
					usb_control();
				}
			}

			if (usb_int_Status & CDB_AVAIL_INT)
			{// fetch out the CDB as soon as possible
#ifdef USB2_L1_PATCH
				reject_USB2_L1_IN_IO_transfer();
#endif
				site = *usb_CtxtAvailFIFO;
				PCDB_CTXT pCtxt = (PCDB_CTXT)(HOST_CONTEXT_ADDR + ((u32)site << 6));
				pCtxt->CTXT_Index = site;

				pCtxt->CTXT_usb_state = CTXT_USB_STATE_RECEIVE_CIU;
				*usb_IntStatus = CDB_AVAIL_INT;
				if (pCtxt->CTXT_PHASE_CASE & BIT_11)
				{
					pCtxt->CTXT_XFRLENGTH = *((u32 *)(&pCtxt->CTXT_CDB[0] + 16));// CDB[16 : 19 ] is used to save dCBWLength in BOT mode
				}				
				// Que the new arrived CIU to USB Que
				usb_que_ctxt(pCtxt);	
//				if (pCtxt->CTXT_CDB[0] == SCSI_SYNCHRONIZE_CACHE)
//					dbg_next_cmd = 3;
				if (usb_ctxt.newCtxt == NULL)
					usb_ctxt.newCtxt = pCtxt; //new CIU has not been executed
			}

			if(*usb_Msc0Ctrl & MSC_UAS_MODE)
			{
				return;
			}
			
			/****************************************\
				USB Command IU 
			\****************************************/
			if (bot_active == 0) 
			{
				if (usb_ctxt.newCtxt != NULL)
				{
					usb_pwr_func();
					rw_flag = 0;
					
					*usb_Msc0DICtrlClr = MSC_CSW_RUN;
										
					DBG("cdb avail\n");

					if ((sata0Obj.sobj_init == 0) || (sata1Obj.sobj_init == 0))
					{
						if (usb_QEnum())
							goto chk_usb_msc_isr;
					}
					
					PCDB_CTXT pCtxt = usb_ctxt.newCtxt;
					usb_ctxt.newCtxt = usb_ctxt.newCtxt->CTXT_Next;
					bot_active |= BOT_ACTIVE_PROCESS_CBW;
					
					// check cbw 
					if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_RECEIVE_CIU)
					{
						if (pCtxt->CTXT_SAT_FLAG & (CTXT_SAT_FLAG_LENGTH_ERR|CTXT_SAT_FLAG_SIZE_ERR|CTXT_SAT_FLAG_SIG_ERR|CTXT_SAT_FLAG_LUN_ERR)) //
						{
							MSG("wrg cbw\n");
							while(1)
							{

								*usb_Msc0DICtrl = MSC_DI_HALT;
								*usb_Msc0DOutCtrl = MSC_DOUT_HALT;	
								if (*usb_IntStatus & (HOT_RESET|WARM_RESET|USB_BUS_RST))
								{
									ERR("goto reset!");
									return;
								}
								if (*usb_Msc0IntStatus & BOT_RST_INT)
									goto bot_bulk_reset;
								//if (*usb_IntStatus & VBUS_OFF)
								if (USB_VBUS_OFF())
								{
									return;
								}
								if (*usb_Ep0Ctrl & EP0_SETUP)
								{	// host will issue get_status
									DBG("ep_stp\n");
									usb_control();
								}
							}
						}
						pCtxt->CTXT_ccmIndex = CCM_NULL;
						pCtxt->CTXT_DbufIndex = SEG_NULL;
						pCtxt->CTXT_usb_state = CTXT_USB_STATE_CTXT_SETUP_DONE;
						*((u16 *)(&(pCtxt->CTXT_Status))) = (LUN_STATUS_GOOD << 8)|(CTXT_STATUS_PENDING);
					}
					
					logicalUnit = pCtxt->CTXT_LUN;

					if (dbg_next_cmd)
					{
						MSG("cdb %bx, lun %bx, site %bx\n", pCtxt->CTXT_CDB[0], logicalUnit, site);
						dbg_next_cmd--;
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
						if (check_IILegal_boot(pCtxt))
							continue;
					}
				
					if ((logicalUnit == HDD0_LUN) || (logicalUnit == HDD1_LUN))
					{
						switch (pCtxt->CTXT_CDB[0])
						{
							case SCSI_READ6:
							case SCSI_READ12:		
							case SCSI_READ10:
								translate_rw_cmd_to_rw16(pCtxt);
								
							case SCSI_READ16:
							{
_process_read10:
								if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
								{
									if (scsi_chk_valid_rw_cmd(pCtxt))
										break;

									// check case 2 (Hn < Di), case 7 (Hi < Di), or case 10 (Ho <> Di)
									if (pCtxt->CTXT_PHASE_CASE & 0x0484)
									{
										pCtxt->CTXT_Status = CTXT_STATUS_PHASE;			// phase Error
										usb_device_no_data(pCtxt);
										break;
									}
								}
	//							dbg_next_cmd = 1;
								// case 5 (Hi > Di) or case  6(Hi = Di)
								if (vital_data[logicalUnit].cipherID != CIPHER_NONE)
								{
									if ((vital_data[logicalUnit].cipherID == CIPHER_AES_256_XTS) || (vital_data[logicalUnit].cipherID == CIPHER_AES_128_XTS))
									{
										pCtxt->CTXT_FLAG = CTXT_FLAG_AES|CTXT_FLAG_XTS;
									}
									else
										pCtxt->CTXT_FLAG = CTXT_FLAG_AES;
									pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S0R_AES;
								}
								else
								{
									pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S0R;	
									pCtxt->CTXT_FLAG = 0;		
								}
							
								pCtxt->CTXT_Prot = PROT_DMAIN;
								pCtxt->CTXT_ccm_cmdinten = 0; 
		//						pCtxt->CTXT_LunStatus = LUN_STATUS_GOOD;
								pCtxt->CTXT_Status = CTXT_STATUS_PENDING;
								pCtxt->CTXT_No_Data = 0;

								//dbg_next_cmd = 3;
								//MSG("read\n");
					#ifndef DATASTORE_LED
								if ((arrayMode == RAID0_STRIPING_DEV)
						#ifdef SUPPORT_SPAN
									|| (arrayMode == SPANNING)
						#endif
									)
									raid_exec_ctxt(pCtxt);
                        
						#if 1
                            //#ifdef SUPPORT_RAID1
								else if (arrayMode == RAID1_MIRROR)
								{	// use the ping pong read/write
									if (array_status_queue[0] == AS_GOOD)
									{
										pSataObj = &sata0Obj;
							#if 1
									 	// but does not toggle to the sata1 always to avoid to disturb the sequential read/write
										if ( raid1_active_timer == 0 )
										{
											if (sata1Obj.sobj_device_state == SATA_DEV_READY)
											{
												pSataObj = &sata1Obj;
												pCtxt->CTXT_DbufIndex |= DBUF_SATA1_CHANNEL;
											}
										}
							#endif			
									}
									else
									{
							#ifdef DBG_FUNCTION1
										pSataObj = &sata0Obj;
							#else

										if (raid1_use_slot == 0)
											pSataObj = &sata0Obj;
										else
										{
											pSataObj = &sata1Obj;
											pCtxt->CTXT_DbufIndex |= DBUF_SATA1_CHANNEL;
										}
										
							#endif			
									}
									sata_exec_ctxt(pSataObj, pCtxt);
								}
                            //#endif //SUPPORT_RAID1
						#endif		
								else
								{
									if ((logicalUnit == HDD0_LUN) && (sata0Obj.sobj_device_state != SATA_NO_DEV))
									{
										pSataObj = &sata0Obj;
									}
									else
									{
										if (vital_data[logicalUnit].cipherID != CIPHER_NONE)
											pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S1R_AES;
										else
											pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S1R;
										pSataObj = &sata1Obj;
									}
									sata_exec_ctxt(pSataObj, pCtxt);
								}
                    #else//DATASTORE_LED
                                    if ((logicalUnit == HDD0_LUN) && (sata0Obj.sobj_device_state != SATA_NO_DEV))
                                    {
                                        pSataObj = &sata0Obj;
                                    }
                                    else
                                    {
                                        if (vital_data[logicalUnit].cipherID != CIPHER_NONE)
                                        pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S1R_AES;
                                        else
                                            pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S1R;
                                        pSataObj = &sata1Obj;
                                    }
                                    sata_exec_ctxt(pSataObj, pCtxt); 
                    #endif//DATASTORE_LED
							}
							break;
                            
						case SCSI_WRITE6:
						case SCSI_WRITE10:
						case SCSI_WRITE12:
							translate_rw_cmd_to_rw16(pCtxt);
							
						case SCSI_WRITE16:
							if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
							{
								if (scsi_chk_valid_rw_cmd(pCtxt))
									break;
								
								// check check case 3(Hn < Do), 8(Hi <> DO), or case 13 (Ho < Do)
								if (pCtxt->CTXT_PHASE_CASE & 0x2108)
								{
									pCtxt->CTXT_Status = CTXT_STATUS_PHASE;			// CSW status
									usb_device_no_data(pCtxt);
									break;							
								}
							}
							
							u32 byteLength = ((CmdBlk(7) << 8) | CmdBlk(8)) << 9;					
							
							if (pCtxt->CTXT_PHASE_CASE & BIT_11)
							{
								// dcbwLength is saved in cmd cdb[16:19] in little endian in case 11
								*usb_Msc0Residue = pCtxt->CTXT_XFRLENGTH;
								hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);	
								usb_device_no_data(pCtxt);
								break;
							}
							// when caching write page is supported, the host will try to issue SCSI write10 with FUA bit set
							pCtxt->CTXT_FIS[FIS_COMMAND] = 0x35; // ATA WRITE DMA EXT
							
							if (vital_data[logicalUnit].cipherID != CIPHER_NONE)
							{
								if ((vital_data[logicalUnit].cipherID == CIPHER_AES_256_XTS) || (vital_data[logicalUnit].cipherID == CIPHER_AES_128_XTS))
								{
									pCtxt->CTXT_FLAG = CTXT_FLAG_AES|CTXT_FLAG_XTS;
								}
								else
									pCtxt->CTXT_FLAG = CTXT_FLAG_AES;
								pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S0W_AES;
							}
							else
							{
								pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S0W;
								pCtxt->CTXT_FLAG = 0;			
	//							pCtxt->CTXT_DBUF_IN_OUT = (RX_DBUF_SATA0_R_PORT << 4) |  RX_DBUF_USB_W_PORT;
							}

	//						dbg_next_cmd = 1;
							pCtxt->CTXT_Prot = PROT_DMAOUT;
							pCtxt->CTXT_ccm_cmdinten = 0; 
							pCtxt->CTXT_No_Data = 0;
	//						pCtxt->CTXT_LunStatus = LUN_STATUS_GOOD;
							pCtxt->CTXT_Status = CTXT_STATUS_PENDING;

							if (arrayMode != JBOD)
							{
								if (raid_exec_ctxt(pCtxt) == CTXT_STATUS_ERROR)
								{
									hdd_err_2_sense_data(pCtxt, ERROR_UA_NOT_READY);
									usb_device_no_data(pCtxt);
									break;
								}
							}
							else
							{
								if ((logicalUnit == HDD0_LUN) && (sata0Obj.sobj_device_state != SATA_NO_DEV))
									pSataObj = &sata0Obj;
								else
								{
									if (vital_data[logicalUnit].cipherID != CIPHER_NONE)
										pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S1W_AES;
									else
										pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S1W;
									pSataObj = &sata1Obj;
								}
								sata_exec_ctxt(pSataObj, pCtxt);
							}
														
							if (dbg_next_cmd)
								MSG("Ct S %bx\n", pCtxt->CTXT_Status);

							break;
						
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
							scsi_StartAtaCmd(pSataObj, pCtxt);

		//					*usb_IntStatus = CDB_AVAIL_INT|MSC0_INT;
						}	// switch (pCtxt->CTXT_CDB[0])
					}
#ifdef WDC_VCD 
					else if (logicalUnit ==VCD_LUN)
					{
						vcd_start_command(pCtxt);
					}
#endif
#ifdef WDC_SES
					else if (logicalUnit == SES_LUN)
					{
						ses_start_command(pCtxt);
					}
#endif
					else
					{
						scsi_cmd_unsupport_lun(pCtxt);
					}
				}
			}
			else 
			{
				if (bot_active & (BOT_ACTIVE_WAIT_FOR_SATA0_READY | BOT_ACTIVE_WAIT_FOR_SATA1_READY))
				{
					if (( sata0Obj.sobj_State == SATA_READY) || (sata0Obj.sobj_device_state == SATA_NO_DEV))
					{
						bot_active &= ~BOT_ACTIVE_WAIT_FOR_SATA0_READY;
					}
					if ((sata1Obj.sobj_State == SATA_READY) || (sata1Obj.sobj_device_state == SATA_NO_DEV))
					{
						bot_active &= ~BOT_ACTIVE_WAIT_FOR_SATA1_READY;
					}
				}
			}

			if (usb_int_Status & MSC0_INT)
			{
chk_usb_msc_isr: 
				u32	 msc_int_status = *usb_Msc0IntStatus;

				if (msc_int_status & MSC_CSW_DONE_INT)
				{
					DBG("csw dn\n");
					//*usb_Msc0IntStatus = MSC_CSW_DONE_INT|MSC_TX_DONE_INT|MSC_RX_DONE_INT|MSC_DIN_HALT_INT|MSC_DOUT_HALT_INT;
CSW_DONE:
					if (dbg_next_cmd)
						MSG("csw dn clr\n");
					*usb_Msc0IntStatus = *usb_Msc0IntStatus;
					usb_ctxt.curCtxt = NULL;
					*usb_Msc0DICtrlClr = MSC_CSW_RUN;
					rw_flag = 0; // for the normal case, the rw_flag should be cleared when CSW is sent out
					bot_active &= ~BOT_ACTIVE_PROCESS_CBW;

					
#ifdef CSW_0_LENGTH
					csw_ctrl = 0;
#endif
					chk_power_Off();
#if WDC_ENABLE_QUICK_ENUMERATION
					if (re_enum_flag)
					{
						DBG("re-enum2\n");

						// re-enumration	
						Delay(250);
						if (re_enum_flag == DOUBLE_ENUM)
							usb_reenum();
						else
							return;
//							Delay(5);// wait for USB3's training
						re_enum_flag = 0;
					}
#endif
					goto begin;
				}

				if (msc_int_status & (MSC_TX_DONE_INT|MSC_RX_DONE_INT|MSC_DIN_HALT_INT|MSC_DOUT_HALT_INT))
					usb_msc_isr();
			}	// if ((usb_int_Status & MSC0_INT)
		}	// if (chip_IntStaus & USB_INT)

		if (*chip_IntStaus & SATA0_INT)
		{		
			sata_isr(&sata0Obj);
		}

		if (*chip_IntStaus & SATA1_INT)
		{		
			sata_isr(&sata1Obj);
		}
		usb_chk();
	} // End of while loop
}

