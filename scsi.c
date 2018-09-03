/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/
#define SCSI_C
#include	"general.h"

extern void raid_rebuild_initiated(u8 target);

static u8 pending_user_bs_log2m9[MAX_SATA_CH_NUM];
static u64 pending_user_data_size[MAX_SATA_CH_NUM];
static u8 pending_user_array_bs_log2m9;
static u64 pending_user_array_size;
static u8 * mode_page_ptr;

/****************************************\
	Ata_return_descriptor_sense
\****************************************/
#if 1
void Ata_return_descriptor_sense(PCDB_CTXT	pCtxt, u8 *buffer)
{
	buffer[0] = 0x09;	// ATA return
	buffer[1] = 12;		// ADDITIONAL LENGTH (n-1)
	buffer[2] = 0x00;	//

	buffer[3] = pCtxt->CTXT_FIS[FIS_ERROR];	// ERROR

	buffer[4] = 0x00;
	buffer[5] = pCtxt->CTXT_FIS[FIS_SEC_CNT];	// ATA Sector Count

	buffer[6] = 0x00;
    	buffer[7] = pCtxt->CTXT_FIS[FIS_LBA_LOW];	// ATA LBA(7:0))

	buffer[8] = 0x00;
	buffer[9] = pCtxt->CTXT_FIS[FIS_LBA_MID];	// ATA LBA(15:8))

	buffer[10] = 0x00;
    	buffer[11] = pCtxt->CTXT_FIS[FIS_LBA_HIGH];	// ATA LBA(23:16))

    	buffer[12] = pCtxt->CTXT_FIS[FIS_DEVICE];	// device
    	buffer[13] = pCtxt->CTXT_FIS[FIS_STATUS];	// status

	if (extended_descriptor)
	{
	    buffer[2] = 0x01;	// Extend
		buffer[4] = pCtxt->CTXT_FIS[FIS_SEC_CNT_EXT];
		buffer[6] = pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT];	//sata_ch_reg->sata_LbaLH;
		buffer[8] = pCtxt->CTXT_FIS[FIS_LBA_MID_EXT];	//sata_ch_reg->sata_LbaMH;
		buffer[10] = pCtxt->CTXT_FIS[FIS_LBA_HIGH_EXT];	//sata_ch_reg->sata_LbaHH;
	}
}
#endif


/****************************************\
	hdd_ata_err_2_sense_data
\****************************************/
void hdd_ata_err_2_sense_data(PCDB_CTXT	pCtxt)
{
	u8	*pSenseData;

	if (curMscMode == MODE_UAS)
	{
		pSenseData = &(pCtxt->CTXT_CDB[12]);
	}
	else
	{
		pSenseData = &(bot_sense_data[0]);
	}
	*((u32 *)(&pSenseData[0])) = 18;			// Sense_Length
	
	pCtxt->CTXT_Status = CTXT_STATUS_ERROR;	
	pCtxt->CTXT_LunStatus = LUN_STATUS_CHKCOND;


	{
		u8 error;

		error = pCtxt->CTXT_FIS[FIS_ERROR];
		if (error & ATA_ERR_ICRC)  //interface CRC error has occurred
		{
			pSenseData[1] =  SCSI_SENSE_ABORTED_COMMAND;
			pSenseData[2] = 0x47;							//
			pSenseData[3] = 0x03;							// INFORMATION UNIT iuCRC ERROR DETECTED
			return;
		}
		else if (error & ATA_ERR_UNC)  //data is uncorrectable
		{
			pSenseData[1] =  SCSI_SENSE_MEDIUM_ERROR;		// sense key: MEDIUM Error
			pSenseData[2] = 0x11;							// ASC: UNRECOVERED READ ERROR
//          		pSenseData[3] = 0x00;							//
			return;
		}
		else if (error & ATA_ERR_IDNF) // ID not found
		{
			pSenseData[1] =  SCSI_SENSE_MEDIUM_ERROR;		// sense key:MEDIUM Error
			pSenseData[2] = 0x14;							//
			pSenseData[3] = 0x01;							// RECORD NOT FOUND						
			return;
		}
		else // if (error & ATA_ERR_ABRT) // Aborted Command: NO ADDITIONAL SENSE INFORMATION
		{
			pSenseData[1] =  SCSI_SENSE_ABORTED_COMMAND;	// sense key: 
		}
		return;
	}

}

/****************************************\
	hdd_ata_return_tfr
\****************************************/
void hdd_ata_return_tfr(PCDB_CTXT	pCtxt)
{
	u8	*pSenseData;

	if (curMscMode == MODE_UAS)
	{
		pSenseData = &(pCtxt->CTXT_CDB[12]);
	}
	else
	{
		pSenseData = &(bot_sense_data[0]);
	}
	*((u32 *)(&pSenseData[0])) = 22;			// Sense_Length

	if (pCtxt->CTXT_CDB[0] == SCSI_ATA_PASS_THR16)
	{
		pSenseData[1] = pCtxt->CTXT_CDB[0] & 0x01;		// extend bit
	}

	pCtxt->CTXT_Status = CTXT_STATUS_ERROR;	
	pCtxt->CTXT_LunStatus = LUN_STATUS_CHKCOND;

}

/****************************************\
	hdd_err_2_sense_data
\****************************************/
void hdd_err_2_sense_data(PCDB_CTXT	pCtxt, u32 error)
{
	u8	*pSenseData;

	nextDirection = DIRECTION_SEND_STATUS;
	if (curMscMode == MODE_UAS)
	{
		pSenseData = &(pCtxt->CTXT_CDB[12]);
	}
	else
	{
		pSenseData = &(bot_sense_data[0]);
	}
	*((u32 *)(&pSenseData[0])) = 18;			// Sense_Length
	
	//pCtxt->CTXT_Status = CTXT_STATUS_ERROR;	
	//pCtxt->CTXT_LunStatus = LUN_STATUS_CHKCOND;
	*((u16 *)(&(pCtxt->CTXT_Status))) = (LUN_STATUS_CHKCOND << 8)|(CTXT_STATUS_ERROR);


	switch (error)
	{
	case ERROR_NONE:
		//pCtxt->CTXT_Status = CTXT_STATUS_GOOD;	
		//pCtxt->CTXT_LunStatus = LUN_STATUS_GOOD;
		*((u16 *)(&(pCtxt->CTXT_Status))) = (LUN_STATUS_GOOD << 8)|(CTXT_STATUS_GOOD);
		//pSenseData[0] = 0;
	    //pSenseData[1] = 0;
	    //pSenseData[2] = 0;	
		//pSenseData[3] = 0;
		*((u32 *) &(pSenseData[0])) = 0;
		break;
		
	case ERROR_ILL_OP:
		pSenseData[1] = SCSI_SENSE_ILLEGAL_REQUEST;	// sense key: ILLEGAL REQUEST
		pSenseData[2] = 0x20;							// ASC: INVALID COMMAND OPERATION CODE
		break;

	case ERROR_ILL_CDB:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	// sense key: ILLEGAL REQUEST
		pSenseData[2] = 0x24;							// ASC: INVALID Field in CDB
		break;

	case ERROR_ILL_PARAM:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	// sense key: ILLEGAL REQUEST
		pSenseData[2] = 0x26;							// ASC
		pSenseData[3] = 0x01;							// ASCQ: INVALID FIELD IN PARAMETER LIST
		break;

	case ERROR_COMMAND_SEQUENCE:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	// sense key: ILLEGAL REQUEST
		pSenseData[2] = 0x2C;							// ASC: command sequence error
		pSenseData[3] = 0x00;							// ASCQ: 
		break;		

	case ERROR_UA_BECOMING_READY:
		pSenseData[1] =  SCSI_SENSE_NOT_READY;  // sense key: NOT READY
		pSenseData[2] = 0x04;					// ASC: INVALID FIELD IN CDB
		pSenseData[3] = 0x01;					// ASCQ
		break;

	case ERROR_UA_NOT_READY_INIT:
	   	pSenseData[1] =  SCSI_SENSE_NOT_READY;  // sense key: NOT READY
		pSenseData[2] = 0x04;					// LUN NOT READY, INITIALIZING COMMAND REQUIRED
		pSenseData[3] = 0x02;					//
		break;

	case ERROR_UA_NOT_READY:
		pSenseData[1] =  SCSI_SENSE_NOT_READY;  // sense key: NOT READY
	   	pSenseData[2] = 0x04;					// LUN NOT READY, CAUSE NOT REPORTABLE
//	    	pSenseData[3] = 0x00;					//
		break;

	case ERROR_UA_NO_MEDIA:
		pSenseData[1] =  SCSI_SENSE_NOT_READY;  // sense key: NOT READY
		pSenseData[2] = 0x3A;					// ASC: MEDIUM NOT PRESENT
		break;

	case LOGICAL_UNIT_NOT_CONFIGURED:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	// sense key: ILLEGAL REQUEST,
		pSenseData[2] = 0x68;							// ASC: LOGICAL UNIT NOT CONFIGURED 
		break;
		
	case LBA_OUT_RANGE:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	// sense key: ILLEGAL REQUEST,
		pSenseData[2] = 0x21;							// ASC: LOGICAL BLOCK ADDRESS OUT OF RANGE
		break;

	case ERROR_MEDIA_CHANGED:
		pSenseData[1] =  SCSI_SENSE_UNIT_ATTENTION;	// sense key: ILLEGAL REQUEST,
		pSenseData[2] = 0x28;							// ASC: LOGICAL BLOCK ADDRESS OUT OF RANGE
		break;			
		
	case ERROR_WRITE_PROTECT:
		pSenseData[1] =  SCSI_SENSE_DATA_PROTECT;	
		pSenseData[2] = 0x74;		
		pSenseData[3] = 0x71;
		break;

	case ERROR_ROUNDED_PARAMETER:
		pSenseData[1] =  SCSI_SENSE_RECOVERED_ERROR;	
		pSenseData[2] = 0x37;		
		pSenseData[3] = 0x00;	
		break;

	case ERROR_PARAM_LIST_LENGTH:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	// sense key: ILLEGAL REQUEST
		pSenseData[2] = 0x1A;							// ASC
		pSenseData[3] = 0x00;							
		break;
		
	case ERROR_ASSERT_FAILED:
		pSenseData[1] =  SCSI_SENSE_HARDWARE_ERROR;	
		pSenseData[2] = 0x44;		
		pSenseData[3] = 0xa5;
		break;

	case ERROR_HW_FAIL_INTERNAL:
		pSenseData[1] =  SCSI_SENSE_HARDWARE_ERROR;	
		pSenseData[2] = 0x44;
#if 0
		u8 tmp8 = 0;
		if (*usb_MscEmStatus & PROTOCOL_RETRY)
			tmp8 = 0x01;
		else if (*usb_MscEmStatus & ZERO_DATA_RETRY)
			tmp8 = 0x2;
		else if (*usb_MscEmStatus & TX_LINK_RETRY)
			tmp8 = 0x03;
		*usb_MscEmStatus &= ~(TX_LINK_RETRY | ZERO_DATA_RETRY | PROTOCOL_RETRY); 		
		pSenseData[3] = tmp8;
#else
		pSenseData[3] = 0;
#endif
		break;

	case ERROR_UNSUPPORTED_SES_FUNCTION:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	
		pSenseData[2] = 0x35;		
		pSenseData[3] = 0x01;
		break;

	case ERROR_UNLOCK_EXCEED:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	
		pSenseData[2] = 0x74;		
		pSenseData[3] = 0x80;
		break;

	case ERROR_INCORRECT_SECURITY_STATE:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	
		pSenseData[2] = 0x74;		
		pSenseData[3] = 0x81;
		break;
		
	case ERROR_AUTHENTICATION_FAILED:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	
		pSenseData[2] = 0x74;		
		pSenseData[3] = 0x40;
		break;	

	case ERROR_ATA_MEMORIZED:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	
		pSenseData[2] = 0xff;		
		pSenseData[3] = 0xf2;
		break;	

	case ERROR_LBA_OUT_RANGE:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	
		pSenseData[2] = 0x21;		
		pSenseData[3] = 0x00;
		break;

	case ERROR_MEDIUM_LOCKED:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	
		pSenseData[2] = 0x53;		
		pSenseData[3] = 0x02;
		break;	
		
	case ERROR_ILLEGAL_TRACK_MODE:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	
		pSenseData[2] = 0x64;		
		pSenseData[3] = 0x00;
		break;

	case ERROR_DIAG_FAIL_COMPONENT:
		pSenseData[1] =  SCSI_SENSE_HARDWARE_ERROR;	
		pSenseData[2] = 0x40;		
		pSenseData[3] = 0x81;
		break;
		
	case ERROR_HDD_FAIL_INITIALIZATION:
		pSenseData[1] =  SCSI_SENSE_HARDWARE_ERROR;	
		pSenseData[2] = 0x44;		
		pSenseData[3] = 0x81;
		break;
		
	case ERROR_INVALID_FLASH:
		pSenseData[1] =  SCSI_SENSE_HARDWARE_ERROR;	
		pSenseData[2] = 0x44;		
		pSenseData[3] = 0x8B;
		break;

	case ERROR_INVALID_PCBA:
		pSenseData[1] =  SCSI_SENSE_HARDWARE_ERROR;	
		pSenseData[2] = 0x44;		
		pSenseData[3] = 0x8C;
		break;
		
	case ERROR_INVALID_CHIP_VER:
		pSenseData[1] =  SCSI_SENSE_HARDWARE_ERROR;	
		pSenseData[2] = 0x44;		
		pSenseData[3] = 0x8D;
		break;
		
	case ERROR_PERMANENT_WRITE_PROTECT:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	
		pSenseData[2] = 0x27;		
		pSenseData[3] = 0x80;		
		break;

	case ERROR_INVALID_LUN:
		pSenseData[1] =  SCSI_SENSE_ILLEGAL_REQUEST;	
		pSenseData[2] = 0x25;		
		pSenseData[3] = 0x00;		
		break;

	case ERROR_DATA_PHASE_ABORTED:
		pSenseData[1] =  SCSI_SENSE_ABORTED_COMMAND;	
		pSenseData[2] = 0x4B;		// ASC data phase error
		break;
		
	default:		
		pSenseData[1] = SCSI_SENSE_UNIQUE; //0x09;	
	 	pSenseData[2] = 0xff;		
		pSenseData[3] = 0xfe;
		break;
	}
	return;
}

u32 off_line(PCDB_CTXT	pCtxt)
{
	u32 tmp = CmdBlk(2) & 0xc0;

	if (tmp == 0x00)
	{
		return 0;	// 0sec
	}
	if (tmp == 0x40)
	{
		return 2;	// 2sec
	}
	else if (tmp == 0x80)
	{
		return 6;	// 6sec
	}
	else //if (tmp == 0xC0)
	{
		return 14;	// 14sec
	}
}

/************************************************************************\
 xlate_bs8_to_log2m9() - Computes the log2 minus 9 of a block size.
 This is used by the MODE SELECT handler to parse block descriptors.

 Arguments:
	shift_bs	Logical block size divided by 256 (right shifted 8 bits)

 Returns:
	The log2 less 9 of the block size: 0= 512b, 1= 1KB, 2= 2KB, 3= 4KB, etc.
	If the logical block size is invalid or unsupported, returns 255.
 */
u8 xlate_bs8_to_log2m9(u16 shift_bs)
{
u8  ii;
u16 bs;

	ii = 9;
	bs = 512 >> 8;

	for(;  ii <= WDC_MAX_LOGICAL_BLOCK_SIZE_LOG2;  ii++, bs<<=1)
	{
		if (shift_bs == bs)  return ii-9;
	}

	// The block size is not supported, so return 255 to indicate error.
	return 0xff;
}
void update_pending_user_data(void)
{
	
	pending_user_bs_log2m9[0] = vital_data[0].user_bs_log2m9;
	pending_user_data_size[0] = vital_data[0].lun_capacity;
	pending_user_bs_log2m9[1] = vital_data[1].user_bs_log2m9;
	pending_user_data_size[1] = vital_data[1].lun_capacity;
	pending_user_array_size = Min(pending_user_data_size[0], pending_user_data_size[1]);
#ifndef DATASTORE_LED
	if (arrayMode == RAID0_STRIPING_DEV)
		pending_user_array_size <<= 1;
#ifdef SUPPORT_SPAN
	else if (arrayMode == SPANNING)
		pending_user_array_size = pending_user_data_size[0] + pending_user_data_size[1];
#endif
#endif
	pending_user_array_bs_log2m9 = pending_user_bs_log2m9[0];
}
/************************************************************************\
 scsi_mode_select_cmd() - MODE SELECT(6) and (10) command handler.

 */
void scsi_mode_select_cmd(PCDB_CTXT pCtxt)
{
struct all_mode_params  orig_params, new_params;
u8 * page_ptr;
u8 block_desc_length;
u8 rounded_param;
u8 device_config_changed;
u64 tmp_user_area_size;
u8 tmp_user_bs_log2m9;
u8 zero_or_max_blocks;
u16 bytes_remaining;

	if (dbg_next_cmd)
		MSG("mselc\n");

	// Rearrange MODE SELECT(6) commands into MODE SELECT(10).
	if (cdb.opcode == SCSI_MODE_SELECT6)
	{
		// Move and extend the Parameter List Length field.
		cdb.byte[7] = 0;
		cdb.byte[8] = cdb.byte[4];

		// All the other interesting fields are in the same places.
	}
	// Get the Parameter List Length. 
	bytes_remaining = (cdb.mode_select10.param_len[0] << 8) + cdb.mode_select10.param_len[1];
	if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
	{
		// Mode parameters cannot be changed when the drive is locked.
		if ((IS_LOCKED(1) && (arrayMode == JBOD)) || (IS_LOCKED(0)) || (arrayMode == NOT_CONFIGURED))
		{
			goto data_write_protect;
		}

#ifndef SCSI_COMPLY // microsoft scsi compliance test will not set this one, but in WD's spec, this bit will be checked
		// Make sure the Save Pages (SP) bit is set.
		if (!(cdb.mode_select10.flags & MODE_SELECT_SP))
		{
			hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
			return;
		}
#endif
		if (bytes_remaining == 0)
		{// if the byte cnt is 0, it shall not take as error. firmware checks the phase case in the function usb_device_no_data
			nextDirection = DIRECTION_SEND_STATUS;
			return;
		}
		
		if (cdb.opcode == SCSI_MODE_SELECT6)
		{
			// Make sure the parameter list includes the entire header.
			if (bytes_remaining < 4)  goto wrong_param_list_length;	
		}
		else
		{
			if (bytes_remaining < 8)  goto wrong_param_list_length;
		}
	}

	if (scsi_chk_hdd_pwr(pCtxt)) 	return;	
	if (scsi_start_receive_data(pCtxt, bytes_remaining)) return;
	
	// Ok, the CDB is valid and the mode parameter list has been received.
	// Proceed as follows:
	// 1. Make two copies of the current parameters, call them the
	//    orig_params  and  new_params.
	// 2. Parse the mode parameter list, saving new values into  new_params.
	// 3. If an error occured during parsing, return an error; the current
	//    parameters are not changed.
	// 4. Otherwise copy the  new_params  into current and save them to disk.
	// 5. Compare current to  orig_params  and use those that have changed.

	// This flag is set if a mode parameter is rounded to a supported value.
	rounded_param = 0;

	// This is set if anything in the Device Config page changes.
	device_config_changed = 0;

	// Save a copy of the current parameters for later reference.
	xmemcpy((u8 *)&nvm_unit_parms.mode_save, (u8 *)&orig_params.basic, sizeof(orig_params.basic));
	orig_params.device_config4 = nvm_quick_enum.dev_config;
	orig_params.device_config5 = 0;

	u8 slot_num = 0;
	if (logicalUnit == HDD0_LUN)
	{
		if (arrayMode == JBOD)
		{
			if (sata0Obj.sobj_device_state != SATA_NO_DEV)
			{
				orig_params.user_data_size = pending_user_data_size[0];
				orig_params.user_bs_log2m9 = pending_user_bs_log2m9[0];
				tmp_user_area_size = vital_data[0].lun_capacity;
				tmp_user_bs_log2m9 = vital_data[0].user_bs_log2m9;
			}
			else
			{
				slot_num = 1;
				orig_params.user_data_size = pending_user_data_size[1];
				orig_params.user_bs_log2m9 = pending_user_bs_log2m9[1];
				
				tmp_user_area_size = vital_data[1].lun_capacity;
				tmp_user_bs_log2m9 = vital_data[1].user_bs_log2m9;	
			}
		}
		else
		{
			orig_params.user_data_size = pending_user_array_size;
			orig_params.user_bs_log2m9 = pending_user_array_bs_log2m9;
			tmp_user_area_size = array_data_area_size;
			tmp_user_bs_log2m9 = vital_data[0].user_bs_log2m9;
		}
	}
	else if (logicalUnit == HDD1_LUN)
	{
		slot_num = 1;
		orig_params.user_data_size = pending_user_data_size[1];
		orig_params.user_bs_log2m9 = pending_user_bs_log2m9[1];
		
		tmp_user_area_size = vital_data[1].lun_capacity;
		tmp_user_bs_log2m9 = vital_data[1].user_bs_log2m9;		
	}
	else
	{ // for the SES lun, it shall set drive according to selected drive letter if the array mode is JBOD
	  // but if there's only one Disk LUN like RAID0 & RAID1, it shall change the Disk LUN only	
		if (arrayMode == JBOD)
		{
			slot_num = diag_drive_selected - 1;
			orig_params.user_data_size = pending_user_data_size[slot_num];
			orig_params.user_bs_log2m9 = pending_user_bs_log2m9[slot_num];
			tmp_user_area_size = vital_data[slot_num].lun_capacity;
			tmp_user_bs_log2m9 = vital_data[slot_num].user_bs_log2m9;
		}
		else
		{
			orig_params.user_data_size = pending_user_array_size;
			orig_params.user_bs_log2m9 = pending_user_array_bs_log2m9;
			tmp_user_area_size = array_data_area_size;
			tmp_user_bs_log2m9 = vital_data[0].user_bs_log2m9;			
		}
	}

	// Make another copy of the current parameters, to use for parsing.
	xmemcpy((u8 *)&orig_params, (u8 *)&new_params, sizeof(new_params));
	bytes_remaining = (cdb.mode_select10.param_len[0] << 8) + cdb.mode_select10.param_len[1];
	// Validate the header. Most fields are reserved, except for the
	// Block Descriptor Length field, which must be 0.
	if (cdb.opcode == SCSI_MODE_SELECT6)
	{
		// Make sure the parameter list includes the entire header.
		if (bytes_remaining < 4)  goto wrong_param_list_length;

		// Check the Block Descriptor Length; it indicates whether a block
		// descriptor is present. Up to one short descriptor is allowed.
		block_desc_length = mc_buffer[3];
		if ( (block_desc_length != 0)  &&
		     (block_desc_length != SHORT_BLOCK_DESCRIPTOR_SIZE) )
		{
			goto invalid_param_field;
		}

		// Block descriptors and mode pages follow the header.
		page_ptr = mc_buffer + 4;		// Mode parameter header is 4 bytes.
		bytes_remaining -=  4;
	}
	else		// MODE SELECT (10)
	{
		// Make sure the parameter list includes the entire header.
		if (bytes_remaining < 8)  goto wrong_param_list_length;

		// Check the Block Descriptor Length: for starters, at most
		// only one descriptor is accepted, so the Length MSB must be 0.
		if (mc_buffer[6])  goto invalid_param_field;

		block_desc_length = mc_buffer[7];

		// Accept up to one block descriptor, long or short
		// depending on the LongLBA bit.
		if ( (block_desc_length == 0)  ||
		     ( (mc_buffer[4] & 0x01) &&
		      (block_desc_length == LONG_BLOCK_DESCRIPTOR_SIZE))  ||
		     (!(mc_buffer[4] & 0x01) &&
		      (block_desc_length == SHORT_BLOCK_DESCRIPTOR_SIZE)) )
		{
			// Oh, good, there may be one long or short block descriptor.
		}
		else
		{
			goto invalid_param_field;
		}

		page_ptr = mc_buffer + 8;		// Mode parameter header is 8 bytes.
		bytes_remaining -=  8;
	}

	// Parse the Block Descriptor if there is one.
	//

	// This flag will be set if the Number of Logical Blocks in the
	// descriptor is either 0 or 0xff..ff. Those values are special.
	zero_or_max_blocks = 0;
#ifdef DEBUG
	printf("block_desc_length %bx\n", block_desc_length);
#endif
	// The virtual CD-ROM does not support block descriptors.
	if (((logicalUnit == cdrom_lun) ||(logicalUnit == ses_lun)) && block_desc_length)
	{
		goto invalid_param_field;
	}

	// Make sure the block descriptor (if present) is not truncated.
	if (block_desc_length  &&  (bytes_remaining < block_desc_length))
	{
		goto wrong_param_list_length;
	}

	// Since there are two kinds of descriptors (long- and short-LBA),
	// parse it into a common format - the 40-bit LBA - and then process it.
	if (block_desc_length == LONG_BLOCK_DESCRIPTOR_SIZE)
	{
		// Bytes 12..15 is the Logical Block Length.
		// Since we only support block sizes from 512 bytes to
		// 2**WDC_MAX_LOGICAL_BLOCK_SIZE_LOG2; the MSB and LSB must be 0.
		if (page_ptr[12] | page_ptr[15])  goto invalid_param_field;

		new_params.user_bs_log2m9 =
			xlate_bs8_to_log2m9((page_ptr[13] << 8) + page_ptr[14]);

		// F/W only support block size is 512B/2KB/4KB
		if ((new_params.user_bs_log2m9 != 0) && (new_params.user_bs_log2m9 != 1) && (new_params.user_bs_log2m9 != 2) && (new_params.user_bs_log2m9 != 3))
			goto invalid_param_field;

		// Bytes 0..7 is # of Logical Blocks. 
//		COPYU64_REV_ENDIAN(&new_params.user_data_size, page_ptr);
		// remove the warning by force varible type to 64bit
		new_params.user_data_size = ((u64)page_ptr[0] << 56) + ((u64)page_ptr[1] << 48) + ((u64)page_ptr[2] << 40) + ((u64)page_ptr[3] << 32)
								+ ((u64)page_ptr[4] << 24) + ((u64)page_ptr[5] << 16) + ((u64)page_ptr[6] << 8) +((u64) page_ptr[7]);
		// ...but validate the entire field before using just those 40 bits.
		if (( new_params.user_data_size  == 0) ||
		      (new_params.user_data_size  == 0xffffffffffffffff) )
		{
			// Got a zero or 2**64 -1, which have special meaning.
			zero_or_max_blocks = 1;
		}
		else	// # of Blocks is between 1 and 2**64 -2, inclusive.
		{
			// We don't support capacity >= 2**40 blocks, so MSB have to be 0.
			if (page_ptr[0] | page_ptr[1] | page_ptr[2])  goto invalid_param_field;
		}
	}
	else if (block_desc_length == SHORT_BLOCK_DESCRIPTOR_SIZE)
	{
		// Bytes 5..7 is the Logical Block Length, 24-bits' worth.
		// Within the range of supported block sizes, the LSB must be 0.
		if (page_ptr[7] != 0)  goto invalid_param_field;
		u16 tmp16 = (page_ptr[5] << 8) + page_ptr[6];
		new_params.user_bs_log2m9 =
			xlate_bs8_to_log2m9( tmp16 );
		MSG("bs %bx\n", new_params.user_bs_log2m9);
		// F/W only support block size is 512B/1K/2KB/4KB
		if ((new_params.user_bs_log2m9 != 0) && (new_params.user_bs_log2m9 != 1) && (new_params.user_bs_log2m9 != 2) && (new_params.user_bs_log2m9 != 3))
			goto invalid_param_field;

		// Bytes 0..3 is the Number of Logical Blocks.
		new_params.user_data_size = (u32)((page_ptr[0] << 24) + (page_ptr[1] << 16) + (page_ptr[2] << 8) + page_ptr[3]);
		if ((new_params.user_data_size == 0) || (new_params.user_data_size == 0xffffffff))
			zero_or_max_blocks = 1;
	}

	if (block_desc_length)
	{
		if (zero_or_max_blocks)
		{
 			// The capacity does not change if the new capacity is 0 and
			// the new block size is same as current.
			if ( !new_params.user_data_size  &&
			     (new_params.user_bs_log2m9 == tmp_user_bs_log2m9) )
			{
				xmemcpy((u8 *)&orig_params.user_data_size, (u8 *)&new_params.user_data_size, 8);
			}
			else
			{
				// The new capacity is 0xff..ff or the block size changed,
				// so reset the capacity to the maximum.
				new_params.user_data_size = (tmp_user_area_size & 0xFFFFFFFFFFFFF800) - INI_VCD_SIZE;
			}
		}
		else
		{
			u8 bits_kept = 32 - new_params.user_bs_log2m9;

			// The # of Logical Blocks needs to be multiplied by the
			// new block size; before doing so, check if that will overflow.
			if ( new_params.user_bs_log2m9  &&
			     ((*(u32*)(&new_params.user_data_size) >> bits_kept) != 0) )
			{
				goto invalid_param_field;
			}

			new_params.user_data_size = new_params.user_data_size << new_params.user_bs_log2m9;

			if (arrayMode == JBOD)
			{
				if ((logicalUnit == HDD0_LUN) && (sata0Obj.sobj_device_state != SATA_NO_DEV))
					pSataObj = &sata0Obj;
				else
					pSataObj = &sata1Obj;			
				if (new_params.user_data_size > (pSataObj->sectorLBA & 0xFFFFFFFFFFFFF800))
					goto invalid_param_field;
			}
			else
			{
				if (new_params.user_data_size > (array_data_area_size & 0xFFFFFFFFFFFFF800))
					goto invalid_param_field;
			}
		}

		bytes_remaining -= block_desc_length;
		page_ptr += block_desc_length;
	}

	// Parse the mode parameter pages one by one.
	//
	while (bytes_remaining  > 0)
	{
		switch (*page_ptr & MODE_PAGE_CODE_MASK)
		{
#ifdef SUPPORT_CACHING_MODE_PAGE
		case MODE_CACHING_MODE_PAGE:
			if ((logicalUnit != HDD0_LUN) && (logicalUnit != HDD1_LUN))  goto invalid_param_field;
			
			// Make sure the Write cache enable bit in byte 2 is present.
			if (bytes_remaining < 3)  goto wrong_param_list_length;

			// Check the Page Length field.
			if (page_ptr[1] != 18)  goto invalid_param_field;

			// enable write cache bit is 0 or RCD bit is 1, fail the command
			if (((page_ptr[2] & 0x04) == 0) || (page_ptr[2] & 0x01)) goto invalid_param_field;
			break;
#endif
			
	 	case MODE_POWER_COND_PAGE:
			// The virtual CD-ROM does not support this mode page.
			if (logicalUnit == cdrom_lun)  goto invalid_param_field;

			// Make sure the entire mode page is present.
			if (bytes_remaining < 12)  goto wrong_param_list_length;

			// Check the Page Length field.
			if (page_ptr[1] != 38)  goto invalid_param_field;

			// The Idle bit and Idle condition timer are not supported.
			if ((page_ptr[3] & 0x02)  ||
			     page_ptr[4] || page_ptr[5] || page_ptr[6] || page_ptr[7] )
			{
				goto invalid_param_field;
			}

			// Fetch the Standby bit and Standby condition timer.
			new_params.basic.standby_timer_enable = page_ptr[3] & 0x01;
			new_params.basic.standby_time = (page_ptr[8] << 24) + (page_ptr[9] << 16) + (page_ptr[10] << 8) + page_ptr[11];
#ifdef DEBUG
	printf("new_params.basic.standby_timer_enable %bx\n", new_params.basic.standby_timer_enable);
	printf("new_params.basic.standby_time %lx\n", new_params.basic.standby_time);
#endif
			break;

		case MODE_WDC_OPERATIONS_PAGE:
#if 0
			// Bit-buckets do not support this mode page.
			if (product_detail.options & PO_IS_BIT_BUCKET)  goto invalid_param_field; 
#endif

			if (bytes_remaining < 12)  goto wrong_param_list_length;

			// Check the Page Length field.
			if (page_ptr[1] != 10)  goto invalid_param_field;

			// Check Signature field.
			if (page_ptr[2] != OPS_SIGNATURE)  goto invalid_param_field;

			// Flags in byte 4 are not supported (Loose SBP2, eSATA, etc.)
			if (page_ptr[4])  goto invalid_param_field;

			if (product_detail.options & PO_NO_ENCRYPTION)
			{
				if (page_ptr[5] & OPS_LOOSE_LOCKING)
					goto invalid_param_field;
			}

			// Save the CD Media Valid and Enable CD Eject bits.
			new_params.basic.cd_valid_cd_eject = page_ptr[5] & (OPS_CD_MEDIA_VALID | OPS_ENABLE_CD_EJECT);
			new_params.basic.loose_locking = page_ptr[5] & OPS_LOOSE_LOCKING;
			
			// Byte 8 is Power LED Brightness.
			// Only fully on and off are supported, so round to 0 or 255.
			new_params.basic.power_led_brightness =  0 - (page_ptr[8] >> 7);

			if (new_params.basic.power_led_brightness != page_ptr[8])
			{
				rounded_param = 1;		// Brightness was actually rounded!
			}

			// Backlight Brightness is not supported.
			if (page_ptr[9])  goto invalid_param_field;

			if (page_ptr[10] & OPS_W_ON_B)
			{
				goto invalid_param_field;
			}

			break;

		case MODE_WDC_DEVICE_CONFIG_PAGE:
#if 0
			// Bit-buckets do not support this mode page.
			if (product_detail.options & PO_IS_BIT_BUCKET)  goto invalid_param_field; 
#endif
			DBG("dev conf pg\n");

			// The virtual CD-ROM does not support this mode page.
	 		if (logicalUnit == cdrom_lun)  goto invalid_param_field;

			if (bytes_remaining < 8)  goto wrong_param_list_length;

			// Check the Page Length field.
			if (page_ptr[1] != 6)  goto invalid_param_field;

			// Check Signature field.
			if (page_ptr[2] != DEVCFG_SIGNATURE)  goto invalid_param_field;


			// The unchangeable bits in byte 4 must not be changed.
			if (page_ptr[4] & ~(DEVCFG_DISABLE_AUTOPOWER|DEVCFG_DISABLE_CDROM | DEVCFG_DISABLE_SES|DEVCFG_DISABLE_U1U2|DEVCFG_DISABLE_UAS))
			{
				goto invalid_param_field;
			}
			MSG("%bx\n", product_detail.options);
			if (product_detail.options & PO_NO_CDROM)
			{
				if (((page_ptr[4] & DEVCFG_DISABLE_CDROM) == 0) && ((product_detail.options & PO_EMPTY_DISC) == 0))
					goto invalid_param_field;
			}

			if (product_detail.options & PO_NO_SES)
			{
				if ((page_ptr[4] & DEVCFG_DISABLE_SES) == 0)
					goto invalid_param_field;
			}

			// Nothing in byte 5 is allowed to change.
			if (page_ptr[5])  goto invalid_param_field;
			
			new_params.device_config4 = page_ptr[4];
			DBG("byte 4: %bx\n", new_params.device_config4);
			new_params.device_config5 = page_ptr[5];

			break;

		// case MODE_CD_CAPABILITY_PAGE:		// Read-only page, do not accept.
		default:
			// Invalid or unsupported Page Code.
			goto invalid_param_field;
		}

		// Update the byte count to "consume" this mode page and bump
		// the page pointer to the next mode page.
		bytes_remaining -= page_ptr[1] + 2;
		page_ptr +=  page_ptr[1] + 2;
	}

	// Turn on and re-init the HDD if it is off.
	if (hdd_power != HDDs_BOTH_POWER_ON)
	{
		if (ata_startup_hdd(1, SATA_TWO_CHs))
		{
			hdd_err_2_sense_data(pCtxt, ERROR_UA_BECOMING_READY);
			return;
		}
	}
	
	// Done parsing and no errors occured! Save the new mode parameters.
	xmemcpy((u8 *)(&new_params.basic), (u8 *)(&nvm_unit_parms.mode_save),
	                     sizeof(nvm_unit_parms.mode_save));

	save_nvm_unit_parms();

	// Now save the device configuration parameters if the device config is changed
	if ( orig_params.device_config4 != new_params.device_config4)
	{
		nvm_quick_enum.dev_config = new_params.device_config4;
		save_nvm_quick_enum();
	}

	// Remember the user-data area's new size and logical block length...
	if (arrayMode == JBOD)
	{
		pending_user_data_size[slot_num] = new_params.user_data_size;
		pending_user_bs_log2m9[slot_num] = new_params.user_bs_log2m9;
	}
	else
	{
		pending_user_data_size[0] = new_params.user_data_size;
		pending_user_bs_log2m9[0] = new_params.user_bs_log2m9;	
		pending_user_data_size[1] = new_params.user_data_size;
		pending_user_bs_log2m9[1] = new_params.user_bs_log2m9;
	}

	// ...but if the logical block length did not change, then
	// the new size takes effect immediately.
	if (arrayMode == JBOD)
	{
		if ((tmp_user_bs_log2m9 == pending_user_bs_log2m9[slot_num])  &&
		     (tmp_user_area_size != pending_user_data_size[slot_num]) )
		{
			vital_data[slot_num].lun_capacity = pending_user_data_size[slot_num];
			device_config_changed = 1;
		}
	}
	else
	{
		if ((tmp_user_bs_log2m9 == pending_user_array_bs_log2m9)  &&
		     (tmp_user_area_size != pending_user_array_size) )
		{
			ram_rp[0].array_size = pending_user_array_size;
			ram_rp[1].array_size = pending_user_array_size;
			device_config_changed = 1;
		}
	}

	
#ifndef INITIO_STANDARD_FW
	if (device_config_changed)
	{
		// These parameters are stored in the Init data. There are
		// two copies; those should to be updated independently since
		// only the Init data is changing (password-protected data is not)
		//
		MSG("mode save vital: %lx %lx\n", (u32)vital_data[0].lun_capacity, (u32)vital_data[1].lun_capacity);
		if (arrayMode == JBOD)
		{
			if (Write_Disk_MetaData(slot_num, WD_VITAL_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP))
			{
				hdd_ata_err_2_sense_data(pCtxt);

				// is there a point trying to restore the original params?
				// there are two copies of Init Data; what if only one was updated?
				DBG("update iniData fail\n");
				return;
			}
		}
		else
		{
#if 1
			if (Write_Disk_MetaData(0, WD_RAID_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP)
			|| Write_Disk_MetaData(1, WD_RAID_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP))
#else
			if (Write_Disk_MetaData(0, WD_VITAL_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP)
			|| Write_Disk_MetaData(1, WD_VITAL_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP))
#endif
			{
				hdd_ata_err_2_sense_data(pCtxt);

				// is there a point trying to restore the original params?
				// there are two copies of Init Data; what if only one was updated?
				DBG("update iniData fail\n");
				return;
			}	
		}
	}
#endif

	// Apply the mode parameters that have changed.
	//
	update_user_data_area_size();
	update_array_data_area_size();

	// Did the CD Media Valid bit change?
	if ( (nvm_unit_parms.mode_save.cd_valid_cd_eject ^ orig_params.basic.cd_valid_cd_eject) &
	     OPS_CD_MEDIA_VALID)
	{
		vcd_set_media_presence(
			nvm_unit_parms.mode_save.cd_valid_cd_eject & OPS_CD_MEDIA_VALID );
	}

	// Adjust the LED brightness if that settting has changed.
	if (nvm_unit_parms.mode_save.power_led_brightness !=
	    orig_params.basic.power_led_brightness)
	{
		// Turning the lights off and on resets their brightness but
		// also stops any activity blinking. We'll just ignore this
		// minor side effect....
		lights_out();
		set_power_light(1);
	}

	// Reset the HDD standby timer if it has changed.
	if ((nvm_unit_parms.mode_save.standby_timer_enable !=
	     orig_params.basic.standby_timer_enable) ||
		(nvm_unit_parms.mode_save.standby_time != orig_params.basic.standby_time) )
	{
		update_standby_timer();
		reset_standby_timer();
	}
	// Report whether any parameters were rounded.
	if (rounded_param)
	{
		hdd_err_2_sense_data(pCtxt, ERROR_ROUNDED_PARAMETER);
	}
	else
		nextDirection = DIRECTION_SEND_STATUS;
	MSG("sta %bx\n", pCtxt->CTXT_Status);
	return;

invalid_param_field:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_PARAM);
	return;

wrong_param_list_length:
	hdd_err_2_sense_data(pCtxt, ERROR_PARAM_LIST_LENGTH);
	return;

data_write_protect:
	hdd_err_2_sense_data(pCtxt, ERROR_WRITE_PROTECT);
  	return;
}

/************************************************************************\
 scsi_report_luns_cmd() - REPORT LUNS command handler.

 */
void scsi_report_luns_cmd(PCDB_CTXT pCtxt)
{
	xmemset(mc_buffer, 0, 64);
	nextDirection = DIRECTION_SEND_DATA;
	alloc_len =((u32)cdb.byte[6] << 24) + ((u32)cdb.byte[7] << 16) + ((u32)cdb.byte[8] << 8) + cdb.byte[9];	
	
	//decode the select report field
	if ((CmdBlk(2) == 0) ||(CmdBlk(2) == 2))
	{// select report 0 & 2, shall return all logical units
		// Bytes 0..3 is the LUN List Length field.
		// The LUN List starts at offset 8 and each entry is 8 bytes.
		// Since we have at most three LUNs, we only use byte-sized quantities.

		// Mass-storage is LUN 0 and always present.
		mc_buffer[3] = 8;
		if (HDD1_LUN != UNSUPPORTED_LUN)
		{
			// Increase the LUN List Length to include one more entry,
			// and then set the entry to include the LUN.
			mc_buffer[3] += 8;
			mc_buffer[ mc_buffer[3] +1 ] = HDD1_LUN;
		}

		// Add more LUN entries for the other LUNs that are present.

		if (VCD_LUN != UNSUPPORTED_LUN)
		{
			// Increase the LUN List Length to include one more entry,
			// and then set the entry to include the LUN.
			mc_buffer[3] += 8;
			mc_buffer[ mc_buffer[3] +1 ] = VCD_LUN;
		}

		if (SES_LUN != UNSUPPORTED_LUN)
		{
			mc_buffer[3] += 8;
			mc_buffer[ mc_buffer[3] +1 ] = SES_LUN;
		}

		byteCnt = mc_buffer[3] + 8;	
	}
	else if (CmdBlk(2) == 1)
	{
		byteCnt = mc_buffer[3] + 8;
	}
	else
	{
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
	}
}

/************************************************************************\
 assign_luns() - Assign LUNs to the various logical units.

 The virtual CD-ROM and SES are given logical unit numbers if
 it is not disabled; disabled logical units are given UNSUPPORTED_LUN.

 Prerequisites:
	The Device Configuration parameters (in Init data) is loaded.

 */
void assign_luns(void)
{
u8 next_lun;

	// Assign logical unit numbers to the virtual CD-ROM and SES.
	// The mass-storage (disk) is always LUN 0, so others start at 1.
	//
	re_enum_flag = 0;
	next_lun = 1;
	cdrom_lun = UNSUPPORTED_LUN;
	ses_lun = UNSUPPORTED_LUN;
	hdd1_lun = UNSUPPORTED_LUN;
	
	if (num_DiskLuns_created > 1)
	{// when number of Disk LUNs is greater than 1, create one more Disk LUN
		hdd1_lun = next_lun++;
	}

//#ifndef INITIO_STANDARD_FW		// manage it to see VCD all the time or not, part 1/2
	if (arrayMode != NOT_CONFIGURED)
//#endif		
	{
#ifdef WDC_VCD
		if (((nvm_quick_enum.dev_config & DEVCFG_DISABLE_CDROM) == 0) 
		&& (((product_detail.options & PO_NO_CDROM) == 0) || (product_detail.options & PO_EMPTY_DISC)))
		{
			// Assign a LUN to the virtual CD-ROM if it isn't disabled.
			cdrom_lun = next_lun++;
		}
#endif
#ifdef WDC_SES
	 	if (((nvm_quick_enum.dev_config & DEVCFG_DISABLE_SES) == 0) && ((product_detail.options & PO_NO_SES) == 0))
	 	{
			// Do the same for the SES logical unit.
			ses_lun = next_lun++;
	 	}
#endif
	}

	DBG("next_lun %bx\n", next_lun);	
#ifdef WDC 
	*usb_Msc0LunCtrl = (*usb_Msc0LunCtrl & 0xF8) | (next_lun-1); // set the LUN[2-0]
	u8 val = *usb_Msc0LunCtrl;
	DBG("\tMax_lun: %bx\n", val);
#endif

}
#ifdef SUPPORT_CACHING_MODE_PAGE
/************************************************************************\
 fill_caching_page() - Fills in caching mode page.
 limit to disk lun
  */
void fill_caching_mode_page(void)
{
// page length 20
	xmemset(mode_page_ptr, 0x00, 20);
	*mode_page_ptr = 0x80 | MODE_CACHING_MODE_PAGE; // PS bit (bit 7), Page code field
	*(mode_page_ptr + 1) = 18;
	switch (cdb.mode_sense10.page_code_ctrl & MODE_PC_MASK)
	{
	case MODE_PC_CHANGABLE_VALUES:
		break;

	case MODE_PC_DEFAULT_VALUES:
		*(mode_page_ptr + 2) = 0x14;
		*(mode_page_ptr + 4) = 0xFF; // MSB, disable pre-fetch transfer length
		*(mode_page_ptr + 5) = 0xFF; // LSB
		*(mode_page_ptr + 8) = 0xFF; // MSB, Maximum pre-fetch
		*(mode_page_ptr + 9) = 0xFF; // LSB
		*(mode_page_ptr + 10) = 0xFF; //MSB, Maximum pre-fetch ceiling
		*(mode_page_ptr + 11) = 0xFF; // LSB
		*(mode_page_ptr + 13) = 0xFF; // number of cache segments	
		break;
		
	case MODE_PC_CURRENT_VALUES:
		*(mode_page_ptr + 2) = 0x14;
		*(mode_page_ptr + 4) = 0xFF; // MSB, disable pre-fetch transfer length
		*(mode_page_ptr + 5) = 0xFF; // LSB
		*(mode_page_ptr + 8) = 0xFF; // MSB, Maximum pre-fetch
		*(mode_page_ptr + 9) = 0xFF; // LSB
		*(mode_page_ptr + 10) = 0xFF; //MSB, Maximum pre-fetch ceiling
		*(mode_page_ptr + 11) = 0xFF; // LSB
		*(mode_page_ptr + 13) = 0xFF; // number of cache segments		
		break;
	}
	mode_page_ptr += 20; 
}
#endif

/************************************************************************\
 fill_caching_page() - Fills in caching mode page.
 limit to disk lun
  */
void fill_control_page(void)
{
// page length 20
	xmemset(mode_page_ptr, 0x00, 12);
	*mode_page_ptr = 0x80 | MODE_CONTROL_PAGE;
	*(mode_page_ptr + 1) = 10;
	switch (cdb.mode_sense10.page_code_ctrl & MODE_PC_MASK)
	{
	case MODE_PC_CHANGABLE_VALUES:
		break;
	default:
		*(mode_page_ptr + 3) = 0x10;
		break;
	}
	mode_page_ptr += 12; 
}

/************************************************************************\
 fill_block_descriptor() - Fills in a block descriptor.

 Prerequisites:
	- The CDB is a valid MODE SENSE(10) command.
	- mode_page_ptr  points to a zeroed buffer.

 Arguments:
	None.

 Returns:
	Size of the block descriptor, in bytes.
 */
u8 fill_block_descriptor(void)
{
u8 descriptor[LONG_BLOCK_DESCRIPTOR_SIZE];
u8 desc_length;
#if 0//def DEBUG
printf("fill_block\n");
#endif
	xmemset(descriptor, 0, sizeof(descriptor));

	// Fill in one block descriptor.
	// The descriptor has the # of logical blocks and block size.
	// These values may not be the drive's current parameters;
	// they are the last values set by the OS or application.

	// The # of blocks reported depends on the block size.
	// Do the math first as if for a long LBA block descriptor,
	// and then rearrange the bytes as needed.

	// Compute and put the # of Blocks into bytes 0..7.
	u8 slot_num = 0;
	if ((logicalUnit == HDD0_LUN) || (logicalUnit == HDD1_LUN))
	{
		slot_num = logicalUnit;
	}
	else if (logicalUnit == SES_LUN)
	{
		slot_num = diag_drive_selected - 1;
	}
	u64 tmp64 = pending_user_data_size[slot_num] -1;
	tmp64 = tmp64 >> pending_user_bs_log2m9[slot_num];
	WRITE_U64_BE(descriptor, tmp64);
	

	// Return long LBA block descriptors if the host accepts them.
	u32 logical_block_len = (u32)1 << (pending_user_bs_log2m9[slot_num] +9);
	if (cdb.mode_sense10.flags & MODE_SENSE_LLBAA)
	{
		// Bytes 0..7 is the # of Logical Blocks - already filled in.

		// Bytes 8..11 are reserved, already set to 0.

		// Bytes 12..15 is Logical Block Length.
		WRITE_U32_BE(&descriptor[12], logical_block_len);

		desc_length = LONG_BLOCK_DESCRIPTOR_SIZE;
	}
	else	// Append one short-LBA block descriptor.
	{
		// Bytes 0..3 is the # of Logical Blocks.
		// The 40-bit number is in bytes 3..7 (see above), so move it
		// to the right place, but return  2**32 -1  if it's too large.
		if (descriptor[3])
		{
			*(u32*)(descriptor+0) = 0xffffffff;
		}
		else
		{
			*(u32*)(descriptor+0) = *(u32*)(descriptor+4);
		}

		// Byte 4 is reserved.
		// Bytes 5..7 is Logical Block Length (only 24 bits!) As long as
		// the block length exponent is less than 24, byte 4 will be zero.
		WRITE_U32_BE(&descriptor[4], logical_block_len);

		desc_length = SHORT_BLOCK_DESCRIPTOR_SIZE;
	}

	// Copy the block descriptor into the mode parameter buffer...
	xmemcpy(descriptor, mode_page_ptr, desc_length);

	// ...and increment the pointer accordingly.
	mode_page_ptr += desc_length;

	return desc_length;
}

/************************************************************************\
 fill_cd_capabilities_page() - Fills in the CD-ROM's Capabilities and 
 Mechanical Status page

 Prerequisites:
	- The CDB is a valid MODE SENSE(10) command.
	- mode_page_ptr  points to a zeroed buffer.
 */
void fill_cd_capabilities_page()
{
const u8 cd_cap_template[] = {
	MODE_CD_CAPABILITY_PAGE,	// Byte 0: Page Code
	20,							// Byte 1: Page Length
	0,							// Byte 2: Readable media
	0,							// Byte 3: Writable media
	0x40,						// Byte 4: capabilities, set Multisession bit
	0x00,						// Byte 5: capabilities
	0x09,						// Byte 6: capabilities, set Eject, Lock bits
	0x00,						// Byte 7: capabilities
	0x23, 0x28,					// Bytes 8..9: Max Read Speed, just use 9000
	0, 0, 0, 0,
	0x23, 0x28,					// Bytes 14..15: Current Read Speed
	// The rest of the page is unused - let it be zero.
	};

	switch (cdb.mode_sense10.page_code_ctrl & MODE_PC_MASK /*Page Control*/)
	{
 	case MODE_PC_CHANGABLE_VALUES:
		// This page is read-only; nothing is changable.
		// Only the page code and length needs to be set.
		xmemcpy((u8 *)cd_cap_template, mode_page_ptr, 2);
		break;

	default:		// Current, Saved and Defaults are all the same
		// Fill in the non-zero parts of the mode page from the template;
		// the rest of the buffer is already zero-filled.
		xmemcpy((u8 *)cd_cap_template, mode_page_ptr, sizeof(cd_cap_template));

		if (cd_medium_lock)  mode_page_ptr[6] |= 0x2;		// Copy Lock State
	}

	mode_page_ptr += 22;		// Added this many bytes to the buffer.
	return;
}


/************************************************************************\
 fill_device_config_page() - Fills in WD's Device Configuration mode page.

 Prerequisites:
	- The CDB is a valid MODE SENSE(10) command.
	- mode_page_ptr  points to a zeroed buffer.
 */
void fill_device_config_page()
{
u8 page[16];

	// Optimization: pointer dereferences are very expensive with this
	// version of the Keil compiler. Thus using  mode_page_ptr  to
	// fill in the mode page one byte at a time is inefficient, compared
	// to assembling the page in a local buffer (which the compiler
	// knows the address of) and then copying it to  mode_page_ptr.

	xmemset(page, 0, sizeof(page));

	// Set page code and Parameters Savable (PS) bit
	page[0] = MODE_WDC_DEVICE_CONFIG_PAGE | MODE_PRAMS_SAVABLE;
	page[1] = 6;		// Page length

	switch (cdb.mode_sense10.page_code_ctrl & MODE_PC_MASK)
	{
 	case MODE_PC_CHANGABLE_VALUES:
		// Only the virtual CD-ROM and SES logical units can be disabled;
		// disabling Auto-Power is not supported.

		// If the product supports the virtual CD-ROM or SES logical unit,
		// then allow the coresponding disable bit to be changed.
		if ((!(product_detail.options & PO_NO_CDROM)) || (product_detail.options & PO_EMPTY_DISC))
		{
			page[4] |= DEVCFG_DISABLE_CDROM;
		}

		
           
		if (!(product_detail.options & PO_NO_SES))
		{
			page[4] |= DEVCFG_DISABLE_SES;
		}

		page[4] |= DEVCFG_DISABLE_U1U2;
		// The Whitelist and 2TB Limit settings cannot be changed.
		break;

	case MODE_PC_DEFAULT_VALUES:
		page[2] = DEVCFG_SIGNATURE;

		// Some products never show the virtual CD-ROM or SES logical units.
		// These should match the default values in  ini_reset_defaults()
		if (product_detail.options & PO_NO_CDROM)
		{
			page[4] |= DEVCFG_DISABLE_CDROM;
		}

		if (product_detail.options & PO_NO_SES)
		{
			page[4] |= DEVCFG_DISABLE_SES;
		}
		//Changed default value of disable U1/U2 field from zero to one
		//The disable U1/U2 bit is changeable only on products with a USB3.0 port.

		page[4] |= DEVCFG_DISABLE_U1U2;

		
		break;

	default:		// Current or Saved values;they are the same!
		page[2] = DEVCFG_SIGNATURE;
		page[4] = nvm_quick_enum.dev_config;
		if ((product_detail.options & PO_NO_CDROM) && ((product_detail.options & PO_EMPTY_DISC) == 0))
		{
			page[4] |= DEVCFG_DISABLE_CDROM;
		}
		
		if (product_detail.options & PO_NO_SES)
		{
			page[4] |= DEVCFG_DISABLE_SES;
		}
	}

	xmemcpy(page, mode_page_ptr, 8);
	mode_page_ptr += 8;
}

/************************************************************************\
 fill_operations_page() - Fills in WD's Operations mode page.

 Prerequisites:
	- The CDB is a valid MODE SENSE(10) command.
	- mode_page_ptr  points to a zeroed buffer.
 */
void fill_operations_page(void)
{
u8 page[16];

	xmemset(page, 0, 16);

	// Set page code and Parameters Savable (PS) bit
	page[0] = MODE_WDC_OPERATIONS_PAGE | MODE_PRAMS_SAVABLE;
	page[1] = 10;		// page length

	switch (cdb.mode_sense10.page_code_ctrl & MODE_PC_MASK)
	{
 	case MODE_PC_CHANGABLE_VALUES:
		page[5] = OPS_CD_MEDIA_VALID | OPS_ENABLE_CD_EJECT;
		
		if ((product_detail.options & PO_NO_ENCRYPTION) == 0)
		{
			page[5] |= OPS_LOOSE_LOCKING;
		}
		
		page[8] = 0xFF;			// Power LED brightness is changable
		break;

	case MODE_PC_DEFAULT_VALUES:
		page[2] = OPS_SIGNATURE;

		// These defaults should match those set by  load_defaults()
		page[5] = OPS_CD_MEDIA_VALID;
		page[8] = 0xff;
		page[10] = 0;	
		break;

	default:		// Current or Saved values;they are the same!
		page[2] = OPS_SIGNATURE;
		page[5] = nvm_unit_parms.mode_save.cd_valid_cd_eject & 
					(OPS_CD_MEDIA_VALID | OPS_ENABLE_CD_EJECT);
		
		if ((product_detail.options & PO_NO_ENCRYPTION) == 0)
		{
			page[5] |= (nvm_unit_parms.mode_save.loose_locking & OPS_LOOSE_LOCKING);
		}
		page[8] = nvm_unit_parms.mode_save.power_led_brightness;
	}

	// Copy the page into the mode page buffer...
	xmemcpy(page, mode_page_ptr, 12);

	// ...and increment the pointer accordingly.
	mode_page_ptr += 12;
}

/************************************************************************\
 fill_power_condition_page() - Fills in the Power Condition mode page.

 Prerequisites:
	- The CDB is a valid MODE SENSE(10) command.
	- mode_page_ptr  points to a zeroed buffer.
 */
void fill_power_condition_page(void)
{
u8 page[44];

	xmemset(page, 0, 44);

	page[0] = MODE_POWER_COND_PAGE | MODE_PRAMS_SAVABLE;
	page[1] = 38;				// Page length

	switch (cdb.mode_sense10.page_code_ctrl & MODE_PC_MASK)
	{
 	case MODE_PC_CHANGABLE_VALUES:
		page[3] = 0x01;			// Standby bit is changable

		WRITE_U32_BE(&page[8], 0xffffffff);
		break;

	case MODE_PC_DEFAULT_VALUES:
		page[3] = 0x01;			// Standby bit defaults to 1, enabled.

		// Return the default Standby Condition Time.
		WRITE_U32_BE(&page[8], WDC_DEFAULT_STANDBY_TIME);
		break;

	default:		// Current or Saved values; they are the same!
		// Byte 3, bit 0 is the Standby bit.
		page[3] = nvm_unit_parms.mode_save.standby_timer_enable ? 1 : 0;

		// Bytes 8..11 is the Standby Condition Timer.
		WRITE_U32_BE(&page[8], nvm_unit_parms.mode_save.standby_time);
	}

	xmemcpy(page, mode_page_ptr, 40);
	mode_page_ptr += 40;
	return;
}
/************************************************************************\
 fill_apollo_mode_pages() - Append mode pages that Apollo products use.

 Prerequisites:
	- The CDB is a valid MODE SENSE(10) command.
	- mode_page_ptr  points to a buffer for the  fill_*()  functions.

 Returns:
	0 if no error occurs;
	1 if the request can't be satisified, e.g., an invalid mode page code.
 */
u8 fill_apollo_mode_pages(PCDB_CTXT pCtxt)
{
	if (logicalUnit == cdrom_lun)
	{
		// Check the Page Code and fill in the supported pages.
		switch (cdb.mode_sense10.page_code_ctrl & MODE_PAGE_CODE_MASK)
		{
		case MODE_CONTROL_PAGE:
_FILL_CONTROL_PAGE:	
			fill_control_page();
			break;	
			
		case MODE_CD_CAPABILITY_PAGE:
			fill_cd_capabilities_page();
			break;

		case MODE_WDC_OPERATIONS_PAGE:
			fill_operations_page();
			break;

		case MODE_ALL_PAGES:
			// Attach all mode pages in ascending page code order
			fill_control_page();
			fill_operations_page();
			fill_cd_capabilities_page();
			break;

		default:				// Invalid or unsupported Page Code.
			goto invalid_CDB_field;
		}
	}
	else
	{
		// Disk or SES logical unit.
		switch (cdb.mode_sense10.page_code_ctrl & MODE_PAGE_CODE_MASK)
		{
		case MODE_CONTROL_PAGE:
			goto _FILL_CONTROL_PAGE;

#ifdef SUPPORT_CACHING_MODE_PAGE			
		case MODE_CACHING_MODE_PAGE:
			if (logicalUnit == ses_lun)
				goto invalid_CDB_field;
			fill_caching_mode_page();
			break;
#endif
			
		case MODE_POWER_COND_PAGE:
			fill_power_condition_page();
			break;

		case MODE_WDC_DEVICE_CONFIG_PAGE:
			fill_device_config_page();
			break;

		case MODE_WDC_OPERATIONS_PAGE:
			fill_operations_page();
			break;

		case MODE_ALL_PAGES:
			// Attach all mode pages in ascending page code order
#ifdef SUPPORT_CACHING_MODE_PAGE
			if (logicalUnit != ses_lun)
				fill_caching_mode_page();
#endif
			fill_control_page();
			fill_power_condition_page();
			fill_device_config_page();
			fill_operations_page();
			break;

		default:
			goto invalid_CDB_field;
		}
	}

	return 0;

invalid_CDB_field:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
	return 1;
}

/************************************************************************\
 scsi_mode_sense_cmd() - MODE SENSE(6) and (10) command handler.

 */
void scsi_mode_sense_cmd(PCDB_CTXT pCtxt)
{
u8 deviceSpecific, blockDescLength;

//	MSG("mode sen: %bx\n", cdb.opcode);
//	dbg_next_cmd = 1;
	
	// Rearrange MODE SENSE(6) commands into MODE SENSE(10).
	if (cdb.opcode == SCSI_MODE_SENSE6)
	{
		// Move and extend the Allocation Length field.
		cdb.byte[7] = 0;
		cdb.byte[8] = cdb.byte[4];

		// The other fields (Page/Subpage Code, PC and DBD) are in the
		// same place for both commands, but clear the Long LBA Accepted
		// bit - MODE SENSE 6 always returns short block descriptors.
		cdb.mode_sense10.flags &= ~MODE_SENSE_LLBAA;
	}
#if 0//def DEBUG
printf("page_code_ctrl:%bx\nsub_page: %bx\n", cdb.mode_sense10.page_code_ctrl, cdb.mode_sense10.subpage_code);
#endif
#if 1
	// Fuzz: randomly return short block descriptors even if LLBAA.
	if ( ((pSataObj->sectorLBA & 0x7F00000000) == 0)  &&
	     ((cdb.mode_sense10.page_code_ctrl & MODE_PAGE_CODE_MASK) == MODE_ALL_PAGES) )
	{
		cdb.mode_sense10.flags &= ~MODE_SENSE_LLBAA;
	}
#endif

	// Clear some space in the buffer.
	// All the mode pages don't add up to 256 bytes, so this is plenty.
	xmemset(mc_buffer, 0, 256);
	nextDirection = DIRECTION_SEND_DATA;

	// Determine Device Specific Parameter value.
	if ((logicalUnit == HDD0_LUN) || (logicalUnit == HDD1_LUN))
	{
		deviceSpecific = 0x10;		// Set DPOFUA bit
#ifdef WRITE_PROTECT
		if (write_protect_enabled)
			deviceSpecific |= 0x80;		// set WP bit
#endif
	}
	else
	{
		// The SES and virtual CD-ROM sets this param to 0.
		deviceSpecific = 0;
	}

	// Construct the mode parameter list Header.
	// The Header structure is different for the 6- and 10-byte commands.
	if (cdb.opcode == SCSI_MODE_SENSE6)
	{	//construct header for 6-byte MODE SENSE command

		// Byte 0 is Mode Data Length; fill it in later.
		// Byte 1 is Medium type; already set to 0 for all devices.

		// Byte 2 is the Device Specific Parameter.
		mc_buffer[2] = deviceSpecific;

		// Byte 3 is Block Descriptor Length; this is set later.

		mode_page_ptr = &mc_buffer[4];	// Parameter pages start here. 
		byteCnt = 4;
	}
	else	// MODE SENSE(10)
	{
		// Bytes 0 & 1 is Mode Data Length; fill it in later.
		// Byte 2 is Medium type; already set to 0 for all devices.

		// Byte 3 is the Device Specific Parameter.
		mc_buffer[3] = deviceSpecific;
//		mc_buffer[4] = 0x01; // LongLBA bit is setted in the mode page header

		// Byte  4 has a flag that is already set to 0.
		// Bytes 6 & 7 is Block Descriptor Length; this is set later.

		mode_page_ptr = &mc_buffer[8];	// Parameter pages start here. 
		byteCnt = 8;
	}

	// Only the disk (and SES) LUNs support block descriptors;
	// return a block descriptor if the host didn't disallow it.
	//
	if ( (logicalUnit == VCD_LUN) ||
	     (cdb.mode_sense10.flags & MODE_SENSE_DBD) )
	{
		blockDescLength = 0;
	}
	else 
	{
		blockDescLength = fill_block_descriptor();
	}


	// Reject requests for subpages other than subpage 0 and "all subpages."
	// Our mode pages only have subpage 0.
	if ((cdb.mode_sense10.subpage_code != MODE_SUBPAGE_0)  &&
	     (cdb.mode_sense10.subpage_code != MODE_ALL_SUBPAGES))
	{
		goto invalid_CDB_field;
	}

	if (fill_apollo_mode_pages(pCtxt))  goto quit_now;

	// Whew, almost done!
	// Compute how much data is being returned...
	byteCnt = mode_page_ptr - mc_buffer;

	DBG("bytecnt %lx\n", byteCnt);

	// ...and fill out the mode parameter header accordingly.
	if (cdb.opcode == SCSI_MODE_SENSE6)
	{
		// Byte 0 is Mode Data Length
		mc_buffer[0] = byteCnt - 1;

		// Byte 3 is Block Descriptor Length.
		mc_buffer[3] = blockDescLength;
	}
	else
	{
		// Bytes 0 & 1 is Mode Data Length; setting the LSB is enough.
		mc_buffer[1] = byteCnt - 2;
		// Set the LongLBA bit if returning long LBA block descriptors.
		if (cdb.mode_sense10.flags & MODE_SENSE_LLBAA)
		{
			mc_buffer[4] |= BLOCK_DESCRIPTOR_LONGLBA;
		}
		// Bytes 6 & 7 is Block Descriptor Length; setting the LSB is enough.
		mc_buffer[7] = blockDescLength;
	}
	alloc_len = (cdb.mode_sense10.alloc_len[0] << 8) + cdb.mode_sense10.alloc_len[1];
quit_now:
	return;

invalid_CDB_field:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
	return;
}

/****************************************************************\
	SCSI Request sense Command for HDD, CDROM & SES

	returns information regarding parameters of the target & Lun.
\***************************************************************/
void scsi_request_sense_cmd(PSATA_OBJ  pSob, PCDB_CTXT	pCtxt)
{
	nextDirection = DIRECTION_SEND_DATA;	
	byteCnt = 18;
	alloc_len = CmdBlk(4);
	xmemset(mc_buffer, 0, 18);
	mc_buffer[0] = 0x70;
	mc_buffer[7] = 10;		// additional Sense Length

	if(curMscMode != MODE_UAS)
	{
		if (ata_passth_prepared)
		{
			ata_passth_prepared = 0;
			if (CmdBlk(1) & 0x01) //Desc bit is enable
			{
				bot_sense_data[0] = 22;
			}
			else
			{
				bot_sense_data[0] = 18;
#if 1  // simple way, it's just for atapassth_protocol 15
				// ATA_PASSTHR_INFOR_AVAILABLE
				bot_sense_data[1] = SCSI_SENSE_RECOVERED_ERROR;	// 2 sense key
				bot_sense_data[2] = 0x00; 							// 12 asc
				bot_sense_data[3] = 0x1D;							// 13 ASCQ
				sata_return_tfr(pSob, pCtxt);
				mc_buffer[3] = pCtxt->CTXT_FIS[FIS_ERROR];			// 3:6 information
				mc_buffer[4] = pCtxt->CTXT_FIS[FIS_STATUS];
				mc_buffer[5] = pCtxt->CTXT_FIS[FIS_DEVICE];
				mc_buffer[6] = pCtxt->CTXT_FIS[FIS_SEC_CNT];

				mc_buffer[9] = pCtxt->CTXT_FIS[FIS_LBA_HIGH];			// 8:11 cmd-specific information
				mc_buffer[10] = pCtxt->CTXT_FIS[FIS_LBA_MID];
				mc_buffer[11] = pCtxt->CTXT_FIS[FIS_LBA_LOW];

				u8 extend = 0;
				u8 seccnt_upper_nonzero = 0;
				u8 lba_upper_nonzero = 0;

				if (extended_descriptor)
					extend = 1;
				if (pCtxt->CTXT_FIS[FIS_SEC_CNT_EXT])
					seccnt_upper_nonzero = 1;
				if (*((u32 *) &(pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT])) & 0x00FFFFFF)
					lba_upper_nonzero = 1;
				// bits layout: extend|seccnt_upper_nonzero|lba_upper_nonzero|0| log_index = 0
				mc_buffer[8] = (extend << 7) | (seccnt_upper_nonzero << 6) | (lba_upper_nonzero << 5); 
#endif
			}
		}
	
		if (bot_sense_data[0] == 22)
		{	// extended Descriptor
			mc_buffer[0] = 0x72;		// response code
			mc_buffer[7] = 14;		// additional Sense Length

			sata_return_tfr(pSob, pCtxt);

			Ata_return_descriptor_sense(pCtxt, &mc_buffer[8]);
			byteCnt = 22;
		}
		else
		{
			if (CmdBlk(1) & 0x01) //Desc bit is enable
			{
				hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);	
				return;
			}
			else
			{
				if (bot_sense_data[0] == 18)
				{
					mc_buffer[2] =  bot_sense_data[1];	// sense code
					mc_buffer[12] = bot_sense_data[2];	// ASC
					mc_buffer[13] = bot_sense_data[3];	// ASCQ
				}
			}
		}
		bot_sense_data[0] = 0;
	}
	else if (CmdBlk(1) & 0x01) //Desc bit is enable
	{
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);	
		return;
	}
}
/************************************************************************\
 fill_product_id() - Writes the USB Product ID (PID) into a buffer.

 The USB PID is put into the buffer as four ASCII hexadecimal chars.

 */
void fill_product_id(u8 *dest)
{
#if 0//def DEBUG
printf("fill pid %x\n", product_detail.USB_PID);
#endif
u16 pid = product_detail.USB_PID;

	*dest++ = Hex2Ascii(pid >> 12);
	*dest++ = Hex2Ascii(pid >>  8);
	*dest++ = Hex2Ascii(pid >>  4);
	*dest++ = Hex2Ascii(pid);

}

/************************************************************************\
 fill_product_name() - Fills an output buffer with the product model name.

 Up to 16 bytes will be copied - the length of the Product ID field
 in standard Inquiry data.

 Prerequisites:
	- The output buffer is filled with spaces.
	  Usually ASCII fields have to be right-justified with spaces;
	  the caller must pre-fill the destination buffer with spaces.
 */
void fill_product_name(u8 *dest, u8 deviceType)
{
u8 pidIdx = 0;
#if 0//def DEBUG
printf("fill prod name: dev_type %bx\n", deviceType);
#endif
	// Caller must pre-fill the product ID field with spaces

	// No matter the condition, the destination buffer must be filled
	// with someting because callers count on finding at least one char.

	// The Product ID string is different for each LUN.
	//
	if ((deviceType == DEVICE_TYPE_DISK) || ((deviceType & 0x1F) == DEVICE_TYPE_UNKNOWN))
	{
		// Product Identification 
#ifdef INITIO_STANDARD_FW
		u8 *name = globalNvram.modelText;

		pidIdx = globalNvram.modelStrLength;
		if (pidIdx > 16)
			pidIdx = 16;
		xmemcpy((u8 *)name, dest, pidIdx);
#else
		char *name = product_detail.product_name;

		pidIdx = codestrlen((u8 *)name);
		xmemcpy((u8 *)name, dest, pidIdx);
#if 0//def DEBUG
		printf("product_detail.options %bx\n", product_detail.options);
#endif
		// Some products append the USB PID to the model name.
		pidIdx = (product_detail.options & PO_ADD_PID_TO_NAME)?  (pidIdx+1) : 0;
#if 0//def DEBUG
		printf("pidIdx %bx\n", pidIdx);
#endif
#endif
	}
	else if (deviceType == DEVICE_TYPE_CDROM)
	{
		static const char name[] = "Virtual CD";
		xmemcpy((u8 *)name, dest, sizeof(name)-1);

		// The USB PID is always appended to this LUN's name.
		pidIdx = sizeof(name);
	}
	else if (deviceType == DEVICE_TYPE_SES)
	{
		static const char name[] = "SES Device";
		xmemcpy((u8 *)name, dest, sizeof(name)-1);
	}
	else
	{	// Should never get here; device type is invalid/unsupported!
		dest[0] = '#';
		dest[1] = Hex2Ascii(deviceType >> 4);
		dest[2] = Hex2Ascii(deviceType);
		dest[3] = '?';
	}

#ifndef INITIO_STANDARD_FW
	if (pidIdx)
	{
		// Bounds-check the string index: output must be less than 16 chars.
		if (pidIdx <= 12)
		{
			// Put the 4-digit USB PID into the product ID string.
			fill_product_id(dest + pidIdx);
		}
		else
		{
			// Should never get here; the model name is too long!
			// Check the model name strings in  apollo.c
			dest[15] = '?';
		}
	}
#endif	
}

/************************************************************************\
 fill_serial_number() - Copies the HDD or unit serial # (as Stored SN)

 Up to HDD_SERIAL_NUM_LEN bytes will be copied.
 (The strange thing is that ATA HDD serials are 20 chars, but Apollo
 products' serial# fields are only 16 chars.)

 Prerequisites:
	- The output buffer is filled with spaces.
 */
void fill_serial_number(u8 *dest)
{
	xmemcpy(&unit_serial_num_str[0], dest,  HDD_SERIAL_NUM_LEN);
}

/************************************************************************\
 fill_variable_len_usn() - Copies the unit serial #  (Reported SN)

 	 the serial wil be of variable length 
 */
u8 fill_variable_len_usn(u8 *dest)
{
	u8 i = 0;
	for (i = 0; i < HDD_SERIAL_NUM_LEN; i++)
	{
		if (unit_serial_num_str[i] == 0x20)
			break;
		dest[i] = unit_serial_num_str[i];
	}

	if (i == (HDD_SERIAL_NUM_LEN -1))
		i = HDD_SERIAL_NUM_LEN;

	return i;
}

/************************************************************************\
 fill_standard_inquiry() - Fills in standard Inquiry data.

 Prerequisites:
	- the  mc_buffer  has been cleared except for byte 0, which contains
	  the Peripheral Device Type and Qualifier.
 */
void fill_standard_inquiry(void)
{
	// Byte 0 is the Peripheral Device Type/Qualifier, filled in by caller.

	// Pre-fill the ASCII and vendor-specific portions with spaces.
	xmemset(&mc_buffer[8], ' ', 48);

	// Set the RMB bit in byte 1 on the CD-ROM LUN
#ifdef WDC_VCD
	if (logicalUnit == cdrom_lun)
	{
		*(mc_buffer+1) = 0x80;			// Set the RMB bit
	}
#endif
#ifdef RMB_SWITCH  // for test
	if (rmb_enabled)
#endif		
#ifdef RMBDISK
	*(mc_buffer+1) = 0x80;
#endif

	mc_buffer[2] = INQUIRY_VERSION_SPC4;	// Byte 2 is the SPC version.
	mc_buffer[3] = 0x2 | 0x10;					// Byte 3: Response Data Format = 2, hierarchical support is enable

	// Byte 4 is the additional length, filled in later.

	// Set the EncServ bit in byte 6 on the SES LUN.
	if (logicalUnit == ses_lun)
	{
		mc_buffer[6] = 0x40;			// Set the EncServ bit
	}
	if (curMscMode == MODE_UAS)
		mc_buffer[7] = 0x02; // command QUE bit is set

	// Bytes 8..15 is the Vendor ID
#ifdef INITIO_STANDARD_FW
	u8 venlen = globalNvram.vendorStrLength;
	if (venlen > 8)
		venlen = 8;
	xmemcpy(globalNvram.vendorText, mc_buffer+8, venlen);
#else
	xmemcpy((u8 *)product_detail.vendor_name,  mc_buffer+8,
	         codestrlen((u8 *)product_detail.vendor_name));
#endif
	// Remainder of vendor ID field is already filled with spaces

	// Bytes 16..31 is the Product ID.
	fill_product_name(mc_buffer+16, mc_buffer[0] /*peripheral device type*/);

	// Bytes 32..35 is Firmware revision
	mc_buffer[32] = firmware_version_str[0];
	mc_buffer[33] = firmware_version_str[1];
	mc_buffer[34] = firmware_version_str[2];
	mc_buffer[35] = firmware_version_str[3];

	// WD-specific bytes 36..39: USB product ID
	fill_product_id(mc_buffer + 36);

#if 1  // Obsolete SN
	xmemset(mc_buffer+40, 0, 80);
#else
	// WD-specific bytes 40..55: serial number.
	// The serial # might overflow this field, so cleanup afterwards.
	fill_serial_number(mc_buffer + 40);
	xmemset(mc_buffer+56, 0, 64);		// Erase any overflow.
#endif	

	// Bytes 58..73 are eight 16-bit Version Descriptors.
	if ((logicalUnit == HDD0_LUN) || (logicalUnit == HDD1_LUN))
	{
		mc_buffer[58] = (u8)(VERSION_SPC4 >> 8);
		mc_buffer[59] = (u8)(VERSION_SPC4);
		mc_buffer[60] = (u8)(VERSION_SBC3 >> 8);
		mc_buffer[61] = (u8)(VERSION_SBC3);
		if (curMscMode == MODE_UAS)
		{
			mc_buffer[62] = (u8)(VERSION_UAS_R02>> 8);
			mc_buffer[63] = (u8)(VERSION_UAS_R02);			
		}
	}

	// The remaining descriptors are not used; already filled with 0.
	mc_buffer[4] = 76-5;      // Byte 4: additional length, add 2 bytes for the alignment of dword to pass WHQL
	byteCnt = 76;
}

/************************************************************************\
 fill_inquiry_vpd_page() - Fills in a Vital Product Data (VPD) page.

 Prerequisites:
	- the CDB is a valid INQUIRY command asking for a VPD page.
	- the  mc_buffer  has been cleared except for byte 0, which contains
	  the Peripheral Device Type and Qualifier.
 */
void fill_inquiry_vpd_page(PCDB_CTXT pCtxt)
{
	u8 tmp_user_bs_log2m9 = 0;
	// Fill in fields that are common to all VPD pages.
	// Byte 0 is the Peripheral Device Type/Qualifier, filled in by caller.
	mc_buffer[1] = cdb.inquiry.page_code;  // Byte 1: page code

	// Byte 2 is reserved.

	switch (cdb.inquiry.page_code)
	{
	case INQUIRY_SUPPORTED_PAGES:
		if ((logicalUnit == HDD0_LUN) || (logicalUnit == HDD1_LUN))
		{
			mc_buffer[3] = 7;			// Page Length

			// List supported VPD pages in ascending page code order.
			mc_buffer[4] = INQUIRY_SUPPORTED_PAGES;
			mc_buffer[5] = INQUIRY_UNIT_SERIAL_NUMBER;
			mc_buffer[6] = INQUIRY_DEVICE_ID;
			mc_buffer[7] = INQUIRY_BLOCK_LIMITS;  // Disk LUN only.
			mc_buffer[8] = INQUIRY_BLOCK_DEVICE_CHARACTERISTICS; // Disk Lun only
			mc_buffer[9] = INQUIRY_WDC_ACTIVE_IFACES;
			//mc_buffer[10] = INQUIRY_WDC_RAW_CAPACITY;
			mc_buffer[10] = INQUIRY_WDC_FACTORY_CONFIGURE;
		}
		else if (logicalUnit == UNSUPPORTED_LUN)
		{
			mc_buffer[3] = 1;
			mc_buffer[4] = INQUIRY_SUPPORTED_PAGES;
		}
		else
		{
			// List VPD pages supported by the CD-ROM and SES LUNs.
			mc_buffer[3] = 4;			// Page Length
			mc_buffer[4] = INQUIRY_SUPPORTED_PAGES;
			mc_buffer[5] = INQUIRY_UNIT_SERIAL_NUMBER;
			mc_buffer[6] = INQUIRY_DEVICE_ID;
			mc_buffer[7] = INQUIRY_WDC_ACTIVE_IFACES;
			//mc_buffer[8] = INQUIRY_WDC_RAW_CAPACITY;
		}

		byteCnt = mc_buffer[3] + 4;

		break;

	case INQUIRY_BLOCK_LIMITS:			// Block Limits page
		// This page is defined by SBC-3, so only the Disk LUN supports it.
		if ((logicalUnit != HDD0_LUN) && (logicalUnit != HDD1_LUN))
		{
			hdd_err_2_sense_data(pCtxt, ERROR_ILL_PARAM);
			break;
		}

		mc_buffer[3] = 60;				// Page length

		// Bytes 6..7 is the Optimal Transfer Length Granularity.
		// Set this to the # of logical blocks in a physical block.

		// Using byte 7 as a temporary, set it to the HDD's physical
		// block size exponent (either assumed or actual size)...
#if WDC_ALWAYS_REPORT_4K_PHYSICAL
		mc_buffer[7] = 12;				// 2**12 == 4096
#else
		mc_buffer[7] = pSataObj->physicalBlockSize_log2;
#endif
		if ((logicalUnit == HDD0_LUN) && (sata0Obj.sobj_device_state != SATA_NO_DEV))
			tmp_user_bs_log2m9 = vital_data[0].user_bs_log2m9;
		else
			tmp_user_bs_log2m9 = vital_data[1].user_bs_log2m9;
				
		// Compare the physical block size to the current logical block size.
		if (mc_buffer[7] > (tmp_user_bs_log2m9 + 9))
		{
			// There's more than one logical block in each physical block,
			// so the "optimal" transfer length is one physical block.
			// Compute the # of logical blocks in each physical block.
			mc_buffer[7] = (1 << (mc_buffer[7]) - (tmp_user_bs_log2m9 +9));
		}
		else
		{
			// The current logical block size is same or larger than a
			// physical block, so the optimal xfer length is one logical block.
			// (This is OK as long as all block lengths are powers-of-2.)
			mc_buffer[7] = 1;
		}

		// Bytes 8..11 is the Maximum Transfer Length for read/write cmds.
		// Set this to the largest xfer length accepted by
		// scsi_xlate_rw16_lba_for_hdd(). This is the same same as:
		//  *(u32*)(mc_buffer+8) = (unsigned)0xffff >> IniData.user_bs_log2m9;
		// Bytes 8 and 9 are already 0.
		u8 bs_overflow_mask = 0xff00 >> vital_data[0].user_bs_log2m9;
		if (arrayMode == JBOD)
		{
			// in JBOD mode, if the SATA0 is removed, report the block limits information following's SATA1
			if ((logicalUnit == HDD1_LUN) || (sata0Obj.sobj_device_state == SATA_NO_DEV))
			{
				bs_overflow_mask = 0xff00 >> vital_data[1].user_bs_log2m9;
			}
		}
		
		mc_buffer[10] = 0xff & ~bs_overflow_mask;//bs_overflow_mask;
		mc_buffer[11] = 0xff;

		// Bytes 12..15 is the Optimal Transfer Length.
		*(u32*)(mc_buffer+12) = *(u32*)(mc_buffer+8);	// Same as the max xfer len.

		// Bytes 16..19 is the max transfer length for XDREAD, XDWRITE, etc.
		*(u32*)(mc_buffer+16) = *(u32*)(mc_buffer+8);

		byteCnt = 64;
		break;

	case INQUIRY_BLOCK_DEVICE_CHARACTERISTICS:
		// This page is defined by SBC-3, so only the Disk LUN supports it.
		if ((logicalUnit != HDD0_LUN) && (logicalUnit != HDD1_LUN))
		{
			hdd_err_2_sense_data(pCtxt, ERROR_ILL_PARAM);
			break;
		}
		mc_buffer[3] = 60;				// Page length

		//logic unit == disk number??   ram_rp[0] or ram_rp[1] ???FIX_ME
		if (arrayMode == JBOD)
		{
			mc_buffer[4] = (u8)(ram_rp[logicalUnit].disk[ram_rp[logicalUnit].disk_num].mediumRotationRate>>8);
			mc_buffer[5] = (u8)ram_rp[logicalUnit].disk[ram_rp[logicalUnit].disk_num].mediumRotationRate;
			mc_buffer[7] = ram_rp[logicalUnit].disk[ram_rp[logicalUnit].disk_num].nominalFormFactor;
		
		}
		else
		{
			if(ram_rp[0].disk[ram_rp[0].disk_num].mediumRotationRate ==  ram_rp[1].disk[ram_rp[1].disk_num].mediumRotationRate)
			{
				mc_buffer[4] = (u8)(ram_rp[0].disk[ram_rp[0].disk_num].mediumRotationRate>>8);
				mc_buffer[5] = (u8)ram_rp[0].disk[ram_rp[0].disk_num].mediumRotationRate;
			}
			
			if(ram_rp[0].disk[ram_rp[0].disk_num].nominalFormFactor ==  ram_rp[1].disk[ram_rp[1].disk_num].nominalFormFactor)
			{
				mc_buffer[7] = ram_rp[0].disk[ram_rp[0].disk_num].nominalFormFactor;
			}
		}
				
		// the rest value shall be return 0
		byteCnt = 64;
		break;		

	case INQUIRY_UNIT_SERIAL_NUMBER:
#if 1
		mc_buffer[3] = fill_variable_len_usn(mc_buffer + 4);

		byteCnt = mc_buffer[3] + 4;				// Page length
#else
		mc_buffer[3] = 16;				// Page length

		// Pre-fill in the ASCII portion with spaces
		xmemset(&mc_buffer[4], ' ', 16);

		fill_serial_number(mc_buffer + 4);

		byteCnt = 20;
#endif		
		break;

	case INQUIRY_DEVICE_ID:
		mc_buffer[3] = 12;                // Page length
		// First identification descriptor: NAA descriptor
		mc_buffer[4+0] = 0x01 ;           //proto id= 0, code set = binary
		mc_buffer[4+1] = 0x03 ;           //piv=0, assoc=LUN, type=NAA
		mc_buffer[4+2] = 0 ;              //Reserved
		mc_buffer[4+3] = 8;                //identifier length

		// Bytes 4..11 in the descriptor is the NAA designator from product information
		// if there's no saved product information, shall initialize the random data for wwn
		// 
		if (logicalUnit == HDD0_LUN)
		{
			xmemcpy(prodInfo.disk_wwn, &mc_buffer[4+4], 8);
		}
		else if (logicalUnit == HDD1_LUN)
		{
			xmemcpy(prodInfo.disk1_wwn, &mc_buffer[4+4], 8);
		}
		else if (logicalUnit == VCD_LUN)
		{
			xmemcpy(prodInfo.vcd_wwn, &mc_buffer[4+4], 8);
		}
		else
		{
			xmemcpy(prodInfo.ses_wwn, &mc_buffer[4+4], 8);	
		}
		
		byteCnt = 16;
		break;

	case INQUIRY_WDC_ACTIVE_IFACES:		// WD Active Interfaces page
		mc_buffer[3]= 16;				// Page length

		// First and only Interface Descriptor 
		mc_buffer[4+0] = 0x80 ;			// Bit 8 is Active interface
		mc_buffer[4+1] = 'U' ;			// active Interface name: USB 2/3.0
		mc_buffer[4+2] = 'S' ;	
		mc_buffer[4+3] = 'B' ;	
		if (*usb_DevStatus_shadow & USB3_MODE)
			mc_buffer[4+4] = '3' ;	
		else
			mc_buffer[4+4] = '2' ;	
		mc_buffer[4+5] = '.' ;	
		mc_buffer[4+6] = '0' ;
//		mc_buffer[4+7] = ' ' ;
		
		mc_buffer[4 + 8 +0] = 0x00 ;		// Bit 8 is Active interface
		mc_buffer[4 + 8 +1] = 'U' ;			// supported interface
		mc_buffer[4 + 8 +2] = 'S' ;	
		mc_buffer[4 + 8 +3] = 'B' ;	
		if (*usb_DevStatus_shadow & USB3_MODE)
			mc_buffer[4 + 8 +4] = '2' ;	
		else
			mc_buffer[4 + 8 +4] = '3' ;	
		mc_buffer[4 + 8 +5] = '.' ;	
		mc_buffer[4 + 8 +6] = '0' ;
//		mc_buffer[4 + 8 +7] = ' ' ;

		byteCnt = 20;
		break;
#if 0
	case INQUIRY_WDC_RAW_CAPACITY:
		mc_buffer[3] = 20;				// Page Length

		mc_buffer[4] = 0x30;			// Signature.
		mc_buffer[6] = 1;				// Max # of disks.
		mc_buffer[7] = 1;				// Disks currently installed.

		// Bytes 8..15 is Total Number of Blocks.
//		*((u64 *)(mc_buffer + 8)) = pSataObj->sectorLBA;
		WRITE_U64_BE(&mc_buffer[8], pSataObj->sectorLBA);

		// Bytes 16..19 is the Block Length that goes with 
		// the Total # of Blocks; this is always 512.
		mc_buffer[18] = (512 >> 8);

		// Bytes 20..23 is the Preferred Block Length;
		// this is the HDD's physical block size.
#if WDC_ALWAYS_REPORT_4K_PHYSICAL
//		*(u32*)(mc_buffer+20) = 4096;
		WRITE_U32_BE(&mc_buffer[20], 4096);
		
#else
//		*(u32*)(mc_buffer+20) = (u32)1 << pSataObj->physicalBlockSize_log2;
		WRITE_U32_BE(&mc_buffer[20], (u32)1 << pSataObj->physicalBlockSize_log2);
#endif

		byteCnt = 24;
		break;
#endif
	case INQUIRY_WDC_FACTORY_CONFIGURE:
		mc_buffer[3] = 80;				// Page Length

		mc_buffer[4] = 0x30;			// Signature.
		mc_buffer[5] |= productInfo_freeze;		// FROZEN (bit0)
		mc_buffer[7] = 0x00;			// SLOW ENUM ONCE=0 (bit0)

		if (!(prodInfo.new_VID[0] || prodInfo.new_VID[1] || prodInfo.new_PID[0] || prodInfo.new_PID[1]))
		{
			mc_buffer[8] = (u8)(product_detail.USB_VID >> 8);
			mc_buffer[9] = (u8)product_detail.USB_VID;
			mc_buffer[10] = (u8)(product_detail.USB_PID >> 8);
			mc_buffer[11] =(u8)product_detail.USB_PID;
		}
		else
		{
			mc_buffer[8] = prodInfo.new_VID[0];
			mc_buffer[9] = prodInfo.new_VID[1];
			mc_buffer[10] = prodInfo.new_PID[0];
			mc_buffer[11] = prodInfo.new_PID[1];
		}

		mc_buffer[12 + 0] = 0x42;		// signature

		xmemcpy(prodInfo.disk_wwn, mc_buffer+12+4, 8);
		xmemcpy(prodInfo.disk1_wwn, mc_buffer+12+12, 8);
		xmemcpy(prodInfo.vcd_wwn, mc_buffer+12+20, 8);
		xmemcpy(prodInfo.ses_wwn, mc_buffer+12+28, 8);
		xmemcpy(prodInfo.container_ID, mc_buffer+12+36, 16);
		xmemcpy(prodInfo.iSerial, mc_buffer+12+52, 20);
		
		byteCnt = 84;
		break;

	default:			// Invalid or unsupported VPD page code
ILLEGAL_CDB:
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
		break;
	}
	return;
}
/************************************************************************\
 scsi_inquiry_cmd() - INQUIRY command handler.
 */
void scsi_inquiry_cmd(PCDB_CTXT pCtxt, u8 inqDeviceType)
{
enum {
	EVPD_AND_CMDDT = INQUIRY_EVPD | INQUIRY_CMDDT
};

	MSG("inq %bx\n", logicalUnit);
//	dbg_next_cmd = 1;
	// Zero out the buffer; 250 bytes is plenty for all inquiries.
	xmemset(mc_buffer, 0, 250);

	// Fill in fields that are common to all inquiries:
	// Byte 0 is the Peripheral Device Type and Qualifier.
	mc_buffer[0] = inqDeviceType;
	if (logicalUnit == UNSUPPORTED_LUN)
		mc_buffer[0] |= 0x60; // PERIPHERAL QUALIFIER for Unsupported LUN
	nextDirection = DIRECTION_SEND_DATA;
	switch (cdb.inquiry.flags & EVPD_AND_CMDDT)
	{
	case 0:				// No flags set: return standard inquiry data.
		// The Page Code must be 0.
		if (cdb.inquiry.page_code)
			goto _ILLEGAL_CDB;
		else
		{
			fill_standard_inquiry();
		}
		break;

	case INQUIRY_EVPD:
		// Return a Vital Product Data (VPD) page.
		if (logicalUnit == UNSUPPORTED_LUN)
		{
			// the unsupported LUN only support the "INQUIRY_SUPPORTED_PAGES"
			if (cdb.inquiry.page_code != INQUIRY_SUPPORTED_PAGES)
				goto _ILLEGAL_CDB;
		}
		fill_inquiry_vpd_page(pCtxt);
//		dbg_next_cmd = 1;
		break;

	default:			// Invalid or unsupported flags.
_ILLEGAL_CDB:
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
		break;
	}
	alloc_len = (cdb.inquiry.alloc_len[0] << 8) + cdb.inquiry.alloc_len[1];
}
// if the slot has no device pluged in or the device is power off, return 1
// otherwise return 0
static u8 is_otherSlot_pwrDown(PSATA_OBJ pSob)
{
	if (pSob->sobj_sata_ch_seq)
	{// current slot is Sata0, the other slot is Slot 0
		pSataObj = &sata0Obj;
	}
	else
	{
		pSataObj = &sata1Obj;
	}
	// the other slot is not attached device or is power down
	if ((pSataObj->sobj_device_state != SATA_NO_DEV) || (pSataObj->sobj_State == SATA_STANDBY)  || (pSataObj->sobj_State == SATA_PWR_DWN))
	{
		return 1;
	}
	else 
	{
		return 0;
	}
}
/************************************************************************\
 scsi_start_stop_cmd() - START/STOP UNIT command handler.

 This is only used by the disk and SES logical units.
 The virtual CD-ROM has its own command handler.

 */
void scsi_start_stop_cmd(PCDB_CTXT pCtxt)
{
#define SSU_POWER_COND_MODIFIER_MASK  0x0f
	// The only accepted Power Condition Modifier for any of our
	// supported power conditions is 0.
	if (cdb.start_stop_unit.pc_modifier & SSU_POWER_COND_MODIFIER_MASK)
	{
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
		return;
	}
	nextDirection = DIRECTION_SEND_STATUS;
	// Check the Power Condition field.
	//
	switch (cdb.start_stop_unit.pc_action & SSU_POWER_COND_MASK)
	{
	case POWER_COND_START_VALID:		// Process LoEj and Start bits
		// Check the LoEj (Load/Eject) and Start bits.
		switch (cdb.start_stop_unit.pc_action & SSU_START_LOEJ_MASK)
		{
		case SSU_STOP:
			// Spin down the HDD and then adjust the power/activity LED.
			// Flush the disk's cache...
			// for JBOD, the SSU command spin down or spin up the corresponding channel
			if (arrayMode != JBOD)
			{
				if (((sata0Obj.sobj_State == SATA_STANDBY)  || (sata0Obj.sobj_State == SATA_PWR_DWN))
				&& ((sata1Obj.sobj_State == SATA_STANDBY)  || (sata1Obj.sobj_State == SATA_PWR_DWN)))
					return;
				else
				{
					ata_shutdown_hdd(TURN_POWER_OFF, SATA_TWO_CHs);
					set_activity_for_standby(1);
#if FAN_CTRL_EXEC_TICKS
					enableFan(0);
#endif
				}
			}
			else
			{
				if ((pSataObj->sobj_State == SATA_STANDBY)  || (pSataObj->sobj_State == SATA_PWR_DWN))
				{
					// check the other slot's state
					if (is_otherSlot_pwrDown(pSataObj))
					{
						set_activity_for_standby(1);
					}
					return;
				}
				else
				{
					ata_shutdown_hdd(TURN_POWER_OFF, (pSataObj->sobj_sata_ch_seq) ? SATA_CH1 : SATA_CH0);
					// check the other slot's state
					if (is_otherSlot_pwrDown(pSataObj))
					{
						set_activity_for_standby(1);
					}
				}
			}
			break;

		case SSU_START:
			// Do the reverse of STOP: bring the activity LED out of
			// standby mode blinking first, so that it will show
			// disk activity as the HDD is spining up.
_SSU_START:
			set_activity_for_standby(0);
			// Initialize the HDD if it is powered off right now.
			// The HDD should be up and spinning now, but spin it again
			// anyway to blink the activity LED and reset the standby timer.
			if (arrayMode != JBOD)
			{
				if (((sata0Obj.sobj_State == SATA_STANDBY)  || (sata0Obj.sobj_State == SATA_PWR_DWN))
				&& ((sata1Obj.sobj_State == SATA_STANDBY)  || (sata1Obj.sobj_State == SATA_PWR_DWN)))
				{
					if (cdb.start_stop_unit.immed)
					{
						ata_startup_hdd(0, SATA_TWO_CHs);
						MSG("CTXT_FLAG %bx\n", pCtxt->CTXT_FLAG);
					}
					else
					{
						if (ata_startup_hdd(1, SATA_TWO_CHs))
						{
							hdd_err_2_sense_data(pCtxt, ERROR_UA_BECOMING_READY);
						}
					}
				}
			}
			else
			{
				if ((pSataObj->sobj_State == SATA_STANDBY)  || (pSataObj->sobj_State == SATA_PWR_DWN))// in the state < SATA_STANDBY, the command can't be sent out
				{
					ata_startup_hdd(0, SATA_TWO_CHs);
					if (cdb.start_stop_unit.immed == 0)
					{
						pCtxt->CTXT_CONTROL = CTXT_CTRL_DIR; // set to data in
						MSG("%bx\n", pCtxt->CTXT_Index);
						pCtxt->CTXT_FLAG = CTXT_FLAG_U2B;
					}
					else
					{
						pCtxt->CTXT_FLAG = 0;
					}
					
					spinup_timer = 0; // doesn't trigger 7secs time to response device not ready
					ata_ExecUSBNoDataCmd(pSataObj, pCtxt, ATA6_IDLE_IMMEDIATE);
					nextDirection = DIRECTION_NONE;
					if (cdb.start_stop_unit.immed)
					{
						MSG("CTXT_FLAG %bx\n", pCtxt->CTXT_FLAG);
						pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
						bot_device_no_data(pCtxt);
					}
				}								
			}
			break;

#if 0	// No need to do anything for Eject and Load.
		case SSU_EJECT:
		case SSU_LOAD:
			break;
#endif
		}
		break;

	case POWER_COND_ACTIVE:
	case POWER_COND_IDLE:
		// ...except the standby condition timer is disabled.
		if (arrayMode != JBOD)
			disable_upkeep |= (UPKEEP_HOST_CONTROLS_POWER_CONDITION_S0|UPKEEP_HOST_CONTROLS_POWER_CONDITION_S1);
		else 
		{
			disable_upkeep |= ((pSataObj->sobj_sata_ch_seq)? UPKEEP_HOST_CONTROLS_POWER_CONDITION_S1 : UPKEEP_HOST_CONTROLS_POWER_CONDITION_S0);
		}
		
		goto _SSU_START;

	case POWER_COND_STANDBY:
	case POWER_COND_FORCE_STANDBY_0:
		// Spin down the HDD and then adjust the power/activity LED.
		// ...then spin it down.
		if (arrayMode != JBOD)
		{
			ata_shutdown_hdd(TURN_POWER_OFF, SATA_TWO_CHs);
			set_activity_for_standby(1);
#if FAN_CTRL_EXEC_TICKS
			enableFan(0);
#endif
		}
		else
		{
			if (pSataObj->sobj_State != SATA_PWR_DWN)
			{
				ata_shutdown_hdd(TURN_POWER_OFF, (pSataObj->sobj_sata_ch_seq) ? SATA_CH1 : SATA_CH0);
			}

			// check the other slot's state
			if (is_otherSlot_pwrDown(pSataObj))
			{
				set_activity_for_standby(1);
			}
		}
		break;

	case POWER_COND_LU_CONTROL:
		// This command tells the logical unit (i.e., us) to resume
		// managing our power condition. It does not change the
		// the HDD's power state.

		// Enable and reset the standby timer if it was disabled.
		// If the timer was unilaterally reset, then a sequence of
		// this commands would prevent the HDD from being spun down
		// even though this command doesn't access the HDD.
		// 
		if (arrayMode != JBOD)
		{
			if (!(disable_upkeep & (UPKEEP_HDD_SPUN_DOWN_S0 | UPKEEP_HDD_SPUN_DOWN_S1))  &&
			     (disable_upkeep & (UPKEEP_HOST_CONTROLS_POWER_CONDITION_S0 |UPKEEP_HOST_CONTROLS_POWER_CONDITION_S1)))
			{
				reset_standby_timer();
			}
			disable_upkeep &= ~(UPKEEP_HOST_CONTROLS_POWER_CONDITION_S0 | UPKEEP_HOST_CONTROLS_POWER_CONDITION_S1);
		}
		else
		{
			u8 tmp_ch_flg = ((pSataObj->sobj_sata_ch_seq) ? 0: 1);
			if (!(disable_upkeep & (UPKEEP_HDD_SPUN_DOWN_S0 << tmp_ch_flg))  &&
			     (disable_upkeep & (UPKEEP_HOST_CONTROLS_POWER_CONDITION_S0 << tmp_ch_flg)))
			{
				reset_standby_timer();
			}

			// We (the device) manages our power conditions now.
			disable_upkeep &= ~(UPKEEP_HOST_CONTROLS_POWER_CONDITION_S0 << tmp_ch_flg);
		}
		break;

	// case POWER_COND_SLEEP:
	// case POWER_COND_FORCE_IDLE_0:
	default:	// Invalid or unsupported Power Condition.
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
		break;
	}

}

#ifndef INITIO_STANDARD_FW
u8 chk_wwn_action(u8 para)
{
	//action 2 shall be rejected by Interlaken device server firmware as an invalid 
	// if ((para == 0) || (para == 1) || (para == 2)) return (1 << para);
	if ((para == 0) || (para == 1)) return (1 << para);
	else if (para == 0x50) return 0x8;
	else
		return 0;
}
u16 chk_wwn_action_all_luns(u32 wwn_para)
{
	u16 tmp16 = 0; 	
	for (u8 i =0; i < 4; i++)  // supported 4 wwns
	{
		tmp16 |= (chk_wwn_action((u8)wwn_para) << (4*i));
		wwn_para = wwn_para >> 8;
	}
	return tmp16;
}
void fill_wwn_all_luns(u8 *wwn_buf)
{
	u8 *ptr = prodInfo.disk_wwn;
	for (u8 i = 0; i < 4; i++, wwn_buf +=8, ptr+=8)
	{
		if (xmemcmp(wwn_buf, ptr, 8) == 1)
		{
			if (wwn_buf[0] == 0)
				continue;
			else
			{
				if (wwn_buf[0] == 0x50)
					xmemcpy(wwn_buf, ptr, 8);
				else 
					xmemset(ptr, 0, 8);
			}
		}
	}
}

u8 chk_usn(u8 *usn)
{
	u8 tmp = USN_NOT_VALID;
	
	if (usn[0] == 0x00)  // USN no change
		tmp = USN_NO_CHANGE;
	else if (usn[0] == 0x01)  // USN erase
		tmp = USN_IS_ERASED;
	else		// new USN
	{
		if ((usn[0] > 0x20) && (usn[0] < 0x7F)) // range of 0x21 to 0x7E
		{
			for (u8 i = 1; i < HDD_SERIAL_NUM_LEN; i++)
			{
				if ((usn[i] > 0x19) && (usn[i] < 0x7F))  // 0x20 or 0x21~0x7E
				{
					if ((usn[i] == 0x20) && (i < 19))
					{
						for (u8 j=i+1; j<HDD_SERIAL_NUM_LEN; j++)
						{
							if (usn[j] != 0x20)
								goto usn_invalid;
						}
					}
				}
				else
					goto usn_invalid;
			}

			tmp = USN_IS_VALID;
		}
			
	}
usn_invalid:	

	return tmp;
}
/************************************************************************\
factory_config(PCDB_CTXT	pCtxt) - SCSI_WDC_FACTORY_CONFIG
 update the factory_config command in v1.014 2/21/2011
(1) Change the product configuration ("Soft strap" the drive) which also resets parameters back to factory default for the new configuration.
(2) Reset parameters back to factory default without changing the current product configuration (PID/VID is not changed).
     F/W sets a temp flag to indicate that the parameters should be setted to the default value.
(3) Perform a Slow Enumeration on the next enumeration
     Keep the original configuration and paramters.
(4) Nothing (NOP-like operation)

WD update the spec to set the WWN as well in 08/08/2012
(1) Data parameters format: 
     [0]  		signature 0x42
     [4-11] 	Disk1 WWN
     [12-19]	Disk2 WWN
     [20-27]	VCD WWN
     [28-35]	SES WWN
     [36-51]	Container ID 
     [52-71]	Unit Serial Number
(2) WWN supported actions
	NAA 	ACTION
	0		    0			Don't change LUN's WWN
	0		    1			Erase LUN's WWN
	0		    2			Derive logical unit's WWN from HDD's WWN it shall be rejected by Interlaken device server firmware as an invalid 
	5		    *			Use WWN as-is for LUN's WWN
 ************************************************************************/
void factory_config(PCDB_CTXT	pCtxt)
{
	#define SLOW_ENUM_BIT 	0x01
	#define PERM_BIT		0x01	// Obsolete44
	#define RE_ENUM_BIT		0x02
	
	u32 l_byteCnt = (CmdBlk(8) << 8) + CmdBlk(9);

	if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
	{
		// The SIGNATURE shall be set to 0x30
		if ((CmdBlk(1) != FACTORY_FREEZE) && (CmdBlk(1) != FACTORY_CONFIGUR))
			goto invalid_CDB_field;

		if ((IS_LOCKED(1) && (arrayMode == JBOD)) || (IS_LOCKED(0)))
			goto data_protect_error;

		if (CmdBlk(11) != 0)
			goto invalid_CDB_field;

		// make sure it's no data command for factory freeze subcommand
		if ((CmdBlk(1) == FACTORY_FREEZE) && (l_byteCnt))
			goto invalid_CDB_field;

		// Interlaken does not support the functionality of the SLOW ENUM ONCE bit
		if (CmdBlk(3) & SLOW_ENUM_BIT)
			goto invalid_CDB_field;
	}
	
	if (scsi_chk_hdd_pwr(pCtxt)) 	return;	
	if (scsi_start_receive_data(pCtxt, l_byteCnt)) return;
	
	u16 fc_config_ctrl = 0;
		#define SAME_CONFIGURATION	0x01
		#define ZERO_VIDPID				0x02
		#define RESET_PARAMETERS		0x04
		#define NO_CHANGE_TO_FLASH	0x08
//		#define SLOW_ENUM_ONCE_BIT	0x10
//		#define PERM_BIT_IN_CDB		0x20
		#define NO_CHANGE_USN			0x10
		#define ZERO_CONTAINER_ID		0x20 
		#define RE_ENUM_IN_CDB			0x40
		#define CLEAR_SN				0x80

	u16 wwn_action_tmp = 0;
	
	// c3 30 01 00 [xx xx xx xx] 00 00 00
	nextDirection = DIRECTION_SEND_STATUS;

	if (CmdBlk(1) == FACTORY_FREEZE)
	{
		productInfo_freeze = 1;
		prodInfo.factory_freezed = 1;
		goto _SAVE_PROD_INFO;
	}

	if (l_byteCnt)
	{
		// when there's data payload, check if the data pattern is valid
		if (mc_buffer[0] != 0x42)
			goto invalid_param_field;
		u32 tmp32 = mc_buffer[4] + (mc_buffer[12] << 8) + (mc_buffer[20] << 16) + (mc_buffer[28] << 24);
		MSG("%lx\n", tmp32);
		wwn_action_tmp = chk_wwn_action_all_luns(tmp32);
		MSG("wnt %x\n", wwn_action_tmp);
		if (((wwn_action_tmp & 0x0F) == WWN_NOT_VALID) 
			|| ((wwn_action_tmp & 0xF0) == WWN_NOT_VALID) 
			|| ((wwn_action_tmp & 0xF00) == WWN_NOT_VALID)
			|| ((wwn_action_tmp & 0xF000) == WWN_NOT_VALID)) // check if the parameters are valid
			goto invalid_param_field;

		// recognize a Container ID of all zeros to indicate that the FACTORY CONFIGURE command shall not change the existing
		// if (!((*(u64*)&mc_buffer[36]) || (*(u64*)&mc_buffer[44])))   // 36...51  may alignment issue
		if (!(mc_buffer[36] || mc_buffer[37] || mc_buffer[38] || mc_buffer[39] || 
			mc_buffer[40] || mc_buffer[41] || mc_buffer[42] || mc_buffer[43] || 
			mc_buffer[44] || mc_buffer[45] || mc_buffer[46] || mc_buffer[47] ||
			mc_buffer[48] || mc_buffer[49] || mc_buffer[50] || mc_buffer[51]))
			fc_config_ctrl |= ZERO_CONTAINER_ID;

		// Unit Serial Number's action code check
		u8 usn_tmp = chk_usn(&mc_buffer[52]);
		if (usn_tmp == USN_NOT_VALID)
			goto invalid_param_field;
		else if (usn_tmp == USN_NO_CHANGE)
			fc_config_ctrl |= NO_CHANGE_USN;
		else if (usn_tmp == USN_IS_ERASED)
			fc_config_ctrl |= CLEAR_SN;
	}

	//The NEW VENDOR ID and NEW PRODUCT ID fields are both zero, and the slow_enum bit is not setted, 
	// the command shall succeed w/o effective result
	if (!(CmdBlk(4) || CmdBlk(5) || CmdBlk(6) || CmdBlk(7)))
		fc_config_ctrl |= ZERO_VIDPID;
		
	// If the NEW VENDOR ID and NEW PRODUCT ID correspond to the device's current identity, 
	// F/W will reset the parameters to default value
	if ((CmdBlk(4) == (u8)(product_detail.USB_VID >> 8)) &&
		(CmdBlk(5) == (u8)product_detail.USB_VID) &&
		(CmdBlk(6) == (u8)(product_detail.USB_PID >> 8)) &&
		(CmdBlk(7) == (u8)product_detail.USB_PID))
	{
		fc_config_ctrl |= SAME_CONFIGURATION;
	}

	if (CmdBlk(2) & RE_ENUM_BIT)
		fc_config_ctrl |= RE_ENUM_IN_CDB;

#if 0  // Interlaken does not support the functionality of the SLOW ENUM ONCE bit
	if (CmdBlk(3) & SLOW_ENUM_BIT)
		fc_config_ctrl |= SLOW_ENUM_ONCE_BIT;
#endif	
	
	// check th enew vid & pid is valid in the supported list
	u16 new_vid = (CmdBlk(4) << 8) + CmdBlk(5);	
	u16 new_pid = (CmdBlk(6) << 8) + CmdBlk(7);

	u8 prodIdx = check_product_config(new_vid, new_pid);
//	if (fc_config_ctrl & PERM_BIT_IN_CDB) // remove the perm bit setting check in CDB, obsolte field, use in Spec V0.87
	if (fc_config_ctrl & ZERO_VIDPID)
	{
		// save perm bit in flash	
		if ((fc_config_ctrl & (NO_CHANGE_USN | ZERO_CONTAINER_ID)) == (NO_CHANGE_USN | ZERO_CONTAINER_ID))		
			fc_config_ctrl |= NO_CHANGE_TO_FLASH;
	}
	else
	{
		if ((fc_config_ctrl & SAME_CONFIGURATION) == 0)
		{
			if (prodIdx == ILLEGAL_BOOT_UP_PRODUCT)
				goto invalid_CDB_field;
			if (check_compability_product(prodIdx))
				goto invalid_CDB_field;
			if (productInfo_freeze)
				goto error_permanent_write;
		}
		else
		{
			// VID, PID = default VID & PID
			if ((CmdBlk(4) != prodInfo.new_VID[0]) ||
				(CmdBlk(5) != prodInfo.new_VID[1]) ||
				(CmdBlk(6) != prodInfo.new_PID[0]) ||
				(CmdBlk(7) != prodInfo.new_PID[1]))
			{
				prodInfo.new_VID[0] = CmdBlk(4);
				prodInfo.new_VID[1] = CmdBlk(5);
				prodInfo.new_PID[0] = CmdBlk(6);
				prodInfo.new_PID[1] = CmdBlk(7);
			}
		}
		fc_config_ctrl |= RESET_PARAMETERS;
	}

	u8 save_prod_info = 0;
	u16 saved_wwn_action = wwn_action;
	// check the WWN and save them to flash
	if ((l_byteCnt) && ((wwn_action_tmp & WWN_NO_CHANGE_ALL_LUNS) != WWN_NO_CHANGE_ALL_LUNS))
	{
		if (productInfo_freeze)
			goto error_permanent_write;
		if (wwn_action_tmp != wwn_action)
		{
			for (u8 i = 0; i < 12; i+=4)
			{
				if ((wwn_action_tmp & (WWN_NO_CHANGE << i)) == 0)
					wwn_action = (wwn_action & ~(0x0F << i)) | (wwn_action_tmp & (0x0F << i));	
			}	
		}
		fill_wwn_all_luns(&mc_buffer[4]);
		save_prod_info = 1;
		fc_config_ctrl &= ~NO_CHANGE_TO_FLASH; 
	}
	
	if (fc_config_ctrl & ZERO_VIDPID)
		fc_config_ctrl |= SAME_CONFIGURATION; // when the VID, PID is zero, the command doesn't change VID & PID
		
	// do the software strap
	u8 saved_product_model = 0xFF; // invalid value
	// change the SCSI vendor text, SCSI mode text, USB iProduct string, option
	if ((fc_config_ctrl & SAME_CONFIGURATION) == 0)
	{
		// save the new PnP identifiers
		saved_product_model = product_model;
		product_model = prodIdx;
		MSG("%bx\n", product_model);
		identify_product();
		prodInfo.new_VID[0] = CmdBlk(4);
		prodInfo.new_VID[1] = CmdBlk(5);
		prodInfo.new_PID[0] = CmdBlk(6);
		prodInfo.new_PID[1] = CmdBlk(7);
		save_prod_info = 1;
	}
	
	wwn_action = saved_wwn_action;
	
	if (fc_config_ctrl & RESET_PARAMETERS)
	{
		ini_reset_defaults(TARGET_BOTH_SLOTS);
		// cipher ID is reset to the NOT Config state
	}
	
	if (fc_config_ctrl & CLEAR_SN)
	{
		//xmemset((u8 *)&nvm_quick_enum.disk0Info.numOfSectors, 0xff, 20);
		//xmemset((u8 *)&nvm_quick_enum.disk1Info.numOfSectors, 0xff, 20);
		xmemset(prodInfo.iSerial, 0xFF, 20);
		save_prod_info = 1;
	}
	else if (!(fc_config_ctrl & NO_CHANGE_USN))
	{
		xmemcpy(&mc_buffer[52], prodInfo.iSerial, 20);
		save_prod_info = 1;
	}

	if (!(fc_config_ctrl & ZERO_CONTAINER_ID))
	{
		xmemcpy(&mc_buffer[36], prodInfo.container_ID, 16);
		save_prod_info = 1;
	}

	if (saved_product_model != 0xFF)
	{// product model has been changed
		product_model = saved_product_model;
		identify_product();
	}
	
	MSG("fc %bx\n", fc_config_ctrl);
	if ((fc_config_ctrl & NO_CHANGE_TO_FLASH) == 0)
	{
		if ((fc_config_ctrl & RESET_PARAMETERS) == 0)
		{
			nvm_quick_enum.disk0Info.numOfSectors= sata0Obj.sectorLBA;
			nvm_quick_enum.disk0Info.user_bs_log2m9 = sata0Obj.physicalBlockSize_log2;
			nvm_quick_enum.disk1Info.numOfSectors= sata1Obj.sectorLBA;
			nvm_quick_enum.disk1Info.user_bs_log2m9 = sata1Obj.physicalBlockSize_log2;
		}
		else
		{
			nvm_quick_enum.disk0Info.numOfSectors= vital_data[0].lun_capacity;
			nvm_quick_enum.disk0Info.user_bs_log2m9 = vital_data[0].user_bs_log2m9;
			nvm_quick_enum.disk1Info.numOfSectors= vital_data[1].lun_capacity;
			nvm_quick_enum.disk1Info.user_bs_log2m9 = vital_data[1].user_bs_log2m9;			
	 		if (nvm_quick_enum.header.signature != WDC_QUICKENUM_DATA_SIG) 
	 		{
	 			nvm_quick_enum.header.signature = WDC_QUICKENUM_DATA_SIG;
	 			nvm_quick_enum.disk0Info.medium_rotation_rate = sata0Obj.sobj_medium_rotation_rate;
				nvm_quick_enum.disk1Info.medium_rotation_rate = sata1Obj.sobj_medium_rotation_rate;
	 		}
		}
		save_nvm_quick_enum();
	}

	if (save_prod_info)
	{
_SAVE_PROD_INFO:
		save_nvm_prod_info();
	}

	if (fc_config_ctrl & RE_ENUM_IN_CDB) 		// REENUM bit
	{
		//re-enumerate on its interface bus
		re_enum_flag = FORCE_OUT_RE_ENUM;
	}
	DBG("qEmEn\n");	
	return;


invalid_CDB_field:	
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
	return;

invalid_param_field:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_PARAM);
	return;
error_permanent_write:
	hdd_err_2_sense_data(pCtxt, ERROR_PERMANENT_WRITE_PROTECT);
	return;
	
data_protect_error:
	hdd_err_2_sense_data(pCtxt, ERROR_WRITE_PROTECT);
	return;
}


#if 0
/************************************************************************\
 factory_cmd() - SCSI_WDC_FACTORY_CMD command handler.

 // SCSI_WDC_FACTORY_CMD command's Action field.
enum {
	FACTORY_CONFIGUR		= 0x30,
	FACTORY_FREEZE		= 0x31
};

 */
void factory_cmd(PCDB_CTXT pCtxt)
{
	u32 l_byteCnt = (CmdBlk(8) << 8) + CmdBlk(9);

	if ((IS_LOCKED(1) && (arrayMode == JBOD)) || (IS_LOCKED(0)))
		goto data_protect_error;


	switch ( CmdBlk(1) /*Action*/ )	
	{
		case FACTORY_CONFIGUR:

			
			break;

		case FACTORY_FREEZE:
			if (l_byteCnt)
				goto invalid_CDB_field;


			
			break;

		default:
			goto invalid_CDB_field;
			//break;
	}

	return;

invalid_CDB_field:	
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
	return;	

data_protect_error:
	hdd_err_2_sense_data(pCtxt, ERROR_WRITE_PROTECT);
	return;
	
}
#endif
#endif


#if !defined(INITIO_STANDARD_FW) || defined(SCSI_COMPLY)
/************************************************************************\
 scsi_format_unit_cmd() - FORMAT UNIT command handler.

 */
void scsi_format_unit_cmd(PCDB_CTXT pCtxt)
{
	u8 blk_size_changed = 0;
	// Do not allow locked drives to be formatted.
#ifndef INITIO_STANDARD_FW
	if (IS_LOCKED(0) || IS_LOCKED(1))
	{
		hdd_err_2_sense_data(pCtxt, ERROR_WRITE_PROTECT);
		return;
	}
#endif

	// Check the flags in byte 1: we don't accept a parameter list.
	// Bits 3 (CmpLst) and 5 (LongList) are ignored; others must be 0.
	if (cdb.format_unit.flags &
		(FORMAT_UNIT_DEFECT_LIST_FMT | FORMAT_UNIT_FMTDATA | FORMAT_UNIT_FMTPINFO))
	{
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
		return;
	}

	// The vendor-specific field is reserved for our future use.
	if (cdb.format_unit.vendor_specific != 0)
	{
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
		return;
	}
#ifndef INITIO_STANDARD_FW
	if (hdd_power != HDDs_BOTH_POWER_ON)
	{
		if (ata_startup_hdd(1, SATA_TWO_CHs))
		{
			hdd_err_2_sense_data(pCtxt, ERROR_UA_BECOMING_READY);
			return;
		}
	}

	u8 slot_num = logicalUnit;
	// Commit the user-data area's new size and logical block length.
	if (vital_data[slot_num].user_bs_log2m9 != pending_user_bs_log2m9[slot_num])
	{
		vital_data[slot_num].user_bs_log2m9 = pending_user_bs_log2m9[slot_num];
		MSG("change blk_s\n");
//		re_enum_flag = DOUBLE_ENUM;
		blk_size_changed = 1;
	}
	vital_data[slot_num].lun_capacity = pending_user_data_size[slot_num];
	// ??? 
	// need add the signature & create CRC and header
	// Save the Init data the same way the MODE SELECT handler does.
	if (Write_Disk_MetaData(slot_num,WD_VITAL_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP))
	{
		*(((u16 *) &(pCtxt->CTXT_FIS[FIS_STATUS]))) =  *((u16 *)(&sata_ch_reg->sata_Status));

		// generate SCSI Sense code
		hdd_ata_err_2_sense_data(pCtxt);
		// is there a point trying to restore the original params?
		// there are two copies of Init Data; what if only one was updated?

		return;
	}

	// Use the new capacity and block size now that it has been saved.
	update_user_data_area_size();
	update_array_data_area_size();
//	if (re_enum_flag == DOUBLE_ENUM)// only when the block size is changed, F/W do the reenumerate once again
	if (blk_size_changed)
	{
		u8 temp_blk_size = 0;
		if (vital_data[slot_num].user_bs_log2m9 < 4) // 1024, 2K, 4K
			temp_blk_size = vital_data[slot_num].user_bs_log2m9;
		*usb_Msc0LunSATCtrl = (*usb_Msc0LunSATCtrl & ~HOST_BLK_SIZE) | (temp_blk_size << 4);
	}
#endif
	// All done, return GOOD status.
	nextDirection = DIRECTION_SEND_STATUS;

}
#endif



//initio vendor command 

void initio_vendor_inquiry()
{
	xmemset(mc_buffer, 0x00, 0x80);
	
	mc_buffer[0] = 0x3A;			
#ifdef WDC_DOWNLOAD_MICROCODE
	mc_buffer[1] = 0x50;				//Model ID initio 3A10, WD 3A50
#else
	mc_buffer[1] = 0x10;
#endif

	mc_buffer[2] = globalNvram.fwVersion[0];
	mc_buffer[3] = globalNvram.fwVersion[1];
	xmemcpy(globalNvram.vendorText, mc_buffer+4, 8);		//4-11
	xmemcpy(globalNvram.modelText, mc_buffer+12, 16);	//12-27

	mc_buffer[28] = globalNvram.fwRevisionText[0];
	mc_buffer[29] = '.';
	mc_buffer[30] = globalNvram.fwRevisionText[1];
	mc_buffer[31] = globalNvram.fwRevisionText[2];

	static const char copy_right[] = "(C) 2002-13 Initio Corp.";
	xmemcpy((u8 *)copy_right, mc_buffer+32, 24);			//32-55
	mc_buffer[56] = *chip_Revision;
	*(mc_buffer + 60) = (u8)(*chip_id2 >> 24) & 0xFF;
	*(mc_buffer + 61) = (u8)(*chip_id2 >> 16) & 0xFF;
	*(mc_buffer + 62) = (u8)(*chip_id2 >> 8) & 0xFF;
	*(mc_buffer + 63) = (u8)(*chip_id2) & 0xFF;
	*(mc_buffer + 64) = (u8)(*fw_tmp) & 0x03;
	*(mc_buffer + 65) = 0x01;	
	if(*fw_tmp &  BOOT_FROM_PRIMARY)
	{
		*(mc_buffer + 66) = 1;	
	}
	else //BOOT_FROM_SECONDARY
	{
		*(mc_buffer + 66) = 2;		
	}
	
	*(mc_buffer + 67) = globalNvram.validNvramLocation | 0x01;	//normal firmware always boot from SPI ?
	
	*(mc_buffer + 68) = usbMode;
	*(mc_buffer + 69) = 0x00;  				// not support SCSI HID
	
	*(mc_buffer + 70) = flash_type;  
	*(mc_buffer + 71) = flash_vid;
	*(mc_buffer + 72) = flash_pid;

	*(mc_buffer + 74) = (u8)((MAX_FW_IMAGE_SIZE_INI>>24) & 0x000000FF);  
	*(mc_buffer + 75) = (u8)((MAX_FW_IMAGE_SIZE_INI>>16) & 0x000000FF);  
	*(mc_buffer + 76) = (u8)((MAX_FW_IMAGE_SIZE_INI>>8) & 0x000000FF);  
	*(mc_buffer + 77) = (u8)(MAX_FW_IMAGE_SIZE_INI & 0x000000FF);  

	*(mc_buffer + 78) = USB_AdapterID;
	*(mc_buffer + 79) = USB_PortID0;
	*(mc_buffer + 80) = USB_PortID1;
	*(mc_buffer + 81) = USB_PortID2;	
	byteCnt = 128;
	alloc_len = 128;
}

#ifdef WDC_DOWNLOAD_MICROCODE
void spi_flash_write_fail_handle(u32 flash_addr)
{
	u32 FW_sector_offset = flash_addr;
	u16 FW_rewrite_size = (u16)(flash_addr & 0xFFF);

	if ((FW_sector_offset & 0xFFF) != 0)
	{
		FW_sector_offset &= 0xFF000; // alignment to 4K boundary
		u32 rewrite_spi_addr = 0;
		
		rewrite_spi_addr = FW_sector_offset;
		spi_read_flash(mc_buffer, rewrite_spi_addr, FW_rewrite_size);
		spi_write_flash(mc_buffer, rewrite_spi_addr, FW_rewrite_size);	
	}
}
#endif

/****************************************************************\
	SCSI write buffer (10) Command for HDD, & CDROM

\***************************************************************/
void scsi_write_buffer10_cmd(PCDB_CTXT pCtxt)
{
	u32 cmdErrorStatus = ERROR_ILL_CDB;
	u32  current_erase_addr;
	if (cdb.byte[1] == INI_VEN_CMD)	//initio vendor unique scsi command.
	{		
		switch(cdb.byte[2])
		{	
			case INI_VEN_FW_DOWNLOAD_CTL:
				if ((CmdBlk(6) == 0x00) &&
				(CmdBlk(7) == 0x00) &&
				(CmdBlk(8) == 0x08))
				{
					byteCnt = 8;
					cmdErrorStatus = NO_ERROR;
				}
				break;

				
			case INI_VEN_SPI_ERASE_SECTOR:
				if ((CmdBlk(5) == 0x00) &&
				(CmdBlk(6) == 0x00) &&
				(CmdBlk(7) == 0x00) &&
				(CmdBlk(8) == 0x00))
				{
					if (!start_fw_downloading)
					{
						cmdErrorStatus = ERROR_COMMAND_SEQUENCE;
						break;
					}
					
					current_erase_addr  = (CmdBlk(3) << 16) + (CmdBlk(4) << 8) + CmdBlk(5);
					if (current_erase_addr & 0xFFF)
					{
						break;
					}
					spi_erase_flash_sector(current_erase_addr);
					nextDirection = DIRECTION_SEND_STATUS;
					return;
				}
				break;
				
			case INI_VEN_SPI_ERASE_CHIP:
				if ((CmdBlk(5) == 0x00) &&
				(CmdBlk(6) == 0x00) &&
				(CmdBlk(7) == 0x00) &&
				(CmdBlk(8) == 0x00))
				{
					if (!start_fw_downloading)
					{
						cmdErrorStatus = ERROR_COMMAND_SEQUENCE;
						break;
					}
					
					spi_erase_flash_chip();
					nextDirection = DIRECTION_SEND_STATUS;
					//cmdErrorStatus = NO_ERROR;
					return;
				}
				break;

				
			case INI_VEN_SPI_DOWNLOAD_BINARY:
				if ((CmdBlk(5) == 0x00) &&
				(CmdBlk(6) == 0x00) &&
				(CmdBlk(7) == 0x02) &&
				(CmdBlk(8) == 0x00))
				{
					if (!start_fw_downloading)
					{
						cmdErrorStatus = ERROR_COMMAND_SEQUENCE;
						break;
					}
					u32 fwAddr = 0;
					fwAddr = (CmdBlk(3) << 16) + (CmdBlk(4) << 8) + CmdBlk(5);
					if ((fwAddr >= MAX_FW_IMAGE_SIZE_INI) || (fwAddr & 0x1FF))
					{
						break;
					}
				
					if (fwAddr == 0)
					{
						current_image_offset = 0;
						download_microcode_ctrl = DOWNLOAD_FLASH_START;
						*crc_Ctrl = 0;
						*crc_Ctrl = CRC_En;
					}
					
					if ((download_microcode_ctrl != DOWNLOAD_FLASH_START) 
					|| (fwAddr != current_image_offset))
					{
						cmdErrorStatus = ERROR_COMMAND_SEQUENCE;
						break;
					}
					
					byteCnt = 512;
					cmdErrorStatus = NO_ERROR;
				}
				break;

				 
			case INI_VEN_SPI_WRITE_NVRAM:
				if ((CmdBlk(3) == 0x00) &&
				(CmdBlk(4) == 0x00) &&
				(CmdBlk(5) == 0x00) &&
				(CmdBlk(6) == 0x00) &&
				(CmdBlk(7) == 0x01) &&
				(CmdBlk(8) == 0x00))
				{
					if (!start_fw_downloading)
					{
						cmdErrorStatus = ERROR_COMMAND_SEQUENCE;
						break;
					}
					byteCnt = 256;
					cmdErrorStatus = NO_ERROR;
				}
			    	break;

			case INI_VEN_SET_USB_PORT_ID:
				if ((CmdBlk(6) == 0x00) &&
				(CmdBlk(7) == 0x00) &&
				(CmdBlk(8) == 0x08))
				{
					byteCnt = 8;
					cmdErrorStatus = NO_ERROR;
				}
				break;

			case INI_VEN_FUN_GSPI_SYNC_BURST_F0:
				if((cdb.byte[3] == 0x00)
				&& (cdb.byte[4] == 0x00)
				&& (cdb.byte[5] == 0x00)
				&& (cdb.byte[6] == 0x00)
				&& (cdb.byte[7] == 0x00)
				&& (cdb.byte[8] == 0x10)
				&& (cdb.byte[9] == 0x00))
				{
					byteCnt = 16;
					cmdErrorStatus = NO_ERROR;
				}
				break;

			case INI_VEN_FUN_GSPI_SYNC_BURST_F1:
			case INI_VEN_FUN_GSPI_SYNC_BURST_9BITS:
				if((cdb.byte[3] == 0x00)
				&& (cdb.byte[4] == 0x00)
				&& (cdb.byte[5] == 0x00)
				&& (cdb.byte[6] == 0x00)
				&& (cdb.byte[7] == 0x02)
				&& (cdb.byte[8] == 0x00)
				&& (cdb.byte[9] == 0x00))
				{
					byteCnt = 512;	
					cmdErrorStatus = NO_ERROR;
				}
				break;

			case INI_VEN_READ_WRITE_VCD:
				// 3B 10 27 XX XX XX 80/81 00 XX 00
				vcd_read_write_virtual_cd_cmd(pCtxt);
				return;

			case INI_VEN_FUN_FAN_CTL:
				if((cdb.byte[3] == 0x00)
				&& (cdb.byte[4] == 0x00)
				&& (cdb.byte[6] == 0x00)
				&& (cdb.byte[7] == 0x02)
				&& (cdb.byte[8] == 0x00)
				&& (cdb.byte[9] == 0x00))
				{
					set_fan_state(cdb.byte[5]);
					nextDirection = DIRECTION_SEND_STATUS;
					return;
				}
				break;
#if 0
			case INI_VEN_RAID_CLEAN_MATADATA:
				if((cdb.byte[3] == 0x00)
				&& (cdb.byte[4] == 0x00)
				&& (cdb.byte[6] == 0x00)
				&& (cdb.byte[7] == 0x00)
				&& (cdb.byte[8] == 0x00)
				&& (cdb.byte[9] == 0x00))
				{
					initio_vendor_debug_clean_matadata();
					nextDirection = DIRECTION_SEND_STATUS;
					return;					
				}
				break;
#endif				
				
			default:
				break;
		}
	}
#ifndef WDC_RAID
	else if (cdb.byte[1] == RAID_SERVICE_ACTION)
	{
		raid_wdc_maintenance_out_cmd(pCtxt);
		return;  // should use return because the func has started to recive the data
	}
#endif				
#ifdef WDC_DOWNLOAD_MICROCODE
	else if (cdb.byte[1] == WD_CDB_DL_MICROCODE_MODE) 
	{	
		if (CmdBlk(2) == 0x00)
		{
			u32 alloc_len = (cdb.byte[6] << 16) + (cdb.byte[7] << 8) + cdb.byte[8];
			if ((alloc_len == 512) && (cdb.byte[2] < 2)) 
			{
				byteCnt = 512;
				cmdErrorStatus = NO_ERROR;
			}

			// stop to rebuild or verify
			raid_mirror_status = 0; 
		}
	}
	
	else if (cdb.byte[1] == WD_CDB_ACTIVATE_MICROCODE_MODE)
	{
		// activate the defered Microcode mode
		// check the downloading is finished and is there a deferred microcode
		if (download_microcode_ctrl & DOWNLOAD_MICROCODE_DONE)
		{
			download_microcode_ctrl = ACTIVATE_DEFERED_MICROCODE;
			pwr_off_enable = 1; // return to main code firstly
			nextDirection = DIRECTION_SEND_STATUS;
			return;
		}
		else
		{
			cmdErrorStatus = ERROR_COMMAND_SEQUENCE;
		}
	}
#endif

lj_write_buffer_check_error:
	if(cmdErrorStatus)
	{
		hdd_err_2_sense_data(pCtxt, cmdErrorStatus);
		return;	
	}

	usb_device_data_out(pCtxt, byteCnt);	
}

/****************************************************************\
	SCSI read buffer (10) Command for HDD, & CDROM

\***************************************************************/
void scsi_read_buffer10_cmd(PCDB_CTXT pCtxt)
{
	nextDirection = DIRECTION_SEND_DATA;
	if (cdb.byte[1] == INI_VEN_CMD)	//initio vendor unique scsi command.
	{
		xmemset(mc_buffer, 0x00, 0x80);
		switch(cdb.byte[2])
		{
			case INI_VEN_INQUIRY:
				if ((CmdBlk(3) == 0x00) &&
					(CmdBlk(4) == 0x00) &&
					(CmdBlk(5) == 0x00) &&
					(CmdBlk(6) == 0x00) &&
					(CmdBlk(7) == 0x00) &&
					(CmdBlk(8) == 0x80))
					{
						initio_vendor_inquiry();
						return;
					}
					break;
				
			case INI_VEN_READ_NVRAM:
				if ((CmdBlk(3) == 0x00) &&
					(CmdBlk(4) == 0x00) &&
					(CmdBlk(5) == 0x00) &&
					(CmdBlk(6) == 0x00) &&
					(CmdBlk(7) == 0x01) &&
					(CmdBlk(8) == 0x00))
					{
						byteCnt = sizeof(INI_NVRAM);
						alloc_len = byteCnt;
						xmemcpy(((u8 *)&globalNvram), mc_buffer, sizeof(INI_NVRAM));				
						return;
					}
					break;
				
			case INI_VEN_SPI_READ_FW_IMAGE:
				if (is_chip_for_wd)
				{
					break;
				}
			
				// Initio Read Flash could read the whole chip if UI want
				u32 flashBase;
				flashBase = (CmdBlk(3) << 16) + (CmdBlk(4) << 8);
				byteCnt = 512;
				alloc_len = byteCnt;
				
				if (!read_fw_from_flash(flashBase, byteCnt))
				{
					return;
				}
				
				break;
				
			case INI_VEN_READ_WRITE_VCD:
				// 3C 10 27 XX XX XX 80/81 00 XX 00
				nextDirection = DIRECTION_NONE;
				vcd_read_write_virtual_cd_cmd(pCtxt);
				return;		
#ifdef SCSI_HID
			case INI_VEN_SCSI_HID_READ:
				// 3C 10 0A 00 00 25 26 00 04 00
				if ((cdb.byte[3] == 0x00) &&
					(cdb.byte[4] == 0x00) &&
					(cdb.byte[5] == 0x25) &&
					(cdb.byte[6] == 0x26) &&
					(cdb.byte[7] == 0x00) &&
					(cdb.byte[8] == 0x04) &&
					(cdb.byte[9] == 0x00)) 
				{
					byteCnt = 4;
					alloc_len = byteCnt;

					mc_buffer[0] = backup_button_pressed;
					mc_buffer[1] = 0x00;
					mc_buffer[2] = 0x00;
					mc_buffer[3] = 0x00;

					if (backup_button_pressed)	// to be sure the AP to get it.
						backup_button_pressed = 0;
					return;
				}
				break;
#endif				
#ifdef INI_SCSI_DBG
			case INI_VEN_RAID_DBG_F1:
					if((cdb.byte[3] == 0x00) &&
					(cdb.byte[4] == 0x00) &&
					(cdb.byte[6] == 0x00) &&
					(cdb.byte[7] == 0x00) &&
					(cdb.byte[8] == 0x04) &&
					(cdb.byte[9] == 0x00))
					{
						byteCnt = 4;
						alloc_len = byteCnt;
						*raid_config = (*raid_config & ~RAID0_STRIPE_SIZE)|((u16)(cdb.byte[5]&0x0F)<<12);
						u8 stripe_size = ((*raid_config & RAID0_STRIPE_SIZE)>>12)&0x0F;
						mc_buffer[0] = 0xF1;
						mc_buffer[1] = 0x00;
						mc_buffer[2] = 0x00;
						mc_buffer[3] = stripe_size;
						return;
					}
					break;

			case INI_VEN_RAID_DBG_F2:
					 if ((cdb.byte[2] == 0xF2) && 
					(cdb.byte[3] == 0x00) &&
					(cdb.byte[4] == 0x00) &&
					(cdb.byte[6] == 0x00) &&
					(cdb.byte[7] == 0x00) &&
					(cdb.byte[8] == 0x04) &&
					(cdb.byte[9] == 0x00))
					{
						byteCnt = 4;
						alloc_len = byteCnt;
						if (cdb.byte[5] == 0x01)
							*raid_read_ctrl |= RAID_RD_RESET;
						else if (cdb.byte[5] == 0x02)
							*raid_write_ctrl |= RAID_WR_RESET;
						mc_buffer[0] = 0xF2;
						mc_buffer[1] = 0x00;
						mc_buffer[2] = 0x00;
						mc_buffer[3] = cdb.byte[5];
						return;
					 }
					 break;
					 
			case INI_VEN_RAID_DBG_F3:
					if ((cdb.byte[3] == 0x00) &&
					(cdb.byte[4] == 0x00) &&
					(cdb.byte[6] == 0x00) &&
					(cdb.byte[7] == 0x00) &&
					(cdb.byte[8] == 0x04) &&
					(cdb.byte[9] == 0x00))
					{	// to modify array_status, valid in mirror
						byteCnt = 4;
						alloc_len = byteCnt;
						//raid_mirror_status = cdb.byte[5];
						switch (cdb.byte[5])
						{
							case 1:
								raid_rebuild_initiated(RAID1_REBUILD0_1);
								break;
							case 2:
								raid_rebuild_initiated(RAID1_REBUILD1_0);
								break;
							case 3:
								raid_mirror_status = RAID1_STATUS_VERIFY;
								raid_mirror_operation = RAID1_VERIFY;
								verify_watermark = 0;
								break;	

							default:
								break;
						}
						mc_buffer[0] = 0xF3;
						mc_buffer[1] = array_status_queue[0];
						mc_buffer[2] = raid_mirror_status;
						mc_buffer[3] = raid_mirror_operation;
						raid1_active_timer = RAID_REBUILD_VERIFY_ONLINE_ACTIVE_TIMER;
						return;
					}
					break;

			case INI_VEN_RAID_CLEAN_MATADATA:
				if((cdb.byte[3] == 0x00)
				&& (cdb.byte[4] == 0x00)
				&& (cdb.byte[6] == 0x00)
				&& (cdb.byte[7] == 0x00)
				&& (cdb.byte[8] == 0x00)
				&& (cdb.byte[9] == 0x00))
				{
					initio_vendor_debug_clean_matadata();
					nextDirection = DIRECTION_SEND_STATUS;
					return;					
				}
				break;

#if 0
			case INI_VEN_DUMP_METADATA:
				// 3c 10 F6 0/1 0/1 00 00 00 00 00 
				if (((cdb.byte[3] == 0x00) || (cdb.byte[3] == 0x01)) &&
					((cdb.byte[4] == 0x00) || (cdb.byte[4] == 0x01)) &&
					(cdb.byte[6] == 0x00) &&
					(cdb.byte[7] == 0x00) &&
					(cdb.byte[8] == 0x00) &&
					(cdb.byte[9] == 0x00))
				{
					return_meta_data(cdb.byte[3], WD_RAID_PARAM_PAGE, cdb.byte[4]);
				}
				return;
#endif				
#endif				
			default:	
				break;	
		}
	}
#ifndef WDC_RAID
	else if (cdb.byte[1] == RAID_SERVICE_ACTION)
	{
		raid_wdc_maintenance_in_cmd(pCtxt);
		return;
	}
#endif		
#ifndef INITIO_STANDARD_FW
	else if (cdb.byte[1] == WD_CDB_VD_SPE_MODE)
	{
		// file image format
		/*************************************************
		*** byte[0~511] Firmware header
		*** byte[512~n] Firmware code
		*** n should be multiple of 2^offsetBoundary
		*************************************************/
		 if((CmdBlk(6) == 0x00) &&
		(CmdBlk(7) == 0x02) &&
		(CmdBlk(8) == 0x00)&& 
		((CmdBlk(2) == 0x00) ||
		(CmdBlk(2) == 0x01)))
		{
			alloc_len = (cdb.byte[6] << 16) + (cdb.byte[7] << 8) + cdb.byte[8];
			u32 buffer_offset = (cdb.byte[3] << 16) + (cdb.byte[4] << 8) + cdb.byte[5];
			if ((alloc_len == 512) && (cdb.byte[2] < 0x2) && (buffer_offset < MAX_FW_IMAGE_SIZE_WD) && ((buffer_offset & 0x1FF) == 0))
			{
				byteCnt = 512;
				u32 flash_addr = check_fw_base_addr(cdb.byte[2]);
				flash_addr = flash_addr - WD_FW_HEADER + buffer_offset;
				if (buffer_offset == 0)
				{
					// check the valid of the firmware in corresponding area in flash floows the buffer ID
					// check signature 
					u8 firmware_valid = 0;
					#define FW_SIGNATURE_OFFSET 0x1C1E0
					xmemset(&mc_buffer[3072], 0x00, 4);
					spi_read_flash((u8 *)(&mc_buffer[3072]), flash_addr+FW_SIGNATURE_OFFSET, 4);
					DBG("si %bx, %bx\n", mc_buffer[3072], mc_buffer[3073]);
					if ((mc_buffer[3072] == 0x25) && (mc_buffer[3073] == 0xc9) && (mc_buffer[3074] == 0x36) && (mc_buffer[3075] == 0x10))
					{
						xmemset(&mc_buffer[3072], 0x00, 0x200);
						spi_read_flash((u8 *)(&mc_buffer[3072]), flash_addr, 0x200);
						// check the signature of the FW header
						if (((mc_buffer[3072] == 0x01) && (mc_buffer[3073] == 0x00))
						// check chips signature
						&& ((mc_buffer[3074] == 0x25) && (mc_buffer[3075] == 0xc9) && (mc_buffer[3076] == 0x36) && (mc_buffer[3077] == 0x10))
						// check wd signature
						&& ((mc_buffer[3078] == 0x03) && (mc_buffer[3079] == 0x03) && (mc_buffer[3080] == 0x05) && (mc_buffer[3081] == 0x05)))
						{
							//check the CRC 
							// backup the mc_buffer data
							u32 flash_addr_off = 0;
							*crc_Ctrl = 0; 
							while (flash_addr_off < 0x1C1FE)
							{
								if (flash_addr_off == 0x1BC00)
								{
									spi_read_flash((u8 *)(&mc_buffer[0]), flash_addr+ flash_addr_off, 0x600);								
									if (crc16(mc_buffer, 0x600) == 0)
									{
										MSG("va\n");
										firmware_valid = 1;
									}
									else
									{
										MSG("crc Er\n");
									}
										
									 flash_addr_off+=0x600;
									 *crc_Ctrl = 0; 
								}
								else
								{
									spi_read_flash((u8 *)(&mc_buffer[0]), flash_addr+ flash_addr_off, 0xC00);
									u16 crc_data = crc16(mc_buffer, 0xC00);
									 flash_addr_off+=0xC00;
								}							
							}
						}
						else
						{
							MSG("sig Er\n");
						}
					}

					mc_buffer[0] = 0x01;
					mc_buffer[1] = 0x00;
					WRITE_U32_BE(&mc_buffer[2], CHIPS_SIGNATURE);
					WRITE_U32_BE(&mc_buffer[6], WDC_FW_SIGNATURE);
					static const char part_name[] = "INIC-3610 FW  ";
					xmemcpy((u8 *)part_name, &mc_buffer[10], 14);
					if (firmware_valid)
					{
						mc_buffer[24] = 1;
						u8 str[4];
						get_fw_version(str, (cdb.byte[2] == 0));
						xmemcpy(str, &mc_buffer[25], 4);
					}
					else
					{
						xmemset(&mc_buffer[24], 0, 5);
					}
					xmemset(&mc_buffer[29], 0x00, 512-29);
					return;
				}
				else if ((buffer_offset & 0x1FF) == 0)// should be aligned in 512 bytes
				{
					MSG("f addr %lx\n", flash_addr);
					spi_read_flash(mc_buffer, flash_addr, 512);
					return;				
				}
			}
		}
	}
#endif
	else if (cdb.byte[1] == WD_CDB_DESCRIPTOR_MODE) // descriptor mode
	{
		if ((CmdBlk(2) == 0x00) || (CmdBlk(2) == 0x01))
		{	
			byteCnt = (CmdBlk(6) << 16) + (CmdBlk(7) << 8) + (CmdBlk(8));
			alloc_len = byteCnt;

			if (byteCnt < 4)
			{
				goto lj_read_buffer_err_ill_cdb;
			}

			*(mc_buffer + 0) = WD_OFFSET_BOUNDARY_512; // 2*9 = 512 for default
			*(mc_buffer + 1) = (u8)(MAX_FW_IMAGE_SIZE_WD >> 16); // buffer offset should be smaller than 64K
			*(mc_buffer + 2) = (u8)(MAX_FW_IMAGE_SIZE_WD >> 8);
			*(mc_buffer + 3) = (u8)(MAX_FW_IMAGE_SIZE_WD);
			return;
		}
	}
lj_read_buffer_err_ill_cdb:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);

}


/************************************************************************\
 scsi_read_disk_capacity_cmd() - READ CAPACITY(10)/(16) command handler.

 This is only used by the disk logical unit.
 The virtual CD-ROM has its own READ CAPACITY command handler.

 */
void scsi_read_disk_capacity_cmd(PCDB_CTXT pCtxt)
{
	u64 tmp_capacity;
	u8 tmp_user_bs_log2m9 = 0;

	// This command returns the last LBA of the user-data area (disk LUN)
	// The last LBA is relative to the current logical block size.
	// Computing the last LBA thus requires explicit 40-bit math, so
	// crunch the bits first as if this is a READ CAPACITY(16) command 
	// and then rearrange the result as needed.

	xmemset(mc_buffer, 0, 32);

	// Compute and put the Last LBA into bytes 0..7.
	//
	// first set mc_buf[0 ~ 7] to the user-data size...
	if (arrayMode == JBOD)
	{
		if ((logicalUnit == HDD0_LUN) && (sata0Obj.sobj_device_state != SATA_NO_DEV))
		{
			tmp_capacity = user_data_area_size[0];
			tmp_user_bs_log2m9 = vital_data[0].user_bs_log2m9;
		}
		else
		{
			tmp_capacity = user_data_area_size[1];
			tmp_user_bs_log2m9 = vital_data[1].user_bs_log2m9;
		}
	}
	else
	{
		tmp_capacity = array_data_area_size;	
		tmp_user_bs_log2m9 = vital_data[0].user_bs_log2m9;
	}

	WRITE_U64_BE(mc_buffer, tmp_capacity - 1);
	if (tmp_user_bs_log2m9)  // 
	{
		tmp_capacity >>= tmp_user_bs_log2m9;
		WRITE_U64_BE(mc_buffer, tmp_capacity - 1);
	}
	
	u8 PMI = (CmdBlk(8) & 0x01);
	u32 lba_num = (cdb.byte[2] << 24) + (cdb.byte[3] << 16) + (cdb.byte[4] << 8) + cdb.byte[5];
	if (cdb.opcode == SCSI_SERVICE_ACTION_IN16)
	{
		PMI = (CmdBlk(14) & 0x01);
		lba_num =((u64)cdb.byte[2] << 56)+ ((u64)cdb.byte[3] << 48) + ((u64)cdb.byte[4] << 40) + ((u64)cdb.byte[5] << 32) + (cdb.byte[6] << 24) + (cdb.byte[7] << 16) + (cdb.byte[8] << 8) + cdb.byte[9];		
	}
	if (PMI == 0)
	{
		if (lba_num)
		{
_EEROR_ILL_CDB:
			hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
			return;
		}
	}
	else
	{
		if (lba_num >= tmp_capacity)
		{
			hdd_err_2_sense_data(pCtxt, ERROR_LBA_OUT_RANGE);
			return;
		}	
	}
	
	// Now fill in the rest of the data depending the command received.
	switch (cdb.opcode)
	{
	case SCSI_READ_CAPACITY:
		if ((CmdBlk(8) & 0x01) == 0)
		{
			if (CmdBlk(2) || CmdBlk(3) || CmdBlk(4) || CmdBlk(5))
			{
				hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
				return;
			}
		}
		// Bytes 0..3 is the Last LBA.
		// The 40-bit Last LBA is in bytes 3..7 (see above), so move it
		// to the right place, but return  2**32 -1  if it's too large.
		if (mc_buffer[3])
		{
			if (rdCapCounter >= 5)  
				*(u32*)&mc_buffer[0] = 0xfeffffff;
			else
			{
				*(u32*)&mc_buffer[0] = 0xffffffff;
				rdCapCounter++;
			}
		}
		else
		{
			*(u32*)&mc_buffer[0] = *(u32*)&mc_buffer[4];
		}

		// Bytes 4..7 is Logical Block Size.
		u32 tmp32= (u32)1 << (tmp_user_bs_log2m9 + 9);
		WRITE_U32_BE(&mc_buffer[4], tmp32);
//		mc_buffer[0] += 0x10;

		byteCnt = 8;
		alloc_len = 8;
		nextDirection = DIRECTION_SEND_DATA;
		break;

	case SCSI_SERVICE_ACTION_IN16:
		// Assume caller gave us a real READ CAPACITY(16) cmd!

		// Bytes 0..7 is the Last LBA - already computed above.

		// Bytes 8..11 is Logical Block Size.
		tmp32= (u32)1 << (tmp_user_bs_log2m9 + 9);
		WRITE_U32_BE(&mc_buffer[8], tmp32);

		// Byte 12 has Prot_En and P_Type fields, which are already 0.

		// Byte 13, bits 0..3 is Logical Blks per Physical Blk Exponent.
		// If the logical block size is larger than the physical sector
		// size, then technically this field should have a negative value.
		// Since that's not allowed, leave it set to 0 if that's the case.

#if WDC_ALWAYS_REPORT_4K_PHYSICAL
		if (12 /*log2(4096)*/ > (tmp_user_bs_log2m9 +9))
		{
			mc_buffer[13] = 12 - (tmp_user_bs_log2m9 + 9);
		}

		// Byte 14..15 have 14 bits indicating the Lowest Aligned LBA;
		// since we don't know the HDD's actual physical sector size,
		// just assume the LBA alignment is 0. (Already set to 0, above.)
#endif	// WDC_ALWAYS_REPORT_4K_PHYSICAL
		rdCapCounter = 0;
		byteCnt = 32;
		alloc_len = (cdb.byte[10] << 24) + (cdb.byte[11] << 16) + (cdb.byte[12] << 8) + cdb.byte[13];
		nextDirection = DIRECTION_SEND_DATA;
		break;

#if WDC_INCLUDE_ASSERTIONS
	default:
		// Should never get here; caller knows we only do READ CAPACITY!
		hdd_err_2_sense_data(pCtxt, ERROR_ASSERT_FAILED);
		break;
#endif	// WDC_INCLUDE_ASSERTIONS
	}

}

void scsi_cmd_unsupport_lun(PCDB_CTXT pCtxt)
{
	MSG("cdb %bx, lun %bx\n", pCtxt->CTXT_CDB[0], logicalUnit);
#ifdef UAS
	uas_ci_paused |= UAS_PAUSE_WAIT_FOR_USB_RESOURCE;
	uas_ci_paused_ctxt = pCtxt;
#endif

	if ((CmdBlk(0) == SCSI_INQUIRY) || (CmdBlk(0) == SCSI_REPORT_LUNS) || (CmdBlk(0) == SCSI_REQUEST_SENSE))
	{
		logicalUnit = UNSUPPORTED_LUN;
		scsi_StartAtaCmd(pSataObj, pCtxt);
		dbg_next_cmd = 0;
	}
	else
	{
		pCtxt->CTXT_usb_state = CTXT_USB_STATE_CTXT_SETUP_DONE;
		hdd_err_2_sense_data(pCtxt, ERROR_INVALID_LUN);
		usb_device_no_data(pCtxt);
	}
}

void scsi_sync_cache_cmd(PCDB_CTXT pCtxt)
{
	if (pSataObj->sobj_WrCache == WRITE_CACHE_ENABLE)
	{
		
		if (hdd_power)
		{	
			if(arrayMode == JBOD)
			{
				DBG("flush cache\n");
				pCtxt->CTXT_FLAG = CTXT_FLAG_U2B;
				ata_ExecUSBNoDataCmd(pSataObj, pCtxt, ATA6_FLUSH_CACHE_EXT);
				return;
			}
			else
			{
				if(hdd_power & HDD0_POWER_ON)
				{
					ata_ExecNoDataCmd(&sata0Obj, ATA6_FLUSH_CACHE_EXT, 0, WDC_SATA_TIME_OUT);
				}
				if(hdd_power & HDD1_POWER_ON)
				{
					ata_ExecNoDataCmd(&sata1Obj, ATA6_FLUSH_CACHE_EXT, 0, WDC_SATA_TIME_OUT);
				}
			}
		}
	}
	// send good status right away
	nextDirection = DIRECTION_SEND_STATUS;
}

// firmware has completed check CDB filed, and it checks if the HDD is power on
// check if there's data parameter need be received
// return value: 0 -> no data transfer & HDD is powered.
// 			    1 -> either wait for the bulk out data or the HDD to be spun up
u8 scsi_chk_hdd_pwr(PCDB_CTXT pCtxt)
{
	if (hdd_power != HDDs_BOTH_POWER_ON)
	{
		//if (arrayMode !=)
		
		{
			ata_startup_hdd(0, SATA_TWO_CHs);
			if (curMscMode == MODE_UAS)
			{
				uas_ci_paused_ctxt = pCtxt;
			}
			else
			{
				bot_active &= ~BOT_ACTIVE_PROCESS_CBW;	
			}
			// because HDD is not spun up like mode select/send Diag.../log sense..., 
			// pending the execution command until the HDD is spun up, or respond HDD is becoming ready after 7secs timeout
			usb_ctxt.newCtxt = pCtxt; 
			pCtxt->CTXT_usb_state = CTXT_USB_STATE_CHK_CDB_FIELD_DONE;
		}
		return 1;
	}
	return 0;
}

u8 scsi_start_receive_data(PCDB_CTXT pCtxt, u32 l_byteCnt)
{
	if ((l_byteCnt != 0) && (pCtxt->CTXT_usb_state < CTXT_USB_STATE_AWAIT_USB_RECEIVE_DONE))
	{// if the command transfering data, receive data parameter firstly
		if (dbg_next_cmd)
			MSG("rec d\n");
		usb_device_data_out(pCtxt, l_byteCnt);
		pCtxt->CTXT_usb_state = CTXT_USB_STATE_AWAIT_USB_RECEIVE_DONE;
		return 1;
	}
	return 0;
}

// merge the check of read write command to save code size
u8 scsi_chk_valid_rw_cmd(PCDB_CTXT pCtxt)
{
	if (CmdBlk(15))
	{
		goto _ILLEGAL_CDB;
	}
	
	if (arrayMode == NOT_CONFIGURED)
	{
_WRITE_PROTECT:
		hdd_err_2_sense_data(pCtxt, ERROR_WRITE_PROTECT);
		usb_device_no_data(pCtxt);
		return 1;		
	}
	else if (arrayMode == JBOD)
	{
		if (IS_LOCKED_OR_UNCONFIGED(logicalUnit))
		{
			goto _WRITE_PROTECT;
		}
	}
#ifdef WRITE_PROTECT
	else if ((write_protect_enabled) && ((CmdBlk(0) == SCSI_WRITE6) || (CmdBlk(0) == SCSI_WRITE10)
		|| (CmdBlk(0) == SCSI_WRITE12) || (CmdBlk(0) == SCSI_WRITE16)))
	{
		goto _WRITE_PROTECT;
	}	
#endif
	else
	{
		if (IS_LOCKED_OR_UNCONFIGED(0))
			goto _WRITE_PROTECT;
	}

	if(pCtxt->CTXT_SAT_FAIL_REASON & (CTXT_SAT_SECCNT_ERR|CTXT_SAT_LBA_ERR|CTXT_SAT_LBA_OVRN))
	{	//transfer length > 0xffff
_ILLEGAL_CDB:
		if (pCtxt->CTXT_SAT_FAIL_REASON & (CTXT_SAT_LBA_OVRN|CTXT_SAT_LBA_ERR))
			hdd_err_2_sense_data(pCtxt, LBA_OUT_RANGE);
		else
			hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
		usb_device_no_data(pCtxt);
		return 1;						
	}

	if (scsi_chk_hdd_pwr(pCtxt))
	{ 
		return 1;
	}

	if (curMscMode == MODE_UAS)
	{	
		if (CmdBlk(1) & 0xE0)
		{
			if ((CmdBlk(0) != SCSI_WRITE6) && (CmdBlk(0) != SCSI_READ6)) goto _ILLEGAL_CDB;
		}
		if (((pCtxt->CTXT_FIS[FIS_FEATURE]) == 0) && ((pCtxt->CTXT_FIS[FIS_FEATURE_EXT]) == 0))
		{// the sector_CNT is in Feature & Feature_EXP byte of FIS27 for FPDMA
			*((u16 *)(&(pCtxt->CTXT_Status))) = (LUN_STATUS_GOOD << 8)|(CTXT_STATUS_GOOD);
			usb_device_no_data(pCtxt);
			return 1;
		}
	}
	else
	{
		if (product_detail.options & PO_SUPPORT_WTG)	//Interlaken 2X is not a WTG product
		{
			#define FUA_BIT	BIT_3
			if (CmdBlk(1) & FUA_BIT)
			{
				if ((CmdBlk(0) != SCSI_WRITE6) && (CmdBlk(0) != SCSI_READ6)) goto _ILLEGAL_CDB;
			}
		}
		
		// Artemis doesn't support the protection information
		if (CmdBlk(1) & RDPROTECT_BITS)
		{
			if ((CmdBlk(0) != SCSI_WRITE6) && (CmdBlk(0) != SCSI_READ6))
				goto _ILLEGAL_CDB;
		}

		if (CmdBlk(1) & RELADR_BIT)
		{
			if ((CmdBlk(0) == SCSI_WRITE10) || (CmdBlk(0) == SCSI_READ10) || (CmdBlk(0) == SCSI_WRITE12) || (CmdBlk(0) == SCSI_READ12))
				goto _ILLEGAL_CDB;
		}
		
		//case 4
		if((CmdBlk(13)==0) && (CmdBlk(12)==0) && (CmdBlk(11)==0) && (CmdBlk(10)==0))
		{
			if (pCtxt->CTXT_PHASE_CASE & BIT_4)
			{//Hi > Dn or Ho > Dn
				hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
			}
			else
			{
				pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
			}
			usb_device_no_data(pCtxt);
			return 1;
		}
	}
	return 0;
}

/****************************************\
	scsi_StartAtaCmd
\****************************************/
#if 1
void scsi_StartAtaCmd(PSATA_OBJ  pSob, PCDB_CTXT	pCtxt)
{
#ifdef UAS
	uas_ci_paused |= UAS_PAUSE_WAIT_FOR_USB_RESOURCE;
	uas_ci_paused_ctxt = pCtxt;
#endif
	xmemcpy32((u32 *)(&CmdBlk(0)), cdb.AsUlong, 4);
	u8 slot_num = 0;
	if (arrayMode == JBOD)
	{
		slot_num = logicalUnit;
	}
	nextDirection = DIRECTION_NONE;
	pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
	pCtxt->CTXT_usb_state = CTXT_USB_STATE_CTXT_SETUP_DONE;
	u32 sect_cnt;
	u8 rebuild_delay = 0;
	
	switch ( CmdBlk(0) )
	{
#if 1
	case SCSI_ATA_PASS_THR:
		extended_descriptor = 0;
		//pCtxt->CTXT_FIS[FIS_COMMAND] = CmdBlk(9);		// COMMAND
		//pCtxt->CTXT_FIS[FIS_FEATURE] = CmdBlk(3);		// FEATURES (7:0)
		*((u32 *) (&pCtxt->CTXT_FIS[FIS_TYPE])) = (CmdBlk(3) << 24)|(CmdBlk(9) << 16)|(0x80 << 8)|(HOST2DEVICE_FIS);		// FEATURES(7:0) + COMMAND

		//pCtxt->CTXT_FIS[FIS_LBA_LOW] = CmdBlk(5);		// LBA (7:0)
		//pCtxt->CTXT_FIS[FIS_LBA_MID] = CmdBlk(6);		// LBA (15:8)
		//pCtxt->CTXT_FIS[FIS_LBA_HIGH] = CmdBlk(7);	// LBA (23:16)
		//pCtxt->CTXT_FIS[FIS_DEVICE] = CmdBlk(8);		// DRV_HEAD_REG
		*((u32 *) (&pCtxt->CTXT_FIS[FIS_LBA_LOW])) = (CmdBlk(8) << 24)|(CmdBlk(7) << 16)|(CmdBlk(6) << 8)|(CmdBlk(5));		// Device + LBA (23:0)

		*((u32 *) (&pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT])) = 0;		// 

		//pCtxt->CTXT_FIS[FIS_SEC_CNT] = CmdBlk(4);		// SECTOR_CNT_REG
		sect_cnt = CmdBlk(4);
		*((u32 *) (&pCtxt->CTXT_FIS[FIS_SEC_CNT])) = CmdBlk(4);		// SECTOR_CNT_REG

		goto SAT_PASSTHROUGH16;

	case SCSI_ATA_PASS_THR16:	//SCSI_ATA_PASS_THR = 0x85 (USED SCSI-ATA PASS-THROUGH 16 BYTES FORMAT)

		*((u32 *) (&pCtxt->CTXT_FIS[FIS_TYPE])) = (CmdBlk(4) << 24)|(CmdBlk(14) << 16)|(0x80 << 8)|(HOST2DEVICE_FIS);		//FEATURES (7:0) + COMMAND

		*((u32 *) (&pCtxt->CTXT_FIS[FIS_LBA_LOW])) = (CmdBlk(13) << 24)|(CmdBlk(12) << 16)|(CmdBlk(10) << 8)|(CmdBlk(8));	//Device + LBA (23:0)

		*((u32 *) (&pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT])) = (CmdBlk(3) << 24)|(CmdBlk(11) << 16)|(CmdBlk(9) << 8)|(CmdBlk(7));	//FEATURES (15:8) + LBA (47:24)

		sect_cnt = (CmdBlk(5) << 8) |CmdBlk(6);
		*((u32 *) (&pCtxt->CTXT_FIS[FIS_SEC_CNT])) = (CmdBlk(5) << 8) |CmdBlk(6);		// SECTOR_CNT(15:0)

		extended_descriptor = CmdBlk(1) & 1;
		
SAT_PASSTHROUGH16:

		//ata_passth_prepared = 1;
		if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
		{
			// F/W fails the ata security commands
			if ((pCtxt->CTXT_FIS[FIS_COMMAND] <0xF7) && (pCtxt->CTXT_FIS[FIS_COMMAND] >0xF1))
			{
				goto _ILL_CDB;
			}
			if ((IS_LOCKED(0) && (logicalUnit == HDD0_LUN)) || (IS_LOCKED(1) && (logicalUnit == HDD1_LUN)))
			{
				hdd_err_2_sense_data(pCtxt, ERROR_WRITE_PROTECT);
				break;
			}

			if (scsi_chk_hdd_pwr(pCtxt)) return;
		}
		u8 protocol = CmdBlk(1) & 0x1E;
		u8 t_Length = CmdBlk(2) & 0x03;
		if (arrayMode != JBOD)
		{
			pSob = (diag_drive_selected & SATA_CH1) ? &sata1Obj : &sata0Obj;
		}

		if (protocol == (0 << 1))			// ATA Hard Reset
		{
			MSG("A P\n");
			sata_Reset(pSob, SATA_HARD_RST);
			//delay_sec(off_line(pCtxt));
			//pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
			pCtxt->CTXT_LunStatus = LUN_STATUS_GOOD;			
			usb_device_no_data(pCtxt);
			return;
		}
		else if (protocol == (1 << 1))		// ATA Soft Reset
		{
			sata_Reset(pSob, SATA_SOFT_RST);
			//delay_sec(off_line(pCtxt));
			//pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
			pCtxt->CTXT_LunStatus = LUN_STATUS_GOOD;			
			usb_device_no_data(pCtxt);
			return;
		}
		else if (protocol == (3 << 1))		//non-data
		{
			pCtxt->CTXT_FLAG = CTXT_FLAG_U2B | CTXT_FLAG_B2S;
			pCtxt->CTXT_Prot = PROT_ND;
#ifdef UAS
			if (curMscMode == MODE_UAS)
				pCtxt->CTXT_XFRLENGTH = 0;
#endif
			pCtxt->CTXT_ccm_cmdinten = D2HFISIEN;
			pCtxt->CTXT_PHASE_CASE = 0x0002;
#ifdef FAST_CONTEXT
			pCtxt->CTXT_No_Data = MSC_TX_CTXT_NODATA;
#else
			pCtxt->CTXT_No_Data = (MSC_TX_CTXT_VALID|MSC_TX_CTXT_NODATA) >> 8;
#endif
			pCtxt->CTXT_DbufIndex = SEG_NULL;
			//pCtxt->CTXT_AES_DbufIndex = SEG_NULL;
			sata_exec_ctxt(pSob, pCtxt);										
			return;
		}
		else if (((protocol == (4 << 1)) || (protocol == (10 << 1))) && ((CmdBlk(2) & 0x08)) && (((t_Length == 0x02) && (CmdBlk(2) & 0x04)) || ((t_Length == 0x03) && ((CmdBlk(2) & 0x04) == 0))))	//PIO data-in / UDMA data-in
		{
			pCtxt->CTXT_FLAG = 0;

			if (protocol == (4 << 1))
				pCtxt->CTXT_Prot = PROT_PIOIN;
			else
				pCtxt->CTXT_Prot = PROT_DMAIN;
#ifdef UAS
			if (curMscMode == MODE_UAS)
			{
				if (t_Length == 0x03)
					goto _ILL_CDB;
				else  // The transfer length is an unsigned integer specified in the SECTOR_COUNT (7:0) field.
				{
					pCtxt->CTXT_XFRLENGTH = sect_cnt << 9;
					pCtxt->CTXT_CONTROL |= CTXT_CTRL_DIR;			// Data-in
				}
			}
#endif

			pCtxt->CTXT_ccm_cmdinten = 0; 
			// input port of TX buffer Seg 0  is SATA 0 Read
			// Output port of TX buffer Seg 0  is USB Write
			pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S0R;
			if (pSob->sobj_sata_ch_seq)
			{
				pCtxt->CTXT_DbufIndex |= DBUF_SATA1_CHANNEL;
			}

			// set case 6		
			pCtxt->CTXT_PHASE_CASE = 0x0040;
			sata_exec_ctxt(pSob, pCtxt);										
			return;
		}
		else if (((protocol == (5 << 1)) || (protocol == (11 << 1))) && (((CmdBlk(2) & 0x08)) == 0) && (((t_Length == 0x02) && (CmdBlk(2) & 0x04)) || ((t_Length == 0x03) && ((CmdBlk(2) & 0x04) == 0))))	//PIO data-out / UDMA data-out
		{
			pCtxt->CTXT_FLAG = 0;

			if (protocol == (5 << 1))
				pCtxt->CTXT_Prot = PROT_PIOOUT;
			else
				pCtxt->CTXT_Prot = PROT_DMAOUT;
#ifdef UAS
			if (curMscMode == MODE_UAS)
			{
				if (t_Length == 0x03)
					goto _ILL_CDB;
				else  // The transfer length is an unsigned integer specified in the SECTOR_COUNT (7:0) field.
				{
					pCtxt->CTXT_XFRLENGTH = sect_cnt << 9;
					pCtxt->CTXT_CONTROL &= ~CTXT_CTRL_DIR;			// Data-out
				}
			}
#endif	
			// set case 12		
			pCtxt->CTXT_PHASE_CASE = 0x1000;

			pCtxt->CTXT_ccm_cmdinten = 0; 
			pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S0W;
			if (pSob->sobj_sata_ch_seq)
			{
				pCtxt->CTXT_DbufIndex |= DBUF_SATA1_CHANNEL;
			}

			sata_exec_ctxt(pSob, pCtxt);	
			return;
		}
		else if (protocol == (15 << 1)) // Return response Information
		{
			// The correct behavior for ATA PASS THROUGH Protocol 15 is to prepare Sense data so that the next REQUEST SENSE command returns 
			// that information to the host.  For Protocol 15 the ATA PASS THROUGH command shall have no data transfer.  
			hdd_ata_return_tfr(pCtxt);
			usb_device_no_data(pCtxt);
			ata_passth_prepared = 1;		
			return;
		}
		else
		{
_ILL_CDB:
			hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
			usb_device_no_data(pCtxt);

			return;
		}
#endif

	/****************************************\
		SCSI_INQUIRY
	\****************************************/
	case SCSI_INQUIRY:
//		if(curMscMode == MODE_UAS)
//			usb_dir = 1;			// data-in

		DBG("\tInquiry HDD!");
		if (logicalUnit == UNSUPPORTED_LUN)
			scsi_inquiry_cmd(pCtxt, DEVICE_TYPE_UNKNOWN);
		else	
			scsi_inquiry_cmd(pCtxt, DEVICE_TYPE_DISK);	
		break;

	/****************************************\
		SCSI_READ_CAPACITY
	\****************************************/
	case SCSI_READ_CAPACITY:
		DBG("Read Capacity\n");
		if (cdb.byte[1] & RELADR_BIT)  // v1.023
		{
			hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
			break;
		}
		scsi_read_disk_capacity_cmd(pCtxt);
		break;

	/****************************************\
		SCSI_READ_CAPACITY16
	\****************************************/
	case SCSI_SERVICE_ACTION_IN16:
		DBG("Read Capacity 16\n");
		if ((cdb.byte[1] & SA_MASK) == SA_READ_CAPACITY16)
		{
			scsi_read_disk_capacity_cmd(pCtxt);
		}
		else
		{
			hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
		}
		break;

	case SCSI_REZERO:
		// REZERO UNIT is defined in SCSI-2 but has been obsolete since SBC.
		// This is needed for compatibility with some obsolete PCs ??
	  	nextDirection = DIRECTION_SEND_STATUS;	// Just returns GOOD status.
		break;
			
	/****************************************\
		SCSI_VERIFY16
	\****************************************/
	case SCSI_VERIFY16:
		if (CmdBlk(2) || CmdBlk(3) ||	//LBA > 48-bits 
			CmdBlk(10) || CmdBlk(11))	//transfer length > 0xffff
		{
			hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
			break;
		}
#ifdef ATA_5		
		if (pSob->sobj_deviceType == DEVICETYPE_HD_ATA5)
		{

			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_TYPE]))) = (ATA6_READ_VERIFY_SECTORS << 16)|(0x80 << 8)|(HOST2DEVICE_FIS);		// COMMAND
			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_LBA_LOW]))) = (0x40 << 24)|(CmdBlk(7) << 16)|(CmdBlk(8) << 8)|(CmdBlk(9));		// Device + LBA (27:0)
			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT]))) = ((CmdBlk(4) << 16)|(CmdBlk(5) << 8)|(CmdBlk(6)));		// 
			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_SEC_CNT]))) = CmdBlk(13);		// SECTOR_CNT(7:0)

		}
		else
#endif
		{
			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_TYPE]))) = (ATA6_READ_VERIFY_SECTORS_EXT << 16)|(0x80 << 8)|(HOST2DEVICE_FIS);	// COMMAND
			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_LBA_LOW]))) = (0x40 << 24)|(CmdBlk(7) << 16)|(CmdBlk(8) << 8)|(CmdBlk(9));		// Device + LBA (27:0)
			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT]))) = ((CmdBlk(4) << 16)|(CmdBlk(5) << 8)|(CmdBlk(6)));		// 
			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_SEC_CNT]))) = (CmdBlk(12) << 8)|CmdBlk(13);										// SECTOR_CNT(15:0)
		}
		goto _verify10;
	
	/****************************************\
		SCSI_VERIFY10
	\****************************************/
	case SCSI_VERIFY10 :
		if (cdb.byte[1] & RELADR_BIT) // v1.023
		{
			hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
			break;
		}
#ifdef ATA_5	
		if (pSob->sobj_deviceType == DEVICETYPE_HD_ATA5)
		{

			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_TYPE]))) = (ATA6_READ_VERIFY_SECTORS << 16)|(0x80 << 8)|(HOST2DEVICE_FIS);		// COMMAND
			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_LBA_LOW]))) = ((0x40|CmdBlk(2)) << 24)|(CmdBlk(3) << 16)|(CmdBlk(4) << 8)|(CmdBlk(5));		// Device + LBA (27:0)
			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT]))) = 0;		// 
			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_SEC_CNT]))) = CmdBlk(8);		// SECTOR_CNT(7:0)

		}
		else
#endif
		{
			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_TYPE]))) = (ATA6_READ_VERIFY_SECTORS_EXT << 16)|(0x80 << 8)|(HOST2DEVICE_FIS);	// COMMAND
			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_LBA_LOW]))) = (0x40 << 24)|(CmdBlk(3) << 16)|(CmdBlk(4) << 8)|(CmdBlk(5));		// Device + LBA (23:0)
			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT]))) = CmdBlk(2);													//LBA(31:24)
			*((u32 *)(&(pCtxt->CTXT_FIS[FIS_SEC_CNT]))) = (CmdBlk(7) << 8)|CmdBlk(8);										// SECTOR_CNT(15:0)
		}

_verify10:
		u64 lba = ((u64)pCtxt->CTXT_FIS[FIS_LBA_HIGH_EXT] << 40) | ((u64)pCtxt->CTXT_FIS[FIS_LBA_MID_EXT] << 32) | ((u64)pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT] << 24) 
				| ((u64)pCtxt->CTXT_FIS[FIS_LBA_HIGH] << 16) | ((u64)pCtxt->CTXT_FIS[FIS_LBA_MID] << 8) | pCtxt->CTXT_FIS[FIS_LBA_LOW];
		u32 xfer_cnt = *((u32 *)(&(pCtxt->CTXT_FIS[FIS_SEC_CNT])));
		if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
		{
			if (cdb.byte[1] & VRPROTECT_BITS)
			{
				hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
				break;
			}

			if (IS_LOCKED_OR_UNCONFIGED(0) || ( arrayMode == NOT_CONFIGURED))
			{
				hdd_err_2_sense_data(pCtxt, ERROR_WRITE_PROTECT);
				break;
			}	
			
			if (((lba + xfer_cnt) > user_data_area_size[slot_num]) || (lba > user_data_area_size[slot_num]))
			{
				hdd_err_2_sense_data(pCtxt, ERROR_LBA_OUT_RANGE);
				break;
			}
			
			if (scsi_chk_hdd_pwr(pCtxt)) return;
		}
		pCtxt->CTXT_FLAG = CTXT_FLAG_U2B|CTXT_FLAG_B2S;
		pCtxt->CTXT_Prot = PROT_ND;
		pCtxt->CTXT_DbufIndex = SEG_NULL;
		//pCtxt->CTXT_AES_DbufIndex = SEG_NULL;
		pCtxt->CTXT_PHASE_CASE = 0x0002;
		
		if (curMscMode == MODE_UAS)
			pCtxt->CTXT_XFRLENGTH = 0;
		pCtxt->CTXT_ccm_cmdinten = D2HFISI;
		if (sata_exec_ctxt(pSataObj, pCtxt) == CTXT_STATUS_ERROR)
		{	// sense Sense IU if error return 
//			usb_append_rx_que_ctxt(pCtxt);
			usb_device_no_data(pCtxt);
		}
		return;

	/****************************************\
		SCSI_MODE_SENSE
	\****************************************/
	case SCSI_MODE_SENSE10:
	/****************************************\
		SCSI_MODE_SENSE6
	\****************************************/
	case SCSI_MODE_SENSE6:
		scsi_mode_sense_cmd(pCtxt);
		break;

	/****************************************\
		SCSI_MODE_SELECT(10)
	\****************************************/
	case SCSI_MODE_SELECT10:   //
	case SCSI_MODE_SELECT6:  // 
		scsi_mode_select_cmd(pCtxt);
		break;
#if !defined(INITIO_STANDARD_FW) || defined(SCSI_COMPLY)
	case SCSI_FORMAT_UNIT:
		scsi_format_unit_cmd(pCtxt);
		break;
#endif
	/****************************************\
		SCSI_WRITE_BUFFER10
	\****************************************/
	case SCSI_WRITE_BUFFER10: 				// write buffer
		scsi_write_buffer10_cmd(pCtxt);
		break;
	/****************************************\
		SCSI_READ_BUFFER10
	\****************************************/
	case SCSI_READ_BUFFER10:      // read buffer
		scsi_read_buffer10_cmd(pCtxt);
		break;

	/****************************************\
		SCSI_TEST_UNIT_READY
	\****************************************/
	case SCSI_TEST_UNIT_READY:
		if ((pSataObj->sobj_State == SATA_RESETING) || (pSataObj->sobj_State == SATA_PHY_RDY))
			hdd_err_2_sense_data(pCtxt, ERROR_UA_BECOMING_READY);
		else
		{
			if (IS_LOCKED(slot_num))
			{
				hdd_err_2_sense_data(pCtxt, ERROR_WRITE_PROTECT);
			}
			else
			{
				nextDirection = DIRECTION_SEND_STATUS;
			}
		}
		break;

	/****************************************\
		SCSI_SYNCHRONIZE_CACHE
	\****************************************/
	case SCSI_SYNCHRONIZE_CACHE:	
		if (cdb.byte[1] & RELADR_BIT) // v1.023
		{
			hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
			break;
		}
	case SCSI_SYNCHRONIZE_CACHE16:	
		DBG("SCSI_SYNCHRONIZE_CACHE\n");
		u64 lba_num;
		u32 sec_cnt;

		if ( CmdBlk(0) == SCSI_SYNCHRONIZE_CACHE)
		{
			lba_num =  (cdb.byte[2] << 24) + (cdb.byte[3] << 16) + (cdb.byte[4] << 8) + cdb.byte[5];
			sec_cnt = (cdb.byte[7] << 8) + cdb.byte[8];
		}
		else
		{
			lba_num = ((u64)cdb.byte[2] << 56)+ ((u64)cdb.byte[3] << 48) + ((u64)cdb.byte[4] << 40) + ((u64)cdb.byte[5] << 32) + (cdb.byte[6] << 24) + (cdb.byte[7] << 16) + (cdb.byte[8] << 8) + cdb.byte[9];
			sec_cnt = (cdb.byte[10] << 24) + (cdb.byte[11] << 16) + (cdb.byte[12] << 8) + cdb.byte[13];
		}
		u64 tmp_capa;
		u8 tmp_user_bs_log2m9;
		if (arrayMode == JBOD)
		{
			if ((logicalUnit == HDD0_LUN) && (sata0Obj.sobj_device_state != SATA_NO_DEV))
			{
				tmp_capa = user_data_area_size[0];
				tmp_user_bs_log2m9 = vital_data[0].user_bs_log2m9;
			}
			else
			{
				tmp_capa = user_data_area_size[1];
				tmp_user_bs_log2m9 = vital_data[1].user_bs_log2m9;
			}
		}
		else
		{
			tmp_capa = user_data_area_size[0];
			tmp_user_bs_log2m9 = vital_data[0].user_bs_log2m9;
		}
		if ( (lba_num >= (tmp_capa >> tmp_user_bs_log2m9)) || ((lba_num + sec_cnt) > (tmp_capa >> tmp_user_bs_log2m9)))
		{
			hdd_err_2_sense_data(pCtxt, ERROR_LBA_OUT_RANGE);
			break;
		}
		scsi_sync_cache_cmd(pCtxt);
		break;

	/****************************************\
		SCSI_START_STOP_UNIT
	\****************************************/
	case SCSI_START_STOP_UNIT:   // it seems that nothing may be done
		scsi_start_stop_cmd(pCtxt);
		break;		

	case SCSI_REPORT_LUNS:
		scsi_report_luns_cmd(pCtxt);
		break;

	case SCSI_REQUEST_SENSE:
		scsi_request_sense_cmd(pSob, pCtxt);
		break;

	// Diagnostics.
	case SCSI_RCV_DIAG_RESULTS:
		diag_receive_diag_results_cmd(pCtxt);
		break;

	case SCSI_SEND_DIAGNOSTIC:
		diag_send_diagnostic_cmd(pCtxt);
		break;

	case SCSI_LOG_SELECT:
		diag_log_select_cmd(pCtxt);			
		break;

	case SCSI_LOG_SENSE:
		diag_log_sense_cmd(pCtxt);
		break;

	// Reading/writing the handy store.
#ifdef WDC_HANDY
	case SCSI_WDC_READ_HANDY_CAPACITY:
		handy_read_handy_capacity_cmd(pCtxt); // --->v0.07_14.32_0404_A
		break;

	case SCSI_WDC_WRITE_HANDY_STORE:
	case SCSI_WDC_READ_HANDY_STORE:
		handy_read_write_store_cmd(pCtxt);
		break;
#endif

#ifdef WDC_RAID
	case SCSI_WDC_MAINTENANCE_IN:
		raid_wdc_maintenance_in_cmd(pCtxt);
		break;

	case SCSI_WDC_MAINTENANCE_OUT:
		raid_wdc_maintenance_out_cmd(pCtxt);
		break;
#endif
#ifndef INITIO_STANDARD_FW
	// Data-encryption (security) commands.
	//
	case SCSI_WDC_ENCRYPTION_STATUS:
		aes_encryption_status_cmd(pCtxt);
		break;

	case SCSI_WDC_SECURITY_CMD:
		aes_security_cmd(pCtxt);
		break;	

	case SCSI_WDC_FACTORY_CONFIG:
		factory_config(pCtxt);
		break;

#endif
	/****************************************\
		default
	\****************************************/
	default:
		MSG(" IL_OP %BX\n", CmdBlk(0));
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_OP);
		break;
	}
	
	if (nextDirection == DIRECTION_SEND_DATA)
	{
		byteCnt = Min(byteCnt, alloc_len); // check the allocation length
		if (dbg_next_cmd)
			MSG("xfer len %lx\n", byteCnt);
		if (rebuild_delay)
			raid1_active_timer = RAID_REBUILD_VERIFY_ONLINE_ACTIVE_TIMER;
		usb_device_data_in(pCtxt, byteCnt);
	}
	else if (nextDirection == DIRECTION_SEND_STATUS)
	{
		usb_device_no_data(pCtxt);
	}
}
#endif


void scsi_post_data_out(PCDB_CTXT pCtxt)
{
	DBG("post data out\n");
	u32 cmdErrorStatus = NO_ERROR;
	
	if (Rx_Dbuf->dbuf_IntStatus & DBUF_RDRDY)// wait for Data Out Done
	{

		if (read_data_from_cpu_port(pCtxt->CTXT_XFRLENGTH, DIR_RX, 0) == 0)
		{
			return;
		}
		
		// seg_done shall be setted after read out the data
		set_dbufDone(pCtxt->CTXT_DbufIndex);
	}
	else
	{
		return;
	}

	usb_ctxt.post_dout = 0;
	usb_rx_fifo_rst();
	reset_dbuf_seg(pCtxt->CTXT_DbufIndex);
	free_dbufConnection(pCtxt->CTXT_DbufIndex);

	pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
	
	logicalUnit = pCtxt->CTXT_LUN;
	xmemcpy32((u32 *)(&CmdBlk(0)), cdb.AsUlong, 4);	
	
	switch (cdb.opcode)
	{
	case SCSI_MODE_SELECT6:  
	case SCSI_MODE_SELECT10: 
		// process Mode Parameter
		scsi_mode_select_cmd(pCtxt);
		break;
		
	case SCSI_SEND_DIAGNOSTIC:
		diag_send_diagnostic_cmd(pCtxt);
		break;
		
#ifndef INITIO_STANDARD_FW
	case SCSI_WDC_FACTORY_CONFIG:
		factory_config(pCtxt);
		break;
		
	case SCSI_WDC_SECURITY_CMD:
		aes_security_cmd(pCtxt);
		break;
		
#ifdef WDC_RAID
	case SCSI_WDC_MAINTENANCE_OUT:
		raid_wdc_maintenance_out_cmd(pCtxt);
		break;
#endif

#endif

	case SCSI_WRITE_BUFFER10:	
		if (cdb.byte[1] == INI_VEN_CMD)	//initio vendor unique scsi command.
		{
			switch(cdb.byte[2])
			{	
				case INI_VEN_FW_DOWNLOAD_CTL:
						if ((CmdBlk(6) == 0x00) &&
						(CmdBlk(7) == 0x00) &&
						(CmdBlk(8) == 0x08))
						{
							if ((mc_buffer[0] != 0x25) ||
							(mc_buffer[1] != 0xc9) ||
							(mc_buffer[2] != 0x36) ||
							(mc_buffer[3] != 0x10))
							{
								cmdErrorStatus = ERROR_ILL_CDB;
								goto lj_post_data_out_err_check;
							}

							if(mc_buffer[7] == INI_SUB_START_FW_DOWNLOADING)
							{
								start_fw_downloading = 1;
							}
							
							else if(mc_buffer[7] == INI_SUB_ACTIVATE_CODE_IN_FLASH)
							{
								download_microcode_ctrl = ACTIVATE_DEFERED_FLASH;
								pwr_off_enable = 1; // return to main code firstly
							}
							else
							{
								cmdErrorStatus = ERROR_ILL_CDB;
								goto lj_post_data_out_err_check;
							}
						}
						break;
						
				case INI_VEN_SPI_DOWNLOAD_BINARY:
						if ((CmdBlk(5) == 0x00) &&
						(CmdBlk(6) == 0x00) &&
						(CmdBlk(7) == 0x02) &&
						(CmdBlk(8) == 0x00))
						{
							xmemcpy(mc_buffer, tmp_buffer, 512);
					
							spi_write_flash(tmp_buffer, current_image_offset + INI_FW_OFFSET, 512);		
							spi_read_flash(mc_buffer, current_image_offset + INI_FW_OFFSET, 512);

							if (xmemcmp(mc_buffer, tmp_buffer, 512))
							{
								flash_write_error_handle(current_image_offset, 1);
								cmdErrorStatus = ERROR_HW_FAIL_INTERNAL;
								goto lj_post_data_out_err_check;
							}
							current_image_offset += 0x200;
							u16 crc = 0;
							if (current_image_offset == MAX_FW_IMAGE_SIZE_INI)
							{
								crc = crc16(mc_buffer, 510);
								*crc_Ctrl = 0;
								if (crc)
								{	
									spi_erase_flash_sector(LAST_SECTOR_PRIM);
									cmdErrorStatus = ERROR_ILL_PARAM;
									goto lj_post_data_out_err_check;
								}
								else
								{
									download_microcode_ctrl = DOWNLOAD_FLASH_DONE;
									current_image_offset = 0;
								}
							}
							else
							{
								crc = crc16(mc_buffer, 512);
							}							
						}
						break;
					 
				case INI_VEN_SPI_WRITE_NVRAM:
						if ((CmdBlk(3) == 0x00) &&
						(CmdBlk(4) == 0x00) &&
						(CmdBlk(5) == 0x00) &&
						(CmdBlk(6) == 0x00) &&
						(CmdBlk(7) == 0x01) &&
						(CmdBlk(8) == 0x00))
						{
							if ((mc_buffer[0] != 0x25) && (mc_buffer[1] != 0xc9))
							{
								cmdErrorStatus = ERROR_ILL_PARAM;
								goto lj_post_data_out_err_check;
							}
							else
							{
								xmemcpy(mc_buffer, tmp_buffer, 256);
								spi_write_flash(tmp_buffer, GLOBAL_NVRAM_OFFSET, 256);		
								spi_read_flash(mc_buffer, GLOBAL_NVRAM_OFFSET, 256);
									
								if (xmemcmp(mc_buffer, tmp_buffer, 256))
								{	// wait for AP to retry again
									cmdErrorStatus = ERROR_HW_FAIL_INTERNAL;
									goto lj_post_data_out_err_check;
								}						
							}
						}
						break;

				case INI_VEN_SET_USB_PORT_ID:
						if ((CmdBlk(6) == 0x00) &&
						(CmdBlk(7) == 0x00) &&
						(CmdBlk(8) == 0x08))
						{
							if ((mc_buffer[0] != 0x25) &&
							(mc_buffer[1] != 0xc9) &&
							(mc_buffer[2] != 0x36) &&
							(mc_buffer[3] != 0x10))
							{
								cmdErrorStatus = ERROR_ILL_CDB;
								goto lj_post_data_out_err_check;
							}

							USB_AdapterID = mc_buffer[4];
							USB_PortID0 = mc_buffer[5];
							USB_PortID1 = mc_buffer[6];
							USB_PortID2 = mc_buffer[7];
							*cpu_SCRATCH0_reg = (((u32)USB_AdapterID<<24) | ((u32)USB_PortID2<<16)|((u32)USB_PortID1<<8)|USB_PortID0);
						}
						break;
						
				case INI_VEN_FUN_GSPI_SYNC_BURST_F0:
						if((cdb.byte[3] == 0x00)
						&& (cdb.byte[4] == 0x00)
						&& (cdb.byte[5] == 0x00)
						&& (cdb.byte[6] == 0x00)
						&& (cdb.byte[7] == 0x00)
						&& (cdb.byte[8] == 0x10)
						&& (cdb.byte[9] == 0x00))
						{
							MSG("GSPI Async\n");
							enable_GSPI_function();
							for (u8 i = 0; i < 0x10; i++)
							{
								MSG("%bx\n", mc_buffer[i]);
								gSPI_byte_xfer(mc_buffer[i]);
							}
						}
						break;

				case INI_VEN_FUN_GSPI_SYNC_BURST_F1:
				case INI_VEN_FUN_GSPI_SYNC_BURST_9BITS:
						if((cdb.byte[3] == 0x00)
						&& (cdb.byte[4] == 0x00)
						&& (cdb.byte[5] == 0x00)
						&& (cdb.byte[6] == 0x00)
						&& (cdb.byte[7] == 0x02)
						&& (cdb.byte[8] == 0x00)
						&& (cdb.byte[9] == 0x00))
						{
							MSG("GSPI Sync");
							u8 gSPI_xfer_mode = 0;
							if (cdb.byte[2] == 0xF1)
							{
								MSG(" 8bits");
								gSPI_xfer_mode = GSPI_SYNC_BURST_MODE;
							}
							else
							{
								MSG(" 9bits");
								gSPI_xfer_mode = GSPI_SYNC_BURST_MODE_9BITS;
							}
							MSG("\n%bx, %bx, %bx, %bx\n", mc_buffer[0], mc_buffer[1], mc_buffer[510], mc_buffer[511]);
							enable_GSPI_function();
							gSPI_burst_xfer(mc_buffer, 512, gSPI_xfer_mode, GSPI_DIR_OUT);
							gSPI_burst_xfer(mc_buffer, 512, gSPI_xfer_mode, GSPI_DIR_IN);
						}
						break;	
						
				default:
					break;
			}

		}
#ifndef WDC_RAID
		else if (cdb.byte[1] == RAID_SERVICE_ACTION)
		{
			raid_wdc_maintenance_out_cmd(pCtxt);
			break;
		}
#endif				
#ifdef WDC_DOWNLOAD_MICROCODE
		else if (cdb.byte[1] == WD_CDB_DL_MICROCODE_MODE)
		{
			if(cdb.byte[2] == 0x00)
			{
				// file image format
				/*************************************************
				*** byte[0~511] Firmware header
				*** byte[512~n] Firmware code
				*** n should be multiple of 2^offsetBoundary
				*************************************************/
				u32 alloc_len = (cdb.byte[6] << 16) + (cdb.byte[7] << 8) + cdb.byte[8];
				u32 buffer_offset = (cdb.byte[3] << 16) + (cdb.byte[4] << 8) + cdb.byte[5];
				u32 flash_addr = check_fw_base_addr(cdb.byte[2]);
				DBG("w buf_offset %lx\n", buffer_offset);

				if ((alloc_len == 512) && (buffer_offset < MAX_FW_IMAGE_SIZE_WD)) // buffer offset shall be less than buf capacity
				{
					// check firmware header
					if (buffer_offset == 0)
					{
						// check chips signature
						if ((mc_buffer[2] != 0x25) || 
						     (mc_buffer[3] != 0xc9) ||
						     (mc_buffer[4] != 0x36) ||
						     (mc_buffer[5] != 0x10))
						{
							cmdErrorStatus = ERROR_ILL_CDB;
							goto lj_post_data_out_err_check;			
						}
						
						// check wd signature
						else if ((mc_buffer[6] != 0x03) || 
						     (mc_buffer[7] != 0x03) ||
						     (mc_buffer[8] != 0x05) ||
						     (mc_buffer[9] != 0x05))
						{
							cmdErrorStatus = ERROR_ILL_CDB;
							goto lj_post_data_out_err_check;
						}
						else
						{
							*crc_Ctrl = 0; 
							next_buffer_offset = 0x200;

							spi_erase_flash_sector(flash_addr - 0x1000); // erase 
							spi_write_flash(mc_buffer, flash_addr-512, 512);		
							spi_read_flash(&mc_buffer[2048], flash_addr-512, 512);
							//mc_buffer[0] += 1; // for test purpose, suppose it will fail the download microcode command
							if (xmemcmp(mc_buffer, &mc_buffer[2048], 512))
							{
								MSG("MisH\n");
								hdd_err_2_sense_data(pCtxt, ERROR_HW_FAIL_INTERNAL);
								break;
							}
							
							crc16(mc_buffer, 512);	
							MSG("start load FW\n");
						}
						
						download_microcode_ctrl = DOWNLOAD_MICROCODE_START;
					}
					else if ((download_microcode_ctrl == DOWNLOAD_MICROCODE_START) && (next_buffer_offset == buffer_offset))
					{
						// save code
						if (buffer_offset == FW_IMAGE_LAST_SECTOR_OFFSET_WD) 
						{	
							if (crc16(mc_buffer, 512))
							{
								MSG("Crc error\n");
								hdd_err_2_sense_data(pCtxt, ERROR_ILL_PARAM);
								break;
							}
							*crc_Ctrl = 0; 
							// recalculate the CRC again
							// backup the last block data
							xmemcpy(mc_buffer, &mc_buffer[4095-512], 512);

							u32 flash_addr_off = 0;
							while (flash_addr_off < FW_IMAGE_LAST_SECTOR_OFFSET_WD)
							{
								if (flash_addr_off == (FW_IMAGE_LAST_SECTOR_OFFSET_WD - 0x400))
								{
									spi_read_flash((u8 *)(&mc_buffer[0]), flash_addr+ flash_addr_off -0x200, 0x400);
									 crc16(mc_buffer, 0x400);
									 flash_addr_off+=0x400;
								}
								else
								{
									spi_read_flash((u8 *)(&mc_buffer[0]), flash_addr+ flash_addr_off -0x200, 0xC00);
									crc16(mc_buffer, 0xC00);
									 flash_addr_off+=0xC00;
								}							
							}
							
							xmemcpy(&mc_buffer[4095-512], mc_buffer, 512);							
							u32 cur_generation;
							#define FW_GENERATION_OFFSET 0x1BFE4
							#define FW_GENERATION_LOCAL_OFFSET 0x1E4
							MSG("active code %lx\n", *fw_tmp);
							if (*fw_tmp & BOOT_FROM_PRIMARY)
							{
								spi_read_flash((u8 *)(&mc_buffer[3072]), 0x4000+FW_GENERATION_OFFSET, 4);
							}
							else
							{
								spi_read_flash((u8 *)(&mc_buffer[3072]), 0x24000+FW_GENERATION_OFFSET, 4);
							}
							
							cur_generation = ((u32)mc_buffer[3072] << 24) + ((u32)mc_buffer[3073] << 16) + ((u32)mc_buffer[3074] << 8) + mc_buffer[3075];
							
							MSG("cur gen %lx\n", cur_generation);

							cur_generation++;
							WRITE_U32_BE(&mc_buffer[FW_GENERATION_LOCAL_OFFSET], cur_generation);
							
							MSG("new gen %bx,%bx,%bx,%bx\n", mc_buffer[FW_GENERATION_LOCAL_OFFSET], mc_buffer[FW_GENERATION_LOCAL_OFFSET+1], mc_buffer[FW_GENERATION_LOCAL_OFFSET+2], mc_buffer[FW_GENERATION_LOCAL_OFFSET+3]);						

							u16 new_crc = crc16(&mc_buffer[0], 510);
#if 0
							MSG("new crc %x\n", new_crc);
							
							mc_buffer[508] = (u8)(new_crc>>8);
							mc_buffer[509] = (u8)new_crc;
							MSG("mc_buf[508] %bx,  mc_buf[509] %bx\n", mc_buffer[508], mc_buffer[509]);
							new_crc = crc16(&mc_buffer[508], 2);
#endif
							mc_buffer[510] = (u8)(new_crc>>8);
							mc_buffer[511] = (u8)new_crc;
							MSG("mc_buf[510] %bx,  mc_buf[511] %bx\n", mc_buffer[510], mc_buffer[511]);
							
							*crc_Ctrl = 0; // stop CRC engine
							MSG("stop CRC\n");
						}
						
						flash_addr = flash_addr + buffer_offset -512;
						
						DBG("W F addr %lx\n", flash_addr);
						
						spi_write_flash(mc_buffer, flash_addr, 512);		
						spi_read_flash(&mc_buffer[2048], flash_addr, 512);
						
						if (xmemcmp(mc_buffer, &mc_buffer[2048], 512))
						{
							MSG("Mis C\n");

							spi_flash_write_fail_handle(flash_addr);
							spi_erase_flash_sector((flash_addr& 0xFFFF000));
							cmdErrorStatus = ERROR_HW_FAIL_INTERNAL;
							goto lj_post_data_out_err_check;
						}
						
						if (buffer_offset != FW_IMAGE_LAST_SECTOR_OFFSET_WD)
						{
							crc16(mc_buffer, 512);
						}

						next_buffer_offset = buffer_offset + 512;
						if (buffer_offset == FW_IMAGE_LAST_SECTOR_OFFSET_WD) // if it's last sector
						{
							download_microcode_ctrl = DOWNLOAD_MICROCODE_DONE;
						}

					}
				}
				else
				{
					cmdErrorStatus = ERROR_COMMAND_SEQUENCE;
					goto lj_post_data_out_err_check;
				}
			}
			else
			{
				cmdErrorStatus = ERROR_ILL_CDB;
				goto lj_post_data_out_err_check;
			}
		}
#endif
		break;


	default:
		break;

	}
	
lj_post_data_out_err_check:
	if(cmdErrorStatus != 0)
	{
		hdd_err_2_sense_data(pCtxt, cmdErrorStatus);
	}

	if (*usb_IntStatus_shadow & (HOT_RESET|WARM_RESET|USB_BUS_RST))
	{
#if (PWR_SAVING)
		turn_on_USB23_pwr();
#endif
		return;
	}

	usb_ctxt_send_status(pCtxt);
}
int   scsi_wdc_param_check(int type, PCDB_CTXT pCtxt)
{
	if(type == TYPE_DESTSLOT)
	{
		if(!(cdb.byte[6] & BIT_CP_USESLOT))	  // --->v0.07_14.32_0328_A
		{
			if(cdb.byte[6] & MASK_CP_DESTSLOT)
			{
				goto ERR_WDC_ILL_CDB;
			}
		}
		else
		{
			// 0x80, 0x81 to here
			// use slot is set to 1, the destination slot shall set to 0, 1 and the corresponding drive shall be attached
			if(!((((cdb.byte[6] & MASK_CP_DESTSLOT)==1)  && (sata1Obj.sobj_device_state == SATA_DEV_READY)) ||
				(((cdb.byte[6] & MASK_CP_DESTSLOT)==0)  && (sata0Obj.sobj_device_state == SATA_DEV_READY))))
			{
				MSG("cd %bx st %bx %bx\n", (cdb.byte[6] & MASK_CP_DESTSLOT), sata0Obj.sobj_device_state, sata1Obj.sobj_device_state);
				goto ERR_WDC_ILL_CDB;
			}
		}
	}
	
	else if(type == TYPE_DESTLUN)
	{
		if (cdb.byte[6] & BIT_CP_USELUN)	  // --->v0.07_14.32_0328_A
		{
			// if the USE LUN is one, the DESTINATION DISK LUN field shall be 0 or 1 because interlaken 2X can support up to two disk Luns
			// and the command shall be sent to non-disk lun 
			if(!(((((cdb.byte[6] & MASK_CP_DESTLUN)==1) && (hdd1_lun != UNSUPPORTED_LUN) ) || ((cdb.byte[6] & MASK_CP_DESTLUN) == 0))
				&& (logicalUnit == ses_lun))) 
			{
				goto ERR_WDC_ILL_CDB;
			}

		}
		else
		{
			// if the USE_LUN is 0, the DESTINATION DISK LUN shall be 0, and it shall be sent to Disk LUN
			// otherwise, shall fail the command
			if((cdb.byte[6] & MASK_CP_DESTLUN) ||(logicalUnit == ses_lun))
			{
				goto ERR_WDC_ILL_CDB;
			}
		}
	}

	return 0;

ERR_WDC_ILL_CDB:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
	return 1;	
}


