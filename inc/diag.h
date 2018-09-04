/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2010-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *
 *****************************************************************************/
#ifndef INCLUDE_DIAG_H
#define INCLUDE_DIAG_H


#undef  Ex
#ifdef DIAG_C
	#define Ex
#else
	#define Ex extern
#endif
// Temperature Conditions. This must match the definition of
// the same-named field in the Temperature Condition diagnostic page.
enum {
	TC_NOMINAL			= 0,
	TC_WARM				= 1,
	TC_DANGER			= 2,
	TC_SHUTDOWN			= 3
};

// temperature TRANSITION POINTS
#if 1
enum {
	TP_LOWDOWN		= 50,
	TP_LOWUP		= 60,
	TP_HIDOWN		= 65,
	TP_HIUP			= 70
};
#else
enum {
	TP_LOWDOWN		= 45,
	TP_LOWUP		= 45,
	TP_HIDOWN		= 45,
	TP_HIUP			= 45
};
#endif

// Use this temperature value if the HDD does not report its
// temperature or an error occurs while trying to read it.
#define ASSUMED_TEMPERATURE_IF_NA		60

// Self-test Log
//
typedef struct _diag_test_log
{
	u8 para_code[2];
	u8 ctrl_bits;
	u8 para_legth;
	u8 self_tests;
	u8 self_test_no;
	u8 time_stamp[2];
	u8 address[8];
	u8 sense_key;
	u8 asc;
	u8 ascq;
	u8 vendor_specific;
} DIAG_TEST_LOG;


/* diag.c */
Ex DIAG_TEST_LOG   diag_test_log[2];// 0 is channel 0;1 is channel 1
Ex u8   diag_drive_selected;
Ex u8 unit_wide_cur_temperature; 

extern void diag_check_hdd_temperature(void);
extern void diag_receive_diag_results_cmd(PCDB_CTXT	pCtxt);
extern void diag_send_diagnostic_cmd(PCDB_CTXT pCtxt);
extern void diag_log_sense_cmd(PCDB_CTXT pCtxt);
extern void diag_log_select_cmd(PCDB_CTXT pCtxt);

extern u8 get_smart_data(u8 sata_ch_flag);
extern u8 get_temperature(void);

#endif	// INCLUDE_DIAG_H

// EOF
