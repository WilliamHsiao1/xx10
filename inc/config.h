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
 * config.h		2010/09/09	Winston
 *
 *****************************************************************************/


#ifndef CONFIG_H
#define CONFIG_H


// ++++++++++++++++++++++++++++++++++++++++++++++ //
// +++++++++++++++++++ CONFIG +++++++++++++++++++ //
// ++++++++++++++++++++++++++++++++++++++++++++++ //

//#define SN_DBG
//#define	FORCE_NO_SATA3
//#define DBG_FUNCTION
//#define UART_RX

#define DB_CLONE_LED
#define DK101_IT1

#define DATASTORE_LED   //for Datastore case LED Spec.
#define INITIO_STANDARD_FW
#ifdef INITIO_STANDARD_FW
#define HDR2526
#ifdef HDR2526
#define HDR2526_LED
#define HDR2526_BUZZER		// 2800Hz
#define DATABYTE_RAID
//#define SUPPORT_RAID0
//#define SUPPORT_RAID1
//#define SUPPORT_SPAN  // just hardware_raid && no AES now
//#define HDR2526_RM		// for removable disk
//#define HDR2526_GPIO_HIGH_PRIORITY
#define WRITE_PROTECT
#define SCSI_HID
/********  HDR2526_LED & HDR2526  ****************
1        HDD1 LED display( GPIO 11 )  
2        HDD2 LED display ( GPIO 1)  // be careful to use UART_RX
3        RAID Setting ( GPIO5/GPIO6 )
4        RAID Mode LED Display ( GPIO10/GPIO9/GPIO8 )
5        Write protect ( GPIO 4 )
6        Clone button ( GPIO 17)
7        Reset button ( GPIO7)
8	   Fail Buzzer (GPIO 12)	
 
9        Fan control ( GPIO3)  // It's only used in the Initio standard function
10        Power control (GPIO9/GPIO8 )  // It's only used in the Initio standard function
******************************************************/
#endif

//#define AES_EVA
#define SAVE_RAID_IN_FLASH_ONLY
#define SUPPORT_HR_HOT_PLUG
//#define POWER_BUTTON
#define AUTO_REBUILD
#define HARDWARE_RAID
#define HDD_CLONE	// just work in JBOD offline mode: curren clone button is GPIO17, it's confilct with FAN tac in GPIO
#ifdef HDR2526_RM
//#define RMB_SWITCH  // FOR TEST
#define RMBDISK
#define HOST_4K	// for 4k block case: RMBDISK should set it for big drive
#endif
//#define WDC_VCD
#define SUPPORT_CACHING_MODE_PAGE
#else
#define WDC_SES
#define WDC_VCD
#define WDC_HANDY
#define WDC_RAID
#define SUPPORT_CACHING_MODE_PAGE
#define WDC_CHECK
#endif
//#define RX_DETECTION
#define LINK_RECOVERY_MONITOR
//#define RAID_ERROR_TEST

//#define ADJUST_USB_TX_SSC
//#define RESIDUE_LEN_DEBUG
#define UAS
//#define INTERNAL_TEST
//#define REJECT_U1U2
//#define LONG_CABLE
#define RAID
#define AES_EN
//#define DBG_AES
//#define UNCERTAIN_AES
//#define DEBUG_USB3_ERR_CNT 

//#define USEGPIO2
#define USB2_L1_PATCH

#define WDC_DOWNLOAD_MICROCODE

//#define WIN8_UAS_PATCH

#define USB3_BURST_PACKETS_8 
#define BEST_CLOCK CLOCK_160M
#define CPU_CLOCK_UAS CLOCK_160M // use higher speed CPU for UAS for better performace on random r/w
#define CPU_CLOCK_BOT CLOCK_137M // use lower speed CPU for BOT to comsume less power
#define DBUF_CLOCK	CLOCK_137M
#define AES_CLOCK	CLOCK_192M
#define PERIPH_CLOCK CLOCK_64M
#define DBUF_CLOCK_USB2 CLOCK_64M
//#define CPU_CLOCK_DIV2

//#define SUPPORT_UNSUPPORTED_FLASH
//#define FPGA
#ifdef DBG_FUNCTION
//#define JTAG_DBG
//#define ERR_RB_DBG  		// for test: the variable "err_rb_dbg"'s vaule: 
//#define RB_DONE_AT_ONCE_DBG   // for rebuild done ASAP
//#define CL_DONE_AT_ONCE_DBG	// for clone done ASAP
//#define LEDS
#endif
#define INI_SCSI_DBG		
//#define SLOW_INSERTION
//#define HW_QULIFY_POLLING

#ifdef FPGA
//#define LA_DUMP
#endif

#define INTERRUPT
#define INIC_3610
#define SCSI_COMPLY

#define PWR_SAVING_LEVEL0  	  0
#define PWR_SAVING_LEVEL1    1		//PWR_SAVING
#define PWR_SAVING_LEVEL2    2		//PWR_SAVING_SUSPEND
#define PWR_SAVING_LEVEL3    3
#define PWR_SAVING_LEVEL4    4
#define PWR_SAVING    PWR_SAVING_LEVEL4
//#define DBG_PWR_SAVING

//#define USB3_SYNC_MODE_EN

#ifdef DBG_FUNCTION
#define DEBUG_LTSSM
//#define DEBUG_ENUM
#endif
//#define COMPLIANCE
#define SFLASH_PAGE_PROGRAM

//------------Aviod print Warning in MetaWare-------
//	print case, use:
//	#define DBG						new_print
//	non-print case,use:
//	#define DBG(...)
//--------------------------------------------------
#ifdef DBG_FUNCTION
	//#define print					new_print
	#define	DBG0(...)//					new_print
	#define	DBG(...)//					new_print
//	#define	DBG						new_print
//	#define MSG(...)	//					new_print
	#define MSG						new_print
#ifdef UART_RX
	#define ERR(...)		//				new_print
#else
	#define ERR(...)//						new_print
#endif
	#define	pv8(addr)				new_pv8(addr)
	#define	pv16(addr)				new_pv16(addr)
	#define	pv32(addr)				new_pv32(addr)

//	#define	printv8(value)			lib_printv8(value)
//	#define	printv16(value)			lib_printv16(value)
//	#define	printv32(value)			lib_printv32(value)	
#else
	//#define print					
	#define	DBG0
	#define	DBG(...)
	#define MSG(...)
	#define ERR(...)
	#define	pv8(addr)
	#define	pv16(addr)
	#define	pv32(addr)
	#define	printv8(value)
	#define	printv16(value)
	#define	printv32(value)	
#endif
#endif	// #ifdef	CONFIG_H


