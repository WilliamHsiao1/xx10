/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 1998-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *
 *******************************************************************************/
#define INI_C
#include "general.h"


// The Default Passwords are constant so put them in the
// code segment. Use  ini_fill_default_password()  to copy them.
static const u8 default_password32[] = 
				{	0x03, 0x14, 0x15, 0x92, 0x65, 0x35, 0x89, 0x79,
					0x32, 0x38, 0x46, 0x26, 0x43, 0x38, 0x32, 0x79,
					0xFC, 0xEB, 0xEA, 0x6D, 0x9A, 0xCA, 0x76, 0x86,
					0xCD, 0xC7, 0xB9, 0xD9, 0xBC, 0xC7, 0xCD, 0x86 };

static const u8 default_password16[] =
				{	0x03, 0x14, 0x15, 0x92, 0x65, 0x35, 0x89, 0x79,
					0x2B, 0x99, 0x2D, 0xDF, 0xA2, 0x32, 0x49, 0xD6 };


static u8 ReadWritePrivatePage(u8 log_addr, u8 page_addr, u8 options);

#ifndef INITIO_STANDARD_FW
static u8 CheckIniData(void);
static u8 load_ini_Data(u8 slot_num,u8 backup_flg);
static u8 check_passworded_data(u8 slot_num);
static u8 init_new_hdd(u8 slot_num);
#endif

void ClearUserPassword(u8 slot_num)
{
	xmemset((u8 *)PwdData.psw_disk[slot_num].user_psw, 0, MAX_PASSWORD_SIZE);
}

void SetUserPassword(const u8 *pwd, u8 len,u8 slot_num)
{
	xmemset((u8 *)PwdData.psw_disk[slot_num].user_psw, 0, MAX_PASSWORD_SIZE);

	if (len)  xmemcpy((u8 *)pwd, (u8 *)PwdData.psw_disk[slot_num].user_psw, len);
}

/************************************************************************\
 seal_drive() - Locks the drive if the user has set a password.

 The user has to re-authenticate in order to access their data.

 Nothing is done if the user's password is not set.
 */
void seal_drive(void)
{
	// Lock the drive (disallow read/writes to the disk LUN) if
	// the user has set a password.
	if (IS_USER_PASSWORD_SET(0))
	{
		// Throw away the password-protected data (which includes
		// the DEK), and then disable the AES function.
		ClearPwdData(0);
		*aes_control &= ~(AES_DECRYP_EN | AES_ENCRYP_EN);
		current_aes_key = AES_NONE;
		security_state[0] |= SS_LOCKED;
	}
	
	if (arrayMode == JBOD)
	{
		if (IS_USER_PASSWORD_SET(1))
		{
			ClearPwdData(1);
			current_aes_key2 = AES_NONE;
			security_state[1] |= SS_LOCKED;
		}
	}
}
// generate Random Number 
u32 generate_RandNum_DW(void)
{
	u32 tmp32 = 0;
	*aes_Seed_Ctrl |= AES_SEED_ENABLE;
	for (int i = 0; i < 16; i++) // generate 1 DW
	{
		// 
		tmp32 <<= 1;
		tmp32 |= *aes_Seed & 0x1; 
		tmp32 |= (*aes_Seed & 0x10) << 12; 
		Delayus(2);
	}

	DBG("RN %lx\n", tmp32);
	*aes_Seed_Ctrl &= ~AES_SEED_ENABLE;
	return tmp32;
}

/*************************************************\
*				enable_aes_key					 *
*       enable the aes key to read/write data from/to SATA *		
\*************************************************/
void enable_aes_key(u8 *key, u8 KeySet)
{
u8 i;
DBG("cipherID %bx\n", vital_data[KeySet].cipherID);

	u32 volatile * keySetCrl;
	u32 volatile * keyArray1;
	u32 volatile * keyArray2;

	if (KeySet == PRIVATE_KEYSET1)
	{
		keySetCrl = aes_Keyset1_ctrl;
		keyArray1 = aes_Keyset1_Key1_Array;
		keyArray2 = aes_Keyset1_Key2_Array;
	}
	else
	{
		keySetCrl = aes_Keyset2_ctrl;
		keyArray1 = aes_Keyset2_Key1_Array;
		keyArray2 = aes_Keyset2_Key2_Array;
	}
	*keySetCrl = AES_MODE_ECB;

	switch (vital_data[KeySet].cipherID)
	{
	case CIPHER_AES_256_XTS:
		*keySetCrl = AES_MODE_XTS;   //XTS 256
		for (i = 0; i < 8; i++, keyArray2++)
			*keyArray2 = *((u32*)key+8+i);	
		
	case CIPHER_AES_256_ECB:
		*keySetCrl |= AES_256_SEL;   //ECB, key 256, blocksize is 512
		for (i = 0; i < 8; i++, keyArray1++)
			*keyArray1 = *((u32*)key+i);
		break;

	case CIPHER_AES_128_XTS:
		*keySetCrl = AES_MODE_XTS;   //XTS 128
		for (i = 0; i < 4; i++, keyArray2++)
			*keyArray2 = *((u32*)key+8+i);	
	case CIPHER_AES_128_ECB:
		for (i = 0; i < 4; i++, keyArray1++)
			*keyArray1 = *((u32*)key+i);
		break;
		
	default:
		*keySetCrl &= ~AES_ENCRYP_EN;
		return;
	}
	
	// do key expantion here
	*keySetCrl |= AES_KEY1_AUTOGEN;
	Delay(10);
	*keySetCrl &= ~AES_KEY1_AUTOGEN;

	if ((vital_data[KeySet].cipherID == CIPHER_AES_256_XTS) || (vital_data[KeySet].cipherID == CIPHER_AES_128_XTS))
	{
		*keySetCrl |= AES_KEY2_AUTOGEN;
		Delay(10);
		*keySetCrl &= ~AES_KEY2_AUTOGEN;	
	}	
}

/****************************************\
	switch to password protected mode
	use the password as the aes key to protect 
	the data-encryption aes key
\****************************************/
void switch_to_pswProtMode(PCDB_CTXT pCtxt, u8 options)
{
DBG("switch_to_pswProtMode\n");
	u8 tmpBuf[MAX_PASSWORD_SIZE*2];
	u8 keySet;
	u8 slot_num = 0;

	keySet = options & PRIVATE_KEYSET_NUM;
	slot_num = keySet; // keyset 1 for slot 0 & keyset2 for slot 1	
	
	*aes_control &= ~AES_DECRYP_EN;
	*aes_control &= ~AES_ENCRYP_EN;

	xmemcpy((u8 *)PwdData.psw_disk[slot_num].user_psw, tmpBuf, MAX_PASSWORD_SIZE);

	if (keySet == PRIVATE_KEYSET2)
	{
		if (options & PRIVATE_READ_DATA)
		{
			*aes_control |= AES_DECRYP_SEL_KEYSET2;
		}
		else
		{
			*aes_control |= AES_ENCRYP_SEL_KEYSET2;
		}
	}
	
	if ((vital_data[slot_num].cipherID == CIPHER_AES_256_XTS) || (vital_data[slot_num].cipherID == CIPHER_AES_128_XTS))
	{
		xmemcpy((u8 *)PwdData.psw_disk[slot_num].user_psw, tmpBuf+32, MAX_PASSWORD_SIZE);
		// set up the IVECT for XTS
		u32 volatile * keySet_LBA0;
		u32 volatile * keySet_LBA1;	
		if (options & PRIVATE_READ_DATA)
		{	
			if (keySet == PRIVATE_KEYSET1)
			{
				keySet_LBA0 = aes_Keyset1_Dec_LBA0;
				keySet_LBA1 = aes_Keyset1_Dec_LBA1;
			}
			else
			{
				keySet_LBA0 = aes_Keyset2_Dec_LBA0;
				keySet_LBA1 = aes_Keyset2_Dec_LBA1;
			}
		}
		else
		{
			if (keySet == PRIVATE_KEYSET1)
			{
				keySet_LBA0 = aes_Keyset1_Enc_LBA0;
				keySet_LBA1 = aes_Keyset1_Enc_LBA1;
			}
			else
			{
				keySet_LBA0 = aes_Keyset2_Enc_LBA0;
				keySet_LBA1 = aes_Keyset2_Enc_LBA1;
			}			
		}
		*keySet_LBA0 = (pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT] << 24) + (pCtxt->CTXT_FIS[FIS_LBA_HIGH] << 16) + (pCtxt->CTXT_FIS[FIS_LBA_MID] << 8) + pCtxt->CTXT_FIS[FIS_LBA_LOW];
		*keySet_LBA1 = (pCtxt->CTXT_FIS[FIS_LBA_HIGH_EXT] << 8) + pCtxt->CTXT_FIS[FIS_LBA_MID_EXT];
		MSG("keySetL %lx %lx", *keySet_LBA0, *keySet_LBA1);
	}
	
	if (keySet == PRIVATE_KEYSET1)
	{
		current_aes_key = AES_PSW_PROTECT_MODE;	
	}
	else
	{
		current_aes_key2 = AES_PSW_PROTECT_MODE;	
	}

	enable_aes_key(tmpBuf, keySet);
	if (keySet == PRIVATE_KEYSET2)
	{
		*aes_control &= ~AES_DECRYP_SEL_KEYSET2;
	}
}

void switch_to_cryptMode(u8 KeySet)
{
	u8 slot_num = 0;
	*aes_control &= ~AES_DECRYP_EN;
	*aes_control &= ~AES_ENCRYP_EN;
	
	if(arrayMode == JBOD)
	{
		if(KeySet == PRIVATE_KEYSET2)
		{
			slot_num = 1;
			current_aes_key2 = AES_ENCRYPT_MODE;
			enable_aes_key((u8 *)PwdData.cipher_dek_disk[slot_num].DEK1, KeySet);
			return;
		}
	}
	current_aes_key = AES_ENCRYPT_MODE;
	enable_aes_key((u8 *)PwdData.cipher_dek_disk[slot_num].DEK0, KeySet);

}
/************************************************************************\
 ReadWritePrivatePage() - Reads or writes to the Firmware Private area.

 Prerequisites:
	- if writing, mc_buffer contains one private page's worth of data.

 Side effects:
	- the CDB will be clobbered.

 Arguments:
	log_addr if need read or write the log page,it need set 0x80~0x9f,if not log page,it is unusefull
	page_addr	the block # in the FW private area to read or write.
				log page addr must between 0x00~0x0f,every log just only 16 pages
	options		one or more PRIVATE_* flags

 Returns:
	INI_RC_OK  if no errors occured.
	INI_RC_IO_ERROR if an HDD error occurs and ATA status is memorized.

	If INI_RC_IO_ERROR is returned, caller must use ERROR_ATA_MEMORIZED
	instead of ERROR_ATA to recall the ATA error.
 */
u8 ReadWritePrivatePage(u8 log_addr, u8 page_addr, u8 options)
{
	u8 rc = INI_RC_OK;
	CDB_CTXT ctxt;

	DBG("RW PriPg %bx, options %bx\n", page_addr, options);
//	dbg_next_cmd = 1;
	if (options & PRIVATE_IN_SATA1)
	{
		pSataObj = &sata1Obj;
	}
	else
	{
		pSataObj = &sata0Obj;
	}
	// if (pSataObj->sobj_device_state != SATA_DEV_READY)  // for HARD_RAID, it may SATA_DEV_UNKNOWN when reenum.
	if ((pSataObj->sobj_State < SATA_PHY_RDY) || (pSataObj->sobj_State > SATA_READY))
	{
		return INI_RC_IO_ERROR;
	}
	
	ctxt.CTXT_FLAG = CTXT_FLAG_B2S | CTXT_FLAG_SYNC;
	ctxt.CTXT_TimeOutTick = WDC_SATA_TIME_OUT;
	ctxt.CTXT_ccm_cmdinten = 0;
	ctxt.CTXT_XFRLENGTH = 512;
	ctxt.CTXT_FIS[FIS_TYPE] = 0x27;						// FIS 27
	ctxt.CTXT_FIS[FIS_C]	= 0x80;						// command bit valid
	ctxt.CTXT_FIS[FIS_CONTROL]	= 0x00;	
	if ((options & PRIVATE_RW_MASK) == PRIVATE_WRITE_DATA)
	{
		ctxt.CTXT_CONTROL = 0;			// Data-Out	
		if (options & PRIVATE_ENCRYPT_MASK)
		{
			ctxt.CTXT_FLAG |= CTXT_FLAG_AES;
			if (options & PRIVATE_IN_SATA1)
				ctxt.CTXT_DbufIndex = DBUF_SEG_B2S1W_AES;
			else
				ctxt.CTXT_DbufIndex = DBUF_SEG_B2S0W_AES;
		}
		else
		{
			if (options & PRIVATE_IN_SATA1)
				ctxt.CTXT_DbufIndex = DBUF_SEG_B2S1W;
			else
				ctxt.CTXT_DbufIndex = DBUF_SEG_B2S0W;
		}
		set_dbufSource(ctxt.CTXT_DbufIndex);
		write_data_by_cpu_port(ctxt.CTXT_XFRLENGTH, DIR_RX);
		Delay(10);
		set_dbufDone(ctxt.CTXT_DbufIndex);
	}
	else
	{
		ctxt.CTXT_CONTROL = CTXT_CTRL_DIR;			// Data-in
		// CPU R <-- SATA 0
		if (options & PRIVATE_ENCRYPT_MASK)
		{
			ctxt.CTXT_FLAG |= CTXT_FLAG_AES;
			if (options & PRIVATE_IN_SATA1)
				ctxt.CTXT_DbufIndex = DBUF_SEG_B2S1R_AES;
			else
				ctxt.CTXT_DbufIndex = DBUF_SEG_B2S0R_AES;
		}
		else
		{
			if (options & PRIVATE_IN_SATA1)
				ctxt.CTXT_DbufIndex = DBUF_SEG_B2S1R;
			else
				ctxt.CTXT_DbufIndex = DBUF_SEG_B2S0R;
		}
	}
	
	if (options & PRIVATE_LOG_BACKUP)
	{
		// Backup copies are stored in the host-specific SMART logs.

		// An out-of-bounds log page # is a firmware bug!
		// (There are only 32 host-specific log pages.)
		// This would normally warrant an assert-fail, but this will do.
		if (page_addr > 0x1f)  page_addr = 0x1f;
		
		// Read or write one host-specific SMART log sector.
		// First copies are stored in the host-specific SMART logs 0x80.
		// Second Backup copies are stored in the host-specific SMART logs 0x9f.
		// Read or write one host-specific SMART log page sector.

		ctxt.CTXT_FIS[FIS_DEVICE]	= MASTER;
		ctxt.CTXT_FIS[FIS_LBA_LOW]	= log_addr;//log address
		ctxt.CTXT_FIS[FIS_LBA_MID]	= page_addr;//Page # (7:0) 
		ctxt.CTXT_FIS[FIS_LBA_HIGH]	= 0x00;//Reserved
		*((u32 *) &(ctxt.CTXT_FIS[FIS_LBA_LOW_EXT]))	= 0; //Page # (15:8)we just use page address 0,1,2...1f,so set it to default 0
		*((u32 *) &(ctxt.CTXT_FIS[FIS_SEC_CNT]))	= 0x01;
		
		if ((options & PRIVATE_RW_MASK) == PRIVATE_WRITE_DATA)
		{
			ctxt.CTXT_Prot = PROT_PIOOUT;
			ctxt.CTXT_FIS[FIS_FEATURE]	= 0x00;//Reserved
			ctxt.CTXT_FIS[FIS_COMMAND]	= ATA6_WRITE_LOG_EXT;
		}
		else
		{
			ctxt.CTXT_Prot = PROT_PIOIN;
			ctxt.CTXT_FIS[FIS_FEATURE]	= 0x00;//Log Specific ,it is unnecessary  
			ctxt.CTXT_FIS[FIS_COMMAND]	= ATA6_READ_LOG_EXT;
		}
	}
	else	// not PRIVATE_LOG_BACKUP
	{
		// The primary copy is kept in the Firmware Private area and is
		// aligned on a 4K sector boundary. Use the CDB to do large LBA math.

	
		//not PRIVATE_LOG_BACKUP does not use the log_addr,does read write the smart log page
#ifdef WDC_RAID
		u64 tmp64 = page_addr << FW_PRIVATE_ALIGNMENT_LOG2;
		tmp64 += ((pSataObj->sectorLBA & 0xFFFFFFFFFFFFF800) -INI_VCD_SIZE);
#else
		u64 tmp64 = page_addr;
		tmp64 += ((pSataObj->sectorLBA & 0xFFFFFFFFFFFFF800) - log_addr);
#endif
		WRITE_U64_BE(cdb.rw16x.lba, tmp64);
		// Read or write one sector in the firmware private area.
		WRITE_U32_BE(cdb.rw16x.xfer, 1);

		// Load the ATA task-file regs with the LBA from the CDB.
		ata_load_fis_from_rw16Cdb(&ctxt, DMA_TYPE);

		if ((options & PRIVATE_RW_MASK) == PRIVATE_WRITE_DATA)
		{
			ctxt.CTXT_Prot = PROT_DMAOUT;
			ctxt.CTXT_FIS[FIS_COMMAND]	= ATA6_WRITE_DMA_EXT;
		}
		else
		{
			ctxt.CTXT_Prot = PROT_DMAIN;	
			ctxt.CTXT_FIS[FIS_COMMAND]	= ATA6_READ_DMA_EXT;
		}
	}
#if 1 //def DEBUG_AES_INTERNAL
	if (options & PRIVATE_ENCRYPT_MASK)
	{
		switch_to_pswProtMode(&ctxt, options);
		if ((options & PRIVATE_RW_MASK) == PRIVATE_READ_DATA)
		{
			if ((options & PRIVATE_KEYSET_NUM) == PRIVATE_KEYSET2)
				*aes_control |= AES_DECRYP_SEL_KEYSET2;
			*aes_control |= AES_DECRYP_EN;
		}
		else
		{
			if ((options & PRIVATE_KEYSET_NUM) == PRIVATE_KEYSET2)
				*aes_control |= AES_ENCRYP_SEL_KEYSET2;
			*aes_control |= AES_ENCRYP_EN;
		}
	}
#endif

	u32 sataFrameIntEn = pSataObj->pSata_Ch_Reg->sata_FrameIntEn;
	pSataObj->pSata_Ch_Reg->sata_FrameIntEn &= ~D2HFISIEN;
	rc = sata_exec_ctxt(pSataObj, &ctxt);	
	pSataObj->pSata_Ch_Reg->sata_FrameIntEn = sataFrameIntEn;
	
	DBG("rc %bx\n", rc);
	DBG("mc_buffer[400] : %bx, %bx \n", mc_buffer[400], mc_buffer[401]);

	if (rc == CTXT_STATUS_LOCAL_TIMEOUT)
	{
		if ((options & PRIVATE_RW_MASK) == PRIVATE_WRITE_DATA)
		{
			rst_dout(&ctxt);
		}
		else
		{
			rst_din(&ctxt);
		}		
	}
	else
	{
		pSataObj->sobj_State = SATA_READY;

		// Flush HDD cache if private data page was just written
		if ((options & PRIVATE_RW_MASK) == PRIVATE_WRITE_DATA)
		{
			ata_ExecNoDataCmd(pSataObj, ATA6_FLUSH_CACHE_EXT, 0, WDC_SATA_TIME_OUT);
		}
	}

	DBG("state:%x\n", rc);

	return rc;
}

#define COPY_FROM_VITALPARAMS_TO_PSWDATA	0
#define COPY_FROM_PSWDATA_TO_VITALPARAMS	1
void copy_DEK_between_VitalParams_pswdata(PVITAL_PARMS tmp_vital_data, CIPHER_DEK_INFO *temp_cipher_dek, u8 direction)
{
	if (direction == COPY_FROM_VITALPARAMS_TO_PSWDATA)
	{
		xmemcpy(&(tmp_vital_data->DEK_partA[0]), &(temp_cipher_dek->DEK0[0]), 12);
		xmemcpy(&(tmp_vital_data->DEK_partB[0]), &(temp_cipher_dek->DEK0[12]), 12);
		xmemcpy(&(tmp_vital_data->DEK_partC[0]), &(temp_cipher_dek->DEK0[24]), 8);
		
		xmemcpy(&(tmp_vital_data->DEK_partC[8]), &(temp_cipher_dek->DEK1[0]), 4);
		xmemcpy(&(tmp_vital_data->DEK_partD[0]), &(temp_cipher_dek->DEK1[4]), 12);
		xmemcpy(&(tmp_vital_data->DEK_partE[0]), &(temp_cipher_dek->DEK1[16]), 12);
		xmemcpy(&(tmp_vital_data->DEK_partF[0]), &(temp_cipher_dek->DEK1[28]), 4);	
	}
	else
	{
		xmemcpy(&(temp_cipher_dek->DEK0[0]), &(tmp_vital_data->DEK_partA[0]), 12);
		xmemcpy(&(temp_cipher_dek->DEK0[12]), &(tmp_vital_data->DEK_partB[0]), 12);
		xmemcpy(&(temp_cipher_dek->DEK0[24]), &(tmp_vital_data->DEK_partC[0]), 8);
		
		xmemcpy(&(temp_cipher_dek->DEK1[0]), &(tmp_vital_data->DEK_partC[8]), 4);
		xmemcpy(&(temp_cipher_dek->DEK1[4]), &(tmp_vital_data->DEK_partD[0]),  12);
		xmemcpy(&(temp_cipher_dek->DEK1[16]), &(tmp_vital_data->DEK_partE[0]), 12);
		xmemcpy(&(temp_cipher_dek->DEK1[28]), &(tmp_vital_data->DEK_partF[0]), 4);	
	}
}

/************************************************************************\
Parameter:
	sata_ch:
		0 is raid disk 0;
		1 is raid disk 1;
	page_addr:
		WD_RAID_PARAM_PAGE			= 0x00,
		WD_VITAL_PARAM_PAGE			= 0x01,
		WD_OTHER_PARAM_PAGE			= 0x02,
	rlog_addr:
		WD_LOG_ADDRESS				= 0x80,
		INI_DATA_LOG_ADDRESS			= 0x81,
		INI_UNUSE_LOG_ADDRESS			= 0x85,
		INI_UNIT_PARAMS_LOG_ADDR		= 0x9E,
		WD_BACKUP_LOG_ADDRESS		= 0x9F,
 Returns:
	0  if no errors occured.
	1  if an HDD or data error occurs.
 */
u8 Read_Disk_MetaData(u8 slotNum,u8 page_addr, u8 rlog_addr)
{
	u8 options = 0;
	u8 rc = INI_RC_OK;

	if (slotNum)
	{
		if (sata1Obj.sobj_device_state == SATA_NO_DEV) return INI_RC_NO_DEV;
		slotNum = 1;
	}
	else
	{
		if (sata0Obj.sobj_device_state == SATA_NO_DEV) return INI_RC_NO_DEV;
	}

	options |= PRIVATE_READ_DATA|PRIVATE_LOG_BACKUP;
	options |= (slotNum != 0) ? PRIVATE_IN_SATA1:0;
		
	//read log page
	rc = ReadWritePrivatePage(rlog_addr,page_addr, options);
	if (rc != 0)
	{	
		return rc; // invalid
	}

	if(page_addr == WD_RAID_PARAM_PAGE)
	{
		xmemcpy(mc_buffer, (u8 *)(&tmp_rp), sizeof(RAID_PARMS));
		//verify RAID PARAMS
		if ( tmp_rp.header.signature == WDC_RAID_PARAM_SIG)
		{
			if (verify_metadata_integrity((u8 *)(&tmp_rp)) == 0)	
			{
				//verify meta data ok,it is valid.
				//copy the data to relational ram_rp buffer for  use.
				//xmemcpy((u8 *)(&tmp_rp), (u8 *)&ram_rp[slotNum], sizeof(RAID_PARMS));
				return INI_RC_OK; // valid
			}
			else return INI_RC_CRC_ERROR;
		}
		else return INI_RC_DATA_NOT_SET;
	}
	else if(page_addr == WD_VITAL_PARAM_PAGE)
	{
		xmemcpy(mc_buffer, (u8 *)(&tmp_vital_data), sizeof(VITAL_PARMS));
		//verify VITAL PARAMS
		if (tmp_vital_data.header.signature == WDC_VITAL_PARAM_SIG)
		{	//verify DEK CRC and vital CRC
			if (verify_metadata_integrity((u8 *)(&tmp_vital_data)) == 0)	
			{
				DBG("tmp CipID %bx\n", tmp_vital_data.cipherID);
				//verify meta data ok,it is valid.
				//copy the data to relational vital_data buffer for  use.
				xmemcpy((u8 *)(&tmp_vital_data), (u8 *)&vital_data[slotNum], sizeof(VITAL_PARMS));
				vital_data[slotNum].lun_capacity = tmp_vital_data.lun_capacity;

				return INI_RC_OK; // valid
			}
			else return INI_RC_CRC_ERROR;
		} 
		else return INI_RC_DATA_NOT_SET;
	}
	else if(page_addr == WD_OTHER_PARAM_PAGE)
	{
		xmemcpy(mc_buffer, (u8 *)(&tmp_other_Params), sizeof(OTHER_PARMS));
		//verify Other PARAMS
		if (tmp_other_Params.header.signature == WDC_OTHER_PARAM_SIG)
		{
			if (verify_metadata_integrity((u8 *)(&tmp_other_Params)) == 0)	
			{
				//verify meta data ok,it is valid.
				// copy the data to relational ram_rp buffer for  use.
				xmemcpy((u8 *)(&tmp_other_Params), (u8 *)&other_Params[slotNum], sizeof(OTHER_PARMS));
				return INI_RC_OK; // valid
			}
			else return INI_RC_CRC_ERROR;
		}
		else return INI_RC_DATA_NOT_SET;
	}
	else
	{
		MSG("pg_addr err\n");
	}
	return INI_RC_OK;
}

/************************************************************************\
Parameter:
	sata_ch:
		0 is raid disk 0;
		1 is raid disk 1;
	page_addr:
		WD_RAID_PARAM_PAGE			= 0x00,
		WD_VITAL_PARAM_PAGE			= 0x01,
		WD_OTHER_PARAM_PAGE			= 0x02,
	save_option:
	only save raid params log page need care it's value
		#define FIRST_BACKUP	0x01
		#define SECOND_BACKUP	0x02

 Returns:
	0  if no errors occured.
	1  if an HDD or data error occurs.
 */
 u8 Write_Disk_MetaData(u8 slotNum,u8 page_addr,u8 save_option)
{
	VITAL_PARMS tmp1_vital_data;
	u8 options = 0;
	u8 wlog_addr = WD_LOG_ADDRESS;
	u8 rc = 0;

	if (slotNum)
	{
		if (sata1Obj.sobj_device_state == SATA_NO_DEV) return INI_RC_NO_DEV;
		slotNum = 1;
	}
	else
	{
		if (sata0Obj.sobj_device_state == SATA_NO_DEV) return INI_RC_NO_DEV;
	}
	//fill data to mc_buffer
	if(page_addr == WD_RAID_PARAM_PAGE)
	{
		//fill raid_data to mc_buffer
		xmemcpy((u8 *)&ram_rp[slotNum],(u8 *)(&tmp_rp), sizeof(RAID_PARMS));
		//verify RAID PARAMS
		if (tmp_rp.header.signature == WDC_RAID_PARAM_SIG)
		{
			//create TOTAL PAYLOAD CRC
			tmp_rp.header.payload_crc = create_Crc16((u8 *)(&(tmp_rp.vcd_size)), tmp_rp.header.payload_len);
			//create  HEADER CRC
			tmp_rp.header.header_crc = create_Crc16((u8 *)(&(tmp_rp.header.signature)),10);
			xmemcpy((u8 *)(&tmp_rp),mc_buffer, sizeof(RAID_PARMS));
		}
		else
		{
			MSG("rd_param err\n");
		}
	}
	else if(page_addr == WD_VITAL_PARAM_PAGE)
	{
		//fill vital_data to mc_buffer
		xmemcpy((u8 *)&vital_data[slotNum],(u8 *)(&tmp1_vital_data), sizeof(VITAL_PARMS));
		//verify VITAL PARAMS and create CRC
		if (tmp1_vital_data.header.signature == WDC_VITAL_PARAM_SIG)
		{	
			//create DEK CRC 
			if(tmp1_vital_data.DEK_signature == WDC_IDEK_SIG)
			{
				tmp1_vital_data.DEK_CRC = create_Crc16((u8 *)(&(tmp1_vital_data.reserved2[0])), WDEK_CRC_LENGTH);
				vital_data[slotNum].DEK_CRC = tmp1_vital_data.DEK_CRC;
			}
			else
				return INI_RC_BLANK_DATA;

			xmemcpy((u8 *)(&tmp1_vital_data),mc_buffer, sizeof(VITAL_PARMS));
			MSG("sav vtdata %x\n", tmp1_vital_data.DEK_CRC);
		}
		else
		{
			MSG("vt_param err\n");
		}
	}
	else if(page_addr == WD_OTHER_PARAM_PAGE)
	{
		//fill other params to mc_buffer
		xmemcpy((u8 *)&other_Params[slotNum],(u8 *)(&tmp_other_Params), sizeof(OTHER_PARMS));
		//verify OTHER PARAMS
		if (tmp_other_Params.header.signature == WDC_OTHER_PARAM_SIG)
		{
			//create TOTAL PAYLOAD CRC
			tmp_other_Params.header.payload_crc = create_Crc16((u8 *)(&(tmp_other_Params.raid_rebuildCheckPoint)), tmp_other_Params.header.payload_len);
			//create  HEADER CRC
			tmp_other_Params.header.header_crc = create_Crc16((u8 *)(&(tmp_other_Params.header.signature)),10);
			xmemcpy((u8 *)(&tmp_other_Params),mc_buffer, sizeof(OTHER_PARMS));
		}
		else
		{
			MSG("oth_param err\n");
		}
	}
	
	//Read the vital log page data with encryption.
	if(page_addr == WD_VITAL_PARAM_PAGE)
	{
		options = (slotNum != 0) ? (PRIVATE_IN_SATA1|PRIVATE_KEYSET2):(0 |PRIVATE_KEYSET1);
		options |= PRIVATE_WRITE_DATA|PRIVATE_LOG_BACKUP|PRIVATE_ENCRYPT_MASK;
	}
	else
	{
		options = (slotNum != 0) ? PRIVATE_IN_SATA1:0;
		options |= PRIVATE_WRITE_DATA|PRIVATE_LOG_BACKUP;
	}
	//write log 0x80
	if((page_addr == WD_VITAL_PARAM_PAGE) ||
		(page_addr == WD_OTHER_PARAM_PAGE) ||
		((save_option & FIRST_BACKUP) && (page_addr == WD_RAID_PARAM_PAGE)))
	{
		rc = ReadWritePrivatePage(wlog_addr, page_addr, options);
		if (rc == 0)
		{	//write  log page data ok
			if (page_addr == WD_VITAL_PARAM_PAGE)
			{	//write encrypt vital data ok
				//then need read out to fill header data without encrypt
				options = (options & ~(PRIVATE_ENCRYPT_MASK | PRIVATE_RW_MASK)) | PRIVATE_READ_DATA;
				rc = ReadWritePrivatePage(WD_LOG_ADDRESS,WD_VITAL_PARAM_PAGE,options);
				if(rc == INI_RC_OK)
				{
					// so, from header to reserved2 and capacity are plaintext.
					xmemcpy((u8 *)(&tmp1_vital_data),mc_buffer, 16);
					((PVITAL_PARMS)mc_buffer)->lun_capacity = tmp1_vital_data.lun_capacity;
					
					//recreate TOTAL PAYLOAD CRC
					((PVITAL_PARMS)mc_buffer)->header.payload_crc = create_Crc16((u8 *)(&(((PVITAL_PARMS)mc_buffer)->user_bs_log2m9)), ((PVITAL_PARMS)mc_buffer)->header.payload_len);
					//recreate  HEADER CRC
					((PVITAL_PARMS)mc_buffer)->header.header_crc = create_Crc16((u8 *)(&(((PVITAL_PARMS)mc_buffer)->header.signature)),10);
					// write vital data again without encryption
					options = (options & ~(PRIVATE_ENCRYPT_MASK | PRIVATE_RW_MASK));
					rc = ReadWritePrivatePage(wlog_addr, page_addr, (options & ~PRIVATE_ENCRYPT_MASK));
					if (rc)
						return rc;
				}
				else
				{
					return rc;
				}

			}
		}
		else
		{
			return rc;
		}
	}
	
	//write log 0x9f
	if((page_addr == WD_VITAL_PARAM_PAGE) ||
		(page_addr == WD_OTHER_PARAM_PAGE) ||
		((save_option & SECOND_BACKUP) && (page_addr == WD_RAID_PARAM_PAGE)))
	{
		wlog_addr = WD_BACKUP_LOG_ADDRESS;
		// for vital params' first copy, following the squence that write DEK with encryption, and read back and change the head and 
		// second copy, save data to drive directly with same data of first copy
		// but the option shall be modified because the option is changed when do the first copy
		if (page_addr == WD_VITAL_PARAM_PAGE)
		{
			options = (options & ~(PRIVATE_ENCRYPT_MASK | PRIVATE_RW_MASK));
		}
		
		rc = ReadWritePrivatePage(wlog_addr, page_addr, options);
		if(rc)
		{
			return rc;
		}
	}

	return 0; 
}




u8 Read_Vital_DEK(u8 slot_num,u8 backup_flg)
{
	u8 rc;
	u8 temp_size = sizeof(tmp_vital_data);
	u8 options = (slot_num != 0) ? (PRIVATE_IN_SATA1|PRIVATE_KEYSET2):(0 |PRIVATE_KEYSET1);
	options |= PRIVATE_READ_DATA|PRIVATE_LOG_BACKUP|PRIVATE_ENCRYPT_MASK;
	
	if(slot_num)
		slot_num = 1;
	if(backup_flg == FIRST_BACKUP)
	{
		//first check vital data signature and crc
		rc = Read_Disk_MetaData(slot_num,WD_VITAL_PARAM_PAGE,WD_LOG_ADDRESS);
		if(rc) return rc;	
		//read dek data
		rc = ReadWritePrivatePage(WD_LOG_ADDRESS,WD_VITAL_PARAM_PAGE,options);
	}
	else
	{
		//first check vital data signature and crc
		rc = Read_Disk_MetaData(slot_num,WD_VITAL_PARAM_PAGE,WD_BACKUP_LOG_ADDRESS);
		if(rc) return rc;	
		//read dek data
		rc = ReadWritePrivatePage(WD_BACKUP_LOG_ADDRESS,WD_VITAL_PARAM_PAGE,options);		
	}
	if(rc) return INI_RC_IO_ERROR;	
	
	xmemcpy((u8 *)(&(((PVITAL_PARMS)mc_buffer)->DEK_signature)), (u8 *)(&tmp_vital_data.DEK_signature), 96);
	//verify VITAL PARAMS
	DBG("dek sig %lx %bx\n", tmp_vital_data.DEK_signature, slot_num);
	if (tmp_vital_data.DEK_signature == WDC_IDEK_SIG)
	{	//verify DEK CRC and vital CRC
		if(verify_Crc16((u8 *)(&tmp_vital_data.reserved2[0]), WDEK_CRC_LENGTH, tmp_vital_data.DEK_CRC) == 0)
		{
			//verify meta data ok,it is valid.
			//copy the data to relational vital_data buffer for  use.
			xmemcpy((u8 *)(&tmp_vital_data), (u8 *)&vital_data[slot_num], sizeof(VITAL_PARMS));

			copy_DEK_between_VitalParams_pswdata(&tmp_vital_data, &PwdData.cipher_dek_disk[slot_num], COPY_FROM_VITALPARAMS_TO_PSWDATA);
#ifndef DATASTORE_LED
			if ((arrayMode == RAID0_STRIPING_DEV)
#ifdef SUPPORT_RAID1
                    || (arrayMode == RAID1_MIRROR)
#endif
#ifdef SUPPORT_SPAN
					|| (arrayMode == SPANNING)
#endif
				)
			{
				if ((slot_num == 1) && (vital_data[0].cipherID == CIPHER_NOT_CONFIGURED))
				{
					xmemcpy((u8 *)&PwdData.cipher_dek_disk[1], (u8 *)&PwdData.cipher_dek_disk[0], 64);
				}
			}
#endif
			return INI_RC_OK; // valid
		}
		MSG("Dek CRC Err\n");
	}
	return INI_RC_WRONG_PASSWORD; // fail
}

void fill_vitalParam(u8 slot_num)
{
	// first fill all the vital_data[0] with random number,96/4 = 24;
	for (u32 i=0; i < 24; i+= 4)
	{
		*((u32 *)(&vital_data[slot_num].DEK_signature) + i) = generate_RandNum_DW(); 
	}
	// fill the really data at some off the byte
	vital_data[slot_num].header.signature = WDC_VITAL_PARAM_SIG;
	vital_data[slot_num].header.payload_len = sizeof(VITAL_PARMS) - sizeof(METADATA_HEADER);
	if (slot_num == 1)
		vital_data[slot_num].lun_capacity = (sata1Obj.sectorLBA & 0xFFFFFFFFFFFFF800);
	else
		vital_data[slot_num].lun_capacity = (sata0Obj.sectorLBA & 0xFFFFFFFFFFFFF800);

	if ((product_detail.options & PO_NO_CDROM) == 0)
	{
		vital_data[slot_num].lun_capacity -= INI_VCD_SIZE;
	}
	
	vital_data[slot_num].reserved[0] = 0x00;
	vital_data[slot_num].reserved[1] = 0x00;
	vital_data[slot_num].DEK_signature = WDC_IDEK_SIG;
	vital_data[slot_num].reserved2[0] = 0x00;
	vital_data[slot_num].reserved2[1] = 0x00;
	copy_DEK_between_VitalParams_pswdata(&vital_data[slot_num], &PwdData.cipher_dek_disk[slot_num], COPY_FROM_PSWDATA_TO_VITALPARAMS);
}

//  just for case ff xx OR ff 00 
u8 sync_vital_data(u8 tar_slot)
{
	u8 rc = INI_RC_OK;
	u8 src_slot = (tar_slot == 0) ? 1 : 0;

	// 1st, sync vital data for use
	xmemcpy((u8 *)&vital_data[src_slot], (u8 *)&vital_data[tar_slot], sizeof(VITAL_PARMS)-8);

	// 2nd re-calc CRC & save it: DEK_CRC will be same, payload_crc & header_crc wil be different.
	MSG("sec %bx %bx\n", security_state[0], security_state[1]);
	if (vital_data[src_slot].cipherID != CIPHER_NONE )
	{	// for AES
		if (!IS_LOCKED(src_slot))
		{
			MSG("sync vt %bx\n", tar_slot);

			if (IS_USER_PASSWORD_SET(src_slot))
			{
				xmemcpy( PwdData.psw_disk[src_slot].user_psw,  PwdData.psw_disk[tar_slot].user_psw, MAX_PASSWORD_SIZE);
			}
			else	
			{
				ini_fill_default_password(tar_slot, PwdData.psw_disk[tar_slot].user_psw);
			}
			rc = Write_Disk_MetaData(tar_slot, WD_VITAL_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP);

			MSG("src %bx %lx %x\n", src_slot, vital_data[src_slot].DEK_signature, vital_data[src_slot].DEK_CRC);
			MSG("tar %bx %lx %x\n", tar_slot, vital_data[tar_slot].DEK_signature, vital_data[tar_slot].DEK_CRC);
			need_to_sync_vital = 0xFF;		
		}
		else
		{
			MSG("unSync %bx\n", tar_slot);
			need_to_sync_vital = tar_slot;
		}
	}	

	security_state[tar_slot] = security_state[src_slot];
	return rc;	
}

// add a parameter name as priority to sync the vital data in case that both the disk has the valid vital data
// For instance, drive A & drive B are belonged to Array 1(RAID1), and drive C & drive D are belonged to Array 2 (RAID0).
// User decides to use drive A and drive C to create a new Array 3 in RAID 1 mode, and want to keep the data of drive A.
// then firmware will sync the vital data from drive A to drive C
// the firmware V1.005 does not enable it, it might be used in future
void check_disks_vital_data(u8 priority)
{
	MSG("chk vt %bx %bx\n", vital_data[0].cipherID, vital_data[1].cipherID);
	
	if (!((vital_data[0].cipherID == CIPHER_NOT_CONFIGURED) &&
		(vital_data[1].cipherID == CIPHER_NOT_CONFIGURED)))
	{	// ff xx OR xx xx OR 00 00 OR ff 00
		if (!((vital_data[0].cipherID == CIPHER_NONE) && 
			(vital_data[1].cipherID == CIPHER_NONE)))
		{  // ff xx OR xx xx OR ff 00
			if ((vital_data[0].cipherID == CIPHER_NOT_CONFIGURED) ||
				(vital_data[1].cipherID == CIPHER_NOT_CONFIGURED))
			{	 // ff xx OR ff 00
				if (vital_data[0].cipherID != CIPHER_NOT_CONFIGURED)  // there is a cipher in src
				{
					//sync and save dek data
					sync_vital_data(1);
				}
				else
				{
					//sync and save dek data
					sync_vital_data(0);
				}
			}
			else if (priority)
			{
				if (priority == SYNC_VITAL_DATA_DISK0_HIGH_PRIORITY)
				{
					sync_vital_data(1);
				}
				else
				{
					sync_vital_data(0);
				}
			}
			// else xx xx recheck whether they are same vita data
			else if ((vital_data[0].DEK_signature != vital_data[1].DEK_signature)
				|| xmemcmp(vital_data[0].DEK_partA, vital_data[1].DEK_partA, 12)
				|| xmemcmp(vital_data[0].DEK_partB, vital_data[1].DEK_partB, 12)
				|| xmemcmp(vital_data[0].DEK_partC, vital_data[1].DEK_partC, 12)
				|| xmemcmp(vital_data[0].DEK_partD, vital_data[1].DEK_partD, 12)
				|| xmemcmp(vital_data[0].DEK_partE, vital_data[1].DEK_partE, 12)
				|| xmemcmp(vital_data[0].DEK_partF, vital_data[1].DEK_partF, 4))
			{
				MSG("diff vt\n");
				DBG("sig %lx %lx\n", vital_data[0].DEK_signature, vital_data[1].DEK_signature);
				DBG("dek %bx %bx\n", vital_data[0].DEK_partA[0], vital_data[1].DEK_partA[0]);
				security_state[0] = SS_NOT_CONFIGURED;
				security_state[1] = SS_NOT_CONFIGURED;
			}
		}
		// else 00 00 do nothing
	}	
	// else FF FF do nothing
}	

void init_reset_default_nvm_unit_parms(void)
{
	// Set user parameters to defaults.
	//
	xmemset((u8 *)&nvm_unit_parms, 0, sizeof(nvm_unit_parms));

	nvm_unit_parms.header.signature = WDC_UNIT_PARAMS_SIG;
	nvm_unit_parms.header.payload_len = sizeof(mode_page_save_data);

	// The HDD standby timer is enabled by default.
	nvm_unit_parms.mode_save.standby_timer_enable = 1;
	nvm_unit_parms.mode_save.standby_time = WDC_DEFAULT_STANDBY_TIME;

	nvm_unit_parms.mode_save.cd_valid_cd_eject = OPS_CD_MEDIA_VALID;

	// Lights default to full brightness and black-on-white.
	nvm_unit_parms.mode_save.power_led_brightness = WDC_DEFAULT_LED_BRIGHTNESS;
	nvm_unit_parms.mode_save.diskLun0WrCacheEn = 1;
	nvm_unit_parms.mode_save.diskLun1WrCacheEn = 1;
}
/************************************************************************\
 ini_reset_defaults(u8 target) - Resets init data and user's parameters to defaults.

 This should only be called during power-on initialization. The current
 password data will be discarded.

 */
void ini_reset_defaults(u8 target)
{
	u8 options = product_detail.options;

	// Clear the password-protected data (no defaults)
	xmemset((u8 *)&PwdData, 0xee, sizeof (PwdData));

	// Set vital parameters to defaults.
	if(target & TARGET_SLOT0)
	{
		xmemset((u8 *)&vital_data[0], 0, sizeof(VITAL_PARMS));

		vital_data[0].header.signature = WDC_VITAL_PARAM_SIG;
		vital_data[0].header.payload_len = sizeof(VITAL_PARMS) - sizeof(METADATA_HEADER);
		if (options & PO_NO_ENCRYPTION)
		{
			vital_data[0].cipherID = CIPHER_NONE;
		}
		else
		{
			vital_data[0].cipherID = CIPHER_NOT_CONFIGURED;
		}
		if (options & PO_NO_CDROM)
		{
			vital_data[0].lun_capacity = (sata0Obj.sectorLBA & 0xFFFFFFFFFFFFF800);		
		}
		else
		{
			vital_data[0].lun_capacity = (sata0Obj.sectorLBA & 0xFFFFFFFFFFFFF800) - INI_VCD_SIZE;
		}
		vital_data[0].user_bs_log2m9 = sata0Obj.physicalBlockSize_log2;
	}
	
	if(target & TARGET_SLOT1)
	{
		xmemset((u8 *)&vital_data[1], 0, sizeof(VITAL_PARMS));

		vital_data[1].header.signature = WDC_VITAL_PARAM_SIG;
		vital_data[1].header.payload_len = sizeof(VITAL_PARMS) - sizeof(METADATA_HEADER);
		if (options & PO_NO_ENCRYPTION)
		{
			vital_data[1].cipherID = CIPHER_NONE;
		}
		else
		{
			vital_data[1].cipherID = CIPHER_NOT_CONFIGURED;
		}
		if (options & PO_NO_CDROM)
		{
			vital_data[1].lun_capacity = (sata1Obj.sectorLBA & 0xFFFFFFFFFFFFF800);
		}
		else
		{
			vital_data[1].lun_capacity = (sata1Obj.sectorLBA & 0xFFFFFFFFFFFFF800) - INI_VCD_SIZE;
		}
		vital_data[1].user_bs_log2m9 = sata1Obj.physicalBlockSize_log2;
	}
}

/************************************************************************\
 ini_fill_default_password() - Copies the Default Password to a buffer.

 Prerequisites:
	- Current Cipher ID is set

 Arguments:
	pw			pointer to data buffer in   space

 */
void ini_fill_default_password(u8 slot_num,u8   *pw)
{
//	for (u8 i = 0; i<2; i++)
	switch (vital_data[slot_num].cipherID)
	{
	case CIPHER_NONE:
		xmemset(pw, 0, CIPHER_NONE_PW_SIZE);
		break;

	case CIPHER_AES_256_ECB:
	case CIPHER_AES_256_XTS:
		xmemcpy((u8 *)default_password32, pw, 32);
		break;

	case CIPHER_AES_128_ECB:
	case CIPHER_AES_128_XTS:
		xmemcpy((u8 *)default_password16, pw, 16);
		break;

	// case CIPHER__NOT_CONFIGURED:
	default:
		// Should not get here - the current Cipher ID is invalid!
		xmemset(pw, 0, MAX_PASSWORD_SIZE);
	}
}

void ClearPwdData(u8 slot_num)
{
	xmemset((u8 *)&PwdData.cipher_dek_disk[slot_num], 0, sizeof(CIPHER_DEK_INFO));
	xmemset((u8 *)&PwdData.psw_disk[slot_num].user_psw, 0, sizeof(PASSWORD_DISK));
}



/************************************************************************\
 check_passworded_data() - Checks the whether the password-protected data
 in  mc_buffer  is valid; validity means the password is correct.

 This function cannot tell the difference between incorrectly decrypted
 data and data that was not initialized to begin with; the "wrong
 password" error is used for both cases

 Prerequisites:
	- mc_buffer contains password-protected data read from the HDD
	- Current Cipher ID is set
	- user_password is set

 Returns:
	INI_RC_OK  if data is valid, i.e., it was decrypted correctly.
	INI_RC_DATA_NOT_SET  if the data is invalid.
    INI_RC_WRONG_PASSWORD if data doesn't look right.
 */
u8 check_passworded_data(u8 slot_num)
{
VITAL_PARMS *VITAL_DATA= ((PVITAL_PARMS)(&mc_buffer[PWD_PROTECT_OFFSET]));


	if (VITAL_DATA->header.signature != WDC_VITAL_PARAM_SIG)
	{
MSG("sign wr\n");
		return  (VITAL_DATA->cipherID == CIPHER_NONE)?
		        INI_RC_DATA_NOT_SET : INI_RC_WRONG_PASSWORD;
	}

	// Verify the checksum.
	// The checksum covers everything except the header.
	if ( VITAL_DATA->header.payload_crc !=
	     Checksum16( (u8*)VITAL_DATA + sizeof(VITAL_DATA->header),
	                VITAL_DATA->header.payload_len) )
	{
MSG("chksum wr\n");
		return  (VITAL_DATA->cipherID == CIPHER_NONE)?
		        INI_RC_DATA_NOT_SET : INI_RC_WRONG_PASSWORD;
	}

	// The user's password must be checked explicitly in cleartext mode.
	if (VITAL_DATA->cipherID == CIPHER_NONE)
	{
		// 
		CIPHER_DEK_INFO *tmp_cipher_dek;
		copy_DEK_between_VitalParams_pswdata(VITAL_DATA, tmp_cipher_dek, COPY_FROM_VITALPARAMS_TO_PSWDATA);
		if (xmemcmp(&tmp_cipher_dek->DEK0[0],
		            user_password, CIPHER_NONE_PW_SIZE))
		{
			return INI_RC_WRONG_PASSWORD;
		}
	}

	return INI_RC_OK;
}

/************************************************************************\
 ini_save_passworded_data() - Save the password-protected data.

 Password-protected data includes the DEK, cipher ID, etc.
 The the password-protected data and Init data are stored together,
 so the Init data is also saved.

 Prerequisites:
	- PwdData is filled in except the checksum (e.g., the DEK is set)
	- Current Cipher ID is set
	- user_password is set, to either the user's password or Default PW.

 Returns:
	one of the INI_RC_* return codes.
 */
u8 ini_save_passworded_data(u8 target)
{ // need more effort for the random number generated by HW

	DBG("save psw data\n");
	u8 rc = INI_RC_OK;
	
	// Encrypt the password-protected data if data encryption is enabled.
	// save to slot0
	if (target & TARGET_SLOT0)
	{
		// save to 1st & 2nd log page
		rc = Write_Disk_MetaData(0,WD_VITAL_PARAM_PAGE, FIRST_BACKUP|SECOND_BACKUP);
		MSG("sl0 rc %bx\n", rc);
		if(rc != INI_RC_OK)
			return rc;	
	}
	//save to slot 1
	if (target & TARGET_SLOT1)
	{
		// save to 1st & 2nd log page
		rc = Write_Disk_MetaData(1,WD_VITAL_PARAM_PAGE, FIRST_BACKUP|SECOND_BACKUP);
		MSG("sl1 rc %bx\n", rc);
		if(rc != INI_RC_OK)
			return rc;
	}

#ifdef DBG_FUNCTION
	MSG("crc%lx %lx\n", vital_data[0].DEK_CRC, vital_data[1].DEK_CRC);
#endif	
	//if need  we also save it to flash
	return rc;
}

/************************************************************************\
 CheckIniData() - Checks the validity of the Init data in  mc_buffer

 Prerequisites:
	- Init data is in  mc_buffer

 Returns:
	INI_RC_OK if the Init data is valid.
	INI_RC_DATA_NOT_SET if the Init data is not valid.
 */
u8 CheckIniData(void)
{
	DBG("((VITAL_PARMS*)mc_buffer)->header.signature:%lx\n", ((PVITAL_PARMS)mc_buffer)->header.signature);
//DBG("((VITAL_PARAM*)mc_buffer)->signature2:%lx\n", ((VITAL_PARMS*)mc_buffer)->signature2);

	if ((INI_DATA_SIG != ((PVITAL_PARMS)mc_buffer)->header.signature))
	{
		return INI_RC_DATA_NOT_SET;
	}
	return INI_RC_OK;
}

/************************************************************************\
 CheckPassword() - Checks if a Password is correct.

 Prerequisites:
	- Cipher ID is set
	- user_password contains the password to check

 Returns:
	INI_RC_OK if the password is correct.
	INI_RC_IO_ERROR if cannot read the private page (HDD error).
	INI_RC_DATA_NOT_SET if the Init data is not valid.
	INI_RC_WRONG_PASSWORD if the password is incorrect
 */
u8 CheckPassword(u8 slot_num)
{
	u8 rc;
	DBG("check psw\n");
	if (need_to_sync_vital == 0)
		slot_num = 1;
	
	//use  Password read first copy dek
	rc = Read_Vital_DEK(slot_num, FIRST_BACKUP);
	
	//if read first copy dek command IO error,try read 2nd copy
	if (rc == INI_RC_IO_ERROR)
		rc = Read_Vital_DEK(slot_num, SECOND_BACKUP);

	return rc;
}

/************************************************************************\
 load_ini_Data() - Read Init data and the data encryption key (DEK)

 Arguments:
	which		0= use primary copy of Init data, else PRIVATE_LOG_BACKUP

 Returns:
	one of the INI_RC_* return codes.
 */
u8 load_ini_Data(u8 slot_num,u8 backup_flg)
{
	u8 rc;
	DBG("load_ini_Data, slot_num: %bx\n", slot_num);

	// Read the private page and use the Init data if it is valid.
	mc_buffer[0] = 0;			// Clear the buffer (a little bit of it)
	mc_buffer[1] = 0;
	
	if (backup_flg == FIRST_BACKUP)
		rc = Read_Disk_MetaData(slot_num,WD_VITAL_PARAM_PAGE, WD_LOG_ADDRESS);
	else
		rc = Read_Disk_MetaData(slot_num,WD_VITAL_PARAM_PAGE,WD_BACKUP_LOG_ADDRESS);

	DBG("rc: %bx\n", rc);
	if (rc == INI_RC_DATA_NOT_SET)
	{
		u8 nWords = 128;
		u32 *dp = (u32*)mc_buffer;
		
		// Instead of simply reporting there's no Init data, check if
		// the buffer is all zeros which indicates it might be a new HDD.
		do
		{
			if (*dp)  return rc;
			dp++;
			nWords--;
		} while (nWords);		// Checks 128 32-bit words.

		return INI_RC_BLANK_DATA;
	}
	else if (rc)
	{
		return rc;
	}
	
	DBG("ciph %bx\n", vital_data[slot_num].cipherID);
	//vital_data[slot_num].cipherID = CIPHER_AES_256_ECB; // debug purpose

	// Check if encryption is enabled and figure out the security state.
	switch (vital_data[slot_num].cipherID)
	{
	case CIPHER_NONE:
		// no encryption
		ini_fill_default_password(slot_num, user_password);

		switch (check_passworded_data(slot_num))
		{
		case INI_RC_OK:
			// The password is blank so the drive is unlocked and accessable.
			security_state[slot_num] &= ~(SS_NOT_CONFIGURED | SS_LOCKED |
			                    SS_USER_SET_PASSWORD | SS_ENCRYPTION_ENABLED);

			xmemcpy(&mc_buffer[PWD_PROTECT_OFFSET], (u8 *)&PwdData, sizeof(PwdData));
			break;

		case INI_RC_WRONG_PASSWORD:
			// The Default Password is wrong - the user has set their own!
			security_state[slot_num] &= ~(SS_NOT_CONFIGURED | SS_ENCRYPTION_ENABLED);
			security_state[slot_num] |=   SS_LOCKED | SS_USER_SET_PASSWORD;

			ClearPwdData(slot_num);
			break;

		default:
			// The password data is invalid; something needs to be configured.
			security_state[slot_num] = SS_NOT_CONFIGURED;
		}
		
		if ((product_detail.options & PO_NO_ENCRYPTION) == 0)
		{
			security_state[slot_num] = SS_NOT_CONFIGURED;
		}
		rc = INI_RC_OK;
		break;

	case CIPHER_AES_128_ECB:
	case CIPHER_AES_128_XTS:
	case CIPHER_AES_256_ECB:
	case CIPHER_AES_256_XTS:
		// Re-read the private page, but this time try to decrypt
		// it with the Default Password. If successful, then we have
		// the DEK and can allow access to the user-data area.
		ini_fill_default_password(slot_num, PwdData.psw_disk[slot_num].user_psw);
		
		rc = Read_Vital_DEK(slot_num, backup_flg);

		// Don't return an error if the data is "wrong" - it just means
		// the DEK is encrypted with the user's password.
		if (rc == INI_RC_IO_ERROR) 
			break;
		else if(rc == INI_RC_OK)
		{
			security_state[slot_num] &= ~(SS_NOT_CONFIGURED | SS_LOCKED | SS_USER_SET_PASSWORD);
			security_state[slot_num] |=   SS_ENCRYPTION_ENABLED;
		}
		else	// the DEK is wrapped by user's password
		{
			security_state[slot_num] &= ~SS_NOT_CONFIGURED;
			security_state[slot_num] |=  SS_ENCRYPTION_ENABLED | SS_LOCKED | SS_USER_SET_PASSWORD;		
			ClearPwdData(slot_num);
		}
		// inprove the code to protect the data in HDD when attach the unencrypted product.
		if (product_detail.options & PO_NO_ENCRYPTION)
		{
			DBG("unencrypted prod\n");
			security_state[slot_num] = SS_NOT_CONFIGURED;
		}

		rc = INI_RC_OK;
		break;

	// case CIPHER__NOT_CONFIGURED:
	default:	// Not Configured, invalid or unsupported cipher.
//		security_state = SS_NOT_CONFIGURED;
		rc = INI_RC_OK;
	}

	return rc;
}

/************************************************************************\
 init_new_hdd() - Product-specific initialization of a new HDD.

 This should only be called by  ini_security_init()  when it determines
 there is no Init data on the HDD.

 Prerequisites:
	Parameters have been reset to the firmware's default values.

 Returns:
	INI_RC_OK if the HDD was initialized, i.e., Init data saved.
	INI_RC_IO_ERROR if an I/O error occured.
	INI_RC_DATA_NOT_SET if nothing was initialized.
 */
u8 init_new_hdd(u8 slot_num)
{
u8 rc = INI_RC_OK;
	// Products that do not encrypt the user's data and default to
	// cleartext mode are usable immediately - no DEK required.
	if ( (vital_data[slot_num].cipherID == CIPHER_NONE)  &&
	     (product_detail.options & PO_NO_ENCRYPTION) )
	{
		// Do what RESET DEK does to set cleartext mode.
		ClearPwdData(slot_num);
		ini_fill_default_password(slot_num,(u8 *)(&PwdData.psw_disk[slot_num].user_psw[0]));

		//ClearUserPassword(slot_num);
		rc = ini_save_passworded_data(slot_num);
	}
	else
	{
		rc = INI_RC_DATA_NOT_SET;
	}
	return rc;
}

void init_security_Params(void)
{
	u8 primary_is_blank = 0;
	
	unlock_counter[0] = MAX_UNLOCK_ALLOWED;
	unlock_counter[1] = MAX_UNLOCK_ALLOWED;
	
	for (u8 i = 0; i < 2; i++)
	{
		if (i == 0)
		{
			if (sata0Obj.sobj_device_state == SATA_NO_DEV)
				continue;
		}
		else
		{
			if (sata1Obj.sobj_device_state == SATA_NO_DEV)
				continue;
		}
		
		// Load Init data and Data Encryption Key (DEK) from the 1st copy.
		switch (load_ini_Data(i, FIRST_BACKUP))
		{
		case INI_RC_OK:
			// The Init data was read successfully!
			break;
		case INI_RC_BLANK_DATA:
			// The sector is blank (all zeros); remember this and
			// check the backup copy before deciding what to do.
			primary_is_blank = 1;
			// ...fall thru...

		default:
			// The primary copy could not be read or is corrupt;
			// try the 2nd copy.
			switch (load_ini_Data(i, SECOND_BACKUP))
			{
			case INI_RC_OK:
				// The 2nd copy data is OK, so copy it back to the 1st copy.
				u8 options = (i != 0) ? (PRIVATE_IN_SATA1|PRIVATE_LOG_BACKUP):(0|PRIVATE_LOG_BACKUP);
				
				ReadWritePrivatePage(WD_BACKUP_LOG_ADDRESS,WD_VITAL_PARAM_PAGE, PRIVATE_READ_DATA |options);
				ReadWritePrivatePage(WD_BACKUP_LOG_ADDRESS,WD_VITAL_PARAM_PAGE, PRIVATE_WRITE_DATA|options);
				break;
			case INI_RC_BLANK_DATA:
				// The 2nd copy is blank; if the 1st copy was also blank,
				// then assume this is a new HDD. Some products are usable
				// with just the default settings - try it.
				if (primary_is_blank)
				{
					MSG("new save vital: %lx %lx\n", (u32)vital_data[0].lun_capacity, (u32)vital_data[1].lun_capacity);
					init_new_hdd(i);	// Product-specific initialization.
				}
				break;

			// Uh, oh: the backup copy didn't help, so wait for furthur instructions.
			default: // 
				break;
			}
		}
	}
#ifndef DATASTORE_LED
	if ((((arrayMode == RAID0_STRIPING_DEV)
#ifdef SUPPORT_RAID1
        || (arrayMode == RAID1_MIRROR)
#endif
#ifdef SUPPORT_SPAN
		|| (arrayMode == SPANNING)
#endif
		) && (metadata_status == MS_GOOD)))
	{
		check_disks_vital_data(SYNC_VITAL_DATA_SAME_PRIORITY);
	}
#endif
	if ((sata0Obj.sobj_device_state == SATA_NO_DEV) && (sata1Obj.sobj_device_state == SATA_DEV_READY))
	{
		security_state[0] = security_state[1];
	}
}
/************************************************************************\
 update_user_data_area_size() - Update the cached user-data area size.

 */
void update_user_data_area_size(void)
{
	// To improve performance, the disk logical unit's current capacity
	// and related info is cached in data-space variables.
	user_data_area_size[0]  = vital_data[0].lun_capacity;
	user_data_area_size[1]  = vital_data[1].lun_capacity;
}

// initialize and update the parameters
void init_update_params(void)
{
	// Reset everything to defaults, then use the parameters/settings
	// saved on the disk if those are valid.
	ini_reset_defaults(TARGET_BOTH_SLOTS);

	//init security params:vital params & security init
#ifdef INITIO_STANDARD_FW
#if 0 // manage it to see VCD all the time or not, part 2/2
	// when array mode is NOT_CONFIG, it still need VCD, 
	ram_rp[0].vcd_size = INI_VCD_SIZE;
	ram_rp[1].vcd_size = INI_VCD_SIZE;
#endif
	vital_data[0].cipherID = CIPHER_NONE;
	vital_data[1].cipherID = CIPHER_NONE;
	current_aes_key = AES_NONE;
	current_aes_key2 = AES_NONE;
	xmemset(PwdData.cipher_dek_disk[0].DEK0, 0, MAX_KEY_SIZE);
	xmemset(PwdData.cipher_dek_disk[0].DEK1, 0, MAX_KEY_SIZE);
	xmemset(PwdData.cipher_dek_disk[1].DEK0, 0, MAX_KEY_SIZE);
	xmemset(PwdData.cipher_dek_disk[1].DEK1, 0, MAX_KEY_SIZE);

	u64 tmp64 = INI_PRIVATE_SIZE;
	if ((product_detail.options & PO_NO_CDROM) == 0)   // there is CD
		tmp64 += INI_VCD_SIZE;
#ifdef DBG_CAP
	vital_data[0].lun_capacity = VIR_CAP; // 10G
	vital_data[1].lun_capacity = VIR_CAP; // 10G
#else
	vital_data[0].lun_capacity = (sata0Obj.sectorLBA & 0xFFFFFFFFFFFFF800) - tmp64;
	vital_data[1].lun_capacity = (sata1Obj.sectorLBA & 0xFFFFFFFFFFFFF800) - tmp64;
#endif
	security_state[0] = 0;
	security_state[1] = 0;
#ifdef AES_EVA
	vital_data[0].cipherID = CIPHER_AES_256_ECB;
	vital_data[1].cipherID = CIPHER_AES_256_ECB;
	current_aes_key = AES_NONE;
	current_aes_key2 = AES_NONE;
	security_state[0] |=   SS_ENCRYPTION_ENABLED;
	security_state[1] |=   SS_ENCRYPTION_ENABLED;
#endif
#else
	init_security_Params();
#endif
	MSG("ciperID %bx, %bx\n", vital_data[0].cipherID, vital_data[1].cipherID);
	MSG("secState %bx, %bx\n", security_state[0], security_state[1]); 
	
	update_user_data_area_size();
	update_array_data_area_size();
	
#ifdef HOST_4K
	vital_data[0].user_bs_log2m9 = 3;
	vital_data[1].user_bs_log2m9 = 3;
#else
	if (vital_data[0].user_bs_log2m9 >= 4)
	{
		vital_data[0].user_bs_log2m9 = 0;
	}
	if (vital_data[1].user_bs_log2m9 >= 4)
	{
		vital_data[1].user_bs_log2m9 = 0;
	}
#endif	
	MSG("cap0 %bx %lx\n", (u8)(vital_data[0].lun_capacity >> 32), (u32)(vital_data[0].lun_capacity));
	MSG("cap1 %bx %lx\n", (u8)(vital_data[1].lun_capacity >> 32), (u32)(vital_data[1].lun_capacity));
	MSG("arrC %bx %lx\n", (u8)(array_data_area_size >> 32), (u32)array_data_area_size);
	MSG("UserLog2 %bx, %bx\n", vital_data[0].user_bs_log2m9, vital_data[1].user_bs_log2m9);
	// Set the mode parameter block descriptors to saved values.
	update_pending_user_data();

	update_standby_timer();

#ifndef HDR2526
	if (nvm_unit_parms.mode_save.power_led_brightness != WDC_DEFAULT_LED_BRIGHTNESS)
	{
		// when fetch the LED brightness from HDD, and it's not the default value should turn on LED or turn off bases on this control bit
		lights_out(); 
		set_power_light(1);
	}
#endif

	if ((nvm_unit_parms.mode_save.cd_valid_cd_eject & OPS_CD_MEDIA_VALID) && ((product_detail.options & PO_EMPTY_DISC) == 0))
	{
		vcd_set_media_presence(1);
	}
	else
		cd_media_present = 0;
}
void initio_vendor_debug_clean_matadata()
{
	u8 options;
	spi_erase_flash_sector(WDC_QUICK_ENUM_DATA_ADDR);
	spi_erase_flash_sector(WDC_PRODUCT_INFO_ADDR);
	spi_erase_flash_sector(WDC_RAID_PARAM_ADDR);

	xmemset(mc_buffer,0x00,512);

	for(u8 i=0; i<2; i++)
	{
		options = (i != 0) ? (PRIVATE_IN_SATA1|PRIVATE_LOG_BACKUP):(0|PRIVATE_LOG_BACKUP);
		
		ReadWritePrivatePage(WD_LOG_ADDRESS,WD_RAID_PARAM_PAGE, PRIVATE_WRITE_DATA|options);
		ReadWritePrivatePage(WD_LOG_ADDRESS,WD_VITAL_PARAM_PAGE, PRIVATE_WRITE_DATA|options);
		ReadWritePrivatePage(WD_LOG_ADDRESS,WD_OTHER_PARAM_PAGE, PRIVATE_WRITE_DATA|options);
		ReadWritePrivatePage(WD_BACKUP_LOG_ADDRESS,WD_RAID_PARAM_PAGE, PRIVATE_WRITE_DATA|options);
		ReadWritePrivatePage(WD_BACKUP_LOG_ADDRESS,WD_VITAL_PARAM_PAGE, PRIVATE_WRITE_DATA|options);
		ReadWritePrivatePage(WD_BACKUP_LOG_ADDRESS,WD_OTHER_PARAM_PAGE, PRIVATE_WRITE_DATA|options);
	}
}
// EOF
