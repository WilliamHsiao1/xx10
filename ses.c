/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2008-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/
#define SES_C
#include "general.h"
/************************************************************************\
 ses_start_command() - Command dispatcher for the SES logical unit.

 This is called when a new command is received for this logical unit.

 */
 #ifdef WDC_SES
void ses_start_command(PCDB_CTXT pCtxt)
{
#ifdef UAS
	uas_ci_paused |= UAS_PAUSE_WAIT_FOR_USB_RESOURCE;
	uas_ci_paused_ctxt = pCtxt;
#endif
	xmemcpy32((u32 *)(&CmdBlk(0)), cdb.AsUlong, 4);
	nextDirection = DIRECTION_NONE;
	pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
	pCtxt->CTXT_usb_state = CTXT_USB_STATE_CTXT_SETUP_DONE;

	switch (CmdBlk(0))
	{
	case SCSI_INQUIRY:
		scsi_inquiry_cmd(pCtxt, DEVICE_TYPE_SES);
		break;

	case SCSI_REPORT_LUNS:
		scsi_report_luns_cmd(pCtxt);
		break;

	case SCSI_REQUEST_SENSE:
		scsi_request_sense_cmd(pSataObj, pCtxt);
		break;

	case SCSI_TEST_UNIT_READY:
		// Just return GOOD status.
		nextDirection = DIRECTION_SEND_STATUS;
		break;

#if 0 // remove in spec v0.99
	// The START/STOP UNIT, SYNCHRONIZE CACHE and FORMAT UNIT commands
	// are not defined by the SES command specifications, but are
	// supported here for the benefit of WD SmartWare and other apps.
	//
	case SCSI_START_STOP_UNIT:			// Not defined by SES
		scsi_start_stop_cmd(pCtxt);
		break;

	case SCSI_SYNCHRONIZE_CACHE:		// Not defined by SES
		if (hdd_power)
		{
			pCtxt->CTXT_FLAG = CTXT_FLAG_U2B;
			ata_ExecUSBNoDataCmd(pSataObj, pCtxt, ATA6_FLUSH_CACHE_EXT);
		}
		else
			nextDirection = DIRECTION_SEND_STATUS;
		break;
#endif

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


	// Mode parameters.
	//
	case SCSI_MODE_SELECT6:
	case SCSI_MODE_SELECT10:
		scsi_mode_select_cmd(pCtxt);
		break;

	case SCSI_MODE_SENSE6:
	case SCSI_MODE_SENSE10:
		scsi_mode_sense_cmd(pCtxt);
		break;     
#if 0 // remove in spec v0.99
	case SCSI_FORMAT_UNIT:				// Not defined by SES

	case SCSI_FORMAT_DISK:
		scsi_format_unit_cmd(pCtxt);
		break;
#endif

	// Reading/writing the handy store.
#ifdef WDC_HANDY
	case SCSI_WDC_READ_HANDY_CAPACITY:
		handy_read_handy_capacity_cmd(pCtxt);// --->v0.07_14.32_0404_A
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

	// Data-encryption (security) commands.
	//
	case SCSI_WDC_ENCRYPTION_STATUS:
		aes_encryption_status_cmd(pCtxt);
		break;

	case SCSI_WDC_SECURITY_CMD:
		aes_security_cmd(pCtxt);
		break;
		
	// WD-specific commands for updating the virtual CD image.
	//
	case SCSI_WDC_READ_VIRTUAL_CD:
	case SCSI_WDC_WRITE_VIRTUAL_CD:
		vcd_read_write_virtual_cd_cmd(pCtxt);	
		break;

	case SCSI_WDC_READ_VIRTUAL_CD_CAPACITY:
		vcd_read_virtual_cd_capacity_cmd(pCtxt);	
		break;

	default:
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_OP);
		break;
	}
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
}
#endif
// EOF
