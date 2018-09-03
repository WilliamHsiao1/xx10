/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/
#define HANDY_C
#include "general.h"


// Number of handy-store blocks.
// Warning: Don't increase this without code review!
#define HANDY_STORE_SIZE		16

// Maximum number of blocks that can be read or written at once.
#define MAX_HANDY_STORE_XFER	1

// Starting location of the handy store in the FW private area
#define INI_HANDY_STORE_START	0x88

#ifdef WDC_HANDY
/********************************************************\
	The READ HANDY CAPACITY command
	returns information about the Handy Store.
	
	Logical Units (Disk, SES)
\********************************************************/
void handy_read_handy_capacity_cmd(PCDB_CTXT pCtxt) // --->v0.07_14.32_0404_A
{

	if(scsi_wdc_param_check(TYPE_DESTLUN,pCtxt))
	{
		return;
	}
	
	xmemset(mc_buffer, 0, 12);
	byteCnt = 12;
	alloc_len = 12;

	// Bytes 0..3 is the Last Handy Block Address
	mc_buffer[3] = HANDY_STORE_SIZE - 1;

	// Bytes 4..7 is the Block Length (512 bytes)
	mc_buffer[6] = 0x02;	


	// Bytes 8..9 are reserved, already set to 0.

	// Bytes 10..11 is Max Transfer Length (blocks)
	mc_buffer[11] = MAX_HANDY_STORE_XFER;

	//usb_dir = 1;
	nextDirection = DIRECTION_SEND_DATA;
	return;
}

/*****************************************************\
	The READ and WRITE HANDY STORE commands transfers
	data to/from the handy store area.

	Logical Units (Disk, SES)
\*****************************************************/

void handy_read_write_store_cmd(PCDB_CTXT pCtxt)
{
	// --->v0.07_14.32_0328_A 
	if(scsi_wdc_param_check(TYPE_DESTLUN,pCtxt))
	{
		return;
	}
	u8 dest_lun = cdb.byte[6] & MASK_CP_DESTLUN; // --->v0.07_14.32_0404_A
	
	// Don't allow transfers larger than allowed.
	u16 sec_cnt = (cdb.rw10.xfer[0] << 8) + cdb.rw10.xfer[1]; 
	u32 tmp32 = (cdb.rw10.lba[0] << 24) + (cdb.rw10.lba[1] << 16) + (cdb.rw10.lba[2] << 8) + cdb.rw10.lba[3];

	if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
	{
		// Writes not allowed when Locked (or Unlock Exceeded) or Not Configured.
		if ( (cdb.opcode == SCSI_WDC_WRITE_HANDY_STORE)  &&
		     IS_LOCKED_OR_UNCONFIGED(dest_lun) ) 
		{
			hdd_err_2_sense_data(pCtxt, ERROR_WRITE_PROTECT);
			return;
		}
		
		// Nothing to do if Transfer Length is zero.
		if (sec_cnt == 0)
		{
			nextDirection = DIRECTION_SEND_STATUS;
			return;
		}
		
		if (sec_cnt > MAX_HANDY_STORE_XFER)
		{
			hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
			return;
		}

		// Bounds-check the requsted blocks and transfer length.
		if (  (tmp32 >= HANDY_STORE_SIZE) ||
		     ((tmp32 + sec_cnt) > HANDY_STORE_SIZE) )
		{
			hdd_err_2_sense_data(pCtxt, ERROR_LBA_OUT_RANGE);
			return;
		}
	}
	if (scsi_chk_hdd_pwr(pCtxt))	return;


	// Compute the absolute LBA of the requested handy-store block.
	//
	// First align the handy block address to a FW private page boundary...
#if (HANDY_STORE_SIZE << FW_PRIVATE_ALIGNMENT_LOG2) < 256

	// Use cheaper math if there are only a few handy store blocks.
	cdb.byte[5] <<= FW_PRIVATE_ALIGNMENT_LOG2;
#else
	tmp32 <<= FW_PRIVATE_ALIGNMENT_LOG2;
#endif

	// ...add the relative starting location in the FW private area...
	tmp32 += INI_HANDY_STORE_START;

#if 1
	WRITE_U32_BE(cdb.rw10.lba, tmp32);

	#ifdef DEBUG
		for (i = 0; i < 4; i++)
		{
			printf("%bx\t", cdb.rw10.lba[i]);
		}
		printf("\n");
	#endif


	// ...rearrange the CDB to make room for LBA math...
	cdb.byte[10] = 0;
	cdb.byte[11] = 0;
	cdb.byte[12] = cdb.byte[7];
	cdb.byte[13] = cdb.byte[8];

	// Copy and zero-extend the LBA.
	cdb.byte[ 6] = cdb.byte[2];
	cdb.byte[ 7] = cdb.byte[3];
	cdb.byte[ 8] = cdb.byte[4];
	cdb.byte[ 9] = cdb.byte[5];
	cdb.byte[ 5] = cdb.byte[4] = cdb.byte[3] = cdb.byte[2] = 0;
#endif
	// ...and finally add the FW private area's starting LBA.	
	u64 tmp64;
	tmp64 = tmp32;
	WRITE_U64_BE(cdb.rw16x.lba, tmp64);



	// Turn on and re-init the HDD if it is off.
	ata_load_fis_from_rw16Cdb(pCtxt, DMA_TYPE);
	
	pCtxt->CTXT_FLAG = 0;

	pCtxt->CTXT_Status = CTXT_STATUS_PENDING;
	pCtxt->CTXT_ccm_cmdinten = 0; 
	
	if (CmdBlk(0)== SCSI_WDC_WRITE_HANDY_STORE)
	{
		pCtxt->CTXT_Prot = PROT_PIOOUT;
		pCtxt->CTXT_PHASE_CASE = BIT_12; // case 12
		pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S0W;
//		pCtxt->CTXT_DBUF_IN_OUT = (TX_DBUF_SATA0_R_PORT << 4) | TX_DBUF_USB_W_PORT; 
		pCtxt->CTXT_FIS[FIS_COMMAND] = ATA6_WRITE_LOG_EXT;
	}
	else
	{
		// case 5 (Hi > Di) or case  6(Hi = Di)
		pCtxt->CTXT_Prot = PROT_PIOIN;
		pCtxt->CTXT_PHASE_CASE = BIT_6; // case 6
		pCtxt->CTXT_DbufIndex = DBUF_SEG_U2S0R;
//		pCtxt->CTXT_DBUF_IN_OUT = (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_SATA0_W_PORT; 
		pCtxt->CTXT_FIS[FIS_COMMAND] = ATA6_READ_LOG_EXT;
	}	

	pSataObj = dest_lun ? &sata1Obj : &sata0Obj;
	sata_exec_ctxt(pSataObj, pCtxt);

	return;
}
#endif

// EOF
