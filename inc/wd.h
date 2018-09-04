/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2010-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/
#ifndef WD_H
#define WD_H

#undef  Ex
#ifdef WD_C
	#define Ex
#else
	#define Ex extern
#endif

#define WDC

// Support saving and using the quick enumeration info.
#define WDC_ENABLE_QUICK_ENUMERATION	0

// Enable the Packet ID (PID) detection fix.
// This improves compatibility with some USB hosts and PCs.
#define WDC_ENABLE_SYNC_PID_DETECT		1
// Use the new D2B TotalCnt register in the 1607E chip.
#define WDC_ENABLE_NEW_D2B_COUNTER		1
// Do not be functional with older revisions of the 1607E chip.
#define WDC_USE_NEW_CHIPS_ONLY			1

// Detect and respond to SATA PHY ready/not ready events.
// Do not enable without review; much of this is Initio's feature.
#define WDC_SUPPORT_SATA_HOTPLUG		0

// Allow data encryption to be turned off (cleartext mode) on Apollo drives.
#define WDC_SUPPORT_CIPHER_NONE			0

// Allow the drive to be locked with a password in cleartext mode.
#define WDC_SUPPORT_CIPHER_NONE_LOCK	0

// Include READ(12), WRITE(12) and VERIFY(12) cmds on the disk LUN.
#define WDC_SUPPORT_RWV12				0

// Wait forever for the SATA PHY handshaking to finish during init.
#define WDC_WAIT_FOREVER_SATA_PHY		1

// Require checksuming of the HDD's IDENTIFY DEVICE data. The
// checksum is optional (per ATA) but all WD drives have checksums.
#define WDC_REQUIRE_ID_DEVICE_CHECKSUM	1

// Time to wait for HDD to become ready during init.
// The unit is 111ms ticks, so Initio's original 20s will be 178.
// Set to 0 to wait forever for the HDD to become ready.
#define WDC_HDD_READY_TICKS				0		// In 111ms ticks; 0 to disable.

// Make use of the HDD's Power Up in Standby (PUIS) feature.
#define WDC_USE_PUIS					0

// Set the HDD's Acoustic Management level during init.
#define WDC_SET_AAM						0

// Turn on or off the HDD's Write Cache during init.
#define WDC_SET_WRITE_CACHE				1

// Largest supported logical block length.
// This value minus 9 is the maximum # of bits that LBA and transfer
// length values have to be shifted.
// Warning: do not use more than 17 (8 bits' shift) without a code review!
#define WDC_MAX_LOGICAL_BLOCK_SIZE_LOG2	15
#define WDC_TEST_LOGICAL_BLOCK_SIZE_LOG2		0

// Always say the HDD's physical sector size is 4KB.
// This avoids having to use the
#define WDC_ALWAYS_REPORT_4K_PHYSICAL	1

// Check the HDD against a whitelist of drives that use 4KB physical
// sectors but do not report it in their ID DEVICE data.
#define WDC_USE_4K_PHYSICAL_WHITELIST	0

//  SATA command standart timout tick.
// The unit is deciseconds
#define WDC_SATA_TIME_OUT		(5 /*seconds*/ *10)

// SATA command Long timout tick.
// The unit is deciseconds
#define WDC_SATA_LONG_TIME_OUT		(25 /*seconds*/ *10)

// Default HDD standby time.
// The unit is deciseconds, just like the Power Condition mode page.
#define WDC_DEFAULT_STANDBY_TIME		(20 /*minutes*/ * 60 /*seconds*/ *10)

#define WDC_DEFAULT_LED_BRIGHTNESS	0xff

// HDD temperature polling interval.
// The unit is 10-second ticks, so set this to 24 for 4 minute intervals.
// Set it to 0 to disable the temperature polling feature.
#define WDC_HDD_TEMPERATURE_POLL_TICKS	0//2400		//( 4min *60sec * 10); 0 to disable.
#define WDC_SHORT_HDD_TEMPERATURE_POLL_TICKS	0// 50		//( 5sec * 10); 0 to disable.

// Fake-out HDD temperature readings for testing purposes.
#define WDC_TEST_TEMPERATURE_POLL		0

// Enable it to handles the speed of the cooling fan and triggers the thermal shutdown die state.
#define FAN_CTRL_EXEC_TICKS		0// 150		// 15sec * 10; 0 to disable in FW

// Shutdown immediately if an error or invalid data is encountered
// when reading the HDD's temperature.
#define WDC_TEMPERATURE_POLL_DIE_TRYING	0//2400

// Turn off parts of the bridge chip to reduce power usage.
#define WDC_BRIDGE_POWER_SAVE			1

// Allow only WD's firmware updaters to update this product.
#define WDC_FIRMWARE_UPDATER_ONLY		0

// Compatibility hack for IMI disk duplicators.
#define WDC_IMI_DISK_DUPPER_HACK		1

// Make the Passport AV compatible with some Sony Blu-ray players.
// The Passport AV is in the HP-Oasis-Opus product group.
#if WDC_PRODUCT_GROUP == WDC_HP_AND_OASIS
#   define WDC_SONY_BD_PLAYER_HACK		1
#endif

#define WDC_INCLUDE_ASSERTIONS			0

#define WDC_INCLUDE_TEST_CODE			0
#ifndef INITIO_STANDARD_FW
#define WDC_FWCONFIG_ONLY				1
#endif

// Quick-n-dirty EPD demo; do not enable in production code!
#define WDC_EPD_DEMO					0

// Select 0 to force a full/robust epd update for each and every epd update. 
// Select 1 to use the less stable partial update approach - 
// this normally uses WDC_EPD_MAX_PARTIAL_UPDATES consecutive partial epd 
// updates per every 1 full/robust epd update
#define WDC_EPD_PARTIAL_UPDATE			0

// This controls the number of consecutive partial updates before issuing a 
// full/robust update. NOTE: This is used only when WDC_EPD_PARTIAL_UPDATE == 1
#define WDC_EPD_MAX_PARTIAL_UPDATES		5

// Quick and dirty Rev A testing; do not enable in production code!
// If this feature is not enabled, a connected Rev A device will not update.
#define WDC_EPD_SUPPORT_REV_A			0

// Length in bytes of the HDD's serial number.
#define HDD_SERIAL_NUM_LEN		20

// Convert the version # to ASCII - at compile time instead of at runtime.
#define WDC_FIRMWARE_VERSION			0x1000

// For STAFF to clean data saved in HDD, it should be DISABLED in released F/W.
#define WDC_CLEAN_HDD_DATA		0
#endif 
