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

#define	ATA_C

#include	"general.h"
//#include "dispatch.h"


/****************************************\
	ata_ExecUSBNoDataCmd
\****************************************/
u32 ata_ExecUSBNoDataCmd(PSATA_OBJ  pSob, PCDB_CTXT pCtxt, u8 command)
{
	DBG("ata_ExecUSBNoDataCmd: %BX\n", command);

	pCtxt->CTXT_FLAG |= CTXT_FLAG_B2S;
	pCtxt->CTXT_Prot = PROT_ND;
	if (curMscMode == MODE_UAS)
		pCtxt->CTXT_XFRLENGTH = 0;
	pCtxt->CTXT_TimeOutTick = 0;
	pCtxt->CTXT_ccm_cmdinten = D2HFISIEN;
	pCtxt->CTXT_DbufIndex = SEG_NULL;
	*((u32 *)&(pCtxt->CTXT_FIS[FIS_TYPE])) = (command << 16)|(0x80 << 8)|(HOST2DEVICE_FIS) ;
	*((u32 *)&(pCtxt->CTXT_FIS[FIS_LBA_LOW])) = (MASTER << 24);
	*((u32 *)&(pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT])) = 0;
	*((u32 *)&(pCtxt->CTXT_FIS[FIS_SEC_CNT])) = 0;

	return sata_exec_ctxt(pSob, pCtxt);
}


/****************************************\
	AtaExecNoDataCmd
\****************************************/
u32 ata_ExecNoDataCmd(PSATA_OBJ  pSob, u32 cmdAndFeature, u32 fis_lba, u8 timeout_tick)
{
	CDB_CTXT ctxt;
	DBG("nd cmd\n");
	if (timeout_tick == 0)
		ctxt.CTXT_FLAG = CTXT_FLAG_B2S;
	else
		ctxt.CTXT_FLAG = CTXT_FLAG_B2S|CTXT_FLAG_SYNC;
	ctxt.CTXT_Prot = PROT_ND;
	ctxt.CTXT_XFRLENGTH = 0;
	ctxt.CTXT_ccm_cmdinten = D2HFISIEN;
	ctxt.CTXT_TimeOutTick = timeout_tick;
	ctxt.CTXT_DbufIndex = SEG_NULL;
	*((u32 *)&(ctxt.CTXT_FIS[FIS_TYPE])) = (cmdAndFeature << 16)|(0x80 << 8)|(0x27) ;
	*((u32 *)&(ctxt.CTXT_FIS[FIS_LBA_LOW])) = (MASTER << 24)|fis_lba;
	*((u32 *)&(ctxt.CTXT_FIS[FIS_LBA_LOW_EXT])) = 0;
	*((u32 *)&(ctxt.CTXT_FIS[FIS_SEC_CNT])) = 0;

	return sata_exec_ctxt(pSob, &ctxt);
}

/****************************************\
	AtaExecNoDataCmd
\****************************************/
u32 ata_ExecNoDataCmd_TwoHDDs(u32 cmdAndFeature, u32 fis_lba, u8 timeout_tick)
{
	CDB_CTXT ctxt;
	DBG("nd cmd\n");

	ctxt.CTXT_FLAG = CTXT_FLAG_B2S;
	ctxt.CTXT_Prot = PROT_ND;
	ctxt.CTXT_XFRLENGTH = 0;
	ctxt.CTXT_ccm_cmdinten = D2HFISIEN;
	ctxt.CTXT_TimeOutTick = timeout_tick;
	ctxt.CTXT_DbufIndex = SEG_NULL;
	*((u32 *)&(ctxt.CTXT_FIS[FIS_TYPE])) = (cmdAndFeature << 16)|(0x80 << 8)|(0x27) ;
	*((u32 *)&(ctxt.CTXT_FIS[FIS_LBA_LOW])) = (MASTER << 24)|fis_lba;
	*((u32 *)&(ctxt.CTXT_FIS[FIS_LBA_LOW_EXT])) = 0;
	*((u32 *)&(ctxt.CTXT_FIS[FIS_SEC_CNT])) = 0;
	CDB_CTXT ctxt1 = ctxt;

	if ((sata_exec_ctxt(&sata0Obj, &ctxt) != CTXT_STATUS_PENDING) || (sata_exec_ctxt(&sata1Obj, &ctxt1) != CTXT_STATUS_PENDING))
		return CTXT_STATUS_ERROR;
	while (1)
	{// wait for two HDDs's status fis response
		for (u8 i = 0; i < 2; i++)
		{
			pSataObj = (SATA_OBJ *)&sata0Obj + i;
			PEXT_CCM pScm = &pSataObj->sobj_Scm[i?ctxt1.CTXT_ccmIndex:ctxt.CTXT_ccmIndex];
			sata_ch_reg = pSataObj->pSata_Ch_Reg;
			if (sata_ch_reg->sata_FrameInt & D2HFISI)
			{
				sata_ch_reg->sata_FrameInt = D2HFISI;
				// clear SATA Frame interrupt
				sata_ch_reg->sata_IntStatus = FRAMEINTPEND;
				// one channel receive the FIS 34
				pSataObj->sobj_State = SATA_READY;
			}
			if (pScm->scm_TimeOutTick == 0)
			{
				sata_Reset(&sata0Obj, SATA_HARD_RST);
				sata_Reset(&sata1Obj, SATA_HARD_RST);
				return CTXT_STATUS_ERROR;
			}
		}
		if ((sata0Obj.sobj_State == SATA_READY) && (sata1Obj.sobj_State == SATA_READY))
			break;
	}
	// Release CCM/SCM
	sata_DetachCCM(&sata0Obj, 1 << ctxt.CTXT_ccmIndex);
	sata_DetachCCM(&sata1Obj, 1 << ctxt1.CTXT_ccmIndex);

	// Check Status register of ATA TFR
	if ((sata0Obj.pSata_Ch_Reg->sata_Status & ATA_STATUS_CHECK) || (sata1Obj.pSata_Ch_Reg->sata_Status & ATA_STATUS_CHECK))
	{
		return CTXT_STATUS_ERROR;
	}
	else
	{
		return CTXT_STATUS_GOOD;
	}
}

#ifdef WDC
/************************************************************************\
 ata_startup_hdd() - Turn on, spin up and initialize the HDD.
	local_B2S_flag will be used for the command like check the HDDs' temperature
	the others USB to SATA commands will not use it
	lun is used to calculate the next state of the spin up function
 Returns:
	0 if successful; 1 if the HDD isn't responding or an error occured.
 */
u8 ata_startup_hdd(u8 sync_flag, u8 sata_ch_flag)
{
u8 rc = 1;

	// Turn on power to the HDD and whatever else needed.
	// base the new architecture for 3607, F/W will not wait for the power on & spin up 
	u8 spin_up_two_disks = 0;
	MSG("pwr %bx, ch %bx\n", hdd_power, sata_ch_flag-1);
	if (arrayMode == JBOD)
	{
		if (hdd_power & HDD1_POWER_ON)
		{ // SATA0 is power off & SATA 1 is power on
			if ((sata_ch_flag & SATA_CH0) == 0)
				return 0;
			else
			{ // turn on SATA 0 only
				pSataObj = &sata0Obj;
			}
		}
		else if (hdd_power & HDD0_POWER_ON)
		{// SATA 1 is power off
			if ((sata_ch_flag & SATA_CH1) == 0)
				return 0;
			else // turn on SATA1
				pSataObj = &sata1Obj;
		}
		else
		{ // SATA 0 & 1 is both power off
			if (sata_ch_flag == SATA_TWO_CHs)
				spin_up_two_disks = 1; // turn on both SATA0 & SATA1
			else if (sata_ch_flag & SATA_CH0)
				pSataObj = &sata0Obj;
			else 
				pSataObj = &sata1Obj;
		}
	}
	else
	{
		spin_up_two_disks = 1;
	}
	
	if (spin_up_two_disks)
	{
		set_hdd_power(0x0F, 0);
		disable_upkeep &= ~(UPKEEP_HDD_SPUN_DOWN_S0 | UPKEEP_HDD_SPUN_DOWN_S1);
		// In most cases, the standby condition timer should be enabled.
		// Caller must disable it explicitly if it should not be.
		disable_upkeep &= ~(UPKEEP_HOST_CONTROLS_POWER_CONDITION_S0|UPKEEP_HOST_CONTROLS_POWER_CONDITION_S1);
		if (curMscMode == MODE_UAS)
		{
			if (sata0Obj.sobj_device_state != SATA_NO_DEV)
			{
				uas_ci_paused |= UAS_PAUSE_SATA0_NOT_READY;
			}
			if (sata1Obj.sobj_device_state != SATA_NO_DEV)
			{
				uas_ci_paused |= UAS_PAUSE_SATA1_NOT_READY;
			}
		}
		else
		{
			if (sata0Obj.sobj_device_state != SATA_NO_DEV)
			{
				bot_active |= BOT_ACTIVE_WAIT_FOR_SATA0_READY;
			}
			if (sata1Obj.sobj_device_state != SATA_NO_DEV)
			{
				bot_active |= BOT_ACTIVE_WAIT_FOR_SATA1_READY;
			}
		}
	}
	else
	{
		if (pSataObj->sobj_sata_ch_seq)
		{
			set_hdd_power(PWR_CTL_SLOT1_EN |PU_SLOT1, 0);
			
			disable_upkeep &= ~UPKEEP_HDD_SPUN_DOWN_S1;
			// In most cases, the standby condition timer should be enabled.
			// Caller must disable it explicitly if it should not be.
			disable_upkeep &= ~UPKEEP_HOST_CONTROLS_POWER_CONDITION_S1;
			if (curMscMode == MODE_UAS)
			{
				if (sata1Obj.sobj_device_state != SATA_NO_DEV)
				{
					uas_ci_paused |= UAS_PAUSE_SATA1_NOT_READY;
				}
			}
			else
			{
				if (sata1Obj.sobj_device_state != SATA_NO_DEV)
				{
					bot_active |= BOT_ACTIVE_WAIT_FOR_SATA1_READY;
				}
			}
		}
		else
		{
			set_hdd_power(PWR_CTL_SLOT0_EN|PU_SLOT0, 0);
			disable_upkeep &= ~UPKEEP_HDD_SPUN_DOWN_S0;
			// In most cases, the standby condition timer should be enabled.
			// Caller must disable it explicitly if it should not be.
			disable_upkeep &= ~UPKEEP_HOST_CONTROLS_POWER_CONDITION_S0;
			if (curMscMode == MODE_UAS)
			{
				if (sata0Obj.sobj_device_state != SATA_NO_DEV)
				{
					uas_ci_paused |= UAS_PAUSE_SATA0_NOT_READY;
				}
			}
			else
			{
				if (sata0Obj.sobj_device_state != SATA_NO_DEV)
				{
					bot_active |= BOT_ACTIVE_WAIT_FOR_SATA0_READY;
				}
			}
		}
	}
	set_activity_for_standby(0);


	if (sync_flag)
	{
		u8 flag = spin_up_two_disks ? SATA_TWO_CHs: (1 << pSataObj->sobj_sata_ch_seq);
		rc = sata_await_fis34(flag);
		if (rc == 0)
		{	
			sata_spinup_timer = 7000 - sata_spinup_timer;
			sata_spinup_timer = (sata_spinup_timer >> 7); // 
			
			MSG("%x\n", sata_spinup_timer);
			if (sata_spinup_timer) 
			{
				if (sata_spinup_timer > 42) // it seems that the tool of WD will reset the device
					sata_spinup_timer = 42;
				if (spin_up_two_disks)
					rc = ata_ExecNoDataCmd_TwoHDDs(ATA6_IDLE_IMMEDIATE, 0, sata_spinup_timer); 
				else 
					rc = ata_ExecNoDataCmd(pSataObj, ATA6_IDLE_IMMEDIATE, 0, sata_spinup_timer); 
			}
			else
				rc = 1;
		}
	}
	
	if (rc)
	{
		_disable();
		chk_fis34_timer = CHECK_FIS34_TIMER;
		spinup_timer = 3; 	// the spin up timer is around 7 secs 
#if WDC_HDD_TEMPERATURE_POLL_TICKS
		expect_temperatureTimer32 = WDC_HDD_TEMPERATURE_POLL_TICKS;
		check_temp_flag = 0;
#endif
#if FAN_CTRL_EXEC_TICKS
		fan_ctrl_exec_ticker = FAN_CTRL_EXEC_TICKS;
		fan_ctrl_exec_flag = 0;
#endif
		_enable();
	}
// power on HDD, and wait for HDD's ready
// if the HDD is not ready in 7secs, report lun not ready
//	init_timer1_interrupt(7000, BEST_CLOCK); // set the 7 secs timer
	MSG("%bx\n", rc);
	return rc;
}

/************************************************************************\
 ata_shutdown_hdd() - Spins down and turns off the HDD.

 Arguments:
	power_off	true to turn off the HDD after it is spun down.

 Returns:
	Nothing.
 */
void ata_shutdown_hdd(u8 power_off, u8 sata_ch_flag)
{
	// Explicitly spinning down the HDD before turning it off
	// avoids an emergency retract and mechanical stresses.
	// It's also a good idea to flush its cache too.
	MSG("shtdnHDD %bx, pwr%bx\n", power_off, hdd_power);

	// Don't talk to the HDD if it is powered off, duh!
	for (u8 i = 0; i < 2; i++)
	{
		u8 slotNum = (1<<i);
		if ((sata_ch_flag & slotNum) == 0) // find the channel which will be shutdown
			continue;
		if (i)
			pSataObj = &sata1Obj;
		else
			pSataObj = &sata0Obj;
		
		SATA_CH_REG volatile *sata_ch_reg = pSataObj->pSata_Ch_Reg;
		if ((hdd_power & (HDD0_POWER_ON << i)) && (pSataObj->sobj_device_state != SATA_NO_DEV))
		{
			// The HDD should not be busy or waiting for data at this point,
			// but it might be if the USB cable was unplugged in the middle
			// of a command; reset the HDD if it's busy so we can move on.
			// Don't be reset-happy or we risk losing cached data.
			if ((pSataObj->sobj_State != SATA_PWR_DWN) && (pSataObj->sobj_State != SATA_READY))
			{
				// Reset the HDD and wait for it to be ready, but not forever.
				scan_sata_device(slotNum);
			}
			if (pSataObj->sobj_State == SATA_READY)
			{
				// Flush the disk's cache...
				ata_ExecNoDataCmd(pSataObj, ATA6_FLUSH_CACHE_EXT, 0, WDC_SATA_TIME_OUT);

				// ...then spin it down.
				ata_ExecNoDataCmd(pSataObj, ATA6_STANDBY_IMMEDIATE, 0, WDC_SATA_TIME_OUT);
			}
#ifndef SUPPORT_HR_HOT_PLUG		// we can not cut the power of sata phy if we need support hot plug
#if (PWR_SAVING)
			// Turn off the SATA drive and unneeded circuitry...
			if (!((arrayMode == RAID1_MIRROR) && (raid_mirror_status)))  // it is going to rebuild offline
				set_hdd_power(((!power_off | PWR_CTL_SLOT0_EN) << i), 0); 
#endif
#endif
		}
		// ...and suspend background activities.
		disable_upkeep |= (UPKEEP_HDD_SPUN_DOWN_S0 << (i*8));
	}
}

void ata_hdd_standby(void)
{
	if (standby_timer_expired)
	{		
		if (!(disable_upkeep) && (sata0Obj.sobj_State == SATA_READY) && (sata1Obj.sobj_State == SATA_READY))
		{
			MSG("H stdby 2\n");
			ata_shutdown_hdd(TURN_POWER_OFF, SATA_TWO_CHs);
#ifdef HDR2526_LED
			hdr2526_led_off(ACT_IDLE_OFF);
#else			
			set_activity_for_standby(1);
#endif
#if FAN_CTRL_EXEC_TICKS
			enableFan(0);
#endif
			standby_timer_expired = 0;
		}
	}
}
/****************************************\
	ATA_get_serial_num
\****************************************/
void ata_get_serial_num(u8 *serialStr)
{
u8 indx;

	// left-justfied Serial #
	for (u32 i=0;  i < HDD_SERIAL_NUM_LEN;  i++) {
		if (serialStr[i] != ' ')
		{
			// found 1st non-space character
			if (i == 0)
			{
				// Left-justfied already
				// replace 0 with Space charcter.
				for (; i < HDD_SERIAL_NUM_LEN;  i++)
				{
					if (serialStr[i] == 0)
						serialStr[i] = ' ';
				}
				break;
			}

			// not Left-justfied
			indx = 0;
			for (; i < HDD_SERIAL_NUM_LEN;  i++) {
		      // quit as soon as we hit a 0.  WD pads the serial number with 0's (not
		      // to be confused with an ascii '0')
				u8 val = serialStr[i];
				if (val == 0)
					val = ' ';
				serialStr[indx] = val;
				indx++;
			}
			// discard "WD-"
			if ( (serialStr[0] == 'W') && 
			     (serialStr[1] == 'D') &&
			     (serialStr[2] == '-') )
			{
				for (i = 0; i < indx; i++)
				{
					serialStr[i] = serialStr[i+3];
				}
				indx -= 3;
			}			
			for (; indx < HDD_SERIAL_NUM_LEN;  indx++)
				serialStr[indx] = ' ';
			break;
		}
	}
}

// use to check the check for ID command
u8 ata_checksum8(const u8 *buf)
{
u8 cksum, nWords;

	cksum = 0;
	nWords = 0;			// Use an 8-bit counter and sum two bytes at a time.

	do
	{
		// This is faster and smaller than doing "cksum += *buf++" twice.
		cksum += buf[0];
		cksum += buf[1];
		buf += 2;

		nWords--;
	} while (nWords);
#ifdef DEBUG
	printf("chsum: %x\n", (int)cksum);
#endif
	return cksum;
}

void ata_load_fis_from_rw16Cdb(PCDB_CTXT pCtxt, u8 xfr_type)
{
	// Load the ATA task file with the LBA and Xfer Length in the CDB.
	// Optimization: loading registers in I/O-address order results
	// in smaller & faster code. Initio does the same.


	// Load the Logical Block Address (LBA).
	//pCtxt->CTXT_FIS[FIS_LBA_LOW] =   cdb.byte[9];
	//pCtxt->CTXT_FIS[FIS_LBA_MID] =   cdb.byte[8];
	//pCtxt->CTXT_FIS[FIS_LBA_HIGH] =   cdb.byte[7];
	//pCtxt->CTXT_FIS[FIS_DEVICE] = 0x40;
	*((u32 *)&(pCtxt->CTXT_FIS[FIS_LBA_LOW])) = (0x40 << 24) | (cdb.byte[7] << 16) | (cdb.byte[8] << 8) | (cdb.byte[9]);

	//pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT] =  cdb.byte[6];
	//pCtxt->CTXT_FIS[FIS_LBA_MID_EXT] =  cdb.byte[5];
	//pCtxt->CTXT_FIS[FIS_LBA_HIGH_EXT] = cdb.byte[4];		// 0;
	//pCtxt->CTXT_FIS[FIS_FEATURE_EXT] = 0;
	*((u32 *)&(pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT])) = (cdb.byte[4] << 16) | (cdb.byte[5] << 8) | (cdb.byte[6]);
	if (xfr_type == FPDMA_TYPE)
	{
		// Read and write commands reserve the Feature register, so set it to 0.
		pCtxt->CTXT_FIS[FIS_FEATURE] = cdb.byte[13];
		pCtxt->CTXT_FIS[FIS_FEATURE_EXT] = cdb.byte[12];
	}
	else
	{
		// Load the transfer length - only the low-order 16 bits are used.	
		//pCtxt->CTXT_FIS[FIS_SEC_CNT] =  cdb.byte[13];
		//pCtxt->CTXT_FIS[FIS_SEC_CNT_EXT] =  cdb.byte[12];
		pCtxt->CTXT_FIS[FIS_FEATURE] = 0;
//		pCtxt->CTXT_FIS[FIS_FEATURE_EXT] = 0;
		*((u32 *)&(pCtxt->CTXT_FIS[FIS_SEC_CNT])) =  (cdb.byte[12] << 8) | (cdb.byte[13]);
	}
	// Caller has to write the ATA Command register to kick off the command.
}
#endif

// create the function to save code
// to fetch the Identify_device data 
// or general purpose log's directory
u8 fetch_HDD_data(u8 ata_cmd, u8 sata_ch_flag,u8 log_addr, u8 sync_flag)
{
	CDB_CTXT ctxt;
	
#ifdef DBG_FUNCTION
	if (ata_cmd == ATA6_IDENTIFY_DEVICE)
	{
		MSG("ID %bx\n", sata_ch_flag-1);
	}
	else if (ata_cmd == ATA6_READ_LOG_EXT)
	{
		MSG("RdQueErr %bx\n", sata_ch_flag-1);
	}
#endif
	
	if (sync_flag)
	{
		ctxt.CTXT_FLAG = CTXT_FLAG_B2S |CTXT_FLAG_SYNC;
	}
	else
	{
		ctxt.CTXT_FLAG = CTXT_FLAG_B2S;
	}

	ctxt.CTXT_Prot = PROT_PIOIN;
	
	ctxt.CTXT_TimeOutTick = WDC_SATA_TIME_OUT;

	if(sata_ch_flag == SATA_CH0)
	{
		pSataObj =  &sata0Obj;
		ctxt.CTXT_DbufIndex = DBUF_SEG_B2S0R;
	}
	else
	{
		pSataObj =  &sata1Obj;
		ctxt.CTXT_DbufIndex = DBUF_SEG_B2S1R;
	}

	if (ata_cmd == ATA6_READ_LOG_EXT)
	{
		MSG("dbufS4 %bx\n", dbufObj.TxDbuf_segStatus[4]);
		if (dbufObj.TxDbuf_segStatus[4] == SEG_BUSY)
		{
			ctxt.CTXT_DbufIndex |= DBUF_U2S_SEG2;
		}
	}
	
	ctxt.CTXT_ccm_cmdinten = 0;
	ctxt.CTXT_XFRLENGTH = 512;
	*((u32 *)&(ctxt.CTXT_FIS[FIS_TYPE])) = (ata_cmd << 16)|(0x80 << 8)|(0x27) ;

	*((u32 *)&(ctxt.CTXT_FIS[FIS_LBA_LOW])) = (MASTER << 24) | log_addr;
	*((u32 *)&(ctxt.CTXT_FIS[FIS_LBA_LOW_EXT])) = 0;
	u8 tmp8 = 0;
	if (ata_cmd == ATA6_READ_LOG_EXT)
	{
		tmp8 = 1;	
	}
	*((u32 *)&(ctxt.CTXT_FIS[FIS_SEC_CNT])) = tmp8;
	
	sata_exec_ctxt(pSataObj, &ctxt);
	return 0;
}

// read the Queued Error log page 
u8 read_QueuedErrLog(u8 sata_ch_flag)
{
	return fetch_HDD_data(ATA6_READ_LOG_EXT, sata_ch_flag, 0x10, 0x00);	
}

/****************************************\
	ata_ReadID
	Reading 512 bytes of information
	in response to IDENTIFY_DEVICE
\****************************************/
u8 ata_ReadID(u8 sata_ch_flag)  // could have argument - ATA/ATAPI , which ID command to use     // to do 2 times and compare
{
	return fetch_HDD_data(ATA6_IDENTIFY_DEVICE, sata_ch_flag, 0x00, 0x00);
}

/*************************************************************\
	ata_init
	return value: 0 -> success
			      1 -> Sata device initialize failure
			      2 -> Serial number doesn't match the saved SN of flash
\*************************************************************/
u8 ata_init(PSATA_OBJ  pSob)
{
	if ( (mc_buffer[255 *2] != 0xA5)  ||
	     (ata_checksum8(mc_buffer) != 0) )
	{
		pSob->sobj_Init_Failure_Reason = HDD_INIT_FAIL_ATA_INIT_ERROR;
		return 1;
	}
	
	memcpySwap16(pSob->modelNum, (mc_buffer + 27 * 2), 20);

	// Start by assuming the HDD's physical block size is 512 bytes.
	pSob->physicalBlockSize_log2 = 9;			// 2**9 = 512

	if ((mc_buffer[(106 * 2) +1] & 0xc0) == 0x40 /*is word 106 valid?*/ )
	{
		// Spec_4.1.2_4 Don't use this HDD if its logical sector size is > 512 bytes.
		if (mc_buffer[(106 * 2) +1] & 0x10)  
		{
			pSob->sobj_Init_Failure_Reason = HDD_INIT_FAIL_ATA_INIT_ERROR;
			return 1;
		}
		if (mc_buffer[(106 * 2) +1] & 0x20 /*large physical blocks?*/ )
		{
			pSob->physicalBlockSize_log2 = (mc_buffer[106 * 2] & 0x0f) + 9;
		}
	}
	
	if (mc_buffer[76 * 2 + 1] & 0x01)
	{	// support NCQ
		u32 bit, qdepth = mc_buffer[75 * 2] & 0x1F;

		bit = 1;
		for (;qdepth != 0 ; qdepth--) 
		{
			bit = (bit << 1) | 1;
		}
		DBG("Qdep: %lx\n", bit);
//		bit = 0xFFF;
 		pSob->default_cFree = bit;
		pSob->cFree = bit;		
	}
	else
	{
		MSG("Qd 1\n");
		ncq_supported = 0;
	}	 

#ifdef SUPPORT_PARTIAL_MODE
	if (mc_buffer[76 * 2 + 1] & 0x02)  // bit 9
	{
		MSG("PM support\n");
		ata_ExecNoDataCmd(pSob, (FEATURE_SET_TRANSFER_MODE << 8)|ATA6_SET_FEATURES, 0, WDC_SATA_TIME_OUT);
	}
#endif	
	//ata 6 && 48 bit addr supported
	//word 80 & 83 from device information
	if (mc_buffer[(80 * 2)] & 0x40 && mc_buffer[(83 * 2) + 1] & 0x04)
	{
		pSob->sobj_deviceType= DEVICETYPE_HD_ATA6;
		pSob->sectorLBA =  *((u64 *)(&mc_buffer[100*2]));
	}
	else
	{
		pSob->sobj_deviceType = DEVICETYPE_HD_ATA5;
		pSob->sectorLBA = *((u32 *)(&mc_buffer[60*2]));
	}
	
	// Don't use this HDD if it is too small.
	if (pSob->sectorLBA < MIN_HDD_SIZE)
	{
		pSob->sobj_Init_Failure_Reason = HDD_INIT_FAIL_ATA_INIT_ERROR;
		return 1;
	}
	
//	*usb_Msc0LunExtentLo	= (u32)((pSob->sectorLBA-1) & 0xFFFFFFFF);
//	*usb_Msc0LunExtentHi	= (u32)((pSob->sectorLBA-1) >> 8);

	DBG("S%bx: LBA: %bx %lx\n", pSob->sobj_sata_ch_seq, (u8)(pSob->sectorLBA >> 32), (u32)pSob->sectorLBA);

	pSob->sobj_WrCache = WRITE_CACHE_UNSUPPORT;

	// Is Write Cache supported ?
	if (mc_buffer[(82 * 2)] & 0x20)
	{	// Is Write Cache Enabled ?
		pSob->sobj_WrCache = WRITE_CACHE_ENABLE;		
		if ((mc_buffer[(85 * 2)] & 0x20) == 0)
		{
			pSob->sobj_WrCache = WRITE_CACHE_DISABLE;
		}
	}
	memcpySwap16(pSob->serialNum, mc_buffer + 10 * 2, 10);
	ata_get_serial_num(pSob->serialNum);

	disable_upkeep &= ~((pSob->sobj_sata_ch_seq )? UPKEEP_HDD_SPUN_DOWN_S1 : UPKEEP_HDD_SPUN_DOWN_S0);
	//compare the return data from device (identify dev cmd) with fixed data
	//from global NVRAM to determine operation mode for SET_FEATURES command

	pSob->sobj_AAM_State = AAM_STATUS_UNSUPPORT;
	// Check AAM status .
	if (mc_buffer[83 *2 + 1] & 0x02) 	// AAM feature set supported?
	{
		pSob->sobj_AAM_State = AAM_STATUS_ENABLE;
		if((mc_buffer[86 *2 + 1] & 0x02) == 0)
			pSob->sobj_AAM_State = AAM_STATUS_DISABLE;
		if(mc_buffer[94 * 2] == AAM_MAX_PERFORMANCE)
			pSob->sobj_AAM_State |= AAM_ISMAX_PERFORMANCE;
	}

	pSob->sobj_Smart = SMART_STATUS_UNSUPPORT;
	// Check SMART status .
	// Many Artemis features depend on SMART.
	if (mc_buffer[82 *2] & 0x01) 	// SMART feature set supported?
	{
		pSob->sobj_Smart = SMART_STATUS_ENABLE;
		if((mc_buffer[85 *2] & 0x01) == 0)
			pSob->sobj_Smart = SMART_STATUS_DISABLE;
	}

	pSob->sobj_nominal_form_factor = mc_buffer[168 * 2] & 0x0F;

	pSob->sobj_medium_rotation_rate = (mc_buffer[217 * 2 + 1] << 8) + mc_buffer[217 * 2];	
	DBG("rr %x\n", pSob->sobj_medium_rotation_rate);

	return 0;
}
u8 ata_enable_wr_cache(PSATA_OBJ  pSob)
{
	MSG("eWC %bx\n", pSob->sobj_sata_ch_seq);

	if (pSob->sobj_WrCache == WRITE_CACHE_DISABLE)
	{	// Is Write Cache Enabled ?
		pSob->sobj_WrCache = WRITE_CACHE_ENABLE;		// Write Cache enabled
		ata_ExecNoDataCmd(pSob, (FEATURE_ENABLE_WRITE_CACHE << 8)|ATA6_SET_FEATURES, 0, 0);
		pSob->sobj_subState = SATA_ENABLE_WRITE_CACHE;
		rw_time_out = 50;
		return 0;
	}
	return 1;
}

u8 ata_en_AAM(PSATA_OBJ pSob)
{
	MSG("eAAM %bx\n", pSob->sobj_sata_ch_seq);
	// Enable AAM if it is not already enabled  or set max performance if it is not max performance.
	if ((pSob->sobj_AAM_State & AAM_STATUS_DISABLE) ||
		((pSob->sobj_AAM_State & AAM_ISMAX_PERFORMANCE) == 0))
	{	// Is AAM Enabled ? or is not max performance?
		pSob->sobj_AAM_State = AAM_STATUS_ENABLE |AAM_ISMAX_PERFORMANCE;
		// Enabled AAM and set max performance
		ata_ExecNoDataCmd(pSob, (ENABLE_AAM_FEATURE <<8)|ATA6_SET_FEATURES, (AAM_MAX_PERFORMANCE<<16), 0);
		pSob->sobj_subState = SATA_ENABLE_AAM;
		rw_time_out = 50;
		return 0;
	}
	return 1;
}

u8 ata_en_smart_operation(PSATA_OBJ pSob)
{
	MSG("eSMOP %bx\n", pSob->sobj_sata_ch_seq);
	// Enable SMART if it is not already enabled.
	// Many Apollo features depend on SMART.

	if (pSob->sobj_Smart == SMART_STATUS_DISABLE)
	{	// Is SMART Enabled ?
		pSob->sobj_Smart = SMART_STATUS_ENABLE;		// Write Cache enabled
		// Enabled SMART
		ata_ExecNoDataCmd(pSob, (SMART_ENABLE_OPERATIONS<<8)|ATA6_SMART, SMART_SIGNATURE_LOW|(SMART_SIGNATURE_HIGH<<16), 0);
		pSob->sobj_subState = SATA_ENABLE_SMART_OPERATION;
		rw_time_out = 50;
		return 0;
	}
	return 1;
}

u8 ataInit_quick_enum(void) // just save the code size
{
	u32 val = 0;
	sata0Obj.sobj_init = 1;
	sata1Obj.sobj_init = 1;

	if (usb_active)
		return 0;
	usb_active = 1;
	ini_init();
	
	return 0;
}
