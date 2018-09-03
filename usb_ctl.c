/******************************************************************************
 *
 *   Copyright (C) Initio Corporation 2009-2013, All Rights Reserved
 *
 *   This file contains confidential and propietary information
 *   which is the property of Initio Corporation.
 *
 *****************************************************************************/

#include	"general.h"

#define	OUTBUF	usb_CtrlBuffer

//USB2.0 device descriptor
u8 const DeviceUSB2[] = {
	18,					//DeviceUSB2[0] = sizeof(DeviceUSB2);	// Descriptor length
	DESCTYPE_DEVICE,	//DeviceUSB2[1] 		// Descriptor type - Device
	0x10,				//DeviceUSB2[2] 		// Specification Version (BCD)
	0x02,				//DeviceUSB2[3] 		// inidcate USB 2.1 speed compilant
	0x00,				//DeviceUSB2[4] 		// Device class
	0x00,				//DeviceUSB2[5] 		// Device sub-class
	0x00,				//DeviceUSB2[6] 		// Device sub-sub-class
	0x40,				//DeviceUSB2[7] 		// Maximum packet size(64B)
	0xFD,				//DeviceUSB2[8] 		// 2 bytes Vendor ID
	0x13,				//DeviceUSB2[9] 		//
	0x80,				//DeviceUSB2[10]		// 2 bytes Product ID:
	0x38,				//DeviceUSB2[11] 		//
	0x00,				//DeviceUSB2[12			// Product version ID
	0x00,				//DeviceUSB2[13] 
	IMFC,				//DeviceUSB2[14] 		// iManufacture=01 (index of string describe manufacture)
	IPRODUCT,			//DeviceUSB2[15] 		// iProduct=02 (index of string describe product)
	ISERIAL,			//DeviceUSB2[16] 		// iSerial=03 (index of string describe dev serial #)
	0x01,				//DeviceUSB2[17] 		// Number of configurations
	0x00,0x00								//18-19 : pads
};

   //USB 2.1 BOT+UAS:config +interface Descriptor+Two endpoints descriptors
u8 const CfgUSB2[] = {
	9,				//CfgUSB2[0]				// config Descriptor length							00
	DESCTYPE_CFG,	//CfgUSB2[1] 				//DESCTYPE_CFG = 0x02
	9+9+7+7,		//CfgUSB2[2] = sizeof(CfgUSB2) + sizeof(BotInfUSB2) + sizeof(EndPointUSB2); //total length of data return
	//9+9+7+7+ 9+28+16,		//CfgUSB2[2] = sizeof(CfgUSB2) + sizeof(BotInfUSB2) + sizeof(EndPointUSB2)+ //total length of data return
	//								sizeof(UasInfUSB2)+ sizeof(UasEndPointUSB2)+sizeof(Pipe)
	0x00,			//CfgUSB2[3] 
	0x01,			//CfgUSB2[4] 				// Number of interfaces
	0x01,			//CfgUSB2[5] 				// Configuration number
	ICONFIG,		//CfgUSB2[6] 				// iConfiguration=00 (index of string describe config)
	0xc0,			//CfgUSB2[7]				// Attributes = 1100 0000 (bit7 res=1 and bit6 =self power)
	0x14			//CfgUSB2[8]				// Power comsumption  (2mA)
#if 1
	,9,				//BotInfUSB2[0]				// interface Descriptor length						09
	DESCTYPE_INF,	//BotInfUSB2[1]				// Descriptor type - Interface
	0x00,			//BotInfUSB2[2] 				// Zero-based index of this interface
	0x00,			//BotInfUSB2[3] 				// Alternate setting
	0x02,			//BotInfUSB2[4] 				// Number of end points
	0x08,			//BotInfUSB2[5] 				// MASS STORAGE Class.
	0x06,			//BotInfUSB2[6] 				// 06: SCSI transplant Command Set 
	0x50,			//BotInfUSB2[7] 				// BULK-ONLY TRANSPORT
	IINF,			//BotInfUSB2[8]				//iInterface=00
	7,				//EndPointUSB2[0][0]		// Bulk Out Endpoint Descriptor length				// 18
	DESCTYPE_ENDPOINT,	//EndPointUSB2[0][1]	// Descriptor type - Endpoint						// 19
	0x81,			//EndPointUSB2[0][2] 		// In Endpoint number 2							// 20
	0x02,			//EndPointUSB2[0][3] 		// Endpoint type (bit0-1) - Bulk						// 21
	0x00,			//EndPointUSB2[0][4] 		// 2 bytes Maximun packet size (bit0-10)				// 22
	0x02,			//EndPointUSB2[0][5] 													// 23
	0x00,			//EndPointUSB2[0][6] 		// Polling interval								// 24
	7,				//EndPointUSB2[1][0]		// Bulk-In Endpoint Descriptor length					// 25
	DESCTYPE_ENDPOINT,	//EndPointUSB2[1][1] 	// Descriptor type - Endpoint						// 26
	0x02,
	0x02,			//EndPointUSB2[1][3]		// Endpoint type - Bulk							// 28
	0x00,			//EndPointUSB2[1][4]		// Maximun packet size							// 29
	0x02,			//EndPointUSB2[1][5] 													// 30
	0x00			//EndPointUSB2[1][6]		// Polling interval								// 31
#endif

#ifdef UAS
	,9,				//UasInfUSB2[0] = sizeof(UasInfUSB2);	// uas interface Descriptor len				// 32
	DESCTYPE_INF,	//UasInfUSB2[1] 			// Descriptor type - Interface						// 33
	0x00,			//UasInfUSB2[2] 			// Zero-based index of this interface					// 34
	0x01,			//UasInfUSB2[3] 			// Alternate setting
	0x04,			//UasInfUSB2[4] 			// Number of end points
	0x08,			//UasInfUSB2[5] 			// MASS STORAGE Class.
	0x06,			//UasInfUSB2[6] 			// 06: SCSI transplant Command Set 
	0x62,			//UasInfUSB2[7] 			// UAS
	IINF,			//UasInfUSB2[8] 			// iInterface=00
					//					xmemcpy(UasEndPointUSB3[0], (u8 *)OUTBUF+53, 7);
	7,				//UasEndPointUSB2[0][0]		//command out endpoint Desc len						// 41
	DESCTYPE_ENDPOINT,	//UasEndPointUSB2[0][1] 	// Descriptor type - Endpoint							// 42
	0x04,			//UasEndPointUSB2[0][2] 		// Command Out									// 43
	0x02,			//UasEndPointUSB2[0][3] 		// bulk											// 44
	0x00,			//UasEndPointUSB2[0][4] 		// 2 bytes Maximun packet size (bit0-10)					// 45
	0x02,			//UasEndPointUSB2[0][5] 														// 46
	0x00,			//UasEndPointUSB2[0][6] 		// Polling interval endpoint for data xfer					// 47
	0x04,			//Pipe[0][0] 					//Pipe length									// 48
	0x24,			//Pipe[0][1] 					//type -- pipe									// 49
	0x01,			//Pipe[0][2] 					//ID -- command pipe							// 50
	0x00,			//Pipe[0][3] 					//reserved									// 51
					//
	7,				//UasEndPointUSB2[1][0] = 		// status pipe endpoint Descriptor len					//52
	DESCTYPE_ENDPOINT,	//UasEndPointUSB2[1][1]		// Descriptor type - Endpoint						// 53
	0x83,			//UasEndPointUSB2[1][2]			// Status In									// 54
	0x02,			//UasEndPointUSB2[1][3]			// Endpoint type - Bulk							// 55
	0x00,			//UasEndPointUSB2[1][4]			// Maximun packet size 512						// 56
	0x02,			//UasEndPointUSB2[1][5]														// 57
	0x00,			//UasEndPointUSB2[1][6]			// Polling interval								// 58
	
	0x04,			//Pipe[1][0] 					// pipe length									//59
	0x24,			//Pipe[1][1] = ;				// type -- pipe									// 60
	0x02,			//Pipe[1][2] = ;				// ID -- status pipe								// 61
	0x00,			//Pipe[1][3] = ;				// reserved									// 62
					//
	7,				//UasEndPointUSB2[2][0]			// Data Out Endpoint Descriptor length				// 63
	DESCTYPE_ENDPOINT,	//UasEndPointUSB2[2][1]		// Descriptor type - Endpoint						// 64
	0x02,
	0x02,			//UasEndPointUSB2[2][3] 		// bulk											// 66
	0x00,			//UasEndPointUSB2[2][4] 		// Maximun packet size 512							// 67
	0x02,			//UasEndPointUSB2[2][5] 														// 68
	0x00,			//UasEndPointUSB2[2][6]			// Polling interval								// 69
	
	0x04,			//Pipe[2][0]					// pipe length									//70
	0x24,			//Pipe[2][1]					// type -- pipe
	0x04,			//Pipe[2][2]					// ID -- data out
	0x00,			//Pipe[2][3]					// reserve
					//
	7,				//UasEndPointUSB2[3][0]			// Data-In Endpoint Descriptor length					// 74
	DESCTYPE_ENDPOINT,	//UasEndPointUSB3[3][1]		// Descriptor type - Endpoint						// 75
	0x81,
	0x02,			//UasEndPointUSB2[3][3]			// Endpoint type - Bulk							// 77
	0x00,			//UasEndPointUSB2[3][4]			// Maximun packet size 512						// 78
	0x02,			//UasEndPointUSB2[3][5]														// 79		
	0x00,			//UasEndPointUSB2[3][6]			// Polling interval
	//
	0x04,			//Pipe[3][0]					//pipe length									//78
	0x24,			//Pipe[3][1]					//type -- pipe
	0x03,			//Pipe[3][2]					//ID -- data in
	0x00			//Pipe[3][3]					//reserved,
// for align
	,0x00,00,00
#endif
};

	// USB 3.0 device descriptor
u8 const DeviceUSB3[] = {
	18,				//DeviceUSB3[0] = sizeof(DeviceUSB3);	// Descriptor length
	DESCTYPE_DEVICE,//DeviceUSB3[1] 			// Descriptor type - Device
	0x00,			//DeviceUSB3[2] 			// Specification Version (BCD)
	0x03,			//DeviceUSB3[3]				// inidcate USB 3.0/Supper speed compilant
	0x00,			//DeviceUSB3[4] 			// Device class
	0x00,			//DeviceUSB3[5] 			// Device sub-class
	0x00,			//DeviceUSB3[6] 			// Device sub-sub-class
	0x09,			//DeviceUSB3[7] 			// Maximum packet size(512B)
	0xFD,			//DeviceUSB3[8] 			// 2 bytes Vendor ID:
	0x13,			//DeviceUSB3[9] 			//
	0x80,			//DeviceUSB3[10]  			// 2 bytes Product ID:
	0x38,			//DeviceUSB3[11]  ;			//
	0x00,			//DeviceUSB3[12]  			// Product version ID
	0x00,			//DeviceUSB3[13] 
	IMFC,			//DeviceUSB3[14] 			// iManufacture=01 (index of string describe manufacture)
	IPRODUCT,		//DeviceUSB3[15]			// iProduct=02 (index of string describe product)
	ISERIAL,		//DeviceUSB3[16] 			// iSerial=03 (index of string describe dev serial #)
	0x01,			//DeviceUSB3[17] 			// Number of configurations
	0x00,0x00								//18-19 : pads
};

 // BOT+UAS: config +interface Descriptor+Two endpoints descriptors
u8 	const CfgUSB3[] = {
	9,				//CfgUSB3[0] = sizeof(CfgUSB3);											//0
	DESCTYPE_CFG,	//CfgUSB3[1]				//DESCTYPE_CFG
	9+9+7+6+7+6,	//CfgUSB3[2] = sizeof(CfgUSB3) +sizeof(BOTInfUSB3) +sizeof(BotEndPointUSB3) +2 * sizeof(BotSSEndPointComp); //total length of data return
					//9+9+7+6+7+6 +9+7+6+4 +7+6+4 +7+6+4 +7+6+4,	//CfgUSB3[2] = sizeof(CfgUSB3) + sizeof(BOTInfUSB3) + sizeof(BOTEndPointUSB3) +sizeof(BOTSSEndPointComp)+
					//							sizeof(UasInfUSB3) + sizeof(UasEndPointUSB3) +sizeof(Pipe) + sizeof(UasSSEndPointComp); //total length of data return
	0x00,			//CfgUSB3[3]
	0x01,			//CfgUSB3[4]				// Number of interfaces
	0x01,			//CfgUSB3[5]				// Configuration number
	ICONFIG,		//CfgUSB3[6]				// iConfiguration=00 (index of string describe config)
	0xC0,			//CfgUSB3[7]				// Attributes = 1100 0000 (bit7 res=1 and bit6 =self power)
	1,				//CfgUSB3[8]				// Power comsumption of the USB device from the bus (8mA)
#if 1//ndef UAS
	9,				//BOTInfUSB3[0] = sizeof(BOTInfUSB3);	// Descriptor length
	DESCTYPE_INF,	//BOTInfUSB3[1]				// Descriptor type - Interface						//9
	0x00,			//BOTInfUSB3[2]				// Zero-based index of this interface
	0x00,			//BOTInfUSB3[3]				// Alternate setting
	0x02,			//BOTInfUSB3[4]				// Number of end points
	0x08,			//BOTInfUSB3[5]				// MASS STORAGE Class.
	0x06,			//BOTInfUSB3[6]				// 06: SCSI transplant Command Set 
	0x50,			//BOTInfUSB3[7]				// BULK-ONLY TRANSPORT
	IINF,			//BOTInfUSB3[8]				// iInterface=00

	7,				//BotEndPointUSB3[0][0] = sizeof(BotEndPointUSB3[0]);	// Endpoint0  Descriptor length 18
	DESCTYPE_ENDPOINT,//	BotEndPointUSB3[0][1]	// Descriptor type - Endpoint
	0x81,			//BotEndPointUSB3[0][2]		// In Endpoint number 11
	0x02,			//BotEndPointUSB3[0][3]		// Endpoint type (bit0-1) - Bulk
	0x00,			//BotEndPointUSB3[0][4]		// 2 bytes Maximun packet size (bit0-10)
	0x04,			//BotEndPointUSB3[0][5]
	0x00,			//BotEndPointUSB3[0][6]		// Polling interval endpoint for data xfer

	6,				//BotSSEndPointComp[0]		//USB SS Endpoint Companion Descriptor							25						
	DESCTYPE_ENDPOINT_COMP,	//BotSSEndPointComp[1] = ;
#ifdef USB3_BURST_PACKETS_8
	0x07,			//BotSSEndPointComp[2]		// Max Burst: 7+1
#else
	0x03,			//BotSSEndPointComp[2]		// Max Burst: 3+1
#endif
	0x00,			//BotSSEndPointComp[3]		// Streaming is not supported.
	0x00,			//BotSSEndPointComp[4]		// Not a periodic endpoint.
	0x00,			//BotSSEndPointComp[5]

	7,				//BotEndPointUSB3[1][0] = sizeof(BotEndPointUSB3[0]);	// Descriptor length			31
	DESCTYPE_ENDPOINT,	//BotEndPointUSB3[1][1]	// Descriptor type - Endpoint
	0x02,			//BotEndPointUSB3[1][2] 	// Endpoint number 10 and direction OUT	
	0x02,			//BotEndPointUSB3[1][3] 	// Endpoint type - Bulk
	0x00,			//BotEndPointUSB3[1][4] 	// Maximun packet size
	0x04,			//BotEndPointUSB3[1][5] 
	0x00,			//BotEndPointUSB3[1][6] 	// Polling interval

	6,				//BotSSEndPointComp[0]  	//USB SS Endpoint Companion Descriptor							38
	DESCTYPE_ENDPOINT_COMP,	//BotSSEndPointComp[1]
#ifdef USB3_BURST_PACKETS_8
	0x07,			//BotSSEndPointComp[2] 		// Max Burst: 7+1
#else
	0x03,			//BotSSEndPointComp[2] 		// Max Burst: 3+1.
#endif
	0x00,			//BotSSEndPointComp[3] 
	0x00,			//BotSSEndPointComp[4] 		// Not a periodic endpoint.
	0x00,			//BotSSEndPointComp[5] 
#endif
#ifdef UAS
	9,				//UasInfUSB3[0] = sizeof(UasInfUSB3);	// uas interface Descriptor len			//44
	DESCTYPE_INF,	//UasInfUSB3[1] 			// Descriptor type - Interface
	0x00,			//UasInfUSB3[2] 			// Zero-based index of this interface
	0x01,			//UasInfUSB3[3] 			// Alternate setting
	0x04,			//UasInfUSB3[4] 			// Number of end points
	0x08,			//UasInfUSB3[5] 			// MASS STORAGE Class.
	0x06,			//UasInfUSB3[6] 			// 06: SCSI transplant Command Set 
	0x62,			//UasInfUSB3[7] 			// UAS
	IINF,			//UasInfUSB3[8] 			// iInterface=00
					//					xmemcpy(UasEndPointUSB3[0], (u8 *)OUTBUF+53, 7);
	7,				//UasEndPointUSB3[0][0] = sizeof(UasEndPointUSB3[0]);	//command out end point Desc len		// 53
	DESCTYPE_ENDPOINT,	//UasEndPointUSB3[0][1] 	// Descriptor type - Endpoint
	0x04,			//UasEndPointUSB3[0][2] 		// Out direction IN: (bit7=0 OUT and bit7=1 IN endpoint))
	0x02,			//UasEndPointUSB3[0][3] 		// bulk
	0x00,			//UasEndPointUSB3[0][4] 		// 2 bytes Maximun packet size (bit0-10)
	0x04,			//UasEndPointUSB3[0][5] 
	0x00,			//UasEndPointUSB3[0][6] 		// Polling interval endpoint for data xfer
	0x06,			//UasSSEndPointComp[0][0]		//USB SS Endpoint Companion Descriptor Len				//60
	DESCTYPE_ENDPOINT_COMP,	//UasSSEndPointComp[0][1] 
	0x00,			//UasSSEndPointComp[0][2] 		// Max_Burst
	0x00,			//UasSSEndPointComp[0][3] 		// Streaming is not supported.
	0x00,			//UasSSEndPointComp[0][4] 		// Not a periodic endpoint.
	0x00,			//UasSSEndPointComp[0][5] 
	0x04,			//Pipe[0][0] 					//Pipe length									//66
	0x24,			//Pipe[0][1] 					//type -- pipe
	0x01,			//Pipe[0][2] 					//ID -- command pipe
	0x00,			//Pipe[0][3] 					//reserved
					//
	7,				//UasEndPointUSB3[1][0] = 		// status pipe endpoint Descriptor len			//70
	DESCTYPE_ENDPOINT,	//UasEndPointUSB3[1][1]		// Descriptor type - Endpoint
	0x83,			//UasEndPointUSB3[1][2]
	0x02,			//UasEndPointUSB3[1][3]			// Endpoint type - Bulk
	0x00,			//UasEndPointUSB3[1][4]			// Maximun packet size 1024
	0x04,			//UasEndPointUSB3[1][5]
	0x00,			//UasEndPointUSB3[1][6]			// Polling interval
	0x06,			//UasSSEndPointComp[1][0]		// USB SS Endpoint Companion Descriptor Len		//77
	DESCTYPE_ENDPOINT_COMP,	//UasSSEndPointComp[1][1]
#ifdef USB3_BURST_PACKETS_8
	0x07,			//UasSSEndPointComp[1][2]		// Max Burst : 7+1
#else
	0x03,			//UasSSEndPointComp[1][2] 		// Max Burst : 3+1
#endif
	MAX_UAS_STREAM,			//UasSSEndPointComp[1][3] 		// Max Stream : 32
	0x00,			//UasSSEndPointComp[1][4] 		// Not a periodic endpoint.
	0x00,			//UasSSEndPointComp[1][5] 
	0x04,			//Pipe[1][0] 					// pipe length									//83
	0x24,			//Pipe[1][1] = ;				// type -- pipe
	0x02,			//Pipe[1][2] = ;				// ID -- status pipe
	0x00,			//Pipe[1][3] = ;				// reserved				
					//				xmemcpy(UasEndPointUSB3[2], (u8 *)OUTBUF+87, 7);
	7,				//UasEndPointUSB3[2][0]			// Data Out Endpoint Descriptor length			//87
	DESCTYPE_ENDPOINT,	//UasEndPointUSB3[2][1]		// Descriptor type - Endpoint
	0x02,	
	0x02,			//UasEndPointUSB3[2][3] 		// bulk
	0x00,			//UasEndPointUSB3[2][4] 		// Maximun packet size 1024
	0x04,			//UasEndPointUSB3[2][5] 
	0x00,			//UasEndPointUSB3[2][6]			// Polling interval
	0x06,			//UasSSEndPointComp[2][0]		// USB SS Endpoint Companion Descriptor Len		//94
	DESCTYPE_ENDPOINT_COMP,	//UasSSEndPointComp[2][1]
#ifdef USB3_BURST_PACKETS_8
	0x07,			//UasSSEndPointComp[2][2]		// Max Burst : 7+1
#else
	0x03,			//UasSSEndPointComp[2][2]		// Max Burst : 3+1
#endif
	MAX_UAS_STREAM,			//UasSSEndPointComp[2][3]		// Max Stream : 32
	0x00,			//UasSSEndPointComp[2][4]		// Not a periodic endpoint.
	0x00,			//UasSSEndPointComp[2][5]
	0x04,			//Pipe[2][0]					// pipe length									//100
	0x24,			//Pipe[2][1]					// type -- pipe
	0x04,			//Pipe[2][2]					// ID -- data out
	0x00,			//Pipe[2][3]					// reserve
					//
	7,				//UasEndPointUSB3[3][0]			// Data-In Endpoint Descriptor length			// 104
	DESCTYPE_ENDPOINT,	//UasEndPointUSB3[3][1]		// Descriptor type - Endpoint
	0x81,			
	0x02,			//UasEndPointUSB3[3][3]			// Endpoint type - Bulk
	0x00,			//UasEndPointUSB3[3][4]			// Maximun packet size 1024
	0x04,			//UasEndPointUSB3[3][5]
	0x00,			//UasEndPointUSB3[3][6]			// Polling interval
	0x06,			//UasSSEndPointComp[3][0]		//USB SS Endpoint Companion Descriptor Len		// 111
	DESCTYPE_ENDPOINT_COMP,	//UasSSEndPointComp[3][1] = ;
#ifdef USB3_BURST_PACKETS_8
	0x07,			//UasSSEndPointComp[3][2]		// Max Burst : 7+1
#else
	0x03,			//UasSSEndPointComp[3][2]		// Max Burst : 3+1
#endif
	MAX_UAS_STREAM,			//UasSSEndPointComp[3][3]		// Max Stream : 32
	0x00,			//UasSSEndPointComp[3][4]		// Not a periodic endpoint.
	0x00,			//UasSSEndPointComp[3][5]
	//
	0x04,			//Pipe[3][0]					//pipe length															//117
	0x24,			//Pipe[3][1]					//type -- pipe
	0x03,			//Pipe[3][2]					//ID -- data in
	0x00,			//Pipe[3][3]					//reserved
	0x00, 0x00, 0x00					// Pad																	// 121
#else
	0x00			//BotSSEndPointComp[5] 
#endif
};



// 	USB Binary Object Stor Descriptor
u8 const BOSS[] = {										//5+7+10
	5,					//BOSS[0] = sizeof(BOSS);
	DESCTYPE_BOSS,		//BOSS[1] = ;				// BOSS Descriptor Type
	5+7+10,				//BOSS[2] = sizeof(BOSS) + sizeof(USB2ExtendCapabilties)+sizeof(USB3Capabilties);	//total length of data return
	0,					//BOSS[3] = 0;
	2,					//BOSS[4] = 2;							// number of device Capabilties

	7,					//USB2ExtendCapabilties[0] = sizeof(USB2ExtendCapabilties);
	DESCTYPE_DEV_CAP,	//USB2ExtendCapabilties[1]  			// Device Capability Descriptor Type
	0x02,				//USB2ExtendCapabilties[2] 				//USB 2.0 Extension Capability
	0x02,				//USB2ExtendCapabilties[3] 				// Link Power Management Supported
	0x00,				//USB2ExtendCapabilties[4] 
	0x00,				//USB2ExtendCapabilties[5] 
	0x00,				//USB2ExtendCapabilties[6] 

	10,					//USB3Capabilties[0] = sizeof(USB3Capabilties);
	DESCTYPE_DEV_CAP,	//USB3Capabilties[1]  		// Device Capability Descriptor Type
	0x03,				//USB3Capabilties[2]  		// USB 3.0 Device Capability
	0x00,				//USB3Capabilties[3]  		// Does not support Latency Tolerance Messages
	0x0E,				//USB3Capabilties[4]		// USB 3.0 5Gb/s, USB 2.0 High and Full Speeds
	0x00,				//USB3Capabilties[5]  
	0x01,				//USB3Capabilties[6]		// All functionality supported down to USB 2.0 Full Speed
	0x0A,				//USB3Capabilties[7] 		// Less than 10us U1 Exit Latency
	0x80,				//USB3Capabilties[8] 		// Less than 128us U2 Exit Latency
	0x00,				//USB3Capabilties[9] 
	0x00,0x00								//22-23 : pads
};

/****************************************\
	Ep0_2SendStatus
\****************************************/
void Ep0_2SendStatus(void)
{

//	DBG("Ep0_2SendStatus\n");

	*usb_Ep0TxLengh = 0;

	// device ready to go to control status stage
	*usb_Ep0Ctrl = EP0_SRUN;
	*usb_Ep0CtrlClr =  EP0_STATE;
	u8 i = 0; 
	while(1)
	{
		*usb_Ep0CtrlClr =  EP0_STATE;
		if ((reg_r8(usb_Ep0Ctrl) & EP0_SRUN) == 0x00)
			break;
		if (*(usb_IntStatus) & (HOT_RESET|WARM_RESET|USB_BUS_RST))
			break;
		if (*(usb_Msc0IntStatus_shadow ) & BOT_RST_INT)
			break;
		if(!USB_VBUS()) return;
		if (++i == 200)
			break;
	}
	*usb_Ep0CtrlClr =  EP0_STATE;
//	DBG("end of ep0_2SendSatus!\n");
}


/****************************************\
	Ep0_2Send
\****************************************/
void Ep0_2Send(u8 * ptr, u32 len)
{

	//*usbDeviceStat = CTRL_DATA;
	*usb_Ep0CtrlClr = CTRL_DATA;
	*usb_Ep0TxLengh = (u16)len;
	*usb_Ep0BufLengh = (u16)len;
	
	xmemcpy(ptr, (u8 *)OUTBUF, len);

	//xfer data from data buffer to USB
	reg_w8(usb_Ep0Ctrl, EP0_RUN);
	while(1)
	{
		if ((reg_r8(usb_Ep0Ctrl) & EP0_RUN) == 0x00)
		{	
			break;
		}
		if (*(usb_IntStatus_shadow) & (HOT_RESET|WARM_RESET|USB_BUS_RST))
			return;
		if (*(usb_Msc0IntStatus_shadow) & BOT_RST_INT)
			return;
		if(!USB_VBUS()) return;
	}
	Ep0_2SendStatus();
}

/****************************************\
	Ep0_2Send_direct
\****************************************/
void Ep0_2Send_direct(u32 len)
{

	//*usbDeviceStat = CTRL_DATA;
	*usb_Ep0CtrlClr = CTRL_DATA;
	
	*usb_Ep0TxLengh = (u16)len;
	*usb_Ep0BufLengh = (u16)len;

	*usb_Ep0Ctrl |= 0x80;

	reg_w8(usb_Ep0Ctrl, EP0_RUN);
DBG("E0\t");
#if 1
	while(1)
	{
		if ((reg_r8(usb_Ep0Ctrl) & EP0_RUN) == 0x00)
		{
			break;
		}

		if (*(usb_IntStatus_shadow) & (HOT_RESET|WARM_RESET|USB_BUS_RST))
		{
			return;
		}
		if (*(usb_Msc0IntStatus_shadow) & BOT_RST_INT)
		{	
			return;
		}
		if(!USB_VBUS()) return;
	}
	Ep0_2SendStatus();
DBG("E1\n");
#endif
}

u8 usb_read_PortID(void)
{
	if ((inPktSetup[WLENGTH] & 0xFF) == 0x5A)
	{
		OUTBUF[0] = 0x25;
		OUTBUF[1] = 0xc9;
		OUTBUF[2] = 0x36;
		OUTBUF[3] = 0x10;

		OUTBUF[4] = USB_AdapterID;
		OUTBUF[5] = USB_PortID0;
		OUTBUF[6] = USB_PortID1;
		OUTBUF[7] = USB_PortID2;

		Ep0_2Send_direct(8);
		return 1;
	}
	return 0;
}

/****************************************\
	usb_control
\****************************************/
void usb_control()
{
	u32 tmp = 0;
DBG("Stp %bx, %bx\n", *inPktSetup, *(inPktSetup+1));
	*usb_Ep0CtrlClr = EP0_SETUP;	

	//REQTYPE_DIR(setup) ((((setup)[BMREQTYPE]) & 0x80) >> 7) where BMREQTYPE=00h-Offset
	//where inPktSetup->6038h - 603Fh (8 bytes setup packet)
	//byte0.7 = 0: host to device(OUT) or byte0.7 = 1: device to host(IN)
	//Is byte0.7 = 00/01?
	//(1)
	if (REQTYPE_DIR(inPktSetup) == REQTYPE_D2H)		//REQTYPE_D2H=01h
	{

DBG("D2H\n"); 

		//device to host
		//byte0.7 = 01 -> device to host:IN
		//byte 0: bit 5&6 - 0:standard, 1:class
		//(1.1) - standard
		switch (REQTYPE_TYPE(inPktSetup))
		{

		/****************************************\
			REQTYPE_STD
		\****************************************/
		case REQTYPE_STD:
			
//DBG("\tREQTYPE_STD\n");
			//Analyze byte 1 (bmRequestType): used USB spec. table9-3
			//used only 4:	BREQ_GET_STAT (0x00), BREQ_GET_DESC (0x06)
			//				BREQ_GET_CFG (0x08), BREQ_GET_INTERFACE	(0x0A)
			//(1.1.1)
			switch(inPktSetup[BREQ])	//BREQ=01h-Offset
			{
#ifdef SET_CLR_FEAT_FW
			/****************************************\
				BREQ_GET_STAT
			\****************************************/
			case BREQ_GET_STAT:    //(inPktSetup[BREQ=0x01])

//DBG("\tBREQ_GET_STAT\n");
				OUTBUF[0] = 0x00;
				OUTBUF[1] = 0x00;

				//REQTYPE_RECIP(setup) (((setup)[BMREQTYPE]) & 0x1F)
				//byte0.0-4 - 0:device, 1:interface, 2:endpoint, 3: other, 4...31: res
				switch(REQTYPE_RECIP(inPktSetup))
				{
				//(1.1.1.1) - REQTYPE_DEV=0x00 (standard request code Tbl 9-4)
				case REQTYPE_DEV:
					//DBG("\tREQTYPE_DEV\n");
					if ((product_detail.options & PO_IS_PORTABLE) == 0)
						OUTBUF[0] = 0x01;	  // self power
					if (*usb_DevStatus & U1_ENABLE)
						OUTBUF[0] |= BIT_2;
					if (*usb_DevStatus & U2_ENABLE)
						OUTBUF[0] |= BIT_3;
					Ep0_2Send_direct(2);
					return;

				//(1.1.1.2) - REQTYPE_INF=0x01 (descriptor type Tbl 9-5)
				case REQTYPE_INF:
//					DBG("\tREQTYPE_INF\n");
					Ep0_2Send_direct(2);
					return;

				//(1.1.1.3) - REQTYPE_EP=0x02 (standard feature selector Tbl 9-6)
				//byte4.7 defined direction	- (i) byte4.7=0 as OUT endpoint
				//							- (ii) byte4.7=1 as IN endpoint
				//and byte4.0-3 defined endpoint number
				case REQTYPE_EP:
					switch(inPktSetup[WINDEX] & 0x7F) //clr direction bit
					{
					//byte4.7=0 imply OUT endpoint w/specific endpoint 1
					case 0x00:
						if (*usb_Ep0IntEn & CTRL_HALT_STAT)
						{
							OUTBUF[0] = 0x01;
						}
						break;
					case 0x03: //status pipe
						if (*usb_Msc0StatCtrl & STATE_HALT)
							OUTBUF[0] = 0x01;
						break;
					case 0x04:// command pipe
						if (*usb_Msc0CmdCtrl & MSC_CMD_HALT)
							OUTBUF[0] = 0x01;
						 break;
					case 0x01: //bulk in
						if (*usb_Msc0DICtrl & MSC_DI_HALT)
							OUTBUF[0] = 0x01;
						break;
					//byte4.7=0 imply OUT endpoint w/specific endpoint 2
					case 0x02:
						if (*usb_Msc0DOutCtrl & MSC_DOUT_HALT)
							OUTBUF[0] = 0x01;
						break;
					default: break;
					}
					Ep0_2Send_direct(2);
					return;
				}
				break;
#endif

			/****************************************\
				BREQ_GET_DESC
			\****************************************/
			//(1.1.2) - continue (inPktSetup[BREQ]) where [BREQ=BREQ_GET_DESC=0x06]
			//		  - continue analyze byte 1 (bRequest): used USB spec. table9-3
			case BREQ_GET_DESC:
				tmp = *((u16 *)(inPktSetup + WLENGTH));
				//1.1.2.1 - byte 2: used to pass a para to device according to request
				//WVALUE=0x02 (offset of packet 8 bytes format)
				switch(inPktSetup[WVALUE])  //descriptor index - lbyte of wvalue
				{							//descriptor type - hbyte of wvalue
				//Tbl 9-8
				case 0x00:	//(inPktSetup[WVALUE=0x00])

//					DBG("\tValue: 0\n");
					//analyze hbyte of WVALUE -> descriptor type
					switch(inPktSetup[WVALUE + 1])
					{
					//1.1.2.1.1  device descriptor
					//byte6=WLENGTH indicate number of bytes to trasnfer
					case DESCTYPE_DEVICE:	//inPktSetup[WVALUE + 1=DESCTYPE_DEVICE=0x01]

						tmp = Min(tmp, DeviceUSB2[0]);
						if (*usb_DevStatus_shadow & USB3_MODE)
						{
							xmemcpy((u8 *)DeviceUSB3, (u8 *)OUTBUF, tmp);
						}
						else
						{
							xmemcpy((u8 *)DeviceUSB2, (u8 *)OUTBUF, tmp);
						}
#ifdef INITIO_STANDARD_FW
						OUTBUF[8] = (u8)globalNvram.USB_VID[1];	// 2 bytes Vendor ID:
						OUTBUF[9] = (u8)globalNvram.USB_VID[0];	//
						OUTBUF[10] = (u8)globalNvram.USB_PID[1];	// 2 bytes Product ID:
						OUTBUF[11] = (u8)globalNvram.USB_PID[0];	//
#else
						OUTBUF[8] = (u8)product_detail.USB_VID;	// 2 bytes Vendor ID:
						OUTBUF[9] = (u8)(product_detail.USB_VID >> 8);	//
						OUTBUF[10] = (u8)product_detail.USB_PID;	// 2 bytes Product ID:
						OUTBUF[11] = (u8)(product_detail.USB_PID >> 8);	//
#endif		
						*((u16 *)&OUTBUF[12]) = firmware_version;		// Product version ID
						
						Ep0_2Send_direct(tmp);
						return;

					//1.1.2.1.2 get config descriptor
					case DESCTYPE_CFG:	//inPktSetup[WVALUE + 1=DESCTYPE_CFG=0x02]
					
//DBG("\tDESCTYPE_CFG\n");
						if (*usb_DevStatus_shadow & USB3_MODE)
						{
							DBG("U3\n");
							if (ncq_supported)
								tmp = Min(tmp, 121);
							else
								tmp = Min(tmp, CfgUSB3[2]);	
							xmemcpy((u8 *)CfgUSB3, (u8 *)OUTBUF, tmp);
#ifdef UAS
							if (ncq_supported)
							{
								OUTBUF[2] = 9 + 9 + 7 + 6 + 7 + 6 + 9 + 7 + 6 + 4 + 7 + 6 + 4 + 7 + 6 + 4 + 7 + 6 + 4;//9+9+7+6+4 +7+6+4 +7+6+4 +7+6+4;
									//sizeof(BOTInfUSB3) + sizeof(BOTEndPointUSB3) +sizeof(BOTSSEndPointComp)+
									//sizeof(InfUSB3) + sizeof(UasEndPointUSB3) +
									//sizeof(Pipe) + sizeof(UasSSEndPointComp); //total length of data return
							}
#endif	

							if (product_detail.options & PO_IS_PORTABLE)
							{
								OUTBUF[7] = 0x80;					// Attributes = 1100 0000 (bit7 reserved=1 and bit6 =self power)
								OUTBUF[8] = 112;					// Power comsumption of the USB device from the bus (896mA)
							}

							
#if 0	// there's no unconfigured current in NEC & Microsoft's usb3 driver						
							if (!(*usb_DevState & USB_CONFIGURED))
							{
								// Is Bus Power
								if (!(OUTBUF[7] & 0x40))
								{	// Yes, set to 150 ma
									OUTBUF[8] = (150 >> 3);
								}
							}	
#endif						
						}
						else
						{
							DBG("\tNOT USB3 MODE\n");
#ifdef UAS
							if (usbMode == CONNECT_USB1)
								tmp = Min(tmp, 32); //support BOT only 9+9+7+7
							else
#endif
							{
								if (ncq_supported)
									tmp = Min(tmp, 85);
								else
									tmp = Min(tmp, CfgUSB2[2]);
							}
							if (tmp > 0)
							{
								xmemcpy((u8 *)CfgUSB2, (u8 *)OUTBUF, tmp);
#ifdef UAS
								if (ncq_supported)
									OUTBUF[2] = 9 +9 + 7 + 7 + 9 + 28 + 16; //9 +9+28+16; 
#endif

								if (product_detail.options & PO_IS_PORTABLE)
								{
									OUTBUF[7] = 0x80;	// Attributes = 1100 0000 (bit7 res=1 and bit6 =self power)
									OUTBUF[8] = 250;					// Power comsumption of the USB device from the bus (2mA)
								}

#ifdef UAS
								if (usbMode == CONNECT_USB1)
									OUTBUF[2] = 32;
#endif
							
								if (!(*usb_DevState_shadow & USB_CONFIGURED))
								{
									// Is Bus Power
									if (!(OUTBUF[7] & 0x40))
									{	// Yes, set to 100 ma
										OUTBUF[8] = (100 >> 1);
									}
								}
								if ((*usb_DevStatus & USB2_HS_MODE) == 0)	//full speed
								{
									
									OUTBUF[22] = 0x40;
									OUTBUF[23] = 0x00;
									OUTBUF[29] = 0x40;
									OUTBUF[30] = 0x00;
									
									OUTBUF[45] = 0x40;
									OUTBUF[46] = 0x00;
									OUTBUF[56] = 0x40;
									OUTBUF[57] = 0x00;
									
									OUTBUF[67] = 0x40;
									OUTBUF[68] = 0x00;
									OUTBUF[78] = 0x40;
									OUTBUF[79] = 0x00;
								}							
							}
						}
						Ep0_2Send_direct(tmp);
						return;

					//1.1.2.1.3  get string descriptor
					case DESCTYPE_STR:	//inPktSetup[WVALUE + 1=DESCTYPE_STR=0x03]
						if (usb_read_PortID())	return;
						tmp = Min(tmp, sizeof(desc.Str));
						Ep0_2Send(desc.Str, tmp);
						return;

					//1.1.2.1.4 get device descriptor
					case DESCTYPE_DEV_QUAL:			// USB2.0 only
						if (*usb_DevStatus & USB2_MODE)
						{
//							DBG("\tUSB20 Mode\n");
							xmemcpy((u8 *)DeviceUSB2, (u8 *)OUTBUF, 10);
							OUTBUF[0] = 10;
							OUTBUF[1] = DESCTYPE_DEV_QUAL;
							OUTBUF[8] = DeviceUSB2[17];
							OUTBUF[9] = 0x00;
							tmp = Min(tmp, 10);
							Ep0_2Send_direct(tmp);
							return;
						}
						break;

					//1.1.2.1.5 get config descriptor
					//inPktSetup[WVALUE + 1=DESCTYPE_OTHER_SPD_CFG=0x07]
					case DESCTYPE_OTHER_SPD_CFG:
						if (*usb_DevStatus & USB2_MODE)
						{
//							DBG("\tUSB20 Mode\n");
#ifdef UAS
							if (usbMode == CONNECT_USB1)
							{
								tmp = Min(tmp, 32); //support BOT only 9+9+7+7
							}
							else
#endif							
							{
								if (ncq_supported)
									tmp = Min(tmp, 85);
								else
									tmp = Min(tmp, CfgUSB2[2]);
							}
							if (tmp > 0)
							{
//								DBG("\ttmp > 0\n");
								xmemcpy((u8 *)CfgUSB2, (u8 *)OUTBUF, tmp);
#ifdef UAS
								if (ncq_supported)
									OUTBUF[2] = 9 +9 + 7 + 7 + 9 + 28 + 16; //9 +9+28+16; 
								if (usbMode == CONNECT_USB1)
									OUTBUF[2] = 32;
#endif

								OUTBUF[1] = DESCTYPE_OTHER_SPD_CFG;

								if (!(*usb_DevState_shadow & USB_CONFIGURED))
								{
									// Is Bus Power
									if (!(OUTBUF[7] & 0x40))
									{	// Yes, set to 100 ma
										OUTBUF[8] = (100 >> 1);
									}

								}
								if (*usb_DevStatus & USB2_HS_MODE)
								{
									OUTBUF[22] = 0x40;
									OUTBUF[23] = 0x00;
									OUTBUF[29] = 0x40;
									OUTBUF[30] = 0x00;
									
									OUTBUF[45] = 0x40;
									OUTBUF[46] = 0x00;
									OUTBUF[56] = 0x40;
									OUTBUF[57] = 0x00;
									
									OUTBUF[67] = 0x40;
									OUTBUF[68] = 0x00;
									OUTBUF[78] = 0x40;
									OUTBUF[79] = 0x00;
								}							
							}
							Ep0_2Send_direct(tmp);
							return;
						}
						break;

					case DESCTYPE_ENDPOINT_COMP:			// USB3.0 only
						//get endpoint descriptor
						if (*usb_DevStatus_shadow & USB3_MODE)
						{
//							DBG("\tUSB30\n");
							xmemcpy((u8 *)(&CfgUSB3[24]), (u8 *)OUTBUF, 6);
							tmp = Min(tmp, 6);
							Ep0_2Send_direct(tmp);
							return;
						}
						break;

					case DESCTYPE_BOSS:			// USB3.0 only
						//get BOSS descriptor
						tmp = Min(tmp, BOSS[2]);

						xmemcpy((u8 *)BOSS, (u8 *)OUTBUF, tmp);
						Ep0_2Send_direct(tmp);
						return;

					}	// end of case 0x00:	//(inPktSetup[WVALUE=0x00])


				case IMFC:	//(inPktSetup[WVALUE=0x01])
					switch(inPktSetup[WVALUE + 1]) //descriptor type
					{
					case DESCTYPE_STR:
						{
							if (usb_read_PortID())	return;
//							DBG("\tDESCTYPE_STR\n");
							tmp = Min(tmp, desc.Mfc[0]);
							Ep0_2Send(desc.Mfc, tmp);
							return;
						}
					}
					break;

				case IPRODUCT:	//(inPktSetup[WVALUE=0x02])

//DBG("\tIPRODUCT\n");
					switch(inPktSetup[WVALUE + 1]) //descriptor type
					{
					case DESCTYPE_STR:
						{
//DBG("\tDESCTYPE_STR\n");
						
							tmp = Min(tmp, desc.Product[0]);
							Ep0_2Send(desc.Product, tmp);
							return;
						}
					}
					break;

				case ISERIAL:	//(inPktSetup[WVALUE=0x03])

					switch(inPktSetup[WVALUE + 1]) //descriptor type
					{
					case DESCTYPE_STR:
						{
//							DBG("\tDESCTYPE_STR\n");
							tmp = Min(tmp, desc.Serial[0]);

							Ep0_2Send(desc.Serial, tmp);
							return;
						}
					}
					break;

				default:
					break;				
				}				
				break;	// End of switch(inPktSetup[WVALUE])
			}	// End of switch(inPktSetup[BREQ])
			break;
		}	// End of switch (REQTYPE_TYPE(inPktSetup))
	 }	// End device to host
	 else	// REQTYPE_H2D	 
	 {

		DBG("H2D\n"); 
		
		//host to device
		switch (REQTYPE_TYPE(inPktSetup))
		{
		/****************************************\
			REQTYPE_STD
		\****************************************/
		case REQTYPE_STD:
			switch(inPktSetup[BREQ])
			{

			/****************************************\
				BREQ_SET_FEATURE
			\****************************************/
			case BREQ_SET_FEATURE:
				MSG("SF\n");
//				DBG("\tBREQ_SET_FEATURE\n");
				switch(REQTYPE_RECIP(inPktSetup))
				{
				case REQTYPE_DEV:

//					DBG("\tREQTYPE_DEV\n");
					if (inPktSetup[WVALUE] == FEAT_SEL_TEST_MODE)
					{
//						DBG("\tFEAT_SEL_TEST_MODE\n");
						switch(inPktSetup[WINDEX + 1])
						{
						case TEST_J:
//							DBG("\tTEST_J\n");
							Ep0_2SendStatus();
							reg_w8(usb_USB2TestMode, TM_TEST_J);
							while(1)
							{
								Delay(1);
								if (USB_VBUS_OFF())
									return;
							}

						case TEST_K:
//							DBG("\tTEST_K\n");
							Ep0_2SendStatus();
							reg_w8(usb_USB2TestMode, TM_TEST_K);
							while(1)
							{
								Delay(1);
								if (USB_VBUS_OFF())
									return;
							}

						case TEST_SE0_NAK:
							//dont set any run bits
//							DBG("\tTEST_SE0_NAK\n");
							Ep0_2SendStatus();
							reg_w8(usb_USB2TestMode, TM_TEST_SE0_NAK);
							while(1)
							{
								Delay(1);
								if (USB_VBUS_OFF())
									return;
							}

						case TEST_PACKET:
						{
#if 0
							static const u32 test_data[] = 
										{	0x00000000, 0x00000000, 0xAAAAAA00, 0xAAAAAAAA, 0xEEEEEEAA, 0xEEEEEEEE, 0xFFFFFEEE, 0xFFFFFFFF,
											0xFFFFFFFF, 0xDFBF7FFF, 0xFDFBF7EF, 0xDFBF7EFC,	0xFDFBF7EF
										};
#endif

							MSG("TsPK\n");

							Ep0_2SendStatus();
#if 0 // it's for the patch code, finally it should be fixed by HW 
							_disable();
							spi_phy_wr_retry(PHY_SPI_U2, 0x00, 0x81);
							*usb_USB2TestMode = TM_TEST_PACKET;
							u8 tmp8 = spi_phy_rd(PHY_SPI_U2, 07) | 0x04;
							spi_phy_wr_retry(PHY_SPI_U2, 07, tmp8);
							_enable();
							Delay(5000);
							DBG("Start test\n");
#endif

							*usb_Ep0CtrlClr = CTRL_DATA;

							*usb_Ep0TxLengh = 53;
							*usb_Ep0BufLengh = 53;
							OUTBUF[0] = 00;
							OUTBUF[1] = 00;
							OUTBUF[2] = 00;
							OUTBUF[3] = 01;
							OUTBUF[4] = 0xC3;
							xmemset((u8 *)(&OUTBUF[5]), 00, 9);
							OUTBUF[14] = 0x7F;
							xmemset((u8 *)(&OUTBUF[15]), 0xFF, 11);
							OUTBUF[26] = 0xBF;
							OUTBUF[27] = 0x7E;
							OUTBUF[28] = 0xFD;
							OUTBUF[29] = 0xFB;
							OUTBUF[30] = 0xF7;
							OUTBUF[31] = 0xEF;
							OUTBUF[32] = 0xDF;
							OUTBUF[33] = 0x9F;
							OUTBUF[34] = 0xBF;
							OUTBUF[35] = 0x7E;
							OUTBUF[36] = 0xFD;
							OUTBUF[37] = 0xFB;
							OUTBUF[38] = 0xF7;
							OUTBUF[39] = 0xEF;
							OUTBUF[40] = 0xBF;
							OUTBUF[41] = 0x7E;
							OUTBUF[42] = 0x6D;
							OUTBUF[43] = 0x73;
							OUTBUF[44] = 0x7F;
							for (u8 i = 45; i < 55; i++)
								OUTBUF[i] = i;
							
							*usb_USB2TestMode = TM_TEST_PACKET;
							*usb_Ep0Ctrl = EP0_RUN;
							while(1)
							{
								Delay(1);
								if (USB_VBUS_OFF())
									return;
							}
						}
						
						case TEST_FORCE_ENABLE:
//							DBG("\tTEST_FORCE_ENABLE\n");
							Ep0_2SendStatus();
							break;

							
						}
					}
#ifdef SET_CLR_FEAT_FW
					else if ((inPktSetup[WVALUE] == FEAT_U1_ENABLE) || (inPktSetup[WVALUE] == FEAT_U2_ENABLE))
					{
						// set feature U1 U2 enable 
						DBG("Set Feat U %bx\n", *usb_DevState_shadow);
						if ((*usb_DevState_shadow & USB_CONFIGURED) == 0)
							goto _REQUEST_ERROR;
						Ep0_2SendStatus();
						if (inPktSetup[WVALUE] == FEAT_U1_ENABLE)
						{
							DBG("1\n");
							*usb_DevStatus |= U1_ENABLE;
						}
						else
						{
							DBG("2\n");
							*usb_DevStatus |= U2_ENABLE;
						}
						return;
					}
					break;

				case REQTYPE_EP:
					
//					DBG("\tREQTYPE_EP\n");
					Ep0_2SendStatus();
					//ep in inPktSetup[WINDEX]
					if (inPktSetup[WVALUE] == 0)
					{
						DBG("set halt %bx\n", inPktSetup[WINDEX]);
						switch(inPktSetup[WINDEX] & 0x7F) //clr direction bit
						{			
						case 0x00:			// control pipe
							*usb_Ep0IntEn |= CTRL_HALT_STAT;
							return;
						case 0x01:			// Bulk-In
	//						DBG("\tBulk-In\n");
							*usb_Msc0DICtrl = MSC_DI_HALT;
							return;
						case 0x02:			// Bulk-out
	//						DBG("\tBulk-Out\n");
							*usb_Msc0DOutCtrl = MSC_DOUT_HALT;
							return;
						case 0x04:			// command pipe
							*usb_Msc0CmdCtrl = MSC_CMD_HALT;
							return;
						case 0x03:			// status pipe
							*usb_Msc0StatCtrl = STATE_HALT;
							return;
						}
					}
					break;	

				case REQTYPE_INF:
					if (inPktSetup[WVALUE] == 0)
					{// function suspend function
						*usb_Msc0Ctrl |= MSC_FUNC_SUSPEND;
						Ep0_2SendStatus();
						MSG("func susp\n");
						return;
					}
#endif
				}
				break;
#ifdef SET_CLR_FEAT_FW
			case BREQ_CLR_FEATURE:
				DBG("CLR FEAT %bx\n", inPktSetup[WINDEX]);
				switch(REQTYPE_RECIP(inPktSetup))
				{

				case REQTYPE_DEV:
					if ((inPktSetup[WVALUE] == FEAT_U1_ENABLE) || (inPktSetup[WVALUE] == FEAT_U2_ENABLE))
					{
						// clear feature U1 U2 enable
						DBG("U");
						if (inPktSetup[WVALUE] == FEAT_U1_ENABLE)
						{
							DBG("1\n");
							*usb_DevStatus &= ~U1_ENABLE;
						}
						else
						{
							DBG("2\n");
							*usb_DevStatus &= ~U2_ENABLE;
						}						
					}
					else
						goto _REQUEST_ERROR;
					break;
				case REQTYPE_INF:
					if (inPktSetup[WVALUE] == 0)
					{// function suspend function
						*usb_Msc0Ctrl &= ~MSC_FUNC_SUSPEND;
						MSG("func susp res\n");
						break;
					}	
					else
						goto _REQUEST_ERROR;
					
				case REQTYPE_EP:
#ifdef UAS_CLEAR_FEAT_ABORT
					uas_abort_cmd_flag = 0;
#endif
					switch(inPktSetup[WINDEX]) //clr direction bit
					{	
					case 0x00:
						*usb_Ep0IntEn &= ~CTRL_HALT_STAT;
						break;
						
					case 0x81: // DATAIN Clear feature
						*usb_MSC0_CLRFEAT_halt = MSC_DATAIN_CLRFEAT_HALT; 
#ifdef WIN8_UAS_PATCH
						if (intel_SeqNum_monitor & INTEL_SEQNUM_CHECK_CONDITION)
						{
							intel_host_flag = 1;
							intel_SeqNum_Monitor_Count = 0;
							intel_SeqNum_monitor = 0;
							MSG("ITL h\n");
						}
#endif
#ifdef UAS_CLEAR_FEAT_ABORT
						if (curMscMode == MODE_UAS)
						{
							DBG("CLR FEAT HALT DI\n");
							uas_abort_cmd_flag = UAS_ABORT_BY_CLEAR_FEATURE_DIN_HALT;
						}
#endif
						break;
						
					case 0x02: // DATAOUT Clear feature
#ifdef WIN8_UAS_PATCH
						if (intel_SeqNum_monitor & INTEL_SEQNUM_CHECK_CONDITION)
						{
							intel_host_flag = 1;
							intel_SeqNum_Monitor_Count = 0;
							intel_SeqNum_monitor = 0;
							MSG("ITL h\n");
						}
#endif
#ifdef UAS_CLEAR_FEAT_ABORT
						if (curMscMode == MODE_UAS)
						{
							DBG("CLR FEAT HALT DO\n");
							uas_abort_cmd_flag = UAS_ABORT_BY_CLEAR_FEATURE_DOUT_HALT;
						}
#endif
						*usb_MSC0_CLRFEAT_halt |= MSC_DATAOUT_CLRFEAT_HALT; 
						break;
						
					case 0x04: // CMD EP clear feature
						*usb_MSC0_CLRFEAT_halt |= MSC_CMD_CLRFEAT_HALT; 
						break;
						
					case 0x83: // STATUS EP clear feature 
						*usb_MSC0_CLRFEAT_halt |= MSC_STAT_CLRFEAT_HALT; 
						break;
						
					default:
						goto _REQUEST_ERROR;
						
					}
				}
				Ep0_2SendStatus();
#ifdef UAS_CLEAR_FEAT_ABORT
				if (uas_abort_cmd_flag)
				{
					uas_abort_cmd_by_clr_feature(uas_abort_cmd_flag);
				}
#endif
				return;
#endif

#ifdef UAS
			case BREQ_SET_INTERFACE:
				MSG(("IF "));
				if ((*usb_DevState & USB_CONFIGURED) == 0)
					break;
				
				//get Alternate Setting
				tmp = inPktSetup[2];
				if (tmp > 1)
				{
					break;
				}
				else if (tmp == 1)
				{	// UAS
					MSG(("UAS\n"));
					*usb_CtxtSize = 0xC000 | MAX_UAS_CTXT_SITE; // reset the ctxt memory
					*usb_Msc0Ctrl |= MSC_ALTERNATE;
					*usb_Msc0Ctrl |= MSC_SET_INTFC_RESET;
					tmp = SATA_NCQ_CMD;
					if (curMscMode == MODE_BOT)
					{
						mscInterfaceChange = 1;
					}
					curMscMode = MODE_UAS;
				}
				else //if (tmp8 == 0)
				{	// BOT
					MSG(("BOT\n"));
					*usb_Msc0Ctrl = *usb_Msc0Ctrl & (~MSC_ALTERNATE);
					*usb_Msc0Ctrl |= MSC_SET_INTFC_RESET;
					tmp = SATA_DMAE_CMD;
					curMscMode = MODE_BOT;
				}
				*usb_Msc0LunCtrl = (*usb_Msc0LunCtrl & ~0x70); // LUN0
				*usb_Msc0LunSATCtrl = (*usb_Msc0LunSATCtrl & ~SAT_CMD) | tmp;
				if (HDD1_LUN != UNSUPPORTED_LUN)
				{
					// set the SAT transfer for HDD1 lun
					*usb_Msc0LunCtrl = (*usb_Msc0LunCtrl & ~0x70) | 0x10; // set the LUN ctrl index to 1
					*usb_Msc0LunSATCtrl = (*usb_Msc0LunSATCtrl & ~SAT_CMD) | tmp;
				}				
				Ep0_2SendStatus();
				return;
#endif

			}
			break;

		/****************************************\
			REQTYPE_CLASS
		\****************************************/
		case REQTYPE_CLASS:

//			DBG("\tREQTYPE_CLASS\n");
			switch(inPktSetup[BREQ])
			{
			case BREQ_SET_IDLE:		// 0x0A:
//				DBG("\tBREQ_SET_IDLE\n");
				//what is this?
				Ep0_2SendStatus();
				return;
			}
			break;

		} // End of switch (REQTYPE_TYPE(inPktSetup))

	}	// End of REQTYPE_DIR(inPktSetup) D2H and H2D Case


//	DBG("unsupport SETUP packet\n");
_REQUEST_ERROR:
	MSG("REQ ERR\n");
	//unsupport SETUP packet
	reg_w8(usb_Ep0Ctrl, EP0_HALT);	
}

/****************************************\
	Text16
\****************************************/
void Text16(u8 *dest, u8 *src, u32 len)
{
	u32	i;

	for (i = 0; i < len; i++)
	{
		*dest++ = *src++;
		*dest++ = 0x00;
	}
}

/****************************************\
	Hex2Text16
\****************************************/
void Hex2Text16(u8 *dest, u8 *src, u32 len)
{
	u32	i;

	for (i = 0; i < len; i++)
	{
		u8 val;

		val = *src >> 4;
		if (val >= 0x0A)
			*dest = 'A' - 10 + val;
		else
			*dest = '0' + val;

		dest++;
		*dest++ = 0x00;

		val = *src & 0x0f;
		if (val >= 0x0A)
			*dest = 'A' - 10 + val;
		else
			*dest = '0' + val;

		dest++;
		*dest++ = 0x00;

		src++;
	}
}

/****************************************\
	InitDesc
\****************************************/
void InitDesc(void)			//DESCBUF xdata desc
{
	//USB2.1 config descriptor


	//interface descriptor
	//BotInfUSB2[0] = sizeof(InfUSB2);		// Descriptor length
	//BotInfUSB2[1] = DESCTYPE_INF;			// Descriptor type - Interface
	//BotInfUSB2[2] = 0x00;					// Zero-based index of this interface
	//BotInfUSB2[3] = 0x00;					// Alternate setting
	//BotInfUSB2[4] = 0x02;					// Number of end points
	//BotInfUSB2[5] = 0x08;					// MASS STORAGE Class.
	//BotInfUSB2[6] = 0x06;					// 06: SCSI transplant Command Set 
	//BotInfUSB2[7] = 0x50;					// BULK-ONLY TRANSPORT
	//BotInfUSB2[8] = IINF;					//iInterface=00

	//EndPointUSB2[0][0] = sizeof(EndPointUSB2[0]);	// Descriptor length
	//EndPointUSB2[0][1] = DESCTYPE_ENDPOINT;	// Descriptor type - Endpoint
	//EndPointUSB2[0][2] = 0x8B;			// Endpoint number 11
										// direction IN: (bit7=0 OUT and bit7=1 IN endpoint))
	//EndPointUSB2[0][3] = 0x02;			// Endpoint type (bit0-1) - Bulk
	//EndPointUSB2[0][4] = 0x00;			// 2 bytes Maximun packet size (bit0-10)
	//EndPointUSB2[0][5] = 0x02;
	//EndPointUSB2[0][6] = 0x00;			// Polling interval endpoint for data xfer

	//EndPointUSB2[1][0] = sizeof(EndPointUSB2[0]);	// Descriptor length
	//EndPointUSB2[1][1] = DESCTYPE_ENDPOINT;	// Descriptor type - Endpoint
	//EndPointUSB2[1][2] = 0x0A;			// Endpoint number 10 and direction OUT
	//EndPointUSB2[1][3] = 0x02;			// Endpoint type - Bulk
	//EndPointUSB2[1][4] = 0x00;			// Maximun packet size
	//EndPointUSB2[1][5] = 0x02;
	//EndPointUSB2[1][6] = 0x00;			// Polling interval

	// USB 3.0 device descriptor

	//USB3.0 device descriptor
	
	desc.Str[0] = sizeof(desc.Str);
	desc.Str[1] = DESCTYPE_STR;
	desc.Str[2] = 0x09;
	desc.Str[3] = 0x04;

#ifdef INITIO_STANDARD_FW
	u8 tmp_len = globalNvram.vendorStrLength;
	desc.Mfc[0] = 2 + tmp_len * 2;
	desc.Mfc[1] = DESCTYPE_STR;
//	CrSetVendorText(Module_Vendor_Text);
	Text16(desc.Mfc + 2, globalNvram.vendorText, tmp_len);

	tmp_len = globalNvram.modelStrLength;
	desc.Product[0] = 2 + tmp_len * 2;
	desc.Product[1] = DESCTYPE_STR;
//	CrSetModelText(Model_ID_Text);
	Text16(desc.Product + 2, globalNvram.modelText, tmp_len);
#else
	u8 tmp_len = codestrlen((u8 *)product_detail.vendor_USB_name);
	desc.Mfc[0] = 2 + tmp_len * 2;
	desc.Mfc[1] = DESCTYPE_STR;
//	CrSetVendorText(Module_Vendor_Text);
	Text16(desc.Mfc + 2, (u8 *)product_detail.vendor_USB_name, tmp_len);

	u8 add_pid_len = 5;
	if ((product_detail.options & PO_ADD_PID_TO_NAME) == 0)
		add_pid_len = 0;
	tmp_len = codestrlen((u8 *)product_detail.product_name) + add_pid_len;
	desc.Product[0] = 2 + tmp_len * 2;
	desc.Product[1] = DESCTYPE_STR;
//	CrSetModelText(Model_ID_Text);
	Text16(desc.Product + 2, (u8 *)product_detail.product_name, tmp_len -add_pid_len);

	if (product_detail.options & PO_ADD_PID_TO_NAME)
	{
		u16 pid = product_detail.USB_PID;
		u8 temp[5];
		temp[0] = 0x20;
		temp[1] = Hex2Ascii(pid >> 12);
		temp[2] = Hex2Ascii(pid >>  8);
		temp[3] = Hex2Ascii(pid >>  4);
		temp[4] = Hex2Ascii(pid);
		Text16(desc.Product + 2 + 2 * (tmp_len -5), temp, 5);
	}
#endif
	
	if (product_model == ILLEGAL_BOOT_UP_PRODUCT)
		xmemset(unit_serial_num_str, 0x20, 20);

	fill_variable_len_usn(globalNvram.iSerial);
	
	{
		u8 * addr;
		u32	i;

		desc.Serial[0] = sizeof(desc.Serial);
		desc.Serial[1] = DESCTYPE_STR;
		addr = &desc.Serial[2];

		for (i = 0; i < 20; i++)
		{
			u8 serialChar;
			u8 val;

			// the following lines takes care of byte-swap
			val = unit_serial_num_str[i];

			serialChar = (val >> 4) + '0';
			if (serialChar > '9')
				serialChar += 'A' - '9' - 1;
			*addr = serialChar;
			addr++;
			*addr = 0;
			addr++;

			serialChar = (val & 0x0F) + '0';
			if (serialChar > '9')
				serialChar += 'A' - '9' - 1;
			*addr = serialChar;
			addr++;
			*addr = 0;
			addr++;
		} // for
	}
}


