/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************
 * 
 * 3610		2013/01/20	Bill		Initial version
 *
 *****************************************************************************/
#ifndef RAID_C
#define RAID_C
#include "general.h"

#define DEFAULT_REBUILD_RANGE_LENTH	0x800			// 1 MiB	(512B per sector)
#define DEFAULT_REBUILD_METADATA_UPDATE_THRESHOLD		0x200000	// 1 GiB	

#define MAX_REBUILD_XFER_LENTH 		0x10000			// 32 MiB
#define REBUILD_RANGE_LENTH		0x10000//DEFAULT_REBUILD_RANGE_LENTH	
#define REBUILD_METADATA_UPDATE_THRESHOLD		0x1000000//DEFAULT_REBUILD_METADATA_UPDATE_THRESHOLD

static u64 rebuildErrLBA = 0;
static u8 rebuildRetryCnt = 0;
static const u8 maxRebuildRetryCnt = 5;		// the allowable times of rebuild 

static u8 instant_rebuild_cnt = 0;		// to count how many instant rebuild occured in a power circle
static const u8 instant_rebuild_threshold = 5;	// the allowable times of instant rebuild 

static const u8 report_priority[] = 
	{0x01, 0x02, 0x0B, 0x0A, 0x04, 0x05, 0x06, 0x07, 0x03, 0x08, 0x09};	// table 6-1 the smaller, the higher priority

#ifdef SAVE_RAID_IN_FLASH_ONLY
static u8 newer_disk;  // 1(slot0), 2(slot1), 3(both)
#endif

void raid_init(void)
{
#ifdef INITIO_STANDARD_FW
	*raid_config = RAID_SATA_BLKSIZE_512 | RAID0_STRIPE_SIZE_4K;
#else
	u16 stripe_size;
	if (ram_rp[0].stripe_size > 7) 
	{// the stripe size is limited to 64K in hardware RAID engine
		stripe_size = RAID0_STRIPE_SIZE_4K;
	}
	else
	{
		stripe_size = ((u16)(ram_rp[0].stripe_size & 0x0F)) << 12;
	}
	MSG("strpSz %x\n", stripe_size);
	*raid_config = RAID_SATA_BLKSIZE_512 | stripe_size;
#endif
	if (arrayMode == RAID1_MIRROR)
	{
		*raid_config |= RAID_DEV;
	}
#ifdef SUPPORT_SPAN
	else if (arrayMode == SPANNING)
	{
		u64 tmp64 = (sata0Obj.sectorLBA & 0xFFFFFFFFFFFFF800);
		if ((product_detail.options & PO_NO_CDROM) == 0)
		{
			tmp64 -= INI_VCD_SIZE;
		}
#ifndef WDC_RAID
		tmp64 -= INI_PRIVATE_SIZE;
#endif
		// span capacity is only used in span mode
		*SPAN_capacity = tmp64;
		*raid_config |= SPAN_DEV0;
	}
#endif
	else
	{
		*raid_config |= STRIPE_DEV0;
	}

	if ((arrayMode == RAID0_STRIPING_DEV)
#ifdef SUPPORT_SPAN
		|| (arrayMode == SPANNING)
#endif
		)
	{
		// if the two drives are swapping, we need programmed the hardware to support
		if ((cur_status[SLOT0] == SS_GOOD) && (cur_status[SLOT1] == SS_GOOD))
		{
			if ((ram_rp[0].disk_num == 1) && (ram_rp[1].disk_num == 0))
			{
				MSG("drive swap\n");
#ifdef SUPPORT_SPAN
				if (arrayMode == SPANNING)
				{
					u64 tmp64 = (sata1Obj.sectorLBA & 0xFFFFFFFFFFFFF800);
					if ((product_detail.options & PO_NO_CDROM) == 0)
					{
						tmp64 -= INI_VCD_SIZE;
					}
#ifndef WDC_RAID
					tmp64 -= INI_PRIVATE_SIZE;
#endif
					// span capacity is only used in span mode
					*SPAN_capacity = tmp64;
					*raid_config |= SPAN_DEV1;
				}
				else
#endif
				*raid_config |= STRIPE_DEV1; // hardware starts from drive 1
			}
		}
	}
	instant_rebuild_flag = 0;
	MSG("rdCon %x\n", *raid_config);	
}


// when fetch the raid Params from drive, check the header to see if there's valid raid parameters
// after that, populate two structures for the other module
// populate the raid Params to raidParam[n]
// set one to raidParamsValid[n]
static u8 check_valid_raidParams(u8 *ptr)
{
	if ( ((PRAID_PARMS)ptr)->header.signature == WDC_RAID_PARAM_SIG)
	{
		return verify_metadata_integrity(ptr);
	}
	else return 0;
}

static void populate_slot_mapping(void)
{
	#define ID_STRING_LEN  61

	u8 nvm_disk_num0 = 0xFF;
	u8 nvm_disk_num1 = 0xFF;

	u8 nvm0_id[ID_STRING_LEN]={((u8)0x20)};
	u8 nvm1_id[ID_STRING_LEN]={((u8)0x20)};
	u8 slot0_id[ID_STRING_LEN]={((u8)0x30)};
	u8 slot1_id[ID_STRING_LEN]={((u8)0x30)};
	slot_to_nvm[0] = 0xFF;
	slot_to_nvm[1] = 0xFF;
#ifdef SAVE_RAID_IN_FLASH_ONLY
	newer_disk = 0;
	MSG("psm: %bx %bx %bx %bx\n", sata0Obj.sobj_State, sata1Obj.sobj_State, sata0Obj.sobj_device_state, sata1Obj.sobj_device_state);
#endif
	
	if (sata0Obj.sobj_device_state == SATA_DEV_READY)
	{
		slot0_id[0] = 'a';
		// copy modelNum and SN
		xmemcpy(sata0Obj.modelNum, &slot0_id[1], 60);
	}

	if (sata1Obj.sobj_device_state == SATA_DEV_READY)
	{
		slot1_id[0] = 'a';
		// copy modelNum and SN
		xmemcpy(sata1Obj.modelNum, &slot1_id[1], 60);
	}

	// check slot0_id == nvm0_id
	if (check_valid_raidParams((u8 *)&nvm_rp[0]) == 0)
	{
		nvm_disk_num0 = nvm_rp[0].disk_num;
		if (nvm_disk_num0 < 2)
		{
			xmemcpy(&nvm_rp[0].disk[nvm_disk_num0].disk_bus_type, nvm0_id, 61);
		}		
	}

	if (check_valid_raidParams((u8 *)&nvm_rp[1]) == 0)
	{
		nvm_disk_num1 = nvm_rp[1].disk_num;
		if (nvm_disk_num1 < 2)
		{
			xmemcpy(&nvm_rp[1].disk[nvm_disk_num1].disk_bus_type, nvm1_id, 61);
		}
	}


#ifdef DBG_FUNCTION
	if (nvm_disk_num0 != 0)
	{// the Drive attached to slot0 is possible to be used in other RAID array
		MSG("slot0 dNum %bx\n", nvm_disk_num0); 
	}
	if (nvm_disk_num1 != 1)
	{
		MSG("slot1 dNum %bx\n", nvm_disk_num1);
	}
#endif
	
	if (xmemcmp(slot0_id, nvm0_id, 61))
	{ // slot0_ID != nvm0_ID	
		if (xmemcmp(slot0_id, nvm1_id, 61) == 0)// check slot0_id == nvm1_id
		{
			slot_to_nvm[0] = 1;
		}
	}
	else
	{
		slot_to_nvm[0] = 0;
	}
	
	if (xmemcmp(slot1_id, nvm0_id, 61))
	{ // slot1_ID != nvm0_ID	
		if (xmemcmp(slot1_id, nvm1_id, 61) == 0)// check slot0_id == nvm1_id
		{
			slot_to_nvm[1] = 1;
		}
	}
	else
	{
		slot_to_nvm[1] = 0;
	}
	
	for (u8 nvm_index = 0; nvm_index < 2;nvm_index++)
	{
		u8 found_it = 0;
		for (u8 slot_index = 0; slot_index <2; slot_index++)
		{
			if(slot_to_nvm[slot_index] == nvm_index)
			{
				found_it = 1;
				break;
			}
		}
		
		if(found_it == 0)
		{
			for (slot_index = 0; slot_index <2; slot_index++)
			{
				if(0xFF == slot_to_nvm[slot_index])
				{
#ifdef SAVE_RAID_IN_FLASH_ONLY
					newer_disk |= (slot_index+1);  // 1(slot0), 2(slot1), 3(both)
#endif
					slot_to_nvm[slot_index] = nvm_index;
					break;
				}
			}
		}
	}

#ifdef SAVE_RAID_IN_FLASH_ONLY
	MSG("newer_disk %bx  ", newer_disk);
#endif
	/***************
	A+B: 	slot_to_nvm[0] = 0; slot_to_nvm[1] = 1;
	B+A:	slot_to_nvm[0] = 1; slot_to_nvm[1] = 0;
	A+C:	slot_to_nvm[0] = 0; (slot_to_nvm[1] = ff; slot_to_nvm[1] = 1;) 
	C+B:	(slot_to_nvm[0] = ff; slot_to_nvm[0] = 0;) slot_to_nvm[1] = 1;  
	C+A:	(slot_to_nvm[0] = ff; slot_to_nvm[0] = 1;) slot_to_nvm[1] = 0;
	B+C:	slot_to_nvm[0] = 1; (slot_to_nvm[1] = ff; slot_to_nvm[1] = 0;) 
	***************/
}

static u8 read_3rd_copy_raidParams(u8 slotNum)
{
	u8 nvm_index = slot_to_nvm[slotNum];
	PSATA_OBJ pSob = (slotNum == 0) ? &sata0Obj : &sata1Obj;
	
	xmemcpy((u8 *)(&nvm_rp[nvm_index]), (u8 *)&tmp_rp, sizeof(RAID_PARMS));
	if (check_valid_raidParams((u8 *)&tmp_rp) == 0)
	{
		u8 nvm_disk = tmp_rp.disk_num;
		if (xmemcmp(tmp_rp.disk[nvm_disk].disk_model_num, pSob->modelNum, 60) == 0)
		{// compare ID strings between HDD's & NVM's
			return COPY3_NVM_GOOD;
		}
	}

	xmemset((u8 *)&tmp_rp, 0, sizeof(RAID_PARMS));
	return COPY_INVALID;
}


static u8 find_good_copy(u8 slotNum, u8 pageAddr)
{
	DBG("find gcpy\n");

#ifdef SAVE_RAID_IN_FLASH_ONLY
	// hdr2526 think the good copy is in FLASH
	if (pageAddr != WD_RAID_PARAM_PAGE)
#endif		
	{
		if ( Read_Disk_MetaData(slotNum, pageAddr, WD_LOG_ADDRESS) == 0)
		{
			return COPY1_DISK_GOOD; //good_copy = 1;
		}

		if (Read_Disk_MetaData(slotNum, pageAddr, WD_BACKUP_LOG_ADDRESS) == 0)
		{
			return COPY2_DISK_GOOD;  //good_copy = 2;
		}
	}

	if (pageAddr == WD_RAID_PARAM_PAGE)
	{
		if (read_3rd_copy_raidParams(slotNum) == COPY3_NVM_GOOD)
		{
			return COPY3_NVM_GOOD;	//good_copy = 3;
		}
	}
	
	return COPY_INVALID;
	
}

// 
static void restore_copy(u8 goodCopy, u8 slotNum, u8 pageAddr)
{	// just support raid param page now
	u8 nvm_index = slot_to_nvm[slotNum];
	u8 rc =0;

	// restore copy 1
	if(goodCopy != COPY1_DISK_GOOD)
	{
		Write_Disk_MetaData(slotNum, WD_RAID_PARAM_PAGE, FIRST_BACKUP);
	}

	// restore copy 2
	if(goodCopy != COPY2_DISK_GOOD)
	{
		//if copy2 is not good
		// read copy2 and compare the data to good copy
		xmemset((u8 *)&tmp_rp, 0xee, sizeof(RAID_PARMS));
		
		rc = Read_Disk_MetaData(slotNum, WD_RAID_PARAM_PAGE, WD_BACKUP_LOG_ADDRESS);
		
		if((rc != INI_RC_OK) ||(xmemcmp((u8 *)&ram_rp[slotNum], (u8 *)(&tmp_rp),  SIZE_RAID_PARMS)))
		{
			Write_Disk_MetaData(slotNum, WD_RAID_PARAM_PAGE, SECOND_BACKUP);
		}		
	}
	
	// restore copy 3
	if (pageAddr == WD_RAID_PARAM_PAGE)
	{
		if ( goodCopy != COPY3_NVM_GOOD)
		{
			// the copy3 is readed and cached in nvm_rp
			if(xmemcmp((u8 *)(&ram_rp[slotNum]),(u8 *)(&nvm_rp[nvm_index]), sizeof(RAID_PARMS)))
			{
				 //compare fail 
				 // we need very be carefull to write flash size, erase every sector is 4k
				 // if erase some of the data ,we must re-write it to flash!!
				 spi_read_flash((u8 *)&mc_buffer, WDC_RAID_PARAM_ADDR, RAID_VITAL_TOTAL_SIZE);
				 u16 addr = (slotNum == 0) ? 0 : sizeof(RAID_PARMS);
				 xmemcpy((u8 *)&ram_rp[slotNum], (u8 *)&mc_buffer[addr], SIZE_RAID_PARMS);
				 spi_write_flash((u8 *)&mc_buffer, WDC_RAID_PARAM_ADDR, RAID_VITAL_TOTAL_SIZE);	
			}		
		}
	}
}

static u8 read_restore_slot_matadata(u8 slotNum, u8 pageAddr)
{
	u8 good_copy = find_good_copy(slotNum, pageAddr);
	
	MSG("gd cpy %bx\n", good_copy);
	
	if (good_copy != COPY_INVALID)
	{
		//copy the data to relational ram_rp buffer for  use.
		if (pageAddr == WD_RAID_PARAM_PAGE)
		{
			xmemcpy((u8 *)(&tmp_rp), (u8 *)&ram_rp[slotNum], sizeof(RAID_PARMS));
#ifndef SAVE_RAID_IN_FLASH_ONLY
			restore_copy(good_copy, slotNum, pageAddr);
#endif
		}
		else if (pageAddr == WD_OTHER_PARAM_PAGE)
		{
			xmemcpy((u8 *)(&tmp_other_Params), (u8 *)&other_Params[slotNum], sizeof(OTHER_PARMS));
			//restore_copy(good_copy, slotNum, pageAddr);
		}
		return 0; // have a good copy
	}

	return 1; // no valid copy
}

static u8 get_disk_num(u8 slot_num, u8 status) 
{ 
	/*//This function assumes at least one slot has a status of Good.  
	//If the status for the requested slot is Good 
	//then the disk number from that slot is returned; otherwise,
	//the index to the other slot is used to get the other disk number 
	//and then the opposite of that other disk number is returned. */
	u8 other_slot;
	u8 other_disk;
	if(status == SS_GOOD) 
	{ 
#ifdef DBG_FUNCTION
		if (ram_rp[slot_num].disk_num > 2)
			MSG("Wdn\n");
#endif
		return ram_rp[slot_num].disk_num;
	} 
	else 
	{ 
		other_slot = (slot_num == 0) ? 1 : 0;
		other_disk = ram_rp[other_slot].disk_num;
		return (other_disk == 0) ? 1 : 0;
	} 
}

static void use_meta_stat(u8 slotNum)
{
#ifdef DBG_FUNCTION
		if (ram_rp[slotNum].disk_num > 2)
			MSG("Wdn\n");
#endif

	u8 diskNum = ram_rp[slotNum].disk_num;
	cur_status[slotNum] = ram_rp[slotNum].disk[diskNum].disk_status;
}

static void set_stat(u8 slotNum, u8 slotStatus)
{
	u8 diskNum = get_disk_num(slotNum, slotStatus);

	ram_rp[slotNum].disk[diskNum].disk_status = slotStatus;
	cur_status[slotNum] = slotStatus;
}

static void clear_slot_metadata(u8 slotNum)
{  // clear slot data most fields
	ram_rp[slotNum].raidGenNum = 0x00;
	ram_rp[slotNum].disk[0].disk_status = SS_EMPTY;
	ram_rp[slotNum].disk[1].disk_status = SS_EMPTY;
	// disk bustype 1byte and model num 40 byte and disk serial num 20 byte are set to 0x00
	xmemset((u8 *)&ram_rp[slotNum].disk[0].disk_bus_type, 0x00, 61);
	xmemset((u8 *)&ram_rp[slotNum].disk[1].disk_bus_type, 0x00, 61);
	
	ram_rp[slotNum].array_size = 0x00;
	ram_rp[slotNum].array_mode = NOT_CONFIGURED;
	ram_rp[slotNum].stripe_size = 0x00;
	ram_rp[slotNum].disk_num = 0x80|slotNum;
}

/*******************************\
copy_slot_metadata()

The Disk Number field is not copied and callers must ensure 
that each slot gets assigned a unique Disk Number value

*****************************/
static void copy_slot_metadata(u8 dest_slotNum, u8 src_slotNum)
{
	ram_rp[dest_slotNum].raidGenNum = ram_rp[src_slotNum].raidGenNum;
		
	// disk bustype 1byte and model num 40 byte and disk serial num 20 byte are copy together
	xmemcpy((u8 *)&ram_rp[src_slotNum].disk[0].disk_bus_type, (u8 *)&ram_rp[dest_slotNum].disk[0].disk_bus_type, 61 );
	xmemcpy((u8 *)&ram_rp[src_slotNum].disk[1].disk_bus_type, (u8 *)&ram_rp[dest_slotNum].disk[1].disk_bus_type, 61 );

	ram_rp[dest_slotNum].vcd_size = ram_rp[src_slotNum].vcd_size;
	ram_rp[dest_slotNum].array_size = ram_rp[src_slotNum].array_size;
	ram_rp[dest_slotNum].array_mode = ram_rp[src_slotNum].array_mode;
	ram_rp[dest_slotNum].stripe_size = ram_rp[src_slotNum].stripe_size;

	ram_rp[dest_slotNum].config_VID[0] = ram_rp[src_slotNum].config_VID[0];
	ram_rp[dest_slotNum].config_VID[1] = ram_rp[src_slotNum].config_VID[1];
	ram_rp[dest_slotNum].config_PID[0] = ram_rp[src_slotNum].config_PID[0];
	ram_rp[dest_slotNum].config_PID[1] = ram_rp[src_slotNum].config_PID[1];
}

static void determine_slot_status(u8 slotNum)
{
	clear_slot_metadata(slotNum);
	cur_status[slotNum] = SS_EMPTY;
	
	PSATA_OBJ pSob = slotNum ? &sata1Obj : &sata0Obj;
	
	if (pSob->sobj_device_state == SATA_DEV_READY)
	{// the disk is present
		if (pSob->sobj_Init_Failure_Reason == HDD_INIT_NO_FAILURE)
		{// the Disk is accepted HDD 
			//Read & Restore Slot Metadata(slot_num)
			if (read_restore_slot_matadata(slotNum, WD_RAID_PARAM_PAGE))
			{	//can not find a good copy
				set_stat(slotNum, SS_BLANK);
			}
			else
			{
#ifdef WDC_RAID
				u16 new_vid = (u16)ram_rp[slotNum].config_VID[1] |((u16)ram_rp[slotNum].config_VID[0] << 8);
				u16 new_pid = (u16)ram_rp[slotNum].config_PID[1] |((u16)ram_rp[slotNum].config_PID[0] << 8);;
				
				if (check_product_config(new_vid, new_pid) == ILLEGAL_BOOT_UP_PRODUCT)
				{// no match vid pid, it is Rejected
					set_stat(slotNum, SS_SOFT_REJECTED);
				}
				else
#endif					
				{
					u8 nvm_disk = ram_rp[slotNum].disk_num;
					
					// compare modelNum and SN
					if (xmemcmp(pSob->modelNum, ram_rp[slotNum].disk[nvm_disk].disk_model_num, 60))
					{	// save's SN & model num is different with currend HDD's	
						set_stat(slotNum, SS_ID_MISMATCH);
					}
					else
					{
						use_meta_stat(slotNum);
					}
				}
			}
		}
		else
		{// the Disk is rejected because it does not match the white list of 
			set_stat(slotNum, SS_HARD_REJECTED);
		}
	}
	// else the disk is not present, SS_EMPTY
}

#if 0
/*******************************\
AssignHomeToOrphans()

show how Blank, Rejected, and Empty slots are grouped with other slots to avoid 
making multiple RAID arrays when just one array is the better result

*****************************/
static void assign_home_to_orphans(void)
{
	// check slot status and clear slot0 data fields
	if((cur_status[SLOT0] == SS_EMPTY) || (cur_status[SLOT0] == SS_BLANK))
	{
		clear_slot_metadata(0); 
	}
	//check slot status and clear slot1 data fields
	if((cur_status[SLOT1] == SS_EMPTY) || (cur_status[SLOT1] == SS_BLANK))
	{
		clear_slot_metadata(1);
	}
	// check array mode
	if((ram_rp[0].array_mode != JBOD) && (ram_rp[1].array_mode != JBOD))
	{
		// check slot status and copy slot1 data to slot0
		if((cur_status[SLOT0] == SS_EMPTY) || (cur_status[SLOT0] == SS_BLANK))
		{
			copy_slot_metadata(0, 1);// 1 to 0
		}
		//  check slot status and copy slot0 data to slot1
		if((cur_status[SLOT1] == SS_EMPTY) || (cur_status[SLOT1] == SS_BLANK))
		{
			copy_slot_metadata(1, 0); // 0 to 1
		}
	}
}
#endif

static u8 same_array(void)
{
	// id_compare_value is the unsigned char variable
	id_compare_value = 0;
	//id_compare_value = 0 all id are the same,;1,one of id is the same;2 ,the two are different
	id_compare_value += xmemcmp((u8 *)&ram_rp[0].disk[0].disk_bus_type,(u8 *)&ram_rp[1].disk[0].disk_bus_type, 61);
	id_compare_value += xmemcmp((u8 *)&ram_rp[0].disk[1].disk_bus_type,(u8 *)&ram_rp[1].disk[1].disk_bus_type, 61);

	MSG("id cmp %bx\n", id_compare_value);
	//if id_compare_value == 0 it is the same data,or it is different
	if ((id_compare_value != 0) ||
		(ram_rp[0].array_mode != ram_rp[1].array_mode) ||	
		(ram_rp[0].array_size != ram_rp[1].array_size) ||
#ifdef WDC_RAID  // ini raid does NOT support setting stripe size now
		(ram_rp[0].stripe_size != ram_rp[1].stripe_size) ||
#endif		
		(ram_rp[0].disk_num == ram_rp[1].disk_num))
	{
		MSG("diff array\n");
		
		return 0; // false,the array is different
	}
	else
	{
		if (ram_rp[0].array_mode == RAID1_MIRROR)
			return 1;//array same,return ok
		else if (ram_rp[0].array_mode == JBOD) // JBOD?
			return 0;// if it is JBOD,rerurn false
		else if (ram_rp[0].raidGenNum != ram_rp[1].raidGenNum)
		{
			MSG("diff gen\n");
			return 0;//rerurn false
		}
	}
	return 1;//array same,return ok
}

static u8 can_be_same_array(void)
{
	u64 tmp64 = 0;
	if ((cur_status[SLOT0] == SS_EMPTY) || (cur_status[SLOT1] == SS_EMPTY))
	{
		return 1; //return true
	}
	
	if ((ram_rp[0].array_mode == RAID1_MIRROR) 
		&& (cur_status[SLOT0] == SS_GOOD)
		&& (cur_status[SLOT1] == SS_BLANK))
	{
		tmp64 = (sata1Obj.sectorLBA & 0xFFFFFFFFFFFFF800);
		if ((product_detail.options & PO_NO_CDROM) == 0)
		{
			tmp64 -= INI_VCD_SIZE;
		}
#ifndef WDC_RAID
		tmp64 -= INI_PRIVATE_SIZE;
#endif
		//tmp64 += 1;
		
		if (tmp64 >= ram_rp[0].array_size)
		{
			return 1; //return true
		}
		else
		{
			set_stat(SLOT1, SS_SOFT_REJECTED);
			MSG("small 1 F\n");
			return 0; //return false
		}
	}
	else if ((ram_rp[1].array_mode == RAID1_MIRROR) 
		&& (cur_status[SLOT0] == SS_BLANK)
		&& (cur_status[SLOT1] == SS_GOOD))
	{
		tmp64 = (sata0Obj.sectorLBA & 0xFFFFFFFFFFFFF800);
		if ((product_detail.options & PO_NO_CDROM) == 0)
		{
			tmp64 -= INI_VCD_SIZE;
		}
#ifndef WDC_RAID
		tmp64 -= INI_PRIVATE_SIZE;
#endif
		
		//tmp64 += 1;
		
		if (tmp64 >= ram_rp[1].array_size)
		{
			ram_rp[0].array_mode = RAID1_MIRROR;	// ram_rp[0] maybe not config, and we need it to judge mode in later
			return 1; //return true
		}
		else
		{
			set_stat(SLOT0, SS_SOFT_REJECTED);
			MSG("small 0 F\n");
			return 0;  //return false
		}
	}
	else if ((ram_rp[0].array_mode == NOT_CONFIGURED)
		&& (ram_rp[1].array_mode == NOT_CONFIGURED))
	{
		// return true
		MSG("bNEW\n");
		return 1;
	}
	else
	{
		// return false
		MSG("can't b S\n");
		return 0;
	}
}

#if 0
static void save_raidParams(u8 slot_value, u8 status0, u8 status1, u8 slot0_disk, u8 slot1_disk)
{
	ram_rp[slot_value].disk[slot0_disk].disk_status = status0;
	ram_rp[slot_value].disk[slot1_disk].disk_status = status1;  
	ram_rp[slot_value].disk[slot0_disk].disk_bus_type = 'a';
	ram_rp[slot_value].disk[slot1_disk].disk_bus_type = 'a';  
	if (slot_value == 1)
	{
		ram_rp[slot_value].disk_num = slot1_disk;
	}
	else
	{
		ram_rp[slot_value].disk_num = slot0_disk;
	}
	
	need_to_store_slot[slot_value] = 1;

	// copy slot0.id to ram_rp[slot_value].disk[slot0_disk]
	// copy slot1.id to ram_rp[slot_value].disk[slot1_disk]
	xmemcpy((u8 *)&sata0Obj.modelNum[0], (u8 *)&ram_rp[slot_value].disk[slot0_disk].disk_model_num[0], 60);
	xmemcpy((u8 *)&sata1Obj.modelNum[0], (u8 *)&ram_rp[slot_value].disk[slot1_disk].disk_model_num[0], 60);
}

static void update_slots(u8 status0, u8 status1) 
{ 
	u8 slot0_disk = get_disk_num(0, status0);
	u8 slot1_disk = get_disk_num(1, status1);
	if (status0 != SS_MISSING) 
	{ 
		save_raidParams(0, status0, status1, slot0_disk, slot1_disk);
	} 	
	
	if (status1 != SS_MISSING) 
	{
		save_raidParams(1, status0, status1, slot0_disk, slot1_disk);
	}
	cur_status[SLOT0] = status0;
	cur_status[SLOT1] = status1;
}
#endif

/*
static void update_slot_for_rebuild_done()
{ 
	// Derive status values and disk numbers 
	u8 status0 = (good_slot == 0) ? SS_GOOD : SS_REBUILDING;
	u8 status1 = (good_slot == 1) ? SS_GOOD : SS_REBUILDING; 
	u8 slot0_disk = get_disk_num(0, status0); 
	u8 slot1_disk = get_disk_num(1, status1); 
	u8 new_disk = (new_slot == 0) ? slot0_disk : slot1_disk; 

	// Update the good slot first 
	ram_rp[good_slot].disk[slot0_disk].disk_status = status0; 
	ram_rp[good_slot].disk[slot1_disk].disk_status = status1; 
	//strcpy(ram_rp[good_slot].disk[slot0_disk].id, slot0.id)
	ram_rp[good_slot].disk[slot0_disk].disk_bus_type = 'a';
	xmemcpy((u8 *)&sata0Obj.modelNum[0], (u8 *)&ram_rp[good_slot].disk[slot0_disk].disk_model_num[0], 60);
	ram_rp[good_slot].disk[slot0_disk].NT_disk_id = 0x00;
	//strcpy(ram_rp[good_slot].disk[slot1_disk].id, slot1.id) 
	ram_rp[good_slot].disk[slot1_disk].disk_bus_type = 'a';
	xmemcpy((u8 *)&sata1Obj.modelNum[0], (u8 *)&ram_rp[good_slot].disk[slot1_disk].disk_model_num[0], 60);
	ram_rp[good_slot].disk[slot1_disk].NT_disk_id = 0x00;
//Please note that this function demonstrates the option to treat the ID strings as one long ASCII string 
//since the bus type, model number, and serial number are stored next to each other and a single null byte 
//follows the serial number field

	// Update the new slot last 
	copy_slot_metadata(new_slot, good_slot); 
	ram_rp[new_slot].disk[slot0_disk].disk_status = status0;              
	ram_rp[new_slot].disk[slot1_disk].disk_status = status1; 
	ram_rp[new_slot].disk_num = new_disk; 
	cur_status[new_slot] = SS_GOOD; 

	// Every slot's metadata needs to be written 
	need_to_store_slot[0] = 1; 
	need_to_store_slot[1] = 1; 
}*/

static void update_slots_for_rebuilding(u8 good_slot, u8 new_slot) 
{ 
	// Derive status values and disk numbers 
	u8 status0 = (good_slot == 0) ? SS_GOOD : SS_REBUILDING;
	u8 status1 = (good_slot == 1) ? SS_GOOD : SS_REBUILDING; 
	u8 slot0_disk = get_disk_num(0, status0); 
	u8 slot1_disk = get_disk_num(1, status1); 
	u8 new_disk = (new_slot == 0) ? slot0_disk : slot1_disk; 

	// Update the good slot first 
	ram_rp[good_slot].disk[slot0_disk].disk_status = status0; 
	ram_rp[good_slot].disk[slot1_disk].disk_status = status1; 
	//strcpy(ram_rp[good_slot].disk[slot0_disk].id, slot0.id)
	ram_rp[good_slot].disk[slot0_disk].disk_bus_type = 'a';
	xmemcpy((u8 *)&sata0Obj.modelNum[0], (u8 *)&ram_rp[good_slot].disk[slot0_disk].disk_model_num[0], 60);
	ram_rp[good_slot].disk[slot0_disk].NT_disk_id = 0x00;
	//strcpy(ram_rp[good_slot].disk[slot1_disk].id, slot1.id) 
	ram_rp[good_slot].disk[slot1_disk].disk_bus_type = 'a';
	xmemcpy((u8 *)&sata1Obj.modelNum[0], (u8 *)&ram_rp[good_slot].disk[slot1_disk].disk_model_num[0], 60);
	ram_rp[good_slot].disk[slot1_disk].NT_disk_id = 0x00;

	if (product_detail.options & PO_NO_CDROM)
	{
		ram_rp[good_slot].vcd_size = 0;
	}
	else
	{
		ram_rp[good_slot].vcd_size = INI_VCD_SIZE;
	}
#ifdef WDC_RAID
	ram_rp[good_slot].config_VID[0] = (u8)(product_detail.USB_VID >> 8);
	ram_rp[good_slot].config_VID[1] = (u8)product_detail.USB_VID;
	ram_rp[good_slot].config_PID[0] = (u8)(product_detail.USB_PID >> 8);
	ram_rp[good_slot].config_PID[1] = (u8)product_detail.USB_PID;
#else
	ram_rp[good_slot].config_VID[0] = (u8)globalNvram.USB_VID[0];
	ram_rp[good_slot].config_VID[1] = (u8)globalNvram.USB_VID[1];
	ram_rp[good_slot].config_PID[0] = (u8)globalNvram.USB_PID[0];
	ram_rp[good_slot].config_PID[1] = (u8)globalNvram.USB_PID[1];
#endif

//Please note that this function demonstrates the option to treat the ID strings as one long ASCII string 
//since the bus type, model number, and serial number are stored next to each other and a single null byte 
//follows the serial number field

	// Update the new slot last 
	copy_slot_metadata(new_slot, good_slot); 
	ram_rp[new_slot].disk[slot0_disk].disk_status = status0;              
	ram_rp[new_slot].disk[slot1_disk].disk_status = status1; 
	ram_rp[new_slot].disk_num = new_disk; 
	cur_status[new_slot] = SS_REBUILDING; 

	// Every slot's metadata needs to be written 
	need_to_store_slot[0] = 1; 
	need_to_store_slot[1] = 1; 
}
		
static void update_slots_for_missing(u8 good_slot, u8 missing_slot) 
{ 
	// Derive status values and disk numbers 
	u8 status0 = (good_slot == 0) ? SS_GOOD : SS_MISSING; 
	u8 status1 = (good_slot == 1) ? SS_GOOD : SS_MISSING; 
	u8 slot0_disk = get_disk_num(0, status0); 
	u8 slot1_disk = get_disk_num(1, status1); 

	// Update only the good slot 
	ram_rp[good_slot].disk[slot0_disk].disk_status = status0; 
	ram_rp[good_slot].disk[slot1_disk].disk_status = status1; 
	need_to_store_slot[good_slot] = 1; 
}

static void update_slots_for_failed(u8 good_slot, u8 failed_slot) 
{ 
	// Derive status values and disk numbers 
	u8 status0 = (good_slot == 0) ? SS_GOOD : SS_FAILED;
	u8 status1 = (good_slot == 1) ? SS_GOOD : SS_FAILED; 
	u8 slot0_disk = get_disk_num(0, status0); 
	u8 slot1_disk = get_disk_num(1, status1); 

	// Update the good slot's metadata 
	ram_rp[good_slot].disk[slot0_disk].disk_status = status0; 
	ram_rp[good_slot].disk[slot1_disk].disk_status = status1; 

	// Update the failed slot's metadata 
	ram_rp[failed_slot].disk[slot0_disk].disk_status = status0;              
	ram_rp[failed_slot].disk[slot1_disk].disk_status = status1; 
	cur_status[failed_slot] = SS_FAILED; 

	// Every slot's metadata needs to be written 
	need_to_store_slot[0] = 1; 
	need_to_store_slot[1] = 1; 
}

static void append_array_status(u8 newStatus, u8 raidMode, u8 status0, u8 status1) 
{ 
	//If the num_arrays variable is less than two then  the slots status to be used for reporting are recorded
	// and the new array status is recorded in the queue 
	//and the num_array variable is incremented; otherwise the new array status is ignored. 

	u8 slot0_priority = report_priority[status0]; 
   	u8 slot1_priority = report_priority[status1];

	if (num_arrays < 2) 
	{
		// If not NotConfig then Empty slot statuses needs to be 
        	// converted into either NotPart or Missing depending on 
        	// the RAID mode. 
		if (newStatus != AS_NOT_CONFIGURED) 
		{ 
			if (raidMode == JBOD) 
			{ 
				status0 = (status0 == SS_EMPTY) ? SS_NOT_PARTICIPATING: status0; 
				status1 = (status1 == SS_EMPTY) ? SS_NOT_PARTICIPATING : status1; 
			} 
			else 
			{ 
				status0 = (status0 == SS_EMPTY) ? SS_MISSING: status0; 
				status1 = (status1 == SS_EMPTY) ? SS_MISSING : status1; 
			} 
		} 
        	report_status[num_arrays][0] = status0; 
        	report_status[num_arrays][1] = status1; 
		report_slot[num_arrays] = (slot0_priority < slot1_priority) ? 0 : 1;

		MSG("report n %bx p %bx s0 %bx s1 %bx\n", num_arrays, report_slot[num_arrays], report_status[num_arrays][0], report_status[num_arrays][1]);
		array_status_queue[num_arrays++] = newStatus;
    	} 
}

static void raid1_degraded(u8 good_slot, u8 other_status)
{
	MSG("Degra: %bx %bx\n", good_slot, other_status);
	u8 other_slot = (good_slot == 0) ? 1 : 0;
	u8 bad_disk = get_disk_num(other_slot, other_status);
	u8 status0 = (good_slot == 0) ? SS_GOOD : other_status;
	u8 status1 = (good_slot == 1) ? SS_GOOD : other_status;

	if ((other_status == SS_GOOD) || (other_status == SS_REBUILDING) || 
		(other_status == SS_BLANK) || (other_status == SS_FAILED))
	{
		// If the other_status is Good, Rebuilding, Blank, or Failed then metadata on the good_slot does not 
		// need to be altered. The WD application will present some useful information to the end-user so 
		// they can determine how they want to proceed.
		append_array_status(AS_DEGRADED, RAID1_MIRROR, status0, status1);
	}
	else if (ram_rp[good_slot].disk[bad_disk].disk_status != SS_MISSING)
	{
		// If the other_status is not Good, Rebuilding, Blank or Failed and the good_slot's metadata does 
		// not currently record that the partner drive status is Missing then we need to update the metadata 
		// to record that now.
		append_array_status(AS_DEGRADED, RAID1_MIRROR, status0, status1);
		update_slots_for_missing(good_slot, other_slot);
	}
	else
	{
		append_array_status(AS_DEGRADED, RAID1_MIRROR, status0, status1);
	}
}

static void raid1_might_be_auto_rebuild(u8 good_slot, u8 other_status)
{
#ifdef DBG_FUNCTION
	if (ram_rp[good_slot].raidGenNum < ram_rp[(!good_slot)].raidGenNum)
		MSG("ASSERT W\n");
	MSG("might be auto %bx\n", (!good_slot));
#endif	
	
	u8 status0 = (good_slot == 0) ? SS_GOOD : other_status;
	u8 status1 = (good_slot == 1) ? SS_GOOD : other_status;

	if ((enable_auto_rebuild) && 
		((other_status == SS_GOOD) || (other_status == SS_REBUILDING) || (other_status == SS_BLANK)))
	{
		u8 other_slot = (good_slot == 0) ? 1 : 0;
		append_array_status(AS_REBUILDING, RAID1_MIRROR, status0, status1);
		update_slots_for_rebuilding(good_slot, other_slot);
		rebuildRetryCnt = 0;
		rebuildLBA = 0;
		raid_mirror_status = RAID1_STATUS_REBUILD;
		raid_rb_state = RAID_RB_IDLE;
#ifdef SAVE_RAID_IN_FLASH_ONLY
		newer_disk = 0;			// clear newr_disk status for raid1->jbod doesn't erase hdd
#endif						
		raid_mirror_operation = (good_slot == 0) ? RAID1_REBUILD0_1 : RAID1_REBUILD1_0;
		other_Params[0].raid_rebuildCheckPoint = REBUILD_METADATA_UPDATE_THRESHOLD;
		other_Params[1].raid_rebuildCheckPoint = REBUILD_METADATA_UPDATE_THRESHOLD;
		Write_Disk_MetaData(0, WD_OTHER_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP);
		Write_Disk_MetaData(1, WD_OTHER_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP);
	}
	else
	{
		raid1_degraded(good_slot, other_status);
	}

}

static void raid1_resume_rebuild(u8 good_slot)
{
	u8 other_slot = (good_slot == 0) ? 1 : 0;
	u8 good_gen = ram_rp[good_slot].raidGenNum;
	u8 other_gen = ram_rp[other_slot].raidGenNum;
	u8 status0 = (good_slot == 0) ? SS_GOOD : SS_REBUILDING;
	u8 status1 = (good_slot == 1) ? SS_GOOD : SS_REBUILDING;

	MSG("rsm\n");
	if (good_gen == other_gen)
	{
		append_array_status(AS_REBUILDING, RAID1_MIRROR, status0, status1);
		rebuildRetryCnt = 0;
#ifdef DBG_FUNCTION
		if (other_Params[good_slot].raid_rebuildCheckPoint == 0)
		{
			MSG("ERRRRR\n");
		}
#endif		
		rebuildLBA = other_Params[good_slot].raid_rebuildCheckPoint - REBUILD_METADATA_UPDATE_THRESHOLD;
		raid_mirror_status = RAID1_STATUS_REBUILD;
		raid_rb_state = RAID_RB_IDLE;
		raid_mirror_operation = (good_slot == 0) ? RAID1_REBUILD0_1 : RAID1_REBUILD1_0;
	}
	// Not sure how a Rebuilding could get a bigger gen count, but we will trust the Good slot in this case
	else if (good_gen > other_gen) 
	{
		raid1_might_be_auto_rebuild(good_slot, SS_REBUILDING);
	}
	else 
	{
		raid1_degraded(good_slot, SS_REBUILDING);
	}
}

static void raid1_both_are_good()
{
	if (ram_rp[0].raidGenNum == ram_rp[1].raidGenNum)
	{
		if ((ram_rp[0].disk[0].disk_status == SS_GOOD)
			&& (ram_rp[0].disk[1].disk_status == SS_GOOD)
			&& (ram_rp[1].disk[0].disk_status == SS_GOOD)
			&& (ram_rp[1].disk[1].disk_status == SS_GOOD))
		{
			append_array_status(AS_GOOD, RAID1_MIRROR, SS_GOOD, SS_GOOD);
		}
		else
		{
			append_array_status(AS_BROKEN, RAID1_MIRROR, SS_GOOD, SS_GOOD);
		}
	}
	else if (ram_rp[0].raidGenNum > ram_rp[1].raidGenNum)
	{
		raid1_might_be_auto_rebuild(0, SS_GOOD);
	}
	else
	{
		raid1_might_be_auto_rebuild(1, SS_GOOD);
	}
}

static void raid1_at_least_one_good(u8 good_slot, u8 other_status)
{
	if (other_status == SS_GOOD)
	{
		raid1_both_are_good();
	}
	else if (other_status == SS_REBUILDING)
	{
		raid1_resume_rebuild(good_slot);
	}
	else if (other_status == SS_BLANK)
	{
		raid1_might_be_auto_rebuild(good_slot, other_status);
	}
	else
	{
		raid1_degraded(good_slot, other_status);
	}
}

static void set_raid1_array_status(u8 status0, u8 status1)
{ 
	if (status0 == SS_GOOD)
	{
		raid1_at_least_one_good(0, status1);
	}
	else if (status1 == SS_GOOD)
	{
		raid1_at_least_one_good(1, status0);
	}
	else
	{
		//If none of the above applies then an array status of Broken is appended.
		append_array_status(AS_BROKEN, RAID1_MIRROR, status0, status1);
	}
}

#ifdef SUPPORT_RAID0
static void set_raid0_array_status(u8 status0, u8 status1)
{
	// If both slot statuses are Good then an array status of Good is appended
	// otherwise, an array status of Broken is appended.
	if((status0 == SS_GOOD) && (status1 == SS_GOOD))
		append_array_status(AS_GOOD, RAID0_STRIPING_DEV, status0, status1);
	else
		append_array_status(AS_BROKEN, RAID0_STRIPING_DEV, status0, status1);
}
#endif

#ifdef SUPPORT_SPAN
static void set_span_array_status(u8 status0, u8 status1)
{
	// If both slot statuses are Good then an array status of Good is appended
	// otherwise, an array status of Broken is appended.
	if((status0 == SS_GOOD) && (status1 == SS_GOOD))
		append_array_status(AS_GOOD, SPANNING, status0, status1);
	else
		append_array_status(AS_BROKEN, SPANNING, status0, status1);
}
#endif

static void set_jbod_array_status(u8 status0, u8 status1)
{
	if (((status0 == SS_GOOD) && ((status1 == SS_NOT_PARTICIPATING) ||(status1 == SS_EMPTY))) ||
		((status1 == SS_GOOD) && ((status0 == SS_NOT_PARTICIPATING) ||(status0 == SS_EMPTY))))
		append_array_status(AS_GOOD, JBOD, status0, status1);
	else
		append_array_status(AS_BROKEN, JBOD, status0, status1);
}

static void set_array_status(u8 raidMode, u8 status0, u8 status1)
{
	if (raidMode == JBOD)
	{
		// JBOD
		set_jbod_array_status(status0,status1);
	}
#ifdef SUPPORT_RAID1
	else if (raidMode == RAID1_MIRROR)
	{
		// RAID 1
		set_raid1_array_status(status0,status1);
	}
#endif
#ifdef SUPPORT_RAID0    
	else if (raidMode == RAID0_STRIPING_DEV)
	{
		// RAID 0
		set_raid0_array_status(status0,status1);
	}
#endif    
#ifdef SUPPORT_SPAN
	else if (raidMode == SPANNING)
	{
		// SPAN
		set_span_array_status(status0,status1);
	}
#endif
	else
	{
		// NOT CONFIGURED
		append_array_status(AS_NOT_CONFIGURED, raidMode, status0, status1);
	}
	
}

static void determine_array_status(void)
{
	num_arrays = 0;

	MSG("rrpam %bx %bx\n", ram_rp[0].array_mode, ram_rp[1].array_mode);
	//compare whether or not the slots represent the same array?
	if (same_array())
	{
		// same array
		MSG("same\n");
		set_array_status(ram_rp[0].array_mode, cur_status[SLOT0], cur_status[SLOT1]);
	}
	else if (can_be_same_array())
	{
		MSG("trys\n");
		u8 l_array_mode = ram_rp[0].array_mode;
		if (cur_status[SLOT0] == SS_EMPTY)
		{// drive in slot 0 is removed
			l_array_mode = ram_rp[1].array_mode;
		}		
		set_array_status(l_array_mode, cur_status[SLOT0], cur_status[SLOT1]);
	}
	else
	{
		// not the same array
		set_array_status(ram_rp[0].array_mode, cur_status[SLOT0], SS_EMPTY);
		set_array_status(ram_rp[1].array_mode, SS_EMPTY, cur_status[SLOT1]);
	}
}

static void determine_metadata_status(void)
{
	if(num_arrays == 1)
	{
		if(array_status_queue[0] == AS_NOT_CONFIGURED)
			metadata_status = MS_NO_VALID;
		else if(array_status_queue[0] == AS_GOOD)
			metadata_status = MS_GOOD;
		else
			metadata_status = MS_STH_NOT_VALID;
	}
	else
	{
		//id_compare_value = 0 all id are the same,;1,one of id is the same;2 ,the two are different
		if((array_status_queue[0] == AS_GOOD) && (array_status_queue[1] == AS_GOOD))
			metadata_status = MS_GOOD;
		else if ((id_compare_value > 0) && (id_compare_value < 2))
			metadata_status = MS_CONFLICT_ARRAYS;
		else
			metadata_status = MS_STH_NOT_VALID;
	}
}

static void store_meta_data(u8 slot_num)
{
	u16 temp_size = sizeof(RAID_PARMS);

	if((need_to_store_slot[slot_num] == 1) &&
		((metadata_status == MS_GOOD) || (metadata_status == MS_STH_NOT_VALID)))
	{
#ifdef DBG_FUNCTION
		if (slot_num == 0)
			MSG("rrp0 %bx %bx %bx %bx\n", ram_rp[0].array_mode, ram_rp[0].disk_num, ram_rp[0].disk[0].disk_status, ram_rp[0].disk[1].disk_status);
		else
			MSG("rrp1 %bx %bx %bx %bx\n", ram_rp[1].array_mode, ram_rp[1].disk_num, ram_rp[1].disk[0].disk_status, ram_rp[1].disk[1].disk_status);
#endif
		ram_rp[slot_num].raidGenNum++;
		ram_rp[slot_num].header.signature = WDC_RAID_PARAM_SIG;
		ram_rp[slot_num].header.payload_len = temp_size-sizeof(METADATA_HEADER);
		ram_rp[slot_num].header.payload_crc = create_Crc16((u8 *)&(ram_rp[slot_num].vcd_size), ram_rp[slot_num].header.payload_len);
		ram_rp[slot_num].header.header_crc = create_Crc16((u8 *)(&ram_rp[slot_num]), sizeof(METADATA_HEADER) - 2);

#ifndef SAVE_RAID_IN_FLASH_ONLY
		//Write 1st Copy & 2nd Copy on HDD[slot_num] and Flush
		u8 rc = 0;
		if (rc = Write_Disk_MetaData(slot_num,WD_RAID_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP))
		{
			MSG("F! hddStor: %bx\n", rc);
		}
#endif
				
		//Write 3rd Copy in NVM[slot_to_nvm[slot_num]]
		spi_read_flash(mc_buffer, WDC_RAID_PARAM_ADDR, RAID_VITAL_TOTAL_SIZE);
		// find right position to save ram_rp in flash according to the map of slot_to_nvm
		u16 addr = (slot_to_nvm[slot_num] == 0) ? 0 : sizeof(RAID_PARMS);
		xmemcpy((u8 *)(&ram_rp[slot_to_nvm[slot_num]]), (u8 *)&mc_buffer[addr], sizeof(RAID_PARMS));
		spi_write_flash(mc_buffer, WDC_RAID_PARAM_ADDR, RAID_VITAL_TOTAL_SIZE);	

		need_to_store_slot[slot_num] = 0;
	}
}

static void store_slots(void)
{
	MSG("sav MD %bx %bx\n", cur_status[0], cur_status[1]);
	if (cur_status[SLOT0] == SS_GOOD)
	{
		store_meta_data(SLOT0);
		store_meta_data(SLOT1);
	}
	else
	{
		store_meta_data(SLOT1);
		store_meta_data(SLOT0);
	}
}

void update_raid1_read_use_slot(void)
{
	raid1_use_slot = 0;
	// arrayMode is raid1, so there is one vol only
	if ((arrayMode == RAID1_MIRROR) && (array_status_queue[0] != AS_GOOD))
	{
		if ((cur_status[SLOT0] | cur_status[SLOT1]) == SS_GOOD) // Both Good, but degrade
		{
			raid1_use_slot = (ram_rp[0].raidGenNum > ram_rp[1].raidGenNum) ? 0 : 1;
		}
		else
		{
			raid1_use_slot = (cur_status[SLOT0] == SS_GOOD) ? 0 : 1;
		}
	}
	MSG("r1slot %bx\n", raid1_use_slot);
}

static void config_disk_arrays(void)
{
	
	//Config 1st Disk LUN
	num_DiskLuns_created = 1;

	if (num_arrays == 2)
	{
		//Two Array
		//Config 2nd Disk LUN
		num_DiskLuns_created++;
	}

	// Notify SCSI
	u8 n = (cur_status[SLOT0] == SS_GOOD) ? 0 : 1;
		// s1( ,B) n2 r1 s0 empty(2) s1 good(0) a0 notC(10) a1 deg(1) ms SNV(2)
		// s1(A, ) n2 r1 s0 good(0) s1 empty(2) a0 deg(1) a1 notC(10) ms SNV(2)
		// s2(A,B) n1 r1 s0 good(0) s1 good(0) a0 deg(1) a1 notC(10) ms SNV(2)
	if (num_arrays == 1)
	{
		if (metadata_status == MS_GOOD)
		{// both arrays good
			// JBOD, slot1 only, use ram_rp[1]
			arrayMode = ram_rp[n].array_mode;
		}
		else if (metadata_status == MS_STH_NOT_VALID)
		{// one array is not good
		 	// RAID1 has one channel degraded or rebuilding
		 	if ((array_status_queue[0] == AS_REBUILDING) 
				|| (array_status_queue[0] == AS_DEGRADED))
		 	{
		 		if ((cur_status[SLOT0] == SS_EMPTY) || (cur_status[SLOT1] == SS_EMPTY))
				{
					arrayMode = JBOD;
				}
				else
				{
					arrayMode = ram_rp[n].array_mode;
				}
		 	}
			else
			{
				arrayMode = JBOD;
			}
		}
//#ifdef HDR2526_GPIO_HIGH_PRIORITY
		raid_mode = ram_rp[n].array_mode;;
//#endif
		
	}
	else
	{
		arrayMode = JBOD;
//#ifdef HDR2526_GPIO_HIGH_PRIORITY
		raid_mode = ram_rp[n].array_mode;
//#endif
		
	}

	update_raid1_read_use_slot();
}

#ifdef HDR2526
#if 1//def HDR2526_GPIO_HIGH_PRIORITY
u8 hard_raid_mapping()
{
	u8 hw_raid_gpio  = (((*gpioDataIn & GPIO5_PIN) >> 4) | ((*gpioDataIn & GPIO6_PIN) >> 6));
    
#ifdef DK101_IT1
        hw_raid_gpio = GPIO_JBOD;
#endif

	MSG("hrm %bx\n", hw_raid_gpio);

	switch (hw_raid_gpio)
	{
	
        case GPIO_RAID0:
#ifdef SUPPORT_RAID0
            //arrayMode = RAID0_STRIPING_DEV;
            return RAID0_STRIPING_DEV;
#endif
        case GPIO_RAID1:
#ifdef SUPPORT_RAID1
            //arrayMode = RAID1_MIRROR;
            return RAID1_MIRROR;
#endif
		case GPIO_JBOD:
			//arrayMode = JBOD;
			return JBOD;
		case GPIO_SPAN:
#ifdef SUPPORT_SPAN
			//arrayMode = SPANING;
			return SPANNING;
#endif			
		default: 
			return 0xFF;
	}

	return 0xFF;
}

#define POWER_ON_ERROR			0x00
#define POWER_ON_DO_NOTHING		0x01
#define POWER_ON_RE_CONFIG		0x02
u8 hdr2526_raid_power_on_config()
{
	u8 gpio_raid = hard_raid_mapping();
	//u8 n = (cur_status[SLOT0] == SS_GOOD) ? 0 : 1;
	u8 n;
	if (cur_status[SLOT0] == SS_GOOD)
		n = 0;
	else if (cur_status[SLOT1] == SS_GOOD)
		n = 1;
	else  // no good, hr just have SS_GOOD/SS_REBUILDING/SS_BLANK
	{
		n = (cur_status[SLOT0] == SS_REBUILDING) ? 0 : 1;
		MSG("pow on no good gpior %bx curraid %bx %bx\n", gpio_raid, ram_rp[0].array_mode, ram_rp[1].array_mode);
		MSG("slots status %bx, %bx\n", cur_status[SLOT0], cur_status[SLOT1]);
		goto _reconfig;
	}

	MSG("pow on gpioraid %bx curraid %bx %bx\n", gpio_raid, ram_rp[0].array_mode, ram_rp[1].array_mode);
	MSG("slots status %bx, %bx\n", cur_status[SLOT0], cur_status[SLOT1]);
	MSG("mtdata status %bx, arrayQue state %bx, %bx\n", metadata_status, array_status_queue[0], array_status_queue[1]);

	if (gpio_raid == 0xFF)
	{
		return POWER_ON_DO_NOTHING;
	}
	
	if (ram_rp[n].array_mode != gpio_raid)
	//if (nvm_rp[n].array_mode != gpio_raid)
	{
		MSG("gpioChange  ");
_reconfig:
		if (hw_raid_config(0))
			return POWER_ON_DO_NOTHING;

		MSG("RE-COFIG RAID\n");
		return POWER_ON_RE_CONFIG;
	}
	else
	{
		if (metadata_status == MS_GOOD)
		{
			MSG("normal\n");
			return POWER_ON_DO_NOTHING;
		}
		else if (metadata_status == MS_STH_NOT_VALID)
		{
			if (gpio_raid == RAID1_MIRROR)
			{
				MSG("snr1 ");
				return POWER_ON_DO_NOTHING;
			}
			else if (gpio_raid == JBOD)
			{
				MSG("snvJ ");
				goto _reconfig;
			}
			else
			{
				MSG("SNV  ");
				goto _reconfig;
			}
		}
		else if (metadata_status == MS_CONFLICT_ARRAYS)
		{
			MSG("conflict??\n");
			goto _reconfig;
			//return POWER_ON_DO_NOTHING;
		}
		else if (metadata_status == MS_NO_VALID)
		{
			MSG("never here\n");
			return POWER_ON_ERROR;
		}
	}
	

	return POWER_ON_ERROR;
}
#endif
#endif
void raid_enumeration(u8 load_flag)
{	
	if (load_flag)  // it must be 0 when set raid config
	{
		read_nvm_raid_parms();
		populate_slot_mapping();
	}
	need_to_store_slot[0] = 0;
	need_to_store_slot[1] = 0;

	determine_slot_status(0);
	determine_slot_status(1);
	MSG("tmpSS: %bx %bx\n", cur_status[SLOT0], cur_status[SLOT1]);
_determine_again:
	determine_array_status();
	MSG("dn %bx %bx\n", ram_rp[0].disk_num, ram_rp[1].disk_num);
	determine_metadata_status();

#if 1//def HDR2526_GPIO_HIGH_PRIORITY
	if (hdr2526_raid_power_on_config() == POWER_ON_RE_CONFIG)
		goto _determine_again;
#endif

	if ((need_to_store_slot[0]) || (need_to_store_slot[1]))
	{
		store_slots();
	}
	
	MSG("slots status %bx, %bx\n", cur_status[SLOT0], cur_status[SLOT1]);
	MSG("mtdata status %bx, arrayQue state %bx, %bx\n", metadata_status, array_status_queue[0], array_status_queue[1]);

	config_disk_arrays();
	MSG("num %bx, mode %bx\n", num_arrays, arrayMode); 
	tickle_fault();
}

void update_array_data_area_size(void)
{
	// case1, 1 vol, 2 slot or 1 slot, queue idx will always be 0
	// case2, 2 vol, arrayMode is JBOD, we don't care array_size now, we will show hdd capacity for conflict and sth_not_valid

	u8 good_slot = (cur_status[SLOT0] == SS_GOOD) ? 0 : 1;

	if ((array_status_queue[0] == AS_GOOD) || (array_status_queue[0] == AS_DEGRADED))
	{
		array_data_area_size = ram_rp[good_slot].array_size;
	}
    else
    {
#ifndef DATASTORE_LED 
        array_data_area_size = Min(user_data_area_size[0], user_data_area_size[1]);
        if (arrayMode == RAID0_STRIPING_DEV)
        {
            array_data_area_size <<= 1;
        }
#ifdef SUPPORT_SPAN
        else if (arrayMode == SPANNING)
        {
            array_data_area_size = user_data_area_size[0] + user_data_area_size[1];
        }
#endif
#endif
    }
}

/************************************************************************\
read_other_parms() -
 Returns:

 */
u8 load_other_parms(void)
{
	u8 rc = 0;
	
	MSG("LD otherPam\n");
	for(u8 i = 0;i < (MAX_SATA_CH_NUM);i++)	// simple way, there is no restore now
	{
		if (find_good_copy(i, WD_OTHER_PARAM_PAGE))
		{
			MSG("rbCKP %lx %lx\n", (u32)(other_Params[i].raid_rebuildCheckPoint >> 32), (u32)other_Params[i].raid_rebuildCheckPoint);
			continue;
		}
		else
		{
			// reset a new other param
			xmemset((u8 *)&other_Params[i], 0, sizeof(OTHER_PARMS));
			
			other_Params[i].header.signature = WDC_OTHER_PARAM_SIG;
			other_Params[i].header.payload_len = sizeof(OTHER_PARMS) - sizeof(METADATA_HEADER);
			other_Params[i].raid_rebuildCheckPoint = 0;

			if (nvm_unit_parms.mode_save.cd_valid_cd_eject & OPS_CD_MEDIA_VALID)
				other_Params[i].vcd_media = 0x02;
		
			Write_Disk_MetaData(i, WD_OTHER_PARAM_PAGE, FIRST_BACKUP|SECOND_BACKUP);
		}
	}

	return rc;
}
/****************************
initialize Raid params & do the raid enum,
initialize secuirity state
initialize vital params and other params
*****************************/
void ini_init(void)
{
    //The Other Parameters metadata shall be read and validated right before RAID Enumeration
    load_other_parms();

    // raid enumerate
    raid_enumeration(1);

    MSG("arrays num %bx, mode %bx\n", num_arrays, arrayMode); // the reasonable valure shall be 1 or 2
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
        // initialize raid mode
        raid_init();
    }
#endif
    //init security params:vital params & security init
    // read user params 
    init_update_params();

#if FAN_CTRL_EXEC_TICKS
	//fan_ctrl_init(sata1Obj.sobj_medium_rotation_rate, sata1Obj.sobj_medium_rotation_rate, 1, 1);
	fan_ctrl_init();
	enableFan(1);
#endif
	
}

void fill_volDesc(PVOL_DESC pTmpVolDesc, u8 lun)
{
	//u8 disk_num = ram_rp[lun].disk_num; // current disk's bay number
	u8 n = report_slot[lun];
	
	pTmpVolDesc->array_status_cmd = array_status_queue[lun];
	pTmpVolDesc->arrayMode = ram_rp[n].array_mode;
	if (pTmpVolDesc->arrayMode  == RAID0_STRIPING_DEV)
		pTmpVolDesc->stripe_size = ram_rp[n].stripe_size;	// 2 Stripe Size: 4K, 1<<(9+(3))
	else
		pTmpVolDesc->stripe_size = 0;
	
	if (array_status_queue[lun] == AS_REBUILDING)
	{// Rebuild Percentage Complete, how to avoid use mutiple
		pTmpVolDesc->rebuild_percentage_complete = (100*rebuildLBA)/ram_rp[n].array_size;
		//pTmpVolDesc->rebuild_percentage_complete = (100*other_Params[n].raid_rebuildCheckPoint)/ram_rp[n].array_size;
	}
	else
	{
		pTmpVolDesc->rebuild_percentage_complete = 0;
	}
	// 4:11 Start LBA 0x0
	pTmpVolDesc->start_lba = 0;
	// 12:19 Number of Blocks in 512Bytes
	WRITE_U64_BE(&(pTmpVolDesc->numOfBlocks), ram_rp[n].array_size);

#if 0 // both status are correct
	if (num_arrays > 1)
	{
		u8 m;
		if (lun == 0)
		{
			m = report_slot[1];
			pTmpVolDesc->slot0Status = report_status[lun][n];
			pTmpVolDesc->slot1Status = report_status[1][m];
		}
		else
		{
			m = report_slot[0];
			pTmpVolDesc->slot0Status = report_status[0][m];
			pTmpVolDesc->slot1Status = report_status[lun][n];
		}
	}
	else
#endif		
	{
		pTmpVolDesc->slot0Status = report_status[lun][0];
		pTmpVolDesc->slot1Status = report_status[lun][1];
	}
	pTmpVolDesc->slot0OrgOrder = 0;					// 21 Original Slot0 Order 
	pTmpVolDesc->slot1OrgOrder = 1;					// 23 Original Slot1 Order 		
}

void fill_raid_volume_descriptor(void)
{
	// we don't consider srcID here:
	RAID_STATUS_CONFIG_PARAM tmpRaidParam;
	xmemset((u8 *)&tmpRaidParam.signature, 0x00, sizeof(RAID_STATUS_CONFIG_PARAM));

#ifdef WDC_RAID
	tmpRaidParam.signature = 0x31;   // signature
#else
	tmpRaidParam.signature = 0x49;   // signature
#endif
	tmpRaidParam.metadataStatus= metadata_status;
	tmpRaidParam.numOfSlot = 0x02;	// Number of Slots
	tmpRaidParam.maxVolPerHDD = 0x01;	// Maximum Volumes per HDD
	tmpRaidParam.numOfVolSetDesc = num_arrays; // the Number of Volume Set Descriptors: only supports up to two volume sets 

	byteCnt = 8;
	fill_volDesc(&tmpRaidParam.volDesc[0], 0);	

	byteCnt += sizeof(VOL_DESC);
	if (num_arrays > 1)
	{// num_arrays can be greater than 1 when there's one drive is removed in JBOD
		 // 1. JBOD
		 // 2. Two drives are not same array 
		fill_volDesc(&tmpRaidParam.volDesc[1], 1);	
		byteCnt += sizeof(VOL_DESC);
	}

	xmemcpy((u8 *)&tmpRaidParam.signature, mc_buffer, byteCnt);
}

// one and only one of the slot descriptors shall have the SLOT STATUS field set to Rebuilding to signify that slot is the target
void raid_rebuild_initiated(u8 target)
{
	MSG("REBUILD initiated: %bx\n", target);
	raid_mirror_status = RAID1_STATUS_REBUILD;
	raid_rb_state = RAID_RB_IDLE;
	raid_mirror_operation = target;
	rebuildLBA = 0;
	rebuildRetryCnt = 0;
	array_status_queue[0] = AS_REBUILDING;

	if (target == RAID1_REBUILD1_0) // slot1 -> slot0
	{
		cur_status[SLOT0] = SS_REBUILDING;
		cur_status[SLOT1] = SS_GOOD;
		update_slots_for_rebuilding(1, 0);
	}
	else
	{
		cur_status[SLOT0] = SS_GOOD;
		cur_status[SLOT1] = SS_REBUILDING;
		update_slots_for_rebuilding(0, 1);
	}

	//update_slots(cur_status[SLOT0], cur_status[SLOT1]);
	other_Params[0].raid_rebuildCheckPoint = REBUILD_METADATA_UPDATE_THRESHOLD;
	other_Params[1].raid_rebuildCheckPoint = REBUILD_METADATA_UPDATE_THRESHOLD;
	Write_Disk_MetaData(0, WD_OTHER_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP);
	Write_Disk_MetaData(1, WD_OTHER_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP);
	store_slots();

	update_raid1_read_use_slot();
	tickle_fault();
	MSG("Done\n");
}

static u64 get_maximum_capacity(u8 slotNum)
{
	u64 slot_lba;
	if (slotNum)
	{
		pSataObj = &sata1Obj;
	}
	else
	{
		pSataObj = &sata0Obj;
	}
	
	if (pSataObj->sobj_device_state == SATA_NO_DEV)
		return 0;
	
	slot_lba = (pSataObj->sectorLBA & 0xFFFFFFFFFFFFF800);
	if ((product_detail.options & PO_NO_CDROM) == 0)
	{
		slot_lba -= INI_VCD_SIZE;
	}
#ifndef WDC_RAID
		slot_lba -= INI_PRIVATE_SIZE;
#endif

	return slot_lba;
}

static void fill_raid_parms(u8 slot_num, u8 array_mode, u8 stripe_size)
{

	if (product_detail.options & PO_NO_CDROM)
	{
		ram_rp[slot_num].vcd_size = 0;
	}
	else
	{
		ram_rp[slot_num].vcd_size = INI_VCD_SIZE;
	}
#ifdef WDC_RAID
	ram_rp[slot_num].config_VID[0] = (u8)(product_detail.USB_VID >> 8);
	ram_rp[slot_num].config_VID[1] = (u8)product_detail.USB_VID;
	ram_rp[slot_num].config_PID[0] = (u8)(product_detail.USB_PID >> 8);
	ram_rp[slot_num].config_PID[1] = (u8)product_detail.USB_PID;
#else
	ram_rp[slot_num].config_VID[0] = (u8)globalNvram.USB_VID[0];
	ram_rp[slot_num].config_VID[1] = (u8)globalNvram.USB_VID[1];
	ram_rp[slot_num].config_PID[0] = (u8)globalNvram.USB_PID[0];
	ram_rp[slot_num].config_PID[1] = (u8)globalNvram.USB_PID[1];
#endif
	ram_rp[slot_num].array_mode = array_mode;		// 1
	ram_rp[slot_num].stripe_size = stripe_size;		// 2
	ram_rp[slot_num].disk_num = slot_num;

	// check the array size: always input the maximum capacity
	u64 tmp_array_size = 0;
	u64 slot_lba[2];
	// here never get 0 capacity, because previous code has intercepted it
	slot_lba[0] = get_maximum_capacity(0);
	slot_lba[1] = get_maximum_capacity(1);
#ifndef DATASTORE_LED
	if ((array_mode == RAID0_STRIPING_DEV)
#ifdef SUPPORT_RAID1
        || (array_mode == RAID1_MIRROR)
#endif
#ifdef SUPPORT_SPAN
		|| (array_mode == SPANNING)
#endif
		)
	{
		tmp_array_size = Min(slot_lba[0], slot_lba[1]);
		if (array_mode == RAID0_STRIPING_DEV)
		{
			tmp_array_size <<= 1;
		}
#ifdef SUPPORT_SPAN
		else if (array_mode == SPANNING)
		{
			tmp_array_size = slot_lba[0] + slot_lba[1];
		}
#endif	
	}
	else
	{
		tmp_array_size = slot_lba[slot_num];
	}
#else
        tmp_array_size = slot_lba[slot_num];
#endif
    
	DBG("slotC %lx, %lx\n", (u32)slot_lba[0], (u32)slot_lba[1]);
	DBG("cap0 %lx\n", (u32)(user_data_area_size[0]));
	DBG("cap1 %lx\n", (u32)(user_data_area_size[1]));
	DBG("arrC %lx\n", (u32)tmp_array_size);
	
	ram_rp[slot_num].array_size = tmp_array_size;

	ram_rp[slot_num].disk[0].disk_bus_type = 'a'; 
	ram_rp[slot_num].disk[0].NT_disk_id = 0x00; 	
	if (sata0Obj.sobj_device_state == SATA_NO_DEV)
	{
		xmemset(ram_rp[slot_num].disk[0].disk_model_num, 0x20, 60);
		ram_rp[slot_num].disk[0].disk_status = SS_MISSING;	
		ram_rp[slot_num].disk[0].usableCapcity = 0;
		ram_rp[slot_num].disk[0].blockLength = 0; // logical sector size in bytes
		ram_rp[slot_num].disk[0].mediumRotationRate = 0;
		ram_rp[slot_num].disk[0].nominalFormFactor= 0;
	}
	else
	{
		xmemcpy(sata0Obj.modelNum, ram_rp[slot_num].disk[0].disk_model_num, 60);
		ram_rp[slot_num].disk[0].disk_status = cur_status[SLOT0];
		
		ram_rp[slot_num].disk[0].usableCapcity = slot_lba[0];
		ram_rp[slot_num].disk[0].blockLength = (1<<sata0Obj.physicalBlockSize_log2); // logical sector size in bytes
		ram_rp[slot_num].disk[0].mediumRotationRate = sata0Obj.sobj_medium_rotation_rate;
		ram_rp[slot_num].disk[0].nominalFormFactor= sata0Obj.sobj_nominal_form_factor;
	}	
	

	ram_rp[slot_num].disk[1].disk_bus_type = 'a'; 
	ram_rp[slot_num].disk[1].NT_disk_id = 0x00; 
	if (sata1Obj.sobj_device_state == SATA_NO_DEV)
	{
		xmemset(ram_rp[slot_num].disk[1].disk_model_num, 0x20, 60); 
		ram_rp[slot_num].disk[1].disk_status = SS_MISSING;	
		ram_rp[slot_num].disk[1].usableCapcity = 0;
		ram_rp[slot_num].disk[1].blockLength = 0; // logical sector size in bytes
		ram_rp[slot_num].disk[1].mediumRotationRate = 0;
		ram_rp[slot_num].disk[1].nominalFormFactor= 0;
	}
	else
	{
		xmemcpy(sata1Obj.modelNum, ram_rp[slot_num].disk[1].disk_model_num, 60);
		ram_rp[slot_num].disk[1].disk_status = cur_status[SLOT1];	
		
		ram_rp[slot_num].disk[1].usableCapcity = slot_lba[1];
		ram_rp[slot_num].disk[1].blockLength = (1<<sata1Obj.physicalBlockSize_log2); // logical sector size in bytes
		ram_rp[slot_num].disk[1].mediumRotationRate = sata1Obj.sobj_medium_rotation_rate;
		ram_rp[slot_num].disk[1].nominalFormFactor= sata1Obj.sobj_nominal_form_factor;
	}
}

static u8 raid_create_array_from_volume(PVOL_DESC pTmpVol, u8 volNum)
{
	MSG("vn %bx am %bx\n", volNum, pTmpVol->arrayMode);
	// 1 array mode
	if (
#ifdef SUPPORT_RAID1
        (pTmpVol->arrayMode != RAID1_MIRROR) &&
#endif
#ifdef SUPPORT_RAID0     
        (pTmpVol->arrayMode != RAID0_STRIPING_DEV) &&
#endif
#ifdef SUPPORT_SPAN
		(pTmpVol->arrayMode != SPANNING) &&
#endif
		(pTmpVol->arrayMode != JBOD) &&
		(pTmpVol->arrayMode != NOT_CONFIGURED))
	{
		return 1;
	}

#ifdef WDC_RAID  // ini doesn't support stripe size setting now
	// 2 Stripe Size, if stripe size equ 3, means 512 * 2(m3) bytes, viz. 4K	
	if (pTmpVol->arrayMode == RAID0_STRIPING_DEV)
	{
		// 3610 can support the stripe size from 512 to 2M bytes
		// WD request support form 512 to 128K:  At a 
		// minimum Interlaken 2X devices shall support values for n from zero to eight inclusively
		if (pTmpVol->stripe_size > 7) return 1;
	}
	else
	{
		if (pTmpVol->stripe_size != 0) return 1;
	}
	// 3 REBUILD PERCENTAGE COMPLETE field
	// 4:11 Start LBA, shall be 0
	if ((pTmpVol->start_lba) || (pTmpVol->rebuild_percentage_complete))
	{
		return 1;
	}

	// 12:19 Number of Blocks
	u64 tmp64 = pTmpVol->numOfBlocks;
	WRITE_U64_BE(&(pTmpVol->numOfBlocks), tmp64);
	MSG("numOfBlocks %lx %lx\n", (u32)(pTmpVol->numOfBlocks>>32), (u32)(pTmpVol->numOfBlocks));
#endif
	u64 slot_lba[2];
	slot_lba[0] = get_maximum_capacity(0);	// if no physical drive, obtain 0
	slot_lba[1] = get_maximum_capacity(1);
	
	u64 temp_user_data_size = Min(slot_lba[0], slot_lba[1]);

	if ((temp_user_data_size == 0) && (pTmpVol->arrayMode != JBOD))
	{	// raid 0 & 1 need 2 device
		return 1;
	}
#ifdef WDC_RAID	
	if (pTmpVol->numOfBlocks != 0xFFFFFFFFFFFFFFFF)  // remaining capacity, interlaken2x it equals maximum capactiy
	{
		if (pTmpVol->arrayMode == RAID0_STRIPING_DEV)
		{
			temp_user_data_size <<= 1;
		}
		else if (pTmpVol->arrayMode == JBOD)
		{
			if ((sata0Obj.sobj_device_state == SATA_DEV_READY) && (sata1Obj.sobj_device_state == SATA_DEV_READY))
				temp_user_data_size = 	slot_lba[volNum];
			else if (sata0Obj.sobj_device_state == SATA_DEV_READY)
				temp_user_data_size = 	slot_lba[0];	
			else 
				temp_user_data_size = 	slot_lba[1];	
		}
		
		// so remaining_capacity = maximum_capaity - start_lba  (but here start_lba == 0)
		if (pTmpVol->numOfBlocks != temp_user_data_size)
		{
			return 1;
		}
	}
#endif
	// slot status shall contain either Good, Rebuilding, Spare, or Not Participating
	// Since Interlaken 2X product Spare shall not be accepted.
	// 1st slot status, 2nd slot status
	if ((pTmpVol->slot0OrgOrder != 0) || (pTmpVol->slot1OrgOrder != 1))
	{
		//first slot order shall be 0, and second shall be one
		return 1;
	}
//	u8 tmp_slot0_status = vol_ptr[20];
//	u8 tmp_slot1_status = vol_ptr[22];
	if (pTmpVol->arrayMode == JBOD)
	{
		if (((pTmpVol->slot0Status != SS_GOOD) && 
			(pTmpVol->slot0Status != SS_NOT_PARTICIPATING) /*&& 
			(pTmpVol->slot0Status != SS_REBUILDING)*/) || 
			((pTmpVol->slot1Status != SS_GOOD) && 
			(pTmpVol->slot1Status != SS_NOT_PARTICIPATING) /*&& 
			(pTmpVol->slot1Status != SS_REBUILDING)*/))
		{
			MSG("tmp_ss %bx %bx", pTmpVol->slot0Status, pTmpVol->slot1Status);
			return 1;
		}		

		if (volNum == 0)
		{
			cur_status[SLOT0] = pTmpVol->slot0Status;
			fill_raid_parms(0, pTmpVol->arrayMode, pTmpVol->stripe_size);
		}
		else
		{
			cur_status[SLOT1] = pTmpVol->slot1Status;
			fill_raid_parms(1, pTmpVol->arrayMode, pTmpVol->stripe_size);
		}
		
	}
	else
	{
		if (((pTmpVol->slot0Status != SS_GOOD) && 
			(pTmpVol->slot0Status != SS_NOT_PARTICIPATING) && 
			(pTmpVol->slot0Status != SS_REBUILDING)) || 
			((pTmpVol->slot1Status != SS_GOOD) && 
			(pTmpVol->slot1Status != SS_NOT_PARTICIPATING) && 
			(pTmpVol->slot1Status != SS_REBUILDING)))
		{
			MSG("tmp_ss %bx %bx", pTmpVol->slot0Status, pTmpVol->slot1Status);
			return 1;
		}	
		// chk & sync raidGenNum
		if (ram_rp[0].raidGenNum > ram_rp[1].raidGenNum)
			ram_rp[1].raidGenNum = ram_rp[0].raidGenNum;
		else
			ram_rp[0].raidGenNum = ram_rp[1].raidGenNum;
		
		cur_status[SLOT0] = pTmpVol->slot0Status;
		cur_status[SLOT1] = pTmpVol->slot1Status;
		fill_raid_parms(0, pTmpVol->arrayMode, pTmpVol->stripe_size);
		fill_raid_parms(1, pTmpVol->arrayMode, pTmpVol->stripe_size);
	}

#ifndef HDR2526_LED
	if (cur_status[SLOT0])
	{
		FAIL_SLOT0_LED_ON();
	}
	else
	{
		FAIL_SLOT0_LED_OFF();
	}
	if (cur_status[SLOT1])
	{
		FAIL_SLOT1_LED_ON();
	}
	else
	{
		FAIL_SLOT1_LED_OFF();
	}
#endif	
	return 0;
}

/************************************************************************\
 set_raid_configuration() - 
 
 	just support one volume desc setting.
	and just consider 'ac_create' is fully different from 'ac_rebuild'
	if the 2 point is wrong, the func would be replaced...... 
 */
void raid_wdc_maintenance_out_cmd(PCDB_CTXT pCtxt)	
{
#ifdef WDC_RAID
	u32 l_byteCnt = ((u32)cdb.byte[6] << 24) + ((u32)cdb.byte[7] << 16) + ((u32)cdb.byte[8] << 8) + cdb.byte[9];
#else
	u32 l_byteCnt = ((u32)cdb.byte[6] << 16) + ((u32)cdb.byte[7] << 8) + ((u32)cdb.byte[8]);
#endif

	if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_CTXT_SETUP_DONE)
	{
		if (IS_LOCKED(0) && IS_LOCKED(1))
		{
			hdd_err_2_sense_data(pCtxt, ERROR_INCORRECT_SECURITY_STATE);
			return;
		}

		if (l_byteCnt == 0)
		{
			nextDirection = DIRECTION_SEND_STATUS;
			return;
		}

		if ((cdb.byte[1] == RAID_SERVICE_ACTION)
		&& (cdb.byte[2] == RAID_REQ_SET_RAID_CONFIG))
		{
			#define SET_RAID_COFIG_PARAM_LEN		32

			if (l_byteCnt < SET_RAID_COFIG_PARAM_LEN)
			{// if that the bytecnt is less 32 to hold one VOLUME DESCRIPTOR, report ILL CDB
				goto invalid_cdb_field;
			}
		}
		else
		{
			goto invalid_cdb_field;
		}
	}
	if (scsi_chk_hdd_pwr(pCtxt)) 	return;	

	if (scsi_start_receive_data(pCtxt, l_byteCnt)) return;	

	RAID_STATUS_CONFIG_PARAM tmpRaidParam;

	xmemcpy(mc_buffer, (u8 *)&tmpRaidParam.signature, sizeof(RAID_STATUS_CONFIG_PARAM));

	if (
#ifdef WDC_RAID
		(tmpRaidParam.signature != 0x31) ||
#else
		(tmpRaidParam.signature != 0x49) ||
#endif
		(tmpRaidParam.numOfSlot != 0x02) ||
#ifdef WDC_RAID		
		(tmpRaidParam.maxVolPerHDD != 0x01) ||
#endif		
		(tmpRaidParam.numOfVolSetDesc == 0x00) ||
		(tmpRaidParam.numOfVolSetDesc > 0x02))
		goto invalid_param_field;

	if (tmpRaidParam.numOfVolSetDesc == 0x02)
	{	
		if (l_byteCnt < sizeof(RAID_STATUS_CONFIG_PARAM))
		{
			// if the bytecnt is less than 56 to hold two VOL DESC
			goto invalid_cdb_field;
		}
		MSG("2 vols ");

		// Create & NO_OP, NO_OP & NO_OP, good
		// Create & Create might be good depend on RAID MODE
		// Rebuild & Create, create & Rebuild fails	
		if (((tmpRaidParam.volDesc[0].array_status_cmd == AC_CREATE) && ((tmpRaidParam.volDesc[1].array_status_cmd == AC_REBUILD)))
		|| ((tmpRaidParam.volDesc[1].array_status_cmd == AC_CREATE) && ((tmpRaidParam.volDesc[0].array_status_cmd == AC_REBUILD)))
		|| ((tmpRaidParam.volDesc[0].array_status_cmd == AC_REBUILD) && (tmpRaidParam.volDesc[1].array_status_cmd == AC_REBUILD)))
		{
			goto invalid_param_field;
		}	

		// Create & Create, if both array mode are JBOD, it's good case
		if ((tmpRaidParam.volDesc[0].array_status_cmd == AC_CREATE) && ((tmpRaidParam.volDesc[1].array_status_cmd == AC_CREATE)))
		{
			if (!((tmpRaidParam.volDesc[0].arrayMode == JBOD) && (tmpRaidParam.volDesc[1].arrayMode == JBOD)))
			{
				goto invalid_param_field;
			}
		}
	}
	
	// should be at least one drive is attached
	if ((sata0Obj.sobj_device_state != SATA_DEV_READY) && (sata1Obj.sobj_device_state != SATA_DEV_READY))
	{
		goto invalid_param_field;
	}

	u8 update_RP_flag = 0;
	for (u8 volNum =0; volNum < tmpRaidParam.numOfVolSetDesc; volNum++)
	{
		//u8 vol_array_cmd = (i==0) ? tmpRaidParam.vol0_Desc.array_status_cmd : tmpRaidParam.vol1_Desc.array_status_cmd;
		switch (tmpRaidParam.volDesc[volNum].array_status_cmd)
		{
			case AC_NO_OP:
				MSG("no_op\n");
				break;
				
			case AC_CREATE:
				MSG("create\n");
				if (raid_create_array_from_volume(&tmpRaidParam.volDesc[volNum], volNum))
					goto invalid_param_field;
				MSG("success\n");
				update_RP_flag = 1;
				break;
				
			case AC_REBUILD:
				MSG("rebuild: %bx %bx\n", array_status_queue[volNum], tmpRaidParam.volDesc[volNum].arrayMode);
#ifdef DBG_FUNCTION
				if (tmpRaidParam.volDesc[volNum].arrayMode == RAID1_MIRROR)
#else
				if ((array_status_queue[volNum] == AS_DEGRADED) && (tmpRaidParam.volDesc[volNum].arrayMode == RAID1_MIRROR))
#endif
				{
					if ((sata0Obj.sobj_device_state != SATA_DEV_READY) || (sata1Obj.sobj_device_state != SATA_DEV_READY))
					{
						goto invalid_param_field;
					}
#ifdef WDC_RAID
					if ((tmpRaidParam.volDesc[volNum].start_lba) && (tmpRaidParam.volDesc[volNum].stripe_size))
						goto invalid_param_field;
#ifndef DBG_FUNCTION
					u64 temp_user_data_size = Min(user_data_area_size[0], user_data_area_size[1]);
					if (tmpRaidParam.volDesc[volNum].numOfBlocks != temp_user_data_size)
						goto invalid_param_field;
#endif	
#endif					
					if (((tmpRaidParam.volDesc[volNum].slot0Status == SS_GOOD) && (tmpRaidParam.volDesc[volNum].slot1Status == SS_REBUILDING))
						|| ((tmpRaidParam.volDesc[volNum].slot0Status == SS_REBUILDING) && (tmpRaidParam.volDesc[volNum].slot1Status == SS_GOOD)))
					{
						dbg_next_cmd = 1;
						if (tmpRaidParam.volDesc[volNum].slot0Status)  // means slot1 -> slot0
						{
							if (vital_data[1].cipherID != CIPHER_NOT_CONFIGURED)  // there is a cipher in src
							{
								//sync and save dek data
								if (sync_vital_data(SLOT0))
									goto io_error;
							}
							raid_rebuild_initiated(RAID1_REBUILD1_0);
						}
						else		// slot0 -> slot1
						{
							if (vital_data[0].cipherID != CIPHER_NOT_CONFIGURED) // there is a cipher in src
							{
								//sync and save dek data
								if (sync_vital_data(SLOT1))
									goto io_error;
							}
							raid_rebuild_initiated(RAID1_REBUILD0_1);
						}
						break;
					}
				}
				else
					goto invalid_param_field;
				
				break;

			default:
				goto invalid_param_field;
		}
	}	

	// save the raid parameters
	if (update_RP_flag)
	{
		if (sata0Obj.sobj_device_state == SATA_DEV_READY)
			need_to_store_slot[0] = 1;
		else
			need_to_store_slot[0] = 0;

		if (sata1Obj.sobj_device_state == SATA_DEV_READY)
			need_to_store_slot[1] = 1;
		else
			need_to_store_slot[1] = 0;
		metadata_status = MS_GOOD;

		if (((ram_rp[0].array_mode == RAID1_MIRROR) && (ram_rp[1].array_mode == RAID1_MIRROR))
			&& (((cur_status[0] == SS_GOOD) && (cur_status[1] == SS_REBUILDING)) ||((cur_status[0] == SS_REBUILDING) && (cur_status[1] == SS_GOOD))))
		{
			other_Params[0].raid_rebuildCheckPoint = REBUILD_METADATA_UPDATE_THRESHOLD;
			other_Params[1].raid_rebuildCheckPoint = REBUILD_METADATA_UPDATE_THRESHOLD;
			Write_Disk_MetaData(0, WD_OTHER_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP);
			Write_Disk_MetaData(1, WD_OTHER_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP);
		}
		store_slots();

		if ((arrayMode == RAID0_STRIPING_DEV)  // stripe capacity will be bigger than any one disk
#ifdef SUPPORT_SPAN
			|| (arrayMode == SPANNING)
#endif
			)
		{
			arrayMode = NOT_CONFIGURED;
		}
		else
		{
			u8 previous_num_arrays = num_arrays;
			//u8 previous_array_mode = arrayMode;
			raid_enumeration(0);
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
				// initialize raid mode
				raid_init();

				check_disks_vital_data(SYNC_VITAL_DATA_SAME_PRIORITY);
			}
#endif
			if (previous_num_arrays > num_arrays) // JBOD to raid1 case: lun 1 will be dummy disk
			{
				array_status_queue[1] = AS_NOT_CONFIGURED;
			}
		}		
	}
	return;

io_error:
	hdd_err_2_sense_data(pCtxt, ERROR_ATA_MEMORIZED);
	return;	
invalid_param_field:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_PARAM);
	return;
invalid_cdb_field:
	hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
	return;
}

void raid_wdc_maintenance_in_cmd(PCDB_CTXT pCtxt)
{
u8 source_id = cdb.byte[3];
	if (cdb.byte[1] != RAID_SERVICE_ACTION)
	{
		goto _INVALID_CDB;
	}

#ifdef WDC_RAID
	// check source ID field 
	// REPORT_RAID_STATUS does not require to check source ID
	// REPORT_SLOT_STATUS requires to check if source ID is correct, the allowed values are slot0, slot1, and physical drives
	if (cdb.byte[2] != RAID_REQ_REPORT_RAID_STATUS)
	{
		if (( source_id != SOURCE_ID_SLOT0) && ( source_id != SOURCE_ID_SLOT1) && ( source_id != SOURCE_ID_PHYSICAL))
		{
			goto _INVALID_CDB;
		}
	}
#endif	

	xmemset(mc_buffer, 0, 512);
	nextDirection = DIRECTION_SEND_DATA;
	alloc_len =((u32)cdb.byte[6] << 24) + ((u32)cdb.byte[7] << 16) + ((u32)cdb.byte[8] << 8) + cdb.byte[9];	
	
	if (cdb.byte[2] == RAID_REQ_REPORT_RAID_STATUS)
	{
		fill_raid_volume_descriptor();
	}
	else if (cdb.byte[2] == RAID_REQ_REPORT_SLOT_DETAILS)
	{
		byteCnt = 4;
#ifdef WDC_RAID
		mc_buffer[0] = 0x31;		// 0	signature
#else
		mc_buffer[0] = 0x49;		// 0	signature
#endif
		mc_buffer[3] = 0x02;		// 3	Number of Detail Descriptors =2 for Interlaken 2X

		// 1st Slot Detail Descriptor 
		mc_buffer[4 + 0] = 'a';	// 0 Bus Type = 'a', indicate the bus type is ATA
		u8 valid_flag = 1;
//		mc_buffer[4 + 1] = 1;		// 1 Valid
		mc_buffer[4 + 3] = 76;	// 3 Desc Length = 50
#ifdef WDC_RAID		
		u8 slot_num = 0;
		if ((source_id == SOURCE_ID_SLOT0) || (source_id == SOURCE_ID_SLOT1))
		{
			slot_num = source_id;
			if (source_id == SOURCE_ID_SLOT0)
			{
				if ((cur_status[SLOT0] == SS_EMPTY) 
					|| (cur_status[SLOT0] == SS_BLANK) 
					|| (cur_status[SLOT0] == SS_HARD_REJECTED) 
					|| (cur_status[SLOT0] == SS_SOFT_REJECTED) 
					|| (cur_status[SLOT0] == SS_NOT_PARTICIPATING))
				{
					valid_flag = 0;
				}
			}
			else
			{
				if ((cur_status[SLOT1] == SS_EMPTY) 
					|| (cur_status[SLOT1] == SS_BLANK) 
					|| (cur_status[SLOT1] == SS_HARD_REJECTED) 
					|| (cur_status[SLOT1] == SS_SOFT_REJECTED) 
					|| (cur_status[SLOT1] == SS_NOT_PARTICIPATING))
				{
					valid_flag = 0;
				}				
			}

			if (valid_flag)
			{
				u8 disk_num = ram_rp[slot_num].disk_num;
#ifdef DBG_FUNCTION
		if (disk_num > 2)
			MSG("WRONG\n");
#endif
				mc_buffer[4 + 1] = 0x01;
				// 4:43		Model Number
				xmemcpy((ram_rp[slot_num].disk[disk_num].disk_model_num), mc_buffer+4+4, 40);
				// 44: 63 	Serial Number
				xmemcpy((ram_rp[slot_num].disk[disk_num].disk_sn), mc_buffer+4+44, 20);
				// 64: 71 	Usable Capacity
				WRITE_U64_BE((mc_buffer+4+64), ram_rp[slot_num].disk[disk_num].usableCapcity);
				// 72: 73 	Block Length
				*(mc_buffer+4+72) = (u8)(ram_rp[slot_num].disk[disk_num].blockLength >> 8);
				*(mc_buffer+4+73) = (u8)(ram_rp[slot_num].disk[disk_num].blockLength);
				// 74: 75 	Medium Rotation Rate
				*(mc_buffer+4+74) = (u8)(ram_rp[slot_num].disk[disk_num].mediumRotationRate >> 8);
				*(mc_buffer+4+75) = (u8)(ram_rp[slot_num].disk[disk_num].mediumRotationRate);
				// 77(3:0 bit) 	Nominal Form Factor 
				mc_buffer[4+77] = ram_rp[slot_num].disk[disk_num].nominalFormFactor;			
			}

			byteCnt += 80;
		}
		else // SOURCE_ID_PHYSICAL
#endif		
		{
			// the last (2nd) Slot Detail Descriptor 
			mc_buffer[84 + 0] = 'a';	// 0 Bus Type = 'a', indicate the bus type is ATA
			mc_buffer[84 + 3] = 76;	// 3 Desc Length = 50
		
			// one question is that when the source ID is Physical, should check slot0 & slot1 or only one slot
			if ((cur_status[SLOT0] == SS_EMPTY) || (cur_status[SLOT0] == SS_MISSING) ||(cur_status[SLOT0] == SS_NOT_PARTICIPATING))
			{
				valid_flag = 0;
			}
			u64 tmp64 = (sata0Obj.sectorLBA & 0xFFFFFFFFFFFFF800);
			if (valid_flag)
			{
				mc_buffer[4 + 1] = 0x1;
				// 4:43	Model Number
				xmemcpy(sata0Obj.modelNum, mc_buffer+4+4, 40);
				// 44: 63 	Serial Number
				xmemcpy(sata0Obj.serialNum, mc_buffer+4+44, 20);
				// 64: 71 	Usable Capacity
				if ((product_detail.options & PO_NO_CDROM) == 0)
				{
					tmp64 = (sata0Obj.sectorLBA & 0xFFFFFFFFFFFFF800) - INI_VCD_SIZE;
				}
#ifndef WDC_RAID
				tmp64 -= INI_PRIVATE_SIZE;
#endif

				WRITE_U64_BE((mc_buffer+4+64), tmp64);
				// 72: 73 	logical Block Length // the Interlaken product reject the logical block length is greater than 512
				*(mc_buffer+4+72) = 0x02;
				*(mc_buffer+4+73) = 0x00;
				// 74: 75 	Medium Rotation Rate
				*(mc_buffer+4+74) = (u8)(sata0Obj.sobj_medium_rotation_rate >> 8);
				*(mc_buffer+4+75) = (u8)sata0Obj.sobj_medium_rotation_rate;
				// 77(3:0 bit) 	Nominal Form Factor 
				mc_buffer[4+77] = sata0Obj.sobj_nominal_form_factor;
			}

			valid_flag = 1;
			if ((cur_status[SLOT1] == SS_EMPTY) || (cur_status[SLOT1] == SS_MISSING) ||(cur_status[SLOT1] == SS_NOT_PARTICIPATING))
			{
				valid_flag = 0;
			}
			if (valid_flag)
			{
				mc_buffer[84 + 1] |= 1;
				// 4:43	Model Number
				xmemcpy(sata1Obj.modelNum, mc_buffer+84+4, 40);
				// 44: 53 	Serial Number
				xmemcpy(sata1Obj.serialNum, mc_buffer+84+44, 20);
				tmp64 = (sata1Obj.sectorLBA & 0xFFFFFFFFFFFFF800);
				if ((product_detail.options & PO_NO_CDROM) == 0)
				{
					tmp64 = (sata1Obj.sectorLBA & 0xFFFFFFFFFFFFF800) - INI_VCD_SIZE;
				}	
#ifndef WDC_RAID
				tmp64 -= INI_PRIVATE_SIZE;
#endif
				// 64: 71 	Usable Capacity
				WRITE_U64_BE((mc_buffer+84+64), tmp64);
				// 72: 73 	logical Block Length // the Interlaken product reject the logical block length is greater than 512
				*(mc_buffer+84+72) = 0x02;
				*(mc_buffer+84+73) = 0x00;
				// 74: 75 	Medium Rotation Rate
				*(mc_buffer+84+74) = (u8)(sata1Obj.sobj_medium_rotation_rate >> 8);
				*(mc_buffer+84+75) = (u8)sata1Obj.sobj_medium_rotation_rate;
				// 77(3:0 bit) 	Nominal Form Factor 
				mc_buffer[84+77] = sata1Obj.sobj_nominal_form_factor;
			}
			byteCnt += 160;
		}
	}
	else
	{
_INVALID_CDB:
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
		return;
	}
}

u32 raid_load_FIS_execute(PCDB_CTXT pCtxt, u8 control)
{
//	PEXT_CCM pScm = NULL;
//	SATA_CCM volatile	*pCCM = NULL;
	SATA_CH_REG volatile *sata_ch_reg = NULL;
	u32 xfer_cnt;
//	u8 CCMIndex = 0;
	u32 ata_lba_low;
	u32 ata_lba_high;
	u16 ata_sec_cnt;

	if (control & SATA0_CH)
	{
		pSataObj = &sata0Obj;
		if (control & CTXT_CTRL_DIR)
		{
			xfer_cnt = *raid_read_sata0_xfer_len;
			ata_lba_low = *raid_read_sata0_lba_low;
			ata_lba_high = *raid_read_sata0_lba_high;
		}
		else
		{
#ifdef ERR_RB_DBG  // for s0w fail case
			if (((err_rb_dbg == BOTH_FAIL) || (err_rb_dbg == R1_W0_FAIL)) && (arrayMode == RAID1_MIRROR) && 
				((pCtxt->CTXT_FIS[FIS_COMMAND] == ATA6_WRITE_DMA_EXT) || ((pCtxt->CTXT_FIS[FIS_COMMAND] == ATA6_WR_FPDMA_QUEUED))) &&
				(!(raid_rebuild_verify_status & RRVS_IN_PROCESS)))
			{
				// 2a 00 00 00 10 00 00 xx xx 00  will cause a FAIL
				if ((*raid_write_sata0_lba_low & 0x00FFFFFF) == 0x1000)
				{
					MSG("write 0 FAIL entrance\n");
					*raid_write_sata0_lba_high |= 0x00010000;
				}
			}
#endif
			xfer_cnt = *raid_write_sata0_xfer_len;
			ata_lba_low = *raid_write_sata0_lba_low;
			ata_lba_high = *raid_write_sata0_lba_high;
		}
	}
	else
	{
		pSataObj = &sata1Obj;
		if (control & CTXT_CTRL_DIR)
		{
			xfer_cnt = *raid_read_sata1_xfer_len;
			ata_lba_low = *raid_read_sata1_lba_low;
			ata_lba_high = *raid_read_sata1_lba_high;
		}
		else
		{
#ifdef ERR_RB_DBG  // for s1w fail case
			if (((err_rb_dbg == BOTH_FAIL) || (err_rb_dbg == R1_W1_FAIL)) && (arrayMode == RAID1_MIRROR) && 
				(pCtxt->CTXT_FIS[FIS_COMMAND] == ATA6_WRITE_DMA_EXT) &&
				(!(raid_rebuild_verify_status & RRVS_IN_PROCESS)))
			{
				// 2a 00 00 00 10 00 00 xx xx 00  will cause a FAIL
				if ((*raid_write_sata1_lba_low & 0x00FFFFFF) == 0x1000)
				{
					MSG("write 1 FAIL entrance\n");
					*raid_write_sata1_lba_high |= 0x00010000;
				}
			}
#endif
			xfer_cnt = *raid_write_sata1_xfer_len;
			ata_lba_low = *raid_write_sata1_lba_low;
			ata_lba_high = *raid_write_sata1_lba_high;
		}
	}
	sata_ch_reg = pSataObj->pSata_Ch_Reg;

	if (xfer_cnt)
	{
		if (xfer_cnt == 0x10000) // bit 16 indicate that bits 15:0==0 indicates 32M, 64K sectors
		{
			xfer_cnt = 0x2000000;  // xfer length
			DBG("32M\n");
			ata_sec_cnt = 0;
		}
		else
		{
			if (xfer_cnt &  BIT_16)
			{
				MSG("xfer cnt err");
				return CTXT_STATUS_ERROR;
			}
			else
			{
				ata_sec_cnt = xfer_cnt;
				xfer_cnt = (xfer_cnt & 0xFFFF) << 9; // calculate the xfer length from sector count
			}
		}
	}
	else
		return CTXT_STATUS_ERROR;

#ifdef RAID_ERROR_TEST
	// change the Ctxt fis following the right ata_lba and sector count
	if ((control & SATA0_CH) == 0)
	{
		ata_lba_high = 0xFFFF;
	}
#endif
	*((u32 *)&pCtxt->CTXT_FIS[4]) = ata_lba_low;
	*((u32 *)&pCtxt->CTXT_FIS[8]) = ata_lba_high;
#ifdef RAID_EXEC_DMA_CMD
	if ((curMscMode == MODE_BOT) || (raid_rebuild_verify_status & RRVS_IN_PROCESS) || (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN))
#else
	if ((curMscMode == MODE_BOT) || (raid_rebuild_verify_status & RRVS_IN_PROCESS))
#endif
	{
#ifdef RAID_EXEC_DMA_CMD
		if ((curMscMode == MODE_UAS) && ((raid_rebuild_verify_status & RRVS_IN_PROCESS) == 0))
		{
			u8 ata_cmd = ATA6_READ_DMA_EXT;
			if ((control & CTXT_CTRL_DIR) == 0)
			{
				pCtxt->CTXT_Prot = PROT_DMAOUT;
				ata_cmd = ATA6_WRITE_DMA_EXT;
			}
			else
			{
				pCtxt->CTXT_Prot = PROT_DMAIN;
			}
			pCtxt->CTXT_FIS[FIS_COMMAND] = ata_cmd;
			pCtxt->CTXT_FIS[FIS_FEATURE] = 0;
			sata_ch_reg->sata_FrameIntEn &= ~D2HFISIEN;
		}
#endif
		*((u32 *)&pCtxt->CTXT_FIS[12]) = ata_sec_cnt;
	}
	else
	{
		pCtxt->CTXT_FIS[FIS_FEATURE] = (u8)ata_sec_cnt;
		pCtxt->CTXT_FIS[FIS_FEATURE_EXT] = (u8)(ata_sec_cnt >> 8);
	}

	
	if (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN)
	{
		// read/write commands which starts the RAID Engine shall be single thread
		raid_xfer_length = xfer_cnt;
	}
#ifdef RAID_EXEC_DMA_CMD	
	if ((curMscMode == MODE_UAS) && ((raid_rebuild_verify_status & RRVS_IN_PROCESS) == 0) && ((pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN) == 0))
#else
	if ((curMscMode == MODE_UAS) && ((raid_rebuild_verify_status & RRVS_IN_PROCESS) == 0))
#endif
	{
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
			return CTXT_STATUS_ERROR;
		}
	}
	else
	{	
		sata_exec_ctxt(pSataObj, pCtxt);
	}
	return CTXT_STATUS_GOOD;
}

u32 raid_exec_ctxt(PCDB_CTXT pCtxt)
{
	SATA_CH_REG volatile *sata_ch_reg = NULL;
	u8 status;
	u32 sata0_xfer_len, sata1_xfer_len;

	// start the Raid calculation
	// when read raid_read_ctrl or raid_write_ctrl register, 
	// the HW automatically load RAID calculation results loaded in the SATA read registers
	// it takes 1 clock to complete the move
	
	if (pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR)
	{
		pCtxt->CTXT_PHASE_CASE = BIT_6;	// Hi = Di
		*raid_read_ata_lba_low = *((u32 *)&pCtxt->CTXT_FIS[FIS_LBA_LOW]);
		*raid_read_ata_lba_high = *((u32 *)&pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT]);
		if ((curMscMode == MODE_UAS) && ((raid_rebuild_verify_status & RRVS_IN_PROCESS) == 0))
			*raid_read_ata_xfer_len = (((u16)pCtxt->CTXT_FIS[FIS_FEATURE_EXT]) << 8) + pCtxt->CTXT_FIS[FIS_FEATURE];
		else
			*raid_read_ata_xfer_len = *((u16 *)&pCtxt->CTXT_FIS[FIS_SEC_CNT]);

	}
	else
	{
		pCtxt->CTXT_PHASE_CASE = BIT_12; // Ho = Do
		*raid_write_ata_lba_low = *((u32 *)&pCtxt->CTXT_FIS[FIS_LBA_LOW]);
		*raid_write_ata_lba_high = *((u32 *)&pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT]);
		if (curMscMode == MODE_UAS)	
			*raid_write_ata_xfer_len = (((u16)pCtxt->CTXT_FIS[FIS_FEATURE_EXT]) << 8) + pCtxt->CTXT_FIS[FIS_FEATURE];
		else
			*raid_write_ata_xfer_len = *((u16 *)&pCtxt->CTXT_FIS[FIS_SEC_CNT]);
		DBG("lbaL %lx, lbaH %lx, xferL %lx\n", *raid_write_ata_lba_low, *raid_write_ata_lba_high, *raid_write_ata_xfer_len);
	}

	pCtxt->CTXT_Status = CTXT_STATUS_PENDING;

	// set up the SATA FIS: sector LBA, sector count, device & feature bytes
	// load SATA FIS to different channels	
	if (pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR)
	{// direct in
		sata0_xfer_len = *raid_read_sata0_xfer_len;
		sata1_xfer_len = *raid_read_sata1_xfer_len;
	}
	else
	{
		sata0_xfer_len = *raid_write_sata0_xfer_len;
		sata1_xfer_len = *raid_write_sata1_xfer_len;
	}
	
	if ((sata0_xfer_len != 0) && (sata1_xfer_len != 0))
	{
		status = STATUS_TWO_CHANNELS_SET_UP;
	}
	else if (sata0_xfer_len)
	{
		status = STATUS_CHANNEL0_SET_UP;
	}
	else if (sata1_xfer_len)
	{
		status = STATUS_CHANNEL1_SET_UP;
	}
	else
	{
		MSG("err xferL\n");
		return CTXT_STATUS_ERROR;
	}
	
#ifdef DBG_FUNCTION
	if (dbg_next_cmd)
		MSG("CHs: %bx\n", status);
#endif
//	status = STATUS_CHANNEL0_SET_UP;
	if (status == STATUS_CHANNEL0_SET_UP)
	{
		// use the single chanel only rather than through RAID IP
		pCtxt->CTXT_FLAG = (pCtxt->CTXT_FLAG | CTXT_FLAG_RAID_SATA_REGS_SET_UP_DONE) & ~CTXT_FLAG_RAID_EN;
		return raid_load_FIS_execute(pCtxt, pCtxt->CTXT_CONTROL | SATA0_CH);
	}
	else if (status == STATUS_CHANNEL1_SET_UP)
	{
		// use the single chanel only rather than through RAID IP
		pCtxt->CTXT_DbufIndex |= DBUF_SATA1_CHANNEL;
		pCtxt->CTXT_FLAG = (pCtxt->CTXT_FLAG | CTXT_FLAG_RAID_SATA_REGS_SET_UP_DONE) & ~CTXT_FLAG_RAID_EN;
		return raid_load_FIS_execute(pCtxt, pCtxt->CTXT_CONTROL);
	}
	else if (status == STATUS_TWO_CHANNELS_SET_UP)
	{
		if (!(raid_rebuild_verify_status & RRVS_IN_PROCESS))
		{
			if (curMscMode == MODE_UAS) 
			{
				
				uas_ci_paused_ctxt = pCtxt;
				u8 pause_ciu_flag = 0;
				if ((sata0Obj.sobj_State > SATA_READY) || (sata1Obj.sobj_State > SATA_READY))
				{
					pause_ciu_flag = 1;
				}
				
				if (usb_ctxt.curCtxt != NULL)
				{
					if ((usb_ctxt.curCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN) == 0)
					{
						pause_ciu_flag = 1;
					}
				}
				
				if (pause_ciu_flag)
				{
					uas_ci_paused |= UAS_PAUSE_WAIT_FOR_USB_RESOURCE;
					usb_ctxt.newCtxt = pCtxt;
					pCtxt->CTXT_usb_state = CTXT_USB_STATE_AWAIT_SATA_READY;
					return CTXT_STATUS_PAUSE;
				}
				uas_ci_paused |= UAS_PAUSE_RAID_RW;
			}
			
			pCtxt->CTXT_DbufIndex |= DBUF_RAID_FUNCTION_ENABLE;
			pCtxt->CTXT_FLAG |=  CTXT_FLAG_RAID_EN;
		}
		if (raid_load_FIS_execute(pCtxt, pCtxt->CTXT_CONTROL | SATA0_CH) == CTXT_STATUS_ERROR)
			return CTXT_STATUS_ERROR;
		
		pCtxt->CTXT_FLAG |= CTXT_FLAG_RAID_SATA_REGS_SET_UP_DONE;
		if (pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR)
		{				
			if ((raid_rebuild_verify_status & RRVS_IN_PROCESS)) // for mirror verify
			{
				*raid_read_ctrl = (RAID1_RD_CMPEN |RAID_RD_START);
				if (dbg_next_cmd)
				{
					MSG("raid_read_ctrl: %lx\n", *raid_read_ctrl);
				}
			}
			else
				*raid_read_ctrl = RAID_RD_START;
			
			raid_xfer_status = RAID_XFER_IN_RPOGRESS|RAID_XFER_READ_FLAG;
		}
		else
		{
			*raid_write_ctrl = RAID_WRITE_CONTINUE|RAID_WR_START;
//			*raid_write_ctrl = RAID_WR_START;
			DBG("raid wr ctrl %lx\n", *raid_write_ctrl);
			raid_xfer_status = RAID_XFER_IN_RPOGRESS|RAID_XFER_WRITE_FLAG;			
		}

		raid_load_FIS_execute(pCtxt, pCtxt->CTXT_CONTROL);
	}
	else
		return CTXT_STATUS_ERROR;
	
	return pCtxt->CTXT_Status;
}

void raid_done_isr()
{
	PCDB_CTXT pCtxt = usb_ctxt.curCtxt;
	if (pCtxt)
	{
		if (raid_xfer_status & RAID_XFER_READ_FLAG)
		{
			u32 raid_rd_ctrl = *raid_read_ctrl;
			u8 ctxt_control = pCtxt->CTXT_CONTROL;
			
			if (raid_rd_ctrl & RAID_RD_CMPERR)
			{

			}
			else // complete condition/early termination 
			{
				if (pCtxt->CTXT_Status != CTXT_STATUS_XFER_DONE)
				{
					return;
				}

				if ((raid_xfer_status & RAID_XFER_DONE) == 0) // wait for SATA xfer done
					return;
				if ( raid_rd_ctrl & RAID_RD_CNTNZ)
				{
					MSG("R ETerm ");
					if (raid_rd_ctrl & RAID_RD_RD0_SDONE)
						MSG("0 Dn\n");
					else if (raid_rd_ctrl & RAID_RD_RD1_SDONE)
						MSG("1 Dn\n");
				}

				if (ctxt_control & CTXT_CTRL_SATA0_EN)				
				{
					sata0Obj.pSata_Ch_Reg->sata_BlkCtrl |= RXSYNCFIFORST;	//reset sata RX  fifo	
					sata0Obj.pSata_Ch_Reg->sata_BlkCtrl;
#ifdef RAID_EXEC_DMA_CMD
					if (curMscMode == MODE_UAS)
					{
						sata0Obj.pSata_Ch_Reg->sata_FrameIntEn |= D2HFISIEN;
					}
#endif
				}

				if (ctxt_control & CTXT_CTRL_SATA1_EN)				
				{
					sata1Obj.pSata_Ch_Reg->sata_BlkCtrl |= RXSYNCFIFORST;	//reset sata RX  fifo	
					sata1Obj.pSata_Ch_Reg->sata_BlkCtrl;
#ifdef RAID_EXEC_DMA_CMD
					if (curMscMode == MODE_UAS)
					{
						sata1Obj.pSata_Ch_Reg->sata_FrameIntEn |= D2HFISIEN;
					}
#endif
				}				

				if ((pCtxt->CTXT_FLAG & CTXT_FLAG_UAS_ERR_ABORT) == 0)
				{
					u16 ata0_error_status = 0;
					u16 ata1_error_status = 0;
					if (ctxt_control & CTXT_CTRL_SATA0_EN)
						ata0_error_status =  *((u16 *)(&(sata0Obj.pSata_Ch_Reg->sata_Status)));
					if (ctxt_control & CTXT_CTRL_SATA1_EN)
						ata1_error_status =  *((u16 *)(&(sata1Obj.pSata_Ch_Reg->sata_Status)));

					pCtxt->CTXT_ccmIndex = CCM_NULL;	

					if ((ata0_error_status & ATA_STATUS_CHECK) || (ata1_error_status & ATA_STATUS_CHECK))
					{
						if (ata0_error_status & ATA_STATUS_CHECK)
							*(((u16 *) &(pCtxt->CTXT_FIS[FIS_STATUS]))) =  ata0_error_status;
						else
							*(((u16 *) &(pCtxt->CTXT_FIS[FIS_STATUS]))) =  ata1_error_status;
						
						dbg_next_cmd = 1;
						MSG("Rr Ata");
						if (ata0_error_status & ATA_STATUS_CHECK)
							MSG("0");
						else
							MSG("1");
						MSG(" err %bx, %bx\n", pCtxt->CTXT_FIS[FIS_STATUS], pCtxt->CTXT_FIS[FIS_ERROR]);
						// generate SCSI Sense code
						if ((pCtxt->CTXT_FLAG & CTXT_FLAG_B2S) == 0)
							hdd_ata_err_2_sense_data(pCtxt);
					}
					else
					{
						pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
					}
				}
				else
				{
					hdd_ata_err_2_sense_data(pCtxt);
				}
				pCtxt->CTXT_CONTROL |= CTXT_CTRL_SEND_SIU;
				pCtxt->CTXT_ccmIndex = CCM_NULL;
				uas_ci_paused &= ~UAS_PAUSE_RAID_RW;
				raid_xfer_status = 0;
				return;
			}
		}
		else if (raid_xfer_status & RAID_XFER_WRITE_FLAG)
		{
			u32 raid_wr_ctrl = *raid_write_ctrl;
			if (dbg_next_cmd)
				MSG("rdWCtrl %lx, %bx, ct s %bx\n", raid_wr_ctrl, raid_xfer_status, pCtxt->CTXT_Status);
			u8 ctxt_control = pCtxt->CTXT_CONTROL;

			// normal
			u8 segIndex = pCtxt->CTXT_DbufIndex;
			if (pCtxt->CTXT_Status != CTXT_STATUS_XFER_DONE)// wait for USB done or early termination
			{
				return;
			}

			if ((raid_xfer_status & RAID_XFER_DONE) == 0) // wait for SATA xfer done
				return;

			if (raid_wr_ctrl & RAID_WR_CNTNZ)
			{
				// early terminate
				MSG("W ETerm ");
				if (raid_wr_ctrl & RAID_WR_WR0_DDONE)
				{
					MSG("0 Dn\n");	
				}
				else if (raid_wr_ctrl & RAID_WR_WR1_DDONE)
				{
					// 
					MSG("1 Dn\n");
				}	
				dbg_next_cmd = 1;
			}
			

			DBG("rxs %bx\n", raid_xfer_status);

			usb_rx_fifo_rst();
#ifdef AES_EN
			if (pCtxt->CTXT_FLAG & CTXT_FLAG_AES)
			{
				*aes_control &= ~(AES_ENCRYP_EN | AES_ENCRYP_KEYSET_SEL);
				DBG("dis AES EN\n");
			}
#endif
//				reset_dbuf_seg(segIndex);
			if (ctxt_control & CTXT_CTRL_SATA0_EN)
			{
				sata0Obj.pSata_Ch_Reg->sata_BlkCtrl |= TXSYNCFIFORST;	//reset sata TX  fifo	
				sata0Obj.pSata_Ch_Reg->sata_BlkCtrl;
#ifdef RAID_EXEC_DMA_CMD
				if (curMscMode == MODE_UAS)
				{
					sata0Obj.pSata_Ch_Reg->sata_FrameIntEn |= D2HFISIEN;
				}
#endif
			}
			if (ctxt_control & CTXT_CTRL_SATA1_EN)
			{
				sata1Obj.pSata_Ch_Reg->sata_BlkCtrl |= TXSYNCFIFORST;	//reset sata TX  fifo	
				sata1Obj.pSata_Ch_Reg->sata_BlkCtrl;
#ifdef RAID_EXEC_DMA_CMD
				if (curMscMode == MODE_UAS)
				{
					sata1Obj.pSata_Ch_Reg->sata_FrameIntEn |= D2HFISIEN;
				}
#endif
			}
#ifdef AES_EN
			if (pCtxt->CTXT_FLAG & CTXT_FLAG_AES)
			{
				*aes_control &= ~(AES_ENCRYP_EN | AES_ENCRYP_KEYSET_SEL);
			}	
#endif

			free_dbufConnection(segIndex);
#ifdef AES_EN
			if (pCtxt->CTXT_FLAG & CTXT_FLAG_AES)
			{
				*aes_control &= ~(AES_ENCRYP_EN | AES_ENCRYP_KEYSET_SEL);
				DBG("dis AES EN\n");
			}
#endif
			usb_rx_fifo_rst();
			*raid_write_ctrl = RAID_WR_RESET;
			*raid_write_ata_xfer_len = 0;
			*raid_write_ata_lba_low = 0;
			*raid_write_ata_lba_high = 0;
			if ((pCtxt->CTXT_FLAG & CTXT_FLAG_UAS_ERR_ABORT) == 0)
			{
				u16 ata0_error_status = 0;
				u16 ata1_error_status = 0;
				if (ctxt_control & CTXT_CTRL_SATA0_EN)
					ata0_error_status =  *((u16 *)(&(sata0Obj.pSata_Ch_Reg->sata_Status)));
				if (ctxt_control & CTXT_CTRL_SATA1_EN)
					ata1_error_status =  *((u16 *)(&(sata1Obj.pSata_Ch_Reg->sata_Status)));

				pCtxt->CTXT_ccmIndex = CCM_NULL;			
				if ((ata0_error_status & ATA_STATUS_CHECK) || (ata1_error_status & ATA_STATUS_CHECK))
				{
					if (ata0_error_status & ATA_STATUS_CHECK)
						*(((u16 *) &(pCtxt->CTXT_FIS[FIS_STATUS]))) =  ata0_error_status;
					else
						*(((u16 *) &(pCtxt->CTXT_FIS[FIS_STATUS]))) =  ata1_error_status;
					
					dbg_next_cmd = 1;
					MSG("Rw Ata");
					if (ata0_error_status & ATA_STATUS_CHECK)
					{
						write_fail_dev = FAIL_SLOT0;
						MSG("0");
					}
					else
					{
						write_fail_dev = FAIL_SLOT1;
						MSG("1");
					}
					MSG(" err %bx, %bx\n", pCtxt->CTXT_FIS[FIS_STATUS], pCtxt->CTXT_FIS[FIS_ERROR]);
					// generate SCSI Sense code
					if ((pCtxt->CTXT_FLAG & CTXT_FLAG_B2S) == 0)
						hdd_ata_err_2_sense_data(pCtxt);
				}
				else
				{
					pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
				}
			}
			else
			{
				hdd_ata_err_2_sense_data(pCtxt);
			}
			uas_ci_paused &= ~UAS_PAUSE_RAID_RW;
			if (!(pCtxt->CTXT_FLAG & CTXT_FLAG_B2S))
			{

				//  send status back to USB Host
#ifdef LINK_RECOVERY_MONITOR
				if (rw_flag & LINK_RETRY_FLAG)
				{
					w_recover_counter++;
					MSG("W recvry %x\n", w_recover_counter);
				}
#endif
				raid_xfer_status = 0;
				usb_ctxt_send_status(pCtxt);
			}
		}
		else
			return;
	}
	else
		MSG("CurCTXT Null\n");
}

void reset_connection_rebuild(u8 dbuf_indx_src, u8 dbuf_indx_dest)
{
	if (raid_mirror_operation & RAID1_VERIFY)
	{
		sata_ch_reg = sata0Obj.pSata_Ch_Reg;
		sata_ch_reg->sata_BlkCtrl |= RXSYNCFIFORST;	//reset sata RX  fifo
		sata_ch_reg->sata_BlkCtrl;
		sata_ch_reg = sata1Obj.pSata_Ch_Reg;
		sata_ch_reg->sata_BlkCtrl |= RXSYNCFIFORST;	//reset sata RX  fifo
		sata_ch_reg->sata_BlkCtrl;
	}
	else if (raid_mirror_operation & RAID1_REBUILD0_1)
	{
		sata_ch_reg = sata0Obj.pSata_Ch_Reg;
		sata_ch_reg->sata_BlkCtrl |= RXSYNCFIFORST;	//reset sata RX  fifo
		sata_ch_reg->sata_BlkCtrl;
		sata_ch_reg = sata1Obj.pSata_Ch_Reg;
		sata_ch_reg->sata_BlkCtrl |= TXSYNCFIFORST;	//reset sata TX  fifo
		sata_ch_reg->sata_BlkCtrl;	
	}
	else //if (raid_mirror_operation & RAID1_REBUILD1_0)
	{
		sata_ch_reg = sata0Obj.pSata_Ch_Reg;
		sata_ch_reg->sata_BlkCtrl |= TXSYNCFIFORST;	//reset sata RX  fifo
		sata_ch_reg->sata_BlkCtrl;
		sata_ch_reg = sata1Obj.pSata_Ch_Reg;
		sata_ch_reg->sata_BlkCtrl |= RXSYNCFIFORST;	//reset sata TX  fifo
		sata_ch_reg->sata_BlkCtrl;	
	}
	
	reset_dbuf_seg(dbuf_indx_src);
	if (!(raid_mirror_operation & RAID1_VERIFY))
	{
		reset_dbuf_seg(dbuf_indx_dest);
		free_dbufConnection(dbuf_indx_src);
		free_dbufConnection(dbuf_indx_dest);
	}
	else
		free_dbufConnection(dbuf_indx_src);
}

void initiate_rb_IO(u64 start, u64 rbLen)
{
	CDB_CTXT ctxt0, ctxt1;

	// clear it for cheking later
	sata0Obj.pSata_Ch_Reg->sata_Status = 0x40;
	sata1Obj.pSata_Ch_Reg->sata_Status = 0x40;
	
	u32 xfer_cnt = (u32)Min(MAX_REBUILD_XFER_LENTH, (rbLen - start));
	u64 rwLBA;

	if (instant_rebuild_flag)
		rwLBA = start;
	else
		rwLBA = rebuildLBA + start;
	
	ctxt0.CTXT_FLAG = CTXT_FLAG_B2S;
	ctxt0.CTXT_Prot = PROT_DMAIN;
	ctxt0.CTXT_TimeOutTick = WDC_SATA_LONG_TIME_OUT;
	rw_time_out = 200;
	ctxt0.CTXT_ccm_cmdinten = D2HFISIEN;
	// TX DBUF -> RX DBUF SATA0/SATA1 -> DD -> SATA1/SATA0
	*((u32 *)&(ctxt0.CTXT_FIS[FIS_TYPE])) = (ATA6_READ_DMA_EXT << 16)|(0x80 << 8)|(0x27) ;

	*((u32 *)&(ctxt0.CTXT_FIS[FIS_LBA_LOW])) = (u32)(rwLBA & 0xFFFFFF) | (0x40 << 24);
	*((u32 *)&(ctxt0.CTXT_FIS[FIS_LBA_LOW_EXT])) = (u32)((rwLBA >> 24) & 0xFFFFFF);


	if (xfer_cnt < MAX_REBUILD_XFER_LENTH)
	{
		*((u32 *)&(ctxt0.CTXT_FIS[FIS_SEC_CNT])) = xfer_cnt;
		ctxt0.CTXT_XFRLENGTH = xfer_cnt << 9;
	}	
	else
	{
		*((u32 *)&(ctxt0.CTXT_FIS[FIS_SEC_CNT])) = 0;	 // 0-> 32MB
		ctxt0.CTXT_XFRLENGTH = 0x2000000;
	}
	
	ctxt1 = ctxt0;
	ctxt1.CTXT_Prot = PROT_DMAOUT;
	ctxt1.CTXT_FIS[FIS_COMMAND] = ATA6_WRITE_DMA_EXT;

	MSG("RBlba %bx %lx\n", (u8)(rwLBA>>32), (u32)rwLBA);
#ifdef ERR_RB_DBG
	if((err_rb_dbg == BOTH_FAIL) || (err_rb_dbg == RB_R_FAIL))
	{
		// read err case (ctxt0 err)
		MSG("will r fail\n");
		ctxt0.CTXT_FIS[FIS_LBA_HIGH_EXT] = 0x01;
	}
	if ((err_rb_dbg == BOTH_FAIL) || (err_rb_dbg == RB_W_FAIL))
	{
		// write err case (ctxt1 err)
		MSG("will w fail\n");
		ctxt1.CTXT_FIS[FIS_LBA_HIGH_EXT] = 0x01;	
	}	
#endif
	raid_rebuild_verify_status = (raid_rebuild_verify_status & ~(RRVS_DONE|RRVS_IO_DONE)) | RRVS_IN_PROCESS;
	if (raid_rebuild_verify_status & RRVS_ONLINE)
	{
		MSG("ref dbf clk\n");
		*cpu_Clock = SEL_REF_DBUFRX|SEL_REF_DBUFTX;
	}
	//MSG("CPU clock:%LX\n", *cpu_clock_select);
	//*cpu_clock_select = (*cpu_clock_select & ~(DBUFRXCLK_DIV | DBUFTXCLK_DIV)) | ((DBUF_CLOCK_USB2 << 16) | (DBUF_CLOCK_USB2 << 12));
	//*cpu_Clock_restore = (*cpu_Clock_restore & ~(DBUFRXCLK_DIV | DBUFTXCLK_DIV)) | ((DBUF_CLOCK_USB2 << 16) | (DBUF_CLOCK_USB2 << 12));

	// when the rebuild is done, update the watermark
	if (raid_mirror_operation & RAID1_REBUILD0_1)		// s0 -> s1
	{
		MSG("S0->S1\n");
		ctxt0.CTXT_DbufIndex = DBUF_SEG_S2SR_REBUILD0;
		sata_exec_ctxt(&sata0Obj, &ctxt0);
		ctxt1.CTXT_DbufIndex = DBUF_SEG_S2SW_REBUILD0;
		sata_exec_ctxt(&sata1Obj, &ctxt1);
	}
	else
	{
		MSG("S1->S0\n");
		ctxt0.CTXT_DbufIndex = DBUF_SEG_S2SR_REBUILD1;
		sata_exec_ctxt(&sata1Obj, &ctxt0);
		ctxt1.CTXT_DbufIndex = DBUF_SEG_S2SW_REBUILD1;
		sata_exec_ctxt(&sata0Obj, &ctxt1);
	}
	return;
}

u8 chk_rb_done()
{
	u8 rb_rc = RB_RC_OK;
	u8 src_dbuf_idx = (raid_mirror_operation == RAID1_REBUILD0_1) ? DBUF_SEG_S2SR_REBUILD0 :  DBUF_SEG_S2SR_REBUILD1;
	u8 dest_dbuf_idx = (raid_mirror_operation == RAID1_REBUILD0_1) ? DBUF_SEG_S2SW_REBUILD0: DBUF_SEG_S2SW_REBUILD1;

	if (*chip_IntStaus & SATA0_INT)
	{	
		DBG("s0 ");
		sata_isr(&sata0Obj);
	}

	if (*chip_IntStaus & SATA1_INT)
	{		
		DBG("s1 ");
		sata_isr(&sata1Obj);
	}

	if ((raid_rebuild_verify_status & RRVS_DONE) == (RRVS_D0_DONE|RRVS_D1_DONE))
	{
		// Both channels were GOOD or FAIL cases
		// 
		reset_connection_rebuild(src_dbuf_idx, dest_dbuf_idx);

		DBG("sobjstate %bx %bx\n", sata0Obj.sobj_State, sata1Obj.sobj_State);
		DBG("satastatus %bx %bx\n", sata0Obj.pSata_Ch_Reg->sata_Status, sata1Obj.pSata_Ch_Reg->sata_Status);
		
		u8 ata0_error_status =  sata0Obj.pSata_Ch_Reg->sata_Status;
		u8 ata1_error_status =  sata1Obj.pSata_Ch_Reg->sata_Status;
		rb_rc = RB_RC_DONE;
		if ((ata0_error_status & ATA_STATUS_CHECK) || (ata1_error_status & ATA_STATUS_CHECK)) // S1 -> S0: W FAIL case
		{
			rb_rc = RB_RC_DONE_ERR;
			if (((raid_mirror_operation & RAID1_REBUILD0_1) && (ata1_error_status & ATA_STATUS_CHECK))
			|| ((raid_mirror_operation & RAID1_REBUILD1_0) && (ata0_error_status & ATA_STATUS_CHECK)))
			{
				MSG(" BOTH or S1W or S0W FAIL");
			}
			MSG(" 11 \n");
#if 1  // just for here to support one type fail now			
			if (!instant_rebuild_flag)
			{
				if (rebuildErrLBA != rebuildLBA)
					rebuildErrLBA = rebuildLBA;
				else
					rebuildRetryCnt++;

				if (rebuildRetryCnt >= maxRebuildRetryCnt)
				{
					// we don't like both fail...or we don't like read fail, so here just write fail
#ifdef HDD_CLONE					
					if (clone_state == 0)		// just for rebuild
#endif						
					{
						if (raid_mirror_operation & RAID1_REBUILD0_1)
							cur_status[SLOT1] = SS_FAILED;
						else 
							cur_status[SLOT0] = SS_FAILED;
					}
				}
			}
#endif			
		}
		else
		{
			MSG(" 00 \n");
		}
		return rb_rc;
	}	

	if (rw_time_out == 0)
	{
#ifdef DBG_FUNCTION1	
		dump_all_regs(0);
#endif		
		if (sata0Obj.sobj_device_state != SATA_NO_DEV)
		{
			sata_timeout_reset(&sata0Obj);
		}
		if (sata1Obj.sobj_device_state != SATA_NO_DEV)
		{
			sata_timeout_reset(&sata1Obj);
		}
		MSG(" rb timeout \n");
		return RB_RC_DONE_ERR;
	}
	
	if ((((raid_rebuild_verify_status & RRVS_IO_DONE) == RRVS_D1_IO_DONE) && 
		(sata1Obj.pSata_Ch_Reg->sata_Status & ATA_STATUS_CHECK)) ||
		(((raid_rebuild_verify_status & RRVS_IO_DONE) == RRVS_D0_IO_DONE) &&
		(sata0Obj.pSata_Ch_Reg->sata_Status & ATA_STATUS_CHECK)))
	{
		// error case in Rebuild
		u8 abort_rebuild = 0;
		u32 sataTCnt = 0;
		if (raid_mirror_operation & RAID1_REBUILD0_1)
		{
			//
			if (sata0Obj.pSata_Ch_Reg->sata_Status & ATA_STATUS_CHECK)
			{
				
				if ((sata0Obj.pSata_Ch_Reg->sata_TCnt) != 0)
				{
					sataTCnt = sata0Obj.pSata_Ch_Reg->sata_TCnt;
					Delayus(100);
					if (sataTCnt == sata0Obj.pSata_Ch_Reg->sata_TCnt)
					{
						abort_rebuild = 1;
#if 1 // clear broken drive's ISR data
						PSATA_OBJ  pSob = &sata0Obj;
						if (pSob->sobj_State == SATA_ACTIVE)
						{
							sata_ch_reg = pSob->pSata_Ch_Reg;
							//u32 sata_frame_int = sata_ch_reg->sata_FrameInt;
							//if (sata_frame_int & D2HFISI)
							{
						#if 0
								delete_curSCM(pSob);
						#else
								PEXT_CCM pScm = pSob->sobj_curScm;
								if (pScm != NULL)
								{
									u32 bit32 = 1 << pScm->scm_ccmIndex;
									pSob->sobj_sBusy &= ~bit32;
									sata_DetachCCM(pSob, bit32);						
								}
								pSob->sobj_curScm = NULL;
						#endif
								pSob->sobj_State = SATA_READY;
								// the following is for UAS only 
								//sata_exec_next_scm(pSob);
								sata_ch_reg->sata_FrameInt = D2HFISI;
								// clear SATA Frame interrupt 
								sata_ch_reg->sata_IntStatus = FRAMEINTPEND;
								sata_ch_reg->sata_IntStatus = (RXDATARLSDONE|RXRLSDONE|DATARXDONE | DATATXDONE);
								//MSG("rI %lx %x %bx\n", sata_ch_reg->sata_FrameInt, sata_ch_reg->sata_IntStatus, pSob->sobj_State);
							}
						}
#endif
						
					}
				}
			}
		}
		else if (raid_mirror_operation & RAID1_REBUILD1_0)
		{
			if (sata1Obj.pSata_Ch_Reg->sata_Status & ATA_STATUS_CHECK)
			{
				if ((sata1Obj.pSata_Ch_Reg->sata_TCnt) != 0)
				{
					sataTCnt = sata1Obj.pSata_Ch_Reg->sata_TCnt;
					Delayus(100);
					if (sataTCnt == sata1Obj.pSata_Ch_Reg->sata_TCnt)
					{
						abort_rebuild = 1;
#if 1 // clear broken drive's ISR data
						PSATA_OBJ  pSob = &sata1Obj;
						if (pSob->sobj_State == SATA_ACTIVE)
						{
							sata_ch_reg = pSob->pSata_Ch_Reg;
							//u32 sata_frame_int = sata_ch_reg->sata_FrameInt;
							//if (sata_frame_int & D2HFISI)
							{
						#if 0
								delete_curSCM(pSob);
						#else
								PEXT_CCM pScm = pSob->sobj_curScm;
								if (pScm != NULL)
								{
									u32 bit32 = 1 << pScm->scm_ccmIndex;
									pSob->sobj_sBusy &= ~bit32;
									sata_DetachCCM(pSob, bit32);						
								}
								pSob->sobj_curScm = NULL;
						#endif

								pSob->sobj_State = SATA_READY;
								// the following is for UAS only 
								//sata_exec_next_scm(pSob);
								sata_ch_reg->sata_FrameInt = D2HFISI;
								// clear SATA Frame interrupt 
								sata_ch_reg->sata_IntStatus = FRAMEINTPEND;
								sata_ch_reg->sata_IntStatus = (RXDATARLSDONE|RXRLSDONE|DATARXDONE | DATATXDONE);
								//MSG("rI %lx %x %bx\n", sata_ch_reg->sata_FrameInt, sata_ch_reg->sata_IntStatus, pSob->sobj_State);
							}
						}
#endif
						
					}
				}					
			}
		}
		
		if (abort_rebuild)
		{			
			MSG("sobjstate %bx %bx\n", sata0Obj.sobj_State, sata1Obj.sobj_State);
			MSG("satastatus %bx %bx\n", sata0Obj.pSata_Ch_Reg->sata_Status, sata1Obj.pSata_Ch_Reg->sata_Status);
			// abort the ccm of sata which has Check condition	
			if (*chip_IntStaus & SATA0_INT)
			{	
				sata_isr(&sata0Obj);
			}

			if (*chip_IntStaus & SATA1_INT)
			{		
				sata_isr(&sata1Obj);
			}
			raid_rebuild_verify_status &= ~RRVS_IN_PROCESS;
			// different cases:
			// case 1: ctxt0 Read fail & TCNT = 0, ctxt1 can complete
			// case 2: ctxt0 Read fail & TCNT != 0, ctxt1 can't complete, and shall be aborted
			// case 3: write fail, do the bitbucket of read to throw away the read data
			reset_connection_rebuild(src_dbuf_idx, dest_dbuf_idx);

			// The fail one will "DONE" (received FIS34) at once, the other one will "HUNG UP"
			if (sata1Obj.sobj_State == SATA_ACTIVE) 
			{
				sata_HardReset(&sata1Obj);
				sata_InitSataObj(&sata1Obj);
				
				// S0 -> S1: R FAIL case
				MSG(" S0R F\n");
			}
			else if (sata0Obj.sobj_State == SATA_ACTIVE) 
			{
				sata_HardReset(&sata0Obj);
				sata_InitSataObj(&sata0Obj);
				
				// S1 -> S0: R FAIL case
				MSG(" S1R F\n");
			}
#if 1  // just for here to support one type fail now			
			if (!instant_rebuild_flag)
			{
				if (rebuildErrLBA != rebuildLBA)
					rebuildErrLBA = rebuildLBA;
				else
					rebuildRetryCnt++;

				if (rebuildRetryCnt >= maxRebuildRetryCnt)
				{
					// we don't like both fail...or we don't like read fail, but here is r fail too many times on the same LBA
					
				}
			}
#endif			
			return RB_RC_DONE_ERR;
		}
	}
	return rb_rc;
}

void raid_rebuild(void)
{
	u64 start = 0;
	u8 rb_rc = RB_RC_OK;
	switch (raid_rb_state)
	{
		case RAID_RB_IDLE:
		{
#ifdef HARDWARE_RAID
			if (raid_config_button_pressed) 
			{
				return;
			}
#endif
			
			// if the SATA is busy, can't initiate the rebuild IO
			if (!((sata0Obj.sobj_State == SATA_READY) && (sata1Obj.sobj_State == SATA_READY))) return;
#ifdef HDD_CLONE
			if ((USB_VBUS()) && (clone_state == 0))
#else			
			if (USB_VBUS())
#endif
			{// online rebuild shall care the bandwith of the USB
				// if there's USB Cmd is going to execute, do not initiate the rebuild to let USB have the full bandwith
		#if 1
				if ((usb_ctxt.ctxt_que != NULL) || (usb_ctxt.curCtxt != NULL) || (raid1_active_timer))
				{
					// adjust the flow led for raid1
			#ifdef HDR2526_LED
					if (arrayMode == RAID1_MIRROR)
					{
						RAID0_LED_OFF();
						JBOD_LED_OFF();
						RAID1_LED_ON();
					}
			#endif		
					return;
				}
		#else
				if ((usb_ctxt.ctxt_que != NULL) || (usb_ctxt.curCtxt != NULL))	return; 
				if (raid1_active_timer)	return;
		#endif		

				raid_rebuild_verify_status |= RRVS_ONLINE;
			}
#ifdef HDR2526_LED
			tickle_rebuild();
#endif
			start = 0;
			// Maximum rebuild length is REBUILD_RANGE_LENTH (32MB)
			rebuildLen = Min(REBUILD_RANGE_LENTH, (array_data_area_size - rebuildLBA));
			initiate_rb_IO(start, (u64)rebuildLen);
#ifdef HDD_CLONE
			if ((USB_VBUS()) && (clone_state == 0))
#else			
			if (USB_VBUS())
#endif
			{
				raid_rebuild_verify_status |= RRVS_ONLINE;
				if (curMscMode == MODE_UAS)
				{
					uas_ci_paused |= UAS_PAUSE_RAID_RB;
				}
				else
				{
					bot_active |= BOT_ACTIVE_RAID_RB;
				}
			}
			else
			{
				raid_rebuild_verify_status &= ~RRVS_ONLINE;
			}
			raid_rb_state = RAID_RB_IN_PROCESS;
			break;
		}

		case RAID_RB_IN_PROCESS:
		{
			rb_rc = chk_rb_done();
			if (rb_rc)
			{
				if (rb_rc == RB_RC_DONE)
					rebuildRetryCnt = 0;
				raid_rb_state = RAID_RB_DONE_UPDATE;
				if (raid_rebuild_verify_status & RRVS_ONLINE)
					*cpu_Clock = 0;
				//*cpu_clock_select = (*cpu_clock_select & ~(DBUFRXCLK_DIV | DBUFTXCLK_DIV)) | ((DBUF_CLOCK << 16) | (DBUF_CLOCK << 12));
				//*cpu_Clock_restore = (*cpu_Clock_restore & ~(DBUFRXCLK_DIV | DBUFTXCLK_DIV)) | ((DBUF_CLOCK << 16) | (DBUF_CLOCK << 12));

				if (instant_rebuild_flag)
				{// check the error case
					if (rb_rc == RB_RC_DONE)
					{
						array_status_queue[0] = AS_GOOD;
						cur_status[SLOT0] = SS_GOOD;
						cur_status[SLOT1] = SS_GOOD;

						// AS_GOOD will ignore "raid1_use_slot", so do NOT need update it here
					}
					else
					{
						//update_slots(cur_status[SLOT0], cur_status[SLOT1]);
						MSG("insrbF: %bx %bx\n", cur_status[SLOT0], cur_status[SLOT1]);
						if (cur_status[SLOT0] == SS_FAILED)
							update_slots_for_failed(1, 0);
						else
							update_slots_for_failed(0, 1);
						store_slots();
						// update "raid1_use_slot" already while start to instant rb, so do NOT need update it here
					}
				}
			} 
			else 
			{
				break;
			}
		}
		
		case RAID_RB_DONE_UPDATE:
               // update the status and params
		{	
			
			if (instant_rebuild_flag)
			{// for instant rebuild, it shall return the Status (CSW or SIU)
				usb_ctxt_send_status(pCtxt_INST_RB);
				pCtxt_INST_RB = NULL;

				MSG("insRB D\n");
				instant_rebuild_flag = 0;
				raid_mirror_status = 0;
			}
			else
			{
				if (rb_rc == RB_RC_DONE)
				{// update the watermark
					rebuildLBA += rebuildLen;
					if (rebuildLBA >= array_data_area_size)
					{
						MSG("\nRAID_GOOD!!\n");
						raid_mirror_status = 0;
						cur_status[SLOT0] = SS_GOOD;
						cur_status[SLOT1] = SS_GOOD;
						array_status_queue[0] = AS_GOOD;

						if (raid_mirror_operation & RAID1_REBUILD0_1)
							ram_rp[1].raidGenNum = ram_rp[0].raidGenNum;
						else if (raid_mirror_operation & RAID1_REBUILD1_0)
							ram_rp[0].raidGenNum = ram_rp[1].raidGenNum;
						ram_rp[0].disk[0].disk_status = SS_GOOD;
						ram_rp[0].disk[1].disk_status = SS_GOOD;
						ram_rp[1].disk[0].disk_status = SS_GOOD;
						ram_rp[1].disk[1].disk_status = SS_GOOD;
						other_Params[0].raid_rebuildCheckPoint = 0;
						other_Params[1].raid_rebuildCheckPoint = 0;
#ifdef HDD_CLONE
						if (clone_state == CS_CLONING)
						{
		#ifdef HDR2526
							clone_state = CS_CLONE_DONE_HOLD;
					#ifdef DBG_FUNCTION
							//update_clone_standby_timer(1200);	// 2 min
							update_clone_standby_timer(600);
					#else
							//update_clone_standby_timer(24000);
							update_clone_standby_timer(12000);
					#endif
		#else
							clone_state = 0;
		#endif
						}
						else
#endif							
						{
							num_arrays = 1;
							append_array_status(AS_GOOD, RAID1_MIRROR, SS_GOOD, SS_GOOD);
							Write_Disk_MetaData(0, WD_OTHER_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP);
							Write_Disk_MetaData(1, WD_OTHER_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP);
							//sss update_slots(cur_status[SLOT0], cur_status[SLOT1]);
							need_to_store_slot[0] = 1;
							need_to_store_slot[1] = 1;
							store_slots();
						}
						tickle_fault();
					}
					else if ((rebuildLBA > other_Params[0].raid_rebuildCheckPoint)
#ifdef HDD_CLONE
						&& (clone_state != CS_CLONING)
#endif						
						)
					{
						MSG("wm ");
						other_Params[0].raid_rebuildCheckPoint += REBUILD_METADATA_UPDATE_THRESHOLD;
						other_Params[1].raid_rebuildCheckPoint += REBUILD_METADATA_UPDATE_THRESHOLD;
						Write_Disk_MetaData(0, WD_OTHER_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP);
						Write_Disk_MetaData(1, WD_OTHER_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP);

#ifdef DBG_FUNCTION
						if (rebuildLBA >= 0x100000000)
							MSG("%bx ", (u8)(rebuildLBA>>32));
						MSG("%lx\n", (u32)rebuildLBA);
#ifdef RB_DONE_AT_ONCE_DBG		// it simulates rb done quickly for other logic test.
						rebuildLBA = array_data_area_size - 0x100; 
#endif
#endif		
					}
#ifdef CL_DONE_AT_ONCE_DBG		// it simulates rb done quickly for other logic test.
					rebuildLBA = array_data_area_size - 0x100; 
#endif

				}
				else if (rebuildRetryCnt >= maxRebuildRetryCnt)
				{
					raid_mirror_status = 0;
					raid_mirror_operation = 0;
					if (cur_status[SLOT0] == SS_FAILED)
						update_slots_for_failed(1, 0);
					else if (cur_status[SLOT1] == SS_FAILED)
						update_slots_for_failed(0, 1);
#ifdef HDD_CLONE
					if (clone_state == CS_CLONING)
					{
						clone_state = CS_CLONE_FAIL;
					}
					else
#endif				
					{
						// to here, some slot was fail, and if w fail, F/W would set its cur_status to FAILED, then saved. 
						// but if r fail, we just tell customer by fuzzer or led, and do not change and save its status
						// so if its reach the condition, means it was w fail
						if ((cur_status[SLOT0] == SS_FAILED) || (cur_status[SLOT1] == SS_FAILED))
						{
							num_arrays = 0;		// because the next func will increase it, so clear here
							set_raid1_array_status(cur_status[SLOT0], cur_status[SLOT1]);
							store_slots();
							update_raid1_read_use_slot();
						}
					}
#ifdef HDR2526_LED						
					fault_leds_active(1);
#endif
					MSG("rb fail to end\n");
				}
			}
			raid_rebuild_verify_status &= ~(RRVS_DONE|RRVS_IO_DONE|RRVS_IN_PROCESS);
			raid_rb_state = RAID_RB_IDLE;
			if (raid_rebuild_verify_status & RRVS_ONLINE)
			{
				if (curMscMode == MODE_UAS)
				{
					uas_ci_paused &= ~UAS_PAUSE_RAID_RB;
				}
				else
				{
					bot_active &= ~BOT_ACTIVE_RAID_RB;
				}
			}
		}
	}
}

// Initiate the instant rebuild IO
u8 instant_rebuild(PCDB_CTXT pCtxt)	
{
	u64 lba = (((u64)(*((u32 *)&pCtxt->CTXT_FIS[FIS_LBA_LOW_EXT]) & 0x00FFFFFF)) << 24) | (*((u32 *)&pCtxt->CTXT_FIS[FIS_LBA_LOW]) & 0x00FFFFFF);
	u32 xfer_cnt;

	if (curMscMode == MODE_UAS)	
		xfer_cnt = (u32)(((u16)pCtxt->CTXT_FIS[FIS_FEATURE_EXT]) << 8) + pCtxt->CTXT_FIS[FIS_FEATURE];
	else
		xfer_cnt =  (*((u32 *)&pCtxt->CTXT_FIS[FIS_SEC_CNT]) & 0x0000FFFF);

#ifdef ERR_RB_DBG
	if (err_rb_dbg)
	{
		if (err_rb_dbg == INS_RB_GOOD)
			MSG("rb good case, wlba %lx %lx cnt %lx\n", (u32)(lba >> 32), (u32)lba, xfer_cnt);
		lba &= ~0x10000000000;	// success case
	}
#endif
	MSG("err %bx, %bx\n", pCtxt->CTXT_FIS[FIS_STATUS], pCtxt->CTXT_FIS[FIS_ERROR]);
	MSG("ins rb lba %lx %lx cnt %lx\n", (u32)(lba >> 32), (u32)lba, xfer_cnt);
	MSG("s0State0 %bx, s1 %bx\n", sata0Obj.sobj_State, sata1Obj.sobj_State);

	if (pCtxt->CTXT_CONTROL & CTXT_CTRL_SATA0_EN)
	{
		MSG("err_s0\n");
		cur_status[SLOT0] = SS_FAILED;
		raid_mirror_operation = RAID1_REBUILD1_0;
	}
	else
	{
		MSG("err_s1\n");
		cur_status[SLOT1] = SS_FAILED;
		raid_mirror_operation = RAID1_REBUILD0_1;
	}

	if (array_status_queue[0] == AS_GOOD)
	{
		// Set Degraded Mode
		array_status_queue[0] = AS_DEGRADED;

		MSG("ins_rb_cnt %bx\n", instant_rebuild_cnt);
		if (instant_rebuild_cnt < instant_rebuild_threshold)
		{
			instant_rebuild_cnt++;

			// Perform Instant Rebuild
			raid_mirror_status = RAID1_STATUS_REBUILD;
			raid_rebuild_verify_status |= RRVS_ONLINE;
			instant_rebuild_flag = 1;
			pCtxt_INST_RB = pCtxt;
			// set the rebuild state to rebuild_online
			// then wait for instant rebuild done and then send out the SIU/CSW
			initiate_rb_IO(lba, (lba+xfer_cnt));
			raid_rb_state = RAID_RB_IN_PROCESS;
			if (curMscMode == MODE_UAS)
			{
				uas_ci_paused |= UAS_PAUSE_RAID_RB;
			}
			else
			{
				bot_active |= BOT_ACTIVE_RAID_RB;
			}
			return 1;
		}
	}
	else
	{
		array_status_queue[0] = AS_DATA_LOST;
	}

	update_raid1_read_use_slot();
	return 0;
}

u8 raid1_error_detection(PCDB_CTXT pCtxt)
{
	if (arrayMode == RAID1_MIRROR)
	{
		if (rw_flag & (READ_FLAG | WRITE_FLAG))
		{
			if ((((pCtxt->CTXT_CONTROL & CTXT_CTRL_SATA0_EN) == 0) || ((sata0Obj.pSata_Ch_Reg->sata_Status & ATA_STATUS_CHECK) == 0))
			&& (((pCtxt->CTXT_CONTROL & CTXT_CTRL_SATA1_EN) == 0) || ((sata1Obj.pSata_Ch_Reg->sata_Status & ATA_STATUS_CHECK) == 0)))
			{// the check condition is not the media access error, for example, the command fails because it's aborted by BOT thirtheen case or some CDB field has invalid params
				return 0;
			}
			
			MSG("IRB %bx, ataSta %bx, %bx\n", pCtxt->CTXT_CONTROL, sata0Obj.pSata_Ch_Reg->sata_Status, sata1Obj.pSata_Ch_Reg->sata_Status);
			
			if (rw_flag & READ_FLAG)
			{
				if (array_status_queue[0] != AS_REBUILDING)
				{
					if (instant_rebuild(pCtxt))	return 1;
				}
			}
			else  if (rw_flag & WRITE_FLAG)
			{
				if (array_status_queue[0] == AS_GOOD)
				{
					array_status_queue[0] = AS_DEGRADED;
					if (write_fail_dev == FAIL_SLOT0)
						cur_status[SLOT0] = SS_FAILED;
					else
						cur_status[SLOT1] = SS_FAILED;
				}
				else if (array_status_queue[0] == AS_REBUILDING)
				{
					if ((write_fail_dev == FAIL_SLOT0) && (cur_status[SLOT0] == SS_REBUILDING))
					{
						cur_status[SLOT0] = SS_FAILED;
						array_status_queue[0] = AS_DEGRADED;
					}
					else if ((write_fail_dev == FAIL_SLOT1) && (cur_status[SLOT1] == SS_REBUILDING))
					{
						cur_status[SLOT1] = SS_FAILED;
						array_status_queue[0] = AS_DEGRADED;
					}
					// stop rebuild
					raid_mirror_status = 0;
				}
				// update slot status
				//update_slots(cur_status[SLOT0], cur_status[SLOT1]);
				MSG("r1wF: %bx %bx\n", cur_status[SLOT0], cur_status[SLOT1]);
				if (cur_status[SLOT0] == SS_FAILED)
					update_slots_for_failed(1, 0);
				else
					update_slots_for_failed(0, 1);
				num_arrays = 0;		// because the next func will increase it, so clear here
				set_raid1_array_status(cur_status[SLOT0], cur_status[SLOT1]);
				store_slots();				
			}

			update_raid1_read_use_slot();
			tickle_fault();
		}
	}
	return 0;
}

#ifdef HARDWARE_RAID
u8 hw_raid_enum_check(void)
{
	u8 rc = 0;
	u8 hw_raid_gpio  = (((*gpioDataIn & GPIO5_PIN) >> 4) | ((*gpioDataIn & GPIO6_PIN) >> 6));

#ifdef DK101_IT1
    hw_raid_gpio = GPIO_JBOD;
#endif

#ifdef DATABYTE_RAID
	if (cfa_active)
		return 0;
	if ((raid_mirror_status) && (hw_raid_gpio == GPIO_RAID1))
	{
		rc = 1;
		goto _ignore_button;
	}
#endif	
	// go ahead til RAID_RB_IDLE at online
	if ((raid_mirror_status) && (raid_rb_state != RAID_RB_IDLE))	
		return 1;		
	MSG("chkREn %bx %bx %bx %bx\n", hw_raid_gpio, arrayMode, ram_rp[0].array_mode, ram_rp[1].array_mode);

	switch (hw_raid_gpio)
	{
		case GPIO_RAID0:
		case GPIO_RAID1:
		case GPIO_SPAN:
			//arrayMode = RAID0_STRIPING_DEV;
			//arrayMode = RAID1_MIRROR;
			//arrayMode = SPANING;
			if ((sata0Obj.sobj_device_state == SATA_NO_DEV) || 
				(sata1Obj.sobj_device_state == SATA_NO_DEV))
			{
				rc = 1;
			}
			// Be sure there is 2 disk then arrayMode indicate true raid mode
#if 0		// request need renum althrough in same mode	
			if (((hw_raid_gpio == GPIO_RAID0) && (arrayMode == RAID0_STRIPING_DEV)) ||
				((hw_raid_gpio == GPIO_RAID1) && (arrayMode == RAID1_MIRROR)) ||
				((hw_raid_gpio == GPIO_SPAN) && (arrayMode == SPANNING)))
			{
				MSG("s raid\n");
				rc = 1;
			}
#endif			
			break;
		case GPIO_JBOD:
			//arrayMode = JBOD;
			if ((sata0Obj.sobj_device_state == SATA_NO_DEV) && 
				(sata1Obj.sobj_device_state == SATA_NO_DEV))
			{
				rc = 1;
			}
#if 0			
			if ((arrayMode == JBOD) && (array_status_queue[0] != AS_BROKEN))
			{
				MSG("s jbod\n");
				rc = 1;
			}
#endif			
			break;
		default: 
			return 1;
	}

_ignore_button:
	if (rc)
	{
		raid_config_button_pressed = 0;
		return 1;
	}

	MSG("toEn\n");
	return 0;
}

#if 0
void raid_mbr_cleaner()
#else
void raid_mbr_cleaner(u8 new_raid)
#endif
{
	/**************************
		current raid		new raid
hdr2526 request: except raid1->jbod
		raid

	**************************/
	u8 i = 0;
	u8 del_num = 2;
	u8 n = (cur_status[SLOT0] == SS_GOOD) ? 0 : 1;

#if 1	
	if (new_raid == JBOD)
	{
		if ((newer_disk == 0) || (cur_status[SLOT0] == SS_EMPTY) || (cur_status[SLOT1] == SS_EMPTY))// just change mode or single disk
		{
			if ((ram_rp[n].array_mode == RAID1_MIRROR) 
				|| (ram_rp[n].array_mode == NOT_CONFIGURED) 
				/*|| (ram_rp[n].array_mode == JBOD)*/) // raid1 --> jbod, notCfig --> jbod
				return;
			MSG("rm0 %bx\n", ram_rp[n].array_mode);
		}
		else 		// change disk
		{
			MSG("rm1 %bx\n", ram_rp[n].array_mode);
			if (ram_rp[n].array_mode == JBOD)  // some one is JBOD
				return;
			MSG("nvmam %bx\n", nvm_rp[0].array_mode);
			if (nvm_rp[0].array_mode == JBOD)  // both changed, previous is JBOD
				return;
		}
	}
#endif	
	MSG("erasHDD\n");

	for (; i < del_num; i++)	
	{
		u8 rc = INI_RC_OK;
		CDB_CTXT ctxt;

	//	dbg_next_cmd = 1;
		if (i == 1)
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
			continue;
		}
		
		ctxt.CTXT_FLAG = CTXT_FLAG_B2S | CTXT_FLAG_SYNC;
		ctxt.CTXT_TimeOutTick = WDC_SATA_TIME_OUT;
		ctxt.CTXT_ccm_cmdinten = 0;
		ctxt.CTXT_XFRLENGTH = 512;
		ctxt.CTXT_FIS[FIS_TYPE] = 0x27;						// FIS 27
		ctxt.CTXT_FIS[FIS_C]	= 0x80;						// command bit valid
		ctxt.CTXT_FIS[FIS_CONTROL]	= 0x00;	

		ctxt.CTXT_CONTROL = 0;			// Data-Out	

		if (i == 1)
			ctxt.CTXT_DbufIndex = DBUF_SEG_B2S1W;
		else
			ctxt.CTXT_DbufIndex = DBUF_SEG_B2S0W;

		set_dbufSource(ctxt.CTXT_DbufIndex);
		write_data_by_cpu_port(ctxt.CTXT_XFRLENGTH, DIR_RX);
		Delay(10);
		set_dbufDone(ctxt.CTXT_DbufIndex);
		
		u64 tmp64 = 0;

		WRITE_U64_BE(cdb.rw16x.lba, tmp64);
		// Read or write one sector in the firmware private area.
		WRITE_U32_BE(cdb.rw16x.xfer, 1);

		// Load the ATA task-file regs with the LBA from the CDB.
		ata_load_fis_from_rw16Cdb(&ctxt, DMA_TYPE);

		ctxt.CTXT_Prot = PROT_DMAOUT;
		ctxt.CTXT_FIS[FIS_COMMAND]	= ATA6_WRITE_DMA_EXT;
		
		u32 sataFrameIntEn = pSataObj->pSata_Ch_Reg->sata_FrameIntEn;
		pSataObj->pSata_Ch_Reg->sata_FrameIntEn &= ~D2HFISIEN;
		rc = sata_exec_ctxt(pSataObj, &ctxt);	
		pSataObj->pSata_Ch_Reg->sata_FrameIntEn = sataFrameIntEn;
		
		DBG("rc %bx\n", rc);

		if (rc == CTXT_STATUS_LOCAL_TIMEOUT)
		{
			rst_dout(&ctxt);
		}
		else
		{
			pSataObj->sobj_State = SATA_READY;
		}

		DBG("state:%x\n", rc);

		//return rc;
	}	
}

//am_cond to indicate we can judge by arrayMode, so just PowOn we can't use it
u8 hw_raid_config(u8 am_cond)
{
DBG("gpio %lx\n", *gpioDataIn);
	u8 hw_raid_gpio  = (((*gpioDataIn & GPIO5_PIN) >> 4) | ((*gpioDataIn & GPIO6_PIN) >> 6));
    
#ifdef DK101_IT1
    hw_raid_gpio = GPIO_JBOD;
#endif
	MSG("gInx %bx\n", hw_raid_gpio);

#ifdef DATABYTE_RAID	// we DO NOT reconfig when in same mode
#ifdef HDR2526_GPIO_HIGH_PRIORITY
	if (am_cond)
	{
		if (((hw_raid_gpio == GPIO_RAID0) && (arrayMode == RAID0_STRIPING_DEV)) ||
			((hw_raid_gpio == GPIO_RAID1) && (arrayMode == RAID1_MIRROR)) ||
			((hw_raid_gpio == GPIO_SPAN) && (arrayMode == SPANNING)))
		{
			MSG("s raid\n");
			raid_mode = arrayMode;
			
			return 1;
		}
		if ((hw_raid_gpio == GPIO_JBOD) && (arrayMode == JBOD) && (array_status_queue[0] == AS_GOOD))
		{
			raid_mode = arrayMode;
		
			MSG("s jbod\n");
			return 1;
		}
	}
#else
	if (am_cond)  // in fact, there is nothing effect in NON gpio high priority F/W for the condition.
	{
#ifndef DATASTORE_LED
		if ((array_status_queue[0] == AS_GOOD) 
			&& (cur_status[SLOT0] == SS_GOOD) && (cur_status[SLOT1] == SS_GOOD)
			&& (((hw_raid_gpio == GPIO_RAID0) && (arrayMode == RAID0_STRIPING_DEV))
#ifdef SUPPORT_RAID1
            ||((hw_raid_gpio == GPIO_RAID1) && (arrayMode == RAID1_MIRROR)) 
#endif
#ifdef SUPPORT_SPAN
            ||((hw_raid_gpio == GPIO_SPAN) && (arrayMode == SPANNING))
#endif
            ))
		{
			MSG("s raid\n");
			raid_mode = arrayMode;
			
			return 1;
		}
#endif
		if ((hw_raid_gpio == GPIO_JBOD) && (arrayMode == JBOD) 
			&& (array_status_queue[0] == AS_GOOD) 
			&& (cur_status[SLOT0] == SS_GOOD)
			&& (cur_status[SLOT1] == SS_GOOD))
		{
			raid_mode = arrayMode;
		
			MSG("s jbod\n");
			return 1;
		}
	}
#endif	
#endif

	switch (hw_raid_gpio)
    {
#ifdef SUPPORT_RAID0
        case GPIO_RAID0:
            //arrayMode = RAID0_STRIPING_DEV;
            if ((sata0Obj.sobj_device_state == SATA_NO_DEV) || 
				(sata1Obj.sobj_device_state == SATA_NO_DEV))
				return 1;
#ifdef HDR2526
			raid_mbr_cleaner(RAID0_STRIPING_DEV);
#endif			
			cur_status[SLOT0] = SS_GOOD;
			cur_status[SLOT1] = SS_GOOD;
			fill_raid_parms(0, RAID0_STRIPING_DEV, 3);
			fill_raid_parms(1, RAID0_STRIPING_DEV, 3);
//#ifdef HDR2526_GPIO_HIGH_PRIORITY
			raid_mode = RAID0_STRIPING_DEV;
//#endif
			break;
#endif
#ifdef SUPPORT_RAID1
		case GPIO_RAID1:
			//arrayMode = RAID1_MIRROR;
			if ((sata0Obj.sobj_device_state == SATA_NO_DEV) || 
				(sata1Obj.sobj_device_state == SATA_NO_DEV))
				return 1;
#ifdef HDR2526
			raid_mbr_cleaner(RAID1_MIRROR);
#endif			
			cur_status[SLOT0] = SS_GOOD;
			cur_status[SLOT1] = SS_GOOD;
			fill_raid_parms(0, RAID1_MIRROR, 3);
			fill_raid_parms(1, RAID1_MIRROR, 3);
//#ifdef HDR2526_GPIO_HIGH_PRIORITY
			raid_mode = RAID1_MIRROR;
//#endif
			break;
#endif
		case GPIO_JBOD:
			//arrayMode = JBOD;
			if ((sata0Obj.sobj_device_state == SATA_NO_DEV) && 
				(sata1Obj.sobj_device_state == SATA_NO_DEV))
				return 1;
#ifdef HDR2526
			raid_mbr_cleaner(JBOD);
#endif			
			if (sata0Obj.sobj_device_state == SATA_DEV_READY)
				cur_status[SLOT0] = SS_GOOD;
			else
				cur_status[SLOT0] = SS_EMPTY;
			if (sata1Obj.sobj_device_state == SATA_DEV_READY)
				cur_status[SLOT1] = SS_GOOD;
			else
				cur_status[SLOT1] = SS_EMPTY;
			fill_raid_parms(0, JBOD, 3);
			fill_raid_parms(1, JBOD, 3);
//#ifdef HDR2526_GPIO_HIGH_PRIORITY
			raid_mode = JBOD;
//#endif
			break;
		case GPIO_SPAN:
#ifdef SUPPORT_SPAN
			//arrayMode = SPANING;
			if ((sata0Obj.sobj_device_state == SATA_NO_DEV) || 
				(sata1Obj.sobj_device_state == SATA_NO_DEV))
				return 1;
#ifdef HDR2526
			raid_mbr_cleaner(SPANNING);
#endif			
			cur_status[SLOT0] = SS_GOOD;
			cur_status[SLOT1] = SS_GOOD;
			fill_raid_parms(0, SPANNING, 3);
			fill_raid_parms(1, SPANNING, 3);
//#ifdef HDR2526_GPIO_HIGH_PRIORITY
			raid_mode = SPANNING;
//#endif
			break;
#endif			
		default: 
			return 1;
	}

#ifdef HDR2526_1
	if (hw_raid_gpio != GPIO_JBOD)
		raid_mbr_cleaner();
#endif

	// chk & sync raidGenNum
	if (ram_rp[0].raidGenNum > ram_rp[1].raidGenNum)
		ram_rp[1].raidGenNum = ram_rp[0].raidGenNum;
	else
		ram_rp[0].raidGenNum = ram_rp[1].raidGenNum;

	MSG("sata sds&ss %bx %bx %bx %bx\n", sata0Obj.sobj_device_state, sata0Obj.sobj_State, sata1Obj.sobj_device_state, sata1Obj.sobj_State);
	if ((sata0Obj.sobj_State >= SATA_PHY_RDY) && (sata0Obj.sobj_State <= SATA_READY))
		need_to_store_slot[0] = 1;
	else
		need_to_store_slot[0] = 0;

	if ((sata1Obj.sobj_State >= SATA_PHY_RDY) && (sata1Obj.sobj_State <= SATA_READY))
		need_to_store_slot[1] = 1;
	else
		need_to_store_slot[1] = 0;

	metadata_status = MS_GOOD;
	store_slots();

	raid_mirror_status = 0;
	// here never be 0, because next time is resume RB
	other_Params[0].raid_rebuildCheckPoint = REBUILD_METADATA_UPDATE_THRESHOLD;
	other_Params[1].raid_rebuildCheckPoint = REBUILD_METADATA_UPDATE_THRESHOLD;
	Write_Disk_MetaData(0, WD_OTHER_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP);
	Write_Disk_MetaData(1, WD_OTHER_PARAM_PAGE, FIRST_BACKUP | SECOND_BACKUP);

#ifdef HDR2526_LED
	hdr2526_led_init();
#endif
	
	return 0;
}
#endif

#ifdef HDD_CLONE
void clone_init(void)
{
	rebuildRetryCnt = 0;
	instant_rebuild_flag = 0; 
	raid_mirror_status = RAID1_STATUS_REBUILD;
#ifdef DBG_CAP
	array_data_area_size = VIR_CAP+0x100000;
#else
	array_data_area_size = sata0Obj.sectorLBA;
#endif
	rebuildLBA = 0;
	raid_rb_state = RAID_RB_IDLE;
	raid_rebuild_verify_status = 0;
	raid_mirror_operation = RAID1_REBUILD0_1;
	other_Params[0].raid_rebuildCheckPoint = REBUILD_METADATA_UPDATE_THRESHOLD;
	other_Params[1].raid_rebuildCheckPoint = REBUILD_METADATA_UPDATE_THRESHOLD;

	clone_state = CS_CLONING;
}
#endif

#ifdef SUPPORT_HR_HOT_PLUG
u8 chk_sata_active_status(void)
{
	u8 cur_sata_active = pre_sata_active;

	//if ((sata0Obj.sobj_device_state != SATA_DEV_UNKNOWN) && (sata1Obj.sobj_device_state != SATA_DEV_UNKNOWN))
	if (((sata0Obj.sobj_State != SATA_RESETING) && (sata1Obj.sobj_State != SATA_RESETING))
		|| ((sata0Obj.sobj_device_state == SATA_NO_DEV) && (sata1Obj.sobj_State != SATA_RESETING))
		|| ((sata1Obj.sobj_device_state == SATA_NO_DEV) && (sata0Obj.sobj_State != SATA_RESETING)))
	{
		for (u8 i = 0; i < 2; i++)
		{
			if (i)
			{
				pSataObj = &sata1Obj;
			}
			else
			{
				pSataObj = &sata0Obj;
			}
			sata_ch_reg = pSataObj->pSata_Ch_Reg;
			if ((sata_ch_reg->sata_PhyStat & PHYRDY) == 0)
			{
				cur_sata_active &= ~(1<<i);
			}
			else
			{
				cur_sata_active |= (1<<i);
			}
		}
	}
#ifdef DBG_FUNCTION
	if (cur_sata_active != pre_sata_active)
		MSG("cur_sata: %bx pre_sata %bx\n", cur_sata_active, pre_sata_active);
#endif
	return cur_sata_active;
}

void sata_status_chk_on_HR(void)
{
	/**************************
	JBOD
	any one is unpluged, re-enum;
	any one is pluged, re-enum;

	RAID1
	only 1 device case:
	a. just re-enum

	2 device case:
	any one dev is unplug, the info in the raid would be delete.
	a. both good, any one is unpluged, the other one may need reset
		(a1). the good one is re-plug, re-enum
		(a2). new one is re-plug, re-enum
	b. good one is unpluged, re-enum
		(b1). the good one is re-plug, re-enum
		(b2). new one is re-plug, re-enum
	c. bad one is unpluged, good one may need reset
		(c1). the bad one is re-plug, re-enum
		(c2). new one is re-plug, re-enum

	RAID0, SPAN
	any one is unpluged, re-enum;
	any one is pluged, re-enum;

	***************************/

	// check sata status
	//PSATA_OBJ pSataObj0, pSataObj1;
	//PSATA_OBJ pSataObj;
	//pSataObj0 = &sata0Obj;
	//pSataObj1 = &sata1Obj;
	// process for different raid when sata was changed
	if (chk_sata_active_status() != pre_sata_active)
	{
#if 1
		hot_plug_enum = 1;
		MSG("reenum4RB\n");

		raid_mirror_status = 0;
		raid_rb_state = RAID_RB_IDLE;
		raid_mirror_operation = 0;
		return;
#else
		if (/*(gpio_raid != JBOD) &&*/ (cur_sata_active > pre_sata_active)) // plug
		{
_HOT_PLUG_RENUM:		
			hot_plug_enum = 1;
			MSG("reenum4RB\n");
			return;
		}
		else if (cur_sata_active < pre_sata_active) // unplug
		{
			fault_leds_active(1);
			if ((gpio_raid == RAID1_MIRROR) && (cur_sata_active))
			{
				if (raid_mirror_status)  // raid1 & rebuilding
				{
					u8 src_dbuf_idx = (raid_mirror_operation == RAID1_REBUILD0_1) ? DBUF_SEG_S2SR_REBUILD0 :  DBUF_SEG_S2SR_REBUILD1;
					u8 dest_dbuf_idx = (raid_mirror_operation == RAID1_REBUILD0_1) ? DBUF_SEG_S2SW_REBUILD0: DBUF_SEG_S2SW_REBUILD1;
					reset_connection_rebuild(src_dbuf_idx, dest_dbuf_idx);

					raid_mirror_status = 0;
					raid_rb_state = RAID_RB_IDLE;
					if (raid_mirror_operation == RAID1_REBUILD0_1)
					{
						if (cur_sata_active == 2)  // good dev is unpluged(slot0); 
						{
							goto _HOT_PLUG_RENUM;
						}
						else // rebuilding dev is unpluged(slot1); the last one is slot0
						{
							pSataObj = &sata1Obj;
							cur_status[SLOT1] = SS_EMPTY;
							pre_sata_active &= 0x01;		// clear slot1
							pSataObj->sobj_device_state = SATA_NO_DEV;
							
							pSataObj = &sata0Obj;		// for next reset step
							if (pSataObj->sobj_State > SATA_READY)
							{
								if (curMscMode == MODE_UAS)
									uas_ci_paused |= UAS_PAUSE_SATA0_NOT_READY;
							}
						}
						
					}
					else if (raid_mirror_operation == RAID1_REBUILD1_0)
					{
						if (cur_sata_active == 1)  // good dev is unpluged(slot1); 
						{
							goto _HOT_PLUG_RENUM;
						}
						else // rebuilding dev is unpluged(slot0); the last one is slot1
						{
							pSataObj = &sata0Obj;
							cur_status[SLOT0] = SS_EMPTY;
							pre_sata_active &= 0x02;		// clear slot0
							pSataObj->sobj_device_state = SATA_NO_DEV;

							pSataObj = &sata1Obj;		// for next reset step
							if (pSataObj->sobj_State > SATA_READY)
							{
								if (curMscMode == MODE_UAS)
									uas_ci_paused |= UAS_PAUSE_SATA1_NOT_READY;
							}
						}
					}

					
				}
				else  // both good dev
				{
					Delay(1000);  // delay for blocking power off case
					if (cur_sata_active == 1) // slot 1 is unpluged, the last one is slot0
					{
						pSataObj = &sata1Obj;
						cur_status[SLOT1] = SS_EMPTY;
						pre_sata_active &= 0x01;		// clear slot1
						pSataObj->sobj_device_state = SATA_NO_DEV;
						MSG("clear 1\n");

						pSataObj = &sata0Obj;		// slot0 may need reset
						if (pSataObj->sobj_State > SATA_READY)
						{
							if (curMscMode == MODE_UAS)
								uas_ci_paused |= UAS_PAUSE_SATA0_NOT_READY;
						}
					}
					else // if (cur_sata_active == 2) slot 0 is unpluged, the last one is slot1
					{
						pSataObj = &sata0Obj;
						cur_status[SLOT0] = SS_EMPTY;
						pre_sata_active &= 0x02;		// clear slot0
						pSataObj->sobj_device_state = SATA_NO_DEV;
						MSG("clear 0\n");

						pSataObj = &sata1Obj;		// slot1 may need reset
						if (pSataObj->sobj_State > SATA_READY)
						{
							if (curMscMode == MODE_UAS)
								uas_ci_paused |= UAS_PAUSE_SATA1_NOT_READY;
						}
					}
				}

				// the last one may need be reseted
				if (pSataObj->sobj_State > SATA_READY)
				{
					sata_HardReset(pSataObj);
					sata_InitSataObj(pSataObj);
					MSG("lsob ss %bx sds %bx\n", pSataObj->sobj_State, pSataObj->sobj_device_state);
				}
				
				arrayMode = JBOD;
				raid_mirror_operation = 0;
				//append_array_status(AS_DEGRADED, RAID1_MIRROR, status0, status1);
				//update_slots_for_missing(good_slot, other_slot);
			}
			else  // other mode or no one is pluged
			{
				goto _HOT_PLUG_RENUM;
			}
		}
#endif		
	}

}
#endif

#endif

