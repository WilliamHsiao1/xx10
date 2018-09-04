/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 1998-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *
 *******************************************************************************/

#ifndef HANDY_H
#define HANDY_H

#undef  Ex
#ifdef HANDY_C
	#define Ex
#else
	#define Ex extern
#endif
#ifdef WDC_HANDY
extern void handy_read_handy_capacity_cmd(PCDB_CTXT	pCtxt); 
extern void handy_read_write_store_cmd(PCDB_CTXT pCtxt);
#endif
#endif