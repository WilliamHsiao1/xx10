/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/


#include	"general.h"
extern void usb_main(void);

//void __no_return ram_start(void)
void ram_start(void)
{
//	move_datas_section();
	zdatas_section();
	MSG("Date: %s | Time: %s\n", __DATE__, __TIME__);
	
#ifdef DBG_FUNCTION
	*gpioCtrlFuncSel0 = (*gpioCtrlFuncSel0 & ~0x03) | UR_TXSEL;
#endif

//	MSG("------------------------------------\n");
#ifndef FPGA
	MSG("U3PL w.\n");
#if (PWR_SAVING)
	turn_on_USB23_pwr();
#endif
	while (1)
	{
		if (*chip_IntStaus & USB3_PLL_RDY)
			break;
	}
	MSG("RDY\n");
#endif

	usb_main();
}

