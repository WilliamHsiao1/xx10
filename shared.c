/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/
 
#define SHARED_C


#include	"general.h"

#if 1//def INITIO_STANDARD_FW
static const u32 clkSetting[] = {
		192000000, 160000000, 137142857, 120000000, 106666666, 96000000, 87272727, 80000000,
		64000000, 48000000, 32000000, 16000000, 8000000, 4000000, 2000000, 1000000, 30000000};
#else
static const u32 clkSetting[] = {
		30000000, 500000000, 375000000, 300000000, 250000000, 214290000, 187500000, 166670000,
		150000000, 136360000, 125000000, 115380000, 107140000, 100000000, 93750000, 88230000};
#endif

#ifndef FPGA
void Delay(u16 time)
{
	//1000: 1 sec
	u32 i;
	u32 j;
	for (i = 0; i < time; i++)
	{
		for (j = 0; j < 22713 ; j++) //137Mhz
		{
		}
	}
}
void Delayus(u16 time)
{
	//1000: 1 sec
	u32 i;
	u32 j;
	for (i = 0; i < time; i++)
	{
		for (j = 0; j < 38 ; j++) //137Mhz
		{
		}
	}
}
#else // in PFGA, the clock is 50MHz
void Delay(u16 time) 
{
	//1000: 1 sec
	u32 i;
	u32 j;
	for (i = 0; i < time; i++)
	{
		for (j = 0; j < 8267 ; j++)
		{
		}
	}
}
void Delayus(u16 time)
{
	//1000: 1 sec
	u32 i;
	u32 j;
	for (i = 0; i < time; i++)
	{
		for (j = 0; j < 9 ; j++)
		{
		}
	}
}
#endif

void memcpySwap16(u8 *dest, u8 *src, u32 n16)
{
	u32	i;
	u8 tmp8;

	for (i = 0; i < n16; i++)
	{
		tmp8 = src[i * 2];
		dest[i * 2] = src[i * 2 + 1];
		dest[i * 2 + 1] = tmp8;
	}
}

void xmemset(u8 *ptr, u8 chr, u32 count)
{
#if 0
	u32 i;
	for (i = 0; i < count; i++)
		*ptr++ = chr;
#else
	while (count--) 
	{
		*ptr++ = (char)chr;
	}
#endif
}

#if 0
void xmemsetdw(u32 addr, u32 value, u32 count)
{
	UART_MSG('+');
	while (count--) 
	{
		reg_w32(addr, value);
		addr	+= 4;
	}
	UART_MSG('-');
}
#endif

u8 xmemcmp(u8 *src, u8 *dest, u32 count)
{
	u32	i;

	for (i = 0; i < count; i++)
	{
		if (*src++ != *dest++)
			return 1;
	}
	return 0;
}

void xmemcpy(u8 *src, u8 *dest, u32 size)
{
	u32 i;
	for (i = 0; i < size; i++)
	{
		*dest++ = *src++;
	}
	return;
}

void xmemcpy32(u32 *src, u32 *dest, u32 size)
{
	u32 i;
	for (i = 0; i < size; i++)
	{
		*dest++ = *src++;
	}
	return;
}

u8 Hex2Ascii(u8 hex)
{
	if ((hex & 0x0F) > 0x09)
	{
		return ((hex & 0x0F) -10 + 'A');
	}
	else
		return ((hex & 0x0F) + '0');
}

u16 codestrlen(u8 *str)
{
u8 *start = str;

	while (*str) str++;

	return  str - start;
}

u8 str_empty(u8 *str, u16 len)
{
	for (int i = 0; i < len; i++)
	{
		if (*str++ != 0)
			return 1;
	}
	return 0;
}

/************************************************************************\
 Checksum16() - Computes an 8-bit Fletcher Checksum;
                returns two 8-bit accumlators (sums) as a 16-bit integer.

 Arguments:
	dp			pointer to data buffer in   space
	len			number of bytes to sum; WARNING: maximum 255 bytes!

 Returns:
	the two 8-bit Fletcher sums in one 16-bit integer.
 */
u16 Checksum16(const u8   *dp, u8 len)
{
u8 a, b;

	// This is the 8-bit Fletcher Checksum algorithm described in RFC 1146
	// (It's called 8-bit but returns two 8-bit accumlators.)

	a = b = 0;
	for(;  len;  dp++, --len)
	{
		a += *dp;
		b += a;
	}

	return  ((u16)a << 8) | b;
}

void CrSetModelText(u8 *buf)
{
	xmemcpy(globalNvram.modelText, buf, 16);
	return;
}


void CrSetVendorText(u8 *buf)
{
	xmemcpy(globalNvram.vendorText, buf, 16);
	return;
}
#ifdef INTERRUPT
 void init_cpu_interrupt()
{	//enable interrupt mask
	DBG("CHIP_IntEn:%LX, chip_IntStaus:%LX, usb_IntEn:%LX\n", *chip_IntEn, *chip_IntStaus, *usb_IntEn);
	sata_ch_reg = sata_Ch0Reg;

	*usb_IntStatus = 0xFFFFFFFF;
	*usb_IntEn = 0;
	
#ifdef UART_RX
	uart_rx_init();
	*chip_IntEn = PERIPH_INTEN;
#endif //UART_RX

	auxreg_w(REG_INT_VECTOR_BASE, 0); 
	auxreg_w(REG_STATUS32_L2, INT_MASK); 
	auxreg_w(REG_AUX_IRQ_LEV, 0xFFF8);

	cpu_intEnable();
}
// the timer 0 interrupt is trigger after timer, the timer is setted according to the CPU clock
// 1000 -> 1000ms
void init_timer0_interrupt(u32 timer, u32 clock_setting)
{
    	_disable();                  // Turn off interrupts
	auxreg_w(REG_TIMER_BUILD, TIMER0);
	
	auxreg_w(REG_CONTROL0, 0x0);					// Disable Timer0
#ifdef FPGA
	u32 tmp32 = 44120000 / 1000;
#else
#ifdef CPU_CLOCK_DIV2
	u32 tmp32 = clkSetting[clock_setting] / 2000;
#else
	u32 tmp32 = clkSetting[clock_setting] / 1000;
#endif
#endif

	u32 reg_limit = tmp32 * timer;
	DBG("reg_limit %lx\n", reg_limit);
	auxreg_w(REG_LIMIT0, reg_limit);					

	auxreg_w(REG_CONTROL0, CONTROL_IE |CONTROL_NH);	//CONTROL_NH Enable the timer interrupt

	auxreg_w(REG_COUNT0, 0);						// set initial value and restart the timer
	timer = 0;
    	_enable();                  // Enable the timer's interrupt level
}

// 1000 -> 1000us
void init_timer1_interrupt(u32 timer, u32 clock_setting)
{
    	_disable();                  // Turn off interrupts
	auxreg_w(REG_TIMER_BUILD, TIMER1);
	auxreg_w(REG_CONTROL1, 0x0);					// Disable Timer0
#ifdef FPGA
	u32 tmp32 = 50000000 / 1000;
#else
#ifdef CPU_CLOCK_DIV2
	u32 tmp32 = clkSetting[clock_setting] / 2000;
#else
	u32 tmp32 = clkSetting[clock_setting] / 1000;
#endif
#endif
	u32 reg_limit = (tmp32 * timer ) / 1000;  // convert ms to us
	DBG("reg_limit1 %lx\n", reg_limit);
	auxreg_w(REG_LIMIT1, reg_limit);	
	auxreg_w(REG_CONTROL1, CONTROL_IE |CONTROL_NH);	//CONTROL_NH Enable the timer interrupt
	auxreg_w(REG_COUNT1, 0);						// set initial value and restart the timer
    	_enable();                  // Enable the timer's interrupt level
}
#endif

void set_uart_tx(u32 clock_setting)
{
	
	*uart0_Ctrl = UART_ENABLE | (clkSetting[clock_setting] / CONFIG_UART_DBG_BAUDRATE);
}

void reset_clock(u8 clock_setting) // set the CPU & DBUF clock
{
// use the 4bits of clock_setting		
	u32 cpu_clk_select = *cpu_clock_select;
	u8 tmp8;
	// set the clock of DBUF & CPU	
	DBG("CPU & DBUF clock: %bx\n", tmp8);
	cpu_clk_select = (cpu_clk_select & 0xFFFFF000) | ((PERIPH_CLOCK & 0x0F) << 8) | ((AES_CLOCK & 0x0F) << 4) | (clock_setting & 0x0F);
	
	UART_FLUSH();
#ifndef FPGA
	*cpu_clock_select = cpu_clk_select;
	tmp8 = ((u8)(*cpu_clock_select >> 8)) & 0x0F;
#ifdef DBG_FUNCTION
	set_uart_tx(tmp8);
#endif
#endif

	MSG("CPU clock:%LX\n", *cpu_clock_select);
}

void do_ASIC_reset(void)
{
	MSG("ASIC Reset\n");
	*cpu_SCRATCH2_reg &= ~ASIC_RESET_NOT_PERFORMED;
	*cpu_Clock |= ASIC_RESET; // HW will generate a PORT
	Delay(1); // suppose that cpu should be restarted by ASIC_RESET
}
