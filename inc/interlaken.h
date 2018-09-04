/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2007-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/

#ifndef INTERLAKEN_H
#define INTERLAKEN_H

#undef  Ex
#ifdef INTERLAKEN_C
	#define Ex 
#else
	#define Ex extern
#endif



// Size of EPD frame buffer
#define EPD_FRAME_BUFFER_SIZE	32
#define EPD_BRANDING_SEGMAP_SIZE	24


// Polarity of the power button.
// 0= the GPIO reads 0 when the switch is pressed.
#define	POWER_BUTTON_PRESSED  		1
#define	RAID_CONFIG_BUTTON_PRESSED	2
#define	CLONE_BUTTON_PRESSED			3

// Use to allow EPD builds such as My Book Elite builds
#define WDC_EPD							0// 1

#ifdef JTAG_DBG
#define WD_ACTIVITY_PWM_SEL			0
#define WD_FAULT_PWM_SEL				0
#define WD_FAIL_SLOT0_PWM_SEL		0
#else
#define WD_ACTIVITY_PWM_SEL			GPIO11_PWM0_FUNC3
#define WD_FAULT_PWM_SEL				GPIO12_PWM1_FUNC3
#define WD_FAIL_SLOT0_PWM_SEL		GPIO10_PWM2_FUNC2
#endif
#define WD_FAIL_SLOT1_PWM_SEL		GPIO14_PWM3_FUNC1

#if FAN_CTRL_EXEC_TICKS
#define WD_FAN_PWM_SEL				GPIO3_PWM4_FUNC1
#endif

#define WD_PID0							GPIO15_PIN
#define WD_PID1							GPIO13_PIN // this bit is not cared
#define WD_PID2							GPIO6_PIN 
#ifdef DB_CLONE_LED
#define WD_PID3							GPIO2_PIN
#else
#define WD_PID3							GPIO7_PIN
#endif

#ifdef POWER_BUTTON	
#define PWR_BUTTON						GPIO4_PIN
#endif

#ifdef DB_CLONE_LED
#define RAID_CONFIG_BUTTON			    GPIO2_PIN
#else
#define RAID_CONFIG_BUTTON			    GPIO7_PIN
#endif
#ifdef HDD_CLONE
#define CLONE_BUTTON					GPIO17_PIN
#endif

#ifdef DB_CLONE_LED
#define CLONE_LED_25					GPIO10_PIN
#define CLONE_LED_25_ON()				*gpioDataOut |= CLONE_LED_25
#define CLONE_LED_25_OFF()				*gpioDataOut &= ~CLONE_LED_25
#define CLONE_LED_100					GPIO7_PIN
#define CLONE_LED_100_ON()				*gpioDataOut |= CLONE_LED_100
#define CLONE_LED_100_OFF()				*gpioDataOut &= ~CLONE_LED_100
#endif

#ifdef HDR2526_LED
#ifdef DB_CLONE_LED
#ifdef DATASTORE_LED
#define JBOD_LED_PIN                    GPIO14_PIN      
#else
#define JBOD_LED_PIN					GPIO3_PIN
#endif
#else
#define JBOD_LED_PIN					GPIO10_PIN
#endif
#define RAID0_LED_PIN					GPIO9_PIN
#define RAID1_LED_PIN					GPIO8_PIN
#else
#define DRIVE0_POWER_PIN				GPIO9_PIN
#define DRIVE1_POWER_PIN				GPIO8_PIN
#define ERROR_LED_D0					GPIO10_PIN
#endif
#ifndef DATASTORE_LED
#define ERROR_LED_D1					GPIO14_PIN
#else
#define ERROR_LED_D1                    GPIO3_PIN
#endif

#ifdef HDR2526_LED
#define HDD0_LED_PIN					GPIO11_PIN
#define HDD1_LED_PIN					GPIO1_PIN
#define FAIL_BUZZER_PIN					GPIO12_PIN
#ifdef HDR2526_BUZZER
#define FAIL_BUZZER_PWM_SEL			GPIO12_PWM1_FUNC3
#endif
#else
#define ACTIVITY_LED_PIN_D0			GPIO11_PIN
#define ACTIVITY_LED_PIN_D1			GPIO12_PIN
#endif

#if FAN_CTRL_EXEC_TICKS
#define FAN_TACH_IN_PIN				GPIO17_PIN
#endif

#define PORTABLE_PRODUCT					WD_PID3//3.5' desktop product, portable product 
#define ATHENA35_PRODUCT					WD_PID2// WD PRO - PRODUCT

#ifdef HDR2526_LED
#define HDD0_LED_ON()					*gpioDataOut |= HDD0_LED_PIN 
#define HDD0_LED_OFF()					*gpioDataOut &= ~HDD0_LED_PIN
#ifdef UART_RX
#define HDD1_LED_ON()					
#define HDD1_LED_OFF()					
#else
#define HDD1_LED_ON()					*gpioDataOut |= HDD1_LED_PIN 
#define HDD1_LED_OFF()					*gpioDataOut &= ~HDD1_LED_PIN
#endif
#else 
#define ACTIVITY_LED_ON()				*gpioDataOut |= ACTIVITY_LED_PIN_D0 
#define ACTIVITY_LED_OFF()				*gpioDataOut &= ~ACTIVITY_LED_PIN_D0
#define ENABLE_WDC_ACTIVITY_PWM()		*gpioCtrlFuncSel0 |= WD_ACTIVITY_PWM_SEL
#define DISABLE_WDC_ACTIVITY_PWM()	*gpioCtrlFuncSel0 &= ~WD_ACTIVITY_PWM_SEL
#define ENABLE_ACTIVITY_LED_FUNC()		*led0_Ctrl1 =  (*led0_Ctrl1 | LED_ENABLE) & ~LED_PWM_MODE
#define DISABLE_ACTIVITY_LED_FUNC()	*led0_Ctrl1 &= ~LED_ENABLE
#endif

#ifdef HDR2526_LED
#ifdef HDR2526_BUZZER
#define FAULT_BUZZER_OFF()				*gpioDataOut &= ~FAIL_BUZZER_PIN
#define ENABLE_FAIL_BUZZER_PWM()		*gpioCtrlFuncSel0 |= FAIL_BUZZER_PWM_SEL
#define DISABLE_FAIL_BUZZER_PWM()		*gpioCtrlFuncSel0 &= ~FAIL_BUZZER_PWM_SEL
#define ENABLE_FAIL_BUZZER_FUNC()		*led1_Ctrl1 =  (*led1_Ctrl1 | LED_ENABLE) & ~LED_PWM_MODE
#define DISABLE_FAIL_BUZZER_FUNC()		*led1_Ctrl1 &= ~LED_ENABLE
#else
#define FAULT_BUZZER_ON()				*gpioDataOut |= FAIL_BUZZER_PIN
#define FAULT_BUZZER_OFF()				*gpioDataOut &= ~FAIL_BUZZER_PIN
#endif
#else
#define FAULT_INDICATOR_LED_ON()		*gpioDataOut |= ACTIVITY_LED_PIN_D1
#define FAULT_INDICATOR_LED_OFF()		*gpioDataOut &= ~ACTIVITY_LED_PIN_D1
#define ENABLE_WDC_FAULT_PWM()		*gpioCtrlFuncSel0 |= WD_FAULT_PWM_SEL
#define DISABLE_WDC_FAULT_PWM()		*gpioCtrlFuncSel0 &= ~WD_FAULT_PWM_SEL
#define ENABLE_FAULT_LED_FUNC()		*led1_Ctrl1 =  (*led1_Ctrl1 | LED_ENABLE) & ~LED_PWM_MODE
#define DISABLE_FAULT_LED_FUNC()		*led1_Ctrl1 &= ~LED_ENABLE
#define FAIL_SLOT0_LED_ON()				*gpioDataOut |= ERROR_LED_D0
#define FAIL_SLOT0_LED_OFF()			*gpioDataOut &= ~ERROR_LED_D0
#endif

#if 0
#define ENABLE_WDC_FAIL_S0_PWM()		*gpioCtrlFuncSel0 |= WD_FAIL_SLOT0_PWM_SEL
#define DISABLE_WDC_FAIL_S0_PWM()		*gpioCtrlFuncSel0 &= ~WD_FAIL_SLOT0_PWM_SEL
#define ENABLE_FAIL_S0_LED_FUNC()			*led2_Ctrl1 =  (*led2_Ctrl1 | LED_ENABLE) & ~LED_PWM_MODE
#define DISABLE_FAIL_S0_LED_FUNC()		*led2_Ctrl1 &= ~LED_ENABLE
#endif

#define FAIL_SLOT1_LED_ON()				*gpioDataOut |= ERROR_LED_D1
#define FAIL_SLOT1_LED_OFF()			*gpioDataOut &= ~ERROR_LED_D1
#if 0
#define ENABLE_WDC_FAIL_S1_PWM()		*gpioCtrlFuncSel0 |= WD_FAIL_SLOT1_PWM_SEL
#define DISABLE_WDC_FAIL_S1_PWM()		*gpioCtrlFuncSel0 &= ~WD_FAIL_SLOT1_PWM_SEL
#define ENABLE_FAIL_S1_LED_FUNC()			*led3_Ctrl1 =  (*led3_Ctrl1 | LED_ENABLE) & ~LED_PWM_MODE
#define DISABLE_FAIL_S1_LED_FUNC()		*led3_Ctrl1 &= ~LED_ENABLE
#endif

#ifdef HDR2526_LED
#define JBOD_LED_ON()						*gpioDataOut |= JBOD_LED_PIN
#define JBOD_LED_OFF()						*gpioDataOut &= ~JBOD_LED_PIN
#define RAID0_LED_ON()						*gpioDataOut |= GPIO9_PIN
#define RAID0_LED_OFF()						*gpioDataOut &= ~GPIO9_PIN
#define RAID1_LED_ON()						*gpioDataOut |= RAID1_LED_PIN
#define RAID1_LED_OFF()						*gpioDataOut &= ~RAID1_LED_PIN
#else
#define POWER_ON_SATA0()					*gpioDataOut |= DRIVE0_POWER_PIN
#define POWER_ON_SATA1()					*gpioDataOut |= DRIVE1_POWER_PIN // to be changed when the function spec is ready
#define POWER_OFF_SATA1()					*gpioDataOut &= ~DRIVE1_POWER_PIN
#define POWER_OFF_SATA0()					*gpioDataOut &= ~DRIVE0_POWER_PIN
#endif

#ifdef FPGA
// PERCLK in FPGA is 50MHz
#define	WDC_LED_TIMER_UNIT	0x0D // 2 power 13 * PER clock(50Mhz) = 163.84
#define 	WDC_FAN_TIMER_UNIT	0x0F // 2 power 15 * PER clock(50Mhz) = 655.36us
#define	WDC_LED_BLINK_PERIOD	10 	// 1.051
#define	WDC_PWM_PERIOD		0xFA
#else
#define	WDC_LED_TIMER_UNIT	0x0F // 2 power 15 * CPU clock(187Mhz/150Mhz) = 175.25/218.45 us
#define	WDC_LED_TIMER_UNIT_LOW_CLOCK	0x0C	// 2 power 12 * CPU clock(25Mhz/30hz) = 163.84/136.5 us
#define 	WDC_FAN_TIMER_UNIT	0x14
#define	WDC_LED_BLINK_PERIOD	10 	// 1.051/1.311 ms per cycle 
#define	WDC_PWM_PERIOD		0xFA
#endif
#ifdef HDR2526_BUZZER
#define	HDR2526_BUZZER_TIMER_UNIT	0x0B		// 2600HZ about 380us
#endif
//
// Global Variables
//
#ifdef WDC_DOWNLOAD_MICROCODE
Ex u8 quick_enum;
Ex u8 flash_vid, flash_pid;
Ex u8 flash_type;
Ex u8 flash_size_larger_than_2Mb;
Ex u8 flash_operation_timer; // in the case of unsupported flash, there should be timer out mechanism

#define MX				0x10
	#define 		MX25L1005		0x10
#define PMC				0x20
	#define 		PM25LV010A		0x20
	#define 		PM25LD010C		0x21
	#define		PM25LD020C		0x22
	#define 		PM25LD040		0x23
	#define		PM25LV020A		0x24
	#define		PM25LV040A		0x25

#define SST				0x30
	#define		SST25VF010A	0x30
	#define		SST25VF020		0x31
	#define		SST25VF020B	0x32
	#define		SST25VF040		0x33
#define WINBOND			0x40
	#define		W25X10BL		0x40
	#define		W25X20BL		0x41
	#define		W25X40BL		0x42
#define SANYO			0x50
	#define		LE25FU206A		0x51
#define GENERIC_FLASH		0x66
#define UNKNOWN_FLASH 	0x00

#define MAX_FLASH_SPACE	0x3000
#endif

Ex u8 cdrom_lun;				// The virtual CD-ROM's logical unit number.
Ex u8 ses_lun;				// The LUN of the SES logical unit.
Ex u8 hdd1_lun;
Ex u8 logicalUnit;
	#define	HDD0_LUN			0x00
	#define	HDD1_LUN			hdd1_lun
	#define	VCD_LUN			cdrom_lun
	#define	SES_LUN			ses_lun
	#define	UNSUPPORTED_LUN	0xff

#ifdef WRITE_PROTECT
Ex u8 write_protect_enabled;
#endif
#ifdef RMB_SWITCH  // for test
Ex u8 rmb_enabled;
#endif	
// Firmware version number, in BCD and ASCII.
Ex u16  firmware_version;
Ex u8 firmware_version_str[4];

Ex u8   FW_state;				// FW state machine like power on, update FW, active.
Ex u8 	power_button_pressed;			// ISR; set when the power button is pressed.
Ex u8 	power_off_commanded;			// ISR; 1= power-off command received from PC.
#ifdef HARDWARE_RAID
Ex u8	raid_config_button_pressed;
#endif
#ifdef HDD_CLONE
Ex u8	clone_button_pressed;
#endif
#ifdef SCSI_HID
Ex u8	backup_button_pressed;
#endif
#ifdef DATASTORE_LED
Ex u8	wait_for_clone;
#endif

Ex u16 	disable_upkeep;				// Conditions that disable background activities.
Ex u8 	ignore_tickles;				// 1= we're tickle-proof.
Ex u8 	standby_timer_expired;		// ISR; set when standby timer expires.

#ifdef PWM
#define ACTIVE_LED_TIMEOUT_1Sec		168		// (1s/(6ms))+1
#define BLINK_RATE_NEVER_BLINK		0
#define BLINK_RATE_3HZ				25		// 300ms/12ms
#define BLINK_RATE_DOT3HZ			250		// (3s/12ms
#else
/******** HDR2526_LED led blink rate
u3 rw @ 5hz
u2 rw @ 1hz
fault led @ 1hz
clone led @ 1.6hz
raid led @ 1.6hz
****************/
#define ACTIVE_LED_TIMEOUT_1Sec		11		// (1s/(100ms))+1
#define ACTIVE_LED_TIMEOUT_2Sec		21
#define ACTIVE_LED_TIMEOUT_3Sec		31
#define BLINK_RATE_NEVER_BLINK		0
#define BLINK_RATE_1D6HZ			3		// 600ms/200ms
#define BLINK_RATE_3HZ				2		// 400ms/200ms
#define BLINK_RATE_DOT3HZ			15		// (3s/200ms
#define BLINK_RATE_1HZ				5		// (1s/(200ms))
#define BLINK_RATE_5HZ				1		// 200ms/200ms
#endif

#if WDC_HDD_TEMPERATURE_POLL_TICKS
Ex u8 	check_temp_flag;				// ISR
Ex u32 	expect_temperatureTimer32;		// ISR
#endif
Ex u32 next_buffer_offset;

#if FAN_CTRL_EXEC_TICKS
Ex u8 fan_ctrl_exec_ticker;		// its target is 15 second
Ex u8 fan_ctrl_exec_flag;
//Ex void fan_ctrl_init(u16 slot0_Rpm, u16 slot1_Rpm, u8 useSlot0, u8 useSlot1);
#define UNKNOWN_HDD_RPM	0xffff
Ex void fan_ctrl_init();
Ex void fan_ctrl_exec();
Ex u16 get_fan_RPM();
Ex u8 getTCond();
Ex void forceFanOnDiag();
Ex void enableFan(u8 enalbe);
#ifdef DBG_FUNCTION
Ex void dumpFan(void);
Ex void setFan(u8 idx, u32 value);
#endif
#endif

// hdd_power  indicates whether the HDD and 1607E's SATA PHY is turned on.
// This flag will be cleared if either is turned off, meaning we can't
// talk to the HDD.
Ex u8 	hdd_power;					// 1= the HDD and SATA PHY is turned on.
	#define HDD0_POWER_ON			0x01
	#define HDD1_POWER_ON			0x02
	#define HDDs_BOTH_POWER_ON	0x03

Ex u32 gpio_accumlated_pulse_count;
Ex u32 gpio_pulse_count; // it will be reset to the new value after read the gpio_count register
Ex u16 gpio_count_timetick;
#define GPIO_COUNTER_TIMEUNIT_7DOT5_SEC	75
#define GPIO_COUNTER_TIMEUNIT_15_SEC	150

// List of Artemis & Carrera, Athena products.
// The  product_model  variable is set to one of these at POR.
// Do not change this without reviewing interlaken.c:product_detail_table!
//
enum Product {
	INTERLAKEN_2U = 0,		// INTERLAKEN 2U
	ILLEGAL_BOOT_UP_PRODUCT,
	INITIO_3610,
	NUM_PRODUCTS				// Must be last item!
};

enum DisplayType {
	LED_DISPLAY = 0,
	SIPIX_DISPLAY,
	NUM_DISPLAYS				// Must be last item!
};

typedef struct _ProductDetail {
	u16 USB_VID;				// USB Vendor ID
	u16 USB_PID;				// USB Product ID
	u16 options;				// Options flags
	u8  product_idx;
	char *vendor_name;
	char *vendor_USB_name;
	char *product_name;
} ProductDetail;

// Product-specific options bitmask.
enum ProductOptions {
	PO_IS_PORTABLE				= 0x01,	// This is a portable/bus-powered device.
	PO_ADD_PID_TO_NAME			= 0x02,	// Add the USB Product ID to model name.
	PO_EMPTY_DISC					= 0x04,  // the device like Artemis 1R or Carrera 1R can enable VCD LUN but without VCD content for special utility usage 
	PO_HAS_POWER_BUTTON			= 0x08, // the product has power button to turn off the hard drive

	PO_NO_ENCRYPTION			= 0x10,	// Never encrypt the user's data
	PO_NO_SES					= 0x20,	// Disable the SES LUN completely
	PO_NO_CDROM				= 0x40,	// Disable the virtual CD completely
	PO_HAS_POWER_SWITCH		= 0x80,	// some product has the Power FET
	PO_SUPPORT_WTG			= 0x100,
	PO_BOT_ONLY				= 0x200,
	PO_COMBINED_ILLEGAL_BOOTUP_OPTIONS	=
		(PO_NO_CDROM | PO_NO_SES | PO_NO_ENCRYPTION)
};

Ex u8	hw_gpio_index;
Ex u8 	product_model;				// What we are; one of enum Product.
Ex ProductDetail product_detail;

// Auto-power states
//
enum AutoPowerState {
	AP_POWER_ON_RESET = 0,		// Power-on state.
	AP_POR_WAIT_FOR_USER,

	AP_NO_HOST_OFF,
	AP_NO_HOST_OFF_WAIT,

	AP_NO_HOST_STICKY_ON,
	AP_NO_HOST_STICKY_ON_WAIT,
	
	AP_RAID_OFFLINE_OPERATION, // the offline raid rebuild or verify or clone function

	AP_USB_ACTIVE,				// A USB host is connected and talking.

	AP_USB_OFF,				// Safe-power down by USB host.
	AP_USB_OFF_WAIT,			// Safe-power down by USB host.

	AP_FW_UPDATE_FROM_USB,
	AP_FIRMWARE_UPDATE		// Firmware update mode
};

// Upkeep disabler flags.
// This is a bitmask of conditions that disables periodic HDD-related
// activities such as temperature polling and the standby timer.
//
enum UpkeepDisableFlags {
	UPKEEP_RUNNING_SELF_TEST_S0	= 0x0001,
	UPKEEP_STANDBY_TIMER_INACTIVE_S0 = 0x0002,
	UPKEEP_HOST_CONTROLS_POWER_CONDITION_S0 = 0x0004,
	UPKEEP_RUNNING_SELF_TEST_S1	= 0x0100,
	UPKEEP_STANDBY_TIMER_INACTIVE_S1 = 0x0200,
	UPKEEP_HOST_CONTROLS_POWER_CONDITION_S1 = 0x0400,

	UPKEEP_ATA_QUIESCENCE_S0		= 0x0010,
	UPKEEP_HDD_SPUN_DOWN_S0		= 0x0080,
	UPKEEP_ATA_QUIESCENCE_S1		= 0x1000,
	UPKEEP_HDD_SPUN_DOWN_S1		= 0x8000,

	UPKEEP_NO_TEMP_POLL_MASK_S0 	= 0x00f0,	// Reasons not to poll HDD temperature
	UPKEEP_NO_TEMP_POLL_MASK_S1 	= 0xf000,	

	UPKEEP_NO_STANDBY_MASK_S0 = 0x00f7,			// HDD is in standby state or executing the backgroud self test
	UPKEEP_NO_STANDBY_MASK_S1 = 0xf700
};


// Bit and field definitions for WD-specific mode pages.
//

// Device Configuration mode page
enum {
	// Byte 2 is the signature.
	DEVCFG_SIGNATURE			= 0x30,

	// Byte 4 enables/disables logical units and features
	DEVCFG_DISABLE_SES			= 0x01,
	DEVCFG_DISABLE_CDROM		= 0x02,
	DEVCFG_DISABLE_UAS		= 0x10,
	DEVCFG_DISABLE_U1U2		= 0x20,
	DEVCFG_DISABLE_AUTOPOWER	= 0x80,

	// Byte 5 enables/disables other features
	DEVCFG_DISABLE_WHITELIST	= 0x01,
};

// Operations mode page
enum {
	// Byte 2 is the signature.
	OPS_SIGNATURE				= 0x30,

	// Byte 5 has virtual CD related settings.
	OPS_ENABLE_CD_EJECT			= 0x01,
	OPS_CD_MEDIA_VALID			= 0x02,
	OPS_LOOSE_LOCKING				= 0x10,

	// Byte 10 has display-related settings.
	OPS_W_ON_B					= 0x01
};


// The largest permissible standby time value in the Power Condition page.
// The actual countdown timer is a 16-bit counter of 10s intervals, so
// the max allowable time is about (2^16) * 100 deciseconds.
// This is over 7 days, well above Apollo requirements.
//#define MAX_STANDBY_TIME		(65533 * 100)
extern u8 check_product_config(u16 new_vid, u16 new_pid);

extern void gpio_init(void);
extern void timer_init(void);
extern u8 check_compability_product(u8 productMode);
extern void hw_strapping_setting(void);	
extern void  identify_product(void);
extern void set_power_light(u8 shine);
extern void set_activity_for_standby(u8 standby);
extern void tickle_fault(void);
extern void tickle_activity(void);
extern void reset_standby_timer(void);
extern void update_standby_timer(void);
#ifdef HDR2526
extern void update_clone_standby_timer(u32 timer);
#endif
extern void set_hdd_power(u8 powerOnHDD, u8 powerOnPHY);	
	#define PU_SLOT0						0x01	//power up slot0
	#define PU_SLOT1 						0x02
	#define PD_SLOT0						0x00	//power up slot0
	#define PD_SLOT1 						0x00
	
	#define PWR_CTL_SLOT0_EN				0x04
	#define PWR_CTL_SLOT1_EN				0x08
	
extern void set_fuel_gauge_bargraph(u16 bargraph);
extern void fuel_gauge_off(void);
extern void set_latent_lock_indicator(u8 shine);
extern void lights_out(void);
extern void handle_rsp_unit_nrd(u8 mscMode);

extern void enable_PWM_function(void);
extern void enable_GSPI_function(void);
extern void set_GSPI_clock(u8 div);
extern void gSPI_byte_xfer(u8 data);
extern void gSPI_burst_xfer(u8 *buf, u16 xferCnt, u8 gSPI_mode, u8 dir);
	// for Asynchronous byte mode, it can only transmit 1 byte
	#define GSPI_ASYNC_BYTE_MODE			0x00
	// for sychronous burst mode, it can transmit in 8bits or 9bits per transfer
	#define GSPI_SYNC_BURST_MODE			0x01
	#define GSPI_SYNC_BURST_MODE_9BITS		0x02
	#define GSPI_DIR_IN			1
	#define GSPI_DIR_OUT		0
extern void set_fan_state(u8 fan_state);
	#define FAN_OFF			0x00	// PWM 2.4%
	#define FAN_LOW_SPEED	0x01	// 4000RPM, 66.7 rotation per sec
	#define FAN_MED_SPEED	0x02	// 5868RPM, 97.8 rotation per sec 
	#define FAN_HIGH_SPEED	0x03	// 7732RPM, 128.87 rotation per sec
	#define FAN_MAX_SPEED	0x04	// PWM 99.88%

extern void drive_die_warning(u8 LED_blink_rate);
	#define TEMP_OVERHEAT_LED_BLINK_RATE		30

#ifdef HDR2526_LED
extern void fault_leds_active(u8 enable);
extern void tickle_rebuild(void);
#if 0	// new request, db.606, disable it
extern void clone_leds_idle_timeout(u32 timer);
#endif
extern void hdr2526_led_init(void);
extern void hdr2526_led_off(u8 turn_off_case);
#define ALL_IS_OFF		0
#define ACT_IDLE_OFF	1
#endif

#ifdef DBG_FUNCTION
extern void dump_stdTimer(void);
#endif
	

#define REINIT_HDD_IF_OFF()	 \
	if (hdd_power != HDDs_BOTH_POWER_ON)  {  ata_startup_hdd(0, logicalUnit+1);  } //two hdds are active
#endif  // INTERLAKEN_H
