/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/

#include	"general.h"

#ifdef INTERRUPT

_Interrupt2 void isr_memerr()
{
	u32 ilink2 = _core_read(30);
#ifdef DBG_FUNCTION
	u32 cpu_clock_temp = *chip_IntStaus;
	u8 connect_maganeStatus = (u8)((*usb_IntStatus_shadow & 0xFF000000) >> 24);
#endif
	MSG("Mem err\n");
	MSG("ilk2: %lx\n", ilink2);
#ifdef DBG_FUNCTION
	MSG("%lx, %bx\n", cpu_clock_temp, connect_maganeStatus);
#endif
	//*cpu_Clock |= ASIC_RESET;
	while (1)
	{
	}
}


_Interrupt2 void isr_inserr()
{
	u32 ilink2 = _core_read(30);
	ERR("Instr error\n");
	ERR("ilk2: %lx\n", ilink2);
	while (1)
	{
	}
}
#endif
