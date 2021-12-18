/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/
#if !defined(ANT_DLL_H)
#define ANT_DLL_H

#include "types.h"

#if defined(__cplusplus)
   extern "C" {
#endif


//////////////////////////////////////////////////////////////////////////////////
// Public Definitions
//////////////////////////////////////////////////////////////////////////////////

// Application callback function pointers
typedef BOOL (*RESPONSE_FUNC)(UCHAR ucChannel, UCHAR ucResponseMsgID);
typedef BOOL (*CHANNEL_EVENT_FUNC)(UCHAR ucChannel, UCHAR ucEvent);
typedef BOOL (*P_ANT_INIT)(UCHAR, USHORT);
typedef BOOL (*P_ANT_INIT_EXT)(UCHAR, USHORT, UCHAR, UCHAR);
typedef BOOL (*P_ANT_GETDEVICEUSBINFO)(UCHAR ,UCHAR*, UCHAR*);
typedef BOOL (*P_ANT_GETDEVICEUSBPID)(USHORT* pusPID_);
typedef BOOL (*P_ANT_GETDEVICEUSBVID)(USHORT* pusVID_);
typedef BOOL (*P_ANT_CLOSE)();
typedef void (*P_ANT_ARF)(RESPONSE_FUNC, UCHAR*);
typedef void (*P_ANT_AEF)(UCHAR, CHANNEL_EVENT_FUNC, UCHAR*);
typedef BOOL (*P_ANT_UNASSIGN)(UCHAR);
typedef BOOL (*P_ANT_ASSIGN)(UCHAR, UCHAR, UCHAR);
typedef BOOL (*P_ANT_ASSIGN_EXT)(UCHAR, UCHAR, UCHAR, UCHAR);
typedef BOOL (*P_ANT_SETID)(UCHAR, USHORT, UCHAR, UCHAR);
typedef BOOL (*P_ANT_SETPER)(UCHAR, USHORT);
typedef BOOL (*P_ANT_SETTIME)(UCHAR, UCHAR);
typedef BOOL (*P_ANT_SETFREQ)(UCHAR, UCHAR);
typedef BOOL (*P_ANT_SETKEY)(UCHAR, UCHAR*);
typedef BOOL (*P_ANT_SETPWR)(UCHAR);
typedef BOOL (*P_ANT_RST)(void);
typedef BOOL (*P_ANT_OPENCHNL)(UCHAR);
typedef BOOL (*P_ANT_CLOSECHNL)(UCHAR);
typedef BOOL (*P_ANT_REQMSG)(UCHAR, UCHAR);
typedef BOOL (*P_ANT_ENABLEEXT)(UCHAR);
typedef BOOL (*P_ANT_TX)(UCHAR, UCHAR*);
typedef BOOL (*P_ANT_TXA)(UCHAR, UCHAR*);
typedef BOOL (*P_ANT_TXB)(UCHAR, UCHAR*, USHORT);
typedef BOOL (*P_ANT_ADDID)(UCHAR, USHORT, UCHAR, UCHAR, UCHAR);
typedef BOOL (*P_ANT_CONFIGLIST)(UCHAR, UCHAR, UCHAR);
typedef BOOL (*P_ANT_CHANTXPWR)(UCHAR, UCHAR);
typedef BOOL (*P_ANT_LOWPRITIME)(UCHAR, UCHAR);
typedef BOOL (*P_ANT_SERIALID)(UCHAR, UCHAR, UCHAR);
typedef BOOL (*P_ANT_SETLED)(UCHAR ucEnable_);
typedef BOOL (*P_ANT_SETCRYSTAL)(void);
typedef BOOL (*P_ANT_SETAGILITY)(UCHAR, UCHAR, UCHAR, UCHAR);
typedef BOOL (*P_ANT_SETPROX)(UCHAR, UCHAR);
typedef BOOL (*P_ANT_SETSCAN)(void);
typedef BOOL (*P_ANT_SETSLEEP)(void);
typedef BOOL (*P_ANT_CWINIT)(void);
typedef BOOL (*P_ANT_CWMODE)(UCHAR, UCHAR);
typedef BOOL (*P_ANT_TXEXT)(UCHAR, UCHAR*);
typedef BOOL (*P_ANT_TXAEXT)(UCHAR, UCHAR*);
typedef BOOL (*P_ANT_TXBEXT)(UCHAR, UCHAR*);
typedef USHORT (*P_ANT_BURSTTFR)(UCHAR, UCHAR*, USHORT);
typedef const UCHAR* (*P_ANT_LIBVER)(void);
typedef void (*P_ANT_NAP)(ULONG);



//////////////////////////////////////////////////////////////////////////////////
// Public Variable Prototypes
//////////////////////////////////////////////////////////////////////////////////

// Function Pointers
extern P_ANT_INIT             ANT_Init;
extern P_ANT_INIT_EXT         ANT_InitExt;
extern P_ANT_GETDEVICEUSBINFO ANT_GetDeviceUSBInfo;
extern P_ANT_GETDEVICEUSBPID  ANT_GetDeviceUSBPID;
extern P_ANT_GETDEVICEUSBVID  ANT_GetDeviceUSBVID;
extern P_ANT_CLOSE            ANT_Close;
extern P_ANT_ARF              ANT_AssignResponseFunction;
extern P_ANT_AEF              ANT_AssignChannelEventFunction;
extern P_ANT_UNASSIGN         ANT_UnAssignChannel;
extern P_ANT_ASSIGN           ANT_AssignChannel;
extern P_ANT_ASSIGN_EXT       ANT_AssignChannelExt;
extern P_ANT_SETID            ANT_SetChannelId;
extern P_ANT_SETPER           ANT_SetChannelPeriod;
extern P_ANT_SETTIME          ANT_SetChannelSearchTimeout;
extern P_ANT_SETFREQ          ANT_SetChannelRFFreq;
extern P_ANT_SETKEY           ANT_SetNetworkKey;
extern P_ANT_SETPWR           ANT_SetTransmitPower;
extern P_ANT_RST              ANT_ResetSystem;
extern P_ANT_OPENCHNL         ANT_OpenChannel;
extern P_ANT_CLOSECHNL        ANT_CloseChannel;
extern P_ANT_REQMSG           ANT_RequestMessage;
extern P_ANT_TX               ANT_SendBroadcastData;
extern P_ANT_TXA              ANT_SendAcknowledgedData;
extern P_ANT_TXB              ANT_SendBurstTransfer;
extern P_ANT_ENABLEEXT        ANT_RxExtMesgsEnable;
extern P_ANT_ADDID            ANT_AddChannelID;
extern P_ANT_CONFIGLIST       ANT_ConfigList;
extern P_ANT_CHANTXPWR        ANT_SetChannelTxPower;
extern P_ANT_LOWPRITIME       ANT_SetLowPriorityChannelSearchTimeout;
extern P_ANT_SERIALID         ANT_SetSerialNumChannelId;
extern P_ANT_SETLED           ANT_EnableLED;
extern P_ANT_SETCRYSTAL       ANT_CrystalEnable;
extern P_ANT_SETAGILITY       ANT_ConfigFrequencyAgility;
extern P_ANT_SETPROX          ANT_SetProximitySearch;
extern P_ANT_SETSCAN          ANT_OpenRxScanMode;
extern P_ANT_SETSLEEP         ANT_SleepMessage;
extern P_ANT_CWINIT           ANT_InitCWTestMode;
extern P_ANT_CWMODE           ANT_SetCWTestMode;
extern P_ANT_TXEXT            ANT_SendExtBroadcastData;
extern P_ANT_TXAEXT           ANT_SendExtAcknowledgedData;
extern P_ANT_TXBEXT           ANT_SendExtBurstTransferPacket;
extern P_ANT_BURSTTFR         ANT_SendExtBurstTransfer;
extern P_ANT_LIBVER           ANT_LibVersion;
extern P_ANT_NAP              ANT_Nap;



//////////////////////////////////////////////////////////////////////////////////
// Public Function Prototypes
//////////////////////////////////////////////////////////////////////////////////
extern BOOL ANT_Load(void);
extern void ANT_UnLoad(void);
extern BOOL ANT_LibVersionSupport();

#if defined(__cplusplus)
   }
#endif

#endif // !defined(ANT_DLL_H)
