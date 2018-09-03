/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/


#include	"general.h"

#ifdef DBG_FUNCTION

#include <stdarg.h>

void HEX8CHAR(u8 hex)
{
	u8	print_char	= (hex & 0xF0) >> 4;

	if (print_char > 0x9)
	{
		UART_MSG(print_char - 0xA + 'A');
	} 
	else
	{
		UART_MSG(print_char + '0');
	}	

	print_char	= hex & 0xF;
	if (print_char > 0x9)	
	{
		UART_MSG(print_char - 0xA + 'A');
	}
	else
	{
		UART_MSG(print_char + '0');
	}
}


void new_pv8(u8 value)
{
	UART_FLUSH();
	UART_MSG('0');
	UART_MSG('x');
	HEX8CHAR(value);
}

void new_pv16(u16 value)
{
	u8	print8	= value >> 8;
	
	UART_FLUSH();

	UART_MSG('0');
	UART_MSG('x');
	HEX8CHAR(print8);

	print8	= value;
	HEX8CHAR(print8);

}

void new_pv32(u32 value)
{
	u8	print8	= value >> 24;
	UART_FLUSH();

	UART_MSG('0');
	UART_MSG('x');
	HEX8CHAR(print8);

	print8	= value >> 16;
	HEX8CHAR(print8);

	print8	= value >> 8;
	HEX8CHAR(print8);

	print8	= value;
	HEX8CHAR(print8);

}


void new_print(const char *format, ...)
{
	u32 t_ulong;
	char *tempPtr,*tempPtr2;
	va_list argp;
	u8 getSize;

	tempPtr = (char *)format;
	va_start(argp, format);

	UART_FLUSH();
	while (*tempPtr != '\0')
	{
		switch (*tempPtr)
		{
			case '\n':
				UART_MSG('\n');
				UART_MSG('\r');
				break;
			case '%':
				// set default to get a 16-bits value
				getSize = 2;
				tempPtr++;
REPARSER_TYPE:
				switch (*tempPtr)
				{
					case '%':
						UART_MSG('%');
						break;
					
					case 'x':
					case 'X':
//PROCESS_UPPER_X:
						if (getSize == 4)
						{
							t_ulong = va_arg(argp, unsigned long);
							pv32(t_ulong);
						}
						else if (getSize == 2)
						{
							t_ulong = va_arg(argp, unsigned int);
							pv16(t_ulong);
						}
						else
						{
							t_ulong = va_arg(argp, unsigned int);
							pv8(t_ulong);
						}
						break;
					case 'c':
					case 'C':
						t_ulong = va_arg(argp, unsigned int);
						UART_MSG(t_ulong);
						break;
					case 's':
					case 'S':
						tempPtr2 = va_arg(argp, char *);
						while (*tempPtr2 != '\0')
						{
							UART_MSG(*tempPtr2);
							tempPtr2++;
						}
						break;
					case 'B':
					case 'b':
						getSize = 1;
						tempPtr++;
						goto REPARSER_TYPE;
					case 'L':
					case 'l':
						getSize = 4;
						tempPtr++;
						goto REPARSER_TYPE;
					case '0':
						tempPtr++;	// read next
						tempPtr++;
						goto REPARSER_TYPE;
					default:
//UNKNOWN:
						UART_MSG('?');
						UART_MSG('?');
				}
				break;
			default:
				UART_MSG(*tempPtr);
		}
		tempPtr++; // next
	}
	UART_FLUSH();
	va_end(argp);
//	Delay(1);
}

#if 0
void lib_printv8(u8 value)
{
	UART_MSG(':');
	UART_MSG(' ');
	UART_MSG('0');
	UART_MSG('x');
	HEX8CHAR(value);
	UART_MSG('\n');
	UART_MSG('\r');
}

void lib_printv16(u16 value)
{
	u8	print8	= value >> 8;

	UART_MSG(':');
	UART_MSG(' ');
	UART_MSG('0');
	UART_MSG('x');
	HEX8CHAR(print8);

	print8	= value;
	HEX8CHAR(print8);

	UART_MSG('\n');
	UART_MSG('\r');
}

void lib_printv32(u32 value)
{
	u8	print8	= value >> 24;

	UART_MSG(':');
	UART_MSG(' ');
	UART_MSG('0');
	UART_MSG('x');
	HEX8CHAR(print8);

	print8	= value >> 16;
	HEX8CHAR(print8);

	print8	= value >> 8;
	HEX8CHAR(print8);

	print8	= value;
	HEX8CHAR(print8);

	UART_MSG('\n');
	UART_MSG('\r');
}
#endif

void MemDump(u8 *addr, u16 size)
{
	u16 count;
	
	count = 0;
	ERR("MemDump: addr:%LX, size:%X\n", (u32)addr, size);
	while (count < size)
	{
		ERR("%BX ", reg_r8(addr));
		count++;
		addr++;
		if ((count & 0x0F) == 0x00)
		{
			ERR("\n");
		}
	}
	ERR("\n");
}

void StrDump(u8 *addr, u16 size)
{
	u16 count;
	
	count = 0;
	ERR("StrDump: addr:%LX, size:%X\n", (u32)addr, size);
	while (count < size)
	{
		ERR("%C", reg_r8(addr));
		count++;
		addr++;
	}
	ERR("\n");
}

#ifdef DEBUG
void PrintSataInfo()
{
	DBG("\nsata_DataRxStat: %x \n", sata_ch_reg->sata_DataRxStat);
	DBG("sata_DataTxStat: %x \n", sata_ch_reg->sata_DataTxStat);
	DBG("sata_DataTcnt: %x \n", sata_ch_reg->sata_TCnt);
	sata_ch_reg->sata_SMCtrl = 1;
	DBG("sata_SMSTAT : %x\n", sata_ch_reg->sata_SMSTAT);
	dbuf_dbg(0);
}
#endif


#ifdef UART_RX
void uart_rx_init()
{
	//reset UART RX FIFO
	DBG("set UART\n");

	//Add UR_RXSEL
	*gpioCtrlFuncSel0 |= UR_TXSEL|UR_RXSEL;
	
	*periph_IntEn = UART0_INT;
	*uart0_IntEn = UART_RXINT_EN;

}

u8 C2B(u8 ch)
{
//	u8 v1, v2;
	
	if ((ch >= '0') && (ch <= '9'))
		return (ch - '0');
		
	if ((ch >= 'A') && (ch <= 'F'))
		return (ch - 'A' + 10);
	
	return 0;
}

u32 StrToU32(u8 ch[])
{
	return ( (C2B(ch[0]) << 28) | (C2B(ch[1]) << 24) | (C2B(ch[2]) << 20) | (C2B(ch[3]) << 16) |
	(C2B(ch[4]) << 12) | (C2B(ch[5]) << 8) | (C2B(ch[6]) << 4) | C2B(ch[7]));
}

u16 StrToU16(u8 ch[])
{
	return ((C2B(ch[0]) << 12) | (C2B(ch[1]) << 8) | (C2B(ch[2]) << 4) | C2B(ch[3]));
}

u16 StrToU8(u8 ch[])
{
	return ((C2B(ch[0]) << 4) | C2B(ch[1]));
}

void uart_rx_parse(u8 buf[])
{
	u32 addr32;
	u8 addr8;
	u8 type8;
	u32 value32;
	u8 value8;
	
	//StrDump(buf, 20);
	//MemDump(buf, 20);
	
	if ((buf[0] == 'H') && (buf[1] == 'E') && (buf[2] == 'L') && (buf[3] == 'P'))
	{
		ERR("--------------------------------\n");
		ERR("| Memory Read:                 |\n");
		ERR("| MR $ADDR                     |\n");
		ERR("| ex: MR 40050016              |\n");
		ERR("|  --------------------------  |\n");
		ERR("| Memory Write:                |\n");
		ERR("| MW $ADDR $VALUE              |\n");
		ERR("| ex: MW 40050016 00001234     |\n");
		ERR("|  --------------------------  |\n");
		ERR("| PHY Read                     |\n");
		ERR("| PR $TAG &ADDR                |\n");
		ERR("| ex: PR 01 08                 |\n");
		ERR("|  --------------------------  |\n");
		ERR("| PHY Write                    |\n");
		ERR("| PW $TAG $ADDR $VALUE         |\n");
		ERR("| ex: PW 01 08 02              |\n");
		ERR("|  --------------------------  |\n");
		return;
	}
	
	//if not a Memory/Phy access
//	if ((buf[0] != 'A') && (buf[0] != 'R') && (buf[0] != 'D') && (buf[0] != 'C'))
	if ((buf[0] != 'A') && (buf[0] != 'R') && (buf[0] != 'D') && (buf[0] != 'F'))
	{
		if (  (buf[0] != 'M') && (buf[0] != 'P'))
		{
			DBG("not M/P\n");	
			goto rx_p_fail2;
		}
		
		//if dir CH not R/W
		if (  (buf[1] != 'R') && (buf[1] != 'W') && (buf[1] != 'B') && (buf[1] != 'M'))
		{
			DBG("not R/W\n");
			goto rx_p_fail2;
		}
	}
		
	if (buf[0] == 'M')
	{
		if (buf[1] == 'R')
		{
			addr32 = StrToU32(&buf[3]);
			MSG("MR %LX\n", addr32);
			MSG(":%LX\n", reg_r32(addr32));
			MSG("--\n");
		}
		else if (buf[1] == 'W')
		{
			addr32 = StrToU32(&buf[3]);
			value32 = StrToU32(&buf[12]);
			MSG("MW %LX %LX\n", addr32, value32);
			reg_w32(addr32, value32);
			MSG("--\n");
		}
		else if (buf[1] == 'B')
		{
			addr32 = StrToU32(&buf[3]);
			value8 = StrToU8(&buf[12]);
			MSG("MW %LX %BX\n", addr32, value8);
			reg_w8(addr32, value8);	
			MSG("--\n");
		}
	}
	else if (buf[0] == 'P')
	{
		if (buf[1] == 'R')
		{
			type8 = StrToU8(&buf[3]);
			addr8 = StrToU8(&buf[6]);
			MSG("PR %BX %BX\n", type8, addr8);
			MSG(":%BX\n", spi_phy_rd(type8, addr8));
			MSG("--\n");
		}
		else if (buf[1] == 'W')
		{
			type8 = StrToU8(&buf[3]);
			addr8 = StrToU8(&buf[6]);
			value8 = StrToU8(&buf[9]);
			MSG("PW %BX %BX %BX\n", type8, addr8, value8);
			spi_phy_wr(type8, addr8, value8);
			MSG("--\n");
		}	
#ifdef SATA_PM_TEST	
		else if (buf[1] == 'M')  // sata PM
		{
			sata_ch_reg = sata_Ch0Reg;

			if (buf[2] == 'R')
			{
				MSG("sata phy ctrl: %lx\n", sata_ch_reg->sata_PhyCtrl);
				MSG("sata phy stat: %x\n", sata_ch_reg->sata_PhyStat);
			}
			else if (buf[2] == 'W')
			{
				MSG("manual wakeup\n");
				if (sata_ch_reg->sata_PhyStat & PMPRTL)
					sata_ch_reg->sata_PhyCtrl &= ~PHYPRTL;
				else if (sata_ch_reg->sata_PhyStat & PMSLMBR)
					sata_ch_reg->sata_PhyCtrl &= ~PHYSLMBR;
				else
					MSG("sata phy is not in P or S mode: %x\n", sata_ch_reg->sata_PhyStat);
			}
			else if (buf[2] == 'P')
			{
				if (sata_ch_reg->sata_PhyStat & PHYRDY)
				{
					MSG("partial\n");
					sata_ch_reg->sata_PhyCtrl |= PHYPRTL;
				}
				else
					MSG("sata PHY is not RDY\n");
			}
			else if (buf[2] == 'S')
			{
				if (sata_ch_reg->sata_PhyStat & PHYRDY)
				{
					MSG("slumber\n");
					sata_ch_reg->sata_PhyCtrl |= PHYSLMBR;
				}
				else
					MSG("sata PHY is not RDY\n");
			}
		}
#endif		
	}
	else if (buf[0] == 'A')
	{
		PCDB_CTXT pCtxt = NULL;
		pCtxt = usb_ctxt.curCtxt;			
		dump_all_regs(pCtxt);
	}
	else if (buf[0] == 'D')
	{
		if (buf[1] == 'P')
			dump_phy_regs();
		else if (buf[1] == 'T')
			dbuf_dbg(DBUF_SEG_DUMP_TXSEG1234);
		else if (buf[1] == 'R')
			dbuf_dbg(DBUF_SEG_DUMP_RXSEG1234);
	}
	else if (buf[0] == 'S')
	{
		if (buf[1] == '0')
		{
			MSG("Hrst S0\n");
			sata0Obj.pSata_Ch_Reg = sata_Ch0Reg;
			sata_HardReset(&sata0Obj);
		}
		if (buf[1] == '1')
		{
			MSG("Hrst S1\n");
			sata1Obj.pSata_Ch_Reg = sata_Ch1Reg;
			sata_HardReset(&sata1Obj);
		}
	}
#if 0
	else if (buf[0] == 'C')
	{
		MSG("%lx %lx\n", *cpu_wake_up, *chip_IntStaus);
	}
#endif
#if FAN_CTRL_EXEC_TICKS
	else if (buf[0] == 'F')  // fan & temperature sate machine dbg
	{
		if (buf[1] == 'G')
		{
			type8 = StrToU8(&buf[3]);
			setFan(0, type8);
		}
		else if (buf[1] == 'T')
		{
			type8 = StrToU8(&buf[3]);
			unit_wide_cur_temperature = type8;
			MSG("c_temp %bx\n", unit_wide_cur_temperature);
		}
		else if (buf[1] == 'F')
		{
			type8 = StrToU8(&buf[3]);
			setFan(0xff, type8);
		}
		else if (buf[1] == 'S')
		{
			type8 = StrToU8(&buf[4]);

			if (buf[2] == 'B')
			{				
				*gpioCtrlFuncSel0 &= ~WD_FAN_PWM_SEL;
				*led4_Ctrl1 &= ~LED_ENABLE;
				//*led4_Ctrl0 = (*led4_Ctrl0 & ~0x000000FF) | (type8); // (dark+bright): ((fan&0xf0 >> 4) + fan&0x0f) * 655.36, such as 0xBB, 0x22 * 655.36 = 14.4ms per cycle

				Delay(100);
				*gpioCtrlFuncSel0 = (*gpioCtrlFuncSel0 & ~0xC0) | WD_FAN_PWM_SEL;

				*led4_Ctrl0 = (*led4_Ctrl0 & ~0xFFFF0000) | ((WDC_FAN_TIMER_UNIT << 24));
				*led4_Ctrl0 = (*led4_Ctrl0 & ~0x000000FF) | (type8); // (dark+bright): ((fan&0xf0 >> 4) + fan&0x0f) * 655.36, such as 0xBB, 0x22 * 655.36 = 14.4ms per cycle
				// enable LED PWM mode, PWM forever, dir: bidirection, period 250(350 * 163.84us = 4.096ms). 
				*led4_Ctrl1 =  (*led4_Ctrl1 | LED_ENABLE) & ~LED_PWM_MODE;

				MSG("*B led4_Ctrl0 %lx\n", *led4_Ctrl0);
			}
			else if (buf[2] == 'P')
			{			
				*gpioCtrlFuncSel0 &= ~WD_FAN_PWM_SEL;
				*led4_Ctrl1 &= ~LED_ENABLE;

				Delay(100);
				*gpioCtrlFuncSel0 = (*gpioCtrlFuncSel0 & ~0xC0) | WD_FAN_PWM_SEL;
				*led4_Ctrl0 = (*led4_Ctrl0 & ~0xFF000000) | ((0x10 << 24));
				*led4_Ctrl0 = (*led4_Ctrl0 & ~0x000000FF) | (type8);

				*led4_Ctrl1 |=  (LED_ENABLE|LED_PWM_MODE);

				MSG("*P led4_Ctrl0 %lx\n", *led4_Ctrl0);
			//	*gpioDataOut |= GPIO3_PIN;
			}
			else
			{
				MSG("*led4_Ctrl0 %lx\n", *led4_Ctrl0);
				MSG("*led4_Ctrl1 %lx\n", *led4_Ctrl1);
				MSG("*per_clock_select %lx\n", *cpu_clock_select & (PD_PER |PERCLK_DIV));
				MSG("Tach C %lx, AC %lx\n", gpio_pulse_count, gpio_accumlated_pulse_count);
			}
		}
		else
		{
			dumpFan();
		}
		
	}
#endif	
	else //if (buf[0] == 'R')
	{
		if (buf[1] == 'L')
		{
			dbg_next_cmd = 1;
		}
		//else if (buf[1] == 'M')
		//{
		//	dbg_next_cmd = 0;
		//}
		else if ((buf[1] == 'V') || (buf[1] == 'D') ||(buf[1] == 'K'))
		{
			if (buf[1] == 'V')
			{
				raid_mirror_status = RAID1_STATUS_VERIFY;
				raid_mirror_operation = RAID1_VERIFY;
				rebuildLBA = 0;
			}
			else if (buf[1] == 'D')
			{
				raid_mirror_status = 0;
				//raid_mirror_operation = 0;
				//rebuildLBA = 0;
			}

			MSG("rms: %bx rmo: %bx rrvs %bx rblba %lx %lx\n", raid_mirror_status, raid_mirror_operation, raid_rebuild_verify_status,
				(u32)(rebuildLBA>>32), (u32)rebuildLBA);
		}
		else if (buf[1] == 'S')
		{
#ifdef ERR_RB_DBG
			if (buf[2] == 'S')
			{
				//for test: close test 0, Good case 1, both fail 2, r fail 3, w fail 4
				type8 = StrToU8(&buf[3]);
				err_rb_dbg = type8;
				if (err_rb_dbg > 0x06)
					err_rb_dbg = CLOSE;
				MSG("err case %bx\n", err_rb_dbg);
			}
			else
#endif				
			{
				MSG("am %bx as %bx %bx nVol %bx ss %bx %bx\n", arrayMode, array_status_queue[0], array_status_queue[1], num_arrays, cur_status[SLOT0], cur_status[SLOT1]);
				MSG("rr0\n");
				MSG ("s0: am %bx dn %bx ss %bx %bx rgn %lx\n", ram_rp[0].array_mode, ram_rp[0].disk_num, ram_rp[0].disk[0].disk_status, ram_rp[0].disk[1].disk_status, ram_rp[0].raidGenNum);
				MSG("rr1\n");
				MSG ("s1: am %bx dn %bx ss %bx %bx rgn %lx\n", ram_rp[1].array_mode, ram_rp[1].disk_num, ram_rp[1].disk[0].disk_status, ram_rp[1].disk[1].disk_status, ram_rp[1].raidGenNum);
			}
		}
		else
		{
			MSG("LBad Cnt: Tx %lx, Rx %lx\n", (u16)((*usb_U3LinkBadCount & TXLINK_LBAD_TOTAL)>>16), (u16)(*usb_U3LinkBadCount & RXLINK_LBAD_TOTAL));
			MSG("rConfig %lx\n", *raid_config);
		}	
	}
	return;

rx_p_fail2:
	//StrDump(buf, 20);
	//MemDump(buf, 20);	
	MSG("-xx-\n");
}
#endif //UART_RX


#endif //DBG_FUNCTION

#ifdef ARC_GCC
void move_datas_section(void)
{
	extern void _datas_section_lma_start;
	extern void _datas_section_vma_start;
	extern void _datas_section_lma_end;
	u8 *src = &_datas_section_lma_start;
	u8 *dst = &_datas_section_vma_start;
	const u8 * const end = &_datas_section_lma_end;

	for ( ; src < end; ) {
//		UART_MSG('a');
		*dst++ = *src++;
	}

	src = &_datas_section_lma_start;
	dst = &_datas_section_vma_start;

	for ( ; src < end; src++, dst++) {
		if (*src != *dst) {
			cpu_halt();
		}
	}
}
void zdatas_section(void)
{
	extern void _zdatas_section_vma_start;
	extern void _zdatas_section_vma_end;
	u8 *start = &_zdatas_section_vma_start;
	const u8 * const end = &_zdatas_section_vma_end;

	while (start < end) {
//		UART_MSG('z');
		*start++ = 0;
	}
}
#else
void move_datas_section(void)
{
u32 i;
	extern char _code_end[];
	extern char _datas_segment_start[];
	extern char _datas_segment_end[];
	//u32 volatile *src = (u32 volatile *)LOAD_DATAS_BASE;
	u8 volatile *src = (u8 volatile *)_code_end;
	u8 volatile *dst = (u8 volatile *)VIRTUAL_DATAS_BASE;
	u32 data_size = _datas_segment_end - _datas_segment_start;

	for ( i = 0; i < data_size; i++) {
//		UART_MSG('a');
		*dst++ = *src++;
	}

	//src = (u32 volatile *)LOAD_DATAS_BASE;
	src = (u8 volatile *)_code_end;
	dst = (u8 volatile *)VIRTUAL_DATAS_BASE;
	
	for ( i = 0; i < data_size; src++, dst++, i++) {
		if (*src != *dst) {
//			UART_MSG('H');
//			UART_MSG('A');
			cpu_halt();
		}
	}
}
void zdatas_section(void)
{
	extern char _zdatas_segment_start[];
	extern char _zdatas_segment_end[];
	u8 *start = (u8 *)_zdatas_segment_start;
	const u8 * const end = (u8 *)_zdatas_segment_end;

	while (start < end) {
//		UART_MSG('z');
		*start++ = 0;
	}
}

#endif



void spi_phy_wr(u8 phy_type, u8 phy_reg, u8 write_value)
{
	u32	reg_value;

	reg_value	= 
		PHY_CMD_WRITE | 
		(phy_type << PHY_SEL_OFFSET) |
		(phy_reg << PHY_ADDR_OFFSET) | 
		(write_value);

	reg_w32(
		PHY_SPI_CTRL, reg_value
	);

	do {
		reg_value = reg_r32(PHY_SPI_CTRL);
	} while ((reg_value&BIT_31) == 0 );
}

u8 spi_phy_rd(u8 phy_type, u8 phy_reg)
{
	u32	reg_value;

	reg_value	= 
		PHY_CMD_READ | 
		(phy_type << PHY_SEL_OFFSET) |
		(phy_reg << PHY_ADDR_OFFSET)|
		0xFF;

	reg_w32(
		PHY_SPI_CTRL, reg_value
	);

	do {
		reg_value = reg_r32(PHY_SPI_CTRL);
	} while ((reg_value&BIT_31) == 0 );
	
	return 	(u8)(reg_value & 0xFF);

}

void spi_phy_wr_retry(u8 tag, u8 add, u8 val)
{
u8 i, temp8;

	for (i=0; i < 3; i++)
	{
		spi_phy_wr(tag, add, val);

		temp8 = 	spi_phy_rd(tag, add);
		if (temp8 == val)
			return;
	}

    DBG("***SPI write fail:%B %BX\n", tag, add);
}

u8 verify_Crc16(u8 *addr, u16 len, u16 crcValue)
{
	u16 val;
	*crc_Ctrl = 0;
	*crc_Ctrl = CRC_En;
	while (len--)	
	{
		*((u8 volatile *)crc_DATA) = *addr++;
	}

	*((u8 volatile *)crc_DATA) = (u8)(crcValue>>8);
	*((u8 volatile *)crc_DATA) = (u8)(crcValue);
	*crc_DATA; // make sure the all crc data is filled in, and flushed
	val = *((u16 volatile *)crc_DATA);
	*crc_Ctrl = 0;
	if (val == 0) 
		return 0; //ok
	else 
		return 1;// fail
}

u16 crc16(u8 *ptr, u32 len)
{
	*crc_Ctrl = CRC_En;
	while (len--)	
	{
		*((u8 volatile *)crc_DATA) = *ptr++;
		*crc_DATA; // make sure the all crc data is filled in, and flushed 
	}

	return	*((u16 volatile *)crc_DATA);
}

u16 create_Crc16(u8 *addr, u16 len)
{
	u16 val;
	*crc_Ctrl = 0;
	*crc_Ctrl = CRC_En;
	while (len--)	
	{
		*((u8 volatile *)crc_DATA) = *addr++;
	}
	*crc_DATA; // make sure the all crc data is filled in, and flushed 
	val = *((u16 volatile *)crc_DATA);
	*crc_Ctrl = 0; // stop CRC engine
	return val;
}
