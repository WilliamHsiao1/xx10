/*****************************************************************************
 *    Copyright (C) Initio Corporation 2006-2013, All Rights Reserved
 * 
 *    This file contains confidential and propietary information
 *    which is the property of Initio Corporation.
 *
 *****************************************************************************/

#ifndef VCD_H
#define VCD_H
#undef  Ex
#ifdef VCD_C
	#define Ex
#else
	#define Ex extern
#endif

#define	TOC_FORMAT_BASIC 			0
#define	TOC_FORMAT_SESSION_INFO  1
#define	TOC_FORMAT_FULL 			2
#define	TOC_FORMAT_PMA 			3
#define	TOC_FORMAT_ATIP			4
#define	TOC_FORMAT_MASK			0x0f

/* vcd.c */
Ex u8 cd_medium_lock;

Ex u8  cd_media_present;
Ex u8  cd_unit_attentions;

#ifdef WDC_VCD
extern void vcd_start_command(PCDB_CTXT	pCtxt);
#endif
extern void vcd_read_virtual_cd_capacity_cmd(PCDB_CTXT pCtxt);
extern void vcd_read_write_virtual_cd_cmd(PCDB_CTXT pCtxt);

//extern void vcd_reset(u8 resetType);
extern void vcd_set_media_presence(u8 bPresent);


#endif	// INCLUDE_VCD_H
