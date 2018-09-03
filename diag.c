/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/
#define DIAG_C
#include "general.h"


static u8 diag_last_page_code;
static u8 temperature_state = TC_NOMINAL;

static u8 exec_dataless_smart_cmd(PCDB_CTXT pCtxt, u8 scmd, u8 param, u8 sata_ch_flag);
static u8  get_smart_data(u8 sata_ch_flag);
#if WDC_HDD_TEMPERATURE_POLL_TICKS
static u8 get_temperature(void);
#endif	// WDC_HDD_TEMPERATURE_POLL_TICKS
static void init_self_test_log(u8 testCode, u8 sata_ch_flag);
static void set_self_test_result(PCDB_CTXT pCtxt, u8 result, u8 error, u8 sata_ch_flag);
static void update_self_test_log(PCDB_CTXT pCtxt);
static void run_default_self_test(PCDB_CTXT pCtxt);
static void start_background_self_test(PCDB_CTXT pCtxt, u8 testType);
static void abort_background_self_test(PCDB_CTXT pCtxt);

//
// Global variables
//
//u8 diag_drive_selected = 1;


/************************************************************************\
 exec_dataless_smart_cmd() - Executes a non-data SMART command.

 Arguments:
	scmd		the SMART command code
	param		parameter for the SMART command; not always used

 Returns:
	Nothing.
 */
u8 exec_dataless_smart_cmd(PCDB_CTXT pCtxt, u8 scmd, u8 param, u8 sata_ch_flag)
{// suppose that this command is runing under single thread and use the sata_ccm[0]
	// Turn on and re-init the HDD if it is off.
	if (hdd_power != HDDs_BOTH_POWER_ON)
	{
		if (ata_startup_hdd(1, sata_ch_flag)) // use the sata reset function 
		{
			hdd_err_2_sense_data(pCtxt, ERROR_UA_BECOMING_READY);
			return 1;
		}
	}
	pSataObj = (sata_ch_flag & SATA_CH1) ? &sata1Obj : &sata0Obj;
	if (sata_ch_flag & SATA_TWO_CHs)
		ata_ExecNoDataCmd_TwoHDDs((scmd<<8)|ATA6_SMART, param | (SMART_SIGNATURE_LOW <<8) | (SMART_SIGNATURE_HIGH<<16), WDC_SATA_TIME_OUT);
	else
		ata_ExecNoDataCmd(pSataObj, (scmd<<8)|ATA6_SMART, param | (SMART_SIGNATURE_LOW <<8) | (SMART_SIGNATURE_HIGH<<16), WDC_SATA_TIME_OUT);
	return 0;
}

/************************************************************************\
 get_smart_data() - Retrieves SMART data from the HDD.

 The SMART data is put into the  mc_buffer.

 Returns:
	0 if the SMART data is in the buffer;
	1 if an ATA error occured or the SMART data's checksum is wrong.
 */
u8 get_smart_data(u8 sata_ch_flag)
{
#ifndef LA_DUMP	
MSG("smart rd\n");
#endif

	u8 rc = 0;
	pSataObj = (sata_ch_flag & SATA_CH1) ? &sata1Obj : & sata0Obj;
	if (hdd_power != HDDs_BOTH_POWER_ON)
	{
		if (ata_startup_hdd(1, SATA_TWO_CHs))
		{
			return 1;
		}
	}

	CDB_CTXT ctxt;
	
	ctxt.CTXT_FLAG = CTXT_FLAG_B2S | CTXT_FLAG_SYNC;
	ctxt.CTXT_Prot = PROT_PIOIN;
	ctxt.CTXT_TimeOutTick = WDC_SATA_TIME_OUT;
	ctxt.CTXT_ccm_cmdinten = 0;
	ctxt.CTXT_XFRLENGTH = 512;
	if (sata_ch_flag & SATA_CH1)
		ctxt.CTXT_DbufIndex = DBUF_SEG_B2S1R;
	else
		ctxt.CTXT_DbufIndex = DBUF_SEG_B2S0R;
//	ctxt.CTXT_DBUF_IN_OUT  = (TX_DBUF_CPU_R_PORT << 4) |  TX_DBUF_SATA0_W_PORT;	
	
	ctxt.CTXT_FIS[FIS_TYPE] = 0x27;						// FIS 27
	ctxt.CTXT_FIS[FIS_C]	= 0x80;						// command bit valid
	ctxt.CTXT_FIS[FIS_COMMAND]	= ATA6_SMART;						// command bit valid

	// Execute the SMART READ DATA command.
	ctxt.CTXT_FIS[FIS_FEATURE] = SMART_READ_DATA;						// FIS 27
	
	*((u32 *) &(ctxt.CTXT_FIS[FIS_LBA_LOW]))	= (SMART_SIGNATURE_HIGH << 16) | (SMART_SIGNATURE_LOW << 8) | 0x00 ;
	*((u32 *) &(ctxt.CTXT_FIS[FIS_LBA_LOW_EXT]))	= 0;
	*((u32 *) &(ctxt.CTXT_FIS[FIS_SEC_CNT]))	= 0;
	
	sata_exec_ctxt(pSataObj, &ctxt);

	if (pSataObj->pSata_Ch_Reg->sata_Status & ATA_STATUS_CHECK)
	{
		// Do not set sense data here. This could be called by the
		// temperature poller, which has to ensure in-progress
		// host-issued commands are not affected by temperature-
		// related errors.

		rc = 1;  //fail
	}
	else
	{
//		read_data_from_cpu_port(512);
//		Tx_Dbuf->dbuf_Seg[ctxt.CTXT_DbufIndex].dbuf_Seg_Control = SEG_DONE;
		// The data structure checksum should be zero.
		if (ata_checksum8(mc_buffer) != 0)
		{
			rc = 1;
		}
	}
	
	pSataObj->sobj_State = SATA_READY;
	
//	Tx_Dbuf->dbuf_Seg[ctxt.CTXT_DbufIndex].dbuf_Seg_Control	=  SEG_RESET;
//	Tx_Dbuf->dbuf_Seg[ctxt.CTXT_DbufIndex].dbuf_Seg_INOUT	= (TX_DBUF_NULL << 8) | (TX_DBUF_NULL << 4) | TX_DBUF_NULL;

    	return rc;			// Got the data and its checksum is OK.
}

#if WDC_HDD_TEMPERATURE_POLL_TICKS

/************************************************************************\
 get_temperature() - Parses SMART data and returns the HDD's temperature.

 Prerequisites:
	The SMART data is in the  mc_buffer; call  get_smart_data()  first.

 Returns:
	The HDD temperature.
 */
u8 get_temperature(void)
{
u8 *attrib;
u8 idx, temperature;

	temperature = ASSUMED_TEMPERATURE_IF_NA;

#if 0//def DEBUG
printf ("temp data ");
for(i = 0; i < 100; i++)
{
printf (" %x", mc_buffer[i+100]);
if (!(i&0x0F))
	printf("\n");
}
#endif

	// Look for temperature info in the SMART data.

	// The vendor-specific portion (starting at offset 2) should contain
	// an array of 30 SMART attributes. Each attribute is 12 bytes.

	attrib = mc_buffer + 2;
	for(idx=0;   idx < 30;   idx++, attrib += 12)
	{
		if (attrib[0] == 0xC2 /*temperature attribute ID*/ )
		{
			// The 6-byte raw value is the degrees Celsius.
			temperature = attrib[5];

			if ( attrib[6] | attrib[7] |
			     attrib[8] | attrib[9] | attrib[10] )
			{
				//temperature = 255;		// Limit to 255 degrees. the other HDD(like Seagate and Hitachi) will set value to the bytes
			}

			break;
		}
	}

#if WDC_TEST_TEMPERATURE_POLL
	// Fake a HDD temperature reading- return successively higher temperatures.
  {
	static u8 test_temperature = 50;

	temperature = test_temperature;
	test_temperature += 5;
  }
#endif	// WDC_TEST_TEMPERATURE_POLL	
	return temperature;
}

/************************************************************************\
 diag_check_hdd_temperature() - Check the HDD's temperature.

 */
void diag_check_hdd_temperature(void)
{
	u8 temp[2] = {0, 0};
	u8 sata_ch_flag = 0;
	// This is called periodically to take the HDD's temperature.
	// The HDD is turned off if it gets too hot (overheats)
	// If we had a fan, its speed would also be set here.
	//
	// Since this may be called during a host-issued command, the
	// SCSI status and sense data must not be changed for any reason.

	// Bug alert: sometimes the SMART data is not correct - maybe it
	// ends up in the wrong place in the mc_buffer?  See alert in  ini.c
	// If at first it doesn't succeed, retry.
	for (u8 i = 0; i < 2; i++)
	{
		if (((i == 0) && (disable_upkeep & UPKEEP_NO_TEMP_POLL_MASK_S0))
			|| ((i) && (disable_upkeep & UPKEEP_NO_TEMP_POLL_MASK_S1)))
			continue;
		
		if (i == 0)
		{
			if (sata0Obj.sobj_device_state != SATA_DEV_READY)
			{
				continue;
			}
			else
			{
				sata_ch_flag = SATA_CH0;
			}
		}
		else
		{
			if (sata1Obj.sobj_device_state != SATA_DEV_READY)
			{
				continue;
			}
			else
			{
				sata_ch_flag = SATA_CH1;
			}
		}
		
		//dbg_next_cmd = 1;

		//  If temperature readings on all available HDDs are ignored because 
		// of errors then the unit wide current temperature shall be set to a value that causes thermal shutdown (e.g. 0xFF).
		if (get_smart_data(sata_ch_flag)) 
		{
			MSG("ER %bx\n", i);
			if (i == 0)
				continue;
			else
			{
				unit_wide_cur_temperature = 0xFF;
				return;
			}
			
		}

		// Parse the SMART data if we got it before exhausting retries.
		temp[i] = get_temperature();
		if (temp[i] == 0xFF)
		{
			MSG("Tmp %bx: %bx\n", i, temp[i]);
			if (i == 0)
				continue;
			else
			{
				unit_wide_cur_temperature = 0xFF;
				return;
			}
		}
		else
		{
			unit_wide_cur_temperature = Max(temp[0], temp[1]);
		}

		//update state
	}

	DBG("current temperature_state: %bx\n", temperature_state);
}

#endif	// WDC_HDD_TEMPERATURE_POLL_TICKS


/************************************************************************\
 init_self_test_log() - Initializes a self-test Results log.

 Arguments:
	testCode	self-test code; i.e., which self-test is being done.

 Returns:
	Nothing.
 */
void init_self_test_log(u8 testCode, u8 sata_ch_flag)
{
	u8 tmp_ch = 0;
	//check & init sata channel 0 self-test Results log
	if (sata_ch_flag & SATA_CH0)
	{
_INIT_SLOG:
		xmemset((u8 *)&diag_test_log[tmp_ch], 0, sizeof(DIAG_TEST_LOG));

		// Set fields that need non-zero values.
		diag_test_log[tmp_ch].para_code[1] = 1;

		// Old bug alert: older versions forgot to set the LBIN and LP bits.
		diag_test_log[tmp_ch].ctrl_bits = 0x03;		// Set LBIN and LP bits to 1.

		diag_test_log[tmp_ch].para_legth = 16;

		// The self-test is in progress; the status must be updated later on.
		diag_test_log[tmp_ch].self_tests = (testCode << 5) | SELF_TEST_IN_PROGRESS;

		xmemset((u8 *)(&diag_test_log[tmp_ch].address), 0xff, 8);

		// Sense Key and ASC have been cleared.
		diag_test_log[tmp_ch].ascq = 0x16;	// OPERATION IN PROGRESS
	}
	tmp_ch++;
	//check & init sata channel 1 self-test Results log
	if((tmp_ch == 1) && (sata_ch_flag & SATA_CH1))
		goto _INIT_SLOG;
}

/************************************************************************\
 set_self_test_result() - Set the self-test's results and sense data.

 Arguments:
	result		self-test result code from the enum SelfTestResultCode.
	error		the SCSI error code to put into the log's sense data.

 Returns:
	Nothing.
 */
void set_self_test_result(PCDB_CTXT pCtxt, u8 result, u8 error, u8 sata_ch_flag)
{
	u8 tmp_channel  = (sata_ch_flag & SATA_CH1) ? 1:0;;

	diag_test_log[tmp_channel].self_tests = (diag_test_log[tmp_channel].self_tests & 0xF0) | result;
	
	hdd_err_2_sense_data(pCtxt, error);
	
	diag_test_log[tmp_channel].sense_key = bot_sense_data[1];
	diag_test_log[tmp_channel].asc =   bot_sense_data[2];
	if((tmp_channel) && (error == ERROR_DIAG_FAIL_COMPONENT))
		diag_test_log[tmp_channel].ascq =  bot_sense_data[3] + 1;//0x82 indicates the second HDD failed
	else
		diag_test_log[tmp_channel].ascq =  bot_sense_data[3];//if error,then 0x81 indicates the first HDD failed
}

/************************************************************************\
 update_self_test_log() - Update the self-test Results log with progress
 and status info from the HDD.

 */
void update_self_test_log(PCDB_CTXT pCtxt)
{
	u8 status;
	u8 sata_ch_flag = 0;
	if (pCtxt->CTXT_LUN == HDD0_LUN)
	{
		if (sata0Obj.sobj_device_state == SATA_DEV_READY)
			sata_ch_flag = SATA_CH0;
		else if (sata1Obj.sobj_device_state == SATA_DEV_READY)
			sata_ch_flag = SATA_CH1;
	}
	else 
		sata_ch_flag = SATA_CH1;

	#define  UPKEEP_RUNNING_SELF_TEST ((sata_ch_flag & SATA_CH1) ? UPKEEP_RUNNING_SELF_TEST_S1 : UPKEEP_RUNNING_SELF_TEST_S0)

	if (get_smart_data(sata_ch_flag))
	{
		// consider returning error instead of marking the test failed
		status = SELF_TEST_UNKNOWN_ERROR;
		goto set_failure_status;
	}

	// Get the self-test status from the Self-test Execution Status byte.
	status = mc_buffer[363] >> 4;
	
	if (status == 0 /*self-test completed w/o error, or no test run*/ )
	{
		set_self_test_result(pCtxt, SELF_TEST_NO_ERROR, ERROR_NONE, sata_ch_flag);

		disable_upkeep &= ~UPKEEP_RUNNING_SELF_TEST;
	}
	else if (status == 15 /*self-test in progress*/ )
	{
		// The results log is marked "test in progress" when the self-test
		// was started, but it doesn't hurt to reinforce that here.
		diag_test_log[sata_ch_flag -1].self_tests &= 0xF0;
		diag_test_log[sata_ch_flag -1].self_tests |= SELF_TEST_IN_PROGRESS;
		if (sata_ch_flag & SATA_CH0)
		{
			disable_upkeep |= UPKEEP_RUNNING_SELF_TEST_S0;
		}
		if (sata_ch_flag & SATA_CH1)
		{
			disable_upkeep |= UPKEEP_RUNNING_SELF_TEST_S1;
		}
   	}
	else		// status is 1..14
	{
		// The ATA self-test status codes map fairly well to SCSI
		// self-test result codes, except ATA defines a few values that
		// are reserved in SCSI land.
		if (status > 7)  status = 7;	// Limit to valid SCSI result codes.

		goto set_failure_status;
	}

	return;

set_failure_status:
	diag_test_log[sata_ch_flag -1].self_tests = (diag_test_log[sata_ch_flag -1].self_tests & 0xF0) | status;

	diag_test_log[sata_ch_flag -1].sense_key = SCSI_SENSE_HARDWARE_ERROR;
	diag_test_log[sata_ch_flag -1].asc  = 0x40;		// DIAGNOSTIC FAILURE ON COMPONENT NN
	diag_test_log[sata_ch_flag -1].ascq = 0x81;		// WD FRU # for the first HDD

	disable_upkeep &= ~UPKEEP_RUNNING_SELF_TEST;
}


/************************************************************************\
 run_default_self_test() - Do the default quickie self-test.

 */
void run_default_self_test(PCDB_CTXT pCtxt)
{
	 u8 sata_ch_flag;
	 u8 test_flag = SATA_CH0;
	if(arrayMode != JBOD)
		sata_ch_flag = SATA_TWO_CHs;
	else if((pCtxt->CTXT_LUN == HDD0_LUN) && (sata0Obj.sobj_device_state == SATA_DEV_READY))
		sata_ch_flag = SATA_CH0;
	else
		sata_ch_flag = SATA_CH1;
	
	// Check (all) the HDD's SMART status; the test is a pass if the
	// HDD doesn't report "threshold exceeded."
	if (exec_dataless_smart_cmd(pCtxt, SMART_RETURN_STATUS, 0, sata_ch_flag))
		return;

	// Initialize and fill in a self-test results log.
	init_self_test_log(0 /*self-test code*/, sata_ch_flag);

	if(sata_ch_flag & SATA_CH0)
	{
		pSataObj = &sata0Obj;	
_CHECK_RESULT:
		if (pSataObj->pSata_Ch_Reg->sata_Status & ATA_STATUS_CHECK)
		{
			// The HDD failed the SMART command! The nerve...
			// Log and return an error.
			set_self_test_result(pCtxt, SELF_TEST_UNKNOWN_ERROR, ERROR_HW_FAIL_INTERNAL, test_flag);
		}
		else if ((pSataObj->pSata_Ch_Reg->sata_LbaH== 0xC2) &&
		          (pSataObj->pSata_Ch_Reg->sata_LbaM == 0x4F))
		{
			// 0x2CF4 The HDD did not detect threshold-exceeded; yea!
			set_self_test_result(pCtxt, SELF_TEST_NO_ERROR, ERROR_NONE, test_flag);
		}
		else
		{
			// The HDD has threshold-exceeded condition; the self-test failed.
			set_self_test_result(pCtxt, SELF_TEST_FAILED, ERROR_DIAG_FAIL_COMPONENT, test_flag);
		}
	}
	
	if((test_flag == SATA_CH0)&& (sata_ch_flag & SATA_CH1))
	{
		test_flag = SATA_CH1;//test sata channel 1
		pSataObj = &sata1Obj;	
		goto _CHECK_RESULT;

	}	
	if(sata_ch_flag & SATA_CH0)
		disable_upkeep &= ~UPKEEP_RUNNING_SELF_TEST_S0;
	if(sata_ch_flag & SATA_CH1)
		disable_upkeep &= ~UPKEEP_RUNNING_SELF_TEST_S1;

}

/************************************************************************\
 start_background_self_test() - Starts a background (off-line) self-test.

 Arguments:
	testType	1= short self-test; 2= extended self-test.

 Returns:
	Nothing.
 */
void start_background_self_test(PCDB_CTXT pCtxt, u8 testType)
{
	// It just so happens the two valid values for  testType, specifying
	// either a short or extended self-test, is the same for ATA SMART
	// commands as it is for SCSI SEND DIAGNOSTIC command's Self-Test Code.
	 u8 sata_ch_flag;
	 
	if(arrayMode != JBOD)
		sata_ch_flag = SATA_TWO_CHs;
	else if((pCtxt->CTXT_LUN == HDD0_LUN) && (sata0Obj.sobj_device_state == SATA_DEV_READY))
		sata_ch_flag = SATA_CH0;
	else
		sata_ch_flag = SATA_CH1;
	// Execute a SMART self-test in off-line mode.
	if (exec_dataless_smart_cmd(pCtxt, SMART_EXEC_OFF_LINE_IM, testType, sata_ch_flag))
		return;

	// Initializing a self-test results Log.
	init_self_test_log(testType, sata_ch_flag);

	// Return an HARDWARE FAILURE error if the SMART cmd failed.
	if(sata_ch_flag & SATA_CH0)
	{
		pSataObj = &sata0Obj;
		if (pSataObj->pSata_Ch_Reg->sata_Status & ATA_STATUS_CHECK)
		{
			set_self_test_result(pCtxt, SELF_TEST_UNKNOWN_ERROR, ERROR_HW_FAIL_INTERNAL, SATA_CH0);
		}
		else
		{
			disable_upkeep |= UPKEEP_RUNNING_SELF_TEST_S0;
		}
	}
	if(sata_ch_flag & SATA_CH1)
	{
		pSataObj = &sata1Obj;
		if (pSataObj->pSata_Ch_Reg->sata_Status & ATA_STATUS_CHECK)
		{
			set_self_test_result(pCtxt, SELF_TEST_UNKNOWN_ERROR, ERROR_HW_FAIL_INTERNAL, SATA_CH1);
		}
		else
		{
			disable_upkeep |= UPKEEP_RUNNING_SELF_TEST_S1;
		}
	}
}

/************************************************************************\
 abort_background_self_test() - Stops a background (off-line) self-test.

 */
void abort_background_self_test(PCDB_CTXT pCtxt)
{
	// Abort off-line mode self-test routine
	 u8 sata_ch_flag;
	 
	if(arrayMode != JBOD)
		sata_ch_flag = SATA_TWO_CHs;
	else if((pCtxt->CTXT_LUN == HDD0_LUN) && (sata0Obj.sobj_device_state == SATA_DEV_READY))
		sata_ch_flag = SATA_CH0;
	else
		sata_ch_flag = SATA_CH1;
	
	if (exec_dataless_smart_cmd(pCtxt, SMART_EXEC_OFF_LINE_IM, 0x7F /*abort test*/, sata_ch_flag))
		return;
	if(sata_ch_flag & SATA_CH0)
	{
		pSataObj = &sata0Obj;
		if (pSataObj->pSata_Ch_Reg->sata_Status & ATA_STATUS_CHECK)
		{
			// Uh, oh, the abort failed. Return SCSI error status but don't
			// update the self-test log - we don't know what's going on.
			hdd_err_2_sense_data(pCtxt, ERROR_HW_FAIL_INTERNAL);
		}
		else
		{
			// Set the self-test result to indicate the test was aborted.
			set_self_test_result(pCtxt, SELF_TEST_ABORTED_BY_CMD, ERROR_NONE,SATA_CH0);
			disable_upkeep &= ~UPKEEP_RUNNING_SELF_TEST_S0;
		}
	}
	if(sata_ch_flag & SATA_CH1)
	{
		pSataObj = &sata1Obj;
		if (pSataObj->pSata_Ch_Reg->sata_Status & ATA_STATUS_CHECK)
		{
			// Uh, oh, the abort failed. Return SCSI error status but don't
			// update the self-test log - we don't know what's going on.
			hdd_err_2_sense_data(pCtxt, ERROR_HW_FAIL_INTERNAL);
		}
		else
		{
			// Set the self-test result to indicate the test was aborted.
			set_self_test_result(pCtxt, SELF_TEST_ABORTED_BY_CMD, ERROR_NONE,SATA_CH1);
			disable_upkeep &= ~UPKEEP_RUNNING_SELF_TEST_S1;
		}
	}

}


/************************************************************************\
 diag_receive_diag_results_cmd() - RECEIVE DIAGNOSTIC RESULTS cmd handler.

 */
void diag_receive_diag_results_cmd(PCDB_CTXT pCtxt)
{
#if 0
	// Bit buckets do not support this command.
	if (product_detail.options & PO_IS_BIT_BUCKET)  goto invalid_opcode;
#endif
	DBG("rcv diag\n");

	if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
	{
		// Check the Page Code Valid (PCV) bit.
		if (!(CmdBlk(1) & 0x1))
		{	// get Page code from last Send DIAGNOSTIC Command
	//		CmdBlk[2] = diag_last_page_code;
			goto invalid_CDB_field;
		}

		// Special case for the Short Enclosure Status page: return
		// that page when any page in 0x01..0x2F is requested.
		if ((CmdBlk(2) >= 1) && (CmdBlk(2) <=0x2f))
		{
			CmdBlk(2) = DIAG_SHORT_STATUS;
		}
	}


	nextDirection = DIRECTION_SEND_DATA;
	alloc_len = (CmdBlk(3)<<8) + CmdBlk(4);

	// All diagnostic pages have a four-byte header:
	// Byte 0 is the page code.
	// Byte 1 is reserved or have other uses.
	// Bytes 2..3 is the 16-bit Page Length.

	// Pre-fill the buffer with zeros, setting reserved bytes to 0...
	xmemset(mc_buffer, 0, 128);

	// ...and fill in the Page Code field.
	mc_buffer[0] = CmdBlk(2);			// Requested Page Code.

	// The virtual CD-ROM supports a different set of pages
	// than the disk and SES logical units.
	//
	if (logicalUnit == cdrom_lun)
	{
		switch (CmdBlk(2) /*page code*/)
		{
		case DIAG_SUPPORTED_PAGES:
			// Byte 0 is the Page Code, already copied from the CDB.
			// Byte 1 is reserved, already set to 0.
			// Byte 2 is Page Length MSB, already set to 0.
			mc_buffer[3] = 1;			// Page Length LSB

			// Supported Diag Pages in ascending page code order...
			mc_buffer[4] = DIAG_SUPPORTED_PAGES;	// ...only one page!
			break;

		default:
			goto invalid_CDB_field;
		}
	}
	else				// Disk and SES logical units
	{
		switch (CmdBlk(2) /*page code*/)
		{
		case DIAG_SUPPORTED_PAGES:
			// Byte 0 is the Page Code, already copied from the CDB.
			// Byte 1 is reserved, already set to 0.
			// Page length is filled in later.

			// List supported Diag Pages in ascending page code order.
			mc_buffer[4] = DIAG_SUPPORTED_PAGES;
			mc_buffer[5] = DIAG_SHORT_STATUS;
			//mc_buffer[6] = DIAG_WDC_PWR_DOWN_CTRL;
			mc_buffer[7] = DIAG_WDC_TEMP_COND_STATUS;
			mc_buffer[8] = DIAG_WDC_SMART_CTRL;
			mc_buffer[9] = DIAG_WDC_SMART_DATA_STATUS;

			// Byte 2..3 is Page Length, which is the # of supported pages.
			mc_buffer[3] = 5;			// Number of diag pages so far.
			break;

		case DIAG_SHORT_STATUS:			// Short Enclosure Status page
			// Byte 0 is the Page Code, already copied from the CDB.
			// Byte 1 is vendor-specific status, already set to 0.
			// Byte 2..3 is Page Length, already set to 0.
			break;
#if 0
		case DIAG_WDC_PWR_DOWN_STATUS:
			// Byte 0 is the Page Code, already copied from the CDB.
			// Byte 1 is reserved, already set to 0.

			// Contents vary by product because some products
			// do not have a power button.
			if (product_detail.options & PO_HAS_POWER_BUTTON)
			{
				// Byte 2 is Page Length MSB, already set to 0.
				mc_buffer[3] = 1;		// Page Length LSB
				mc_buffer[4] = power_button_pressed? 0x04 : 0;

				power_button_pressed = 0;
			}
			break;
#endif
		case DIAG_WDC_TEMP_COND_STATUS:
			// Byte 0 is the Page Code, already copied from the CDB.
			// Byte 1 is reserved, already set to 0.
			// Byte 2 is Page Length MSB, already set to 0.
			mc_buffer[3] = 4;			// Page length LSB

			// Byte 4 bits 0..1 is the Temperature Condition.
#if FAN_CTRL_EXEC_TICKS
			mc_buffer[4] = getTCond();

			mc_buffer[6] = (u8)(get_fan_RPM() >> 8);
			mc_buffer[7] = (u8)get_fan_RPM();
#endif
			break;

		case DIAG_WDC_SMART_STATUS:
			// Byte 0 is the Page Code, already copied from the CDB.
			// Byte 1 is reserved, already set to 0.
			// Byte 2 is Page Length MSB, already set to 0.
			if (scsi_chk_hdd_pwr(pCtxt)) // if the HDD is spun down, wake up HDD firstly
				return;
			mc_buffer[3] = 3;			// Page Length LSB
			mc_buffer[4] = diag_drive_selected;
			
			// Issue a SMART Return Status command...
			if (exec_dataless_smart_cmd(pCtxt, SMART_RETURN_STATUS, 0,diag_drive_selected))
				break;
			pSataObj = (diag_drive_selected & SATA_CH1) ? &sata1Obj : & sata0Obj;
			// ...and put the HDD's SMART Status into bytes 5 and 6.
			if (pSataObj->pSata_Ch_Reg->sata_Status & ATA_STATUS_CHECK)
			{
				mc_buffer[5] = 0xee;	// Oops, an ATA error occured!
				mc_buffer[6] = 0xee;
			}
			else
			{
				// We don't care what the HDD's SMART status is; just report it.
				mc_buffer[5] = pSataObj->pSata_Ch_Reg->sata_LbaH;
				mc_buffer[6] = pSataObj->pSata_Ch_Reg->sata_LbaM;
			}
			break;

		case DIAG_WDC_SMART_DATA_STATUS:
			// Return the HDD's SMART data to the host.
			// Report ATA errors but return the SMART data as it is;
			// the host software has to checksum and parse it.
			if (scsi_chk_hdd_pwr(pCtxt)) // if the HDD is spun down, wake up HDD firstly
				return;
			u8 val = 0;
			if (sata0Obj.sobj_device_state == SATA_DEV_READY)
				val = get_smart_data(SATA_CH0);
			else if (sata1Obj.sobj_device_state == SATA_DEV_READY)
				val = get_smart_data(SATA_CH1);
			if (val)
			{
				hdd_err_2_sense_data(pCtxt, ERROR_HW_FAIL_INTERNAL);
				break;
			}

			// Move SMART data out of the way to make room for page header.
			{
				u8 nWords = 128;
				u32 *dp = (u32*)(mc_buffer+508);

				// Copy two bytes at a time, repeating 256 times.
				do
				{
					dp[2] = dp[0];
					dp--;
					nWords--;
				} while (nWords);
			}

			// Fill in the page header and other info.
			// Since the SMART data was occupying this space before,
			// every byte must be (re-)set here.
			//mc_buffer[0] = DIAG_WDC_SMART_DATA_STATUS;
			//mc_buffer[1] = 0;					// reserved
			//mc_buffer[2] = (516 >> 8);			// Page length MSB
			//mc_buffer[3] = (516 & 0xFF);		// Page length LSB
			*((u32 *) &(mc_buffer[0])) = (0x4<<24)|(0x02 << 16)|(DIAG_WDC_SMART_DATA_STATUS);

			//mc_buffer[4] = 1;					//diag_drive_selected;
			//mc_buffer[5] = 0;					// Reserved
			//mc_buffer[6] = 0;					// Reserved
			//mc_buffer[7] = 0;					// Reserved
			*((u32 *) &(mc_buffer[4])) = 1;
			break;

		default:		// Invalid or unsupported diagnostic page code.
			goto invalid_CDB_field;
		}
	}
	
	byteCnt = (mc_buffer[2] << 8) + mc_buffer[3] + 4;
	return;

invalid_CDB_field:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
	return;

invalid_opcode:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_OP);
	return;
}

/************************************************************************\
 diag_send_diagnostic_cmd() - SEND DIAGNOSTIC command handler.

 */
void diag_send_diagnostic_cmd(PCDB_CTXT pCtxt)
{
	u16 pageLen16;
	u8 lun;
	
	u32 l_byteCnt = (CmdBlk(3) << 8) + CmdBlk(4);

	// Make a copy of just the Self-Test Code and Self-Test bit only.
	// SEND DIAGNOSTIC is a 6-byte command, so there are many unused bytes.
	u8 self_test = CmdBlk(1) & (0xE0 | 0x04);
MSG("self_tst: %bx\n", self_test);

	if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
	{// firmware will check the CDB field for the data out command
		// Parameter list length must be zero for Self-Tests.
		if (self_test /*copy of Self-Test Code and Self-Test bit*/ )
		{
			if (l_byteCnt != 0)  goto invalid_CDB_field;
		}
	}
	
	if (scsi_chk_hdd_pwr(pCtxt)) 	return;	
	if (scsi_start_receive_data(pCtxt, l_byteCnt)) return;	

	// Other fields in CDB byte 1:
	// The PF bit can be ignored; our diagnostic pages all use the standard format.
	// Unit Offline and Device Offline - the default self-test does not
	// take the HDD offline so these bits can be ignored.

	// Special case the virtual CD-ROM: it only supports the mandatory
	// default self-test and diagnostic pages. Having this check here
	// avoids repeated checks later on.
	lun = pCtxt->CTXT_LUN;
	if (lun == cdrom_lun)
	{
		switch (self_test /*copy of Self-Test Code and Self-Test bit*/ )
		{
		case 0x04:		// Self-Test bit.
		case 0:			// Self-Test and Self-Test Code both 0.
			break;

		default:
			goto invalid_CDB_field;
		}
	}

	// Run diagnostics if the Self-Test or Self-Test Code is non-zero,
	// otherwise transfer and process the parameter list.
	//
	switch (self_test /*copy of Self-Test Code and Self-Test bit*/ )
	{
	case 0x04:		// Self-Test bit.
		run_default_self_test(pCtxt);	// Run default self-test.
		break;

	case (1 << 5):	// Start a short self-test in the background
		start_background_self_test(pCtxt, 1);
		break;

	case (2 << 5):	// Start an extended self-test in the background.
		start_background_self_test(pCtxt, 2);
		break;

	case (4 << 5):	// Abort the currently running background self-test.
		abort_background_self_test(pCtxt);
		break;

#if 0
	// Foreground test will tie up the whole device and so are optional.

	case (5 << 5):	// Start a foreground short self-test.
		diag_fg_short_self_test();
		break;

	case (6 << 5):	// Start a foreground extended self-test.
		diag_fg_extend_self_test();
		break;
#endif

	case 0:			// Self-Test and Self-Test Code both 0.
		// Transfer and perform the request(s) in the parameter list.

		// Nothing to do if the Parameter List Length is 0.
		if ((CmdBlk(3) | CmdBlk(4)) == 0)  break;


		//check transfer size
		pageLen16 = (mc_buffer[2] << 8) + mc_buffer[3] + 4;

#if 0
		DBG("pageLen16 %x\tbyteCnt %lx\nmc_buf:\n", pageLen16,byteCnt);
		for (u32 i = 0; i < 7; i++)
			DBG("%bx\t", mc_buffer[i]);
		DBG("\n");
#endif
		if (byteCnt < pageLen16)
		{ // data truncated
			goto invalid_param_field;
		}

		// parse page code of Diagnostic Page.
		// make LUN specific!!
		diag_last_page_code = mc_buffer[0];

		// Special case the virtual CD-ROM: only page 0 is supported.
		if (logicalUnit == cdrom_lun)
		{
			if (mc_buffer[0] != DIAG_SUPPORTED_PAGES)
				goto invalid_param_field;
		}

		switch (mc_buffer[0] /*page code*/ )
		{
		case DIAG_SUPPORTED_PAGES:
			// Make sure only the page header (4 bytes) was sent, per SPC-2.
			if (pageLen16 != 4)  goto invalid_param_field;

			// Nothing else to do.
			break;

		case DIAG_SHORT_STATUS:
			hdd_err_2_sense_data(pCtxt, ERROR_UNSUPPORTED_SES_FUNCTION);
			break;

#if 0
		case DIAG_WDC_PWR_DOWN_CTRL:
			// Make sure we got the entire page.
			if (pageLen16 != 5)  goto invalid_param_field;

			// Byte 4 bit 0 is the Power-Off bit.
			if (mc_buffer[4] & 0x01 /*power-off*/ )
			{
				// Tell the USB main loop and Auto-Power to shutdown.
				pwr_off_enable = 1;
			}

			// Byte 4 bit 1 is Restart Lights; irrelevant for these products.
			// Byte 4 bit 2 is Stop Lights; also irrelevant.
			// Byte 4 bit 4 is Force Fan On; nothing to do, there is no fan!
			break;
#endif
		case DIAG_WDC_SMART_CTRL:
			// Make sure we got the entire page.
			if (pageLen16 != 7)  goto invalid_param_field;

			// Byte 4 is the Diagnostic Drive ID.
			if (arrayMode == JBOD)
			{
				if (!(((mc_buffer[4] == 1) && (sata0Obj.sobj_device_state == SATA_DEV_READY)) ||
				  ((mc_buffer[4] == 2) && (sata1Obj.sobj_device_state == SATA_DEV_READY))))
				 {
				 	goto invalid_param_field;
				 }
			}
			else
			{
				if (mc_buffer[4] != 1) goto invalid_param_field;
			}

			diag_drive_selected = mc_buffer[4];

			// Bytes 5..6 is the SMART Status field and is ignored here.
			break;

		default:		// Invalid Page Code.
			goto invalid_param_field;
		}
		break;	// End of  case 0

	default:	// Invalid or unsupported Self-Test combination.
		goto invalid_CDB_field;
	}

	return;

data_protect_error:
	hdd_err_2_sense_data(pCtxt, ERROR_WRITE_PROTECT);
	return;

invalid_CDB_field:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
	return;

invalid_param_field:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_PARAM);
	return;

invalid_opcode:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_OP);
	return;
}


/************************************************************************\
 diag_log_sense_cmd() - LOG SENSE command handler.

 */
void diag_log_sense_cmd(PCDB_CTXT pCtxt)
{
u8 pc;

	// Assume data will be sent back.
	nextDirection = DIRECTION_SEND_DATA;
	alloc_len = (CmdBlk(7) << 8) + CmdBlk(8);

#if 0
	// Bit buckets do not support this command.
	if (product_detail.options & PO_IS_BIT_BUCKET)  goto invalid_opcode;
#endif

	// Check "PPC", "SP", & Reserved bits: they must all be 0.
	if (CmdBlk(1))  goto invalid_CDB_field;

	// Extract Page Control field.
	pc = (CmdBlk(2) >> 6) & 0x03;
	// Old bug alert: Older versions mistakenly returned error if PC wasn't 1 or 3.

	// The Parameter Pointer field has to be 0.
	if (CmdBlk(5) | CmdBlk(6))  goto invalid_CDB_field;


	// Assemble the requested log page. All pages have a four-byte header:
	// Byte 0 is the page code.
	// Byte 1 is reserved.
	// Bytes 2..3 is the 16-bit Page Length.

	// Pre-fill the buffer with zeros, setting reserved bytes to 0...
	xmemset(mc_buffer, 0, 128);

	// ...and fill in the Page Code field.
	mc_buffer[0] = CmdBlk(2) & LOG_PAGE_CODE_MASK;

	switch (mc_buffer[0] /*page code*/)
	{
	case LOG_SUPPORTED_PAGES:
		// Byte 0 is the Page Code, already copied from the CDB.
		// Byte 1 is reserved, already set to 0.
		// Byte 2 is Page Length MSB, already set to 0.
		mc_buffer[3] = 2;				// Page length LSB

		// Page codes in ascending numerical order.
		mc_buffer[4] = LOG_SUPPORTED_PAGES;
		mc_buffer[5] = LOG_SELF_TEST_PAGE;

		byteCnt = 6;
		break;

	case LOG_SELF_TEST_PAGE:			// Self-Test Results page
		// Update the self-test results log if a self-test in running.
		if (((pCtxt->CTXT_LUN == HDD0_LUN) && (disable_upkeep & UPKEEP_RUNNING_SELF_TEST_S0))
		|| ((pCtxt->CTXT_LUN == HDD1_LUN) && (disable_upkeep & UPKEEP_RUNNING_SELF_TEST_S1)))
		{
			update_self_test_log(pCtxt);
		}

		// Updating the self-test log clobbers the mc_buffer,
		// so build this results page from scratch.
		xmemset(mc_buffer, 0, 404);

		mc_buffer[0] = LOG_SELF_TEST_PAGE;
		// Byte 1 is reserved, already set to 0.
		mc_buffer[2] = 400 >> 8;				// Page length
		mc_buffer[3] = (u8)400;				// Page length

      		  // only provide one test result for now
		if (diag_drive_selected == SATA_CH1)
			xmemcpy((u8 *)&diag_test_log[1], &mc_buffer[4], sizeof(DIAG_TEST_LOG));
		else
			xmemcpy((u8 *)&diag_test_log[0], &mc_buffer[4], sizeof(DIAG_TEST_LOG));
		byteCnt = 404;
		break;

	default:			// Invalid or unsupported log page code.
		goto invalid_CDB_field;
	}

	return;

invalid_CDB_field:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
	return;

invalid_opcode:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_OP);
	return;
}

/************************************************************************\
 diag_log_select_cmd() - LOG SELECT command handler.

 */
void diag_log_select_cmd(PCDB_CTXT pCtxt)
{
#if 0
	// Bit buckets do not support this command.
	if (product_detail.options & PO_IS_BIT_BUCKET)
	{
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_OP);
		return;
	}
#endif

	// Save Parameters (SP) is not supported.
	if (CmdBlk(1) & 0x01 /*SP*/ )
	{
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
		return;
	}

	// No settable log pages, so make sure Parameter List Len is zero.
	if (CmdBlk(7) || CmdBlk(8))
	{
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
		return;
	}

	// All OK, nothing else to do - return GOOD status.
	nextDirection = DIRECTION_SEND_STATUS;
}
// EOF
