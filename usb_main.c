/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *
 *****************************************************************************/


#include	"general.h"


void init_GPIOs_setting(void)
{
#ifdef HDR2526_LED
#ifdef UART_RX
	*gpioDataOut = (*gpioDataOut | GPIO0_PIN) & ~(HDD0_LED_PIN | FAIL_BUZZER_PIN | JBOD_LED_PIN | GPIO9_PIN | RAID1_LED_PIN); 
	*gpioOutEn |= (JBOD_LED_PIN | ERROR_LED_D1 |HDD0_LED_PIN |FAIL_BUZZER_PIN |GPIO9_PIN | RAID1_LED_PIN);
#else
	*gpioDataOut = (*gpioDataOut | GPIO0_PIN) & ~(HDD0_LED_PIN | HDD1_LED_PIN | FAIL_BUZZER_PIN | JBOD_LED_PIN | GPIO9_PIN | RAID1_LED_PIN | CLONE_LED_25 | CLONE_LED_100); 
	*gpioOutEn |= (HDD1_LED_PIN | JBOD_LED_PIN | ERROR_LED_D1|HDD0_LED_PIN|FAIL_BUZZER_PIN|GPIO9_PIN | RAID1_LED_PIN | CLONE_LED_25 | CLONE_LED_100);
#endif
#else
	// GPIO 0 is used as the LED & UART, the GPIO 8 & 9 is used for power control 
	*gpioDataOut = (*gpioDataOut | GPIO0_PIN) & ~(DRIVE0_POWER_PIN | DRIVE1_POWER_PIN); 
	*gpioOutEn |= (ERROR_LED_D0|ERROR_LED_D1|ACTIVITY_LED_PIN_D0|ACTIVITY_LED_PIN_D1|DRIVE0_POWER_PIN | DRIVE1_POWER_PIN);
#endif

	// set the PERClock
	*cpu_clock_power_down_U1 = (*cpu_clock_power_down_U1 & ~PERCLK_DIV_U1) | (CLOCK_64M << 8);
	*cpu_clock_power_down_U2 = (*cpu_clock_power_down_U2 & ~PERCLK_DIV_U2) | (CLOCK_64M << 8);
	//enable_GSPI_function();
	//set_GSPI_clock(2); // use 25M
#if FAN_CTRL_EXEC_TICKS	
	*GPIO_Counter_GPIO_Select = 0x11;	// GPIO17
#endif

	enable_PWM_function();
	u32 led_pwm_setting = (WDC_LED_TIMER_UNIT << 24) | 0x55;
	// test LED 0(GPIO11), 1(GPIO 12), 2(GPIO10), 3(GPIO14) with BLINK mode, the time unit is 1638.4us
#ifdef HDR2526_LED
#ifdef HDR2526_BUZZER	// LED1(GPIO 12)
	*led1_Ctrl0 |= (HDR2526_BUZZER_TIMER_UNIT << 24)|0x55;
	*led1_Ctrl1 |= (LED_ENABLE|LED_FOREVER|LED_PWM_MODE|LED_AUTO_MODE_BIDIR|(WDC_PWM_PERIOD << 8));
#endif
#else
	*led0_Ctrl0 |= led_pwm_setting;
	*led0_Ctrl1 |= (LED_ENABLE|LED_FOREVER|LED_PWM_MODE|LED_AUTO_MODE_BIDIR|(WDC_PWM_PERIOD << 8));
	*led1_Ctrl0 |= led_pwm_setting;
	*led1_Ctrl1 |= (LED_ENABLE|LED_FOREVER|LED_PWM_MODE|LED_AUTO_MODE_BIDIR|(WDC_PWM_PERIOD << 8));
#endif	
#if 0
	*led2_Ctrl0 |= led_pwm_setting;
	*led2_Ctrl1 |= (LED_ENABLE|LED_FOREVER|LED_PWM_MODE|LED_AUTO_MODE_BIDIR|(WDC_PWM_PERIOD << 8));
	*led3_Ctrl0 |= led_pwm_setting;
	*led3_Ctrl1 |= (LED_ENABLE|LED_FOREVER|LED_PWM_MODE|LED_AUTO_MODE_BIDIR|(WDC_PWM_PERIOD << 8));
#endif
#if FAN_CTRL_EXEC_TICKS
	// test LED 4(GPIO 3) as the blink mode with different RPM, off(2.4%)/4000/5868/7732/99.88%
	// 
	//*led4_Ctrl0 |= (WDC_FAN_TIMER_UNIT << 24) | (WDC_PWM_PERIOD << 16);
	*led4_Ctrl0 |= (WDC_FAN_TIMER_UNIT << 24) | 0xA0;
	*led4_Ctrl1 |= (LED_ENABLE|LED_FOREVER|LED_AUTO_MODE_BIDIR);

	gpio_accumlated_pulse_count = 0;
	gpio_pulse_count = 0;
	gpio_count_timetick = GPIO_COUNTER_TIMEUNIT_7DOT5_SEC;
#endif	

#ifdef DBG_PERFORMANCE
	// use GPIO to measure the latency to debug performance
	*gpioCtrlFuncSel0 = *gpioCtrlFuncSel0 & ~0x00FF0000;
	*gpioDataOut = *gpioDataOut & ~(GPIO8_PIN|GPIO9_PIN |GPIO10_PIN | GPIO11_PIN); 
	*gpioOutEn |= (GPIO8_PIN|GPIO9_PIN |GPIO10_PIN | GPIO11_PIN);
#endif

	//*usb_DevCtrlClr = USB_ENUM;		//disable USB enumeration
	*usb_IntStatus = 0xFFFFFFFF;
	*usb_DevCtrl = USB_FW_RDY;
}

void init_hardware_default(void)
{
u8 temp8;

	if ((*chip_id2 == WD_NON_AES_CHIPID) || (*chip_id2 == WD_AES_CHIPID))
	{
		is_chip_for_wd = 1;
	}
	else
	{
		is_chip_for_wd = 0;
	}
	*usb_Threshold0 = 0x44; // RX is 4, TX is 4
	*usb_Threshold1 = 0x44;
	*usb_Threshold2 = 0x44;

      ////////////
      // bit 7 allow skip
      // bit 22 allow truncate data less than 1024 byte
      // default is 1024
      // set 1 to allow 1024      
	*usb_LinkCtrl = (*usb_LinkCtrl & ~TS_DETECT_COUNT) | (ENHANCED_TS_DETECT | TX_DATA3_ABORT_DISABLE|DISABLE_SAVE_LATENCY|DET_WARMRESET_SEP|TS_DETECT_COUNT_32);
	*usb_LinkConfiguration0 = (*usb_LinkConfiguration0 & ~LFPS_POLLING_TYP) | LFPS_POLLING_TYP_1DOT34US;
	*usb_LinkConfiguration1 = (*usb_LinkConfiguration1 & ~(LFPS_PING_TYP|LFPS_TRESETDELAY)) | LFPS_TRESETDELAY_9MS |LFPS_PING_TYP_192NS;

#if 1 // for Lecroy TD 6.5 failure
	*usb_LinkConfiguration0 = (*usb_LinkConfiguration0 & ~LFPS_POLLING1_MAX) | LFPS_POLLING1_MAX_1DOT6US;
#endif
	
#ifdef LA_DUMP
//	*usb_MSC0_Proto_Retry_LA_option = 0x01; // set protocol retry LA to mode 1
	*usb_PM_LA_OPTIONS = (*usb_PM_LA_OPTIONS & ~USB3_PM_LA_HALT_SEL) | LTSSM_EXIT_U1;
	*usb_PM_LA_ADR |= LA_ENABLE;
#endif
	if (*chip_Revision < 0x03)
	{
		*switching_regulator_reg = (*switching_regulator_reg & ~0x07) | 0x07; // set the switch regulator's voltage to 1.08V
		*switching_regulator_reg_PD = (*switching_regulator_reg & ~0x07) | 0x07; // set the switching regulator's voltage of power down state at 1.08V
		*switching_regulator_reg_PU = (*switching_regulator_reg & ~0x07) | 0x07; // set the switching regulator's voltage of power up state at 1.08V
	}
	else
	{
#if 0// test
		*switching_regulator_reg = (*switching_regulator_reg & ~0x07)|0x05; // set the switch regulator's voltage to 1.14V
		// set the switching regulator's voltage of power down state at 0.84V and disable over current & short detection curcuit
		*switching_regulator_reg_PD = ((*switching_regulator_reg & ~0x07) | 0x03) | DIS_OVER_CURRENT_DETECTION | DIS_SHORT_CIRCUIT_DETECTION; 
		*switching_regulator_reg_PU = (*switching_regulator_reg & ~0x07)|0x05; // set the switching regulator's voltage of power up state at 1.14V
#else
		*switching_regulator_reg = (*switching_regulator_reg & ~0x07); // set the switch regulator's voltage to 1.02V
		// set the switching regulator's voltage of power down state at 0.84V and disable over current & short detection curcuit
		*switching_regulator_reg_PD = ((*switching_regulator_reg & ~0x07) | 0x03) | DIS_OVER_CURRENT_DETECTION | DIS_SHORT_CIRCUIT_DETECTION; 
		*switching_regulator_reg_PU = (*switching_regulator_reg & ~0x07); // set the switching regulator's voltage of power up state at 1.02V
#endif
	}
	*cpu_clock_select = ((DBUF_CLOCK & 0x0F) << 16) | ((DBUF_CLOCK & 0x0F) << 12) | ((PERIPH_CLOCK & 0x0F) << 8) | ((AES_CLOCK & 0x0F) << 4) | (BEST_CLOCK & 0x0F);

	// 3608 registers
#if !defined(FPGA) || defined(NEW_FPGA)
#if 0
	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x00);
	//PMA_0[6:5]: internal regulator voltage
	// 00 -> 0.95V, 10-> 1.1V, 01 -> 1.0V, 11 -> 1.15V
	temp8 &= ~0x60; 
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x00, temp8);	

	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x02);
	temp8 &= ~0x08; // turn on SSC
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x02, temp8);
	
	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x03);
	temp8 &= ~0x0F; // set SSC PPM equal 0
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x03, temp8);

	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x05);
	temp8 |= 0x02; // disable the CDR limit detector power down signal 
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x05, temp8);	

	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x06); 
	temp8 = (temp8 & ~0xE0) |BIT_0; // BIT0: enable interpolator giltch remove, Bit7,6: enable LFPS EIDLE qualification, bit 5: enable EIDLE detection
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x06, temp8);

#if 1//def TEK_LOOPBACK
	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x0A);
	temp8 |= 0x02; // enable the bit 1 for LFPS auto calibration
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x0A, temp8);
#else
	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x07); // LFPS detection squelch threshold -> 225/180mv
	temp8 = (temp8 & ~0x0E) | 0x08;	
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x07, temp8);
#endif

#ifndef LONG_CABLE
//	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x08, 0xFC); // AFE equalization: low pole, high gain
	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x08); 
	#define PCAP_IN		0xC0 // pma_8[7:6]: AFE equalization: low pole, less peaking
	#define RTUNE_SQ2	0x20 // AFE equalization: high gain, narrow bandwidth
	temp8 |= (PCAP_IN | RTUNE_SQ2);
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x08, temp8); 
	
	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x09);	
	temp8 = (temp8 & ~0x3F) | 0x1E|0x80; // cdr_lp_gn[5:0], CDR second order filter tracking ranger up to 15K PPM enable: bit_7
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x09, temp8);
#else
	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x08); 
	temp8 = (temp8 & ~0xF0) | 0x90; // pcap is 0x10, RTUNE 0x01
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x08, temp8); 
	
	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x09);	
	temp8 = (temp8 & ~0x3F) | 0x1D|0x80; // cdr_lp_gn[5:0], CDR second order filter tracking ranger up to 15K PPM enable: bit_7
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x09, temp8);
#endif

	// set voltage swing amplitude 
	// PMA_A[3:2]
	// 00 -> 1V, 01 -> 850MV, 10 -> 775MV, 11 -> 625MV
	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x0A);	
	temp8 = (temp8 | BIT_4) & ~(0x60 | 0x0C);
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x0A, temp8);

	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x0B);
	temp8 |= BIT_0;
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x0B, temp8); // disable synchronization to TX
	temp8 = spi_phy_rd(PHY_SPI_U3PCS, 0x28);
#ifdef HW_ADAPTIVE
	temp8 = (temp8 & ~(BIT_3|BIT_6)) | BIT_5; // BIT_3: bypass is disable, BIT_5: firmware control, BIT_6 firmware set loop start	
#else
	temp8 |= BIT_3;
#endif
	spi_phy_wr_retry(PHY_SPI_U3PCS, 0x28, temp8); // fw enable zcap_tun value
#ifndef LONG_CABLE
#ifndef HW_ADAPTIVE
	temp8 = spi_phy_rd(PHY_SPI_U3PCS, 0x29); // set zcap value to 0x0 
	temp8 &= ~0xF0;
	spi_phy_wr_retry(PHY_SPI_U3PCS, 0x29, temp8);
#endif
#else
	temp8 = spi_phy_rd(PHY_SPI_U3PCS, 0x29); // set zcap value to 0x0 
	temp8 = (temp8 & ~0xF0) | 0x70;
	spi_phy_wr_retry(PHY_SPI_U3PCS, 0x29, temp8);	
#endif
	spi_phy_wr_retry(PHY_SPI_SATA0, SATA_PHY_ANALOG_2_3, 0x04);
	MSG("S0 SSC %bx\n", spi_phy_rd(PHY_SPI_SATA0, SATA_PHY_ANALOG_2_3));
	spi_phy_wr_retry(PHY_SPI_SATA1, SATA_PHY_ANALOG_2_3, 0x04);
	MSG("S1 SSC %bx\n", spi_phy_rd(PHY_SPI_SATA1, SATA_PHY_ANALOG_2_3));
#endif
#ifdef INIC_3610
	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x00);
	//PMA_0[6:5]: internal regulator voltage
	// 00 -> 0.95V, 10-> 1.1V, 01 -> 1.0V, 11 -> 1.15V
	temp8 = (temp8 & ~0x60) | 0x40; 
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x00, temp8);	
	
	// set the SSC to upspread which means downspread in close loop mode by analog team's instruction 05/02/2013
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x02, ((spi_phy_rd(PHY_SPI_U3PMA, 0x02) & ~0x06)|SSC_UP_SPREAD_MODE)); // set the SSC central spread mode

	// Set SSC to 2000ppm
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x03, (spi_phy_rd(PHY_SPI_U3PMA, 0x03) & ~0x0F) |SSC_2000PPM |0x80);
	
	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x06); 
	temp8 = (temp8 & ~BIT_5); // bit 5: enable EIDLE detection
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x06, temp8);

	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x07);	
	temp8 = (temp8 & ~BIT_0); 
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x07, temp8);

	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x08);	
	temp8 = (temp8 & ~0xC0) | 0x40; // change the Zcap setting
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x08, temp8);
	
	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x09);	
	temp8 = (temp8 & ~0x3F) | 0x1E|0x80; // cdr_lp_gn[5:0], CDR second order filter tracking ranger up to 15K PPM enable: bit_7
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x09, temp8);

	temp8 = spi_phy_rd(PHY_SPI_U3PCS, 0x29);	
	temp8 = (temp8 & ~0xF0) | 0x40; // change the zcap value, it has bigger margin for MCCI exciser 
	spi_phy_wr_retry(PHY_SPI_U3PCS, 0x29, temp8);
	
	// enable AFE offset voltage calibration
	temp8 = spi_phy_rd(PHY_SPI_SATA0, 0x00);
	temp8 |= BIT_2;	
	spi_phy_wr_retry(PHY_SPI_SATA0, 0x00, temp8);
	spi_phy_wr_retry(PHY_SPI_SATA1, 0x00, temp8);	

	// bit 3: Enable squelch detector offset voltage calibration
	// bit 6: Enable AFE low power mode
	temp8 = spi_phy_rd(PHY_SPI_SATA0, 0x01);
	temp8 |= (BIT_6 | BIT_3);
	spi_phy_wr_retry(PHY_SPI_SATA0, 0x01, temp8);
	spi_phy_wr_retry(PHY_SPI_SATA1, 0x01, temp8);		


	// change the AFE setting for equalizer
	temp8 = 0x0D; // change from 0x83 to 0x0D following analog's instruction
	spi_phy_wr_retry(PHY_SPI_SATA0, 0x02, temp8);
	spi_phy_wr_retry(PHY_SPI_SATA1, 0x02, temp8);

	// set CDR filter loop integral path gain
	temp8 = spi_phy_rd(PHY_SPI_SATA0, 0x03);
	temp8 = (temp8 & ~0x0F) | 0x08;
	spi_phy_wr_retry(PHY_SPI_SATA0, 0x03, temp8);
	spi_phy_wr_retry(PHY_SPI_SATA1, 0x03, temp8);
	
	temp8 = spi_phy_rd(PHY_SPI_SATA0, 0x0C);
	temp8 = (temp8 & ~0xF0); // set SSC to 0
	spi_phy_wr_retry(PHY_SPI_SATA0, 0x0C, temp8);
	spi_phy_wr_retry(PHY_SPI_SATA1, 0x0C, temp8);	


	temp8 = spi_phy_rd(PHY_SPI_SATA0, 0x05);
	temp8 &= ~0x03;
	spi_phy_wr_retry(PHY_SPI_SATA0, 0x05, temp8);
	spi_phy_wr_retry(PHY_SPI_SATA1, 0x05, temp8);	
	
	temp8 = spi_phy_rd(PHY_SPI_SATA0, 0x21);
	// reset SATA CDR when Idle
	temp8 &= ~0x80;
	spi_phy_wr_retry(PHY_SPI_SATA0, 0x21, temp8);
	spi_phy_wr_retry(PHY_SPI_SATA1, 0x21, temp8);
#endif

#else
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x00, 0xBE);
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x01, 0xE4); //B11
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x02, 0x95); //B11	
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x03, 0x45);	
        /////////////////////
       // rx SSC turn on
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x04, 0x2c);	
//  high swing 0.88 v
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x05, 0x17); //B11
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x0C, 0x8A); //B11	

	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x06, 0x98); //B11//?
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x07, 0x43); 
	// set Tx SSC of USB3 PMA 
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x08, 0x82);
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x09, 0x88);  //B11		
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x0A, 0x23);	//B11
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x0B, 0x00);	//B11

#endif
	
       //////////////////////////////	
	// change the USB2 PHY reg 0x00 & 0x09 for USB2 Eye pattern
	temp8 = spi_phy_rd(PHY_SPI_U2, 0x00);	
	if (*chip_Revision >= 0x03)
	{
		temp8 = (temp8 & ~0x03) | 0x02;
	}
	else
	{
		temp8 |= 0x03;
	}
	spi_phy_wr_retry(PHY_SPI_U2, 0x00, temp8);

	temp8 = spi_phy_rd(PHY_SPI_U2, 0x05);	
	temp8 |= 0x08; 
	spi_phy_wr_retry(PHY_SPI_U2, 0x05, temp8);

	temp8 = spi_phy_rd(PHY_SPI_U2, 0x09);	
	temp8 &= ~0x01; 
	spi_phy_wr_retry(PHY_SPI_U2, 0x09, temp8);

	
}

// turn on HDD's power & Power LED
void reset_drives(void)
{
	// turn on LED
	//set_power_light(1);
	// Turn on power to the HDD and whatever else needed.
	set_hdd_power(0x0F, 0);
	_disable();
#ifdef FPGA
	chk_fis34_timer = 50; // 5sec
#else
	chk_fis34_timer = 40; // 2sec
#endif
	spinup_timer = 3; //wait for 7.5secs for the Fis34 after power on
	cntdwn_pwron_phyRdy = 1; // when 2 secs is timeout, assume that there's no device attached	
	_enable();

	set_activity_for_standby(0);
}

void usb_main(void)
{
//	u8 i;
	*chip_IntEn = 0;
	*chip_IntStaus = 0xFFFFFFFF; // disable chipInt
	auxreg_w(REG_CONTROL0, 0x0); // disable time interrupt
	
	
	DBG("\n usb_main\n");
	MSG("chip_Revision:%BX\n", *chip_Revision);
	MSG("fw_tmp %lx\n", *fw_tmp);
	MSG("restart %bx\n", *usb_RestartCode);

	reset_clock(BEST_CLOCK);
	init_hardware_default();
	init_GPIOs_setting();
	
#ifdef ERR_RB_DBG
	err_rb_dbg = 0;
#endif
	FW_state = AP_POWER_ON_RESET;
#ifdef POWER_BUTTON
	power_button_pressed = 0;
	power_off_commanded = 0;
#endif	
#ifdef HARDWARE_RAID
	raid_config_button_pressed = 0;
#endif
#ifdef HDD_CLONE
	clone_state = 0;
	clone_button_pressed = 0;
#endif
#ifdef SCSI_HID
	backup_button_pressed = 0;
#endif
#ifdef SUPPORT_HR_HOT_PLUG
	hot_plug_enum = 0;
#endif
	u8 raidEnumPerformed = 0;
	u8 rebuild_reset_cnt = 0;
	standby_timer_expired = 0;
	re_enum_flag = 0;
	chk_fis34_timer = 0;
	spinup_timer = 0;
	rsp_unit_not_ready = 0;
	raid_rebuild_verify_status = 0;
	raid_mirror_status = 0;
	raid_rb_state = RAID_RB_IDLE;
	write_fail_dev = 0;
	usb2_L1_reject_timer = 0;
	U1U2_ctrl_timer = 0;
	usb_active = 0;
	//Please note that WD currently does not want to support the auto-rebuild feature, 
	//but we may want it in the future.  
	//The enable_auto_rebuild flag shall be set to the constant value of false
#ifdef AUTO_REBUILD
	enable_auto_rebuild = 1;
#else
	enable_auto_rebuild = 0;
#endif
#ifdef WRITE_PROTECT
	if ((*gpioDD & GPIO4_PIN) == 0x00)
		write_protect_enabled = 1;
	else
		write_protect_enabled = 0;
#endif
#ifdef RMB_SWITCH  // for test
	if ((*gpioDD & GPIO0_PIN) == 0x00)
		rmb_enabled = 1;
	else
		rmb_enabled = 0;
#endif

#if defined(INTERNAL_TEST) || defined(INITIO_STANDARD_FW)
	product_model = INITIO_3610;
#else
	// check the HW stapping when power up
	hw_strapping_setting();
#endif	
	identify_product();
	
	spi_read_global_nvram();
	pwr_on_sata_init();

	arrayMode = NOT_CONFIGURED;
	array_status_queue[0] = AS_NOT_CONFIGURED;
	array_status_queue[1] = AS_NOT_CONFIGURED;

	dbuf_init();
	
#ifdef INTERRUPT	
	init_cpu_interrupt();
	timer_init();
	*GPIO_Counter_count; // read the gpio count to clear it
	init_timer0_interrupt(100, BEST_CLOCK);
	init_timer1_interrupt(100, BEST_CLOCK);
#endif

	spi_detect_flash_id();
#ifdef INITIO_STANDARD_FW
	firmware_version = *((u16 *)0x00000329);  // LOCAL_NVR(0x500) - WD_FW_HEADER(0x200) + FW_VER(0x29)
	firmware_version = ChgEndian_16(firmware_version);
	firmware_version_str[0] = ((firmware_version >> 12) & 0x0f) + '0';
	firmware_version_str[1] = ((firmware_version >> 8) & 0x0f) + '0';
	firmware_version_str[2] = ((firmware_version >> 4) & 0x0f) + '0';
	firmware_version_str[3] = (firmware_version & 0x0f) + '0';
#else
	get_fw_version(firmware_version_str, 0);
	fwVerStr_To_fwVerHex();
#endif	
	MSG("v %x\n", firmware_version);

	disable_upkeep = 0;
	ignore_tickles = 0;			// Allow HDD activity tickles.
	security_state[0] = SS_NOT_CONFIGURED;
	security_state[1] = SS_NOT_CONFIGURED;
	need_to_sync_vital = 0xFF;
	current_aes_key2 = AES_NONE;
	current_aes_key = AES_NONE;
	download_microcode_ctrl = 0;

	init_reset_default_nvm_unit_parms();
	gpio_init();	
	lights_out();
	update_standby_timer();
	ncq_supported = 1;

	productInfo_freeze = 0;
#ifdef WDC_RAID
	if(read_nvm_prod_info() == 0)
	{
		product_detail.USB_VID = (prodInfo.new_VID[0] << 8) + prodInfo.new_VID[1];
		product_detail.USB_PID = (prodInfo.new_PID[0] << 8) + prodInfo.new_PID[1];
		MSG("vid %x, pid %x\n", product_detail.USB_VID, product_detail.USB_PID);
		if ((product_detail.USB_VID == 0) && (product_detail.USB_PID == 0))
		{
			xmemset(mc_buffer, 0x55, sizeof(PROD_INFO));
			spi_write_flash(mc_buffer, WDC_PRODUCT_INFO_ADDR, sizeof(PROD_INFO));
			identify_product();
		}
		else
		{
			product_model = check_product_config(product_detail.USB_VID, product_detail.USB_PID);
			xmemcpy(prodInfo.iSerial, unit_serial_num_str, 20);
			if (check_compability_product(product_model))
			{	// check the VID PID matches the board strap ID
			   	// This guards against mistakes in the WD factory using mass programming of flash chips.
				product_model = ILLEGAL_BOOT_UP_PRODUCT;
			}
			// if the VID & PID is not in supported list, the product is set to ILLEGAL_BOOT_UP_PRODUCT
			// but it shall not happen because the VID & PID has been verified before save to flash
			identify_product();
			wwn_action = prodInfo.wwn_actions;	
			productInfo_freeze = prodInfo.factory_freezed;
			
			MSG("wa %x\n", wwn_action);
		}
	}
#else	
	if (load_from_nvram)
	{
		xmemcpy(globalNvram.iSerial, unit_serial_num_str, 20);
		u8 default_wwn[8];
		default_wwn[0] = (0x50 | ((globalNvram.CID[0] >> 4) & 0x0F));
		default_wwn[1] = (((globalNvram.CID[0] & 0x0F) << 4) | ((globalNvram.CID[1] >> 4) & 0x0F));
		default_wwn[2] = (((globalNvram.CID[1] & 0x0F) << 4) | ((globalNvram.CID[2] >> 4) & 0x0F));
		default_wwn[3] = (((globalNvram.CID[2] & 0x0F) << 4) & 0xF0);
		xmemcpy(globalNvram.vendor_specific_id, default_wwn+4, 4);
			
		xmemcpy((u8 *)default_wwn, prodInfo.disk_wwn, 8);
		xmemcpy((u8 *)default_wwn, prodInfo.disk1_wwn, 8);
		prodInfo.disk1_wwn[3] |= 1;
		xmemcpy((u8 *)default_wwn, prodInfo.vcd_wwn, 8);
		prodInfo.vcd_wwn[3] |= 2;
		xmemcpy((u8 *)default_wwn, prodInfo.ses_wwn, 8);
		prodInfo.ses_wwn[3] |= 3;
	}
#endif
	else
	{
		// create the random ID for WWN
		// IEEE OUI 00 10 10
		const u8 default_wwn[] = {0x50, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01};
		xmemcpy((u8 *)default_wwn, prodInfo.disk_wwn, 8);
		xmemcpy((u8 *)default_wwn, prodInfo.disk1_wwn, 8);
		prodInfo.disk1_wwn[3] |= 1;
		xmemcpy((u8 *)default_wwn, prodInfo.vcd_wwn, 8);
		prodInfo.vcd_wwn[3] |= 2;
		xmemcpy((u8 *)default_wwn, prodInfo.ses_wwn, 8);
		prodInfo.ses_wwn[3] |= 3;
		unit_serial_num_str[0] = 'R';
		unit_serial_num_str[1] = 'A';
		unit_serial_num_str[2] = 'N';
		unit_serial_num_str[3] = 'D';
		unit_serial_num_str[4] = 'O';
		unit_serial_num_str[5] = 'M';
		unit_serial_num_str[6] = '_';
		unit_serial_num_str[7] = '_';
		for(u8 i=0; i<12; i++)
		{
			// 12 randomly generated digits from the characters "0123456789ABCDEF" 
			unit_serial_num_str[8 + i] = Hex2Ascii((u8)generate_RandNum_DW() & 0x0F);
		}		
	}

	read_nvm_unit_parms();
	read_nvm_quick_enum();

	globalNvram.fwVersion[0] = (u8)(firmware_version >> 8);
	globalNvram.fwVersion[1] = (u8)firmware_version;	
#ifdef INITIO_STANDARD_FW	
	globalNvram.fwRevisionText[0] = firmware_version_str[1];
	globalNvram.fwRevisionText[1] = firmware_version_str[2];
	globalNvram.fwRevisionText[2] = firmware_version_str[3];
#else
	globalNvram.fwRevisionText[0] = ((WDC_FIRMWARE_VERSION >> 12) & 0x0f) + '0';
	globalNvram.fwRevisionText[1] = ((WDC_FIRMWARE_VERSION >> 4) & 0x0f) + '0';
	globalNvram.fwRevisionText[2] = ( WDC_FIRMWARE_VERSION  & 0x0f) + '0';
#endif

	if (!load_from_nvram)
	{
		globalNvram.USB_VID[0] = (u8)(product_detail.USB_VID >> 8);
		globalNvram.USB_VID[1] = (u8)product_detail.USB_VID;
		globalNvram.USB_PID[0] = (u8)(product_detail.USB_PID >> 8);
		globalNvram.USB_PID[1] = (u8)product_detail.USB_PID;
		xmemset(globalNvram.vendorText, 0x20, 16);
		xmemcpy((u8 *)product_detail.vendor_name, globalNvram.vendorText, codestrlen((u8 *)product_detail.vendor_name));
		globalNvram.vendorStrLength = (u8)codestrlen((u8 *)product_detail.vendor_name);
		xmemset(globalNvram.modelText, 0x20, 32);
		xmemcpy((u8 *)product_detail.product_name, globalNvram.modelText, codestrlen((u8 *)product_detail.product_name));
		globalNvram.modelStrLength = (u8)codestrlen((u8 *)product_detail.product_name);
		xmemcpy(prodInfo.disk_wwn, globalNvram.CID, 8);
		globalNvram.fwCtrl[0] = 0x03;
		//globalNvram.idleTime = WDC_DEFAULT_STANDBY_TIME;
	}	

detect_mode:
	DBG("scra reg%lx\n", *cpu_SCRATCH2_reg);
#ifdef DATABYTE_RAID
#ifdef WRITE_PROTECT
	if ((*gpioDataIn & GPIO4_PIN) == 0x00)
		write_protect_enabled = 1;
	else
		write_protect_enabled = 0;
#endif
	cfa_active = 0;
#endif

	rw_time_out = 200;
	if (raidEnumPerformed == 0)
	{
		reset_drives();
		while (1)
		{
			if (*chip_IntStaus & SATA0_INT)
			{	
				sata_isr(&sata0Obj);
			}

			if (*chip_IntStaus & SATA1_INT)
			{	
				sata_isr(&sata1Obj);
			}

			if (((sata0Obj.sobj_device_state == SATA_NO_DEV) ||(sata0Obj.sobj_device_state == SATA_DEV_READY))
			&& ((sata1Obj.sobj_device_state == SATA_NO_DEV) ||(sata1Obj.sobj_device_state == SATA_DEV_READY)))
			{
				_disable();
				cntdwn_pwron_phyRdy = 0; 
				rw_time_out = 0;
				_enable();
				break;
			}
			
			if (rw_time_out == 0)
			{
				rw_time_out = 200;
				if (sata0Obj.sobj_device_state == SATA_DEV_UNKNOWN)
				{
					sata_timeout_reset(&sata0Obj);
				}
				if (sata1Obj.sobj_device_state == SATA_DEV_UNKNOWN)
				{
					sata_timeout_reset(&sata1Obj);
				}
			}
		}
_raid_config:		
		if ((sata0Obj.sobj_device_state == SATA_NO_DEV) && (sata1Obj.sobj_device_state == SATA_NO_DEV))
		{
			fault_leds_active(1);
#if 0//def DATABYTE_RAID
			cfa_active = 1;
			MSG("CFA\n");
			//sata0Obj.sobj_init = 1; // don't care SATA
			//sata1Obj.sobj_init = 1; // don't care SATA
			//sata0Obj.sobj_device_state = SATA_NO_DEV; // don't care SATA
			//sata1Obj.sobj_device_state = SATA_NO_DEV; // don't care SATA
			sata0Obj.sobj_State = SATA_READY; // don't care SATA
			sata1Obj.sobj_State = SATA_READY; // don't care SATA
#else
			goto detect_mode;
#endif
		}

#ifdef SUPPORT_HR_HOT_PLUG
		pre_sata_active = 0;
		//sata_unrdy_cnt = 0;
		if (sata0Obj.sobj_device_state == SATA_DEV_READY)
			pre_sata_active |= 0x01;
		if (sata1Obj.sobj_device_state == SATA_DEV_READY)
			pre_sata_active |= 0x02;
		if (pre_sata_active == 0x03)
		{
#ifdef HDR2526_BUZZER
			DISABLE_FAIL_BUZZER_PWM();
			DISABLE_FAIL_BUZZER_FUNC();
#endif
			FAULT_BUZZER_OFF();
			fault_leds_active(0);
		}
#endif
		//MSG("phyStat %bx %bx\n", (u8)sata0Obj.pSata_Ch_Reg->sata_PhyStat, (u8)sata1Obj.pSata_Ch_Reg->sata_PhyStat);
		sata0Obj.sobj_init = 1;
#ifdef RAID
		sata1Obj.sobj_init = 1;
#endif
#ifdef DATABYTE_RAID
		if (cfa_active)
		{
#ifdef HDR2526_GPIO_HIGH_PRIORITY		
			// no dev, raid mode same as GPIO
			raid_mode = hard_raid_mapping();
			cur_status[SLOT0] = SS_EMPTY;
			cur_status[SLOT1] = SS_EMPTY;
			hdr2526_led_init();
#endif
		}
		else
#endif
		ini_init();
		raidEnumPerformed = 1;
	}

START_UP:
	*usb_DevCtrl = USB_CORE_RESET;
	rst_all_seg();

	quick_enum = 0;
	wwn_action = 0;
	diag_drive_selected = 1;
	
	if ((*cpu_SCRATCH0_reg) == 0)
	{
		USB_AdapterID = 0xff;
		USB_PortID0 = 0xff;
		USB_PortID1 = 0xff;
		USB_PortID2 = 0xff;
	}
	else
	{
		USB_AdapterID = (u8)(*cpu_SCRATCH0_reg >> 24);
		USB_PortID2 = (u8)(*cpu_SCRATCH0_reg >> 16);
		USB_PortID1 = (u8)(*cpu_SCRATCH0_reg >> 8);
		USB_PortID0 = (u8)(*cpu_SCRATCH0_reg);					
	}

	MSG("chk vbus: %bx\n", FW_state);
		
	while(1) // polling USB VBUS
	{
		switch (FW_state)
		{
			case AP_POWER_ON_RESET:
				FW_state = AP_POR_WAIT_FOR_USER;
			case AP_POR_WAIT_FOR_USER:
				if (USB_VBUS())
				{
					// turn on SATA PHY, etc
					FW_state = AP_USB_ACTIVE;
					break;
				}
				else
				{
					MSG("arrayM %bx, %bx\n", arrayMode, raid_mirror_status);
					if ((arrayMode == RAID1_MIRROR) && (raid_mirror_status))
					{
						FW_state = AP_RAID_OFFLINE_OPERATION;	
						break;
					}
#if 0
					else if (arrayMode == JBOD)
					{
						FW_state = AP_NO_HOST_OFF_WAIT;
						break;
					}
#endif
					else
						FW_state = AP_NO_HOST_OFF;
				}
			case AP_NO_HOST_OFF:
				ata_shutdown_hdd(TURN_POWER_OFF, SATA_TWO_CHs);
				lights_out();
#if FAN_CTRL_EXEC_TICKS
				enableFan(0);
#endif
				FW_state = AP_NO_HOST_OFF_WAIT;
				break;
			case AP_NO_HOST_OFF_WAIT:
#ifdef SUPPORT_HR_HOT_PLUG1
				if (chk_sata_active_status() != pre_sata_active)
				{
					MSG("devChanged\n");
					raidEnumPerformed = 0;
					FW_state = AP_POWER_ON_RESET;
					goto detect_mode;
				}
#endif				

				if (USB_VBUS())
				{
					FW_state = AP_USB_ACTIVE;
				}
				else if ((power_button_pressed) // in the host_off_wait state, F/W will wait for the
#ifdef HARDWARE_RAID
				|| (raid_config_button_pressed)
#endif
#ifdef HDD_CLONE
				|| (clone_button_pressed)
#endif
				)
				{
					if (hdd_power == 0)
					{ // if HDDs are power down, power up drives
						reset_drives();
					}
					
					if (*chip_IntStaus & SATA0_INT)
					{	
						sata_isr(&sata0Obj);
					}

					if (*chip_IntStaus & SATA1_INT)
					{	
						sata_isr(&sata1Obj);
					}
#ifdef POWER_BUTTON
					if (power_button_pressed)
					{
						if (((sata0Obj.sobj_device_state == SATA_NO_DEV) ||(sata0Obj.sobj_device_state == SATA_DEV_READY))
						&& ((sata1Obj.sobj_device_state == SATA_NO_DEV) ||(sata1Obj.sobj_device_state == SATA_DEV_READY)))
						{
							init_update_params();
							reset_standby_timer();
#if FAN_CTRL_EXEC_TICKS
							//fan_ctrl_init(sata1Obj.sobj_medium_rotation_rate, sata1Obj.sobj_medium_rotation_rate, sata0Obj.sobj_device_state, sata1Obj.sobj_device_state);
							fan_ctrl_init();
							enableFan(1);
							if (fan_ctrl_exec_flag)
							{
								fan_ctrl_exec_flag = 0;
								fan_ctrl_exec_ticker = FAN_CTRL_EXEC_TICKS;
								fan_ctrl_exec();
							}
#endif
						}

						power_off_commanded = 0;
						power_button_pressed = 0;
						FW_state = AP_NO_HOST_STICKY_ON_WAIT;
						MSG("disable_upkeep %bx\n", disable_upkeep);
					}
#endif  // POWER_BUTTON
					
#ifdef DATABYTE_RAID
					// for databyte , the button is reset button, so push it you must re-init sth.
					else if (raid_config_button_pressed)
					{
						raid_config_button_pressed = 0;
						if (!hw_raid_enum_check())
						{
							hw_raid_config(1);
							raidEnumPerformed = 0;
							//goto _raid_config;
						}
						// WP may change, clone fail need be clear
						clone_state = 0;
						raid_mirror_status = 0;
						raid_mirror_operation = 0;
						raid_rebuild_verify_status = 0;
						raid_rb_state = 0;
						
						goto detect_mode;
					}
#endif
#ifdef HDD_CLONE
					else if ((clone_button_pressed))
					{
						//MSG("%bx %bx %bx %bx %lx %lx\n", sata0Obj.sobj_device_state, sata1Obj.sobj_device_state, arrayMode, clone_state, (u32)sata0Obj.sectorLBA, (u32)sata1Obj.sectorLBA);
						if ((sata0Obj.sobj_device_state == SATA_DEV_READY) && (sata1Obj.sobj_device_state == SATA_DEV_READY))
						{
							if ((arrayMode == JBOD) 
								/*&& (sata1Obj.sectorLBA >= sata0Obj.sectorLBA) */
								&& (clone_state != CS_CLONING))
							{
					#ifdef DBG_CAP
								if (1)
					#else
								if (sata1Obj.sectorLBA >= sata0Obj.sectorLBA)
					#endif				
								{
									fault_leds_active(0);
								
									clone_init();
									MSG("cl_ini\n");
									dbg_once_flag = 1;
				
									FW_state = AP_RAID_OFFLINE_OPERATION;
								}
								else
								{
									fault_leds_active(1);
									clone_state = CS_CLONE_FAIL;
									MSG("cl fail\n");
								}
							}
							clone_button_pressed = 0;
						}
					}
#endif
					
				}
				break;
			case AP_NO_HOST_STICKY_ON_WAIT:
				if (USB_VBUS())
				{
					FW_state = AP_USB_ACTIVE;
				}
				else if (power_button_pressed ||
	         			power_off_commanded /*4s force off - see timer ISR*/)
				{
					power_off_commanded = 0;
					power_button_pressed = 0;
					FW_state = AP_NO_HOST_OFF;
				}
				// Turn off the HDD when the standby timer expires.
				ata_hdd_standby();
				break;
			case AP_USB_OFF_WAIT:
				if (USB_VBUS() == 0)
				{
					FW_state = AP_NO_HOST_OFF;
				}
				else if (power_button_pressed)
				{
					power_button_pressed = 0;
					FW_state = AP_USB_ACTIVE;
				}
				break;				

			case AP_RAID_OFFLINE_OPERATION:
#ifdef HDD_CLONE
				if ((USB_VBUS()) && (clone_state == 0))
#else
				if (USB_VBUS())
#endif					
				{
#ifdef DBG_FUNCTION
					if (dbg_once_flag == 1)				
						MSG("usbActive\n");
#endif					
					// VBUS ON to terminate the offline rebuild
					if (raid_rb_state == RAID_RB_IDLE)
					{
						MSG("to online: %bx %lx %bx %bx\n", hdd_power, *chip_IntStaus, sata0Obj.sobj_device_state, sata1Obj.sobj_device_state);
						FW_state = AP_USB_ACTIVE;
						break;
					}
				}
				if (raid_mirror_status)
				{
#ifdef DBG_FUNCTION
					if (dbg_once_flag == 1)
					{
						MSG("rbrw: %bx %lx %bx %bx\n", hdd_power, *chip_IntStaus, sata0Obj.sobj_device_state, sata1Obj.sobj_device_state);
						dbg_once_flag = 2;
					}
#endif					
					raid_rebuild_verify_status &= ~RRVS_ONLINE;
					raid1_active_timer = 0;
					// ignore button dura clone or rb offline
					raid_config_button_pressed = 0;
					clone_button_pressed = 0;
					
					if (hdd_power == 0) 
					{// if the drives are power down, power up the drives
						reset_drives();
					}
					
					if (*chip_IntStaus & SATA0_INT)
					{	
						sata_isr(&sata0Obj);
					}

					if (*chip_IntStaus & SATA1_INT)
					{	
						sata_isr(&sata1Obj);
					}
											
					if ((sata0Obj.sobj_device_state == SATA_DEV_READY) && (sata1Obj.sobj_device_state == SATA_DEV_READY))
					{
#ifdef HDD_CLONE	
						rebuild_reset_cnt = 0;
						if (clone_state)
							clone_state = CS_CLONING;
#endif
#ifdef HDR2526_LED
						else
						{
							reset_standby_timer();
						}
#endif						
						raid_rebuild();
					}
					else if ((sata0Obj.sobj_device_state == SATA_NO_DEV) || (sata1Obj.sobj_device_state == SATA_NO_DEV))
					{	// some disk is lost
						MSG("nodev: %bx %bx %bx\n", hdd_power, sata0Obj.sobj_device_state, sata1Obj.sobj_device_state);
#ifdef HDD_CLONE						
						if (clone_state == CS_CLONING)
							clone_state = CS_CLONE_FAIL;
#endif						
					}
					else  // some disk is unknown
					{
#ifdef DBG_FUNCTION
						if (dbg_once_flag == 2)
						{
							MSG("sd_unknown: %bx %lx %bx %bx\n", hdd_power, *chip_IntStaus, sata0Obj.sobj_device_state, sata1Obj.sobj_device_state);
							dbg_once_flag = 3;
						}
#endif					
#ifdef HDD_CLONE						
						if (clone_state == CS_CLONING)
						{
							
							clone_state = CS_CLONE_FAIL;
						}
#endif						

						//fault_leds_active(1);
						if (rw_time_out == 0)
						{
							// rm time out, reset again
							rw_time_out = 51;	// 5sec
							MSG("tout: %bx %lx %bx %bx\n", hdd_power, *chip_IntStaus, sata0Obj.sobj_device_state, sata1Obj.sobj_device_state);
							if ((/*(sata0Obj.sobj_device_state == SATA_DEV_READY) &&*/ (sata1Obj.sobj_device_state == SATA_DEV_UNKNOWN))
								|| ((sata0Obj.sobj_device_state == SATA_DEV_UNKNOWN) /*|| (sata1Obj.sobj_device_state == SATA_DEV_READY)*/))
							{
								rebuild_reset_cnt++;
								if (rebuild_reset_cnt == 5)
								{
									rebuild_reset_cnt = 0;
									raidEnumPerformed = 0;
									raid_mirror_status = 0;
									raid_mirror_operation = 0;
									
									FW_state = AP_POWER_ON_RESET;
									goto detect_mode;
								}
							}

							if (sata0Obj.sobj_device_state != SATA_NO_DEV)
							{
								sata_timeout_reset(&sata0Obj);
							}
							if (sata1Obj.sobj_device_state != SATA_NO_DEV)
							{
								sata_timeout_reset(&sata1Obj);
							}
						}
					}
				}
#ifdef HDR2526_LED
				else if (clone_state == CS_CLONE_DONE_HOLD) 
				{
#ifdef DBG_FUNCTION
					if (dbg_once_flag == 1)				
						MSG("clhold\n");
#endif					
#if 0  //new request, db.606, disable 20 min hold on LED ON state
					// HDR2526 requests hold on the state til 40 min then turn off the drive.
					// (40 /*minutes*/ * 60 /*seconds*/ *10)
			#ifdef DBG_FUNCTION
					clone_leds_idle_timeout(600);		// 1 min
			#else
					clone_leds_idle_timeout(12000);
			#endif
#else
					tickle_rebuild();
#endif

					if ((sata0Obj.sobj_State >= SATA_ACTIVE) // when SATA is busy, led blinks
						|| (sata1Obj.sobj_State >= SATA_ACTIVE))
						reset_standby_timer();
					if (standby_timer_expired)
					{
						// new request, db.606: just to turn off clone led here
						fault_leds_active(0);

                    #ifdef DB_CLONE_LED
                        CLONE_LED_25_OFF();      
			            RAID0_LED_OFF();
			            RAID1_LED_OFF();
			            CLONE_LED_100_OFF();
                    #else
						HDD0_LED_OFF();
						HDD1_LED_OFF();
						JBOD_LED_OFF();
						RAID0_LED_OFF();
						RAID1_LED_OFF();
                    #endif
						standby_timer_expired = 0;
						FW_state = AP_NO_HOST_OFF;
						clone_button_pressed = 0;
						clone_state = 0;
					}

				}
#endif				
				else
				{
					FW_state = AP_NO_HOST_OFF; // when rebuild or verify is finished, turn off the drive, and wait for the USB active
//dbg purpose FW_state = AP_NO_HOST_OFF_WAIT;
				}
				break;

			default:				// invalid state!
				FW_state = AP_POWER_ON_RESET;
				break;					
		}

#ifdef SUPPORT_HR_HOT_PLUG
		if (chk_sata_active_status() != pre_sata_active)
		{
			MSG("devCh FW_s\n");
			// turn off all LED
			hdr2526_led_off(ALL_IS_OFF);

			clone_state = 0;
			raid_mirror_status = 0;
			raid_mirror_operation = 0;
			raid_rebuild_verify_status = 0;
			raid_rb_state = 0;
		
			raidEnumPerformed = 0;
			FW_state = AP_POWER_ON_RESET;
			goto detect_mode;
			//goto begin;
		}
#endif				

		if (FW_state == AP_USB_ACTIVE)
		{
#ifdef HDD_CLONE
			clone_state = 0;
#endif					
			raid_rebuild_verify_status = 0;  // it may cause 1st read fail, so we need clear it
			raid_rb_state = 0;
			*usb_CoreCtrl |= VBUS_ENUM_DISABLE;
			*chip_IntStaus = CHIP_VBUS_ON;
			break;
		}
	}
	power_button_pressed = 0;
	power_off_commanded = 0;	
	unit_wide_cur_temperature = 0;
	if (hdd_power == 0)
	{
		reset_drives();
	}
	else
	{
		usb_active = 1;
	}
#ifdef HDR2526_LED
	hdr2526_led_init();  // maybe there is VBUS just now.
#endif

next:	
	unsupport_boot_up = 0;

	if (product_model == ILLEGAL_BOOT_UP_PRODUCT)
	{
		unsupport_boot_up = UNSUPPORT_HW_STRAPPING_BOOT;
		sata0Obj.sobj_init = 1; // don't care SATA
		sata1Obj.sobj_init = 1; // don't care SATA
		sata0Obj.sobj_device_state = SATA_NO_DEV; // don't care SATA
		sata1Obj.sobj_device_state = SATA_NO_DEV; // don't care SATA
		sata0Obj.sobj_State = SATA_READY; // don't care SATA
		sata1Obj.sobj_State = SATA_READY; // don't care SATA
		product_model = ILLEGAL_BOOT_UP_PRODUCT;
	}
	
BOOT_UP:
	assign_luns();
	usb_hdd();
	*chip_IntStaus = CHIP_VBUS_OFF;

	usbMode = CONNECT_UNKNOWN;
	usb_active = 0;
	chk_fis34_timer = 0; // stop the hw reset timer

#ifdef HDR2526_LED
	lights_out();	// 
	hdr2526_led_init();  // adjust now.
#endif
	
	if (USB_VBUS() == 0)
	{
		FW_state = AP_POR_WAIT_FOR_USER;
		MSG("no VBUS\n");
	}		
	else if (power_off_commanded)
	{
		power_off_commanded = 0;
		FW_state = AP_USB_OFF_WAIT;
		MSG("pwr off\n");
	}
	else if (re_enum_flag == FORCE_OUT_RE_ENUM)
	{
		DBG("force enum\n");
		*usb_DevCtrlClr = USB_ENUM;
		Delay(1000);
		sata0Obj.sobj_init = 0;
		sata1Obj.sobj_init = 0;
		usb_active = 0;
		quick_enum = 0;
		wwn_action = 0;
		goto next;
	}

	*usb_DevCtrlClr = USB_ENUM;  
	*usb_CoreCtrl &= ~VBUS_ENUM_DISABLE;
	_disable();
	spi_phy_wr_retry(PHY_SPI_U3PCS, 0x1C, 0x00); // turn off USB3 adaptive
	_enable();

#ifdef WDC_DOWNLOAD_MICROCODE
	if (download_microcode_ctrl & ACTIVATE_DEFERED_MICROCODE)
	{
		// check the SATA & USB's phy is alive
		*usb_DevCtrl = USB2_PLL_FREERUN;	// enable U2 PLL Freerun
		//	Firmware Enable USB 3.0 PLL 	
		spi_phy_wr(PHY_SPI_U3PCS, 0x13, 0);
		Delay(1);
		
		if ((sata0Obj.sobj_State == SATA_PWR_DWN) && (sata1Obj.sobj_State == SATA_PWR_DWN))
		{			
			ata_startup_hdd(0, SATA_TWO_CHs);
		}
		else if (sata0Obj.sobj_State == SATA_PWR_DWN)
			ata_startup_hdd(0, SATA_CH0);
		else if (sata1Obj.sobj_State == SATA_PWR_DWN)
			ata_startup_hdd(0, SATA_CH1);
		
		// turn off interrupt
		*chip_IntEn = 0; // disable chipInt
		*chip_IntStaus = 0xFFFFFFFF; 
		auxreg_w(REG_CONTROL0, 0x0); // disable time interrupt
		*spi_IntStatus = SPI_IntStatus_Done;
		Delay(50); // 50ms
		*cpu_Clock |= ASIC_RESET; // HW will generate a PORT
		Delay(1); // suppose that cpu should be restarted by ASIC_RESET
	}
#endif
	
	// drive power management
	if ((sata0Obj.sobj_State > SATA_READY) || (sata1Obj.sobj_State > SATA_READY))
	{
#if 1	
		if ((sata0Obj.sobj_State > SATA_READY) && (sata1Obj.sobj_State > SATA_READY))
			scan_sata_device(SATA_TWO_CHs);
		else
		{
			if (sata0Obj.sobj_State > SATA_READY)
				scan_sata_device(SATA_CH0);
			else
				scan_sata_device(SATA_CH1);
		}
#else
		if ((sata0Obj.sobj_State > SATA_READY) || (sata1Obj.sobj_State > SATA_READY))
		{
			if (sata0Obj.sobj_State > SATA_READY)
			{
				sata_HardReset(&sata0Obj);		
				sata_InitSataObj(&sata0Obj);
			}
			if (sata1Obj.sobj_State > SATA_READY)
			{
				sata_HardReset(&sata1Obj);		
				sata_InitSataObj(&sata1Obj);
			}
			rw_time_out = 200;
			_disable();
#ifdef FPGA
			chk_fis34_timer = 50; // 5sec
#else
			chk_fis34_timer = 40; // 2sec
#endif
			spinup_timer = 3; //wait for 7.5secs for the Fis34 after power on
			cntdwn_pwron_phyRdy = 1; // when 2 secs is timeout, assume that there's no device attached	
			_enable();
			
			while (1)
			{
				if (*chip_IntStaus & SATA0_INT)
				{	
					sata_isr(&sata0Obj);
				}

				if (*chip_IntStaus & SATA1_INT)
				{	
					sata_isr(&sata1Obj);
				}

				if (((sata0Obj.sobj_device_state == SATA_NO_DEV) ||(sata0Obj.sobj_device_state == SATA_DEV_READY))
				&& ((sata1Obj.sobj_device_state == SATA_NO_DEV) ||(sata1Obj.sobj_device_state == SATA_DEV_READY)))
				{
					_disable();
					cntdwn_pwron_phyRdy = 0; 
					rw_time_out = 0;
					_enable();
					break;
				}
				
				if (rw_time_out == 0)
				{
					rw_time_out = 200;
					if (sata0Obj.sobj_device_state == SATA_DEV_UNKNOWN)
					{
						sata_timeout_reset(&sata0Obj);
					}
					if (sata1Obj.sobj_device_state == SATA_DEV_UNKNOWN)
					{
						sata_timeout_reset(&sata1Obj);
					}
				}
			}
		}
#endif
		// clear the USB & SATA & Dbuf block for the case that the transferring is stoped by USB hot plug
#ifdef AES_EN
		*aes_control  &= ~(AES_DECRYP_EN | AES_DECRYP_KEYSET_SEL | AES_ENCRYP_EN | AES_ENCRYP_KEYSET_SEL);
#endif
		sata_ch_reg = sata0Obj.pSata_Ch_Reg;
		sata_ch_reg->sata_BlkCtrl = RXSYNCFIFORST | TXSYNCFIFORST; //reset sata RX  fifo	
#ifdef SATARXFIFO_CTRL
		sata_ch_reg->sata_PhyCtrl &= ~BIT_22;   // sataFIFO DDONE clear
#endif					
		sata_ch_reg->sata_BlkCtrl;

		sata_ch_reg = sata1Obj.pSata_Ch_Reg;
		sata_ch_reg->sata_BlkCtrl = RXSYNCFIFORST | TXSYNCFIFORST; //reset sata RX  fifo	
#ifdef SATARXFIFO_CTRL
		sata_ch_reg->sata_PhyCtrl &= ~BIT_22;   // sataFIFO DDONE clear
#endif					
		sata_ch_reg->sata_BlkCtrl;
		
		rst_all_seg();

		Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control;

		*usb_Msc0DICtrlClr = MSC_TXFIFO_RST;
		*usb_Msc0DICtrlClr;					
		*usb_Msc0DOutCtrlClr= MSC_RXFIFO_RST;
		*usb_Msc0DOutCtrlClr;	
	}

	raid_rb_state = RAID_RB_IDLE;
	raid_rebuild_verify_status = 0;
	
#ifdef SUPPORT_HR_HOT_PLUG
	pre_sata_active = 0;
	if (sata0Obj.sobj_device_state == SATA_DEV_READY)
		pre_sata_active |= 0x01;
	if (sata1Obj.sobj_device_state == SATA_DEV_READY)
		pre_sata_active |= 0x02;
	if (hot_plug_enum)
	{
		Delay(500);
		hot_plug_enum = 0;
		usb_active = 0;
		raidEnumPerformed = 0;
		goto detect_mode;
	}
#endif
#ifdef HARDWARE_RAID
	if (raid_config_button_pressed)
	{
		MSG("con_enum\n");
		*usb_DevCtrlClr = USB_ENUM;
		Delay(250);
		if (cfa_active == 0)
			hw_raid_config(1);
		raid_config_button_pressed = 0;
		usb_active = 0;
		raidEnumPerformed = 0;
		goto detect_mode;
	}
#endif	

	// Discard unreported power button presses; the PC wasn't listening.
	power_button_pressed = 0;
	seal_drive();
	
	// Turn off power to the HDD and whatever else needed.
	ata_shutdown_hdd(TURN_POWER_OFF, SATA_TWO_CHs);
	//sata0Obj.sobj_State = SATA_PWR_DWN;
	//sata1Obj.sobj_State = SATA_PWR_DWN;

	// turn off LED
	lights_out();
	MSG("lit off\n");
#if FAN_CTRL_EXEC_TICKS
	enableFan(0);
#endif

	disable_upkeep &= ~(UPKEEP_ATA_QUIESCENCE_S0 | UPKEEP_ATA_QUIESCENCE_S1);
	// turn off SATA PHY, etc
	goto START_UP;
}
