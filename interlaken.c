/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2007-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
*****************************************************************************/
#define INTERLAKEN_C
#include "general.h"
//#undef DEBUG

#ifdef WDC
// All variables used by an ISR are marked "ISR"
static u8 activity_led;			// ISR
static u8 activity_blink_rate;	// ISR
static u8 activity_ticker;		// ISR

static u8 fault_led;			// ISR
static u8 fault_blink_rate;	// ISR
static u8 fault_ticker;		// ISR
static u8 fault_timeout;			// ISR
#ifdef HDR2526_LED
static u8 fault_led_state; // 0: no fault; else fault
#endif
// Power Button detection.
static u8 pb_state;				// ISR; current power button state
static u8 pb_debounce;			// ISR
static u8 pb_ignore_release;		// ISR
static u8 pb_timer;				// ISR
static u8 activity_timeout;			// ISR
#ifdef PWM
static u8 active_led_state;			
	#define ACTIVE_LED_STATUS_ON	0
	#define ACTIVE_LED_STATUS_OFF	1
#endif

#endif


// The HDD standby timer counts down in 0.1s intervals ().
// See  update_standby_timer()
static u32 standby_timer_ticker;	// ISR
static u32 cached_standby_timer_ticks;

// Vendor and product names.
// The compiler only coalesces some literal strings so define the
// strings explicitly and reference them later.
static const char WD_[] = "WD";
static const char HP_[] = "HP";
static const char INITIO_[] = "Initio";
static const char INIC_3610_[] = "INIC-3610";
static const char WesternDigital_[] = "Western Digital";
static const char ExternalHDD_[] = "External HDD";
static const char ILLEGAL_BOOT_UP_[] = "HARDWARE ERROR";

// Don't make Artemis products' name too long; the name and
// the USB Product ID have to fit into 16 chars. See PO_ADD_PID_TO_NAME.
static const char MyBook_[] = "My Book Duo";
static const char MyPassport_[] = "My Passport";
static const char ExtHDD_[] = "Ext HDD";

// Products' information and options.
// This table must be in the same order as the Product enum!
static const ProductDetail  product_detail_table[] = {
//    USB VID, PID,  Product Options... , reserved
//    Vendor Name, USB Descriptor string, Product Name
	// INTERLAKEN_2U: 
	{ 0x1058, 0x0A00, PO_ADD_PID_TO_NAME|PO_HAS_POWER_SWITCH, 0x03,
		(char *)WD_, (char *)WesternDigital_, (char *)MyBook_ },

	{ 0x1058, 0x0000, PO_COMBINED_ILLEGAL_BOOTUP_OPTIONS, 0,
		(char *)WD_, (char *)WesternDigital_, (char *)ILLEGAL_BOOT_UP_ },

#ifdef WDC_VCD
	{ 0x13FD, 0x3A10, PO_NO_ENCRYPTION|PO_NO_SES, 0,
		(char *)INITIO_, (char *)INITIO_, (char *)INIC_3610_ }
#else
#ifdef AES_EVA
	{ 0x13FD, 0x3A10, PO_NO_SES|PO_NO_CDROM, 0,
		(char *)INITIO_, (char *)INITIO_, (char *)INIC_3610_ }
#else
	{ 0x13FD, 0x3A10, PO_NO_ENCRYPTION|PO_NO_SES|PO_NO_CDROM, 0,
		(char *)INITIO_, (char *)INITIO_, (char *)INIC_3610_ }
#endif
#endif
};



#if FAN_CTRL_EXEC_TICKS
static const u8 fanMax = 0xFF;		// it indicate that the Fan FULLY ON, because 
static const u8 fanOffThreshold = 76;
static const u8 fanStep = 25;

static u32 curRPM;		// An integer variable that records the current RPM of the cooling fan
static u8 curFan;		// An integer variable that holds the current duty cycle of the cooling fan.  CurFan marches toward FanGoal
static u8 fanGoal;		// An integer variable that holds the desired duty cycle of the cooling fan 
static u8 pollCnt;		// (N)	PollCnt is incremented on every five minute tick. 
static u8 pollValue;	// ((N*5) min)	An integer variable that holds the desired length of the polling interval in five minute ticks 
static u8 fiveMinuteCnt;	// FiveMinuteCnt is incremented on every 15 second tick.  When FiveMinuteCnt equals 20 (i.e. (5 * 60)/15) the five minute interval has elapsed.
static u8 TCond;		// An integer variable that records the current Temperature Condition that will be reported in receive diagnostics page 
static u8 targetTemperature;
static u8 isFanEnabled;

void turn_off_pwm_fan()
{
	*gpioCtrlFuncSel0 = (*gpioCtrlFuncSel0 & ~0xC0);
	*led4_Ctrl0 = (*led4_Ctrl0 & ~0x000000FF) | (0xA0); 
	*led4_Ctrl1 =  (*led4_Ctrl1 | LED_ENABLE) & ~LED_PWM_MODE;
}

void turn_on_pwm_fan(u8 fan)  // the greater param, the faster
{
	fan /= 25;
	fan = ((10-fan) << 4) | fan;
	MSG("fanOn: %bx\n", fan);
	
	*gpioCtrlFuncSel0 = (*gpioCtrlFuncSel0 & ~0xC0) | WD_FAN_PWM_SEL;
	*led4_Ctrl0 = (*led4_Ctrl0 & ~0xFF000000) | ((WDC_FAN_TIMER_UNIT << 24));
	// (dark+bright): ((fan&0xf0 >> 4) + fan&0x0f) * Tpwm, 
	// fan_max case: fan = 0x0*, always bright, 
	*led4_Ctrl0 = (*led4_Ctrl0 & ~0x000000FF) | (fan); 
	*led4_Ctrl1 =  (*led4_Ctrl1 | LED_ENABLE) & ~LED_PWM_MODE;
}
#if 0
static const u8 rpmToTargetTable[] = {
//	maxRPM, target temperature
	{5400, 60},
	{7200, 55},
	{others, 50}
};
#else
u8 get_cur_target_temperature(u16 hddMaxRPM)
{
	if (hddMaxRPM <= 5400)
		return 60;
	else if (hddMaxRPM <= 7200)
		return 55;
	else
		return 50;
}
#endif

void drive_die_warning(u8 LED_blink_rate)
{
	// drop off USB bus
	//*usb_IntStatus = VBUS_OFF;	
#if (PWR_SAVING)
	turn_on_USB23_pwr();
	for (u16 i = 0; i < 60000; i++)
	{	
		if (*chip_IntStaus & USB3_PLL_RDY)
			break;
		Delayus(3);
	}
#endif
	*usb_DevCtrl = USB_CORE_RESET;
	*usb_DevCtrlClr = USB_ENUM;

	// There is no fan that can be turned on, so the only
	// action we can do is to turn off the HDD and lights.
	// need to change to turn off 2 SATAs both
	ata_shutdown_hdd(TURN_POWER_OFF, SATA_TWO_CHs);
	lights_out();
	enableFan(0);

	auxreg_w(REG_CONTROL0, 0x0); // disable the timer 0
#ifdef HDR2526_LED

#else	
	u8 thermal_blink_led_timer = LED_blink_rate;
	u8 thermal_blink_counter = 2;
	#define BLINK_LED_OFF		0x01
	#define BLINK_LED_ON		0x02
	#define LONG_TERM_OFF		0x03
	u8 led_state = BLINK_LED_OFF;
	ACTIVITY_LED_OFF();
	DISABLE_WDC_ACTIVITY_PWM();	
	while (1)		// Die!
	{
		if (--thermal_blink_led_timer == 0)
		{
			thermal_blink_led_timer = LED_blink_rate; // 125ms
			switch (led_state)
			{
				case BLINK_LED_OFF:
					// set next state
					led_state = BLINK_LED_ON;
					ENABLE_WDC_ACTIVITY_PWM();
					break;
				case BLINK_LED_ON:	
					if (--thermal_blink_counter == 0)
					{
						led_state = LONG_TERM_OFF;
						thermal_blink_counter = 10;
					}
					else
					{
						led_state = BLINK_LED_OFF;
					}
					ACTIVITY_LED_OFF();
					DISABLE_WDC_ACTIVITY_PWM();
					break;
				case LONG_TERM_OFF:
					if (--thermal_blink_counter == 0)
					{
						thermal_blink_counter = 2;
						led_state = BLINK_LED_ON;
						ENABLE_WDC_ACTIVITY_PWM();
					}
					break;
			}
		}
		Delay(1); // run in low frequency
	}
#endif	
}

/***************************************************************\
 fan_ctrl_init() - Initializes and configured the GPIO pins.

 A function called during POR to initialize the fan control 
state machine.  In particular this function picks the value of Target. 
The init function receives as input parameters the RPM 
of each slot and Boolean flags indicating which slots to 
use in the determination of Target.  The POR code 
must pass those flags as false if the slot is empty or 
rejected; true otherwise. 

target: An integer variable that holds the unit wide target 
temperature.  Target is set by the init function call.  
When init is called it determines the target temperature 
for each available HDD by consulting the RpmToTarget 
table.  The unit wide target temperature (i.e. this 
variable called Target) is then set to the lowest 
temperature found for all HDDs. 
*********************************************************************/
//void fan_ctrl_init(u16 slot0_Rpm, u16 slot1_Rpm, u8 useSlot0, u8 useSlot1)
void fan_ctrl_init()
{
	u8 temp0 = 0xff;
	u8 temp1 = 0xff;
	u8 useSlot0 = sata0Obj.sobj_device_state;
	u8 useSlot1 = sata1Obj.sobj_device_state;
	u8 slot0_Rpm = sata0Obj.sobj_medium_rotation_rate;
	u8 slot1_Rpm = sata1Obj.sobj_medium_rotation_rate;

	if (useSlot0 == SATA_DEV_READY)
	{
		temp0 = get_cur_target_temperature(slot0_Rpm);
		//temp0 = get_cur_target_temperature(sata0Obj.sobj_medium_rotation_rate);
	}

	if (useSlot1 == SATA_DEV_READY)
	{
		temp1 = get_cur_target_temperature(slot1_Rpm);
		//temp1 = get_cur_target_temperature(sata0Obj.sobj_medium_rotation_rate);
	}

	targetTemperature = Min(temp0, temp1);

}

 

/***************************************************************\
 fan_ctrl_exec() - 
 
A function called to execute one interation of the fan 
control state machine.  This function must be called 
every 15 seconds.

*********************************************************************/
void fan_ctrl_exec()
{
	// The first step is to take a snapshot of the HwRpmReg and calculate a RPM value 
	// from that register value and store that RPM value into the CurRPM variable
	curRPM = gpio_pulse_count << 2;		//  two ticks per revolution units & 15 sec to get once: so, RPM = HwRpmReg /2 * 2 * 4
//	gpio_accumlated_pulse_count = 0;

	//  If the Boolean flag IsFanEnabled is true then the firmware will proceed with the rest of the steps shown; otherwise firmware returns. 
	if (!isFanEnabled)
		return;

	//Next the CurFan is adjusted towards the FanGoal.
	// If the CurFan is less than the FanGoal then CurFan is incremented by the minimum of a FanStep or the distance between FanGoal and CurFan.	
	if (curFan < fanGoal)
	{
		curFan += Min(fanStep, (fanGoal - curFan));
	}
	// If the CurFan is greater than the FanGoal then CurFan is decremented by the minimum of a FanStep or the distance between CurFan and FanGoal
	else if (curFan > fanGoal)
	{
		curFan -= Min(fanStep, (curFan - fanGoal));
	}

	DBG("curFan %bx fanGoal %bx\n", curFan, fanGoal);
	// Otherwise CurFan is equal to FanGoal so no adjustment is needed
#if 1 // if dbg fan, need turn off it.
	if (curFan > fanOffThreshold)
	{
		//HwPwmReg = curFan;
		turn_on_pwm_fan(curFan);
	}
	else // curFan <= fanOffThreshold
	{
		//HwPwmReg = 0;
		turn_off_pwm_fan();
	}
#endif
	// Next the FiveMinuteCnt is incremented and checked to see if the five minute interval has elapsed.
	fiveMinuteCnt++;
	if (fiveMinuteCnt >= 20)  // 5 minute interval has elapsed
	{
		fiveMinuteCnt = 0;

		pollCnt++;
		if (pollCnt >= pollValue)
		{
			u8 curTemp;
			u8 sata_ch_flag = 0;
			
			pollCnt = 0;

			//  obtains the current unit wide temperature
			curTemp = unit_wide_cur_temperature;
//			curTemp = 5;
			MSG("curTemp %bx target %bx\n", curTemp, targetTemperature);
/*
enum FanCtrlState {
	FS_COOL = 0,			// n/a  				** 30 min  	** Decrease fan toward off 
	FS_CONTENT,			// >= Target - 4  		** 5 min  		** No action

	FS_WARM,			// >= Target  		** 15 min  	** Increase fan toward max
	FS_HOT,				// >= Target + 5  		** 10 min  	** Set fan to max

	FS_DANGER,			// >= Target + 8  		** 10 min  	** Set fan to max and warn user
	FS_EXTREME			// >= Target + 15  	** n/a  		** Thermal shutdown 
};
*/
			// Fan Algorithm
			if (curTemp >= (targetTemperature + 15))	// FS_EXTREME
			{
#ifdef DEBUG
				printf("temperature shut down ");
#endif
				drive_die_warning(TEMP_OVERHEAT_LED_BLINK_RATE);
			}
			else if (curTemp >= (targetTemperature + 8))  	// FS_DANGER
			{
				fanGoal = fanMax;
				pollValue = 2;
				// warn user
				TCond = 2;
			}
			else if (curTemp >= (targetTemperature + 5))	// FS_HOT
			{
				fanGoal = fanMax;
				pollValue = 2;
				TCond = 1;
			}
			else if (curTemp >= (targetTemperature))	// FS_WARM
			{
				pollValue = 3;
				if (fanGoal < fanMax)
				{
					fanGoal += Min(fanStep, (fanMax-fanGoal));
				}
			}
			else if (curTemp >= (targetTemperature - 4))	// FS_CONTENT
			{
				pollValue = 1;
			}
			else		// FS_COOL
			{
				pollValue = 6;
				if (fanGoal > fanOffThreshold)
				{
					fanGoal -= Min(fanStep, (fanGoal-fanOffThreshold));
				}
			}

			if (fanGoal <= fanOffThreshold)
				TCond = 0;
			
		}
	}
	else
		return;
}

/***************************************************************\
 get_fan_RPM() - 
 
A function that returns the value of the CurRPM 
variable.  This function is called by the Receive 
Diagnostic page handling code. 

*********************************************************************/
u16 get_fan_RPM()
{
	return (u16)curRPM;
}

/***************************************************************\
 getTCond() - 
 
A function that returns the value of the TCond variable.  
This function is called by the Receive Diagnostic page 
handling code.

*********************************************************************/
u8 getTCond()
{
	return TCond;
}

/***************************************************************\
 forceFanOnDiag() - 
 
A function that does thes following: 
*  Forces the HwPwmReg to the value of FanMax. 
*  Sets CurFan to FanMax 
*  Sets FanGoal to FanMax 
*  Sets FiveMinuteCnt to zero 
This function is called by the Send Diagnostic page 
handling code.  Since this function sets the 
FiveMinuteCnt to zero the effect of the "force fan on" 
will last for at least five minutes before the fan control 
state machine regains control of the cooling fan.

*********************************************************************/
void forceFanOnDiag()
{
	curFan = fanMax;
	fanGoal = fanMax;

	//HwPwmReg = fanMax;
	turn_on_pwm_fan(curFan);

	fiveMinuteCnt = 0;
}



/***************************************************************\
 forceFanOnDiag() - 
 
A function that enables or disables the fan control state 
machine.  Other portions of the firmware (e.g. Auto-Power) are required to call this function to enable or 
disable the fan algorithm when any HDD is spinning or 
when all HDDs are spun-down respectively. 
The enableFan function receives one Boolean flag 
called "enable" as an input parameter. 
If the "enable" parameter is true then the enableFan 
function performs the following steps: 
*  Set FiveMinuteCnt to 19 
*  Set PollCnt to PollValue 
*  Set the IsFanEnabled flag to true 
If the "enable" parameter is false then the enableFan 
function performs the following steps: 
*  Set CurFan to FanOffThreshold 
*  Set HwPwmReg to zero 
*  Set the IsFanEnabled flag to false

*********************************************************************/
void enableFan(u8 enable)
{
	if (enable)
	{
		fiveMinuteCnt = 19;
		pollCnt = pollValue;
		isFanEnabled = 1;
	}
	else
	{
		curFan = fanOffThreshold;
		isFanEnabled = 0;

		//HwPwmReg = 0;
		turn_off_pwm_fan();
	}
}
#ifdef DBG_FUNCTION
#if FAN_CTRL_EXEC_TICKS
void dumpFan(void)
{
	MSG("curFan %bx fanGoal %bx curRPM %lx\n", curFan, fanGoal, curRPM);
	MSG("curTemp %bx target %bx\n", unit_wide_cur_temperature, targetTemperature);

	MSG("fiveMinuteCnt %bx pollCnt %bx pollValue %bx TCond %bx\n", fiveMinuteCnt, pollCnt, pollValue, TCond);
}
void setFan(u8 idx, u32 value)
{
	switch(idx)
	{
		case 0:
			fanGoal = (u8)value;
			MSG("fanG %bx\n", fanGoal);
			break;
		case 1:
			targetTemperature = (u8)value;
			MSG("tar %bx\n", targetTemperature);
			break;
		default:
			fiveMinuteCnt = 19;
			pollCnt = value;
			MSG("5 * %bx min\n", pollCnt);
			break;
	}
	
}
#endif
#endif

#endif
// check the HW strapping information when power on
// F/W will also decide the soft strap sopporting by the HW setting
// Athena 3.5'' can't be strapped to other product
// Artemis 1U (default) can be strapped to 1R product
// Artemis 1U (default) can be strapped to Athena 2.5" or Carrera 1R
void hw_strapping_setting(void)
{
DBG("gpio %lx\n", *gpioDD);
	hw_gpio_index = (((*gpioDD & WD_PID0) >> 15) | ((*gpioDD & WD_PID1) >> 12) | ((*gpioDD & WD_PID2) >> 4) |((*gpioDD & WD_PID3) >> 4));
	MSG("gInx %bx\n", hw_gpio_index);
	hw_gpio_index = 0x03; // debug purpose
	switch (hw_gpio_index)
	{
		case 0x03:
			product_model = INTERLAKEN_2U;
			break;

		default: 
			product_model = ILLEGAL_BOOT_UP_PRODUCT;
			break;
	}
}

// F/W will determine the VID/PID is compatible with HW strapping
u8 check_compability_product(u8 productMode)
{
#ifdef INITIO_STANDARD_FW
	return 0;
#else
	MSG("p_idx %bx\n", productMode);
	if (product_detail_table[productMode].product_idx != hw_gpio_index) 
		return 1;
	else
		return 0;
#endif

}

/************************************************************************\
 identify_product() - Reads the Product ID straps to figure out what we are.

 This should be called soon after power-on-reset, before everything else.

 Returns:
	0 if the product model is OK;  1 if consistency checks failed.
 */
void identify_product(void)
{
	// Lookup and remember this product's details.
	// product_detail used to be a pointer into the detail table, but
	// pointer dereferences are very expensive on this CPU with Keil.
	xmemcpy((u8 *)(&product_detail_table[ product_model ]),
	         (u8 *)(&product_detail), sizeof(product_detail));

	DBG("product_model %bx\n", product_model);
	MSG("options %bx\n", product_detail.options);

	return;	
}

u8 check_product_config(u16 new_vid, u16 new_pid)
{
#ifdef DEBUG
printf("new_pid %x", new_pid);
#endif
	for (u8 i = 0; i < (sizeof(product_detail_table) / sizeof(product_detail)); i++)
	{

		if ((new_pid == product_detail_table[ i ].USB_PID) && (new_vid == product_detail_table[ i ].USB_VID))
		{
			return i; // product_configuration (VID, PID) is right
		}
	}
	return ILLEGAL_BOOT_UP_PRODUCT; // the PID is not supported in list
}

/***************************************************************\
 gpio_init() - Initializes and configured the GPIO pins.

 This should be called once during firmware initialization.
*********************************************************************/

void gpio_init(void)
{
	// Many products have similar features so some code could be consolidated.
	// Keeping it seperated by product makes it easier to update as
	// product plans change, though.

	activity_led = 0;				// This LED defaults to off.
	fault_led = 0;

	// The power button - if it's present - needs debouncing.
	pb_state = !POWER_BUTTON_PRESSED;
	pb_debounce = 0;
	pb_ignore_release = 0;

	// Only some products have a HDD power switch; assume there
	// isn't a power switch until determined otherwise.
	hdd_power = HDDs_BOTH_POWER_ON;
#ifdef HDR2526_LED
	fault_led_state = 0;
	HDD0_LED_OFF();
	HDD1_LED_OFF();
	JBOD_LED_OFF();
	RAID0_LED_OFF();
	RAID1_LED_OFF();
	FAULT_BUZZER_OFF();
#ifdef HDR2526_BUZZER
	DISABLE_FAIL_BUZZER_PWM();
	DISABLE_FAIL_BUZZER_FUNC();
#endif
#else	
	ACTIVITY_LED_ON(); // all the mode support activity LED
	FAULT_INDICATOR_LED_OFF();
	FAIL_SLOT0_LED_OFF();
	FAIL_SLOT1_LED_OFF();
	DISABLE_WDC_FAULT_PWM();
	DISABLE_FAULT_LED_FUNC();
#endif	
	if (product_detail.options & PO_HAS_POWER_SWITCH)
	{
		hdd_power = 0;
	}
}


/************************************************************************\
 set_power_light() - Turns on or off the power indicator.

 Arguments:
	shine		true to indicate this device is "turned on"
 */
void set_power_light(u8 shine)
{
	// Don't turn on the power light if user darkened the lights.
	if (nvm_unit_parms.mode_save.power_led_brightness < 0x80)
	{
		shine = 0;
	}

	activity_led = shine;

	// This LED shares the same GPIO and pin as the UART, so it
	// can only be controlled in non-debug builds. See  gpio_init()
#ifdef HDR2526_LED
	if (activity_led)
	{
		HDD0_LED_ON();
		HDD1_LED_ON();
		//MSG("hdd_led_on spl\n");
	}
	else
	{
		HDD0_LED_OFF();
		HDD1_LED_OFF();
		//MSG("hdd_led_off spl\n");
	}
#else
#ifndef DEBUG
	if (activity_led)
	{
#ifdef PWM
		active_led_state = ACTIVE_LED_STATUS_ON;
#endif

		ENABLE_WDC_ACTIVITY_PWM();
		ENABLE_ACTIVITY_LED_FUNC();
//		ACTIVITY_LED_ON();
	}
	else
	{
		DISABLE_WDC_ACTIVITY_PWM();
		DISABLE_ACTIVITY_LED_FUNC();
		
		ACTIVITY_LED_OFF();
#ifdef PWM
		active_led_state = ACTIVE_LED_STATUS_OFF;
#endif
	}
#endif	// !defined DEBUG
#endif
}

#ifndef HDR2526_LED
void set_fault_light(u8 shine)
{
	// Don't turn on the power light if user darkened the lights.
	fault_led = shine;

	// This LED shares the same GPIO and pin as the UART, so it
	// can only be controlled in non-debug builds. See  gpio_init()
	if (fault_led)
	{

		ENABLE_WDC_FAULT_PWM();
		ENABLE_FAULT_LED_FUNC();
//		FAULT_INDICATOR_LED_ON();
	}
	else
	{
		DISABLE_WDC_FAULT_PWM();
		DISABLE_FAULT_LED_FUNC();
		
		FAULT_INDICATOR_LED_OFF();
	}
}
#endif

/************************************************************************\
 set_activity_for_standby() - Sets the power/activity indicator to
 indicate HDD standby or not.

 Arguments:
	standby		true to indicate the HDD is standing by;
				false to stop indicating standby.
 */

static u8 sync_led_once = 0;
void set_activity_for_standby(u8 standby)
{
	MSG("brightness %bx\n", nvm_unit_parms.mode_save.power_led_brightness);
	if (nvm_unit_parms.mode_save.power_led_brightness >= 0x80)
	{
		if (standby)
		{
			activity_timeout = 0;		// Blink forever.
			activity_blink_rate = BLINK_RATE_DOT3HZ;	// ~ 0.3 Hz
		}
		else
		{
			// Stop blinking by setting the blink timeout to expire quickly.
#ifdef HDR2526_LED		
			if (clone_state != CS_CLONING)  // in cloning state, there is one rule for LEDs only
#endif				
				activity_timeout = 1; 
			reset_standby_timer();
		}
	}
}

/************************************************************************\
 tickle_activity() - Tickles the disk activity indicator.

 This function should be called periodically during a disk-access
 command to keep the activity LED blinking.

 Nothing is done if the "ignore tickles" flag is set.
 */
void tickle_activity(void)
{

	if (ignore_tickles)  return;

	// Start blinking the activity LED but set it to stop blinking
	// after about 1s. Tickles during that 1s period restarts the timer.

	// If the user darkened the lights, then don't set the
	// activity LED; assume it is always off.
#ifdef HDR2526_LED
	activity_timeout = ACTIVE_LED_TIMEOUT_1Sec;
	if (usbMode == CONNECT_USB3)
	{
		activity_blink_rate = BLINK_RATE_5HZ;
	}
	else
	{
		activity_blink_rate = BLINK_RATE_1HZ;
	}
#else
	if (nvm_unit_parms.mode_save.power_led_brightness >= 0x80)
	{
//		_disable();
		activity_timeout = ACTIVE_LED_TIMEOUT_1Sec;
		activity_blink_rate = BLINK_RATE_3HZ;
//		_enable(); 
	}
#endif	
}

void tickle_fault(void)
{// for Mirror mode
	//fault_led = 1;  // dbg
#ifdef HDR2526_LED
	hdr2526_led_init();
#else
	if ((array_status_queue[0] == AS_DEGRADED) ||
		(array_status_queue[0] == AS_REBUILDING) ||
		(array_status_queue[0] == AS_REBUILD_FAILED) ||
		(array_status_queue[0] == AS_BROKEN))
	{
		fault_timeout = 0;
		fault_blink_rate = BLINK_RATE_DOT3HZ;
	}
	else
	{
		if (fault_blink_rate != BLINK_RATE_NEVER_BLINK)  // to stop the fault blink
		{
			fault_timeout = ACTIVE_LED_TIMEOUT_1Sec;
			fault_blink_rate = BLINK_RATE_DOT3HZ;
		}
	}

	if (cur_status[SLOT0])
		FAIL_SLOT0_LED_ON();
	else
		FAIL_SLOT0_LED_OFF();
	if (cur_status[SLOT1])
		FAIL_SLOT1_LED_ON();
	else
		FAIL_SLOT1_LED_OFF();	
#endif	
}


u32 calc_Idle_timer(u8 time)
{
u32 hddStandbyTimer32;
	if (time == 0)
	{
		hddStandbyTimer32 = 0;// no power management
	}
	else if (time == 12) // 1min
	{
		hddStandbyTimer32 = 600;
	}
	else if (time == 36)// 3min
	{
		hddStandbyTimer32 = 1800;
	}
	else if (time == 60)// 5min
	{
		hddStandbyTimer32 = 3000;
	}
	else if (time == 120)// 10min
	{
		hddStandbyTimer32 = 6000;
	}
	else if (time == 180)// 15min
	{
		hddStandbyTimer32 = 9000;
	}
	else if (time == 240)// 20min
	{
		hddStandbyTimer32 = 12000;
	}
	else if (time == 241)// 30min
	{
		hddStandbyTimer32 = 18000;
	}
	else if (time == 242)// 1hours
	{
		hddStandbyTimer32 = 36000;
	}
	else if (time == 244)// 2hours
	{
		hddStandbyTimer32 = 72000;
	}
	else if (time == 246)// 3hours
	{
		hddStandbyTimer32 = 108000;
	}
	else if (time == 248)// 4hours
	{
		hddStandbyTimer32 = 144000;
	}
	else if (time == 250)// 5hours
	{
		hddStandbyTimer32 = 180000;
	}
	else
	{
		hddStandbyTimer32 = 0;// not support
	}
	return hddStandbyTimer32;
}
/************************************************************************\
 reset_standby_timer() - Reset (restart) the HDD standby timer.

 This should be called when there is disk I/O that should restart
 the HDD standby timer.

 The standby timer has to be enabled separately in order to have any effect.
 */
void reset_standby_timer(void)
{
	// There is no need to restart the standby timer if the user is
	// not aware of the disk I/O. E.g., internal activity such as
	// temperature polling should not affect the standby timer.

	if (!ignore_tickles)
	{
		// The standby timer is a 16-bit variable, so interrupts have
		// to be disabled in order to set it atomically.
//		_disable(); 
		standby_timer_expired = 0;
		standby_timer_ticker = cached_standby_timer_ticks;
//		_enable(); 
	}
}

/************************************************************************\
 update_standby_timer() - Update cached standby timer duration.

 */
void update_standby_timer(void)
{
	if (nvm_unit_parms.mode_save.standby_timer_enable) 
	{
		disable_upkeep &= ~(UPKEEP_STANDBY_TIMER_INACTIVE_S0 | UPKEEP_STANDBY_TIMER_INACTIVE_S1);
	}
	else
	{
		disable_upkeep |= (UPKEEP_STANDBY_TIMER_INACTIVE_S0 | UPKEEP_STANDBY_TIMER_INACTIVE_S1);
	}

	// The standby timer value is denominated in deciseconds and stored
	// as a 32-bit integer, as in the Power Conditions mode page.
	//
#ifdef INITIO_STANDARD_FW
	cached_standby_timer_ticks = calc_Idle_timer(globalNvram.idleTime);
#else
	cached_standby_timer_ticks = nvm_unit_parms.mode_save.standby_time;
#endif
	DBG("stdby_timer %lx\n", cached_standby_timer_ticks);
}

#ifdef HDR2526
void update_clone_standby_timer(u32 timer)
{
	/******** HDR2526_LED
	100%: HDD0 HDD1 always ON, JBOD,RAID0, RAID1 blink @ 1Hz within 20min, ((disable this)then always ON within 20min),  then OFF
		**************/
	cached_standby_timer_ticks = timer;
	reset_standby_timer();
}
#endif
/************************************************************************\
 timer_init() - Initializes timer variables.

 This should be called once during firmware initialization.

 */
void timer_init(void)
{
	// Initialize the timing variables used by the timer tick ISR,
	// to avoid triggering any events accidentally.

	activity_blink_rate = BLINK_RATE_NEVER_BLINK;
	activity_ticker = 0;
	activity_timeout = 0;
	fault_blink_rate = BLINK_RATE_NEVER_BLINK;
	fault_ticker = 0;
	fault_timeout = 0;
	
	standby_timer_ticker = 0;

#if WDC_HDD_TEMPERATURE_POLL_TICKS
	check_temp_flag = 0;
	expect_temperatureTimer32 = 0;
	unit_wide_cur_temperature = 0;
#endif	// WDC_HDD_TEMPERATURE_POLL_TICKS

	pb_debounce = 0;
	pb_timer = 0;

#if FAN_CTRL_EXEC_TICKS
	fan_ctrl_exec_flag = 0;
	fan_ctrl_exec_ticker = 0;
	fiveMinuteCnt = 0;
	pollValue = 0;
	pollCnt = 0;
	
	TCond = 0;
#endif
	// The actual timer - in this case, the watchdog - is started
	// elsewhere; see  common.c
}


/************************************************************************\
 lights_out() - Turns off all the LEDs, making the device appear "off"

 */
void lights_out(void)
{
	// The goal here is to make the device look like it's powered down.
	// Thus, for example, turning off the lock LED here means the
	// device is powered off, not that the security feature is disabled.
	// The lock icon on an EPD is left alone.
	_disable();
	activity_blink_rate = BLINK_RATE_NEVER_BLINK;
	activity_ticker = 0;
	activity_timeout = 0;

#ifdef HDR2526_LED
	if (fault_led_state == 0)
#endif		
	{
		fault_blink_rate = BLINK_RATE_NEVER_BLINK;
		fault_ticker = 0;
		fault_timeout = 0;

		fault_led = 0;
	}
	// Turn off the Power/Activity LED - every product has one.
	set_power_light(0);
	DBG("Lit off");
	_enable();
}

/************************************************************************\
 set_hdd_power() - Turns on or off power to the HDD, etc.

 Caller must initialize the HDD appropriately after turning it on,
 and standby the HDD before turning off power!

 Arguments:
	powerOnHDD	true to power on the HDD if there's FET and also power on SATA PHY; false to turn off power& shutdown SATA PHY
	#define PU_SLOT0					0x01
	#define PU_SLOT1 					0x02
	#define PWR_CTL_SLOT0_EN			0x04
	#define PWR_CTL_SLOT1_EN			0x08
	BIT_2 & BIT_3 is ctrl enable bit, if the ctrl enable bit is set, the corresponding SATA channel need be power on/off
 */
void set_hdd_power(u8 powerOnSATA, u8 delayPwrUp)
{
	u8 i = 0;
	u8 curSlot;
	u8 curSlotPwrCtrlEn = 0;
	
	#define SATA_SLOT0						0x01
	#define SATA_SLOT1						0x02
#ifdef POWER_BUTTON
	if (product_detail.options & PO_HAS_POWER_SWITCH)
	{
		// The power FET control is active-high.
		for (i = 0; i < 2; i++)
		{
			 curSlot =  SATA_SLOT0 << i;	//from slot 0 --> slot 1
			 curSlotPwrCtrlEn = PWR_CTL_SLOT0_EN<<i;
			 
			if (powerOnSATA & curSlotPwrCtrlEn)
			{
				pSataObj = (curSlot & PU_SLOT0) ? &sata0Obj : &sata1Obj;		
				sata_ch_reg = pSataObj->pSata_Ch_Reg;
				
				if (powerOnSATA & curSlot)
				{// power on drive
					if (curSlot == SATA_SLOT0)
					{// power on which slot
						POWER_ON_SATA0();
					}
					else
					{
						if(powerOnSATA & PU_SLOT0)
						{
							if (delayPwrUp)
							{
								Delay(4000);			//in order to minmize peak power comsumption
							}
						}
						POWER_ON_SATA1();
					}
					hdd_power |= curSlot;
					pSataObj->sobj_State = SATA_RESETING;
				}
				else
				{// power off drive
					
					sata_ch_reg->sata_PhyIntEn &= ~PHYDWNIEN;
					sata_ch_reg->sata_PhyIntEn;
					Delay(1);	
					if (curSlot == SATA_SLOT0)
					{
						POWER_OFF_SATA0();
					}
					else
					{
						POWER_OFF_SATA1();
					}
					hdd_power &= ~curSlot;
				//	pSataObj->sobj_State = SATA_PWR_DWN;
				}
			}
		}
	}
	else
#endif		
	{
		// Without a power FET, the HDD is always on.
		hdd_power |= ((powerOnSATA & (PWR_CTL_SLOT0_EN|PWR_CTL_SLOT1_EN)) >> 2);
	}

#if WDC_BRIDGE_POWER_SAVE // phy power control
	for (i = 0; i <2; i++)
	{
		curSlot =  SATA_SLOT1 >> i;		//from slot 1 --> slot 0
		curSlotPwrCtrlEn = PWR_CTL_SLOT1_EN >> i;
		
		if (powerOnSATA & curSlotPwrCtrlEn)
		{
			_disable();

			pSataObj = (curSlot & SATA_SLOT0) ? &sata0Obj : &sata1Obj;	
			sata_ch_reg = pSataObj->pSata_Ch_Reg;
			
			if (powerOnSATA & curSlot)	//sata pll power up
			{
				sata_phy_pwr_ctrl(SATA_PHY_PWR_UP, curSlot);
#ifndef DBG_FUNCTION
				*led0_Ctrl0 = *led0_Ctrl0 | (WDC_LED_TIMER_UNIT << 24) | 0x55; // 1.051ms per cycle & 50% duty
#endif
				Delayus(10);
				// turn on SATA PHY
#ifdef FORCE_NO_SATA3
				sata_ch_reg->sata_PhyCtrl = (sata_ch_reg->sata_PhyCtrl & (~PHYGEN3))
				| (PHYPWRUP|PHYGEN2|PHYGEN1);
#else
				sata_ch_reg->sata_PhyCtrl |= (PHYPWRUP|PHYGEN3|PHYGEN2|PHYGEN1);
#endif
				hdd_power |= curSlot;
#ifdef DATABYTE_RAID
				if (cfa_active == 0)
#endif
				sata_HardReset(pSataObj);		//sata_HardRest will set the sobj_State.
				//hdd_power |= curSlot;
				sata_InitSataObj(pSataObj);
			}
			else	//sata pll power down
			{
				Delay(1);

				sata_ch_reg->sata_PhyIntEn &= ~PHYDWNIEN;
				sata_ch_reg->sata_PhyIntEn;
				
				if (!(sata_ch_reg->sata_PhyStat & PHYRDY))
				{
					//no more sata phy ready, drive must have been power down
					// turn off SATA PHY, etc
					#ifdef DBG_FUNCTION
					DBG("\t\t\tSATA PHY Power Down\n");
					#endif
					
					sata_ch_reg->sata_PhyCtrl &= (~PHYPWRUP|PHYGEN3|PHYGEN2|PHYGEN1);
				}
				
#ifndef DBG_FUNCTION
				*led0_Ctrl0 = (*led0_Ctrl0 & 0xF0FFFFFF) | (WDC_LED_TIMER_UNIT_LOW_CLOCK << 24) ; // 0.978ms/0.819ms per cycle & 50% duty
#endif
				
				sata_phy_pwr_ctrl(SATA_PHY_PWR_DWN, curSlot);

				hdd_power &= ~curSlot;
				pSataObj->sobj_State = SATA_PWR_DWN;
				
			}
			_enable();

			// From a functional point of view, we can't talk to the HDD
			// when the SATA channel is turned off, so we may as well
			// consider the HDD to be "off" if the SATA channel is turned off.
		}
	}
#endif	// WDC_BRIDGE_POWER_SAVE
}

// when hdd spin up from standby state, start the 7 secs timer to poll the phy ready & fis 34 is recieved
// after timer out, F/W will response unit not ready to host by next request_sense or sense_iu
void handle_rsp_unit_nrd(u8 mscMode)
{
	MSG("unit b rdy\n");
	PCDB_CTXT pCtxt;

	PEXT_CCM pCcm = NULL;
	// there's only sobj que or ncq_que can be pending,  because f/w will set the uas_cmdiu_pause when 1 task is que
	if (sata0Obj.sobj_State > SATA_READY)
		pSataObj = &sata0Obj;
	else
		pSataObj = &sata1Obj;
	
	if (mscMode == MODE_UAS)
	{
		if ( pSataObj->sobj_que )
		{
			pCcm= pSataObj->sobj_que;
DBG("d q\n");
		}
		else if ( pSataObj->sobj_ncq_que )
		{
DBG("n q\n");
			pCcm= pSataObj->sobj_ncq_que;
		}
	}
	else
	{
		if ( pSataObj->sobj_que )
		{
			pCcm= pSataObj->sobj_que;
		}
	}
	
	if (pCcm == NULL)
	{
		rsp_unit_not_ready = 0;
		return;
	}
	
	pCtxt = pCcm->scm_pCdbCtxt;
	u32  bit32 = 1 << pCcm->scm_ccmIndex;
	DBG("ccmIdx %bx\n", pCcm->scm_ccmIndex);
	// remove the sata ccm which is pending in que
	sata_DetachCCM(pSataObj, bit32); //
	pSataObj->sobj_sBusy &= (~bit32);
	// release ncq que or dma que
	// fw will save 1 task in the que when power up from standby state
	if (pSataObj->sobj_ncq_que)
	{
		pSataObj->sobj_ncq_que = NULL;
	}
	else if (pSataObj->sobj_que)
		pSataObj->sobj_que = NULL;
	
	// keep the sataObj's state
//	uas_ci_paused = 0;
	rsp_unit_not_ready = 0;
	DBG("CTXT idx %bx\n", pCtxt->CTXT_Index);
	hdd_err_2_sense_data(pCtxt, ERROR_UA_BECOMING_READY);

	if ((pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR) == 0)
	{		
		pCtxt->CTXT_FLAG = CTXT_FLAG_U2B;
		pCtxt->CTXT_PHASE_CASE = 0x0200; // under run case
#ifdef FAST_CONTEXT
		pCtxt->CTXT_No_Data = MSC_RX_CTXT_NODATA;
#else
		pCtxt->CTXT_No_Data = (MSC_RX_CTXT_VALID|MSC_RX_CTXT_NODATA) >> 8;
#endif
		*usb_Msc0RxCtxt = MSC_RX_CTXT_VALID;
		*usb_Msc0DOutCtrl = MSC_DOUT_HALT | MSC_DOUT_FLOW_CTRL;
		Delay(1);
		rst_dout(pCtxt);
		usb_ctxt.curCtxt = pCtxt;
		rw_flag = RD_WR_TIME_OUT; 
	}
	else
		usb_device_no_data(pCtxt);	
}

void enable_GSPI_function(void)
{
	// enable the function select of GPIO 4, 5, 6, 7 to GSPI SCK, SDI, SDO, CS
	*gpioCtrlFuncSel0 = (*gpioCtrlFuncSel0 & ~0x0000ff00) | (GSPI_SCK_SEL | GSPI_SDI_SEL | GSPI_SDO_SEL | GSPI_CS_SEL);
}

// test purpose to test 5 PWM for WD's LED & FAN out
void enable_PWM_function(void)
{
#ifdef JTAG_DBG
	*gpioCtrlFuncSel0 = (*gpioCtrlFuncSel0 & ~0x03FF00C0) | (WD_FAN_PWM_SEL |JTAG_TDO|JTAG_TRST|JTAG_TDI|JTAG_TMS|JTAG_TCK);
#else
#if FAN_CTRL_EXEC_TICKS
	*gpioCtrlFuncSel0 = (*gpioCtrlFuncSel0 & ~0x03C000C0) | (WD_FAN_PWM_SEL |WD_ACTIVITY_PWM_SEL|WD_FAULT_PWM_SEL);
#else
#ifdef HDR2526_LED
#ifdef HDR2526_BUZZER
	*gpioCtrlFuncSel0 = (*gpioCtrlFuncSel0 & ~0x03000000) | (FAIL_BUZZER_PWM_SEL);
#endif
#else
	*gpioCtrlFuncSel0 = (*gpioCtrlFuncSel0 & ~0x03C00000) | (WD_ACTIVITY_PWM_SEL|WD_FAULT_PWM_SEL);
#endif
#endif
#endif
}

// the basic clock is 50MHz/ div 
// should try the div 1, 2, 4, 8
void set_GSPI_clock(u8 div)
{
	*gSPI_ctrl1 = (*gSPI_ctrl1 & ~SPI_DIV) | (div << 16);
}

void gSPI_byte_xfer(u8 data)
{
	*gSPI_Interrupt = SPI_DONE;
	*gSPI_ctr_dir = SPI_CS;
	*gSPI_ctr_cmd = data;
	while(1)
	{
		if (*gSPI_Interrupt & SPI_DONE)
		{
			*gSPI_Interrupt = SPI_DONE;
			*gSPI_ctr_dir = 0;
			break;
		}
	}
}

void gSPI_burst_xfer(u8 *buf, u16 xferCnt, u8 gSPI_mode, u8 dir)
{
	u16 i;
	*gSPI_Interrupt = SPI_DONE;
	if (gSPI_mode == GSPI_SYNC_BURST_MODE_9BITS)
		*gSPI_ctr_mask = *gSPI_ctr_mask & (~SPI_BIT_MASK) | SPI_BIT_MASK_9BITS;
	else
	{
		*gSPI_ctr_wait = (*gSPI_ctr_wait & ~SPI_BYTE_WAIT) | 0x21;
		*gSPI_ctr_mask = 0;
	}
	
	*gSPI_ctrl1 = (*gSPI_ctrl1 & ~SPI_XFERCNT) | xferCnt;

	// 
	if (dir == GSPI_DIR_OUT)
	{
		*gSPI_ctr_dir |= SPI_DIR_OUT |SPI_CS;
		MSG("gspi Int %lx\n", *gSPI_Interrupt);
		for (i = xferCnt; i > 3; i-=4, ((u32 *)buf)++)
		{
			*gSPI_data_Memory_Port = *((u32 *)buf);
			MSG("%lx\t", *((u32 *)buf));
		}
	
		for (;i > 0; i--, buf++)
		{
			MSG("bdata %bx\n", *buf);
			*((u8 *)gSPI_data_Memory_Port) = *buf;
		}
		*gSPI_ctrl2 = SPI_BURSTEN;
	}
	else
	{
		*gSPI_ctr_dir  = (*gSPI_ctr_dir & ~SPI_DIR | SPI_CS);
		for (i = xferCnt; i > 3; i-=4)
		{
			*((u32 *)buf++) = *gSPI_data_Memory_Port;	
		}

		for (; i > 0;i--, buf++)
		{
			*buf = *((u8 *)gSPI_data_Memory_Port);
		}

		*gSPI_ctrl2 = SPI_BURSTEN;
	}
	
	while(1)
	{
		if (*gSPI_Interrupt & SPI_DONE)
		{
			
			*gSPI_Interrupt = SPI_DONE;
			*gSPI_ctrl2 = 0;
			*gSPI_ctr_dir = 0; 
			*gSPI_data_Memory_Address_reg = 0;
			break;
		}
		MSG(".");
	}
}
/*******************************************************************
	FAN_OFF			0x00	// PWM 2.4%
	FAN_LOW_SPEED	0x01	// 4000RPM, 66.7 rotation per sec
	FAN_MED_SPEED	0x02	// 5868RPM, 97.8 rotation per sec 
	FAN_HIGH_SPEED	0x03	// 7732RPM, 128.87 rotation per sec
	FAN_MAX_SPEED	0x04	// PWM 99.88%
********************************************************************/
void set_fan_state(u8 fan_state)
{
	switch (fan_state)
	{
		case FAN_OFF: 
			*led4_Ctrl0 = (*led4_Ctrl0 & ~0x0000FF00) | (6 << 8); // 2.4% PWM output
			// enable LED PWM mode, PWM forever, dir: bidirection, period 250(350 * 163.84us = 4.096ms). 
			*led4_Ctrl1 |= (LED_ENABLE|LED_FOREVER|LED_PWM_MODE|LED_AUTO_MODE_BIDIR|(WDC_PWM_PERIOD << 8));
			break;
		case FAN_LOW_SPEED:
			*led4_Ctrl0 = (*led4_Ctrl0 & ~0x000000FF) | 0xBB; // 22 * 655.36 = 14.4ms per cycle
			// enable LED PWM mode, PWM forever, dir: bidirection, period 250(350 * 163.84us = 4.096ms). 
			*led4_Ctrl1 = 0;//PWM blink mode	
			break;
		case FAN_MED_SPEED:
			*led4_Ctrl0 = (*led4_Ctrl0 & ~0x000000FF) | 0x88; // 16 * 655.36 = 10.5ms per cycle
			// enable LED PWM mode, PWM forever, dir: bidirection, period 250(350 * 163.84us = 4.096ms). 
			*led4_Ctrl1 = 0;//PWM blink mode				
			break;
		case FAN_HIGH_SPEED:
			*led4_Ctrl0 = (*led4_Ctrl0 & ~0x000000FF) | 0x66; // 12 * 655.36 = 7.87ms per cycle
			// enable LED PWM mode, PWM forever, dir: bidirection, period 250(350 * 163.84us = 4.096ms). 
			*led4_Ctrl1 = 0;//PWM blink mode					
			break;
		case FAN_MAX_SPEED:
			// why not fully on???
			break;
		default:
			break;
	}
}

#ifdef DBG_FUNCTION
void dump_stdTimer(void)
{
	MSG("std timer %lx, cachedTm %lx, tmEx %bx, disUpkeep %bx\n", standby_timer_ticker, cached_standby_timer_ticks, standby_timer_expired, disable_upkeep);
}
#endif

#ifdef HDR2526_LED
void fault_leds_active(u8 enable)
{
	if (enable)
	{
		fault_timeout = 0;		// Blink forever.
		fault_blink_rate = BLINK_RATE_1HZ;	// ~ 1 Hz
		fault_led_state = 1;
	}
	else
	{
		fault_timeout = 0;		// Blink forever.
		fault_blink_rate = 0;	// ~ 1 Hz
		fault_led_state = 0;
	}
}

void tickle_rebuild(void)
{
#ifdef DATABYTE_RAID
	if ((raid_rebuild_verify_status & RRVS_ONLINE) == 0)
		fault_timeout = 0;		// Blink forever offline
	else
#endif
	fault_timeout = ACTIVE_LED_TIMEOUT_3Sec;		
	fault_blink_rate = BLINK_RATE_1D6HZ;	// ~ 1 Hz
	fault_led_state = 0;
}

void fault_leds_run(void)
{
#ifdef LEDS
//MSG("faultLed run\n");
#endif
	if (fault_led_state > 0x08)
		fault_led_state = 1;
#ifdef LEDS
	//MSG("  %bx\n", fault_led_state);
#endif
	
	// blink with once long and twice short
	switch(fault_led_state++)
	{
		case 0x01:
		case 0x02:
		case 0x05:
		case 0x07:
			if (clone_state == CS_CLONE_FAIL)
			{
				HDD0_LED_ON();
				HDD1_LED_ON();
				MSG("hdd_led_on flr\n");
			}
#ifdef DATABYTE_RAID
#ifdef HDR2526_GPIO_HIGH_PRIORITY
//#ifdef SUPPORT_RAID1
			else if (raid_mode == RAID1_MIRROR)
			{
				if (cur_status[SLOT0] == SS_FAILED)
					HDD0_LED_ON();
				if (cur_status[SLOT1] == SS_FAILED)
					HDD1_LED_ON();
			}
//#endif
#endif
#endif
#ifdef HDR2526_BUZZER
			ENABLE_FAIL_BUZZER_PWM();
			ENABLE_FAIL_BUZZER_FUNC();
#else			
			FAULT_BUZZER_ON();
#endif
			break;
		case 0x03:
		case 0x04:
		case 0x06:
		case 0x08:	
			if (clone_state == CS_CLONE_FAIL)
			{
				HDD0_LED_OFF();
				HDD1_LED_OFF();
				MSG("hdd_led_off spl\n");
			}
#ifdef DATABYTE_RAID
#ifdef HDR2526_GPIO_HIGH_PRIORITY
//#ifdef SUPPORT_RAID1
			else if (raid_mode == RAID1_MIRROR)
			{
				if (cur_status[SLOT0] == SS_FAILED)
					HDD0_LED_OFF();
				if (cur_status[SLOT1] == SS_FAILED)
					HDD1_LED_OFF();
			}
//#endif
#endif
#endif			
#ifdef HDR2526_BUZZER
			DISABLE_FAIL_BUZZER_PWM();
			DISABLE_FAIL_BUZZER_FUNC();
#endif
			FAULT_BUZZER_OFF();
			break;
	}
}

void hdr2526_led_init(void)
{
	if ((USB_VBUS()) && (cur_status[SLOT0] <= SS_REBUILDING)) // good && rebuilding
		HDD0_LED_ON();
	else
		HDD0_LED_OFF();
	if ((USB_VBUS()) && (cur_status[SLOT1] <= SS_REBUILDING))
		HDD1_LED_ON();
	else
		HDD1_LED_OFF();
	
	u8 arrayLed;
	if (metadata_status == MS_GOOD)
		arrayLed = arrayMode;
	else // if (ram_rp[0].array_mode != NOT_CONFIGURED)
	{
		arrayLed = (cur_status[SLOT1] == SS_GOOD) ? ram_rp[1].array_mode : ram_rp[0].array_mode;
	}
	MSG("arrled %bx raidm %bx am %bx\n", arrayLed, raid_mode, arrayMode);
	fault_leds_active(0);
	JBOD_LED_OFF();
	//MSG("jbod_off init\n");
	RAID0_LED_OFF();
	RAID1_LED_OFF();
#ifdef HDR2526_BUZZER
	DISABLE_FAIL_BUZZER_PWM();
	DISABLE_FAIL_BUZZER_FUNC();
#endif
	FAULT_BUZZER_OFF();

	switch (arrayLed)
	{
		case JBOD:
			JBOD_LED_ON();
			//MSG("~jbod_on init\n");
            //if ((cur_status[SLOT1] == SS_FAILED) || (cur_status[SLOT1] == SS_FAILED))
			if ((cur_status[SLOT0]) || (cur_status[SLOT1]))
				fault_leds_active(1);
			break;
#ifndef DATASTORE_LED
#ifdef SUPPORT_RAID0
        case RAID0_STRIPING_DEV:
            RAID0_LED_ON();
            if ((cur_status[SLOT0]) || (cur_status[SLOT1]))
                fault_leds_active(1);
            break;
#endif
#ifdef SUPPORT_RAID1
        case RAID1_MIRROR:
            RAID1_LED_ON();
            //if ((cur_status[SLOT1] == SS_FAILED) || (cur_status[SLOT1] == SS_FAILED))
            if ((cur_status[SLOT0] > SS_REBUILDING) || (cur_status[SLOT1] > SS_REBUILDING))
                fault_leds_active(1);
            break;
#endif
#ifdef SUPPORT_SPAN
        case SPANNING:
			JBOD_LED_ON();
			RAID0_LED_ON();
			RAID1_LED_ON();
			if ((cur_status[SLOT0]) || (cur_status[SLOT1]))
				fault_leds_active(1);
			break;
#endif
#endif
		default:
			break;
	}
}

void hdr2526_led_off(u8 turn_off_case)
{
	_disable();
	switch (turn_off_case)
	{
		case ALL_IS_OFF:
			activity_blink_rate = BLINK_RATE_NEVER_BLINK;
			activity_ticker = 0;
			activity_timeout = 0;
			activity_led = 0;

			fault_blink_rate = BLINK_RATE_NEVER_BLINK;
			fault_ticker = 0;
			fault_timeout = 0;
			fault_led = 0;

			JBOD_LED_OFF();
			RAID0_LED_OFF();
			RAID1_LED_OFF();
			HDD0_LED_OFF();
			HDD1_LED_OFF();
			break;
		case ACT_IDLE_OFF:
			activity_blink_rate = 0;
			activity_ticker = 0;
			activity_timeout = 0;
			activity_led = 0;

			HDD0_LED_OFF();
			HDD1_LED_OFF();
			JBOD_LED_OFF();
			RAID0_LED_OFF();
			RAID1_LED_OFF();
			sync_led_once = 1;
			break;
	}
	
	_enable();
}

static u8 clone_led_state;
#ifdef DB_CLONE_LED
enum ClonePercentageComplete {
	GE_00 = 0x00,
	GE_25 = 0x01,
	GE_50 = 0x02,
	GE_75 = 0x03,
	E_100 = 0x04,
};
#else
enum ClonePercentageComplete {
	GE_00 = 0x00,
	GE_20 = 0x01,
	GE_40 = 0x02,
	GE_60 = 0x03,
	GE_80 = 0x04,
	E_100 = 0x05,
};
#endif

#if 0	// new request, db.606, disable it
void clone_leds_idle_timeout(u32 timer)
{
	/******** HDR2526_LED
	100%: HDD0 HDD1 always ON, JBOD,RAID0, RAID1 blink @ 1Hz within 20min, ((disable this)then always ON within 20min), then OFF
		**************/
	if (standby_timer_ticker < timer)
	{
		// just to turn off clone led
		fault_leds_active(0);

		JBOD_LED_ON();
		RAID0_LED_ON();
		RAID1_LED_ON();
	}
	else
	{
		tickle_rebuild();
	}
}
#endif

#ifdef DB_CLONE_LED
void clone_leds_run(void)
{
	u8 tmp_clone_step;
    {
	 	tmp_clone_step = (100*rebuildLBA)/array_data_area_size;
		if (tmp_clone_step < 25)
			tmp_clone_step = GE_00;
		else if (tmp_clone_step < 50)
			tmp_clone_step = GE_25;
		else if (tmp_clone_step < 75)
			tmp_clone_step = GE_50;
		else if (tmp_clone_step < 100)
			tmp_clone_step = GE_75;
		else
			tmp_clone_step = E_100;
	}

	switch (tmp_clone_step)
	{
		case GE_00:  // 25, 50, 75, 100 run as light water:
            CLONE_LED_25_OFF();      
			RAID0_LED_OFF();
			RAID1_LED_OFF();
			CLONE_LED_100_OFF();
            #ifdef  DATASTORE_LED
			        JBOD_LED_OFF();//Clone mode GPIO17_PIN pull low
            #endif
            if (clone_led_state >= 0x04)  		//  1 -...-> 4 --> 1
				clone_led_state = 0x00;
			break;
		case GE_25:  // 25 always ON, 50, 75, 100 run as light water: 
            CLONE_LED_25_ON();      
			RAID0_LED_OFF();
			RAID1_LED_OFF();
			CLONE_LED_100_OFF();
            #ifdef  DATASTORE_LED
                    JBOD_LED_OFF();//Clone mode GPIO17_PIN pull low
            #endif
			if (clone_led_state >= 0x04)  		// 2 -...-> 4 --> 2
				clone_led_state = 0x01;
			break;
		case GE_50:	 // 25, 50 always ON, 75, 100 run as light water: 
            CLONE_LED_25_ON();      
			RAID0_LED_ON();
			RAID1_LED_OFF();
			CLONE_LED_100_OFF();
            #ifdef  DATASTORE_LED
                    JBOD_LED_OFF();//Clone mode GPIO17_PIN pull low
            #endif
            if (clone_led_state >= 0x04)  		// 3 -...-> 4 --> 3
				clone_led_state = 0x02;
			break;
		case GE_75:  // 25, 50, 75 always ON, 100 blink @ 1Hz
            CLONE_LED_25_ON();      
			RAID0_LED_ON();
			RAID1_LED_ON();
			//CLONE_LED_100_OFF();
			#ifdef  DATASTORE_LED
                    JBOD_LED_OFF();//Clone mode GPIO17_PIN pull low
            #endif
            if (clone_led_state >= 0x05)  		// 4 -...-> 5 --> 4
				clone_led_state = 0x03;
			break;
		case E_100:  // 25, 50, 75, 100 always ON
            CLONE_LED_25_ON();      
			RAID0_LED_ON();
			RAID1_LED_ON();
			CLONE_LED_100_ON();
            #ifdef  DATASTORE_LED
                    JBOD_LED_OFF();//Clone mode GPIO17_PIN pull low
            #endif
            clone_led_state = 0x05;
			
			break;

		default:
			break;
	}

	clone_led_state++;

	switch (clone_led_state)
	{
		case 0x01:
			CLONE_LED_25_ON();      
			break;
		case 0x02:
			RAID0_LED_ON();
			break;
		case 0x03:
			RAID1_LED_ON();
			break;
		case 0x04:
			CLONE_LED_100_ON();
			break;
        case 0x05:
			CLONE_LED_100_OFF();
			break;
		default:
			break;
	}
}

#else
void clone_leds_run(void)
{
#ifdef LEDS
//MSG("cloneLed run\n");
#endif
	u8 tmp_clone_step;

	{
	 	tmp_clone_step = (100*rebuildLBA)/array_data_area_size;
		if (tmp_clone_step < 20)
			tmp_clone_step = GE_00;
		else if (tmp_clone_step < 40)
			tmp_clone_step = GE_20;
		else if (tmp_clone_step < 60)
			tmp_clone_step = GE_40;
		else if (tmp_clone_step < 80)
			tmp_clone_step = GE_60;
		else if (tmp_clone_step < 100)
			tmp_clone_step = GE_80;
		else
			tmp_clone_step = E_100;
	}

#ifdef LEDS
//	MSG("step %bx", tmp_clone_step);
#endif
	switch (tmp_clone_step)
	{
		case GE_00:  // hdd0, hdd1, jbod, raid0, raid1 run as light water:
			RAID1_LED_OFF();
			HDD0_LED_OFF();
			HDD1_LED_OFF();
			JBOD_LED_OFF();
			RAID0_LED_OFF();
			if (clone_led_state >= 0x05)  		//  1 -...-> 5 --> 1
				clone_led_state = 0x00;
			break;
		case GE_20:  // hdd0 always ON, hdd1, jbod, raid0, raid1 run as light water: 
			RAID1_LED_OFF();
			HDD0_LED_ON();
			HDD1_LED_OFF();
			JBOD_LED_OFF();
			RAID0_LED_OFF();
			if (clone_led_state >= 0x05)  		// 2 -...-> 5 --> 2
				clone_led_state = 0x01;
			break;
		case GE_40:	 // hdd0, hdd1 always ON, jbod, raid0, raid1 run as light water: 
			RAID1_LED_OFF();
			HDD0_LED_ON();
			HDD1_LED_ON();
			JBOD_LED_OFF();
			RAID0_LED_OFF();
			if (clone_led_state >= 0x05)  		// 3 -...-> 5 --> 3
				clone_led_state = 0x02;
			break;
		case GE_60:  // hdd0, hdd1, jbod always ON, raid0, raid1 run as light water: 
			RAID1_LED_OFF();
			HDD0_LED_ON();
			HDD1_LED_ON();
			JBOD_LED_ON();
			RAID0_LED_OFF();
			if (clone_led_state >= 0x05)  		// 4 --> 5 --> 4
				clone_led_state = 0x03;
			break;
		case GE_80:  // hdd0, hdd1, jbod, raid0 always ON, raid1 blink @ 1Hz
			//RAID1_LED_OFF();
			HDD0_LED_ON();
			HDD1_LED_ON();
			JBOD_LED_ON();
			RAID0_LED_ON();
			if (clone_led_state >= 0x06)		// 5 --> 6 --> 5
				clone_led_state = 0x04;
			
			break;
		case E_100:  // hdd0, hdd1  always ON, jbod, raid0, raid1 blink @ 1Hz
			//RAID1_LED_OFF();
			HDD0_LED_ON();
			HDD1_LED_ON();
			//JBOD_LED_OFF();
			//RAID0_LED_OFF();

		 	if (clone_led_state >= 0x08)		// 7 --> 8 --> 7
				clone_led_state = 0x06;
			else if (clone_led_state == 0x05)	// cross the boundary: GE_80 -> E_100
			{
				JBOD_LED_OFF();
				RAID0_LED_OFF();
				RAID1_LED_OFF();
			}
			break;

		default:
			break;
	}

	clone_led_state++;
#ifdef LEDS
//	MSG("  %bx\n", clone_led_state);
#endif
	switch (clone_led_state)
	{
		case 0x01:
			HDD0_LED_ON();
			break;
		case 0x02:
			HDD1_LED_ON();
			break;
		case 0x03:
			JBOD_LED_ON();
			break;
		case 0x04:
			RAID0_LED_ON();
			break;
		case 0x05:
			RAID1_LED_ON();
			break;
		case 0x06:
			RAID1_LED_OFF();
			break;
		case 0x07:
			JBOD_LED_ON();
			RAID0_LED_ON();
			RAID1_LED_ON();
			break;
		case 0x08:
			JBOD_LED_OFF();
			RAID0_LED_OFF();
			RAID1_LED_OFF();
			break;	

		default:
			break;
		
	}
}
#endif

#ifdef DATASTORE_LED
void blink_clone_leds(void)
{
            CLONE_LED_25_ON();      
            RAID0_LED_ON();
            RAID1_LED_ON();
            CLONE_LED_100_ON();
            Delay(300);
            CLONE_LED_25_OFF();      
            RAID0_LED_OFF();
            RAID1_LED_OFF();
            CLONE_LED_100_OFF();
            Delay(300);
}
#endif
void rebuild_leds_run(void)
{
#ifdef LEDS
//MSG("rbLed run\n");
#endif
	u8 tmp_rebuild_step;

	{
	 	tmp_rebuild_step = (100*rebuildLBA)/array_data_area_size;
		if (tmp_rebuild_step < 33)
			tmp_rebuild_step = 0x00;
		else if (tmp_rebuild_step < 66)
			tmp_rebuild_step = 0x01;
		else
			tmp_rebuild_step = 0x02;
	}

//	MSG("step %bx", tmp_clone_step);
	switch (tmp_rebuild_step)
	{
		case 0x00:  // jbod, raid0, raid1 run as light water:
			RAID1_LED_OFF();
			JBOD_LED_OFF();
			RAID0_LED_OFF();
			if (clone_led_state >= 0x03)  		//  1 -...-> 3 --> 1
				clone_led_state = 0x00;
			break;
		case 0x01:  // jbod always ON, raid0, raid1 run as light water: 
			RAID1_LED_OFF();
			JBOD_LED_ON();
			RAID0_LED_OFF();
			if (clone_led_state >= 0x03)  		// 2 -...-> 3 --> 2
				clone_led_state = 0x01;
			break;
		case 0x02:	 // jbod, raid0 always ON, raid1 blink: 
			RAID1_LED_OFF();
			JBOD_LED_ON();
			RAID0_LED_ON();
			if (clone_led_state >= 0x04)  		// 3 -...-> 4 --> 3
				clone_led_state = 0x02;
			break;
		default:
			break;
	}

	clone_led_state++;
//	MSG("  %bx\n", clone_led_state);
	switch (clone_led_state)
	{
		case 0x01:
			JBOD_LED_ON();
			//MSG("jbod_on rlr\n");
			break;
		case 0x02:
			RAID0_LED_ON();
			break;
		case 0x03:
			RAID1_LED_ON();
			break;
		case 0x04:
			RAID1_LED_OFF();
			break;
					
		default:
			break;
		
	}
}
#endif
//u32 index =0;

_Interrupt2 void isr_timer0()
{
#if WDC_HDD_TEMPERATURE_POLL_TICKS
	if (!check_temp_flag )
	{
		if(!expect_temperatureTimer32)
		{
			check_temp_flag = 1;
		}
		else
			expect_temperatureTimer32--;
	}
#endif	// WDC_HDD_TEMPERATURE_POLL_TICKS

	if (raid1_active_timer)
		raid1_active_timer--;
	// Always count down the HDD standby timer and set the Expired
	// flag when it reaches 0; whether or not the HDD is actually
	// spun down is determined elsewhere - see Usb.c
	if (standby_timer_ticker)
	{
		if (--standby_timer_ticker == 0)  standby_timer_expired = 1;
	}

	if (DISABLE_U1U2() == 0)
	{
		if (U1U2_ctrl_timer)
		{
			if ((--U1U2_ctrl_timer == 0) && (usbMode == CONNECT_USB3))
			{
				if (usb_ctxt.ctxt_que == NULL)
				{	
					if ((u1U2_reject_state == U1U2_REJECT) && ((*usb_IntStatus_shadow & CDB_AVAIL_INT) == 0))
					{
						*usb_DevCtrlClr = (USB3_U1_REJECT | USB3_U2_REJECT);
						u1U2_reject_state = U1U2_ACCEPT;
					}
				}
			}
		}
	}

#if FAN_CTRL_EXEC_TICKS
	if (--gpio_count_timetick == 0)
	{
		gpio_count_timetick = GPIO_COUNTER_TIMEUNIT_7DOT5_SEC;
		gpio_pulse_count = *GPIO_Counter_count;
		gpio_accumlated_pulse_count += gpio_pulse_count;
		DBG("Tach C %lx, AC %lx\n", gpio_pulse_count, gpio_accumlated_pulse_count);
	}

	if (fan_ctrl_exec_ticker)
	{
		if (--fan_ctrl_exec_ticker == 0)		// 15 seconds
		{
			fan_ctrl_exec_flag = 1;
		}
	}
#endif
	
#if WDC_ENABLE_QUICK_ENUMERATION
#ifdef SUPPORT_UNSUPPORTED_FLASH
	if (flash_operation_timer)
	{
		flash_operation_timer--;
	}
#endif
#endif

#ifdef DB_CLONE_LED
    //in clone 
    //if a disk is present, the LED is on
    //if a disk is not present, the LED is off
    if (FW_state != AP_USB_ACTIVE)
    {
        if(sata0Obj.sobj_device_state == SATA_DEV_READY)
        {
            HDD0_LED_ON();
        }
        else
        {
            HDD0_LED_OFF();
        }

        if(sata1Obj.sobj_device_state == SATA_DEV_READY)
        {
            HDD1_LED_ON();
        }
        else
        {
            HDD1_LED_OFF();
        }
    }
#endif

#ifdef HDR2526_LED
    if (fault_blink_rate)
    {
        if (fault_ticker-- == 0)
        {
            fault_ticker = fault_blink_rate -1;
            fault_led = !fault_led;		// Blink
            if (fault_led_state)
                fault_leds_run();
            else if ((clone_state == CS_CLONING) || (clone_state == CS_CLONE_DONE_HOLD))
                clone_leds_run();
			else if ((raid_mirror_status) && (raid1_active_timer == 0))
				rebuild_leds_run();
		}
	}

	switch (fault_timeout)
	{
		case 0:						// Timeout is disabled, so do nothing.
			break;

		case 1:						// Ding! Blink timeout: stop blinking
			fault_led = 0;		// and turn off the fault LED.
			fault_blink_rate = 0;
			fault_ticker = 0;
			fault_timeout = 0;
			break;

		default:
			fault_timeout--;
	}

	if (fault_led_state)
	{
		if (fault_led)
		{
//#ifdef HDR2526_GPIO_HIGH_PRIORITY		
	//		u8 gpio_raid = hard_raid_mapping();
//#endif
			if (raid_mode == JBOD)
			{
				if (clone_state != CS_CLONE_FAIL)
					//JBOD_LED_ON();
					;
			}
//#ifdef SUPPORT_RAID0
			else if (raid_mode == RAID0_STRIPING_DEV)
				//RAID0_LED_ON();
				;
//#endif
//#ifdef SUPPORT_RAID1
			else if (raid_mode == RAID1_MIRROR)
				//RAID1_LED_ON();
				;
//#endif
			else
			{
				JBOD_LED_ON();
				RAID0_LED_ON();
				RAID1_LED_ON();
			}
			//MSG("jbod_on fls\n");
		}
		else
		{
			JBOD_LED_OFF();
			RAID0_LED_OFF();
			RAID1_LED_OFF();
		}
	}
	// Figure out what the activity LED should show next.
	if (activity_blink_rate)
	{
		if (activity_ticker-- == 0)
		{
			activity_ticker = activity_blink_rate -1;
			activity_led = !activity_led;		// Blink
		}
	}

	switch (activity_timeout)
	{
		case 0:						// Timeout is disabled, so do nothing.
			break;

		case 1:						// Ding! Blink timeout: stop blinking
			activity_led = 1;		// and turn on the activity LED.
			activity_blink_rate = 0;
			activity_ticker = 0;
			activity_timeout = 0;
			break;

		default:
			activity_timeout--;
	}

	if ((usb_active)
#ifdef SUPPORT_RAID1
        /* || (arrayMode == RAID1_MIRROR)*/
#endif
        )
	{
		if (activity_led)
		{
#ifdef DATABYTE_RAID
			if (sync_led_once)
			{
				sync_led_once = 0;
				hdr2526_led_init();
			}
#endif			
			if (cur_status[SLOT0] < SS_EMPTY)
				HDD0_LED_ON();
			if (cur_status[SLOT1] < SS_EMPTY)
				HDD1_LED_ON();
			//MSG("hdd_led_on tm0\n");
		}
		else
		{
			if (arrayMode != JBOD)
			{
				// fault_led will control ss_failed slot for raid1
				if (cur_status[SLOT0] != SS_FAILED)
					HDD0_LED_OFF();
				if (cur_status[SLOT1] != SS_FAILED)
					HDD1_LED_OFF();
			}
			else
			{
				if ((cur_status[SLOT0] < SS_EMPTY) && (sata0Obj.sobj_State >= SATA_ACTIVE))
					HDD0_LED_OFF();
				if ((cur_status[SLOT1] < SS_EMPTY) && (sata1Obj.sobj_State >= SATA_ACTIVE))
					HDD1_LED_OFF();
			}
			//MSG("hdd_led_off tm0\n");
		}
	}

#else // ini standard activity led
#ifndef PWM	
	if (fault_blink_rate)
	{
		if (fault_ticker-- == 0)
		{
			fault_ticker = fault_blink_rate -1;
			fault_led = !fault_led;		// Blink
		}
	}

	switch (fault_timeout)
	{
		case 0:						// Timeout is disabled, so do nothing.
			break;

		case 1:						// Ding! Blink timeout: stop blinking
			fault_led = 0;		// and turn off the fault LED.
			fault_blink_rate = 0;
			fault_ticker = 0;
			fault_timeout = 0;
			break;

		default:
			fault_timeout--;
	}
	if (fault_led)
	{
//		FAULT_INDICATOR_LED_ON();
		ENABLE_WDC_FAULT_PWM(); // for the LED PWM w/ 50% duty
	}
	else
	{
		FAULT_INDICATOR_LED_OFF();
		DISABLE_WDC_FAULT_PWM();
	}
	// Figure out what the activity LED should show next.
	if (activity_blink_rate)
	{
		if (activity_ticker-- == 0)
		{
			activity_ticker = activity_blink_rate -1;
			activity_led = !activity_led;		// Blink
		}
	}

	switch (activity_timeout)
	{
		case 0:						// Timeout is disabled, so do nothing.
			break;

		case 1:						// Ding! Blink timeout: stop blinking
			activity_led = 1;		// and turn on the activity LED.
			activity_blink_rate = 0;
			activity_ticker = 0;
			activity_timeout = 0;
			break;

		default:
			activity_timeout--;
	}
#ifndef DBG_FUNCTION
	if (activity_led)
	{
//		ACTIVITY_LED_ON();
		ENABLE_WDC_ACTIVITY_PWM(); // for the LED PWM w/ 50% duty
	}
	else
	{
		ACTIVITY_LED_OFF();
		DISABLE_WDC_ACTIVITY_PWM();
	}
#endif
#endif // #ifndef PWM	
#endif // #ifdef HDR2526_LED
	// Check the power button (only on products that have one!)
	//	
	if ((product_detail.options & PO_HAS_POWER_BUTTON)
#ifdef HARDWARE_RAID
		||(RAID_CONFIG_BUTTON_PRESSED)
#endif		
#ifdef HDD_CLONE
		|| (CLONE_BUTTON_PRESSED)
#endif
		)
	{
		// Make all the ISR's variables static so the linker
		// doesn't merge them with other variables.
		static u8 pb_now;

		// Read the power button's current state. The button can be on
		// the same GPIO as the activity LED, or to its own GPIO.
		// The button is reads as 0 when it is pressed.
#ifdef POWER_BUTTON		
		*gpioCtrlFuncSel0 &= ~0x00000300; // switch the GPIO4 function select to GPIO4 input function
		*gpioOutEn &= ~PWR_BUTTON;
#endif		
#ifdef HARDWARE_RAID
		*gpioCtrlFuncSel0 &= ~0x0000C000; // switch the GPIO7 function select to GPIO7 input function
		*gpioOutEn &= ~RAID_CONFIG_BUTTON;
#endif
#ifdef HDD_CLONE
		*gpioCtrlFuncSel1 &= ~0x0000000C; // switch the GPIO17 function select to GPIO17 input function
		*gpioOutEn &= ~CLONE_BUTTON;
#endif
#ifdef POWER_BUTTON
		if ((*gpioDataIn & PWR_BUTTON) == 0)
		{
			pb_now = POWER_BUTTON_PRESSED;	
		}
#endif		
#ifdef HARDWARE_RAID
		if ((*gpioDataIn & RAID_CONFIG_BUTTON) == 0)
		{
			pb_now = RAID_CONFIG_BUTTON_PRESSED;	
		}
#endif	
#ifdef HDD_CLONE
		else if ((*gpioDataIn & CLONE_BUTTON) == 0)
		{
			pb_now = CLONE_BUTTON_PRESSED;	
		}
#endif
		else
		{
			pb_now = 0;			
		}

		if (pb_now != pb_state)
		{
			// The debounce flag acts as a two-cycle debounce timer.
			if (!pb_debounce)
			{
				pb_debounce = 1;
			}
			else
			{
				// Ding! Debounce time satisified, so ackowledge
				// the button has been pressed or released.
				u8 pre_pb_state = pb_state; // to indicate previous button
				pb_state = pb_now;
				pb_timer = 0;

#ifdef POWER_BUTTON
				// If the button has just been released, then
				// acknowledge it *was* pressed and notify...
				if ((pb_state != POWER_BUTTON_PRESSED) && (pre_pb_state == POWER_BUTTON_PRESSED))
				{
					// ...but only if we didn't already take action.
					if (!pb_ignore_release)  power_button_pressed = 1;

					pb_ignore_release = 0;
				}
#endif				
#ifdef SCSI_HID
				if ((usb_active) && ((pb_state != CLONE_BUTTON_PRESSED) && (pre_pb_state == CLONE_BUTTON_PRESSED)))
				{
					backup_button_pressed = 1;
				}
#endif				
#if 0//def DATABYTE_RAID  // db.609rc01 re-request 3s, so close it.
				if ( (pb_state == RAID_CONFIG_BUTTON_PRESSED) &&
				     (pb_timer == 0 /*~1 seconds*/) )
				{
					MSG("DCo push\n");
					raid_config_button_pressed = 1;
					pb_state = 0;
				}
#endif

			}
		}
		else
		{
			pb_debounce = 0;
			pb_timer++;
           // MSG("~pb_timer = %bx pb_debounce = %bx\n",pb_timer,pb_debounce);
#ifdef POWER_BUTTON
			// Force power-off if the power button is pressed too long.
			if ( (pb_state == POWER_BUTTON_PRESSED) &&
			     (pb_timer == 40 /*~4 seconds*/) )
			{
				// Dropping off the USB bus or turning off the lights
				// at this point (while in the ISR) will be hard.

				// An immediate board-level reset would be ideal here,
				// but it's easy to reset the CPU while a board reset
				// is hard to do - at least not without a lot of code.
				// Therefore pretend a power-off command was rcvd from the PC.
				power_off_commanded = 1;
				pb_ignore_release = 1;

				// Alternate strategy: reboot the firmware and cleanup.
				// Hurdles to overcome:
				// 1. how to reboot without leaving the CPU interrupt
				//    logic hanging. Maybe call ShareAsm:IntrReset() ?
				// 2. can main() reset other hardware properly?
				// 3. how does main() know a force shutdown was done?
			}
#endif			
//#ifndef DATABYTE_RAID  // db.609rc01 re-request 3s, so open it.
#ifdef HARDWARE_RAID
			if ( (pb_state == RAID_CONFIG_BUTTON_PRESSED) &&
			     (pb_timer == 30 /*~3 seconds*/) )
			{
				MSG("Co push\n");
				raid_config_button_pressed = 1;
				pb_state = 0;
			}
			else
#endif
//#endif

#ifdef HDD_CLONE
#ifndef DATASTORE_LED
			if ((usb_active == 0) && ((pb_state == CLONE_BUTTON_PRESSED) &&
			     (pb_timer == 30 /*~3 seconds*/)))
			{
				MSG("Cl push\n");
				clone_button_pressed = 1;
				pb_state = 0;
			}
#else
            if ((usb_active == 0) && ((pb_state == CLONE_BUTTON_PRESSED) &&
                 (pb_timer == 30 /*~3 seconds*/)))
            {
                MSG("Cl push 3 seconds\n");
                wait_for_clone = 1;
            }            
            else if(( (usb_active == 0) && (wait_for_clone == 1) && 
                (pb_state == CLONE_BUTTON_PRESSED) && (pb_timer == 10 /*~1 seconds*/)) )
            {
                MSG("~~~~~\n");
                clone_button_pressed = 1;
                pb_state = 0; 
                wait_for_clone = 0;
            }
            else if( (usb_active == 0) && (wait_for_clone == 1) )//please put this on last "else if"!
            {
                MSG("~ wait_for_clone = 1; pb_timer = %x \n",pb_timer);
                blink_clone_leds();
            }
#endif
#endif			

		}
	}
	
	if ((chk_fis34_timer) && (hdd_power))
	{
		// it's a 2.5s timer
		if (--chk_fis34_timer == 0)
		{
			if (cntdwn_pwron_phyRdy)
			{
				
				cntdwn_pwron_phyRdy = 0;
				if ((sata0Obj.sobj_State < SATA_PHY_RDY) || (sata1Obj.sobj_State < SATA_PHY_RDY))
				{
					if (sata0Obj.sobj_State < SATA_PHY_RDY)
					{
						MSG("no HDD0\n");
						sata0Obj.sobj_device_state = SATA_NO_DEV;
					}
					if (sata1Obj.sobj_State < SATA_PHY_RDY)
					{
						MSG("no HDD1\n");
						sata1Obj.sobj_device_state = SATA_NO_DEV;
					}
				}
				else
				{
					chk_fis34_timer = CHECK_FIS34_TIMER;
				}

#if FAN_CTRL_EXEC_TICKS
				fan_ctrl_init();
#endif
			}
			else
			{
				if (((sata0Obj.sobj_State > SATA_PRE_READY) || (sata0Obj.sobj_device_state == SATA_NO_DEV)) 
				&& ((sata1Obj.sobj_State > SATA_PRE_READY) || (sata1Obj.sobj_device_state == SATA_NO_DEV)))
				{
					chk_fis34_timer = 0;
					spinup_timer = 0;
				}
				else
				{
					MSG("s w %bx\n", spinup_timer);
					if ((sata0Obj.sobj_State < SATA_READY) && (sata0Obj.sobj_device_state != SATA_NO_DEV))
					{
						sata_timeout_reset(&sata0Obj);
					}
					if ((sata1Obj.sobj_State < SATA_READY) && (sata1Obj.sobj_device_state != SATA_NO_DEV))
					{
						sata_timeout_reset(&sata1Obj);
					}
				}
				if (spinup_timer)
				{
					--spinup_timer;
					// when spin up is longer than 4.5secs, response unit not ready
					// 2011/12/14, change the 7secs to 4.5secs to fix the issue in Win8 UAS
					// Win 8 UAS, the first command is Read_Capacity10, the Inquiry, after that it's LOG sense
					// the commands must be finished in 5secs, otherwise the system will get BSOD
					if (spinup_timer == 0)
					{
						rsp_unit_not_ready = 1;
						chk_fis34_timer = CHECK_FIS34_TIMER;
						spinup_timer = 3;
					}
					else
					{
						chk_fis34_timer = CHECK_FIS34_TIMER;
					}
				}
			}
		}
	}

	sata_active_scm_count_down(&sata0Obj);
	sata_active_scm_count_down(&sata1Obj);
	if (rw_time_out)
	{	
		rw_time_out--;
	}	

	auxreg_w(REG_CONTROL0, CONTROL_IE|CONTROL_NH ); //CONTROL_NH Enable the timer interrupt
	auxreg_w(REG_COUNT0, 0);						// set initial value and restart the timer

	return;	
}

_Interrupt2 void isr_timer1()
{
#ifdef USB2_L1_PATCH
	if (usb2_L1_reject_timer)
	{
		if ((sata0Obj.sobj_State >= SATA_READY) && (sata1Obj.sobj_State >= SATA_READY))
		{
			if (--usb2_L1_reject_timer == 0)
			{	
				*usb_DevCtrlClr = USB2_L1_DISABLE;
			}
		}
	}
#endif
	
	if (power_on_flag)
	{
		// start to count the LTSSM TS1 duration time
		if (temp_USB3StateCtl == 0x37)
		{
			usb3_ts1_timer--;
		}
		else
			usb3_ts1_timer = 90;
		if (usb3_ts1_timer == 0)
		{
			usb3_enter_loopback_timer = 2;
_SET_LOOPBACK_PHYSETTING:
			// switch to analog loopback test mode
			switch_regs_setting(LOOPBACK_TEST_MODE);
			MSG("L\n");
		}
		if (usb3_enter_loopback_timer)
		{
			if ((temp_USB3StateCtl & 0x0F) == 0x0B)
			{
				// has entered loopback mode
				usb3_enter_loopback_timer = 0;
			}
			else
			{
				if (--usb3_enter_loopback_timer == 0)
				{
					// switch to normal mode phy setting
					switch_regs_setting(NORMAL_MODE);
					MSG("N\n");
				}
			}
		}
	}
	*usb_USB3StateSelect = 0;
      	if(*usb_USB3StateCtrl != temp_USB3StateCtl)	
      	{
      		*usb_USB3StateSelect = 0;
       	temp_USB3StateCtl = *usb_USB3StateCtrl;

		if (temp_USB3StateCtl == 0x16)
		{
			usb_inactive_count++;
		}
		else if (temp_USB3StateCtl != 0x26)
		{
			usb_inactive_count = 0;
		}
		MSG("LS %bx\n", temp_USB3StateCtl); 
		if (usb_inactive_count > 0)
		{
			*usb_DevCtrl = EXIT_SS_INACTIVE|USB_CORE_RESET;
			usb_inactive_count = 0;
		}

		if (temp_USB3StateCtl == 0x0A)
		{
			switch_regs_setting(COMPLIANCE_TEST_MODE);
			MSG("C\n");
		}
		else if ((temp_USB3StateCtl & 0x0F) == 0x0B) // loop back mode
		{
			// in the case that the loopback test state has smoothly come, F/W changes the phy setting
			if (usb3_test_mode_enable == 0)
				goto _SET_LOOPBACK_PHYSETTING;
		}
      	}

	auxreg_w(REG_CONTROL1, CONTROL_IE|CONTROL_NH); //CONTROL_NH Enable the timer interrupt
	auxreg_w(REG_COUNT1, 0);						// set initial value and restart the timer
	return;	
}

_Interrupt2 void isr_asic()
{
#if 1
	u32 irq = _lr(REG_SEMAPHORE);
	u32 ilink2 = _core_read(30);
	DBG("irq: %lx, ilink2: %lx\n", irq, ilink2);
	if(*chip_IntStaus & USB_INT)
	{
		//isr_usb();
		//return;
	}
	else if(*chip_IntStaus & SATA0_INT)
	{
		DBG("\nirq%x\n", irq);
#if 0		
		 if(sata_ch_reg->sata_PhyInt & PHYDWNI)
		{
			#ifdef DEBUG
				printf("\n\n==============phy not ready===================\n\n");
			#endif
			sata_ch_reg->sata_PhyInt = PHYDWNI;
			pSataObj->sobj_State = SATA_RESETING; // the sata status is setted to resetting status, and wait for the phy ready
#if 0
			cpu_reset();
#endif
		}	
#endif
	}
#ifdef UART_RX
	*uart0_INT = UART_RXEINT;

	if (*uart0_INT & UART_RXINT)
	{
		char getChar;
		u8 getEnd = 0;
		
		while ((*uart0_Ctrl & UART_RX_EMPTY) == 0)
		{
			getChar = *uart0_Data;
			
			if (getChar == '\r')
			{
				getEnd = 1;
				UART_MSG('\n');
			}
			else if (getChar == 0x08)
			{	// backspace
				if (uartRxPos != 0)
				{
					uartRxPos--;
				}
				uart_rx_buf[uartRxPos] = 0x00;
				UART_MSG(getChar);
				UART_MSG(' ');
				UART_MSG(getChar);
				continue;
			}			

			UART_MSG(getChar);
			uart_rx_buf[uartRxPos++] = getChar;
			
			//avoid buffer overfloww
			if (uartRxPos > 40)
				uartRxPos = 40;
		}

		if (getEnd == 1)
		{
			uart_rx_parse(uart_rx_buf);
			uartRxPos = 0;
			getEnd = 0;
		}

		*uart0_INT = UART_RXINT;
	}
#endif //UART_RX
#endif
	return;
}
// EOF
