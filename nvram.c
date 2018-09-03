/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/

#define NVRAM_C
#include	"general.h"


void spi_read_flash_vid(u8 spi_rd_vid)
{
	u8	intstat;

	reg_w32(spi_Addr, 0x00);
	reg_w32(spi_Ctrl, (SPI_START | SPI_READ | SPI_DATAV | SPI_ADDRV | (4 << SPI_CNT_SHIFT) | spi_rd_vid)); 
	do {
		intstat = reg_r8(spi_IntStatus);
	} while (!(intstat & SPI_IntStatus_Done));
	reg_w8(spi_IntStatus, intstat);

	DBG("rvid: %lx  ", reg_r32(spi_Data));

	flash_vid = reg_r32(spi_Data);
}

void spi_read_flash_pid()
{
	u8	intstat;
	
	reg_w32(spi_Addr, 0x01);
	reg_w32(spi_Ctrl, (SPI_START | SPI_READ | SPI_DATAV | SPI_ADDRV | (4 << SPI_CNT_SHIFT) | 0xAB)); 
	do {
		intstat = reg_r8(spi_IntStatus);
	} while (!(intstat & SPI_IntStatus_Done));
	reg_w8(spi_IntStatus, intstat);

	DBG("rpid: %lx\n", reg_r32(spi_Data));

	flash_pid = reg_r32(spi_Data);
}



void spi_detect_flash_id()
{
	flash_type = UNKNOWN_FLASH;
	flash_vid = 0;
	flash_pid = 0;
	DBG("flash_type: %x  flash_vid: %x  flash_pid: %x\n", flash_type, flash_vid, flash_pid);
	*spi_Clock = 0x01;

	spi_read_flash_vid(0x90); 
	spi_read_flash_pid();
	flash_size_larger_than_2Mb = 1;
	if (flash_vid == 0x9D)
	{
		if (flash_pid == 0x10)			// PM25LD010C	
		{
			flash_type = PM25LD010C;
			flash_size_larger_than_2Mb = 0;
		}
		else if (flash_pid == 0x11)		// PM25LD020C
			flash_type = PM25LD020C;
	}
 	else if (flash_vid == 0xBF)
	{
		if (flash_pid == 0x49)			// SST25VF010A
		{
			flash_type = SST25VF010A;
			flash_size_larger_than_2Mb = 0;
		}
		else if (flash_pid == 0x43)		// SST25VF020
			flash_type = SST25VF020;
		else if (flash_pid == 0x8C)		// SST25VF020B
			flash_type = SST25VF020B;
		else if (flash_pid == 0x44)		// SST25VF040
			flash_type = SST25VF040;
 	}
	else if (flash_vid == 0xEF)
	{
		if (flash_pid == 0x10) // W25X10BL
		{
			flash_type = W25X10BL;
			flash_size_larger_than_2Mb = 0;
		}
		else if (flash_pid == 0x11)		// W25X20BL
			flash_type = W25X20BL;
		else if (flash_pid == 0x12)		// W25X40BL
			flash_type = W25X40BL;
	}
	else
	{
		spi_read_flash_vid(0xAB);
		flash_vid = *spi_Data;
		flash_pid = *spi_Data >> 8;
		if (flash_vid == 0x9D)
		{
			if (flash_pid == 0x7C)		// PM25LV010A
			{
				flash_type = PM25LV010A;
				flash_size_larger_than_2Mb = 0;
			}
			else if (flash_pid == 0x7D)		// PM25LV020A
				flash_type = PM25LV020A;
			else if (flash_pid == 0x7E)		// PM25LD040
				flash_type = PM25LD040;
		}
		else
		{
			spi_read_flash_vid(0x9F);
			flash_vid = *spi_Data;
			flash_pid = *spi_Data >> 8;
			if ((flash_vid == 0x00) && (flash_pid == 0x62))  // LE25FU206A
			{
				flash_type = LE25FU206A;
			}
		}
	}

	if (flash_size_larger_than_2Mb == 0)
		*fw_tmp = (*fw_tmp & ~BOOT_FROM_SECONDARY) | BOOT_FROM_PRIMARY;

	MSG("Flash %bx, vid %bx  pid %bx\n", flash_type, flash_vid, flash_pid);

}




/************************************************************************\
 spi_read_flash() - erase one sector, support PMC

 */
void spi_read_flash(u8 *buffAddr, u32 flashAddr, u32 size)
{
	u8	intstat;
	u32	cnt;
	*spi_IntStatus = SPI_IntStatus_Done;
	*spi_Addr = 0x00;	
	for (cnt = 0x0; cnt < size; cnt+=4)
	{	
		*spi_Addr = flashAddr + cnt;
		*spi_Ctrl = (SPI_START | SPI_READ | SPI_ADDRV | SPI_DATAV | (4 << SPI_CNT_SHIFT) | SPI_CMD_READ);
		
		do 
		{
			intstat = *spi_IntStatus;
		} while (!(intstat & SPI_IntStatus_Done));
		
		*spi_IntStatus = intstat;
		
		*((u32 *)(buffAddr+ cnt)) = *spi_Data;

		#ifdef DBG_FUNCTION
		DBG("%lx\t", *((u32 *)(buffAddr+ cnt)));
		if ((cnt & 0x0f) == 0x0f)
			DBG("\n");
		#endif
	}
	// CheckSum ??
}


/************************************************************************\
 erase_chip() - erase one sector, just support SST & PMC now

 */
void spi_erase_flash_chip()
{
	u8	intstat;

	*spi_IntStatus = SPI_IntStatus_Done;
	if (((flash_type & 0xF0) == SST) || ((flash_type & 0xF0) == WINBOND))
	{
		*spi_Data = 0x00;
		*spi_Ctrl = (SPI_START | SPI_WRITE | SPI_DATAV | (1 << SPI_CNT_SHIFT)  |0x50); 	// EWSR
		do {
			intstat = *spi_IntStatus;
		} while (!(intstat & SPI_IntStatus_Done));
		*spi_IntStatus = intstat;
	}
	
	*spi_Data = 0x00;
	*spi_Ctrl = (SPI_START | SPI_WRITE | SPI_DATAV | (1 << SPI_CNT_SHIFT) |0x01); 	// WRSR
	do {
		intstat = *spi_IntStatus;
	} while (!(intstat & SPI_IntStatus_Done));
	*spi_IntStatus = intstat;
	
	// WREN
	*spi_Ctrl = (SPI_START | SPI_WRITE |0x06); 	// WREN
	do {
		intstat = *spi_IntStatus;
	} while (!(intstat & SPI_IntStatus_Done));
	*spi_IntStatus = intstat;
	// chip Erase
	*spi_Addr = 0x00;
	if (((flash_type & 0xF0) == SST) || ((flash_type & 0xF0) == WINBOND) || ((flash_type & 0xF0) == SANYO)  || ((flash_type & 0xF0) == UNKNOWN_FLASH))
	{
		//print("erasechip \n");
		*spi_Ctrl = (SPI_START | SPI_WRITE | 0x60);  //CHIP_ER_SST
	}
	else if ((flash_type & 0xF0) == PMC)
		*spi_Ctrl = (SPI_START | SPI_WRITE | 0xC7);  //CHIP_ER_PMC
	do {
		intstat = *spi_IntStatus;
	} while (!(intstat & SPI_IntStatus_Done));
	*spi_IntStatus = intstat;
	do
	{
		*spi_Ctrl = (SPI_START | SPI_READ| SPI_DATAV | (1 << SPI_CNT_SHIFT) | 0x05);  // RDSR
		do {
			intstat = *spi_IntStatus;
		} while (!(intstat & SPI_IntStatus_Done));
		*spi_IntStatus = intstat;
	} while (*spi_Data & 0x1);		
}




/************************************************************************\
 spi_erase_flash_sector() - erase one sector, just support SST & PMC now

 */
void spi_erase_flash_sector(u32 spi_addr)
{
	DBG("erase sec\n");
	u8	intstat;
	u8 l_flash_type = flash_type & 0xF0;
//	u16	size;	

	*spi_IntStatus = SPI_IntStatus_Done;

#ifdef SUPPORT_UNSUPPORTED_FLASH
	if ((l_flash_type == SST) || (l_flash_type == WINBOND) || (l_flash_type == UNKNOWN_FLASH))
#else
	if ((l_flash_type == SST) || (l_flash_type == WINBOND))
#endif
	{
		*spi_Data = 0x00;
		*spi_Ctrl = (SPI_START | SPI_WRITE | SPI_DATAV | (1 << SPI_CNT_SHIFT)  |0x50); 	// EWSR
#ifdef SUPPORT_UNSUPPORTED_FLASH
		_disable();
		flash_operation_timer = 2;
		_enable();
#endif
		do {
			intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
			_disable();
			if (flash_operation_timer == 0)
				break;
			_enable();
#endif
		} while (!(intstat & SPI_IntStatus_Done));
		*spi_IntStatus = intstat;
	}
	
	*spi_Data = 0x00;
	*spi_Ctrl = (SPI_START | SPI_WRITE | SPI_DATAV | (1 << SPI_CNT_SHIFT) |0x01); 	// WRSR
#ifdef SUPPORT_UNSUPPORTED_FLASH
	_disable();
	flash_operation_timer = 2;
	_enable();
#endif
	do {
		intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
		_disable();
		if (flash_operation_timer == 0)
			break;
		_enable();
#endif
	} while (!(intstat & SPI_IntStatus_Done));
	*spi_IntStatus = intstat;
	
	// WREN
	*spi_Ctrl = (SPI_START | SPI_WRITE |0x06); 	// WREN
#ifdef SUPPORT_UNSUPPORTED_FLASH
	_disable();
	flash_operation_timer = 2;
	_enable();
#endif
	do {
		intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
		_disable();
		if (flash_operation_timer == 0)
			break;
		_enable();
#endif
	} while (!(intstat & SPI_IntStatus_Done));
	*spi_IntStatus = intstat;
#if 0
	do 
	{
		reg_w32(spi_Ctrl, (SPI_START | SPI_READ| SPI_DATAV | (1 << SPI_CNT_SHIFT) | 0x05));  // RDSR
		do {
			intstat = reg_r8(spi_IntStatus);
		} while (!(intstat & SPI_IntStatus_Done));
		reg_w8(spi_IntStatus, intstat);
	} while (reg_r32(spi_Data) & 0x1);
#endif	
	// Sector Erase
	*spi_Addr = spi_addr;

	if ((l_flash_type == SST) || (l_flash_type == WINBOND) || (l_flash_type == SANYO)  || (l_flash_type == UNKNOWN_FLASH))
		*spi_Ctrl = (SPI_START | SPI_WRITE | SPI_ADDRV | 0x20);  //SECTOR_ER_SST
	else if (l_flash_type == PMC)
		*spi_Ctrl = (SPI_START | SPI_WRITE | SPI_ADDRV | 0xD7);  //SECTOR_ER_PMC
#ifdef SUPPORT_UNSUPPORTED_FLASH
	_disable();
	flash_operation_timer = 2;
	_enable();
#endif
	do {
		intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
		_disable();
		if (flash_operation_timer == 0)
			break;
		_enable();
#endif
	} while (!(intstat & SPI_IntStatus_Done));
	*spi_IntStatus = intstat;
#ifdef SUPPORT_UNSUPPORTED_FLASH
	_disable();
	flash_operation_timer = 2;
	_enable();
#endif
	do
	{
		*spi_Ctrl = (SPI_START | SPI_READ| SPI_DATAV | (1 << SPI_CNT_SHIFT) | 0x05);  // RDSR
		do {
			intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
			_disable();
			if (flash_operation_timer == 0)
			{
				flash_operation_timer = 2;
				break;
			}
			_enable();
#endif
		} while (!(intstat & SPI_IntStatus_Done));
		*spi_IntStatus = intstat;
#ifdef SUPPORT_UNSUPPORTED_FLASH
		_disable();
		if (flash_operation_timer == 0)
			break;
		_enable();
#endif
#if 0//def DEBUG
printf("sst_status4: %lx\n", reg_r32(spi_Data));
#endif
	} while (*spi_Data & 0x1);		
}




/************************************************************************\
 spi_write_flash() - save all data to flash, and addr is 0x00000

 */
u8 spi_write_flash(u8 *src, u32 spi_addr, u32 size)
{

	u8	intstat;
	u32	cnt = 0;
	u8 rc = 1;
DBG("spi_write_flash\n");
	
	if (size > MAX_FLASH_SPACE)
		rc = 0;
	else
	{

#if 1//def DEBUG
	DBG("get rdy 4 erase, data0: %bx spi_s_addr: %lx size: %lx\n", *src, spi_addr, size);
#endif
		if ((spi_addr & 0xFFF) == 0) // 4K boudary flash
		{
			spi_erase_flash_sector(spi_addr);		
		}

#if 1//def DEBUG
	DBG("ers done\n");
#endif

#if 0//def DEBUG
		DBG("willbesaved:");
		for (cnt = 0x0; cnt < size; cnt++)
		{
			if (!(cnt & 0x0f))
				MSG("\n");
			DBG("%bx ",  *(src+cnt));
		}
#endif	
#ifndef SFLASH_PAGE_PROGRAM
		for (cnt = 0x0; cnt < size; cnt++)
		{
			// WREN			
			*spi_Ctrl = (SPI_START | SPI_WRITE | 0x06); 	// WREN
#ifdef SUPPORT_UNSUPPORTED_FLASH
			_disable();
			flash_operation_timer = 2; // 200 ms should be long enough to complete the operation
#endif
			do {
				intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
				_disable();
				if (flash_operation_timer == 0)
					break;
				_enable();
#endif
			} while (!(intstat & SPI_IntStatus_Done));
			*spi_IntStatus = intstat;
			
			*spi_Ctrl = SFLASH_PAGE_RESET;
			*spi_Ctrl = 0;

			// program data	
			*spi_Addr = spi_addr+cnt;
			*((u8 *)spi_Data) = *(src+cnt);
//			MSG("%bx  ", *(src+cnt));
//			if ((cnt & 0x03) == 0x03)
//				MSG("\n");
			*spi_Ctrl = (SPI_START |SFLASH_PAGE_CLEAR| SPI_WRITE| SPI_DATAV | SPI_ADDRV | 0x02);  // BYTE_PROG
#ifdef SUPPORT_UNSUPPORTED_FLASH
			_disable();
			flash_operation_timer = 2; // 200 ms should be long enough to complete the operation
			_enable();
#endif
			do {
				intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
				_disable();
				if (flash_operation_timer == 0)
					break;
				_enable();
#endif
			} while (!(intstat & SPI_IntStatus_Done));
			*spi_IntStatus = intstat;

#ifdef SUPPORT_UNSUPPORTED_FLASH
			_disable();
			flash_operation_timer = 2; // 200 ms should be long enough to complete the operation
			_enable();
#endif		
			// wait for rdy
			do {
				*spi_Ctrl = (SPI_START | SPI_READ| SPI_DATAV | (1 << SPI_CNT_SHIFT) | 0x05);  // RDSR
				do {
					intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
					_disable();
					if (flash_operation_timer == 0)
					{
						flash_operation_timer = 2;
						break;
					}
					_enable();
#endif
				} while (!(intstat & SPI_IntStatus_Done));
				*spi_IntStatus = intstat;
#if 0//def DEBUG
	printf("sst_status1: %lx\n", reg_r32(spi_Data));
#endif
#ifdef SUPPORT_UNSUPPORTED_FLASH
				_disable();
				if (flash_operation_timer == 0)
				{
					break;
				}
				_enable();
#endif

			} while (*spi_Data & 0x1);

#ifdef SUPPORT_UNSUPPORTED_FLASH
			_disable();
			flash_operation_timer = 2; // 200 ms should be long enough to complete the operation
#endif		
			// WRDIS
			*spi_Ctrl = (SPI_START | SPI_WRITE | 0x04); 	// WRDIS
			do {
				intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
				_disable();
				if (flash_operation_timer == 0)
				{
					flash_operation_timer = 2; 
					break;
				}
				_enable();
#endif
			} while (!(intstat & SPI_IntStatus_Done));
			*spi_IntStatus = intstat;
#ifdef SUPPORT_UNSUPPORTED_FLASH
			_disable();
			if (flash_operation_timer == 0)
				break;
			_enable();
#endif
		}
#else
		u16 page_cnt = 0;
		while (size)
		{
			// if not last page, there's 256 bytes, otherwise there's less than 256 bytes
			if (size > 256)
			{
				page_cnt = 256;
				size -= 256;
			}
			else
			{
				page_cnt = size;
				size = 0;
			}
			
			if ((flash_type & 0xF0) == SST)
			{
				for (;page_cnt > 0; page_cnt--, cnt++)
				{					
					// WREN			
					*spi_Ctrl = (SPI_START | SPI_WRITE | 0x06); 	// WREN
#ifdef SUPPORT_UNSUPPORTED_FLASH
					_disable();
					flash_operation_timer = 2; // 200 ms should be long enough to complete the operation
#endif
					do {
						intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
						_disable();
						if (flash_operation_timer == 0)
							break;
						_enable();
#endif
					} while (!(intstat & SPI_IntStatus_Done));
					*spi_IntStatus = intstat;		
					
					*spi_Ctrl = SFLASH_PAGE_RESET;
					*spi_Ctrl = 0;

					// program data	
					*spi_Addr = spi_addr+cnt;
					*((u8 *)spi_Data) = *(src+cnt);
					*spi_Ctrl = (SPI_START |SFLASH_PAGE_CLEAR| SPI_WRITE| SPI_DATAV | SPI_ADDRV | 0x02);  // BYTE_PROG
					do {
						intstat = *spi_IntStatus;
					} while (!(intstat & SPI_IntStatus_Done));
					*spi_IntStatus = intstat;
					
#ifdef SUPPORT_UNSUPPORTED_FLASH
					_disable();
					flash_operation_timer = 2; // 200 ms should be long enough to complete the operation
					_enable();
#endif						
					// wait for rdy
					do {
						*spi_Ctrl = (SPI_START | SPI_READ| SPI_DATAV | (1 << SPI_CNT_SHIFT) | 0x05);  // RDSR
						do {
							intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
							_disable();
							if (flash_operation_timer == 0)
							{
								flash_operation_timer = 2;
								break;
							}
							_enable();
#endif
						} while (!(intstat & SPI_IntStatus_Done));
						*spi_IntStatus = intstat;
#if 0//def DEBUG
			printf("sst_status1: %lx\n", reg_r32(spi_Data));
#endif
#ifdef SUPPORT_UNSUPPORTED_FLASH
						_disable();
						if (flash_operation_timer == 0)
						{
							break;
						}
						_enable();
#endif

					} while (*spi_Data & 0x1);
					
#ifdef SUPPORT_UNSUPPORTED_FLASH
					_disable();
					flash_operation_timer = 2; // 200 ms should be long enough to complete the operation
#endif		
					// WRDIS
					*spi_Ctrl = (SPI_START | SPI_WRITE | 0x04); 	// WRDIS
					do {
						intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
						_disable();
						if (flash_operation_timer == 0)
						{
							flash_operation_timer = 2; 
							break;
						}
						_enable();
#endif
					} while (!(intstat & SPI_IntStatus_Done));
					*spi_IntStatus = intstat;
#ifdef SUPPORT_UNSUPPORTED_FLASH
					_disable();
					if (flash_operation_timer == 0)
						break;
					_enable();
#endif
				}				
			}
			else
			{
				// WREN			
				*spi_Ctrl = (SPI_START | SPI_WRITE | 0x06); 	// WREN
#ifdef SUPPORT_UNSUPPORTED_FLASH
				_disable();
				flash_operation_timer = 2; // 200 ms should be long enough to complete the operation
#endif
				do {
					intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
					_disable();
					if (flash_operation_timer == 0)
						break;
					_enable();
#endif
				} while (!(intstat & SPI_IntStatus_Done));
				*spi_IntStatus = intstat;
				
				*spi_Ctrl = SFLASH_PAGE_RESET;
				*spi_Ctrl = 0;
				
				*spi_Addr = spi_addr + cnt;
				for (;page_cnt > 0; page_cnt--, cnt++)
				{
					*((u8 *)spi_Data) = *(src+cnt);		
				}	
				*spi_Ctrl = SPI_START|SFLASH_PAGE_CLEAR|SPI_WRITE|SPI_ADDRV | SPI_DATAV|0x02;
#ifdef SUPPORT_UNSUPPORTED_FLASH
				_disable();
				flash_operation_timer = 2; // 200 ms should be long enough to complete the operation
				_enable();
#endif
				do {
					intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
					_disable();
					if (flash_operation_timer == 0)
						break;
					_enable();
#endif
				} while (!(intstat & SPI_IntStatus_Done));
				*spi_IntStatus = intstat;	
			}
			
#ifdef SUPPORT_UNSUPPORTED_FLASH
			_disable();
			flash_operation_timer = 2; // 200 ms should be long enough to complete the operation
			_enable();
#endif		
			// wait for rdy
			do {
				*spi_Ctrl = (SPI_START | SPI_READ| SPI_DATAV | (1 << SPI_CNT_SHIFT) | 0x05);  // RDSR
				do {
					intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
					_disable();
					if (flash_operation_timer == 0)
					{
						flash_operation_timer = 2;
						break;
					}
					_enable();
#endif
				} while (!(intstat & SPI_IntStatus_Done));
				*spi_IntStatus = intstat;
#if 0//def DEBUG
	printf("sst_status1: %lx\n", reg_r32(spi_Data));
#endif
#ifdef SUPPORT_UNSUPPORTED_FLASH
				_disable();
				if (flash_operation_timer == 0)
				{
					break;
				}
				_enable();
#endif

			} while (*spi_Data & 0x1);

#ifdef SUPPORT_UNSUPPORTED_FLASH
			_disable();
			flash_operation_timer = 2; // 200 ms should be long enough to complete the operation
#endif		
			// WRDIS
			*spi_Ctrl = (SPI_START | SPI_WRITE | 0x04); 	// WRDIS
			do {
				intstat = *spi_IntStatus;
#ifdef SUPPORT_UNSUPPORTED_FLASH
				_disable();
				if (flash_operation_timer == 0)
				{
					flash_operation_timer = 2; 
					break;
				}
				_enable();
#endif
			} while (!(intstat & SPI_IntStatus_Done));
			*spi_IntStatus = intstat;
#ifdef SUPPORT_UNSUPPORTED_FLASH
			_disable();
			if (flash_operation_timer == 0)
				break;
			_enable();
#endif
		}
#endif
#ifdef SUPPORT_UNSUPPORTED_FLASH
		_disable();
		if (flash_operation_timer == 0)
		{
			flash_type = UNKNOWN_FLASH;
			rc = 2; // unsupported flash
		}
		_enable();
#endif
		
#if 0//def DEBUG
	MSG("save finished\n");
	//spi_read_flash(WDC_NVRAM_ADDR, sizeof(wdc_nvram));
#endif
	}
	return rc;
}


void spi_read_global_nvram(void) //only read the 0x00-0x7f INITIO NVRAM part
{
	spi_read_flash((u8 *)&globalNvram, GLOBAL_NVRAM_OFFSET, 0x80);	
	MSG("spiGNV %bx %bx\n", globalNvram.Signature[0], globalNvram.Signature[1]);
	if ((globalNvram.Signature[0] != 0x25) && (globalNvram.Signature[1] != 0xC9))
	{
		load_from_nvram = 0;
	}
	else
	{
		load_from_nvram = 1;
	}
	globalNvram.Signature[0] = 0x25;
	globalNvram.Signature[1] = 0xC9;
	globalNvram.nv_Id[0] = 0x36;
	globalNvram.nv_Id[1] = 0x10;
	globalNvram.ModelId[0] = 0x3A;
	globalNvram.ModelId[1] = 0x10;
	globalNvram.validNvramLocation = 0;
//	if(!verify_chk_sum(&globalNvram, sizeof(INI_NVRAM)))	//FIX_ME
	{
		globalNvram.validNvramLocation = VALID_NVRAM_IN_SPI;
	}
}



u8	read_fw_from_flash(u32 flashBase, u32 byteCnt)
{
	// mc_buffer only 512 now, so byteCnt can not greater than 512
	if ((flashBase + byteCnt) > MAX_FW_IMAGE_SIZE_INI)
	{
		return 1;
	}
	
	xmemset(mc_buffer, 0x00, byteCnt);
	spi_read_flash(mc_buffer, flashBase, byteCnt);
	return 0;
}




void flash_write_error_handle(u32 imageOffset, u8 isPrimCopy)
{
	if (is_chip_for_wd)
	{
		if (imageOffset == 0)
		{
			return;
		}
		else
		{
			imageOffset -= WD_FW_HEADER;	//get the firmware offset.
		}
	}

	
	u32 fw_sector_offset = imageOffset;
	
	u16 fw_rewrite_size = (u16)(imageOffset & 0xFFF);

	if ((fw_sector_offset & 0xFFF) != 0)
	{
		fw_sector_offset &= 0xFF000; // alignment to 4K boundary

		u32 rewrite_spi_addr = 0;

		if (is_chip_for_wd)
		{
			if (isPrimCopy)
			{

				rewrite_spi_addr = fw_sector_offset + SPI_FW_PRIMARY;
			}
			else
			{
				rewrite_spi_addr = fw_sector_offset + SPI_FW_SECONDARY;
			}
		}
		
		else
		{
			rewrite_spi_addr = fw_sector_offset + INI_FW_OFFSET;
		}

		spi_read_flash(mc_buffer, rewrite_spi_addr, fw_rewrite_size);
		spi_write_flash(mc_buffer, rewrite_spi_addr, fw_rewrite_size);	
	}
	
}



#ifdef WDC_DOWNLOAD_MICROCODE
void create_chk_sum(u32 *addr, u8 len)
{
	u32 check_sum = 0;
	for (u8 i = 0; i < len-1; i++)
	{
		check_sum += *(addr + i);
	}
	DBG("chk_sum %lx\n", check_sum);
	*(addr + len - 1)  = 0xFFFFFFFF - check_sum + 1;	
}

/*********************
****
**** len is dword aligned
****
*********************/
u8 verify_chk_sum(u32 *addr, u8 len)
{
	u32 check_sum = 0;
	for (u8 i = 0; i < len; i++)
	{
		check_sum += *(addr + i);
		DBG("%lx\t", *(addr + i));
		if ((i & 0x03) == 0x03)
			DBG("\n");
	}
	DBG("\nchksum %lx\n", check_sum);
	if (check_sum)
		return 1;
	else
		return 0;
}

u8 verify_metadata_integrity(u8 *addr)
{
	DBG("vrf mtdata\n");
	METADATA_HEADER *header = ((METADATA_HEADER *)addr);
	// check the header's integrity
	if (verify_Crc16(addr, sizeof(METADATA_HEADER) - 2, header->header_crc))	return 1;
	// check the data payload's integrity
	addr += sizeof(METADATA_HEADER);
	if (verify_Crc16(addr, header->payload_len, header->payload_crc))	return 1;
	DBG("g mtdata!\n");
	return 0;
}



u8 check_offset_boundary(u32 offset, u32 boundary)
{
	u32 boundary_size;
	boundary_size = (1 << boundary);

	if (offset == 0)
		return 0;

	do
	{
		if (offset < boundary_size)
			return 1;

		offset -= boundary_size;
	}
	while(offset);

	return 0;
}


u32 check_fw_base_addr(u8 fw_type)
{
	#define DORMANT_FIRMWARE		0x00
	#define EXECUTING_FIRMWARE	0x01

	if (flash_size_larger_than_2Mb == 0) // when the flash is smaller than 2Mb, only primary is supported
	{
		return SPI_FW_PRIMARY;
	}
	return ((((*fw_tmp & BOOT_FROM_PRIMARY) == BOOT_FROM_PRIMARY) ^ (fw_type== DORMANT_FIRMWARE)) ? SPI_FW_PRIMARY : SPI_FW_SECONDARY);
}


/************************************************************************\
 read_nvm_raid_parms() -
 Returns:
	// returnvalue will be 0,1,2,3;
	// 0 is ok,
	// 1 is vital 0 invalid
	// 2 is vital 1 invalid
	// 3 is the two invalid
 */
u8 read_nvm_raid_parms(void)
{
	u8 returnvalue = 0;
	u16 size = sizeof(nvm_rp);
	u16 tmpsize = sizeof(nvm_rp[1]);
	xmemset(mc_buffer, 0, size);
	spi_read_flash((u8 *)&mc_buffer, WDC_RAID_PARAM_ADDR, size);	
	
	for(u8 i = 0;i < MAX_SATA_CH_NUM;i++)
	{
		if ( ((PRAID_PARMS)&mc_buffer[i*tmpsize])->header.signature == WDC_RAID_PARAM_SIG)
		{
			if (verify_metadata_integrity(&mc_buffer[i*tmpsize]) == 0)	
			{
				//verify meta data ok,it is valid.
				xmemcpy(&mc_buffer[i*tmpsize], (u8 *)&nvm_rp[i], tmpsize);
				continue;
			}
		}
		returnvalue |= (i+1);
	}
	// returnvalue will be 0,1,2,3;
	// 0 is ok,
	// 1 is vital 0 invalid
	// 2 is vital 1 invalid
	// 3 is the two invalid
	return returnvalue;
}

/************************************************************************\
read_nvm_vital_parms() -
 Returns:
	// returnvalue will be 0,1,2,3;
	// 0 is ok,
	// 1 is vital 0 invalid
	// 2 is vital 1 invalid
	// 3 is the two invalid
 */

u8 read_nvm_vital_parms(void)
{
	u8 returnvalue = 0;
	u16 size = sizeof(VITAL_PARMS);
	xmemset(mc_buffer, 0, size);
	spi_read_flash((u8 *)&mc_buffer, WDC_VITAL_PARAM_ADDR, size);	
	
	for(u8 i = 0;i < MAX_SATA_CH_NUM;i++)
	{
		u16 addr = (i==0) ? 0 : (size<<1);
		if ( ((PVITAL_PARMS)&mc_buffer[addr])->header.signature == WDC_VITAL_PARAM_SIG)
		{
			if (verify_metadata_integrity(&mc_buffer[addr]) == 0)	
			{
				//verify meta data ok,it is valid.
				xmemcpy(&mc_buffer[addr], (u8 *)&vital_data[i], size);
				continue;
			}
		}
		returnvalue |= (i+1);
	}
	// returnvalue will be 0,1,2,3;
	// 0 is ok,
	// 1 is vital 0 invalid
	// 2 is vital 1 invalid
	// 3 is the two invalid
	return returnvalue;
}

u8 read_nvm_quick_enum(void)
{	
	xmemset(mc_buffer, 0, sizeof(QUICK_ENUM));
	spi_read_flash((u8 *)&mc_buffer, WDC_QUICK_ENUM_DATA_ADDR, sizeof(QUICK_ENUM));
	if ( ((PQUICK_ENUM )&mc_buffer)->header.signature == WDC_QUICKENUM_DATA_SIG)
	{
		if (verify_metadata_integrity(mc_buffer) == 0)	
		{
			//verify meta data ok,it is valid.
			xmemcpy(mc_buffer, (u8 *)&nvm_quick_enum, sizeof(QUICK_ENUM));
			return 0; // valid
		}
	}
	return 1; // invalid
}

u8 read_nvm_prod_info(void)
{
	xmemset(mc_buffer, 0, sizeof(PROD_INFO));
	spi_read_flash((u8 *)&mc_buffer, WDC_PRODUCT_INFO_ADDR, sizeof(PROD_INFO));	
	if ( ((PROD_INFO*)&mc_buffer)->header.signature == WDC_VID_PRODUCT_INFO_SIG)
	{
		if (verify_metadata_integrity(mc_buffer) == 0)	
		{
			//verify meta data ok,it is valid.
			xmemcpy(mc_buffer, (u8 *)&prodInfo, sizeof(PROD_INFO));
			return 0; // valid
		}
	}
	return 1; // invalid
}

u8 read_nvm_unit_parms(void)
{
	xmemset(mc_buffer, 0, sizeof(UNIT_PARMS));
	spi_read_flash((u8 *)&mc_buffer, WDC_UNIT_PARAMS_ADDR, sizeof(UNIT_PARMS));	
	if ( ((UNIT_PARMS *)&mc_buffer)->header.signature == WDC_UNIT_PARAMS_SIG)
	{
		if (verify_metadata_integrity(mc_buffer) == 0)	
		{
			//verify meta data ok,it is valid.
			xmemcpy(mc_buffer, (u8 *)&nvm_unit_parms, sizeof(UNIT_PARMS));
			return 0; // valid
		}
	}
	return 1; // invalid
}

void save_nvm_unit_parms(void)
{
	nvm_unit_parms.header.signature = WDC_UNIT_PARAMS_SIG;
	nvm_unit_parms.header.payload_len = sizeof(UNIT_PARMS) - sizeof(nvm_unit_parms.header);
	nvm_unit_parms.header.payload_crc = create_Crc16((u8 *)&nvm_unit_parms.mode_save, nvm_unit_parms.header.payload_len);
	nvm_unit_parms.header.header_crc = create_Crc16((u8 *)&nvm_unit_parms.header, sizeof(METADATA_HEADER) - 2);
	// read back 
	spi_read_flash(mc_buffer, WDC_QUICK_ENUM_DATA_ADDR, WDC_UNIT_PARAMS_ADDR + sizeof(UNIT_PARMS));
	xmemcpy((u8 *)&nvm_unit_parms, &mc_buffer[WDC_UNIT_PARAMS_ADDR], sizeof(UNIT_PARMS));
	spi_write_flash(mc_buffer, WDC_QUICK_ENUM_DATA_ADDR, WDC_UNIT_PARAMS_ADDR + sizeof(UNIT_PARMS));
}

void save_nvm_quick_enum(void)
{
	nvm_quick_enum.header.signature = WDC_QUICKENUM_DATA_SIG;
	nvm_quick_enum.header.payload_len = sizeof(QUICK_ENUM) - sizeof(nvm_quick_enum.header);
	nvm_quick_enum.header.payload_crc = create_Crc16((u8 *)& nvm_quick_enum.dev_config, nvm_quick_enum.header.payload_len);
	nvm_quick_enum.header.header_crc = create_Crc16((u8 *)&nvm_quick_enum.header, sizeof(METADATA_HEADER) - 2);	

	spi_read_flash(mc_buffer, WDC_QUICK_ENUM_DATA_ADDR, WDC_UNIT_PARAMS_ADDR + sizeof(UNIT_PARMS));
	xmemcpy((u8 *)&nvm_quick_enum, &mc_buffer[WDC_QUICK_ENUM_DATA_ADDR], sizeof(QUICK_ENUM));
	spi_write_flash(mc_buffer, WDC_QUICK_ENUM_DATA_ADDR, WDC_UNIT_PARAMS_ADDR + sizeof(UNIT_PARMS));
}

void save_nvm_prod_info(void)
{
	prodInfo.header.signature = WDC_VID_PRODUCT_INFO_SIG;
	prodInfo.header.payload_len = sizeof(PROD_INFO) - sizeof(prodInfo.header);
	prodInfo.header.payload_crc = create_Crc16(prodInfo.new_VID, prodInfo.header.payload_len);
	prodInfo.header.header_crc = create_Crc16((u8 *)&prodInfo.header, sizeof(prodInfo.header) - 2);
		
	spi_write_flash((u8 *)&prodInfo, WDC_PRODUCT_INFO_ADDR, sizeof(prodInfo));
}
#endif  // WDC_ENABLE_QUICK_ENUMERATION

#ifndef INITIO_STANDARD_FW
// fetch the firmware version from the firmware header
// if power on, it will read the executing firmware's version
// if read firmware header command, it will fetch the version of the executing fw or dormant fw
void get_fw_version(u8 *src, u8 dormant_fw_flag)
{
#define CHIPS_SIGNATURE_OFFSET 	0x02
#define FIRMWARE_VERSION_OFFSET 	0x19

	// FW header 
	u32 temp32 = (((*fw_tmp & BOOT_FROM_PRIMARY) == BOOT_FROM_PRIMARY) ^ (dormant_fw_flag)) ? 0x3E00 : 0x23E00;
	if (flash_size_larger_than_2Mb == 0)
		temp32 = 0x3E00;
	u32 chip_sig, ver_tmp;
	spi_read_flash((u8 *)&chip_sig, temp32 + CHIPS_SIGNATURE_OFFSET, 0x04);
	spi_read_flash((u8 *)&ver_tmp, temp32 + FIRMWARE_VERSION_OFFSET, 0x04);	
	chip_sig = ChgEndian_32(chip_sig);
	if ((chip_sig == CHIPS_SIGNATURE) && (ver_tmp != 0))
	{// check the signature
		*((u32 *)src) = ver_tmp;
		MSG("sn v\n");
	}
	else // load hard code value value
	{
		*src = ((WDC_FIRMWARE_VERSION >> 12) & 0x0f) + '0';
		*(src+1) = ((WDC_FIRMWARE_VERSION >> 8) & 0x0f) + '0';
		*(src+2) = ((WDC_FIRMWARE_VERSION >> 4) & 0x0f) + '0';
		*(src+3) = ( WDC_FIRMWARE_VERSION  & 0x0f) + '0';
	}
	return;
}

void fwVerStr_To_fwVerHex(void)
{
	firmware_version = ((u16)(firmware_version_str[0]-'0') << 12) + ((u16)(firmware_version_str[1]-'0') << 8) + ((u16)(firmware_version_str[2]-'0') << 4) + (firmware_version_str[3]-'0');
}
#endif
