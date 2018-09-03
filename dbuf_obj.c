/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *
 *****************************************************************************/
#ifndef DBUF_OBJ_C
#define DBUF_OBJ_C
#include "general.h"

// UAS uses the 1 or 2 for the SATA TX & RX overlap
u8 TX_DBUF_GetIdleSeg_UAS(u8 control)
{
	if (control & DBUF_RAID_FUNCTION_ENABLE)
	{
		return control;
	}
	else
	{
		if (dbufObj.TxDbuf_segStatus[1] == SEG_IDLE)
		{	
			return control;
		}
		else if (((dbufObj.TxDbuf_segStatus[3] == SEG_IDLE) && (control & DBUF_AES_EN))
		|| ((dbufObj.TxDbuf_segStatus[2] == SEG_IDLE) && ((control & DBUF_AES_EN) == 0)))
		{
			return (control|DBUF_UAS_SEG2);
		}
		else
			return SEG_NULL;	
	}
}

u8 RX_DBUF_GetIdleSeg_UAS(u8 control)
{
	if (control & DBUF_RAID_FUNCTION_ENABLE)
	{
		return control;
	}
	else
	{
		if (dbufObj.RxDbuf_segStatus[1] == SEG_IDLE)
		{	
			return control;
		}
		else if (((dbufObj.RxDbuf_segStatus[3] == SEG_IDLE) && (control & DBUF_AES_EN))
		|| ((dbufObj.RxDbuf_segStatus[2] == SEG_IDLE) && ((control & DBUF_AES_EN) == 0)))
		{
			return (control|DBUF_UAS_SEG2);
		}
		else
			return SEG_NULL;		
	}
}
// UAS uses the 1 or 2 for the SATA TX & RX overlap
u8 chk_TX_DBUF_idleSeg_B2S(void)
{
	if (dbufObj.TxDbuf_segStatus[4] == SEG_IDLE)
	{	
		return 0;
	}
	else	 return 1;
}
#endif
