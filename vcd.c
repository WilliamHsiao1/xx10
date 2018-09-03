/*****************************************************************************
 *   Copyright (C) Initio Corporation 2006-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/
#define VCD_C
#include "general.h"

// Maximum number of blocks that can be transfered using the READ- and
// WRITE VIRTUAL CD commands.
#define MAX_RW_VCD_BLOCK			0xFFFF

//
// Global Variables
//
#define	PREVENT_ALLOW__UNLOCK_ALL	0
#define	PREVENT_ALLOW__DRIVE_LOCK	1

/************************************************************************\
 vcd_set_media_presence() - Inserts or removes the virtual CD media.

 The virtual CD can be read when it is present in the drive (duh!)
 Otherwise NOT READY errors will be returned for media-access commands.

 Arguments:
	bPresent	true to "insert" the virtual CD into the drive.

 Returns:
	Nothing.
 */
void vcd_set_media_presence(u8 bPresent)
{
#if 1
	// Don't insert the virtual CD if no space has been allocated for it.
	//only one vcd, so just check ram_rp[0].vcd_size
	if ((ram_rp[0].vcd_size == 0) && (ram_rp[1].vcd_size == 0))  bPresent = 0;
	
	// If the disc is changing from not-present to present, then
	// generate a unit attention condition.
	if (bPresent && !cd_media_present)
	{
		cd_unit_attentions |= UA_MEDIUM_CHANGED;
	}
#endif

	cd_media_present = bPresent;
}

/************************************************************************\
 report_unit_attention() - Checks and reports Unit Attention conditions.

 This should be called by all command handlers except the ones that
 must always succeed (INQUIRY, REQUEST SENSE, etc.)

 Returns:
	true if a Unit Attention occured and was reported.
 */
u8 report_unit_attention(PCDB_CTXT pCtxt)
{
	// When a Unit Attention is reported,
	// clear its flag so it isn't reported again.
	if (cd_unit_attentions & UA_MEDIUM_CHANGED)
	{
		hdd_err_2_sense_data(pCtxt, ERROR_MEDIA_CHANGED);
		cd_unit_attentions &=  ~UA_MEDIUM_CHANGED;		
		return 1;
	}
	else
	{
		return 0;		// Nothing to report.
	}
}

/************************************************************************\
 xlate_lba_to_msf() - Converts a CD-ROM block address to MSF format.

 Arguments:
	lba			CD-ROM logical block address (blocks are 2048 bytes)

 Returns:
	packed MSF: bits 16..23 is Minutes, bits 8..15 is Seconds, and the
	lowest byte is Frames. This can be used directly by most MMC structures.
 */
u32 xlate_lba_to_msf(u32 lba)
{
u8	min, sec;
	DBG("lba %lx\n", lba);

	// The data track starts at MSF 0/2/0, so add two seconds' worth of
	// frames to the LBA before converting it.
	lba += 150;					// 2 seconds * 75 frames(blocks) per second

	min  = lba / (60 * 75);				// Minutes
	lba -= (u32)min * 60 * 75;

	sec  = lba / 75;					// Seconds
	lba -= (u32)sec * 75;

	DBG("min %bx, sec %bx, lba %lx\n", min, sec, lba);

	// Remaining 'lba' is frames, range is naturally 0..74.

	// Pack the M, S and F into a 32-bit number.
	return  ((u32)min << 16) | ((u32)sec << 8) | lba;
}

/************************************************************************\
 read_toc_cmd() - READ TOC command handler.

 */
void read_toc_cmd(PCDB_CTXT pCtxt)
{
u8 *ptr8;

	// Much of the TOC data is filled with 0, so pre-clear the buffer.
	xmemset(mc_buffer, 0, 128);

	cdb.read_toc.format &= TOC_FORMAT_MASK;  // Isolate just the Format field.

	// Backwards-compatibility with SFF-8020:
	// If the Format field in byte 2 is zero, use the
	// top two bits of the Control field (byte 9) as Format.
	if (cdb.read_toc.format == 0)
	{
		cdb.read_toc.format = cdb.read_toc.control >> 6;

		// SFF-8020 only defines formats 0..2
		if (cdb.read_toc.format > 2)	goto invalid_CDB_field;
	}
	nextDirection = DIRECTION_SEND_DATA;
	alloc_len = (cdb.byte[7] << 8) + cdb.byte[8];

	switch (cdb.read_toc.format)
	{
	case TOC_FORMAT_BASIC:				// TOC data
		// Check Track Number field in CDB
		if ( (cdb.read_toc.track_session != 0) &&
		     (cdb.read_toc.track_session != 1) &&
		     (cdb.read_toc.track_session != 0xaa) )
		{
			goto invalid_CDB_field;
		}

   		// Fill header, leave TOC Data Length field for later.
		mc_buffer[2] = 1;		// First track #
		mc_buffer[3] = 1;		// Last track #

		// Start filling in the track descriptors
		ptr8 = &mc_buffer[4];

		// Fill in track 1's descriptor only if asked for.
		if ( (cdb.read_toc.track_session == 0) ||
		     (cdb.read_toc.track_session == 1) )
		{
			ptr8[1] = 0x14;		// ADR and Control
			ptr8[2] = 1;		// Track #

			if (cdb.read_toc.msf & 0x02)
			{
				// Starting MSF address of this track in bytes[5..7]
				// This track starts at MSF 0/2/0
				ptr8[6] = 2;
			}
			else
			{
				// Starting LBA of this track in bytes[4..7]
				// Starting LBA is 0; already set!
			}

			ptr8 += 8;			// Each track descriptor is 8 bytes
		}

		// Fill in lead-out area track descriptor (track 0xAA)
		ptr8[1] = 0x14;			// ADR and Control
		ptr8[2] = 0xaa;			// Track #

		// The lead-out area starts immediately after track 1, so
		// its starting LBA is the same as the size of the virtual CD.
		if (cdb.read_toc.msf & 0x02)
		{
			// Starting MSF address of this "track" in bytes[5..7]
//			*((u32 *)&ptr8[4]) = xlate_lba_to_msf(IniData.vcd_size >> 2);
//			*((u32 *)&ptr8[4]) = xlate_lba_to_msf(INI_VCD_SIZE >> 2);
			u32 temp32 = xlate_lba_to_msf(INI_VCD_SIZE >> 2);
			DBG("%lx\n", temp32);
			WRITE_U32_BE(&ptr8[4], temp32);
		}
		else
		{
			// Starting LBA of this "track" in bytes[4..7].
//			*((u32 *)&ptr8[4]) =  INI_VCD_SIZE >> 2;
			WRITE_U32_BE(&ptr8[4], INI_VCD_SIZE >> 2);
		}

		ptr8 += 8;

		byteCnt = ptr8 - mc_buffer;

		// Fill in TOC Data Length field; the length field doesn't include itself.
		mc_buffer[1] = byteCnt -2;
		break;

	case TOC_FORMAT_SESSION_INFO:		// Session Information
		// 1st session#, last session#, & last complete session address

		mc_buffer[0] = 0;			// TOC Data Length field, 16-bits
		mc_buffer[1] = 10;	
		mc_buffer[2] = 1; 			// 1st session # 
		mc_buffer[3] = 1;			// last session #

		mc_buffer[4+1] = 0x14; 		// ADR and Control
		mc_buffer[4+2] = 1; 		// 1st track # in last sesseion 

		// Addfress of 1st track in last session
		if (cdb.read_toc.msf & 0x02)		// Check MSF bit
		{
			// Starting MSF address of this track in bytes[5..7]
			// This track starts at MSF 0/2/0
			mc_buffer[4+6] = 2;
		}
		else
		{
			// Starting LBA of this track in bytes[4..7]
			// Starting LBA is 0, so it is already set!
		}

		byteCnt = 12;
		break;

	case TOC_FORMAT_FULL: 				// Full TOC
		// Return TOC data starting with the session # given in the CDB.
		// This virtual CD only has one session, so the starting session #
		// has to be either 0 or 1.

		if (cdb.read_toc.track_session & 0xfe)	// Allow only 0 and 1.
		{
			goto invalid_CDB_field;
		}
		else
		{
			// Only one session so data is the always the same:
			const u8 TOC_Data[] = {
				// Header: length MSB & LSB, first and last session #s
				0x00, 0x00, 1, 1,
				// Point 0xA0 descriptor: first track #, disc type
				0x01, 0x14, 0, 0xA0,   0,0,0, 0,   1,  0,  0,
				// Point 0xA1 descriptor: last track #
				0x01, 0x14, 0, 0xA1,   0,0,0, 0,   1,  0,  0,
				// Point 0xA2 descriptor: start of lead out (MSF), set below!
				0x01, 0x14, 0, 0xA2,   0,0,0, 0,  79, 59, 74,	// Placeholder MSF values.
				// Track 1 descriptor: start of track (MSF)
				0x01, 0x14, 0, 0x01,   0,0,0, 0,   0,  2,  0
			};

			xmemcpy((u8 *)TOC_Data, mc_buffer, sizeof(TOC_Data));

			// Set the lead-out's starting MSF address.
			// The offset must match the template's point 0xA2 descriptor!
	//		*(u32*)(&mc_buffer[33]) = xlate_lba_to_msf(IniData.vcd_size >> 2);
			u32 tmp32 = xlate_lba_to_msf(INI_VCD_SIZE >> 2);
			WRITE_U32_BE(&mc_buffer[33], tmp32);

			// Fill in TOC Data Length field (it doesn't include itself)
			mc_buffer[1] = sizeof(TOC_Data) - 2;

			byteCnt = sizeof(TOC_Data);
		}
		break;

	default:			// Invalid or unsuppoorted Format
		goto invalid_CDB_field;

	}	// end switch (cdb.read_toc.format)

	return;


invalid_CDB_field:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
	return;
}

/************************************************************************\
 vcd_start_command() - Command dispatcher for the virtual CD-ROM.

 This is called when a new command is received for this logical unit.

 */
#ifdef WDC_VCD
void vcd_start_command(PCDB_CTXT pCtxt)
{
#ifdef UAS
	uas_ci_paused |= UAS_PAUSE_WAIT_FOR_USB_RESOURCE;
	uas_ci_paused_ctxt = pCtxt;
#endif
	u32 lba_num,sec_cnt;

	xmemcpy32((u32 *)(&CmdBlk(0)), cdb.AsUlong, 4);
	nextDirection = DIRECTION_NONE;
	pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
	pCtxt->CTXT_usb_state = CTXT_USB_STATE_CTXT_SETUP_DONE;
	
	// --->v0.07_14.32_0404_A		
	if(cdb.opcode==SCSI_REQUEST_SENSE)
	{
		scsi_request_sense_cmd(pSataObj, pCtxt);
		goto jmp_VCD_CMD_NEXT_DIR;
	}
	
	if(cdb.opcode==SCSI_INQUIRY)	
	{
		scsi_inquiry_cmd(pCtxt, DEVICE_TYPE_CDROM);
		goto jmp_VCD_CMD_NEXT_DIR;
	}

	if(cdb.opcode==SCSI_READ_BUFFER10)	
	{
		scsi_read_buffer10_cmd(pCtxt);
		goto jmp_VCD_CMD_NEXT_DIR;
	}

	if(cdb.opcode==SCSI_WRITE_BUFFER10)
	{
		scsi_write_buffer10_cmd(pCtxt);
		goto jmp_VCD_CMD_NEXT_DIR;
	}

	if (report_unit_attention(pCtxt))  goto jmp_VCD_CMD_NEXT_DIR;
	
	switch (cdb.opcode)
	{

		case SCSI_READ10:
		case SCSI_READ12:
		{
			//---> v0.07_14.19_0327_A
			if (cdb.byte[1] & (RELADR_BIT|BIT_CP_DPO|BIT_CP_FUA))  goto invalid_CDB_field;
			if (!cd_media_present)  goto no_media;

			if(cdb.opcode == SCSI_READ12)
			{
				// The RD Protect, DPO, FUA, FUA_NV and Group Number fields
				// are not supported and therefore ignored.
				// Rearrange and execute this command as if it were a READ(16).
				// Copy the Transfer Length field.
				cdb.byte[10] = cdb.byte[6];
				cdb.byte[11] = cdb.byte[7];
				cdb.byte[12] = cdb.byte[8];
				cdb.byte[13] = cdb.byte[9];
			}
			else
			{
				// Make room in the CDB for 40-bit LBA math by converting
				// this command into a READ(16).
				DBG("rd10 vcd\n");
				cdb.byte[10] = 0;
				cdb.byte[11] = 0;
				cdb.byte[12] = cdb.byte[7];
				cdb.byte[13] = cdb.byte[8];
			}

			// Copy and zero-extend the LBA.
			cdb.byte[6] = cdb.byte[2];
			cdb.byte[7] = cdb.byte[3];
			cdb.byte[8] = cdb.byte[4];
			cdb.byte[9] = cdb.byte[5];
			cdb.byte[5] = cdb.byte[4] = cdb.byte[3] = cdb.byte[2] = 0;
			// CD-ROM blocks are 4x the size of HDD blocks (512B vs. 2KB)
			// Since the SATA HDD can't transfer more than 64k sectors in one
			// command, don't allow reads for more than (65536 / 4) blocks.
			// fix everything to xfer up to 64k CD-ROM blocks someday.
			//
			sec_cnt = (cdb.byte[10] << 24) + (cdb.byte[11] << 16) + (cdb.byte[12] << 8) + cdb.byte[13];
			if (sec_cnt > 0x3fff)  goto invalid_CDB_field;

			// The virtual CD is always much less than 32 GiB, so reject LBAs
			// larger than 2**24 outright, thereby avoiding overflows later.
			// LBA bits 32..63 are always 0 because this was converted
			// from a 10- or 12-byte read command.
			lba_num = (cdb.byte[6] << 24) + (cdb.byte[7] << 16) + (cdb.byte[8] << 8) + cdb.byte[9];
			if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
			{
				if (sec_cnt > 0x3fff)  goto invalid_CDB_field;

				// check the Hi < Di & Ho< > Di case in BOT mode
				if (curMscMode == MODE_BOT)
				{
					u32 tmp32 = pCtxt->CTXT_XFRLENGTH >> 11;
					if (((pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR) == 0) || (tmp32 < sec_cnt))
					{
						pCtxt->CTXT_Status = CTXT_STATUS_PHASE;			// CSW status
						usb_device_no_data(pCtxt);
						return;	
					}
				}

				// The virtual CD is always much less than 32 GiB, so reject LBAs
				// larger than 2**24 outright, thereby avoiding overflows later.
				// LBA bits 32..63 are always 0 because this was converted
				// from a 10- or 12-byte read command.
				if ((lba_num >= (INI_VCD_SIZE >> 2)) || ((lba_num + sec_cnt) > (INI_VCD_SIZE >> 2)))// avoid the LBA larger than the true VCD size
				{
	_Err_LBA_OUT_OF_RANGE:
					hdd_err_2_sense_data(pCtxt, ERROR_LBA_OUT_RANGE);
					break;
				}
				
				// Nothing to do if Transfer Length is zero.
				if (sec_cnt == 0)
				{
					nextDirection = DIRECTION_SEND_STATUS;
					break;
				}
				
				if (scsi_chk_hdd_pwr(pCtxt)) return;
			}

			// Scale the LBA and transfer length to HDD blocks.
			sec_cnt <<= 2;
			lba_num <<= 2;
			WRITE_U32_BE(&cdb.byte[10], sec_cnt);
	//		WRITE_U32_BE(&cdb.byte[6], lba_num);	
			u64 tmp64 = vital_data[0].lun_capacity + lba_num;
			if ((ram_rp[0].vcd_size == 0) || (raid1_use_slot == 1))  // VCD's sync issue
			{
				tmp64 = vital_data[1].lun_capacity + lba_num;
			}
			
			WRITE_U64_BE(&cdb.byte[2], tmp64);
			pCtxt->CTXT_FLAG = 0;
			pCtxt->CTXT_PHASE_CASE = 0x40; // case 6
			pCtxt->CTXT_Status = CTXT_STATUS_PENDING;
			pCtxt->CTXT_ccm_cmdinten = 0; 

			if ((arrayMode == JBOD) && (sata0Obj.sobj_device_state == SATA_NO_DEV))
			{
				pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S1R;	
				pSataObj = &sata1Obj;
			}
			else
			{
				if ((arrayMode == RAID1_MIRROR) && (raid1_use_slot == 1))
				{
					pSataObj = &sata1Obj;
					pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S1R;	
				}
				else
				{
					pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S0R;	
					pSataObj = &sata0Obj;
				}
			}
			
	#ifdef UAS
			if (curMscMode == MODE_UAS)
			{
				pCtxt->CTXT_XFRLENGTH = sec_cnt << 9;
				DBG("ctxt tx len %lx\n", pCtxt->CTXT_XFRLENGTH);
				//pCtxt->CTXT_DbufIndex = SEG_NULL;
				pCtxt->CTXT_CONTROL |= CTXT_CTRL_DIR;			// Data-in
				pCtxt->CTXT_Prot = PROT_FPDMAIN;
				pCtxt->CTXT_FIS[FIS_COMMAND] = ATA6_RD_FPDMA_QUEUED;
				ata_load_fis_from_rw16Cdb(pCtxt, FPDMA_TYPE);
				if (sata_exec_ncq_ctxt(pSataObj, pCtxt) == CTXT_STATUS_ERROR)
				{	// sense Sense IU if error return 
					if (pSataObj->sobj_sata_ch_seq)
					{
						uas_ci_paused |= UAS_PAUSE_SATA1_NCQ_NO_ENOUGH_TAGS;
					}
					else
					{
						uas_ci_paused |= UAS_PAUSE_SATA0_NCQ_NO_ENOUGH_TAGS;
					}
					uas_ci_paused_ctxt = pCtxt;
					pCtxt->CTXT_LunStatus = LUN_STATUS_BUSY;
					uas_device_no_data(pCtxt);
				}
			}
			else
	#endif
			{
				pCtxt->CTXT_Prot = PROT_DMAIN;
				pCtxt->CTXT_FIS[FIS_COMMAND] = ATA6_READ_DMA_EXT;		
				ata_load_fis_from_rw16Cdb(pCtxt, DMA_TYPE);

				if (sata_exec_ctxt(pSataObj, pCtxt) == CTXT_STATUS_ERROR)
				{
					usb_device_no_data(pCtxt);
				}								
			}
			return;		
		}
			
		case SCSI_READ_CAPACITY:
			// either of reladr or PMI bit is set to 1, the device shall return check contion status, v1.023
			if ((cdb.byte[1] & RELADR_BIT) || (cdb.byte[8] & 0x01)) 
				goto invalid_CDB_field;
			if (!cd_media_present)  goto no_media;
			
			if (ram_rp[0].vcd_size)
			{
				WRITE_U32_BE(mc_buffer, ((ram_rp[0].vcd_size >> 2) - 1));// 30MB
			}
			else if (ram_rp[1].vcd_size)
			{
				WRITE_U32_BE(mc_buffer, ((ram_rp[1].vcd_size >> 2) - 1));// 30MB
			}
			else
			{
				goto no_media;
			}

			// Bytes 4..7 is the Block Length (bytes), which is always 2KB.
			WRITE_U32_BE((u8 *)(mc_buffer+4), 2048);

			byteCnt = 8;
			alloc_len = 8;
			nextDirection = DIRECTION_SEND_DATA;
			break;

		case SCSI_READ_TOC:
			
			if (!cd_media_present)  goto no_media;
			read_toc_cmd(pCtxt);
			break;


		case SCSI_TEST_UNIT_READY:
			if (!cd_media_present)  goto no_media;

			// Else return GOOD status.
			nextDirection = DIRECTION_SEND_STATUS;	
			break;

		case SCSI_MECHANISM_STATUS:
			// The mechanism status data happens to be all zero!
			xmemset(&mc_buffer[0], 0, 16);

			byteCnt = 8;
			alloc_len = (cdb.byte[8] << 8) + cdb.byte[9];
			nextDirection = DIRECTION_SEND_DATA;
			break;

		case SCSI_PREVENT_MEDIUM_REMOVAL:
			// Isolate and check the Prevent field.
			switch (CmdBlk(4) & 0x03)
			{
				case PREVENT_ALLOW__UNLOCK_ALL:
					cd_medium_lock = 0;
					break;

				case PREVENT_ALLOW__DRIVE_LOCK:
					// Do not allow the disc to be removed from the drive.
					cd_medium_lock = 1;
					break;

				default:
					goto invalid_CDB_field;
			}

			nextDirection = DIRECTION_SEND_STATUS;
	       	break;

		case SCSI_START_STOP_UNIT:
#if 1
			// Power Condition changes are not supported; the field must be 0.
			if (cdb.start_stop_unit.pc_action & SSU_POWER_COND_MASK)
			{
				goto invalid_CDB_field;
			}

			// Check the LoEj (Load/Eject) and Start bits.
			switch (cdb.start_stop_unit.pc_action & SSU_START_LOEJ_MASK)
			{
			case SSU_EJECT:			// Eject the disc!
				// The PREVENT/ALLOW MEDIUM REMOVAL cmd can lock the disc,
				// as can our Operations mode page.
				if ( cd_medium_lock ||  
				    !(nvm_unit_parms.mode_save.cd_valid_cd_eject & OPS_ENABLE_CD_EJECT) )
				{
					hdd_err_2_sense_data(pCtxt, ERROR_MEDIUM_LOCKED);
					break;
				}

				vcd_set_media_presence(0);
				break;
			}
			nextDirection = DIRECTION_SEND_STATUS;
#else
			usb_device_no_data();
#endif
			break;

		case SCSI_SYNCHRONIZE_CACHE:
			if (cdb.byte[1] & RELADR_BIT) // v1.023
				goto invalid_CDB_field;
			if (!cd_media_present)  goto no_media;
			
			lba_num = (cdb.byte[2] << 24) + (cdb.byte[3] << 16) + (cdb.byte[4] << 8) + cdb.byte[5];
			sec_cnt = (cdb.byte[7] << 8) + cdb.byte[8];
			if ((lba_num >= (INI_VCD_SIZE >> 2)) || ((lba_num + sec_cnt) > (INI_VCD_SIZE >> 2)))
				goto _Err_LBA_OUT_OF_RANGE;
			
			scsi_sync_cache_cmd(pCtxt);
			break;
			
		// Diagnostics.
		// The CD-ROM doesn't have all diagnostic features; e.g., no LOG SENSE.
		//
		case SCSI_RCV_DIAG_RESULTS:

			diag_receive_diag_results_cmd(pCtxt);
			break;

		// Mode parameter pages.
		case SCSI_MODE_SENSE6:
		case SCSI_MODE_SENSE10:
			scsi_mode_sense_cmd(pCtxt);
			break;

		case SCSI_MODE_SELECT6:
		case SCSI_MODE_SELECT10:
			scsi_mode_select_cmd(pCtxt);
			break;

#ifndef INITIO_STANDARD_FW
		// WD-specific commands for reading & writing the virtual CD image.
		//
		case SCSI_WDC_READ_VIRTUAL_CD:
		case SCSI_WDC_WRITE_VIRTUAL_CD:
			// the maximum of  TRANSFER LENGTH is 0xFFFF, so not check the cdb.byte[7] & cdb.byte[8] (TRANSFER LENGTH).  --->v0.07_14.32_0328_A 
			//"no space is allocated for the virtual CD image case " not need implement.  --->v0.07_14.32_0328_A 

			// The WD-specific READ/WRITE VIRTUAL CD commands have to be
			// usable even after the virtual CD is ejected, so ignore the
			// media valid flag.

			vcd_read_write_virtual_cd_cmd(pCtxt);
			break;

		case SCSI_WDC_READ_VIRTUAL_CD_CAPACITY:		
			vcd_read_virtual_cd_capacity_cmd(pCtxt);
			break;
#endif

		default:
			hdd_err_2_sense_data(pCtxt, ERROR_ILL_OP);
			break;
	}

jmp_VCD_CMD_NEXT_DIR:
	
	if (nextDirection == DIRECTION_SEND_DATA)
	{
		byteCnt = Min(byteCnt, alloc_len); // check the allocation length
		usb_device_data_in(pCtxt, byteCnt);
	}
	else if (nextDirection == DIRECTION_SEND_STATUS)
	{
		usb_device_no_data(pCtxt);
	}
	return;


invalid_CDB_field:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
	usb_device_no_data(pCtxt);
	return;

no_media:
	hdd_err_2_sense_data(pCtxt, ERROR_UA_NO_MEDIA);
	usb_device_no_data(pCtxt);
	return;
}
#endif


/************************************************************************\
 vcd_read_write_virtual_cd_cmd() - READ and WRITE VIRTUAL CD cmd handler.

 These are the WD-specific commands for accessing the virtual CD area.

 Logical Units (VCD, SES)
 
 *************************************************************************/
void vcd_read_write_virtual_cd_cmd(PCDB_CTXT pCtxt)
{
	// --->v0.07_14.32_0328_A 
	if(scsi_wdc_param_check(TYPE_DESTSLOT, pCtxt) ) 
	{
		return;
	}
	u8 dest_slot = cdb.byte[6] & MASK_CP_DESTSLOT;

#ifdef INITIO_STANDARD_FW
	// initio just support 8G vcd, so cdb 3, 4, 5 for lba is enough.
	cdb.byte[2] = 0;
#endif
	// Bounds check the LBA.
	u32 tmp32 = (cdb.byte[2] << 24) + (cdb.byte[3] << 16) + (cdb.byte[4] << 8) + cdb.byte[5];
	u16 sec_cnt = (cdb.byte[7] << 8) + cdb.byte[8];
	
	if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
	{
#ifdef INITIO_STANDARD_FW
		if (cdrom_lun == UNSUPPORTED_LUN)
		{
			hdd_err_2_sense_data(pCtxt, ERROR_UA_NO_MEDIA);
			return;
		}
#endif

		// Writes are not allowed when Locked (or Unlock Exceeded).
#if 1
#ifdef INITIO_STANDARD_FW
		if ( (cdb.byte[0]== SCSI_WRITE_BUFFER10)  &&  IS_LOCKED(dest_slot) )
#else
		if ( (cdb.byte[0]== SCSI_WDC_WRITE_VIRTUAL_CD)  &&  IS_LOCKED(dest_slot) )
#endif			
		{
			hdd_err_2_sense_data(pCtxt, ERROR_WRITE_PROTECT);
			return;
		}
#endif
#if MAX_RW_VCD_BLOCK < 0xffff
		if (cdb.rw10.xfer16 > MAX_RW_VCD_BLOCK)
		{
			hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
			return;
		}
#endif

		if ( (tmp32 >= INI_VCD_SIZE) ||
			 ((tmp32 + sec_cnt) > INI_VCD_SIZE) )
		{
			hdd_err_2_sense_data(pCtxt, ERROR_LBA_OUT_RANGE);
			return;
		}
		// Nothing to do if Transfer Length is zero.
		if (sec_cnt == 0)
		{
			nextDirection = DIRECTION_SEND_STATUS;
			return;
		}

		if (scsi_chk_hdd_pwr(pCtxt))	return;
	}
	pSataObj = dest_slot ? &sata1Obj : &sata0Obj;
	// Convert this to the 16-byte variant and transfer the data.
	// Make room in the CDB for 40-bit LBA math by converting
	// this command into a READ(16).
	cdb.byte[10] = 0;
	cdb.byte[11] = 0;
	cdb.byte[12] = cdb.byte[7];
	cdb.byte[13] = cdb.byte[8];

	//u64 tmp64 = IniData.vcd_base + tmp32;
	u64 tmp64 = vital_data[dest_slot].lun_capacity + tmp32;
	DBG("lba %bx %lx\n", (u8)(tmp64>>32), (u32)tmp64);	
	WRITE_U64_BE(&cdb.byte[2], tmp64);
	// ...and kick off the command.
	pCtxt->CTXT_FLAG = 0;
	pCtxt->CTXT_ccm_cmdinten = 0; 
	pCtxt->CTXT_Status = CTXT_STATUS_PENDING;
#ifdef UAS
	if (curMscMode == MODE_UAS)
	{
		pCtxt->CTXT_XFRLENGTH = sec_cnt << 9;
#ifdef INITIO_STANDARD_FW
		if (CmdBlk(0)== SCSI_WRITE_BUFFER10)
#else
		if (CmdBlk(0)== SCSI_WDC_WRITE_VIRTUAL_CD)
#endif			
		{
			//pCtxt->CTXT_CONTROL &= (~CTXT_CTRL_DIR);			// Data-Out
			if (dest_slot == 0)
				pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S0W;
			else
				pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S1W;
			pCtxt->CTXT_Prot = PROT_FPDMAOUT;
			pCtxt->CTXT_PHASE_CASE = BIT_12; // case 12
			pCtxt->CTXT_FIS[FIS_COMMAND] = ATA6_WR_FPDMA_QUEUED;
		}
		else
		{
			if (dest_slot == 0)
				pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S0R;
			else
				pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S1R;
			pCtxt->CTXT_CONTROL |= CTXT_CTRL_DIR;			// Data-in
			pCtxt->CTXT_Prot = PROT_FPDMAIN;
			pCtxt->CTXT_PHASE_CASE = BIT_6; // case 6
			pCtxt->CTXT_FIS[FIS_COMMAND] = ATA6_RD_FPDMA_QUEUED;
		}

		ata_load_fis_from_rw16Cdb(pCtxt, FPDMA_TYPE);

		if (sata_exec_ncq_ctxt(pSataObj, pCtxt) == CTXT_STATUS_ERROR)
		{	// sense Sense IU if error return 
			if (pSataObj->sobj_sata_ch_seq)
			{
				uas_ci_paused |= UAS_PAUSE_SATA1_NCQ_NO_ENOUGH_TAGS;
			}
			else
			{
				uas_ci_paused |= UAS_PAUSE_SATA0_NCQ_NO_ENOUGH_TAGS;
			}
			uas_ci_paused_ctxt = pCtxt;
			pCtxt->CTXT_LunStatus = LUN_STATUS_BUSY;
			uas_device_no_data(pCtxt);
		}
	}
	else
#endif
	{
		ata_load_fis_from_rw16Cdb(pCtxt, DMA_TYPE);

#ifdef INITIO_STANDARD_FW
		if (CmdBlk(0)== SCSI_WRITE_BUFFER10)
#else
		if (CmdBlk(0)== SCSI_WDC_WRITE_VIRTUAL_CD)
#endif			
		{
			pCtxt->CTXT_Prot = PROT_DMAOUT;
			pCtxt->CTXT_PHASE_CASE = BIT_12; // case 12
			if (dest_slot == 0)
				pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S0W;
			else
				pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S1W;
	//		pCtxt->CTXT_DBUF_IN_OUT = (RX_DBUF_SATA0_R_PORT << 4) | RX_DBUF_USB_W_PORT; 
			pCtxt->CTXT_FIS[FIS_COMMAND] = ATA6_WRITE_DMA_EXT;
		}
		else
		{
			// case 5 (Hi > Di) or case  6(Hi = Di)
			pCtxt->CTXT_Prot = PROT_DMAIN;
			pCtxt->CTXT_PHASE_CASE = BIT_6; // case 6
			if (dest_slot == 0)
				pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S0R;
			else
				pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S1R;
	//		pCtxt->CTXT_DBUF_IN_OUT = (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_SATA0_W_PORT; 
			pCtxt->CTXT_FIS[FIS_COMMAND] = ATA6_READ_DMA_EXT;
		}	
		sata_exec_ctxt(pSataObj, pCtxt);
	}
	return;
}

/************************************************************************\
 vcd_read_virtual_cd_capacity_cmd() - READ VIRTUAL CD CAPACITY cmd handler.

 This WD-specific command returns the size of the virtual CD.

 */
void vcd_read_virtual_cd_capacity_cmd(PCDB_CTXT pCtxt)
{

	// "no space is allocated for the virtual CD image case " not need implement.
#if 1
	//if (!IniData.vcd_size)
	if (!ram_rp[logicalUnit].vcd_size)
	{
		hdd_err_2_sense_data(pCtxt, ERROR_UA_NO_MEDIA);
		return;
	}
#endif

	// Bytes 0..3 is the Last CD Area Block Address
//	*((u32 *) &mc_buffer[0]) = INI_VCD_SIZE;
	WRITE_U32_BE(&mc_buffer[0], INI_VCD_SIZE);

	// Bytes 4..7 is the Block Length (bytes)
	mc_buffer[4] = 0;
	mc_buffer[5] = 0;
	mc_buffer[6] = (512 >> 8);		// 512 bytes
	mc_buffer[7] = (512 & 0xff);

	// Bytes 8..9 are reserved
	mc_buffer[8] = 0;
	mc_buffer[9] = 0;

	// Bytes 10..11 is Max Transfer Length (blocks)
	mc_buffer[10] = ( u8 )(MAX_RW_VCD_BLOCK >> 8);
	mc_buffer[11] = ( u8 )MAX_RW_VCD_BLOCK;
	byteCnt = 12;		
	alloc_len = 12;
	nextDirection = DIRECTION_SEND_DATA;
	return;
}

// EOF
