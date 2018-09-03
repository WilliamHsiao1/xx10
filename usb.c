/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/
 
#define USB_C

#include	"general.h"

/****************************************\
	UsbExec_Init
\****************************************/
void UsbExec_Init(void)
{
//	u32 val;
//	u32 i;
	InitDesc();


	*usb_Msc0IntStatus = 0xffffffff;
	*usb_IntStatus = 0xffffffff;
	// when the VBUS has dip, the enumeration will not be turn off
	*usb_CoreCtrl = *usb_CoreCtrl & (~VBUS_RESET_ENUM_EN) | VBUS_ENUM_DISABLE;
	
	if (ncq_supported == 0)
	{
		*usb_Msc0Ctrl = MSC_BOT_ENABLE;
	}
	else
	{
		// always enable UAS and BOT
		*usb_Msc0Ctrl = MSC_UAS_ENABLE |MSC_BOT_ENABLE;
#ifdef UAS
		*usb_DeviceRequestEn &= ~SET_INTERFACE_ENABLE;
#endif
	}


	if (DISABLE_U1U2() == 0)
	{
		*usb_DevCtrlClr = (USB3_U1_REJECT | USB3_U2_REJECT);	
		u1U2_reject_state = U1U2_ACCEPT;
	}
	else
	{
		*usb_DevCtrl = (USB3_U1_REJECT | USB3_U2_REJECT);	
		u1U2_reject_state = U1U2_REJECT;
	}
	

#if WDC_ENABLE_QUICK_ENUMERATION
	if (quick_enum)
		MSG("Q-Em\n");
	else
#endif
		MSG("Em\n");

#ifdef FPGA
	*usb_link_debug_param = (*usb_link_debug_param & ~USB_RX_DETECT_CNT) | USB_RX_DETECT_CNT_EQ_8 | USB_DISCARD_HEADPACKET_WITH_K_BYTE|USB3_TXLINK_MODE;
#else
	*usb_link_debug_param = (*usb_link_debug_param & ~USB_RX_DETECT_CNT) | USB_RX_DETECT_CNT_EQ_8 | USB_DISCARD_HEADPACKET_WITH_K_BYTE|USB3_TXLINK_MODE|UX_EXIT_LFPS_LOW|USB3_Ux_EXIT_DISABLE_FW|LFPS_MODE_1|DISCARD_ZERO_LENGTH_QUALIFIER;
#endif
	// change the max post to 4 to see if the 8 bytes "0" issue will pop up 06/27/2012 
	*usb_USB3Option = (*usb_USB3Option & (~(USB3_DP_SKID_DISABLE_1|USB3_DP_SKID_DISABLE_0|USB3_MAX_POST))) | USB3_DP_SKID_DISABLE_1|USB3_MAX_POST4|USB3_DISABLE_FLOW_DEAD|USB3_ENABLE_ACTIVE_FLOW_PRIME; 
	*usb_USB2Option |= (USB2_WARM_RESET_CONN|USB2_SS_INACTIVE_CONN |USB2_WARM_RESET_SEL_CONN |USB2_SS_INACTIVE_PSM|USB2_WARM_RESET_PSM| USB2_FS_QUALIFIER_POLLING | USB2_QUALIFIER_POLLING| USB2_MODE_PRIORITY); //USB2_MODE_PRIORITY. if the Connection Manager sets USB2_MODE the LTSSM is reset to DISABLED.
//	*usb_Msc0CtxtLimit = 32;
	*usb_Msc0EndPoint_num = 0x1234;
	*usb_USB2_connect_timer = 0x000961a7; // time tick is 1ms, 10ms to timeout
	*usb_usb2_FS_connection_timer = 0x001a61a7; // time tick is 1ms, 30ms to timeout
	*usb_USB3_Exit_Timer = 0x0101; 
	*usb_USB3_LFPS_Timer = 0x00010113;
#ifdef LA_DUMP
	*usb_USB3_Rxelecidle_Filter = 0x05;
#endif	
	*usb_CtxtSize = 0xC000 | MAX_UAS_CTXT_SITE;

	*usb_Msc0LunCtrl = (*usb_Msc0LunCtrl & ~0x70); // LUN0
	*usb_Msc0LunSATCtrl = (*usb_Msc0LunSATCtrl & ~SAT_CMD) | SATA_NCQ_CMD;
#ifndef DATASTORE_LED
    if ((arrayMode == RAID0_STRIPING_DEV)
#ifdef SUPPORT_RAID1
        || (arrayMode == RAID1_MIRROR)
#endif
#ifdef SUPPORT_SPAN
		|| (arrayMode == SPANNING)
#endif
		)
	{
		*usb_Msc0LunExtent = array_data_area_size - 1;
	}
	else
	{
		*usb_Msc0LunExtent = vital_data[HDD0_LUN].lun_capacity;
	}
#else
        *usb_Msc0LunExtent = vital_data[HDD0_LUN].lun_capacity;
#endif

	if (HDD1_LUN != UNSUPPORTED_LUN)
	{
		// set the SAT transfer for HDD1 lun
		*usb_Msc0LunCtrl = (*usb_Msc0LunCtrl & 0x7F) | 0x10; // set the LUN ctrl index to 1
		*usb_Msc0LunSATCtrl = (*usb_Msc0LunSATCtrl & ~SAT_CMD) | SATA_NCQ_CMD;
		*usb_Msc0LunExtent = vital_data[HDD1_LUN].lun_capacity;
	}
	*cpu_Clock_restore = (*cpu_Clock_restore & ~(DBUFRX_CLK_DIV_RESTORE | DBUFTX_CLK_DIV_RESTORE|AES_CLK_DIV_RESTORE | CPU_CLK_DIV_RESTORE)) | (DBUF_CLOCK << 16) | (DBUF_CLOCK << 12) |(PERIPH_CLOCK<< 8)|(AES_CLOCK << 4) | BEST_CLOCK;
	*cpu_clock_power_down_U1 = ((*cpu_clock_power_down_U1 & ~CPUCLK_DIV_U1) | PD_DBUFRX_U1|PD_DBUFTX_U1|PD_AES_U1 | BEST_CLOCK); // set the div of AES & DBUF & CPU clock to lowest value in U1 & U2
	*cpu_clock_power_down_U2 = ((*cpu_clock_power_down_U2 & ~CPUCLK_DIV_U2) | PD_DBUFRX_U2|PD_DBUFTX_U2|PD_AES_U2 | BEST_CLOCK); // set the div of AES & DBUF & CPU clock to lowest value in U1 & U2
	DBG("usb2 op: %x\n", *usb_USB2Option);
	*usb_U3IdleTimeout = 0;
#ifdef SET_CLR_FEAT_FW
	*usb_DeviceRequestEn &= ~(USB_CLEAR_FEATURE_ENABLE | USB_SET_FEATURE_ENABLE|USB_GET_STATUS_ENABLE);
#endif
	*usb_Core_ctrl2 |= USB_DATAOUT_PREDICT_DISABLE; // set burst NumP = available buffer size
	*usb_Msc0DOutCtrl = MSCn_DATAOUT_SDONE_DISABLE;
	MSG("DOUTCTRL %bx\n", *usb_Msc0DOutCtrl);
	// enable USB enumeration after basic USB registers are set up
	*usb_link_debug_param = (*usb_link_debug_param & ~USB_RX_DETECT_CNT) | USB_RX_DETECT_CNT_EQ_8;
	*usb_CoreCtrl = (*usb_CoreCtrl & ~(USB3_BURST_EN | USB3_BURST_MAX_TX|USB3_BURST_MAX_RX | USB3_BURST_TCNT )) | USB3_BURST_TCNT | BURST_PCKETS_NUM | USB_CMD_PENDING_ENABLE;
	*usb_Ep0IntEn = EP0_SETUP;
	*usb_IntEn = CDB_AVAIL_INT|MSC0_INT|CTRL_INT|HOT_RESET|WARM_RESET|USB_BUS_RST|VBUS_OFF;
	rx_detect_count = RX_DETECT_COUNT8;
	*usb2_Timing_Control0 = (*usb2_Timing_Control0 & ~USB2_PWR_ATTACH_MIN) | USB2_PWR_ATTACH_MIN_80MS;
	//Self Power
	if ((product_detail.options & PO_IS_PORTABLE) == 0)	
		*usb_DevCtrl = SELF_POWER;
	//enable USB enumeration
	mscInterfaceChange = 0;
	curMscMode = MODE_UNKNOWN;
	*usb_DevCtrl = (USB_FW_RDY | USB_ENUM);
	
#ifdef DEBUG_LTSSM
	*usb_USB3StateSelect = USB3_LTSSM;
	u8 usb_st_mc = *usb_USB3StateCtrl;
	u16 usb_ltssm_state = *usb_USB3LTSSM_shadow;
	MSG("u %bx, %x\n", usb_st_mc, usb_ltssm_state);
#endif
}



#if (PWR_SAVING >= PWR_SAVING_LEVEL2)

void	usb30_clock_disable(void)
{

#ifdef DBG_PWR_SAVING
	u8 temp8;
	/*
	PWR_SAVING_DOWN___USB3 -1____00002
	Analog PHY TX & RX & PLL & VREG all power down.
	Hardware can turn on the Analog PHY TX & RX & PLL & VREG.
	*/
	spi_phy_wr_retry(PHY_SPI_U3PCS, 0x13, PD_USB3_TX|PD_USB3_RX|PD_USB3_PLL);
	spi_phy_wr_retry(PHY_SPI_U3PCS, 0x13, PD_USB3_TX|PD_USB3_RX|PD_USB3_VREG|PD_USB3_PLL);


	/*
	PWR_SAVING_DOWN___USB3 -2_____00003
	RX common mode voltage to AGND. 
	1: switch RX common mode voltage to AGND(in suspend mode )
	0: RX common mode voltage 0.5V 
	*/
	if (*chip_Revision < 0x03)
	{
		if(usbMode != CONNECT_USB3)
		{
			// In USB 2.0 mode set to 1 LFPS receive function disable.
			temp8 = spi_phy_rd(PHY_SPI_U3PCS, 0x12);
			spi_phy_wr_retry(PHY_SPI_U3PCS, 0x12, temp8|BIT_1);
		}
	}

	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x0A);	
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x0A, temp8|BIT_0);	


	
	/*
	PWR_SAVING_DOWN___INIC3610 Function7 - 3___00006
	DBUF TX<27b> RX<28b>  &  periph<26b> &  AES<25b> Clock disable 
	*/
	*cpu_clock_select  |= BIT_25 |BIT_26 |BIT_27 |BIT_28;

	
	/*
	PWR_SAVING_DOWN___INIC3610 Function7 - 4___00007
	power down the VBUS_SENSE input circuit
	When  powered down the VBUS Sense signal into the ASIC is 0 (OFF).
	*/
	*asic_mode |= BIT_4; 


	
	/*
	PWR_SAVING_DOWN___USB3 - 3___00009
	USB2 PLL power down
	*/
	temp8 = spi_phy_rd(PHY_SPI_U2, 0x09);
	spi_phy_wr_retry(PHY_SPI_U2, 0x09, temp8 & (~BIT_1));
	
	/*
	 PWR_SAVING_DOWN___INIC3610_Function7 - 5
	<b2:0> Adjust switching regulator output voltage
	000: 1.02v  / 001:0.90v / 010:0.96v / 011:0.84v 
	100: 0.78v /  101:1.14v / 110:0.72v / 111:1.08v
	<b19> & <b06>  Turn off the protection circuit.
	 */


	/*
	PWR_SAVING_DOWN___SATA3 - 2___00004
	turn off sata 1.0v power switch regulator
	power off SATA0 SATA1 RX TX power switch.
	turn of the power to the SATA1/0 RX & TX logic.
	*/
	if (*chip_Revision < 0x03)
	{
		*switching_regulator_reg &= ~(BIT_27 | BIT_26 | BIT_25 |BIT_24);	
	}

	/*
	PWR_SAVING_DOWN___SATA3 - 3
	RX/TX Hi-Z mode impedance setting as open.
	*/
	temp8 = spi_phy_rd(PHY_SPI_SATA1,0x00);
	spi_phy_wr_retry(PHY_SPI_SATA1, 0x00, temp8 & (~(BIT_6 |BIT_5 | BIT_4)));
	
	temp8 = spi_phy_rd(PHY_SPI_SATA0,0x00);
	spi_phy_wr_retry(PHY_SPI_SATA0, 0x00, temp8 & (~(BIT_6 |BIT_5 | BIT_4)));


      /*
      PWR_SAVING_DOWN___INIC3610_Function7 - 6____000010 & 000011
      turned off  (internal current reference module <21b>) & (internal LDO <22b>)
     */ 
	*switching_regulator_reg |= PD_LDO; 
	*switching_regulator_reg |= (PD_IVREF |PD_LDO);
	/*
	PWR_SAVING_DOWN___INIC3610 Function7 - 7____000012
	When this bit is set to 1 the input crystal oscillator is disabled and no clocks are present in the ASIC. 
	This bit is only set during USB Suspend states.
	*/
	*cpu_Clock |= XTAL_DISABLE;		
#else

	spi_phy_wr_retry(PHY_SPI_U3PCS, 0x13, PD_USB3_TX|PD_USB3_RX);

#endif


}

void usb_clock_disable(void)
{
	/*
	PWR_SAVING_DOWN___INIC3610 Function7 - 1___00008
	<b29> set the source for USBCLK is REFCLK, otherwise it is sourced from the active USB PLL,
	Hardware  will clear it when USB Wakeup Signaling is detected.
	<b00> set to 1 the CPU Clock is driven by REFCLK.
	*/
	*cpu_Clock |= (USBCLK_SELECT|SEL_REF_CPU|SEL_REF_PER);
	set_uart_tx(CLOCK_DEFAULT);	
	
	if ((usbMode != CONNECT_USB3) ||(*usb_DevStatus & USB2_MODE))
	{
		// in USB2/USB1.1 mode, turn off U2 PLL freerun
		if ((usbMode == CONNECT_USB3) && (*usb_DevStatus & USB2_MODE))
		{
			MSG("SP C\n");
		}
		else
		{
			
			*usb_DevCtrlClr =	USB2_PLL_FREERUN;	// disable U2 PLL Freerun
			MSG("D FR\n");
		}
	}
	else
	{	
		/*
		PWR_SAVING_DOWN___INIC3610_USB23_Function7 - 2____00001
		<b10>When this bit is set to 1 the USB 2.0 Interface will go to the Suspend state.
		This signal is reset when the hardware detects signals on DP/DM
		*/
		*usb_DevCtrl = USB2_FORCE_SUSP;
	}

	usb30_clock_disable();
	
	
}



void	usb_clock_enable(void)
{	

	/*
	Hardware can turn on the Analog PHY TX & RX & PLL & VREG____00002
	sata_phy_pwr_ctrl ____00005
	CDR module<b7> & serial module<b6> & TX driver<b5> & PLL<b4> & voltage regulator<b3> 
	Power analog front end bias<b2> & input buffer<b1> & squelch detection module<b0> 
	*/
	
	u8 temp8;
	/*
	PWR_SAVING_UP___XYZ9 - 1___SATA3 - 1___00004
	power up sata 1.0v power switch regulator
	power up sata0 & sata1 RX & TX power switch.
	turn on the power to the sata1 & 0 RX & TX logic.
	*/
	if (*chip_Revision < 0x03)
	{
		*switching_regulator_reg &= ~(BIT_27 | BIT_26 | BIT_25 |BIT_24);	
	}
	
	*switching_regulator_reg &= ~(DIS_OVER_CURRENT_DETECTION | DIS_SHORT_CIRCUIT_DETECTION);

	while (1)
	{
		if (((*chip_IntStaus & USB3_PLL_RDY) == USB3_PLL_RDY) && 
			((*chip_IntStaus & USB2_PLL_RDY) == USB2_PLL_RDY))
		{
			break;
		}
	}

	/* 
	PWR_SAVING_UP___XYZ9 - 2___INIC3610 Function7 - 4___00007
	power up the VBUS_SENSE input circuit.
	*/
	 *asic_mode &= ~BIT_4; 

	/*
	PWR_SAVING_UP___XYZ9 - 3___INIC3610 Function7 - 3___00006
	DBUF TX(27b) RX(28b)  &  periph(26b) &  AES(25b) Clock enable 
	*/
	*cpu_clock_select  &= ~(BIT_25 |BIT_26 |BIT_27 |BIT_28);

	/*
	PWR_SAVING_UP___XYZ9 - 4___USB3 -2_____00003
	RX common mode voltage 0.5V 
	*/
	temp8 = spi_phy_rd(PHY_SPI_U3PMA, 0x0A);
	spi_phy_wr_retry(PHY_SPI_U3PMA, 0x0A, temp8 & (~BIT_0));

	/*
	PWR_SAVING_UP___XYZ9 - 5___INIC3610 Function7 - 1
	resets the USBCLK_SELECT, CPU, PER signals to switch the source to the USB PLL
	*/
	*cpu_Clock &= ~(USBCLK_SELECT|SEL_REF_CPU|SEL_REF_PER);

	set_uart_tx(CLOCK_64M);

	/*
	PWR_SAVING_UP___XYZ9 - 6___INIC3610_USB23_Function7 - 2____00001
	clear force suspend bit.
	*/

	*usb_DevCtrlClr = USB2_FORCE_SUSP;
	*usb_DevCtrl = USB2_PLL_FREERUN;

	/*
	PWR_SAVING_UP___XYZ9 - 7___INIC3610_USB23_Function
	keep PLL on in suspend mode otherwise the HW will auto pwr down PLL.
	*/
	spi_phy_wr_retry(PHY_SPI_U2, 0x09, BIT_1);
	
	MSG(("\nU3 R\n"));
}

#endif



// to save the code size
// return val 0 means resume from suspend
// 1 -> vbus is off
u8 usb_suspend(void)
{
	u8 tmp8;
	u8 sataPwrCtl = 0;
	
#if (PWR_SAVING)
	turn_on_USB23_pwr();
#endif
	DBG("Susp\n");
#ifdef USEGPIO2
	*cpu_wake_up = (*cpu_wake_up & 0x00FFFF00) | CPU_USB_SUSPENDED|CPU_USB_UNSUSPENED |VBUS_SELECT_GPIO2_SENSE;
#else
	*cpu_wake_up = (*cpu_wake_up & 0x00FFFF00) | CPU_USB_SUSPENDED|CPU_USB_UNSUSPENED;
#endif
	if ( *usb_DevState_shadow & L1_SLEEP)
	{// because the L1 share the same interrupt with suspend, just clear it when it's L1 sleep
		return 0;	
	}
#if (PWR_SAVING)
	turn_on_pwr = 1;	
#endif

#if 1
	if ((sata0Obj.sobj_device_state == SATA_DEV_READY) && (sata0Obj.sobj_State == SATA_READY))
	{
		ata_ExecNoDataCmd(&sata0Obj, ATA6_FLUSH_CACHE_EXT, 0, WDC_SATA_TIME_OUT);
	}
	if ((sata1Obj.sobj_device_state == SATA_DEV_READY) && (sata1Obj.sobj_State == SATA_READY))
	{
		ata_ExecNoDataCmd(&sata1Obj, ATA6_FLUSH_CACHE_EXT, 0, WDC_SATA_TIME_OUT);
	}
#endif
	u16 usb_suspend_check_timer = 10000;
	
	if ((((sata0Obj.sobj_State == SATA_RESETING) || (sata0Obj.sobj_State == SATA_PHY_RDY) || (sata0Obj.sobj_State == SATA_PRE_READY)) && (sata0Obj.sobj_device_state != SATA_NO_DEV))
	|| (((sata1Obj.sobj_State == SATA_RESETING) || (sata1Obj.sobj_State == SATA_PHY_RDY) || (sata1Obj.sobj_State == SATA_PRE_READY)) && (sata1Obj.sobj_device_state != SATA_NO_DEV)))
	{
		usb_suspend_check_timer = 30000;
	}

	DBG("susp timer %x\n", usb_suspend_check_timer);
	for (u16 tmp16 = 0; tmp16 < usb_suspend_check_timer; tmp16++)
	{
		Delay(1);
		if(*cpu_wake_up & CPU_USB_UNSUSPENED)
		{
			MSG("srsm\n");
			*cpu_wake_up = (*cpu_wake_up & 0x00FFFF00) | CPU_USB_SUSPENDED;
			return 0;
		}
		if (USB_VBUS() == 0)
		{
			DBG("VBus off\n");
			return 1;
		}
		if ((*usb_IntStatus_shadow & (USB_BUS_RST |HOT_RESET|CTRL_INT)) || (*usb_USB3LTSSM_shadow & LTSSM_U0))
		{
			*cpu_wake_up = (*cpu_wake_up & 0x00FFFF00) | CPU_USB_SUSPENDED;
			*usb_IntStatus = USB_SUSPEND;
			Delay(1);
			if ((*cpu_wake_up & CPU_USB_SUSPENDED) == 0)
			{
				DBG("special case\n");
				return 0;
			}
		}
		
		if (((sata0Obj.sobj_device_state != SATA_NO_DEV) && ((sata0Obj.sobj_State == SATA_RESETING) || (sata0Obj.sobj_State == SATA_PHY_RDY) || (sata0Obj.sobj_State == SATA_PRE_READY)))
		|| ((sata1Obj.sobj_device_state != SATA_NO_DEV) && ((sata1Obj.sobj_State == SATA_RESETING) || (sata1Obj.sobj_State == SATA_PHY_RDY) || (sata1Obj.sobj_State == SATA_PRE_READY))))
		{
			if (*chip_IntStaus & SATA0_INT)
			{
				sata_isr(&sata0Obj);
			}

			if (*chip_IntStaus & SATA1_INT)
			{
				sata_isr(&sata1Obj);
			}
		}
		else
		{
			if (tmp16 > 10000)
				break;
		}
	}
	
	if ((sata0Obj.sobj_device_state == SATA_DEV_READY) && (sata0Obj.sobj_State == SATA_READY))
		ata_ExecNoDataCmd(&sata0Obj, ATA6_STANDBY_IMMEDIATE, 0, WDC_SATA_TIME_OUT);
	if ((sata1Obj.sobj_device_state == SATA_DEV_READY) && (sata1Obj.sobj_State == SATA_READY))
		ata_ExecNoDataCmd(&sata1Obj, ATA6_STANDBY_IMMEDIATE, 0, WDC_SATA_TIME_OUT);
	
	if ((nvm_unit_parms.mode_save.loose_locking & OPS_LOOSE_LOCKING) == 0)	
	{
		seal_drive();
	}
#ifndef SUPPORT_HR_HOT_PLUG	
#if (PWR_SAVING >= PWR_SAVING_LEVEL2)
	if (hdd_power)
	{	
		sataPwrCtl = (PWR_CTL_SLOT0_EN |PWR_CTL_SLOT1_EN)  | ((PD_SLOT0 |PD_SLOT1));
		set_hdd_power(sataPwrCtl, 0);
#if FAN_CTRL_EXEC_TICKS
		turn_off_pwm_fan(); // turn off the FAN output yo save power
#endif
#ifdef HDR2526_LED

#else
		*gpioOutEn &= ~(ERROR_LED_D0|ERROR_LED_D1|ACTIVITY_LED_PIN_D0|ACTIVITY_LED_PIN_D1|DRIVE0_POWER_PIN | DRIVE1_POWER_PIN);
#endif
	}
#endif
#endif
	// turn off SATA PHY, etc
	DBG("CLOCK_DEFAULT\n");

	// ...and the lights, but blink the power LED to show standby.
	lights_out();
	set_activity_for_standby(1);
	
	auxreg_w(REG_CONTROL1, 0x0); // disable timer 1
#ifdef USEGPIO2
	*cpu_wake_up = (*cpu_wake_up & 0x00FFFF00) | CPU_USB_SUSPENDED|VBUS_SELECT_GPIO2_SENSE;
#else
	*cpu_wake_up = (*cpu_wake_up & 0x00FFFF00) | CPU_USB_SUSPENDED;
#endif

#if (PWR_SAVING >= PWR_SAVING_LEVEL2)
	if (usbMode != CONNECT_UNKNOWN)
		usb_clock_disable();
#endif

	MSG("Susp!\n");

#if (PWR_SAVING >= PWR_SAVING_LEVEL2)
	u32 prev_cpu_ck = *cpu_Clock;
#endif

	while(1)
	{
#if (PWR_SAVING >= PWR_SAVING_LEVEL2)
		if ((usbMode == CONNECT_USB2) || (usbMode == CONNECT_USB1))
		{
			u32 tmp32 = 	*cpu_Clock;
			if (tmp32 != prev_cpu_ck)
			{
				prev_cpu_ck = tmp32;
				*cpu_wake_up = (*cpu_wake_up & 0x00FFFF00) | USB2_WAKEUP_REQ;
				//MSG("U2 W\n");
			}
		}	
#endif		
		if (USB_VBUS_OFF())
		{
			//MSG("vbus off\n");

#if (PWR_SAVING >= PWR_SAVING_LEVEL2)
			if (usbMode != CONNECT_UNKNOWN)
			{
				usb_clock_enable();
			}
#endif


			return 1;
		}		

		if (power_on_flag == 1)
		{
			if (*usb_IntStatus_shadow & USB_BUS_RST)
			{
#ifdef USEGPIO2
				*cpu_wake_up = (*cpu_wake_up & 0x00FFFF00) | CPU_USB_SUSPENDED|VBUS_SELECT_GPIO2_SENSE;
#else
				*cpu_wake_up = (*cpu_wake_up & 0x00FFFF00) | CPU_USB_SUSPENDED;
#endif
				*usb_IntStatus = USB_SUSPEND;
				DBG("clr %lx, %lx\n", *cpu_wake_up, *usb_IntStatus);
				Delay(1);
				DBG("%lx, %lx\n", *cpu_wake_up, *usb_IntStatus);	
				if ((*cpu_wake_up & CPU_USB_SUSPENDED) == 0)
					break;
			}
		}
		
		if (*cpu_wake_up & CPU_USB_UNSUSPENED)
		{
#if (PWR_SAVING >= PWR_SAVING_LEVEL2)
			if (usbMode != CONNECT_UNKNOWN)
				usb_clock_enable();
#endif
#ifdef USEGPIO2
			*cpu_wake_up = (*cpu_wake_up & 0x00FFFF00) | CPU_USB_SUSPENDED|VBUS_SELECT_GPIO2_SENSE;
#else
			*cpu_wake_up = (*cpu_wake_up & 0x00FFFF00) | CPU_USB_SUSPENDED;
#endif
			break;
		}
	}

#if (PWR_SAVING >= PWR_SAVING_LEVEL2)
	sataPwrCtl = 0;
	if (sata0Obj.sobj_device_state == SATA_DEV_READY)
	{
		sataPwrCtl |= (PU_SLOT0 | PWR_CTL_SLOT0_EN);
	}
	if (sata1Obj.sobj_device_state == SATA_DEV_READY)
	{
		sataPwrCtl |= (PU_SLOT1 | PWR_CTL_SLOT1_EN);
	}
	MSG("pwr %bx, DEV %bx, %bx\n", sataPwrCtl, sata0Obj.sobj_device_state, sata1Obj.sobj_device_state);
	set_hdd_power(sataPwrCtl, 0);
	spinup_timer = 3;
	chk_fis34_timer = 30;
#if FAN_CTRL_EXEC_TICKS
	fan_ctrl_exec_flag = 1;
#endif
#ifdef HDR2526_LED

#else
	*gpioOutEn |= (ERROR_LED_D0|ERROR_LED_D1|ACTIVITY_LED_PIN_D0|ACTIVITY_LED_PIN_D1|DRIVE0_POWER_PIN | DRIVE1_POWER_PIN);
#endif
	#if (PWR_SAVING >= PWR_SAVING_LEVEL2)
	/*
	PWR_SAVING_UP___XYZ9 - 9___SATA3 - 3
	set high hiz as 1K (RX/TX Hi-Z mode impedance setting  (ohms))
	*/
	tmp8 = spi_phy_rd(PHY_SPI_SATA1,0x00);
	spi_phy_wr_retry(PHY_SPI_SATA1, 0x00, tmp8 |BIT_6);
	
	tmp8 = spi_phy_rd(PHY_SPI_SATA0,0x00);
	spi_phy_wr_retry(PHY_SPI_SATA0, 0x00, tmp8 |BIT_6);
	#endif
#endif

	
	spi_phy_wr_retry(PHY_SPI_U3PCS, 0x13, FW_CTRL_USB3_PD_RX_EN|FW_CTRL_USB3_PD_TX_EN|FW_CTRL_USB3_PD_VREG_EN|FW_CTRL_USB3_PD_PLL_EN);
	
	set_activity_for_standby(0);

	init_timer1_interrupt(100, BEST_CLOCK);

#if (PWR_SAVING)
	turn_on_pwr = 1;	
#endif

#ifdef USB2_L1_PATCH
	_disable();
	usb2_L1_reject_timer = 10;
	*usb_DevCtrl = USB2_L1_DISABLE;
	_enable();
#endif
//	sata_HardReset(pSataObj);
	MSG("wake\n");
	return 0;
}

void usb_host_rst()
{
#if (PWR_SAVING)
	turn_on_USB23_pwr();
#endif
	*usb_Msc0IntStatus = MSC_DI_TIMEOUT_INT|MSC_DO_TIMEOUT_INT|MSC_CONFIG_INT|MSC_TX_DONE_INT|MSC_RX_DONE_INT|BOT_RST_INT|MSC_DIN_HALT_INT|MSC_DOUT_HALT_INT;
	*usb_IntStatus = CDB_AVAIL_INT|MSC0_INT|CTRL_INT;

	*usb_Msc0DOutCtrlClr = MSC_RXFIFO_RST;
	*usb_Msc0DICtrlClr = MSC_TXFIFO_RST;

	// turn off SATA PHY
	if (sata0Obj.sobj_State > SATA_READY)
	{
		sata_ch_reg = sata0Obj.pSata_Ch_Reg;
		sata_ch_reg->sata_PhyCtrl &= ~PHYPWRUP;	
		// turn on SATA PHY
		sata_ch_reg->sata_PhyCtrl |= PHYPWRUP;	
		sata_ch_reg->sata_BlkCtrl |= (RXSYNCFIFORST | TXSYNCFIFORST);	//reset sata TX  fifo
		sata_ch_reg->sata_BlkCtrl;
	}
	
	if (sata1Obj.sobj_State > SATA_READY)
	{
		sata_ch_reg = sata1Obj.pSata_Ch_Reg;
		sata_ch_reg->sata_PhyCtrl &= ~PHYPWRUP;	
		// turn on SATA PHY
		sata_ch_reg->sata_PhyCtrl |= PHYPWRUP;
		sata_ch_reg->sata_BlkCtrl |= (RXSYNCFIFORST | TXSYNCFIFORST);	//reset sata TX  fifo
		sata_ch_reg->sata_BlkCtrl;
	}

#ifdef AES_EN
	*aes_control &= ~(AES_DECRYP_EN | AES_DECRYP_KEYSET_SEL);
	*aes_control &= ~(AES_ENCRYP_EN | AES_ENCRYP_KEYSET_SEL);
#endif

	pSataObj = &sata0Obj;
	if (pSataObj->sobj_State > SATA_READY)//sata_ch_reg->sata_Status & (ATA_STATUS_BSY | ATA_STATUS_DRQ))
	{
		ERR("sata_Status %bx\n", sata_ch_reg->sata_Status);
		sata_Reset(pSataObj, USB_SATA_RST);
		_disable(); // it's found that the sata can't be ready after 1 comreset
		chk_fis34_timer = CHECK_FIS34_TIMER;
		_enable();
	}

	pSataObj = &sata1Obj;
	if (pSataObj->sobj_State > SATA_READY)//sata_ch_reg->sata_Status & (ATA_STATUS_BSY | ATA_STATUS_DRQ))
	{
		ERR("sata_Status %bx\n", sata_ch_reg->sata_Status);
		sata_Reset(pSataObj, USB_SATA_RST);
		_disable(); // it's found that the sata can't be ready after 1 comreset
		chk_fis34_timer = CHECK_FIS34_TIMER;
		_enable();
	}
		
	_disable();
	spinup_timer = 3; 	// the spin up timer is around 7 secs 
	switch_regs_setting(NORMAL_MODE);	
	_enable();

	*usb_Ep0CtrlClr = EP0_HALT;
	*usb_Msc0DICtrlClr = MSC_DIN_DONE | MSC_CSW_RUN | MSC_DI_HALT | MSC_TXFIFO_RST;
	*usb_Msc0DICtrlClr;
	*usb_Msc0DOutCtrlClr = MSC_DOUT_RESET | MSC_DOUT_HALT | MSC_RXFIFO_RESET ;
	*usb_Msc0DOutCtrlClr;
	*usb_RecoveryError = 0x00;                       //for usb hot rest
	*usb_U3IdleTimeout = 0x0;	
	
	if ((u1U2_reject_state == U1U2_REJECT) && (DISABLE_U1U2() == 0))
	{
		*usb_DevCtrlClr = (USB3_U1_REJECT | USB3_U2_REJECT);
		u1U2_reject_state = U1U2_ACCEPT;
	}

//	*usb_LinkCtrl |= TX_DATA3_ABORT_DISABLE;
	usbMode = CONNECT_UNKNOWN;

#ifdef AES_EN
	*aes_control  &= ~(AES_DECRYP_EN|AES_DECRYP_KEYSET_SEL);
	*aes_control  &= ~(AES_ENCRYP_EN | AES_ENCRYP_KEYSET_SEL);
#endif
	
	rw_flag = 0;
	
	*usb_Msc0IntStatus = BOT_RST_INT;
	*usb_IntStatus = HOT_RESET|WARM_RESET|USB_BUS_RST;
	*usb_DeviceRequestEn &= ~USB_GET_DESCR_STD_UTILITY;
	dbg_next_cmd = 0;
}

void usb_reenum(void)
{
#if (PWR_SAVING)
	turn_on_USB23_pwr();
	for (u16 i = 0; i < 60000; i++)
	{	
		if (*chip_IntStaus & USB3_PLL_RDY)
			break;
		Delayus(3);
	}
#endif
//	*usb_DevCtrl = USB_CORE_RESET;
	*usb_DevCtrlClr = USB_ENUM;
	DBG("re-enum\n");
	Delay(3000);
	*usb_DevCtrl = USB_ENUM;
	//Self Power
	if ((product_detail.options & PO_IS_PORTABLE) == 0)	
		*usb_DevCtrl = SELF_POWER;
}

void usb_tx_fifo_rst(void)
{
	*usb_Msc0DICtrlClr = MSC_TXFIFO_RST;
	*usb_Msc0DICtrlClr;
}
void usb_rx_fifo_rst()
{
	*usb_Msc0DOutCtrlClr= MSC_RXFIFO_RESET;
	*usb_Msc0DOutCtrlClr;
}

void rst_din(PCDB_CTXT pCtxt)
{
#ifdef AES_EN
	if (pCtxt->CTXT_FLAG & CTXT_FLAG_AES)
	{
		*aes_control &= ~(AES_DECRYP_EN|AES_DECRYP_KEYSET_SEL);
	}
#endif
	reset_dbuf_seg(pCtxt->CTXT_DbufIndex);
//	Tx_Dbuf->dbuf_Seg[pCtxt->CTXT_DbufIndex].dbuf_Seg_Control = SEG_RESET;

	*usb_Msc0DICtrlClr	= MSC_TXFIFO_RST;
	
	sata_ch_reg = sata0Obj.pSata_Ch_Reg;
	sata_ch_reg->sata_BlkCtrl |= RXSYNCFIFORST;	//reset sata RX  fifo
	sata_ch_reg->sata_BlkCtrl;
	
	if (sata1Obj.sobj_device_state == SATA_DEV_READY)
	{
		sata_ch_reg = sata1Obj.pSata_Ch_Reg;
		sata_ch_reg->sata_BlkCtrl |= RXSYNCFIFORST;	//reset sata RX  fifo
		sata_ch_reg->sata_BlkCtrl;
	}
	
	DBG("*usb_Msc0DICtrl %bx\n", *usb_Msc0DICtrl);	
	if (pCtxt->CTXT_DbufIndex != SEG_NULL)
		free_dbufConnection(pCtxt->CTXT_DbufIndex);
#ifdef AES_EN
	if (pCtxt->CTXT_FLAG & CTXT_FLAG_AES)
	{
		*aes_control &= ~(AES_DECRYP_EN|AES_DECRYP_KEYSET_SEL);	
	}
#endif
}

void rst_dout(PCDB_CTXT pCtxt)
{
	usb_rx_fifo_rst();
#ifdef AES_EN
	if (pCtxt->CTXT_FLAG & CTXT_FLAG_AES)
	{
		*aes_control &= ~(AES_DECRYP_EN|AES_DECRYP_KEYSET_SEL);
	}
#endif
	
	sata_ch_reg = sata0Obj.pSata_Ch_Reg;
	sata_ch_reg->sata_BlkCtrl |= TXSYNCFIFORST;	//reset sata TX  fifo
	sata_ch_reg->sata_BlkCtrl;
	if (sata1Obj.sobj_device_state == SATA_DEV_READY)
	{
		sata_ch_reg = sata1Obj.pSata_Ch_Reg;
		sata_ch_reg->sata_BlkCtrl |= TXSYNCFIFORST;	//reset sata TX  fifo
		sata_ch_reg->sata_BlkCtrl;
	}
	
	// Disconnect both input & output port
	if (pCtxt->CTXT_DbufIndex != SEG_NULL)
		free_dbufConnection(pCtxt->CTXT_DbufIndex);
#ifdef AES_EN
	if (pCtxt->CTXT_FLAG & CTXT_FLAG_AES)
	{
		*aes_control &= ~(AES_DECRYP_EN|AES_DECRYP_KEYSET_SEL);
	}
#endif
}
void reset_Endpoint(PCDB_CTXT  pCtxt)
{
	if (pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR)
	{
		rst_din(pCtxt);
		*usb_Msc0DICtrl = MSC_DATAIN_RESET;	
		*usb_Msc0DICtrlClr = MSC_TXFIFO_RST;
	}
	else
	{
		rst_dout(pCtxt);
		*usb_Msc0DOutCtrl = MSC_DOUT_RESET;

		*usb_Msc0DOutCtrlClr = MSC_RXFIFO_RST;
	}

	if (pCtxt->CTXT_DbufIndex != SEG_NULL)
	{
		reset_dbuf_seg(pCtxt->CTXT_DbufIndex);
		free_dbufConnection(pCtxt->CTXT_DbufIndex);
	}	
}
void set_csw_run(PCDB_CTXT pCtxt)
{
	if (pCtxt->CTXT_Status == CTXT_STATUS_ERROR)
	{
		if (raid_rb_state == RAID_RB_IDLE)
		{
			if (raid1_error_detection(pCtxt)) return;
		}
	}
	
	usb_deQue_ctxt(usb_ctxt.curCtxt);
	*usb_Msc0DIStatus = pCtxt->CTXT_Status;
	
	*usb_Msc0DICtrl = MSC_CSW_RUN;
	if (pCtxt->CTXT_Status == CTXT_STATUS_GOOD)
		bot_sense_data[0] = 0;
}

#ifdef WIN8_UAS_PATCH
void chk_condition_action_for_Intel_UAS(void)
{
	if (intel_SeqNum_monitor)
	{ 
		// if there's clear_feature halt following, it's intel host
		intel_SeqNum_monitor |= INTEL_SEQNUM_CHECK_CONDITION;
	}
	if (intel_host_flag || intel_SeqNum_monitor)
	{
		for (u8 i = 0; i < 200; i++)
		{
			if ( *usb_Ep0Ctrl & EP0_SETUP)
			{
				usb_control();
				break;
			}
			Delayus(10);
		}
	}
}
#endif

void usb_exec_tx_ctxt(PCDB_CTXT pCtxt)
{
	u8	segIndex;
	
	if (dbg_next_cmd)
	{
		MSG("u tx_cxt %bx, %bx, %bx\n", pCtxt->CTXT_Index, pCtxt->CTXT_DbufIndex, pCtxt->CTXT_FLAG);
	}
	// Set up Dbuf if necessary
	if ((segIndex = pCtxt->CTXT_DbufIndex) != SEG_NULL)
	{
#ifdef AES_EN
		if (pCtxt->CTXT_FLAG & CTXT_FLAG_AES)
		{
			if (pCtxt->CTXT_LUN == HDD0_LUN)
			{
				if (current_aes_key != AES_ENCRYPT_MODE)
				{
					switch_to_cryptMode(PRIVATE_KEYSET1);
				}
				if (pCtxt->CTXT_FLAG & CTXT_FLAG_XTS)
				{
					*aes_Keyset1_Dec_LBA0 = CmdBlk(9) + (CmdBlk(8) << 8) + (CmdBlk(7) << 16) + (CmdBlk(6) << 24);
					*aes_Keyset1_Dec_LBA1 = CmdBlk(5) + (CmdBlk(4) << 8);
				}
				*aes_control |= AES_DECRYP_EN;	// enable decryption
			}
			else
			{
				if (current_aes_key2 != AES_ENCRYPT_MODE)
				{
					switch_to_cryptMode(PRIVATE_KEYSET2);
				}
				if (pCtxt->CTXT_FLAG & CTXT_FLAG_XTS)
				{
					*aes_Keyset2_Dec_LBA0 = CmdBlk(9) + (CmdBlk(8) << 8) + (CmdBlk(7) << 16) + (CmdBlk(6) << 24);
					*aes_Keyset2_Dec_LBA1 = CmdBlk(5) + (CmdBlk(4) << 8);
				}
				*aes_control |= (AES_DECRYP_EN | AES_DECRYP_SEL_KEYSET2);	// enable decryption
			}
		}
#endif
		keep_sourceDbufConnection(segIndex);	
		if ((pCtxt->CTXT_FLAG & (CTXT_FLAG_B2S|CTXT_FLAG_U2B)) == 0) 
		{
//			if (curMscMode == MODE_BOT)
			{
				rw_flag = READ_FLAG;
				if (usbMode == CONNECT_USB3)
				{
					rw_flag |= RW_IN_PROCESSING;
					if (curMscMode == MODE_UAS)
						rw_time_out = 80;
					else
						rw_time_out = 50;
				}
			}
		}
	}

	// sets up the USB Transfer by loading the MSCn TX Context Register
	DBG("usb_Msc0DICtrl %bx\n", *usb_Msc0DICtrl);
#ifdef FAST_CONTEXT
	*usb_Msc0TxCtxt = (pCtxt->CTXT_Index & 0x3F) | pCtxt->CTXT_No_Data;
#else
	*usb_Msc0TxCtxt = MSC_TX_CTXT_VALID|(*((u16 *)&(pCtxt->CTXT_Index)));
#endif

	logicalUnit = pCtxt->CTXT_LUN;
#if 1
	if (dbg_next_cmd)
	{
		MSG("*Msc0TxCtxt %x\n", *usb_Msc0TxCtxt);	
	}
#endif
//	if (curMscMode == MODE_UAS)
//		MSG("UC%bx %bx\n", pCtxt->CTXT_LUN,pCtxt->CTXT_Index);

	// current Active CDB context for USB D-In xfer 
	usb_ctxt.curCtxt = pCtxt;
	if (pCtxt->CTXT_XFRLENGTH == 0)
	{
//		dbg_next_cmd = 1;
		*usb_Msc0Residue = 0;
		usb_ctxt_send_status(pCtxt);
	}
}

////////////////////////////////////////////////////////////////
void usb_exec_que_ctxt(void)
{
	DBG("usb exec tx ctxt que:\n");
	DBG("u exQ\n");	
	if (usb_ctxt.curCtxt == NULL)
	{// there's no active USB command, USB bus is ready to be used
		PCDB_CTXT pCtxt;
#ifdef UAS_READ_PREFECTH
		if (prefetched_rdCmd_ctxt)
		{
			pCtxt = prefetched_rdCmd_ctxt;
			prefetched_rdCmd_ctxt = NULL;
		}
		else
		{
			pCtxt = usb_ctxt.ctxt_que;
		}
#else
		pCtxt = usb_ctxt.ctxt_que;
#endif
		
		while (pCtxt)
		{
			if (dbg_next_cmd)
				MSG("Ct st %bx,c %bx, t %bx\n", pCtxt->CTXT_usb_state, pCtxt->CTXT_CDB[0], (u8)pCtxt->CTXT_ITAG);
			if (pCtxt->CTXT_usb_state > CTXT_USB_STATE_AWAIT_SATA_DMASETUP)
			{
				if (pCtxt->CTXT_CONTROL & CTXT_CTRL_DIR)
					usb_exec_tx_ctxt(pCtxt);
				else
					usb_exec_rx_ctxt(pCtxt);
				break;
			}
			pCtxt = pCtxt->CTXT_Next;
		}

		return;		
	}
	else
	{
		if (dbg_next_cmd)
			MSG("cur c %bx, s %bx, t %bx\n", usb_ctxt.curCtxt->CTXT_CDB[0], usb_ctxt.curCtxt->CTXT_Index, (u8)(usb_ctxt.curCtxt->CTXT_ITAG));
	}
}

void usb_que_ctxt(PCDB_CTXT pCtxt)
{
	pCtxt->CTXT_Next = NULL;
	PCDB_CTXT pQCtxt = usb_ctxt.ctxt_que;
	// pending que for USB TX Contxt
	if (pQCtxt == NULL)
	{
		 usb_ctxt.ctxt_que = pCtxt;
	}
	else
	{
		usb_ctxt.ctxt_que_tail->CTXT_Next = pCtxt;
	}
	usb_ctxt.ctxt_que_tail = pCtxt;
}

u8 usb_deQue_ctxt(PCDB_CTXT pCtxt)
{
	PCDB_CTXT pCtxt_Cur = usb_ctxt.ctxt_que;
	PCDB_CTXT pCtxt_Prev = NULL;
	if (pCtxt_Cur == NULL) // Que is empty
	{
		MSG("Q Em\n");
		return 1;
	}
	
	// find the pCtxt position in USB Ctxt Que
	while (pCtxt_Cur != pCtxt)
	{
		pCtxt_Prev = pCtxt_Cur;
		pCtxt_Cur = pCtxt_Cur->CTXT_Next;
		// pCtxt is not in CtxtQue, it shall not happen, because the Que is managemed by firmware, if it's happens, it's a FW bug
		if (pCtxt_Cur == NULL)  
		{
			MSG("NT In Q\n");			
			return 1;
		}
	}
	
	if (usb_ctxt.ctxt_que_tail == pCtxt)
	{// if the deleted Ctxt is the tail, the new tail should be the previous CTXT node
	 // otherwise, the tail should be kept
		usb_ctxt.ctxt_que_tail = pCtxt_Prev;
	}
	
	if (pCtxt_Prev == NULL) // first node
	{
		usb_ctxt.ctxt_que = usb_ctxt.ctxt_que->CTXT_Next;
	}
	else
	{
		pCtxt_Prev->CTXT_Next = pCtxt_Cur->CTXT_Next;
		pCtxt = NULL;
	}
	return 0;
}

void usb_ctxt_send_status(PCDB_CTXT  pCtxt)
{
	if (dbg_next_cmd)
		MSG("S ST %bx, t %bx\n", pCtxt->CTXT_Status, (u8)pCtxt->CTXT_ITAG);
#ifdef UAS
	if (curMscMode == MODE_UAS)
	{
		pCtxt->CTXT_CONTROL |= CTXT_CTRL_SEND_SIU_DONE;
		uas_send_Sense_IU(pCtxt);
	}
	else
#endif
	{
		set_csw_run(pCtxt);
	}	
}


void usb_exec_rx_ctxt(PCDB_CTXT pCtxt)
{
	u8	segIndex;
	if (dbg_next_cmd)
		MSG("dbufIdx %bx, p-case %x\n", pCtxt->CTXT_DbufIndex, pCtxt->CTXT_PHASE_CASE);
	if ((segIndex = pCtxt->CTXT_DbufIndex) != SEG_NULL)
	{
#ifdef AES_EN
		if (pCtxt->CTXT_FLAG & CTXT_FLAG_AES)
		{
			if (pCtxt->CTXT_LUN == HDD0_LUN)
			{
				if (current_aes_key != AES_ENCRYPT_MODE)
				{
					switch_to_cryptMode(PRIVATE_KEYSET1);
				}
				if (pCtxt->CTXT_FLAG & CTXT_FLAG_XTS)
				{
					*aes_Keyset1_Enc_LBA0 = CmdBlk(9) + (CmdBlk(8) << 8) + (CmdBlk(7) << 16) + (CmdBlk(6) << 24);
					*aes_Keyset1_Enc_LBA1 = CmdBlk(5) + (CmdBlk(4) << 8);
				}
				*aes_control |= AES_ENCRYP_EN;
			}
			else if (pCtxt->CTXT_LUN == HDD1_LUN)
			{
				if (current_aes_key2 != AES_ENCRYPT_MODE)
				{
					switch_to_cryptMode(PRIVATE_KEYSET2);
				}
				if (pCtxt->CTXT_FLAG & CTXT_FLAG_XTS)
				{
					*aes_Keyset2_Enc_LBA0 = CmdBlk(9) + (CmdBlk(8) << 8) + (CmdBlk(7) << 16) + (CmdBlk(6) << 24);
					*aes_Keyset2_Enc_LBA1 = CmdBlk(5) + (CmdBlk(4) << 8);
				}
				*aes_control |= (AES_ENCRYP_EN | AES_ENCRYP_SEL_KEYSET2);
			}	// enable Encryption
		}
#endif
		set_dbufConnection(segIndex);
		
		if ((pCtxt->CTXT_FLAG & (CTXT_FLAG_B2S|CTXT_FLAG_U2B)) == 0)
		{
			rw_flag = WRITE_FLAG;
			if (usbMode == CONNECT_USB3)
			{
				rw_flag |= RW_IN_PROCESSING;
				rw_time_out = 50;
			}
		}
	}
//	dbuf_dbg(0, segIndex);
//	dbuf_dbg(0, pCtxt->CTXT_AES_DbufIndex);
//	MSG("usb int %lx", *usb_Msc0IntStatus);
	DBG("do ctrl %bx\n", *usb_Msc0DOutCtrl);
	
	// sets up the USB Transfer by loading the MSCn RX Context Register
#ifdef FAST_CONTEXT
	*usb_Msc0RxCtxt = (pCtxt->CTXT_Index & 0x3F) | pCtxt->CTXT_No_Data;
#else
	*usb_Msc0RxCtxt = MSC_RX_CTXT_VALID|(*((u16 *)&(pCtxt->CTXT_Index)));
#endif
	logicalUnit = pCtxt->CTXT_LUN;

	if (dbg_next_cmd)
		MSG("set RxCtxt: %X\n", *usb_Msc0RxCtxt);

	// current Active CDB context for USB D-Out xfer 
	usb_ctxt.curCtxt = pCtxt;	
}


/////////////////////////////////////////////////////
void usb_data_out_done()
{
DBG("u dout done\n");
	usb_ctxt.post_dout = 1;
}

/****************************************\
   Dn
\****************************************/
void usb_device_no_data(PCDB_CTXT pCtxt)
{
	DBG("no data\n");
	pCtxt->CTXT_DbufIndex = SEG_NULL;
	pCtxt->CTXT_FLAG = CTXT_FLAG_U2B;
#ifdef UAS
	if(curMscMode == MODE_UAS)
		uas_device_no_data(pCtxt);
	else
#endif
		bot_device_no_data(pCtxt);
}

/****************************************\
   Di or Dn
\****************************************/
void usb_device_data_in(PCDB_CTXT pCtxt, u32 byteCnt)
{
	DBG("data in %LX\n", byteCnt);
#ifdef UAS
	if(curMscMode == MODE_UAS)
		uas_device_data_in(pCtxt, byteCnt);
	else
#endif
		bot_device_data_in(pCtxt, byteCnt);

}


/****************************************\
   Do or Dn
\****************************************/
void usb_device_data_out(PCDB_CTXT pCtxt, u32 byteCnt)
{
	raid1_active_timer = RAID_REBUILD_VERIFY_ONLINE_ACTIVE_TIMER;
	if (byteCnt > sizeof(mc_buffer))
	{// fails the command which will transfer data greater than 4K
		hdd_err_2_sense_data(pCtxt, ERROR_ILL_CDB);
		usb_device_no_data(pCtxt);
		return;
	}
#ifdef UAS
	if(curMscMode == MODE_UAS)
		uas_device_data_out(pCtxt, byteCnt);
	else
#endif
		bot_device_data_out(pCtxt, byteCnt);
}



#if 0
void usb_set_halt_if_dout_flow_ctrl()
{
	PCDB_CTXT pCtxt;
	
	if ((*usb_Msc0DOutCtrl & 0xE1) == 0xA0)
	{
		if ((pCtxt = usb_rx_ctxt.rx_curCtxt))
		{
			if (pCtxt->CTXT_Status != CTXT_STATUS_HALT)
			{
				pCtxt->CTXT_Status = CTXT_STATUS_HALT;
				*usb_Msc0DOutCtrl = MSC_DOUT_HALT;
			}
		}
	}
}
#endif

// create the function to save code size, it will be used both in BOT.c & UAS.c
u8 usb_QEnum(void)
{
	PCDB_CTXT pCtxt = NULL;
	if (((sata0Obj.sobj_State >= SATA_READY) || (sata0Obj.sobj_device_state == SATA_NO_DEV))
	&& ((sata1Obj.sobj_State >= SATA_READY) || (sata1Obj.sobj_device_state == SATA_NO_DEV)))
	{
		u8 val = ataInit_quick_enum();
		MSG("QE %bx, bt %bx\n", val, unsupport_boot_up);
		if (val == QUICK_ENUM_STATUS_ATA_TIMEOUT)
			goto _CHK_CTXT_FIFO;						//quick_enum = 0;
		DBG("quick_END1\n");	
		//dbg_next_cmd = 1;
		pCtxt = usb_ctxt.newCtxt;
		if (pCtxt->CTXT_usb_state == CTXT_USB_STATE_AWAIT_SATA_READY)
		{// the usb state shall be await SATA READY or RECEIVE_CIU
			pCtxt->CTXT_usb_state = CTXT_USB_STATE_RECEIVE_CIU;
		}
		return 0;
	}
	else
	{
_CHK_CTXT_FIFO:
		pCtxt = usb_ctxt.newCtxt;
		pCtxt->CTXT_ccmIndex = CCM_NULL;
		*((u16 *)(&(pCtxt->CTXT_Status))) = (LUN_STATUS_GOOD << 8)|(CTXT_STATUS_PENDING);
		pCtxt->CTXT_DbufIndex = SEG_NULL;
		pCtxt->CTXT_usb_state = CTXT_USB_STATE_CTXT_SETUP_DONE;
		
		if ((CmdBlk(0) == SCSI_INQUIRY) || (CmdBlk(0) == SCSI_REQUEST_SENSE) ||(CmdBlk(0) == SCSI_REPORT_LUNS))
		{
			return 0;
		}

		if ((sata0Obj.sobj_device_state == SATA_NO_DEV)
		|| (sata1Obj.sobj_device_state == SATA_NO_DEV))
		{
			DBG("n-HD\n");
			if (usb_ctxt.newCtxt != NULL)
			{
				pCtxt = usb_ctxt.newCtxt;		
				if (sata0Obj.sobj_device_state == SATA_NO_DEV)
				{
					sata0Obj.sobj_init = 1;
					unsupport_boot_up = UNSUPPORT_NO_HDD0_ATTACHED;
				}
				
				if (sata1Obj.sobj_device_state == SATA_NO_DEV)
				{
					sata1Obj.sobj_init = 1;
					unsupport_boot_up |= UNSUPPORT_NO_HDD1_ATTACHED;
				}
				
				chk_fis34_timer = 0;
				product_model = ILLEGAL_BOOT_UP_PRODUCT;
_INTERNAL_ERROR:
#ifdef WDC_CHECK
				if (check_IILegal_boot(pCtxt) == 1)
					return 1;
				else
#endif
					return 0;
			}
		}							

		if (CmdBlk(0) == SCSI_TEST_UNIT_READY)
		{
			return 0;
		}

		if ((CmdBlk(0) == SCSI_START_STOP_UNIT) && (CmdBlk(1) & 0x01)) // immediate bit is setted
		{
			return 0;
		}
		
		pCtxt->CTXT_usb_state = CTXT_USB_STATE_AWAIT_SATA_READY;		
		return 1;
	}
}

#define TX_SIU_DONE	1
#define RX_SIU_DONE	0
static void usb_uas_SIU_DONE_isr(u8 TX_RX_FLAG)
{
#ifdef DBG_PERFORMANCE
	*gpioDataOut = *gpioDataOut & (~(GPIO10_PIN|GPIO11_PIN)) | (GPIO8_PIN|GPIO9_PIN);
#endif
	PCDB_CTXT pCtxt;
	if (TX_RX_FLAG == TX_SIU_DONE)
	{
		*usb_Msc0IntStatus = MSC_TXSENSE_DONE_INT;
	}
	else
	{
		*usb_Msc0IntStatus = MSC_RXSENSE_DONE_INT;
	}
	*usb_Msc0StatCtrlClr = MSC_STAT_FLOW_CTRL;

	pCtxt = usb_ctxt.curCtxt;
	usb_ctxt.curCtxt = NULL;
	rw_flag = 0;

#if WDC_ENABLE_QUICK_ENUMERATION
	if (TX_RX_FLAG == TX_SIU_DONE)
	{
		chk_power_Off();
		if ((re_enum_flag == DOUBLE_ENUM) || (re_enum_flag == FORCE_OUT_RE_ENUM))
		{
			DBG("re-enum2\n");

			// re-enumration	
			Delay(250);
			if (re_enum_flag == DOUBLE_ENUM)
			{
				usb_reenum();
				re_enum_flag = 0;
			}
			return;
		}
	}
#endif

	// check pending Ctxt queue for TX(DI) Context ????
	usb_exec_que_ctxt();
#ifdef UAS
	if ((uas_ci_paused) && (pCtxt))
	{
		if (pCtxt == uas_ci_paused_ctxt)
		{
			uas_ci_paused &= ~UAS_PAUSE_WAIT_FOR_USB_RESOURCE;
		}
	}
#endif
#ifdef DBG_PERFORMANCE
	*gpioDataOut = *gpioDataOut & (~GPIO10_PIN) | (GPIO8_PIN|GPIO9_PIN|GPIO11_PIN);
#endif
	return;
}

/****************************************\
	usb_msc_isr
\****************************************/
void usb_msc_isr()
{
	DBG("msc_isr\n");
	u32 Msc0Int = *usb_Msc0IntStatus;
	PCDB_CTXT pCtxt;

	if(Msc0Int & MSC_TX_DONE_INT)
	{	//  USB command with Data-in
		u8			segIndex;
#ifdef DBG_PERFORMANCE
		*gpioDataOut = *gpioDataOut & (~(GPIO9_PIN | GPIO11_PIN)) | (GPIO8_PIN|GPIO10_PIN);
#endif

		// if HALT asserted wait HALT clear...
		if (Msc0Int & MSC_DIN_HALT_INT)
		{
			if ((*usb_Msc0DICtrl & MSC_DI_HALT))
				goto msc_isr_check_rx;
		}

		if (pCtxt = usb_ctxt.curCtxt)
		{
			DBG("tx Ctxt exists\n");

			if (pCtxt->CTXT_ccmIndex != CCM_NULL) // it's setted only when the sata tx resource is required
			{
				DBG("msoi %lx ", Msc0Int);
				pCtxt->CTXT_Status = CTXT_STATUS_XFER_DONE; // the usb tx is done
				if ((Msc0Int & ~MSC_TX_DONE_INT) == 0)
					return;
				else
					goto msc_isr_check_rx; 
			}
			DBG("u tx dn\n");
			*usb_Msc0IntStatus = MSC_TX_DONE_INT|MSC_DIN_HALT_INT;
			
			if (curMscMode == MODE_BOT)
				*usb_Msc0Residue = *usb_Msc0TxXferCount;			// for BOT only			
			
			if((segIndex = pCtxt->CTXT_DbufIndex) != SEG_NULL)
			{	
				DBG("clr tx, FLAG %bx, segIdx %bx\n", pCtxt->CTXT_FLAG, segIndex);
				reset_dbuf_seg(segIndex);
				
				usb_tx_fifo_rst();

				DBG("*usb_Msc0DICtrl %bx\n", *usb_Msc0DICtrl);		
				
				free_dbufConnection(segIndex);
#ifdef AES_EN
				if (pCtxt->CTXT_FLAG & CTXT_FLAG_AES)
				{
					DBG("dis AES dec\n");
					*aes_control &= ~(AES_DECRYP_EN|AES_DECRYP_KEYSET_SEL);
				}
#endif
				if (pCtxt->CTXT_FLAG & CTXT_FLAG_RAID_EN)
				{
					DBG("rst utx\n");
					usb_tx_fifo_rst();
					*raid_read_ctrl = RAID_RD_RESET;
					*raid_read_ata_xfer_len = 0;
					*raid_read_ata_lba_low = 0;
					*raid_read_ata_lba_high = 0;
				}
			}

//			if ((*usb_Msc0DICtrl & MSC_DI_HALT) == 0)
			{
				DBG("no halt\n");
#ifndef CSW_0_LENGTH
				if (rw_flag & READ_FLAG)
				{
					if (((*usb_Msc0_FIFO_CNT & 0xFFFF) != 0) || (*usb_Msc0TxXferCount != 0) || (rw_flag & LINK_RETRY_FLAG))
					{
						if (rw_flag & LINK_RETRY_FLAG)
						{
							r_recover_counter++;
							MSG("R recvry %x\n", r_recover_counter);
						}
						else
						{
							if ((*usb_Msc0_FIFO_CNT & 0xFFFF) != 0)
							{
								u16 temp16 = *usb_Msc0_FIFO_CNT & 0xFFFF;
								MSG("U txFifo %x\n", temp16);
							}
							else 
							{
								u32 temp32 = *usb_Msc0TxXferCount;
								MSG("ResLen %lx\n", temp32);
							}	
						}
//										hdd_err_2_sense_data(pCtxt, ERROR_HW_FAIL_INTERNAL);
					}
				}
#endif

#ifdef UAS				
				if((pCtxt->CTXT_Prot & 0x0C) == 0x08)
				{	// // send status Back if it is NCQ Protocol
					// prefetch the next Ctxt, and prepared for the data transfer
#ifdef UAS_READ_PREFECTH
					if (prefetched_rdCmd_ctxt == NULL)
					{
						PCDB_CTXT prefetch_ctxt = pCtxt->CTXT_Next;
						if (prefetch_ctxt)
						{
							//						
							if (prefetch_ctxt->CTXT_Prot == PROT_FPDMAIN)
							{
								if (prefetch_ctxt->CTXT_usb_state > CTXT_USB_STATE_AWAIT_SATA_DMASETUP)
								{
									set_dbuf_readPrefetch(prefetch_ctxt->CTXT_DbufIndex);
									prefetched_rdCmd_ctxt = prefetch_ctxt;
								}
							}
						}
					}
#endif
					DBG("sens IU , site: %BX\n", pCtxt->CTXT_Index);
					if (dbg_next_cmd)
					{
						MSG("u sd st %bx\n", pCtxt->CTXT_CONTROL);
					}
					if ((pCtxt->CTXT_FLAG & CTXT_FLAG_U2B)
					// SetDevbit or next DMASetup has arrived, 
					// and firmware set this flag to indicate that the SIU can be sent out
					|| (pCtxt->CTXT_CONTROL & CTXT_CTRL_SEND_SIU))
					{
						usb_ctxt_send_status(pCtxt);
					}
					else
					{
						pCtxt->CTXT_CONTROL |= CTXT_CTRL_SEND_SIU;
					}
				}
				else
#endif
				{
//						if (pCtxt->CTXT_Status != CTXT_STATUS_PENDING)
//						{
//							if (reg_r32(chip_IntStaus) & SATA0_INT)
//								sata_isr(&sata0Obj);
//						}
					DBG("status response %BX\n", pCtxt->CTXT_Status);
					if (pCtxt->CTXT_Status != CTXT_STATUS_PENDING)
					{

#ifdef UAS
						if (curMscMode == MODE_UAS)
						{
							usb_ctxt_send_status(pCtxt);
						}
						else
#endif
						{	// send csw
							DBG("send csw\n");
							set_csw_run(pCtxt);
						}
					}
				}
				*usb_Msc0IntStatus = MSC_DIN_HALT_INT;
			}
		}
		else
		{
			DBG("usb_tx_ctxt.tx_curCtxt is NULL\n");
		}
		return;
	}	// if(Msc0Int & MSC_TX_DONE_INT)

	else if (Msc0Int & MSC_DIN_HALT_INT)
	{
		if (dbg_next_cmd)
			MSG("ht in %bx\n", *usb_Msc0DICtrl);

		if (*usb_Msc0DICtrl & MSC_DI_HALT)
			goto msc_isr_check_rx;

		*usb_Msc0IntStatus = MSC_DIN_HALT_INT;
		if ((pCtxt = usb_ctxt.curCtxt))
		{
			if (curMscMode == MODE_BOT)
			{
				*usb_Msc0Residue = *usb_Msc0TxXferCount;			
				if (pCtxt->CTXT_No_Data & (MSC_TX_CTXT_NODATA >> 8))
				{
					if (dbg_next_cmd)
						MSG("ctxt sta %bx\n", pCtxt->CTXT_Status);
					
					if (pCtxt->CTXT_Status != CTXT_STATUS_PENDING)
					{
						usb_ctxt_send_status(pCtxt);
					}
				}
			}
		}
	}

	else if (Msc0Int & MSC_TXSENSE_DONE_INT) 
	{	// 	UAS Sense IU Transfer thru TX(DI) context  has completed.
		usb_uas_SIU_DONE_isr(TX_SIU_DONE);
		return;
	}


msc_isr_check_rx:
	// check RX context of MSC0
	if(Msc0Int & MSC_RX_DONE_INT)
	{	// USB Data Out done on active CDB Context of RX Context
		if (dbg_next_cmd)
		{
			MSG("u rx dn %bx, s %bx\n", (u8)Msc0Int, *usb_Msc0DOXferStatus);
			MSG("r1 %bx\n", raid_xfer_status);
		}
#ifdef DBG_PERFORMANCE
		*gpioDataOut = *gpioDataOut & (~(GPIO9_PIN)) | (GPIO8_PIN|GPIO10_PIN|GPIO11_PIN);
#endif

		if ((pCtxt = usb_ctxt.curCtxt))
		{			
			u8 do_crl = *usb_Msc0DOutCtrl;
			if (dbg_next_cmd)
			{
				MSG("ct %bx, %bx %bx\n", pCtxt->CTXT_Status, do_crl, pCtxt ->CTXT_ccmIndex);
			}

			if (pCtxt->CTXT_Status == CTXT_STATUS_PENDING)
			{
				if ((do_crl & (MSC_DOUT_FLOW_CTRL|MSC_RXFIFO_DDONE|MSC_DOUT_HALT)) == (MSC_DOUT_FLOW_CTRL|MSC_RXFIFO_DDONE))
				{
					if (*usb_Msc0RxXferCount)
					{	// set Halt bit & 3607 will send ERDY 
						*usb_Msc0DOutCtrl = MSC_DOUT_HALT;
						pCtxt->CTXT_Status = CTXT_STATUS_HALT;
						// wait for Halt bit clear by system
						return;
					}
				}
				else if (do_crl & (MSC_DOUT_HALT))
				{
					pCtxt->CTXT_Status = CTXT_STATUS_HALT;
					// wait for Halt bit clear by system
					return;
				}
//				if (do_crl & MSC_RXFIFO_EMPTY)
//				{
					goto rx_done_ok;
//				}
//				return;

			}
			else if (pCtxt->CTXT_Status == CTXT_STATUS_HALT)
			{
//				u32 segIndex;

				if (do_crl & MSC_DOUT_HALT)
				{
					if (*usb_Msc0DICtrl & MSC_DIN_FLOW_CTRL)
					{
						*usb_Msc0DOutCtrlClr = MSC_DOUT_HALT;
					}
					return;
				}

rx_done_ok:
				*usb_Msc0IntStatus = MSC_RX_DONE_INT|MSC_DOUT_HALT_INT;


				{
					if (pCtxt->CTXT_FLAG & CTXT_FLAG_U2B)
					{
						if (pCtxt->CTXT_Status >= CTXT_STATUS_HALT)
						{
							if (pCtxt->CTXT_PHASE_CASE & 0x0800)		// Case 11
							{
								pCtxt->CTXT_Status = CTXT_STATUS_PHASE;
							}
							pCtxt->CTXT_Status = CTXT_STATUS_XFER_DONE;
							usb_data_out_done();
						}
						else
						{
							if (dbg_next_cmd)
								MSG("csw r %bx\n", pCtxt->CTXT_Status);
							usb_ctxt_send_status(pCtxt);
						}
					}
					else
					{	// D-out from USB to SATA

						if (pCtxt ->CTXT_ccmIndex != CCM_NULL)
						{
							pCtxt->CTXT_Status = CTXT_STATUS_XFER_DONE;
						}
						else
						{
							if (pCtxt->CTXT_Status <= CTXT_STATUS_PHASE)
							{
								// if it is USB-2-SATA command,  send status Back thru RX context
								if (dbg_next_cmd)
									MSG("rx st\n");
								usb_ctxt_send_status(pCtxt);
							}
							else
							{
								ERR("USB Rx done CTXT_Status:%bx\n", pCtxt->CTXT_Status);
							}
						}
					}
//					*usb_Msc0IntStatus = MSC_DOUT_HALT_INT;
				}
			}
			else if (rw_flag & RD_WR_TIME_OUT)
			{			
				goto rx_done_ok;
			}
			else
			{
				MSG("ct status %bx\n", pCtxt->CTXT_Status);
				*usb_Msc0IntStatus = MSC_RX_DONE_INT|MSC_DOUT_HALT_INT;
			}
		}
		else
		{	// something wrong ????
			ERR("something wrong\n");
			*usb_Msc0IntStatus = MSC_RX_DONE_INT|MSC_DOUT_HALT_INT;
		}
		return;
	}

	// no halt in UAS		
	else if(Msc0Int & MSC_DOUT_HALT_INT)		
	{
		MSG("o\n");

		if ((*usb_Msc0DOutCtrl & MSC_DOUT_HALT))
		{
			return;
		}

		*usb_Msc0IntStatus = MSC_DOUT_HALT_INT;	
		if ((pCtxt = usb_ctxt.curCtxt))
		{
			if (curMscMode == MODE_BOT)
			{
				//*usb_Msc0Residue = *usb_Msc0RxXferCount;
				*usb_Msc0Residue = pCtxt->CTXT_XFRLENGTH;
			}
			else
			{
				return;
			}
			
			if (pCtxt->CTXT_FLAG & CTXT_FLAG_U2B)
			{
				// is USB-2-CPU command(not USB-2-SATA command)
				// process data from USB host for SCSI CDB. ????
//						pCtxt->CTXT_Status = CTXT_STATUS_XFER_DONE;
#ifdef FAST_CONTEXT
				if (pCtxt->CTXT_No_Data & MSC_RX_CTXT_NODATA)
#else
				if (pCtxt->CTXT_No_Data & (MSC_RX_CTXT_NODATA >> 8))
#endif
				{
					if (pCtxt->CTXT_PHASE_CASE & (BIT_13 | BIT_10))
					{	//case 10 
						pCtxt->CTXT_Status = CTXT_STATUS_PHASE;
					}
					else
					{
						if (pCtxt->CTXT_Status > CTXT_STATUS_PHASE) // if the status is already setted by the F/W, F/W should send out the right status
							pCtxt->CTXT_Status = CTXT_STATUS_GOOD;
					}
					usb_ctxt_send_status(pCtxt);
				}
				else
				{
					// it is USB-2-CPU command(not USB-2-SATA command)
					// process data from USB host for SCSI CDB. ????
					usb_data_out_done();
				}
			}
			else
			{

				if (pCtxt ->CTXT_ccmIndex != CCM_NULL)
				{
					pCtxt->CTXT_Status = CTXT_STATUS_XFER_DONE;
				}
				else
				{
					if (pCtxt->CTXT_Status <= CTXT_STATUS_PHASE)
					{
						usb_ctxt_send_status(pCtxt);
					}
					else
					{
						MSG("U Rx dn:%bx\n", pCtxt->CTXT_Status);

					}
				}
			}
		}
		return;
	}	//	if(Msc0Int & MSC_DOUT_HALT_INT)		//
#ifdef UAS
	else if (Msc0Int & MSC_RXSENSE_DONE_INT)
	{
		DBG("rx sense done, site: %BX\n", usb_ctxt.curCtxt->CTXT_Index);
		usb_uas_SIU_DONE_isr(RX_SIU_DONE);
	}
	else if (Msc0Int & MSC_RESP_DONE_INT) 
	{
		MSG("resp dn\n");
		*usb_Msc0IntStatus = MSC_RESP_DONE_INT;
		if (taskCtxtList.curTaskCtxt)
		{
			MSG("Tiu t %bx, flg %bx, func %bx, osite %bx, s %bx\n", (u8)taskCtxtList.curTaskCtxt->TASK_CTXT_ITAG,taskCtxtList.curTaskCtxt->TASK_CTXT_FLAG, taskCtxtList.curTaskCtxt->TASK_CTXT_FUNCTION, taskCtxtList.curTaskCtxt->TASK_CTXT_OSITE, taskCtxtList.curTaskCtxt->TASK_CTXT_SITE);
			*usb_Msc0TaskCtrl = taskCtxtList.curTaskCtxt->TASK_CTXT_SITE; 
			MSG("TCtxt %lx %lx\n", (u32)((*usb_MSC0_Task_Context) >> 32), (u32)*usb_MSC0_Task_Context);
//			*usb_Msc0TaskFreeFIFO = taskCtxtList.curTaskCtxt->TASK_CTXT_SITE;
//			MSG("TCtxt %lx %lx\n", (u32)((*usb_MSC0_Task_Context) >> 32), (u32)*usb_MSC0_Task_Context);
			taskCtxtList.curTaskCtxt = NULL;
		}
		else
			uas_RIU_active = 0;
		uas_exec_TIU_Que();
	}	
#endif
	else if (Msc0Int & MSC_CONFIG_INT)
	{
		*usb_Msc0IntStatus = MSC_CONFIG_INT;
	}
	DBG("U Int %lx\n", Msc0Int);
	return;
}
