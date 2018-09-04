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
 * 3610		2010/04/09	Odin		Initial version
 * 3610		2010/04/15	Odin		From 1610 LG 
 *
 *****************************************************************************/
#ifndef ATAPI_H
#define ATAPI_H

#undef  Ex
#ifdef ATAPI_C
	#define Ex
#else
	#define Ex extern
#endif

#define	MAX_BUFFER_LENGTH		  (64)







#define ATAPIOP_TEST_UNITY_READY  0x00
#define ATAPIOP_INQUIRY           0x12
#define ATAPIOP_REQUESTSENSE      0x03
#define ATAPIOP_READ_CAPACITY     0x25
#define ATAPIOP_READ              0x28
#define ATAPIOP_WRITE             0x2A
#define ATAPIOP_READ_TRACK_INFO   0x52


//#define   DEVICE_CRTL_DFT     0x08
//#define   DEVICE_CRTL_SRST    0x04
//#define   DEVICE_CRTL_IENT    0x02

//#define DEVICECLASS_ATA			0
//#define DEVICECLASS_ATAPI		1
//#define DEVICECLASS_CFA			2
//#define DEVICECLASS_NONE		3
//#define DEVICECLASS_UNKNOWN		4





//#define   ATA_STATUS_BSY  0x80
//#define   ATA_STATUS_DRDY 0x40
//#define   ATA_STATUS_DSC  0x10
//#define   ATA_STATUS_DRQ  0x08
//#define   ATA_STATUS_CORR 0x04
//#define   ATA_STATUS_IDX  0x02
//#define   ATA_STATUS_CHECK  0x01

//#define   ATA_ERR_ICRC    (1 << 7)
//#define   ATA_ERR_UNC     (1 << 6)
//#define   ATA_ERR_MC      (1 << 5)
//#define   ATA_ERR_IDNF    (1 << 4)
//#define   ATA_ERR_MCR     (1 << 3)
//#define   ATA_ERR_ABRT    (1 << 2)


//#define MEDIATYPE_FIXED			0x00
//#define MEDIATYPE_REMOVABLE		0x01
//#define	MEDIATYPE_NON_INIT		0x02


#define	FEATURES_DMA        0x01



#define		PACKET					0xA0


extern u32 atapi_ExecUSBNoDataCmd(PSATA_OBJ  pSob, PCDB_CTXT pCtxt);


extern void atapi_ReadID(PSATA_OBJ  pSoj);

extern void atapi_init(PSATA_OBJ  pSoj);
//extern void atapiSetDMAXferMode(PSATA_OBJ  pSob, u32 mode);
//extern void atapiSetupErrorBlock(u32 error);



#endif

