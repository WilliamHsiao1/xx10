/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/

#ifndef SHARED_H
#define SHARED_H

#undef  Ex
#ifdef SHARED_C
	#define Ex
#else
	#define Ex extern
#endif


#ifdef UART_RX
	Ex u8 uart_rx_buf[50];
	Ex u8 uartRxPos;
#endif

#ifdef ERR_RB_DBG
Ex u8 err_rb_dbg;
#define CLOSE			0x00
#define INS_RB_GOOD		0x01
#define RB_R_FAIL		0x02
#define RB_W_FAIL		0x03
#define R1_W0_FAIL		0x04
#define R1_W1_FAIL		0x05
#define BOTH_FAIL		0x06

#endif

//Ex u32 site;
//Ex PCDB_CTXT pCtxt;
//#ifdef DISPATCH
//Ex u32 cur_tag;
//Ex u32 cmdStartFlag;

//Ex u32 b2hCmdStartFlag;
#if 1
Ex u8	temp_USB3StateCtl;
Ex u8	temp_USB2StateCtl;
Ex u8	usb_inactive_count;
Ex u8	connect_maganeStatus ;
#endif

#ifdef SUPPORT_HR_HOT_PLUG
Ex u8	pre_sata_active;
Ex u8	hot_plug_enum;
//Ex u8	reset_ignore_flag;
#endif

Ex u8	usb3_ts1_timer;
Ex u8	usb3_enter_loopback_timer;
Ex u8	usb3_test_mode_enable;

//Ex u32 Response_IU_Pending;
//Ex u32 Cur_Initiator_Tag;
//Ex u16 Cur_Task_Tag;

Ex u8	curMscMode;
Ex u8	mscInterfaceChange;
Ex u8  	usbMode;
//Ex u32 curSegIndex;
Ex u8 	rdCapCounter;
#ifdef DEBUG_USB3_ERR_CNT
//Ex u32 	linkErrCnt;
Ex u32	decErrCnt;
//Ex u32	disparityErrCnt;
Ex u8	fout_value;
Ex u8	tmp_fout_value;
#endif

Ex u8 	unit_serial_num_str[20];

Ex u8	re_enum_flag;
	#define DOUBLE_ENUM 				0x01 // do the double enum because the HDD's SN is not match value saved in flash
	#define FORCE_OUT_RE_ENUM			0x02 // return to usb_mian, and load the new setting
	#define RECHECK_USB					0x03

#ifdef DATABYTE_RAID
Ex u8 cfa_active;
#endif
// bits[0:3] slot 0
// bits[4:7] slot 1
// bits[8:11] ata init & enum, after the Drive is been resetted, it shall be re-initialized after SATA PHY ready
// bits[12:15] raid enum, check the slot array status, and make the enumeration
enum enum_states
{
	ENUM_SLOT0_INIT_DONE	= BIT_0, // define the physical slot's init state in the SATA port 0
	ENUM_SLOT0_NO_HDD = BIT_1,
	ENUM_SLOT1_INIT_DONE = BIT_4,
	ENUM_SLOT1_NO_HDD = BIT_5,
	ENUM_ATA_INIT_DONE = BIT_8,
	ENUM_RAID_ENUM_DONE= BIT_12
};
	
Ex u8	re_enum_enable; // this is the flag for UAS	

Ex u8 count_no_fis34_failure_sata0;
Ex u8 count_no_fis34_failure_sata1;

Ex u8 rw_flag; // when the read10/write10 doesn't sent out the CSW when connect to dual USB2 host, reset it again
	#define READ_FLAG 							0x01
	#define WRITE_FLAG 							0x02
	#define RW_WAIT_TMOUT_RESET_FLAG 		0x04
	#define LINK_RETRY_FLAG						0x08
	#define RD_WR_TIME_OUT						0x10
	// this is a 5secs timer, if the RW is still in processing after timer out,
	// F/W will check the USB_TX/RX residue length to confirm the USB TX/RX is dead or not
	#define RW_IN_PROCESSING					0x20  
Ex u8 dbg_next_cmd;
Ex u8 dbg_once_flag;
Ex u16 r_recover_counter;
Ex u16 w_recover_counter;
Ex u8 rw_time_out;
Ex u8 	unsupport_boot_up;
	#define UNSUPPORT_CHIP_REV_BOOT			0x01
	#define UNSUPPORT_HW_STRAPPING_BOOT		0x02
	#define UNSUPPORT_FLASH_TYPE_UNKNOW					0x04
	#define UNSUPPORT_HDD0_INITIALIZED_FAIL				0x08
	#define UNSUPPORT_HDD1_INITIALIZED_FAIL				0x10
	#define UNSUPPORT_NO_HDD0_ATTACHED					0x20
	#define UNSUPPORT_NO_HDD1_ATTACHED					0x40
	
Ex SATA_CH_REG volatile *sata_ch_reg;
	
enum bot_active_reason
{
	BOT_NOT_ACTIVE = 0,
	BOT_ACTIVE_PROCESS_CBW = 0x01,
	BOT_ACTIVE_WAIT_FOR_SATA0_READY = 0x02,
	BOT_ACTIVE_WAIT_FOR_SATA1_READY = 0x04,
	BOT_ACTIVE_RAID_RB = 0x08
};
Ex u8 bot_active;
//Ex u8 Init_hardware_flag;


Ex DBUF_REG volatile * Tx_Dbuf;
Ex DBUF_REG volatile * Rx_Dbuf;


//Ex U8	hdd_serial;
//Ex U8	valid_hdd;
Ex u8	USB_AdapterID;
Ex u8	USB_PortID0;
Ex u8	USB_PortID1;
Ex u8	USB_PortID2;

Ex u8 mc_buffer[4096];

typedef enum {// it's shall be same with the image mapping of ld
	 VIRTUAL_DATAS_BASE		=	0x40000000,
}IMAGE_MAPPING;

Ex u32 alloc_len;
Ex u32	byteCnt;
Ex u32	nextDirection;
#define DIRECTION_NONE				0
#define DIRECTION_SEND_DATA			1
#define DIRECTION_SEND_STATUS		2
#define DIRECTION_SEND_STATUS_NOW	3
#define DIRECTION_BUS_RESET			4
#define DIRECTION_RECEIVED_DATA		5
#define DIRECTION_RD_INQ			6
#define DIRECTION_RD_NV				7

#define DIRECTION_SEND_STATUS_WRITE_PROTECT		10


extern void CrSetVendorText(u8 *buf);
extern void CrSetModelText(u8 *buf);
extern void xmemcpy(u8 *src, u8 *dest, u32 size);
extern void xmemcpy32(u32 *src, u32 *dest, u32 size);
extern void memcpy16(u8 *src, u8 *dest, u16 size);
extern void Delay(u16 time);
extern void Delayus(u16 time);
extern void memcpySwap16(u8 *dest, u8 *src, u32 n16);
extern void xmemset(u8 *ptr, u8 chr, u32 count);
extern u8 xmemcmp(u8 *src, u8 *dest, u32 count);
extern u16 codestrlen(u8 *str);
extern u8 Hex2Ascii(u8 hex);
extern u8 str_empty(u8 *str, u16 len);
extern u16 Checksum16(const u8   *dp, u8 len);
//extern void calc_Idle_timer(u8 time);
extern void reset_clock(u8 clock_setting);
extern void do_ASIC_reset(void);
// define the cpu clock
#define CLOCK_DEFAULT	16
#define CLOCK_192M		0
#define CLOCK_160M		1
#define CLOCK_137M		2
#define CLOCK_120M		3
#define CLOCK_107M		4
#define CLOCK_96M		5
#define CLOCK_87M		6
#define CLOCK_80M		7
#define CLOCK_64M		8
#define CLOCK_48M		9
#define CLOCK_32M		10
#define CLOCK_16M		11
#define CLOCK_8M		12
#define CLOCK_4M		13
#define CLOCK_2M		14
#define CLOCK_1M		15
extern void set_uart_tx(u32 clock);
#ifdef INTERRUPT
extern void init_cpu_interrupt();
extern void init_timer0_interrupt(u32 timer, u32 clock);
extern void init_timer1_interrupt(u32 timer, u32 clock_setting);
#endif

#ifdef MEMORY_CHECK
void Main_CheckMemory();
#endif


/*******************************************\
	Util
	ChgEndian_16(0x1234) return 0x3412
	ChgEndian_32(0x12345678) return 0x78563412
\*******************************************/
#define Min(a,b)        (((a) < (b)) ? (a) : (b))
#define Max(a,b)        (((a) > (b)) ? (a) : (b))

//----------------
#define	ChgEndian_16(hex) \
		( ((u16)(hex)>>8)+((u16)(hex)<<8) )

#define	ChgEndian_32(hex) \
		( ((u32)(hex)>>24)+((u32)((hex)&0xFF0000)>>8)+((u32)((hex)&0xFF00)<<8)+((u32)(hex)<<24) )

//----------------------
#define SWAP16(UB16) (((UB16) << 8) | ((UB16) >> 8))
//#define SWAP32(UB32) ((((U32)(UB32) & 0xFF000000) >> 24) | (((U32)(UB32) & 0x00FF0000) >> 8) | (((U32)(UB32) & 0x0000FF00) << 8) | (((U32)(UB32) & 0x000000FF) << 24))

#define COPYU16(dest, src) \
	{\
		*(((u8 *)(dest)) + 0) = *(((u8 *)(src)) + 0); \
		*(((u8 *)(dest)) + 1) = *(((u8 *)(src)) + 1); \
	}


#define COPYU32(dest, src) \
	{\
		*(((u8 *)(dest)) + 0) = *(((u8 *)(src)) + 0); \
		*(((u8 *)(dest)) + 1) = *(((u8 *)(src)) + 1); \
		*(((u8 *)(dest)) + 2) = *(((u8 *)(src)) + 2); \
		*(((u8 *)(dest)) + 3) = *(((u8 *)(src)) + 3); \
	}

#define COPYU16_REV_ENDIAN(dest, src) \
	{\
		*(((u8 *)(dest)) + 0) = *(((u8 *)(src)) + 1); \
		*(((u8 *)(dest)) + 1) = *(((u8 *)(src)) + 0); \
	}


#define COPYU32_REV_ENDIAN(dest, src) \
	{\
		*(((u8 volatile *)(dest)) + 0) = *(((u8 volatile *)(src)) + 3); \
		*(((u8 volatile *)(dest)) + 1) = *(((u8 volatile *)(src)) + 2); \
		*(((u8 volatile *)(dest)) + 2) = *(((u8 volatile *)(src)) + 1); \
		*(((u8 volatile *)(dest)) + 3) = *(((u8 volatile *)(src)) + 0); \
	}

#define	WRITE_U32_BE(addr, value) \
	*((u8 *)addr+3) = (u8)(value); \
	*((u8 *)addr+2) = (u8)(value >>8 ); \
	*((u8 *)addr+1) = (u8)(value >>(8*2)); \
	*(u8 *)addr = (u8)(value >>(8*3)); \

#define COPYU64_REV_ENDIAN(dest, src) \
	{\
		*(((u8 *)(dest)) + 0) = *(((u8 *)(src)) + 7); \
		*(((u8 *)(dest)) + 1) = *(((u8 *)(src)) + 6); \
		*(((u8 *)(dest)) + 2) = *(((u8 *)(src)) + 5); \
		*(((u8 *)(dest)) + 3) = *(((u8 *)(src)) + 4); \
		*(((u8 *)(dest)) + 4) = *(((u8 *)(src)) + 3); \
		*(((u8 *)(dest)) + 5) = *(((u8 *)(src)) + 2); \
		*(((u8 *)(dest)) + 6) = *(((u8 *)(src)) + 1); \
		*(((u8 *)(dest)) + 7) = *(((u8 *)(src)) + 0); \
	}
#define	WRITE_U64_BE(addr, value) \
	*((u8 *)addr+7) = (u8)(value); \
	*((u8 *)addr+6) = (u8)(value >>8 ); \
	*((u8 *)addr+5) = (u8)(value >>(8*2)); \
	*((u8 *)addr+4) = (u8)(value >>(8*3)); \
	*((u8 *)addr+3) = (u8)(value >>(8*4)); \
	*((u8 *)addr+2) = (u8)(value >>(8*5)); \
	*((u8 *)addr+1) = (u8)(value >>(8*6)); \
	*(u8 *)addr = (u8)(value >>(8*7)); \


#endif
