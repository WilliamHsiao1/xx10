/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *
 *
 *****************************************************************************/
 #define AES_C
#include	"general.h"
void cal_key_psw_len(u8 cipher_ID, u8 *buf)
{
	buf[0] = 0x20; // buf[0] for key len
	buf[1] = 0x10; // buf[1] for psw len

	
	switch (cipher_ID)
	{
	case CIPHER_AES_128_ECB:
		buf[0] = 0x10;
		break;
	case CIPHER_AES_128_XTS:
		break;

	case CIPHER_AES_256_ECB:
		buf[1] = 0x20;	
		break;
		
	case CIPHER_AES_256_XTS:
		buf[0] = 0x40;
		buf[1] = 0x20;
		break;

	// case CIPHER__NOT_CONFIGURED:
	default:
		// Should not get here; the Cipher ID is invalid or unsupported.
		buf[1] = 0;
	}
}

/************************************************************************\
 aes_security_cmd() - Command handler for UNLOCK, RESET DEK, etc.
 the caller is SES LUN or Disk LUN
 */
void  aes_security_cmd(PCDB_CTXT pCtxt)
{
u8 pwd_len = 0;
u8 key_len = 0;
u8 tmp[2];
u8 tempOldPW[MAX_PASSWORD_SIZE];
u8 tempNewPW[MAX_PASSWORD_SIZE];

u8 usingDefaults;
u16 tmp16 = 0;
u32 l_byteCnt = (pCtxt->CTXT_CDB[7] << 8) + pCtxt->CTXT_CDB[8];
u8 chk_result = 0;
u8 tar = 0;
u8 slot_num;

	// Encryption keys and passwords must be carefully controlled to
	// avoid accidental data leaks. This function must ensure keys and
	// passwords are sanitized before returning.

	// Determine the expected password length in bytes.
	u8 cipherID;
	
	if(scsi_wdc_param_check(TYPE_DESTLUN,pCtxt))
	{
		return;
	}

	if (arrayMode == JBOD)
	{
		if ((logicalUnit == HDD0_LUN) || (logicalUnit == HDD1_LUN))
		{
			slot_num = logicalUnit;
		}
		else
		{
			slot_num = cdb.byte[6] & MASK_CP_DESTLUN;
		}
		tar = slot_num + 1; //because TARGET_SLOT0 = 1;TARGET_SLOT1 = 2;
	}
	else  // so, we would save sth. to both slots when in raid, DestLun is 1 would be intercepted in scsi_wdc_param_check()
	{
		slot_num = 0;
		tar = TARGET_BOTH_SLOTS;
	}

	cipherID = vital_data[slot_num].cipherID;

	cal_key_psw_len(cipherID, tmp);
	pwd_len = tmp[1];
	MSG("cip %bx, %bx\n", cipherID, pwd_len);

	switch ( CmdBlk(1) /*Action*/ )			
	{
	case UNLOCK_ENCRYPTION:
		if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
		{
#if !WDC_SUPPORT_CIPHER_NONE_LOCK
			// This command is only supported if password-locking is
			// supported when data encryption is disabled.
			if (product_detail.options & PO_NO_ENCRYPTION) goto invalid_CDB_field;
#endif	// ! WDC_SUPPORT_CIPHER_NONE_LOCK

			// This command is only allowed when the drive is Locked...
			if (!IS_LOCKED(slot_num))  goto incorrect_security_state;

			// ...and the current cipher is otherwise valid...
			if (pwd_len == 0)  goto incorrect_security_state;

			// ...and not enough wrong passwords have been tried.
			if (!unlock_counter[slot_num])
			{
				hdd_err_2_sense_data(pCtxt, ERROR_UNLOCK_EXCEED);
				break;
			}

			if (l_byteCnt == 0)
			{// if the byte cnt is 0, it shall not take as error. firmware checks the phase case in the function usb_device_no_data
				nextDirection = DIRECTION_SEND_STATUS;
				return;
			}
			// Make sure parameter list is complete (password & header)
			if ( l_byteCnt < (pwd_len+8) )
			{
				goto invalid_CDB_field;
			}
		}
		if (scsi_chk_hdd_pwr(pCtxt)) 	return;	
		if (scsi_start_receive_data(pCtxt, l_byteCnt)) return;

		// Check signature
		if( mc_buffer[0] != 0x45)  goto invalid_param_field;

		//check password length
		tmp16 = (mc_buffer[6] << 8) + mc_buffer[7];
		if (pwd_len != tmp16) goto invalid_param_field;

		
		SetUserPassword(&mc_buffer[8], pwd_len, slot_num);
#ifndef DATASTORE_LED
		if ((arrayMode == RAID0_STRIPING_DEV)
#ifdef SUPPORT_RAID1
            || (arrayMode == RAID1_MIRROR)
#endif
#ifdef SUPPORT_SPAN
			|| (arrayMode == SPANNING)
#endif
			)
			SetUserPassword(&mc_buffer[8], pwd_len, 1);	// maybe the slot1 is valid disk.
#endif
		switch (CheckPassword(slot_num))
		{
		case INI_RC_OK:		// Success - password is correct
			//unlock success
			security_state[slot_num] &= ~SS_LOCKED;
			// Reset the unlock counter since correct password was given.
			unlock_counter[slot_num] = MAX_UNLOCK_ALLOWED;
			// re-enum to display security partition
			re_enum_flag = DOUBLE_ENUM;
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
				//unlock success
				security_state[1] &= ~SS_LOCKED;
				sync_vital_data(need_to_sync_vital);
			}
			break;
#endif
		case INI_RC_WRONG_PASSWORD:
			unlock_counter[slot_num]--;	
			goto wrong_password;
			
		case INI_RC_IO_ERROR:
			goto  io_error;
		default:
			// it is a command io error, so do not reduce the unlock_counter
			//unlock_counter[slot_num]--;
			goto unexpected_error;
		}
		break;

	case CHANGE_PASSWORD:
		if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
		{
#if !WDC_SUPPORT_CIPHER_NONE_LOCK
			// This command isn't supported when cleartext mode cannot be locked.
			if (product_detail.options & PO_NO_ENCRYPTION) goto invalid_CDB_field;
#endif

			// Not allowed when Locked (or Unlock Exceeded) or Not Configured,
			// or if the current cipher ID is somehow invalid.
			if ((IS_LOCKED_OR_UNCONFIGED(slot_num))  ||  !pwd_len)
			{
				goto incorrect_security_state;
			}

			if (l_byteCnt == 0)
			{// if the byte cnt is 0, it shall not take as error. firmware checks the phase case in the function usb_device_no_data
				nextDirection = DIRECTION_SEND_STATUS;
				return;
			}

			// Make sure parameter list is complete (password & header)
			if ( l_byteCnt < (pwd_len*2 +8) )
			{
				goto invalid_CDB_field;
			}
		}
		if (scsi_chk_hdd_pwr(pCtxt)) 	return;	
		if (scsi_start_receive_data(pCtxt, l_byteCnt)) return;

		// Check signature
		if (mc_buffer[0] != 0x45)  goto invalid_param_field;

		// Check password length
		tmp16 = (mc_buffer[6] << 8) + mc_buffer[7];
		if (pwd_len != tmp16) goto invalid_param_field;

		// Old- and New Default bits cannot both be set at same time
		if ((mc_buffer[3] & 0x11) == 0x11)  goto invalid_param_field;
		
		// Fill in Default Passwords if either the Old- or 
		// New Default bits are set.
		if (mc_buffer[3] & 0x01 /*Old Default bit*/)
		{
			ini_fill_default_password(slot_num,&mc_buffer[8]);
		}
		
		u8 remove_password = 0;
		if (mc_buffer[3] & 0x10 /*New Default bit*/)
		{
			ini_fill_default_password(slot_num,&mc_buffer[8 + pwd_len]);
		}
		// save unlock ok old password first ,or will be destroy
		xmemcpy((u8 *)PwdData.psw_disk[slot_num].user_psw, tempOldPW, MAX_PASSWORD_SIZE);
		
		//check old password first
		SetUserPassword(&mc_buffer[8], pwd_len,slot_num);
#ifndef DATASTORE_LED
		if ((arrayMode == RAID0_STRIPING_DEV)
#ifdef SUPPORT_RAID1
            || (arrayMode == RAID1_MIRROR)
#endif
#ifdef SUPPORT_SPAN
			|| (arrayMode == SPANNING)
#endif
			)
			SetUserPassword(&mc_buffer[8], pwd_len, 1);	// maybe the slot1 is valid disk.
#endif
		// save the input new password first or will be destroy
		xmemcpy(&mc_buffer[ pwd_len + 8], tempNewPW, sizeof(tempNewPW));
		usingDefaults = mc_buffer[3];
			
		switch (CheckPassword(slot_num))
		{
		case INI_RC_OK:		// Password is correct, can continue
			break;

		case INI_RC_WRONG_PASSWORD:
			//input old password is incorrect, copy old ok password to user password
			xmemcpy(tempOldPW,(u8 *)PwdData.psw_disk[slot_num].user_psw, MAX_PASSWORD_SIZE);
			goto wrong_password;

		case INI_RC_IO_ERROR:
			//input old password is incorrect, copy old ok password to user password
			xmemcpy(tempOldPW,(u8 *)PwdData.psw_disk[slot_num].user_psw, MAX_PASSWORD_SIZE);
			goto io_error;
			
		default:
			//input old password is incorrect, copy old ok password to user password
			xmemcpy(tempOldPW,(u8 *)PwdData.psw_disk[slot_num].user_psw, MAX_PASSWORD_SIZE);
			goto unexpected_error;
		}

		// The Old Password is correct, so change over to the New Password.

		if (vital_data[slot_num].cipherID == CIPHER_NONE)
		{
			ClearUserPassword(slot_num);
		}
		else
		{
			// Data encryption is enabled, so re-encrypt the password-
			// protected data (i.e., wrap the DEK) with the New Password.
			
			//if Array is not JBOD mode slot_num=0
			SetUserPassword(tempNewPW, pwd_len,slot_num); 	
			//if not JBOD, we need set slot 2 password at the same time to protect DEK
			if(arrayMode != JBOD)
				SetUserPassword(tempNewPW, pwd_len,1); 
		}

		MSG("change save vita: %lx %lx\n", (u32)vital_data[0].lun_capacity, (u32)vital_data[1].lun_capacity);
		switch (ini_save_passworded_data(tar))
		{
		case INI_RC_OK:		// Success!
			break;

		case INI_RC_IO_ERROR:
			//input old password is incorrect, copy old ok password to user password
			xmemcpy(tempOldPW,(u8 *)PwdData.psw_disk[slot_num].user_psw, MAX_PASSWORD_SIZE);
			goto io_error;
			
		case INI_RC_BLANK_DATA:
		default:
			//input old password is incorrect, copy old ok password to user password
			xmemcpy(tempOldPW,(u8 *)PwdData.psw_disk[slot_num].user_psw, MAX_PASSWORD_SIZE);
			goto unexpected_error;
		}
		// If the New Password is the Default PW (i.e., not a user
		// password), then it means the security is disabled and
		// therefore the Lock icon/LED should be turned off.
		if (usingDefaults & 0x10 /*New Default bit*/)
		{
			security_state[slot_num] &= ~SS_USER_SET_PASSWORD;
		}
		else
		{
			security_state[slot_num] |= SS_USER_SET_PASSWORD;
		}

		break;

	case RESET_DEK:				// Set the cipher and data encryption key (DEK)
		// This command is allowed even on products w/o data encryption.

		// Check the Key Reset Enabler. An Enabler of 0 means the host
		// has not issued an ENCRYPTION STATUS cmd since the last reset,
		// so this request is invalid.
		if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
		{
			u32 tmp32 = (CmdBlk(2) << 24) + (CmdBlk(3) << 16) + (CmdBlk(4) << 8) + CmdBlk(5);
			if (!dek_reset_enabler[slot_num] ||
			    (dek_reset_enabler[slot_num] != tmp32))
			{
				goto invalid_CDB_field;
			}

			// Make sure this key enabler isn't re-used.
			dek_reset_enabler[slot_num] = dek_reset_enabler[slot_num] ^ 0xff;

			
			if (l_byteCnt == 0)
			{// if the byte cnt is 0, it shall not take as error. firmware checks the phase case in the function usb_device_no_data
				nextDirection = DIRECTION_SEND_STATUS;
				return;
			}

			// Make sure the parameter list contains at least the header;
			// the full length will be checked later when the Cipher ID
			// field is validated.
			tmp16 = (CmdBlk(7) << 8) + CmdBlk(8);
			if (tmp16 < 8)  goto invalid_CDB_field;
		}
		if (scsi_chk_hdd_pwr(pCtxt)) 	return;	
		if (scsi_start_receive_data(pCtxt, l_byteCnt)) return;

		// Got the parameter list - now validate it.
		// Start by checking the Signature.
		if (mc_buffer[0] != 0x45)	goto invalid_param_field;

		// Verify the Cipher ID is supported and key length matches.
		// mc_buffer[1] is used to hold the key length in bytes.
		switch (mc_buffer[4])
		{
		case CIPHER_NONE:
			// Artemis's 1R product has no AES function
			if ((product_detail.options & PO_NO_ENCRYPTION) == 0)
				goto invalid_param_field;
			break;

		case CIPHER_AES_128_ECB:
		case CIPHER_AES_128_XTS:
		case CIPHER_AES_256_ECB:
		case CIPHER_AES_256_XTS:
			// Products w/o data encryption can't use this cipher, duh!
			if (product_detail.options & PO_NO_ENCRYPTION)
				goto invalid_param_field;

			
			cal_key_psw_len(mc_buffer[4], tmp);
			key_len = tmp[0];
			MSG("%bx\n", key_len);
			tmp16 = (mc_buffer[6] << 8) + mc_buffer[7];
			MSG("%x\n", tmp16);
			if (tmp16 != (key_len * 8))  goto invalid_param_field;
			// If the PARAMETER LIST LENGTH is less than the length of the parameters then invalid_CDB_field
			if (l_byteCnt < (key_len + 8)) goto invalid_CDB_field;
			//??? mc_buffer[1] = tmp16 >> 3;
			break;					

		default:
			// Unsupported or invalid cipher.
			goto invalid_param_field;
		}

		// Check COMBINE bit,If the COMBINE bit is set to one, 
		// generate a cryptographically-secure pseudo-random key and XOR it with the KEY field; 
		// the result is the new DEK. Otherwise use the KEY field directly as the new DEK. 
		if ( mc_buffer[3] & BIT_0)
		{
			for (u32 i=0; i < (key_len >> 2); i+= 4)
			{
				*((u32 *) (&mc_buffer[8+i])) ^= generate_RandNum_DW(); 
			}
		}

		//if not JBOD, we need set slot 2 password at the same time to protect slot2 DEK
		if(arrayMode != JBOD)
		{
			vital_data[0].cipherID = mc_buffer[4];
			vital_data[1].cipherID = mc_buffer[4];
			ClearPwdData(0);
			ClearPwdData(1);
			xmemcpy(&mc_buffer[8], (u8 *)&PwdData.cipher_dek_disk[0], sizeof(CIPHER_DEK_INFO));
			xmemcpy(&mc_buffer[8], (u8 *)&PwdData.cipher_dek_disk[1], sizeof(CIPHER_DEK_INFO));
		}
		else
		{
			vital_data[slot_num].cipherID = mc_buffer[4];
			ClearPwdData(slot_num);	
			xmemcpy(&mc_buffer[8],(u8 *)&PwdData.cipher_dek_disk[slot_num], sizeof(CIPHER_DEK_INFO));
		}
		
		//if cipherID is not CIPHER_NONE ,set default password to protect Dek	
		if (mc_buffer[4] != CIPHER_NONE)
		{
			if (arrayMode != JBOD)
			{
				ini_fill_default_password(0, PwdData.psw_disk[0].user_psw);
				ini_fill_default_password(1, PwdData.psw_disk[1].user_psw);
			}
			else
			{
				ini_fill_default_password(slot_num, PwdData.psw_disk[slot_num].user_psw);
			}
		}

		// fill vital data,(signature,DEK.etc..)
		fill_vitalParam(slot_num);
		if (arrayMode != JBOD)
		{
			xmemcpy((u8 *)&vital_data[0], (u8 *)&vital_data[1], sizeof(VITAL_PARMS)-8);
			vital_data[1].lun_capacity = (sata1Obj.sectorLBA & 0xFFFFFFFFFFFFF800) - INI_VCD_SIZE;
		}

		MSG("resDEK save vital: %lx %lx\n", (u32)vital_data[0].lun_capacity, (u32)vital_data[1].lun_capacity);

		// save dek with password protect
		u8 savPSW_result = ini_save_passworded_data(tar);
		MSG("sv rsl %bx\n", savPSW_result);
		switch ( savPSW_result )
		{
		case INI_RC_OK:		// Success!
			break;

		case INI_RC_IO_ERROR:
			goto io_error;

		default:
			goto unexpected_error;
		}

		// Reset the security state and force the DEK to be reloaded.
		security_state[slot_num] = (vital_data[slot_num].cipherID != CIPHER_NONE)? SS_ENCRYPTION_ENABLED : 0;

		unlock_counter[slot_num] = MAX_UNLOCK_ALLOWED;

		if (slot_num == 0)
			current_aes_key = AES_NONE;
		else
			current_aes_key2 = AES_NONE;
		break;

	default:	// Invalid Action
		goto invalid_CDB_field;
	}

cleanup:
	// Erase sensitive data to avoid leaks.
	xmemset(mc_buffer, 0x11, 520);
	xmemset(tempNewPW, 0, sizeof(tempNewPW));
	//ClearUserPassword(slot_num);
	return;

invalid_CDB_field:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
	goto cleanup;
#if 0
param_list_length_error:
	hdd_err_2_sense_data(pCtxt, ERROR_PARAM_LIST_LENGTH);
	goto cleanup;
#endif

invalid_param_field:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_PARAM);
	goto cleanup;

incorrect_security_state:
	hdd_err_2_sense_data(pCtxt, ERROR_INCORRECT_SECURITY_STATE);
	goto cleanup;

wrong_password:
	hdd_err_2_sense_data(pCtxt, ERROR_AUTHENTICATION_FAILED);
	goto cleanup;

io_error:
	hdd_err_2_sense_data(pCtxt, ERROR_ATA_MEMORIZED);
	goto cleanup;

unexpected_error:
	hdd_err_2_sense_data(pCtxt, ERROR_HW_FAIL_INTERNAL);
	goto cleanup;
}
/************************************************************************\
 aes_encryption_status_cmd() - ENCRYPTION STATUS command handler.
 the caller is SES LUN or Disk LUN
 */
void aes_encryption_status_cmd(PCDB_CTXT pCtxt)
{
	u8 slot_num;

	// Check the Signature field.
	if (CmdBlk(1) != 0x45)
	{
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
		return;
	}
	
	if(scsi_wdc_param_check(TYPE_DESTLUN,pCtxt))
	{
		return;
	}
	if ((logicalUnit == HDD0_LUN) || (logicalUnit == HDD1_LUN))
		slot_num = logicalUnit;
	else
		slot_num = cdb.byte[6] & MASK_CP_DESTLUN;
	

	xmemset(mc_buffer, 0, 128);			// Prefill buffer with 0x00

	mc_buffer[0] = 0x45; 				// Byte 0: Signature

	// Determine the appropriate Security State to report.
	if (security_state[slot_num] & SS_NOT_CONFIGURED)
	{
		mc_buffer[3] = ESSS_NOT_CONFIGURED;
	}
	else if (!unlock_counter[slot_num])
	{
		mc_buffer[3] = ESSS_UNLOCKS_EXCEEDED;
	}
	else if (IS_LOCKED(slot_num))
	{
		mc_buffer[3] = ESSS_LOCKED;
	}
	else if (IS_USER_PASSWORD_SET(slot_num))
	{
		mc_buffer[3] = ESSS_UNLOCKED;
	} 
	else
	{
		mc_buffer[3] = ESSS_SECURITY_OFF;
	}

	// The Cipher ID and Password Length are meaningful only if a DEK is set.
	if (!(security_state[slot_num] & SS_NOT_CONFIGURED))
	{
		mc_buffer[4] = vital_data[slot_num].cipherID; 

		// Fill in Password Length field.
		switch (vital_data[slot_num].cipherID)		

		{
		case CIPHER_AES_128_ECB:
		case CIPHER_AES_128_XTS:
			// Bytes 6..7 is the Password Length; the MSB is already 0.
			mc_buffer[7] = 16;
			break;

		case CIPHER_AES_256_ECB:
		case CIPHER_AES_256_XTS:
			mc_buffer[7] = 0x20;
			break;

		case CIPHER_NONE:
#if WDC_SUPPORT_CIPHER_NONE_LOCK
			mc_buffer[7] = CIPHER_NONE_PW_SIZE;
#else
			// Password not supported; the PW Length field is already 0.
#endif
			break;
		}
	}

	// Generate a new Key Reset Enabler.
	dek_reset_enabler[slot_num] += 0xB82153CE + generate_RandNum_DW();
	dek_reset_enabler[slot_num] |= 0x01;	// Make sure it's non-zero

	WRITE_U32_BE(&mc_buffer[8], dek_reset_enabler[slot_num]);

	// Fill in the Supported Cipher list.
	if (product_detail.options & PO_NO_ENCRYPTION)
	{
		mc_buffer[15] = 1;		// Byte 15: Number of Ciphers
		mc_buffer[16] = CIPHER_NONE;
		byteCnt = 17;
	}
	else
	{
#if WDC_SUPPORT_CIPHER_NONE
		mc_buffer[15] = 3;		// Byte 15: Number of Ciphers
		mc_buffer[16] = CIPHER_AES_128_ECB;
		mc_buffer[17] = CIPHER_NONE;
		mc_buffer[18] = CIPHER_AES_256_ECB;
		byteCnt = 19;
#else
		mc_buffer[15] = 4;		// Byte 15: Number of Ciphers
		mc_buffer[16] = CIPHER_AES_128_ECB;
		mc_buffer[17] = CIPHER_AES_256_ECB;
		mc_buffer[18] = CIPHER_AES_128_XTS;
		mc_buffer[19] = CIPHER_AES_256_XTS;
		byteCnt = 20;
#endif	// WDC_SUPPORT_CIPHER_NONE
	}
	nextDirection = DIRECTION_SEND_DATA;
	alloc_len = (CmdBlk(7) << 8) + CmdBlk(8);
}
