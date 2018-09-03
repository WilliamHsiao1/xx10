/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *
 *
 *****************************************************************************/
#ifndef DBUF_C
#define 	DBUF_C
#include	"general.h"

/****************************************\
	dbuf_dbg
\****************************************/
#ifdef DBG_FUNCTION//def DEBUG
void chk_rxdbuf_port(u8 port_num, DBUF_REG volatile * buf_ptr)
{
	switch (port_num)
	{
		case RX_DBUF_CPU_W_PORT:
			MSG("CPW ");
			break;
		case RX_DBUF_CPU_R_PORT:
			MSG("CPR ");
			break;
		case RX_DBUF_USB_W_PORT:
			MSG("USW ");
			break;
		case RX_DBUF_SATA0_R_PORT:
			MSG("S0R ");
			break;
		case RX_DBUF_SATA1_R_PORT:
			MSG("S1R ");
			break;
		// reserve port 6,7
		case RX_DBUF_WR_VRY_R_PORT:
			MSG("WRVR ");
			break;
		case RX_DBUF_DD_W_PORT:
			MSG("D2DW ");
			break;
		case RX_DBUF_AESD_R_PORT:
			MSG("ADR ");
			break;
		case RX_DBUF_AESE_W_PORT:
			MSG("AEW ");
			break;
		case RX_DBUF_RAID_R_PORT:
			MSG("RDR ");
			break;
		case RX_DBUF_STRIPE0_W_PORT:
			MSG("SP0W ");
			break;
		case RX_DBUF_STRIPE1_W_PORT:
			MSG("SP1W ");
			break;
		default: return;
	}
	MSG("PCnt %Lx, Addr %LX\n", buf_ptr->dbuf_Port[port_num].dbuf_Port_Count, buf_ptr->dbuf_Port[port_num].dbuf_Port_Addr);
}

void chk_txdbuf_port(u8 port_num, DBUF_REG volatile * buf_ptr)
{
	switch (port_num)
	{
		case TX_DBUF_CPU_W_PORT:
			MSG("CPW ");
			break;
		case TX_DBUF_CPU_R_PORT:
			MSG("CPR ");
			break;
		case TX_DBUF_USB_R_PORT:
			MSG("USR ");
			break;
		case TX_DBUF_SATA0_W_PORT:
			MSG("S0W ");
			break;
		case TX_DBUF_SATA1_W_PORT:
			MSG("S1W ");
			break;
		// reserve port 6,7
		case TX_DBUF_WR_VRY_W_PORT:
			MSG("WRVW ");
			break;
		case TX_DBUF_DD_R_PORT:
			MSG("D2DR ");
			break;
		case TX_DBUF_AESE_R_PORT:
			MSG("AER ");
			break;
		case TX_DBUF_AESD_W_PORT:
			MSG("ADW ");
			break;
		case TX_DBUF_RAID_W_PORT:
			MSG("RDW ");
			break;
		case TX_DBUF_STRIPE0_R_PORT:
			MSG("SP0R ");
			break;
		case TX_DBUF_STRIPE1_R_PORT:
			MSG("SP1R ");
			break;
		default: return;
	}
	MSG("PCnt %Lx, Addr %LX\n", buf_ptr->dbuf_Port[port_num].dbuf_Port_Count, buf_ptr->dbuf_Port[port_num].dbuf_Port_Addr);
}
void dbuf_seg_dbg(u8 dbuf_seg, u8 tx_rx_dir)
{
	DBUF_REG volatile	*buf_ptr;	
	u16 dbufSeg_InOut;
	u8 port_id;
	if (tx_rx_dir == DIR_RX)
	{
		buf_ptr = Rx_Dbuf;
		MSG("RX_DBUF[%bx] SEG_CTRL %lx\n", dbuf_seg, *((u32 *)(&buf_ptr->dbuf_Seg[dbuf_seg].dbuf_Seg_INOUT)));
		dbufSeg_InOut = buf_ptr->dbuf_Seg[dbuf_seg].dbuf_Seg_INOUT;
		port_id = (u8)(dbufSeg_InOut & 0x000F);
		if (port_id != RX_DBUF_NULL)
			chk_rxdbuf_port(port_id, buf_ptr);
		port_id = (u8)(dbufSeg_InOut>>4);
		if (port_id != RX_DBUF_NULL)
			chk_rxdbuf_port(port_id, buf_ptr);
		port_id = (u8)(dbufSeg_InOut>>8);
		if (port_id != RX_DBUF_NULL)
			chk_rxdbuf_port(port_id, buf_ptr);
	}
	else
	{
		buf_ptr = Tx_Dbuf;	
		MSG("TX_DBUF[%bx] SEG_CTRL %lx\n", dbuf_seg, *((u32 *)(&buf_ptr->dbuf_Seg[dbuf_seg].dbuf_Seg_INOUT)));
		dbufSeg_InOut = buf_ptr->dbuf_Seg[dbuf_seg].dbuf_Seg_INOUT;
		port_id = (u8)(buf_ptr->dbuf_Seg[dbuf_seg].dbuf_Seg_INOUT & 0x0F);
		if (port_id != TX_DBUF_NULL)
			chk_txdbuf_port(port_id, buf_ptr);
		port_id = (u8)((buf_ptr->dbuf_Seg[dbuf_seg].dbuf_Seg_INOUT & 0xF0)>>4);
		if (port_id != TX_DBUF_NULL)
			chk_txdbuf_port(port_id, buf_ptr);
	}
}
void dbuf_dbg(u8 dbuf_index)
{
	u8	port_id;
	DBUF_REG volatile	*buf_ptr;	
	u8 tx_rx_dir = 0;
	u16 dbuf_seg_num = 0xFFFF;
	#define DBUF_SEGNUM_NULL	0xF
	
	switch (dbuf_index)
	{
		case DBUF_SEG_B2S0R:
			// local data in SATA command
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0xFFF4;
			break;			
			
		case DBUF_SEG_B2S0W:
			tx_rx_dir = DIR_RX;
			dbuf_seg_num = 0xFFF4;
			break;
			
		case DBUF_SEG_B2S0R_AES:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0xFF34;
			break;		
			
		case  DBUF_SEG_B2S0W_AES:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0xFF21;
			break;
			
		case DBUF_SEG_B2S1R:
			// local data in SATA command
			tx_rx_dir = DIR_RX;
			dbuf_seg_num = 0xFFF4;
			break;
			
		case DBUF_SEG_B2S1W:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0xFFF4;
			break;
			
		case DBUF_SEG_B2S1R_AES:
			tx_rx_dir = DIR_RX;
			dbuf_seg_num = 0xFF34;
			break;		
			
		case  DBUF_SEG_B2S1W_AES:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0xFF12;
			break;
			
		case DBUF_SEG_S2SR_VERIFY:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0xFF32;
			break;

		case DBUF_SEG_S2SR_REBUILD0:
		case DBUF_SEG_S2SR_REBUILD1:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0xFFF2;
			break;

		case DBUF_SEG_S2SW_REBUILD0:
		case DBUF_SEG_S2SW_REBUILD1:
			tx_rx_dir = DIR_RX;
#ifdef RAID_REBUILD_BY_CPU
			dbuf_seg_num = 0xFF34;
#else
			dbuf_seg_num = 0xFFF0;
#endif
			break;
			
		case DBUF_SEG_U2BR:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0xFFF0;
			break;
		case DBUF_SEG_U2BW:
			tx_rx_dir = DIR_RX;
			dbuf_seg_num = 0xFFF0;
			break;
		case DBUF_SEG_U2S0R:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0xFFF1;
			break;
			
		case DBUF_SEG_U2S0R_UAS2:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0xFFF2;
			break;			
			
		case DBUF_SEG_U2S1R:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0xFFF1;	
			break;

		case DBUF_SEG_U2S1R_UAS2:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0xFFF2;	
			break;
			
		case DBUF_SEG_U2S0W:
			tx_rx_dir = DIR_RX;
			dbuf_seg_num = 0xFFF1;
			break;

		case DBUF_SEG_U2S0W_UAS2:
			tx_rx_dir = DIR_RX;
			dbuf_seg_num = 0xFFF2;
			break;
			
		case DBUF_SEG_U2S1W:			
			tx_rx_dir = DIR_RX;
			dbuf_seg_num = 0xFFF1;	
			break;	

		case DBUF_SEG_U2S1W_UAS2:			
			tx_rx_dir = DIR_RX;
			dbuf_seg_num = 0xFFF2;	
			break;

		case DBUF_SEG_U2SR_RAID:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0xF243;
			break;
			
		case DBUF_SEG_U2SW_RAID:
			tx_rx_dir = DIR_RX;
			dbuf_seg_num = 0xF432;
			break;
			
		case DBUF_SEG_U2S0R_AES:
		case DBUF_SEG_U2S1R_AES:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0xFF21;				
			break;

		case DBUF_SEG_U2S0R_AES_UAS2:
		case DBUF_SEG_U2S1R_AES_UAS2:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0xFF23;				
			break;			
			
		case DBUF_SEG_U2S0W_AES:
		case DBUF_SEG_U2S1W_AES:
			tx_rx_dir = DIR_RX;
			dbuf_seg_num = 0xFF12;	
			break;

		case DBUF_SEG_U2S0W_AES_UAS2:
		case DBUF_SEG_U2S1W_AES_UAS2:
			tx_rx_dir = DIR_RX;
			dbuf_seg_num = 0xFF32;
			break;
			
		case DBUF_SEG_U2SR_AES_RAID:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0x1243;				
			break;
			
		case DBUF_SEG_U2SW_AES_RAID:
			tx_rx_dir = DIR_RX;
			dbuf_seg_num = 0x3421;
			break;
			
		case DBUF_SEG_DUMP_TXSEG1234:
			tx_rx_dir = DIR_TX;
			dbuf_seg_num = 0x4321;				
			break;

		case DBUF_SEG_DUMP_RXSEG1234:
			tx_rx_dir = DIR_RX;
			dbuf_seg_num = 0x4321;				
			break;
			
		default: break;		
	}

	for (u8 i = 0; i < 4; i++)
	{
		u8 dbuf_seg = (u8)(dbuf_seg_num & 0x000F);
		u16 dbufSeg_InOut; 
		if (dbuf_seg == DBUF_SEGNUM_NULL)
			break;
		else
		{
			if (tx_rx_dir == DIR_RX)
			{
				buf_ptr = Rx_Dbuf;
				MSG("RX_DBUF[%bx] SEG_CTRL %lx\n", dbuf_seg, *((u32 *)(&buf_ptr->dbuf_Seg[dbuf_seg].dbuf_Seg_INOUT)));
				dbufSeg_InOut = buf_ptr->dbuf_Seg[dbuf_seg].dbuf_Seg_INOUT;
				port_id = dbufSeg_InOut & 0x000F;
				if (port_id != RX_DBUF_NULL)
					chk_rxdbuf_port(port_id, buf_ptr);
				port_id = (dbufSeg_InOut>>4) & 0x000F;
				if (port_id != RX_DBUF_NULL)
					chk_rxdbuf_port(port_id, buf_ptr);
				port_id = (dbufSeg_InOut>>8) & 0x000F;
				if (port_id != RX_DBUF_NULL)
					chk_rxdbuf_port(port_id, buf_ptr);
			}
			else
			{
				buf_ptr = Tx_Dbuf;	
				MSG("TX_DBUF[%bx] SEG_CTRL %lx\n", dbuf_seg, *((u32 *)(&buf_ptr->dbuf_Seg[dbuf_seg].dbuf_Seg_INOUT)));
				dbufSeg_InOut = buf_ptr->dbuf_Seg[dbuf_seg].dbuf_Seg_INOUT;
				port_id = dbufSeg_InOut & 0x000F;
				if (port_id != TX_DBUF_NULL)
					chk_txdbuf_port(port_id, buf_ptr);
				port_id = (dbufSeg_InOut & 0x00F0)>>4;
				if (port_id != TX_DBUF_NULL)
					chk_txdbuf_port(port_id, buf_ptr);
			}
		}
		dbuf_seg_num >>= 4;
	}
}
#endif

/****************************************\
	dbuf_init
\****************************************/
void dbuf_init(void)
{
	// TX DBUF: 32 K
	Tx_Dbuf = tx_Dbuf_addr;
	Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_ADDR		= 0;			// TX Seg0 Base Address 
	Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_INOUT 	= (TX_DBUF_NULL << 8) | (TX_DBUF_NULL << 4) | TX_DBUF_NULL;
	Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_Size		= 4;			// 
	Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_Control	= SEG_RESET;	// empty TX Seg 0 

	Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_ADDR		= 1024*4;	// TX Seg1 Base Address 
	Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT 	= (TX_DBUF_NULL << 8) | (TX_DBUF_NULL << 4) | TX_DBUF_NULL;
	Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_Size		= 4;			// 
	Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_Control	= SEG_RESET;	// empty TX Seg 1 

	Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_ADDR		= 1024*(4 + 4);	// TX Seg2 Base Address 
	Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT 	= (TX_DBUF_NULL << 8) | (TX_DBUF_NULL << 4) | TX_DBUF_NULL;
	Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_Size		= 8;			// 
	Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control	= SEG_RESET;	// empty TX Seg 2 

	Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_ADDR		= 1024*(8 + 4 + 4);	// TX Seg3 Base Address 
	Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT 	= (TX_DBUF_NULL << 8) | (TX_DBUF_NULL << 4) | TX_DBUF_NULL;
	Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_Size		= 8;			// 
	Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_Control	= SEG_RESET;	// empty TX Seg 3 

	Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_ADDR		= 1024*(8 + 8 + 4 + 4);	// TX Seg4 Base Address 
	Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT 	= (TX_DBUF_NULL << 8) | (TX_DBUF_NULL << 4) | TX_DBUF_NULL;
	Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_Size		= 8;			// 
	Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_Control	= SEG_RESET;	// empty TX Seg 4 

	// RX DBUF: 32K
	Rx_Dbuf = rx_Dbuf_addr;	
	Rx_Dbuf->dbuf_Seg[0].dbuf_Seg_ADDR		= 0;			// RX Seg0 Base Address 
	Rx_Dbuf->dbuf_Seg[0].dbuf_Seg_INOUT 	= (RX_DBUF_NULL << 8) | (RX_DBUF_NULL << 4) | RX_DBUF_NULL;
	Rx_Dbuf->dbuf_Seg[0].dbuf_Seg_Size		= 4;			// 
	Rx_Dbuf->dbuf_Seg[0].dbuf_Seg_Control	= SEG_RESET;	// empty RX Seg 0 

	Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_ADDR		= 1024*4;	// RX Seg1 Base Address 
	Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT 	= (RX_DBUF_NULL << 8) | (RX_DBUF_NULL << 4) | RX_DBUF_NULL;
	Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_Size		= 8;			// 
	Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_Control	= SEG_RESET;	// empty RX Seg 1 

	Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_ADDR		= 1024*(8 + 4);// RX Seg2 Base Address 
	Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT 	= (RX_DBUF_NULL << 8) | (RX_DBUF_NULL << 4) | RX_DBUF_NULL;
	Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_Size		= 4;			// 
	Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control	= SEG_RESET;	// empty RX Seg 2 

	// RX dbuf seg 3 & 4 share the same DBUF segment
	Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_ADDR		= 1024*(8 + 4+4);	// RX Seg3 Base Address 
	Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT 	= (RX_DBUF_NULL << 8) | (RX_DBUF_NULL << 4) | RX_DBUF_NULL;
	Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_Size		= 8;			// 
	Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_Control	= SEG_RESET;	// empty RX Seg 3 

	Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_ADDR		= 1024*(8 + 8 +4 + 4);	// RX Seg4 Base Address 
	Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT 	= (RX_DBUF_NULL << 8) | (RX_DBUF_NULL << 4) | RX_DBUF_NULL;
	Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_Size		= 8;			// 
	Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_Control	= SEG_RESET;	// empty RX Seg 4
	rst_all_seg();
}

void rst_all_seg(void)
{
	Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_Control = SEG_RESET;
	Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_Control = SEG_RESET;
	Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control = SEG_RESET;
	Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_Control = SEG_RESET;
	Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_Control = SEG_RESET;

	for (u8 i = 0; i < 5; i++)
	{
		Tx_Dbuf->dbuf_Seg[i].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_NULL << 4) | TX_DBUF_NULL; 				
		dbufObj.TxDbuf_segStatus[i] = SEG_IDLE;
	}
	Rx_Dbuf->dbuf_Seg[0].dbuf_Seg_Control = SEG_RESET;
	Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_Control = SEG_RESET;
	Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control = SEG_RESET;
	Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_Control = SEG_RESET;
	Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_Control = SEG_RESET;

	for (i = 0; i < 5; i++)
	{
		Rx_Dbuf->dbuf_Seg[i].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_NULL << 4) | RX_DBUF_NULL; 				
		dbufObj.RxDbuf_segStatus[i] = SEG_IDLE;
	}
}

void set_dbuf_readPrefetch(u8 dbuf_index)
{
	switch (dbuf_index)
	{			
		case DBUF_SEG_U2S0R:
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = (TX_DBUF_USB_R_PORT << 4) | (*((u8 *) &(Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT)) &0x0F);
			dbufObj.TxDbuf_segStatus[1] = SEG_BUSY;				
			break;
			
		case DBUF_SEG_U2S0R_UAS2:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_USB_R_PORT << 4) | (*((u8 *) &(Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT)) &0x0F);
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;			
			break;
			
			
		case DBUF_SEG_U2S1R:
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = (TX_DBUF_USB_R_PORT << 4) | (*((u8 *) &(Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT)) &0x0F); 
			dbufObj.TxDbuf_segStatus[1] = SEG_BUSY;				
			break;

		case DBUF_SEG_U2S1R_UAS2:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_USB_R_PORT << 4) | (*((u8 *) &(Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT)) &0x0F); 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;				
			break;
			
		case DBUF_SEG_U2SR_RAID:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_USB_R_PORT << 4) | (*((u8 *) &(Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT)) &0x0F);
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;	
			break;
			
		case DBUF_SEG_U2S0R_AES:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_USB_R_PORT << 4) | (*((u8 *) &(Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT)) &0x0F); 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;				
			break;

		case DBUF_SEG_U2S0R_AES_UAS2:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_USB_R_PORT << 4) | (*((u8 *) &(Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT)) &0x0F); 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;				
			break;
			
		case DBUF_SEG_U2S1R_AES:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_USB_R_PORT << 4) | (*((u8 *) &(Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT)) &0x0F); 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;				
			break;

		case DBUF_SEG_U2S1R_AES_UAS2:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_USB_R_PORT << 4) | (*((u8 *) &(Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT)) &0x0F); 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;				
			break;

		default: break;		
	}
}

void set_dbufConnection(u8 dbuf_index)
{
	switch (dbuf_index)
	{
		case DBUF_SEG_B2S0R:
			// local data in SATA command
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_CPU_R_PORT << 4) | TX_DBUF_SATA0_W_PORT; 
			dbufObj.TxDbuf_segStatus[4] = SEG_BUSY;
			break;

		case DBUF_SEG_B2S0R2:
			// local data in SATA command
			Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_CPU_R_PORT << 4) | TX_DBUF_SATA0_W_PORT; 
			dbufObj.TxDbuf_segStatus[0] = SEG_BUSY;
			break;
			
		case DBUF_SEG_B2S0W:
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_SATA0_R_PORT << 4) | RX_DBUF_CPU_W_PORT; 
			dbufObj.RxDbuf_segStatus[4] = SEG_BUSY;			
			break;
			
		case DBUF_SEG_B2S0R_AES:
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_AESE_R_PORT << 4) | TX_DBUF_SATA0_W_PORT; 
			dbufObj.TxDbuf_segStatus[4] = SEG_BUSY;
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_CPU_R_PORT << 4) | TX_DBUF_AESD_W_PORT; 
			dbufObj.TxDbuf_segStatus[3] = SEG_BUSY;
			break;	
			
		case  DBUF_SEG_B2S0W_AES:
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_AESD_R_PORT << 4) | RX_DBUF_CPU_W_PORT; 
			dbufObj.RxDbuf_segStatus[1] = SEG_BUSY;
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_SATA0_R_PORT << 4) | RX_DBUF_AESE_W_PORT; 
			dbufObj.RxDbuf_segStatus[2] = SEG_BUSY;
			break;
			
		case DBUF_SEG_B2S1R:
			// local data in SATA command
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_CPU_R_PORT << 4) | TX_DBUF_SATA1_W_PORT; 
			dbufObj.TxDbuf_segStatus[4] = SEG_BUSY;
			break;

		case DBUF_SEG_B2S1R2:
			// local data in SATA command
			Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_CPU_R_PORT << 4) | TX_DBUF_SATA1_W_PORT; 
			dbufObj.TxDbuf_segStatus[0] = SEG_BUSY;
			break;
			
		case DBUF_SEG_B2S1W:
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = (RX_DBUF_SATA1_R_PORT << 8) | (RX_DBUF_NULL << 4) | RX_DBUF_CPU_W_PORT; 
			dbufObj.RxDbuf_segStatus[4] = SEG_BUSY;	
			break;
			
		case DBUF_SEG_B2S1R_AES:
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_AESE_R_PORT << 4) | TX_DBUF_SATA1_W_PORT; 
			dbufObj.TxDbuf_segStatus[4] = SEG_BUSY;
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_CPU_R_PORT << 4) | TX_DBUF_AESD_W_PORT; 
			dbufObj.TxDbuf_segStatus[3] = SEG_BUSY;
			break;	
			
		case  DBUF_SEG_B2S1W_AES:
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_AESD_R_PORT << 4) | RX_DBUF_CPU_W_PORT; 
			dbufObj.RxDbuf_segStatus[1] = SEG_BUSY;
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (RX_DBUF_SATA1_R_PORT << 8) | (RX_DBUF_NULL << 4) | RX_DBUF_AESE_W_PORT; 
			dbufObj.RxDbuf_segStatus[2] = SEG_BUSY;
			break;
			
		case DBUF_SEG_S2SR_VERIFY: // connect SATA0 to RAID
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_STRIPE0_R_PORT << 4) | TX_DBUF_SATA0_W_PORT;
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = (TX_DBUF_STRIPE1_R_PORT << 4) | TX_DBUF_SATA1_W_PORT;
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;
			dbufObj.TxDbuf_segStatus[3] = SEG_BUSY;
			break;

		case DBUF_SEG_S2SR_REBUILD0: // dbuf seg 2/3 can be used
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_DD_R_PORT << 4) | TX_DBUF_SATA0_W_PORT;
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;
			break;

		case DBUF_SEG_S2SW_REBUILD0:
#ifdef RAID_REBUILD_BY_CPU
			Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = (RX_DBUF_CPU_R_PORT << 4) |RX_DBUF_CPU_W_PORT;
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = (RX_DBUF_SATA1_R_PORT << 8) |(RX_DBUF_NULL << 4) |RX_DBUF_DD_W_PORT;
			dbufObj.RxDbuf_segStatus[3] = SEG_BUSY;
#else
			Rx_Dbuf->dbuf_Seg[0].dbuf_Seg_INOUT = (RX_DBUF_SATA1_R_PORT << 8) |(RX_DBUF_NULL << 4) |RX_DBUF_DD_W_PORT;
#endif
			dbufObj.RxDbuf_segStatus[0] = SEG_BUSY;
			break;

		case DBUF_SEG_S2SR_REBUILD1:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_DD_R_PORT << 4) | TX_DBUF_SATA1_W_PORT;
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;
			break;

		case DBUF_SEG_S2SW_REBUILD1:
#ifdef RAID_REBUILD_BY_CPU
			Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = (RX_DBUF_CPU_R_PORT << 4)|RX_DBUF_CPU_W_PORT;
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = (RX_DBUF_SATA0_R_PORT << 4)|RX_DBUF_DD_W_PORT;
#else
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = (RX_DBUF_SATA0_R_PORT << 4)|RX_DBUF_DD_W_PORT;
#endif
			dbufObj.RxDbuf_segStatus[3] = SEG_BUSY;
			dbufObj.RxDbuf_segStatus[4] = SEG_BUSY;
			break;
			
		case DBUF_SEG_U2BR:
			Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_CPU_W_PORT; 
			dbufObj.TxDbuf_segStatus[0] = SEG_BUSY;				
			break;
			
		case DBUF_SEG_U2BW:
			Rx_Dbuf->dbuf_Seg[0].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_CPU_R_PORT << 4) | RX_DBUF_USB_W_PORT; 
			dbufObj.RxDbuf_segStatus[0] = SEG_BUSY;
			break;
			
		case DBUF_SEG_U2S0R:
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_SATA0_W_PORT; 
			dbufObj.TxDbuf_segStatus[1] = SEG_BUSY;
			break;
			
		case DBUF_SEG_U2S0R_UAS2:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_SATA0_W_PORT; 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;	
			break;
			
			
		case DBUF_SEG_U2S1R:
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_SATA1_W_PORT; 
			dbufObj.TxDbuf_segStatus[1] = SEG_BUSY;				
			break;

		case DBUF_SEG_U2S1R_UAS2:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_SATA1_W_PORT; 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;				
			break;
			
		case DBUF_SEG_U2S0W:
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_SATA0_R_PORT << 4) | RX_DBUF_USB_W_PORT; 
			dbufObj.RxDbuf_segStatus[1] = SEG_BUSY;
			break;

		case DBUF_SEG_U2S0W_UAS2:
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_SATA0_R_PORT << 4) | RX_DBUF_USB_W_PORT; 
			dbufObj.RxDbuf_segStatus[2] = SEG_BUSY;
			break;
			
		case DBUF_SEG_U2S1W:			
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = (RX_DBUF_SATA1_R_PORT << 8) | (RX_DBUF_NULL << 4) | RX_DBUF_USB_W_PORT; 
			dbufObj.RxDbuf_segStatus[1] = SEG_BUSY;
			break;	

		case DBUF_SEG_U2S1W_UAS2:			
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (RX_DBUF_SATA1_R_PORT << 8) | (RX_DBUF_NULL << 4) | RX_DBUF_USB_W_PORT; 
			dbufObj.RxDbuf_segStatus[2] = SEG_BUSY;
			break;	
			
		case DBUF_SEG_U2SR_RAID:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_RAID_W_PORT; 
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_STRIPE0_R_PORT << 4) | TX_DBUF_SATA0_W_PORT; 
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_STRIPE1_R_PORT << 4) | TX_DBUF_SATA1_W_PORT; 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;	
			dbufObj.TxDbuf_segStatus[3] = SEG_BUSY;
			dbufObj.TxDbuf_segStatus[4] = SEG_BUSY;
			break;
			
		case DBUF_SEG_U2SW_RAID:
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (RX_DBUF_RAID_R_PORT << 8) | (RX_DBUF_NULL << 4) | RX_DBUF_USB_W_PORT; 
			Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_SATA0_R_PORT << 4) | RX_DBUF_STRIPE0_W_PORT; 
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = (RX_DBUF_SATA1_R_PORT << 8) | (RX_DBUF_NULL << 4) | RX_DBUF_STRIPE1_W_PORT; 
			dbufObj.RxDbuf_segStatus[2] = SEG_BUSY;	
			dbufObj.RxDbuf_segStatus[3] = SEG_BUSY;
			dbufObj.RxDbuf_segStatus[4] = SEG_BUSY;
			break;
			
		case DBUF_SEG_U2S0R_AES:
			// Tx seg 1,2,3 can be used 
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_AESE_R_PORT << 4) | TX_DBUF_SATA0_W_PORT; 
			dbufObj.TxDbuf_segStatus[1] = SEG_BUSY;
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_AESD_W_PORT; 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;				
			break;

		case DBUF_SEG_U2S0R_AES_UAS2:
			// Tx seg 1,2,3 can be used 
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_AESE_R_PORT << 4) | TX_DBUF_SATA0_W_PORT; 
			dbufObj.TxDbuf_segStatus[3] = SEG_BUSY;
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_AESD_W_PORT; 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;				
			break;
			
		case DBUF_SEG_U2S0W_AES:
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_AESD_R_PORT << 4) | RX_DBUF_USB_W_PORT; 
			dbufObj.RxDbuf_segStatus[2] = SEG_BUSY;
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_SATA0_R_PORT << 4) | RX_DBUF_AESE_W_PORT; 
			dbufObj.RxDbuf_segStatus[1] = SEG_BUSY;
			break;

		case DBUF_SEG_U2S0W_AES_UAS2:
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_AESD_R_PORT << 4) | RX_DBUF_USB_W_PORT; 
			dbufObj.RxDbuf_segStatus[2] = SEG_BUSY;
			Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_SATA0_R_PORT << 4) | RX_DBUF_AESE_W_PORT; 
			dbufObj.RxDbuf_segStatus[3] = SEG_BUSY;
			break;
			
		case DBUF_SEG_U2S1R_AES:
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_AESE_R_PORT << 4) | TX_DBUF_SATA1_W_PORT; 
			dbufObj.TxDbuf_segStatus[1] = SEG_BUSY;
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_AESD_W_PORT; 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;				
			break;

		case DBUF_SEG_U2S1R_AES_UAS2:
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_AESE_R_PORT << 4) | TX_DBUF_SATA1_W_PORT; 
			dbufObj.TxDbuf_segStatus[3] = SEG_BUSY;
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_AESD_W_PORT; 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;				
			break;
			
		case DBUF_SEG_U2S1W_AES:
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_AESD_R_PORT << 4) | RX_DBUF_USB_W_PORT; 
			dbufObj.RxDbuf_segStatus[2] = SEG_BUSY;
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = (RX_DBUF_SATA1_R_PORT << 8) | (RX_DBUF_NULL << 4) | RX_DBUF_AESE_W_PORT; 
			dbufObj.RxDbuf_segStatus[1] = SEG_BUSY;	
			break;

		case DBUF_SEG_U2S1W_AES_UAS2:
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_AESD_R_PORT << 4) | RX_DBUF_USB_W_PORT; 
			dbufObj.RxDbuf_segStatus[2] = SEG_BUSY;
			Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = (RX_DBUF_SATA1_R_PORT << 8) | (RX_DBUF_NULL << 4) | RX_DBUF_AESE_W_PORT; 
			dbufObj.RxDbuf_segStatus[3] = SEG_BUSY;	
			break;

		case DBUF_SEG_U2SR_AES_RAID:
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_AESD_W_PORT; 
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_AESE_R_PORT << 4) | TX_DBUF_RAID_W_PORT; 
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_STRIPE0_R_PORT << 4) | TX_DBUF_SATA0_W_PORT; 
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_STRIPE1_R_PORT << 4) | TX_DBUF_SATA1_W_PORT; 
			dbufObj.TxDbuf_segStatus[1] = SEG_BUSY;	
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;	
			dbufObj.TxDbuf_segStatus[3] = SEG_BUSY;
			dbufObj.TxDbuf_segStatus[4] = SEG_BUSY;
			break;

		case DBUF_SEG_U2SW_AES_RAID:
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_AESD_R_PORT << 4) | RX_DBUF_USB_W_PORT;			
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (RX_DBUF_RAID_R_PORT << 8) | (RX_DBUF_NULL << 4) | RX_DBUF_AESE_W_PORT; 
			Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = (RX_DBUF_NULL << 8) | (RX_DBUF_SATA0_R_PORT << 4) | RX_DBUF_STRIPE0_W_PORT; 
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = (RX_DBUF_SATA1_R_PORT<< 8) | (RX_DBUF_NULL << 4) | RX_DBUF_STRIPE1_W_PORT; 
			dbufObj.RxDbuf_segStatus[1] = SEG_BUSY;
			dbufObj.RxDbuf_segStatus[2] = SEG_BUSY;	
			dbufObj.RxDbuf_segStatus[3] = SEG_BUSY;
			dbufObj.RxDbuf_segStatus[4] = SEG_BUSY;	
			break;

		default: break;		
	}
}

void set_dbufSource(u8 dbuf_index)
{
	switch (dbuf_index)
	{
		case DBUF_SEG_U2BR:
			Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_INOUT = TX_DBUF_CPU_W_PORT; 
			dbufObj.TxDbuf_segStatus[0] = SEG_BUSY;				
			break;
		case DBUF_SEG_B2S0W:
		case DBUF_SEG_B2S1W:
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = RX_DBUF_CPU_W_PORT; 
			dbufObj.RxDbuf_segStatus[4] = SEG_BUSY;				
			break;
		case DBUF_SEG_B2S0W_AES:
		case DBUF_SEG_B2S1W_AES:
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = RX_DBUF_CPU_W_PORT; 
			dbufObj.RxDbuf_segStatus[1] = SEG_BUSY;				
			break;
		case DBUF_SEG_U2S0R:
		case DBUF_SEG_U2S0R_AES:
			if (dbufObj.TxDbuf_segStatus[1] == SEG_BUSY)
			{
				MSG("TX SEG1 is used\n");
			}
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = TX_DBUF_SATA0_W_PORT; 
			dbufObj.TxDbuf_segStatus[1] = SEG_BUSY;	
			break;

		case DBUF_SEG_U2S0R_UAS2:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = TX_DBUF_SATA0_W_PORT; 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;
			break;

		case DBUF_SEG_U2S0R_AES_UAS2:
			if (dbufObj.TxDbuf_segStatus[3] == SEG_BUSY)
			{
				MSG("TX SEG3 is used\n");
			}
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = TX_DBUF_SATA0_W_PORT; 
			dbufObj.TxDbuf_segStatus[3] = SEG_BUSY;	
			break;

		case DBUF_SEG_U2S1R:
		case DBUF_SEG_U2S1R_AES:
			if (dbufObj.TxDbuf_segStatus[1] == SEG_BUSY)
			{
				MSG("TX SEG1 is used\n");
			}
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = TX_DBUF_SATA1_W_PORT; 
			dbufObj.TxDbuf_segStatus[1] = SEG_BUSY;	
			break;

		case DBUF_SEG_U2S1R_UAS2:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = TX_DBUF_SATA1_W_PORT; 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;	
			break;	

		case DBUF_SEG_U2S1R_AES_UAS2:
			if (dbufObj.TxDbuf_segStatus[3] == SEG_BUSY)
			{
				MSG("TX SEG3 is used\n");
			}
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = TX_DBUF_SATA1_W_PORT; 
			dbufObj.TxDbuf_segStatus[3] = SEG_BUSY;	
			break;	

		case DBUF_SEG_U2SR_RAID:
		case DBUF_SEG_U2SR_AES_RAID:
			if (dbufObj.TxDbuf_segStatus[2] == SEG_IDLE)
			{
				Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_NULL << 4) | TX_DBUF_RAID_W_PORT; 
				Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_STRIPE0_R_PORT << 4) | TX_DBUF_SATA0_W_PORT; 
				Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_STRIPE1_R_PORT << 4) | TX_DBUF_SATA1_W_PORT; 
				dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;
				dbufObj.TxDbuf_segStatus[3] = SEG_BUSY;
				dbufObj.TxDbuf_segStatus[4] = SEG_BUSY;	
			}
			break;

		default: break;	
	}
}

void keep_sourceDbufConnection(u8 dbuf_index)
{
	switch (dbuf_index)
	{
		case DBUF_SEG_U2BR:
			Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_INOUT = Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_INOUT | (TX_DBUF_USB_R_PORT << 4) ; 
			dbufObj.TxDbuf_segStatus[0] = SEG_BUSY;				
			break;
		case DBUF_SEG_U2BW:
			break;
			
		case DBUF_SEG_U2S0R:			
		case DBUF_SEG_U2S1R:
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT | (TX_DBUF_USB_R_PORT << 4); 
			dbufObj.TxDbuf_segStatus[1] = SEG_BUSY;	
			break;

		case DBUF_SEG_U2S0R_UAS2:
		case DBUF_SEG_U2S1R_UAS2:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT | (TX_DBUF_USB_R_PORT << 4); 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;	
			break;

		case DBUF_SEG_U2S0R_AES:
		case DBUF_SEG_U2S1R_AES:
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT | (TX_DBUF_AESE_R_PORT << 4); 
			dbufObj.TxDbuf_segStatus[1] = SEG_BUSY;
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_AESD_W_PORT; 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;						
			break;

		case DBUF_SEG_U2S0R_AES_UAS2:
		case DBUF_SEG_U2S1R_AES_UAS2:
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT | (TX_DBUF_AESE_R_PORT << 4); 
			dbufObj.TxDbuf_segStatus[3] = SEG_BUSY;
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_AESD_W_PORT; 
			dbufObj.TxDbuf_segStatus[2] = SEG_BUSY;						
			break;

		case DBUF_SEG_U2SR_RAID:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT | (TX_DBUF_USB_R_PORT << 4) ;
			break;
			
		case DBUF_SEG_U2SR_AES_RAID:
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = (TX_DBUF_NULL << 8) | (TX_DBUF_USB_R_PORT << 4) | TX_DBUF_AESD_W_PORT; 
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT | (TX_DBUF_AESE_R_PORT << 4); 
			dbufObj.TxDbuf_segStatus[1] = SEG_BUSY;
			break;
			
		default: break;	
	}
}

void free_SourcedbufConnection(u8 dbuf_index)
{
	switch (dbuf_index)
	{
		case DBUF_SEG_U2S0R:
		case DBUF_SEG_U2S0R_AES:
		case DBUF_SEG_U2S1R:
		case DBUF_SEG_U2S1R_AES:
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT & 0xF0;
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT;
			break;
			
		case DBUF_SEG_U2S0R_UAS2:
		case DBUF_SEG_U2S1R_UAS2:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT & 0xF0;
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT;
			break;			
		case DBUF_SEG_U2S0R_AES_UAS2:
		case DBUF_SEG_U2S1R_AES_UAS2:
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT & 0xF0;
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT;
			break;
		default: break;	
	}
}

void disconnect_SataDbufConnection(u8 sataSlot)
{	
	u8 portNum;
	if (sataSlot == 0)
	{
		portNum = TX_DBUF_SATA0_W_PORT;
	}
	else
	{
		portNum = TX_DBUF_SATA1_W_PORT;
	}
	for (u8 i = 1; i < 5; i++)
	{
		if ((Tx_Dbuf->dbuf_Seg[i].dbuf_Seg_INOUT & 0x0F) == portNum)
		{
			Tx_Dbuf->dbuf_Seg[i].dbuf_Seg_INOUT &= 0xF0; 
		}
	}
	
	if (sataSlot == 0)
	{
		portNum = RX_DBUF_SATA0_R_PORT;
	}
	else
	{
		portNum = RX_DBUF_SATA1_R_PORT;
	}
	
	for (i = 1; i < 5; i++)
	{
		if (sataSlot == 0)
		{
			if (((Rx_Dbuf->dbuf_Seg[i].dbuf_Seg_INOUT & 0xF0) >> 4) == portNum)
			{
				Rx_Dbuf->dbuf_Seg[i].dbuf_Seg_INOUT &= 0x0F; 
			}			
		}
		else
		{
			if (((Rx_Dbuf->dbuf_Seg[i].dbuf_Seg_INOUT & 0xF00) >> 8) == portNum)
			{
				Rx_Dbuf->dbuf_Seg[i].dbuf_Seg_INOUT &= 0x0FF; 
			}
		}
	}
}

void set_dbufDone(u8 dbuf_index)
{
	switch (dbuf_index)
	{
		case DBUF_SEG_U2BR:
			Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_Control = SEG_DONE;
			break;
		case DBUF_SEG_B2S0W:
		case DBUF_SEG_B2S1W:
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_Control = SEG_DONE; 			
			break;
		case DBUF_SEG_B2S0W_AES:
		case DBUF_SEG_B2S1W_AES:
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_Control = SEG_DONE;
			break;		

		default: break;		
	}
}

// most time, reset the Dbuf source
void reset_dbuf_seg(u8 dbuf_index)
{
	switch (dbuf_index)
	{
		case DBUF_SEG_U2BR:
			// local bridge handling SCSI command
			Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_Control = SEG_RESET;
			Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_Control;
			break;

		case DBUF_SEG_B2S0R:
		case DBUF_SEG_B2S1R:
			// local data in SATA command
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_Control = SEG_RESET;
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_Control;		
			break;

		case DBUF_SEG_B2S0R2:
		case DBUF_SEG_B2S1R2:
			// local data in SATA command
			Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_Control = SEG_RESET;
			Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_Control;		
			break;
			
		case DBUF_SEG_U2BW:
			Rx_Dbuf->dbuf_Seg[0].dbuf_Seg_Control = SEG_RESET; 
			Rx_Dbuf->dbuf_Seg[0].dbuf_Seg_Control;
			break;

		case DBUF_SEG_B2S0W:
		case DBUF_SEG_B2S1W:
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_Control = SEG_RESET; 
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_Control;
			break;
			
		case DBUF_SEG_S2SW_REBUILD0:
		case DBUF_SEG_S2SW_REBUILD1:
#ifdef RAID_REBUILD_BY_CPU
			Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_Control = SEG_RESET; 
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_Control = SEG_RESET; 
			Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_Control;
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_Control;
#else
			Rx_Dbuf->dbuf_Seg[0].dbuf_Seg_Control = SEG_RESET; 
			Rx_Dbuf->dbuf_Seg[0].dbuf_Seg_Control;
#endif
			break;
			
		case  DBUF_SEG_B2S0W_AES:
		case  DBUF_SEG_B2S1W_AES:
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_Control = SEG_RESET; 
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control = SEG_RESET; 
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control;
			break;
			
		case DBUF_SEG_B2S0R_AES:
		case DBUF_SEG_B2S1R_AES:			
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_Control = SEG_RESET;
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_Control = SEG_RESET;
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_Control;
			break;		

		case DBUF_SEG_S2SR_VERIFY:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control = SEG_RESET;
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_Control = SEG_RESET;
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_Control;			
			break;

		case DBUF_SEG_S2SR_REBUILD0:
		case DBUF_SEG_S2SR_REBUILD1:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control = SEG_RESET;
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control;
			break;			
			
		case DBUF_SEG_U2S0R:
		case DBUF_SEG_U2S1R:
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_Control = SEG_RESET;
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_Control;				
			break;

		case DBUF_SEG_U2S0R_UAS2:
		case DBUF_SEG_U2S1R_UAS2:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control = SEG_RESET;
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control;			
			break;
			
		case DBUF_SEG_U2S0W:
		case DBUF_SEG_U2S1W:	
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_Control = SEG_RESET;
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_Control;
			break;
			
		case DBUF_SEG_U2S0W_UAS2:
		case DBUF_SEG_U2S1W_UAS2:			
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control = SEG_RESET;
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control;
			break;	

		case DBUF_SEG_U2S0R_AES:
		case DBUF_SEG_U2S1R_AES:
//			if (dbuf_index == DBUF_SEG_U2S0R_AES)
//				MSG("R S0\n");
//			else
//				MSG("R S1\n");
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_Control = SEG_RESET;
			*aes_control &= ~(AES_DECRYP_EN|AES_DECRYP_KEYSET_SEL);
			*aes_control;
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control = SEG_RESET;
			break;

		case DBUF_SEG_U2S0R_AES_UAS2:
		case DBUF_SEG_U2S1R_AES_UAS2:
//			if (dbuf_index == DBUF_SEG_U2S0R_AES_UAS2)
//				MSG("R S0 2\n");
//			else
//				MSG("R S1 2\n");
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_Control = SEG_RESET;
			*aes_control &= ~(AES_DECRYP_EN|AES_DECRYP_KEYSET_SEL);
			*aes_control;
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control = SEG_RESET;			
			break;
			
		case DBUF_SEG_U2S0W_AES:
		case DBUF_SEG_U2S1W_AES:
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control = SEG_RESET;
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control;
			*aes_control  &= ~(AES_ENCRYP_EN|AES_ENCRYP_KEYSET_SEL);	
			*aes_control;
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_Control = SEG_RESET;
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_Control;	
			break;

		case DBUF_SEG_U2S0W_AES_UAS2:
		case DBUF_SEG_U2S1W_AES_UAS2:
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control = SEG_RESET;
			*aes_control  &= ~(AES_ENCRYP_EN|AES_ENCRYP_KEYSET_SEL);
			*aes_control;
			Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_Control = SEG_RESET;
			Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_Control;	
			break;

		// reset the source DBUF			
		case DBUF_SEG_U2SR_RAID:	
		case DBUF_SEG_U2SR_AES_RAID:
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_Control = SEG_RESET; 
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_Control = SEG_RESET;
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_Control;
			break;
			
		case DBUF_SEG_U2SW_RAID:
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control = SEG_RESET;
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control;	
			break;
			
		case DBUF_SEG_U2SW_AES_RAID:
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_Control = SEG_RESET;
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control = SEG_RESET;
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_Control;	
			break;
			

		default: break;		
	}
}

void free_dbufConnection(u8 dbuf_index)
{
	switch (dbuf_index)
	{
		case DBUF_SEG_B2S0R:
		case DBUF_SEG_B2S1R:
			// local data in SATA command
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = 0;  // (TX_DBUF_NULL << 8) | (TX_DBUF_NULL << 4) | TX_DBUF_NULL
			dbufObj.TxDbuf_segStatus[4] = SEG_IDLE;
			break;

		case DBUF_SEG_B2S0R2:
		case DBUF_SEG_B2S1R2:
			// local data in SATA command
			Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_INOUT = 0;  // (TX_DBUF_NULL << 8) | (TX_DBUF_NULL << 4) | TX_DBUF_NULL
			dbufObj.TxDbuf_segStatus[0] = SEG_IDLE;
			break;
			
		case DBUF_SEG_B2S0W:
		case DBUF_SEG_B2S1W:
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = 0; 
			dbufObj.RxDbuf_segStatus[4] = SEG_IDLE;
			break;

		case DBUF_SEG_B2S0R_AES:
		case DBUF_SEG_B2S1R_AES:
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = 0; 
			dbufObj.TxDbuf_segStatus[3] = SEG_IDLE;
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = 0; 
			dbufObj.TxDbuf_segStatus[4] = SEG_IDLE;
			break;		
			
		case  DBUF_SEG_B2S0W_AES:
		case  DBUF_SEG_B2S1W_AES:
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = 0; 
			dbufObj.RxDbuf_segStatus[1] = SEG_IDLE;
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = 0; 
			dbufObj.RxDbuf_segStatus[2] = SEG_IDLE;
			break;
			
		case DBUF_SEG_S2SR_VERIFY:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = 0; 
			dbufObj.TxDbuf_segStatus[2] = SEG_IDLE;
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = 0; 
			dbufObj.TxDbuf_segStatus[3] = SEG_IDLE;
			break;

		case DBUF_SEG_S2SR_REBUILD0: 
		case DBUF_SEG_S2SR_REBUILD1:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = 0; 
			dbufObj.TxDbuf_segStatus[2] = SEG_IDLE;
			break;

		case DBUF_SEG_S2SW_REBUILD0:
		case DBUF_SEG_S2SW_REBUILD1:
#ifdef RAID_REBUILD_BY_CPU
			Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = 0;
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = 0; 
			dbufObj.RxDbuf_segStatus[3] = SEG_IDLE;
			dbufObj.RxDbuf_segStatus[4] = SEG_IDLE;
#else
			Rx_Dbuf->dbuf_Seg[0].dbuf_Seg_INOUT = 0; 
			dbufObj.RxDbuf_segStatus[0] = SEG_IDLE;
#endif
			break;
			
		case DBUF_SEG_U2BR:
			Tx_Dbuf->dbuf_Seg[0].dbuf_Seg_INOUT = 0; 
			dbufObj.TxDbuf_segStatus[0] = SEG_IDLE;				
			break;
		case DBUF_SEG_U2BW:
			Rx_Dbuf->dbuf_Seg[0].dbuf_Seg_INOUT = 0; 
			dbufObj.RxDbuf_segStatus[0] = SEG_IDLE;
			break;
		case DBUF_SEG_U2S0R:
		case DBUF_SEG_U2S1R:
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = 0; 
			dbufObj.TxDbuf_segStatus[1] = SEG_IDLE;				
			break;
		case DBUF_SEG_U2S0R_UAS2:
		case DBUF_SEG_U2S1R_UAS2:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = 0; 
			dbufObj.TxDbuf_segStatus[2] = SEG_IDLE;				
			break;			
			
		case DBUF_SEG_U2S0W:
		case DBUF_SEG_U2S1W:
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = 0; 
			dbufObj.RxDbuf_segStatus[1] = SEG_IDLE;
			break;

		case DBUF_SEG_U2S0W_UAS2:
		case DBUF_SEG_U2S1W_UAS2:
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = 0; 
			dbufObj.RxDbuf_segStatus[2] = SEG_IDLE;
			break;
			
		case DBUF_SEG_U2SR_RAID:
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = 0; 
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = 0; 
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = 0; 
			dbufObj.TxDbuf_segStatus[2] = SEG_IDLE;
			dbufObj.TxDbuf_segStatus[3] = SEG_IDLE;
			dbufObj.TxDbuf_segStatus[4] = SEG_IDLE;
			break;
			
		case DBUF_SEG_U2SW_RAID:
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = 0; 
			Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = 0; 
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = 0; 
			dbufObj.RxDbuf_segStatus[2] = SEG_IDLE;	
			dbufObj.RxDbuf_segStatus[3] = SEG_IDLE;
			dbufObj.RxDbuf_segStatus[4] = SEG_IDLE;
			break;
			
		case DBUF_SEG_U2S0R_AES:
		case DBUF_SEG_U2S1R_AES:
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = 0; 
			dbufObj.TxDbuf_segStatus[1] = SEG_IDLE;	
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = 0; 
			dbufObj.TxDbuf_segStatus[2] = SEG_IDLE;			
			break;
			
		case DBUF_SEG_U2S0R_AES_UAS2:
		case DBUF_SEG_U2S1R_AES_UAS2:
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = 0; 
			dbufObj.TxDbuf_segStatus[3] = SEG_IDLE;	
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = 0; 
			dbufObj.TxDbuf_segStatus[2] = SEG_IDLE;			
			break;
			
		case DBUF_SEG_U2S0W_AES:
		case DBUF_SEG_U2S1W_AES:
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = 0; 
			dbufObj.RxDbuf_segStatus[2] = SEG_IDLE;
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = 0; 
			dbufObj.RxDbuf_segStatus[1] = SEG_IDLE;
			break;

		case DBUF_SEG_U2S0W_AES_UAS2:
		case DBUF_SEG_U2S1W_AES_UAS2:
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = 0; 
			dbufObj.RxDbuf_segStatus[2] = SEG_IDLE;
			Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = 0; 
			dbufObj.RxDbuf_segStatus[3] = SEG_IDLE;
			break;
			
		case DBUF_SEG_U2SR_AES_RAID:
			Tx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = 0; 
			Tx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = 0; 
			Tx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = 0; 
			Tx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = 0; 	
			dbufObj.TxDbuf_segStatus[3] = SEG_IDLE;
			dbufObj.TxDbuf_segStatus[4] = SEG_IDLE;
			dbufObj.TxDbuf_segStatus[1] = SEG_IDLE;
			dbufObj.TxDbuf_segStatus[2] = SEG_IDLE;	
			break;
			
		case DBUF_SEG_U2SW_AES_RAID:
			Rx_Dbuf->dbuf_Seg[1].dbuf_Seg_INOUT = 0; 
			Rx_Dbuf->dbuf_Seg[2].dbuf_Seg_INOUT = 0; 		
			Rx_Dbuf->dbuf_Seg[3].dbuf_Seg_INOUT = 0;
			Rx_Dbuf->dbuf_Seg[4].dbuf_Seg_INOUT = 0; 
			dbufObj.RxDbuf_segStatus[1] = SEG_IDLE;
			dbufObj.RxDbuf_segStatus[2] = SEG_IDLE;
			dbufObj.RxDbuf_segStatus[3] = SEG_IDLE;
			dbufObj.RxDbuf_segStatus[4] = SEG_IDLE;	
			break;
		default: break;		
	}
}

u8 read_data_from_cpu_port(u32 byteCnt, u8 dbuf_dir, u8 data_Valid_flag)
{
DBG("CPU R %lx\n", byteCnt);
	u16 mc_addr = 0;
	u32 port_cnt = 0;
	DBUF_REG volatile *dbuf_ptr;
	if (dbuf_dir == DIR_TX)
		dbuf_ptr = Tx_Dbuf;
	else
		dbuf_ptr = Rx_Dbuf;
	port_cnt	= (dbuf_ptr->dbuf_Port[TX_DBUF_CPU_R_PORT].dbuf_Port_Count) & PORT_CNT;
	if (data_Valid_flag == 1)
	{
		if (port_cnt != byteCnt) // it shall not happen, dump it for debug purpose just in case
		{
			MSG("pcnt %x\n", (u16)port_cnt);
			for(u8 ii = 0; ii < 100; ii++)
			{
				port_cnt	= (dbuf_ptr->dbuf_Port[TX_DBUF_CPU_R_PORT].dbuf_Port_Count) & PORT_CNT;
				if (port_cnt == byteCnt)
				{
					break;
				}
				Delayus(100);
			}
			
			MSG("pcnt1 %x, %bx\n", (u16)port_cnt, ii); 			
		}
	}
	else
	{
		if (port_cnt != byteCnt)
			return 1;
	}

DBG("cnt %lx\n", port_cnt);
 	for (u32 i = port_cnt; i >3; i-= 4)
	{
		*((u32 *)(mc_buffer + mc_addr)) = dbuf_ptr->dbuf_DataPort;	
		mc_addr += 4;
 	}

	if (i & 0x02) // fetch the high 2 byte
	{
		*((u16 *)&mc_buffer[mc_addr]) = *((u16 *)&dbuf_ptr->dbuf_DataPort);
		mc_addr += 2;
	}
	if (i & 0x01) // fetch the low byte
		mc_buffer[mc_addr++] = *((u8 *)&dbuf_ptr->dbuf_DataPort);
	return 0;
}

void write_data_by_cpu_port(u32 byteCnt, u8 dbuf_dir)
{
	DBUF_REG volatile *dbuf_ptr;	
	u32 tmp32 = byteCnt >> 2;
	u32 *p32 = (u32 *)mc_buffer;
	if (dbuf_dir == DIR_TX)
		dbuf_ptr = Tx_Dbuf;
	else
		dbuf_ptr = Rx_Dbuf;
	for (u32 i = 0; i < tmp32; i++)
	{
		dbuf_ptr->dbuf_DataPort = *p32++;
	}
	
	if (byteCnt & 0x2)		// byte3, byte2
	{
		u16 *p16;	
		p16 = (u16 *)p32;				
		*((u16 *)&dbuf_ptr->dbuf_DataPort) = *p16++;
		if (byteCnt & 0x1)		// byte3
			*((u8 *)&dbuf_ptr->dbuf_DataPort) = *((u8 *)p16);
	}
	else if (byteCnt & 0x1)	// byte1
		*((u8 *)&dbuf_ptr->dbuf_DataPort) = *((u8 *)p32);
}
#endif

