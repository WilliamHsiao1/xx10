/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *
 *
 *****************************************************************************
 * 
 * 3610		2010/04/09	Odin		Initial version
 * 3610		2010/04/27	Odin		USB2.0 BOT Debugging
 *
 *****************************************************************************/

#ifndef BOT_H
#define BOT_H


extern void bot_device_no_data(PCDB_CTXT pCtxt);
extern void bot_device_data_in(PCDB_CTXT pCtxt, u32 byteCnt);
extern void bot_device_data_out(PCDB_CTXT pCtxt, u32 byteCnt);

//extern void bot_usb_bus_reset();



extern void usb_bot();
extern void bot_init();
extern void set_csw_run(PCDB_CTXT pCtxt);
#endif

