/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/
#include "types.h"
#include "libant.h"

#if defined(DSI_TYPES_WINDOWS)
   #include <windows.h>
   #define DYNAMIC_LIB_FILENAME     "ANT_DLL.dll"
   #define DYNAMIC_LIB_INSTANCE     HMODULE
   #define DYNAMIC_LIB_LOAD(X)      LoadLibraryA(X)
   #define DYNAMIC_LIB_LOAD_PROC    GetProcAddress
   #define DYNAMIC_LIB_CLOSE        FreeLibrary
#elif defined(DSI_TYPES_MACINTOSH) || defined(DSI_TYPES_LINUX)
   #include <dlfcn.h>
   #define DYNAMIC_LIB_FILENAME     "libANT.dylib"
   #define DYNAMIC_LIB_INSTANCE     void*
   #define DYNAMIC_LIB_LOAD(X)      dlopen(X, RTLD_NOW)
   #define DYNAMIC_LIB_LOAD_PROC    dlsym
   #define DYNAMIC_LIB_CLOSE        dlclose
#endif

//////////////////////////////////////////////////////////////////////////////////
// Public Variables
//////////////////////////////////////////////////////////////////////////////////

// Function Pointers
P_ANT_INIT              ANT_Init;
P_ANT_INIT_EXT          ANT_InitExt;
P_ANT_GETDEVICEUSBINFO  ANT_GetDeviceUSBInfo;
P_ANT_GETDEVICEUSBPID   ANT_GetDeviceUSBPID;
P_ANT_GETDEVICEUSBVID   ANT_GetDeviceUSBVID;
P_ANT_CLOSE             ANT_Close;
P_ANT_ARF               ANT_AssignResponseFunction;
P_ANT_AEF               ANT_AssignChannelEventFunction;
P_ANT_UNASSIGN          ANT_UnAssignChannel;
P_ANT_ASSIGN            ANT_AssignChannel;
P_ANT_ASSIGN_EXT        ANT_AssignChannelExt;
P_ANT_SETID             ANT_SetChannelId;
P_ANT_SETPER            ANT_SetChannelPeriod;
P_ANT_SETTIME           ANT_SetChannelSearchTimeout;
P_ANT_SETFREQ           ANT_SetChannelRFFreq;
P_ANT_SETKEY            ANT_SetNetworkKey;
P_ANT_SETPWR            ANT_SetTransmitPower;
P_ANT_RST               ANT_ResetSystem;
P_ANT_OPENCHNL          ANT_OpenChannel;
P_ANT_CLOSECHNL         ANT_CloseChannel;
P_ANT_REQMSG            ANT_RequestMessage;
P_ANT_ENABLEEXT         ANT_RxExtMesgsEnable;
P_ANT_TX                ANT_SendBroadcastData;
P_ANT_TXA               ANT_SendAcknowledgedData;
P_ANT_TXB               ANT_SendBurstTransfer;
P_ANT_ADDID             ANT_AddChannelID;
P_ANT_CONFIGLIST        ANT_ConfigList;
P_ANT_CHANTXPWR         ANT_SetChannelTxPower;
P_ANT_LOWPRITIME        ANT_SetLowPriorityChannelSearchTimeout;
P_ANT_SERIALID          ANT_SetSerialNumChannelId;
P_ANT_SETLED            ANT_EnableLED;
P_ANT_SETCRYSTAL        ANT_CrystalEnable;
P_ANT_SETAGILITY        ANT_ConfigFrequencyAgility;
P_ANT_SETPROX           ANT_SetProximitySearch;
P_ANT_SETSCAN           ANT_OpenRxScanMode;
P_ANT_SETSLEEP          ANT_SleepMessage;
P_ANT_CWINIT            ANT_InitCWTestMode;
P_ANT_CWMODE            ANT_SetCWTestMode;
P_ANT_TXEXT             ANT_SendExtBroadcastData;
P_ANT_TXAEXT            ANT_SendExtAcknowledgedData;
P_ANT_TXBEXT            ANT_SendExtBurstTransferPacket;
P_ANT_BURSTTFR          ANT_SendExtBurstTransfer;
P_ANT_LIBVER            ANT_LibVersion;
P_ANT_NAP               ANT_Nap;



//////////////////////////////////////////////////////////////////////////////////
// Private Variables
//////////////////////////////////////////////////////////////////////////////////

static DYNAMIC_LIB_INSTANCE hANTdll;
static BOOL bLoaded = FALSE;


//////////////////////////////////////////////////////////////////////////////////
// Public Functions
//////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
BOOL ANT_Load(void)
{
   if (bLoaded)
      return TRUE;

   hANTdll = DYNAMIC_LIB_LOAD(DYNAMIC_LIB_FILENAME);
   if (hANTdll == NULL)
      return FALSE;

   ANT_Init = (P_ANT_INIT) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_Init");
   ANT_InitExt = (P_ANT_INIT_EXT)DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_InitExt");
   ANT_GetDeviceUSBInfo = (P_ANT_GETDEVICEUSBINFO) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_GetDeviceUSBInfo");
   ANT_GetDeviceUSBPID = (P_ANT_GETDEVICEUSBPID) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_GetDeviceUSBPID");
   ANT_GetDeviceUSBVID = (P_ANT_GETDEVICEUSBVID) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_GetDeviceUSBVID");
   ANT_Close = (P_ANT_CLOSE) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_Close");
   ANT_AssignResponseFunction = (P_ANT_ARF) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_AssignResponseFunction");
   ANT_AssignChannelEventFunction = (P_ANT_AEF) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_AssignChannelEventFunction");
   ANT_UnAssignChannel = (P_ANT_UNASSIGN) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_UnAssignChannel");
   ANT_AssignChannel = (P_ANT_ASSIGN) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_AssignChannel");
   ANT_AssignChannelExt = (P_ANT_ASSIGN_EXT) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_AssignChannelExt");
   ANT_SetChannelId =(P_ANT_SETID) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SetChannelId");
   ANT_SetChannelPeriod = (P_ANT_SETPER) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SetChannelPeriod");
   ANT_SetChannelSearchTimeout = (P_ANT_SETTIME) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SetChannelSearchTimeout");
   ANT_SetChannelRFFreq = (P_ANT_SETFREQ) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SetChannelRFFreq");
   ANT_SetNetworkKey = (P_ANT_SETKEY) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SetNetworkKey");
   ANT_SetTransmitPower = (P_ANT_SETPWR) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SetTransmitPower");
   ANT_ResetSystem = (P_ANT_RST) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_ResetSystem");
   ANT_OpenChannel = (P_ANT_OPENCHNL) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_OpenChannel");
   ANT_CloseChannel = (P_ANT_CLOSECHNL) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_CloseChannel");
   ANT_RequestMessage = (P_ANT_REQMSG) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_RequestMessage");
   ANT_RxExtMesgsEnable = (P_ANT_ENABLEEXT) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_RxExtMesgsEnable");
   ANT_SendBroadcastData = (P_ANT_TX) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SendBroadcastData");
   ANT_SendAcknowledgedData = (P_ANT_TXA) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SendAcknowledgedData");
   ANT_SendBurstTransfer = (P_ANT_TXB) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SendBurstTransfer");
   ANT_AddChannelID = (P_ANT_ADDID) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_AddChannelID");
   ANT_ConfigList = (P_ANT_CONFIGLIST) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_ConfigList");
   ANT_SetChannelTxPower = (P_ANT_CHANTXPWR) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SetChannelTxPower");
   ANT_SetLowPriorityChannelSearchTimeout = (P_ANT_LOWPRITIME) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SetLowPriorityChannelSearchTimeout");
   ANT_SetSerialNumChannelId = (P_ANT_SERIALID) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SetSerialNumChannelId");
   ANT_EnableLED = (P_ANT_SETLED) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_EnableLED");
   ANT_CrystalEnable = (P_ANT_SETCRYSTAL) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_CrystalEnable");
   ANT_ConfigFrequencyAgility = (P_ANT_SETAGILITY) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_ConfigFrequencyAgility");
   ANT_SetProximitySearch = (P_ANT_SETPROX) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SetProximitySearch");
   ANT_OpenRxScanMode = (P_ANT_SETSCAN) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_OpenRxScanMode");
   ANT_SleepMessage = (P_ANT_SETSLEEP) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SleepMessage");
   ANT_InitCWTestMode = (P_ANT_CWINIT) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_InitCWTestMode");
   ANT_SetCWTestMode = (P_ANT_CWMODE) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SetCWTestMode");
   ANT_SendExtBroadcastData = (P_ANT_TXEXT) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SendExtBroadcastData");
   ANT_SendExtAcknowledgedData = (P_ANT_TXAEXT) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SendExtAcknowledgedData");
   ANT_SendExtBurstTransferPacket = (P_ANT_TXBEXT) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SendExtBurstTransferPacket");
   ANT_SendExtBurstTransfer = (P_ANT_BURSTTFR) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_SendExtBurstTransfer");
   ANT_LibVersion = (P_ANT_LIBVER) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_LibVersion");
   ANT_Nap = (P_ANT_NAP) DYNAMIC_LIB_LOAD_PROC(hANTdll, "ANT_Nap");

   if (ANT_Init == NULL)
      return FALSE;
   if (ANT_InitExt == NULL)
      return FALSE;
   if (ANT_Close == NULL)
      return FALSE;
   if (ANT_AssignResponseFunction == NULL)
      return FALSE;
   if (ANT_AssignChannelEventFunction == NULL)
      return FALSE;
   if (ANT_UnAssignChannel == NULL)
      return FALSE;
   if (ANT_AssignChannel == NULL)
      return FALSE;
   if(ANT_AssignChannelExt == NULL)
      return FALSE;
   if (ANT_SetChannelId == NULL)
      return FALSE;
   if (ANT_SetChannelPeriod == NULL)
      return FALSE;
   if (ANT_SetChannelSearchTimeout == NULL)
      return FALSE;
   if (ANT_SetChannelRFFreq == NULL)
      return FALSE;
   if (ANT_SetNetworkKey == NULL)
      return FALSE;
   if (ANT_SetTransmitPower == NULL)
      return FALSE;
   if (ANT_ResetSystem == NULL)
      return FALSE;
   if (ANT_OpenChannel == NULL)
      return FALSE;
   if (ANT_CloseChannel == NULL)
      return FALSE;
   if (ANT_RequestMessage == NULL)
      return FALSE;
   if (ANT_SendBroadcastData == NULL)
      return FALSE;
   if (ANT_SendAcknowledgedData == NULL)
      return FALSE;
   if (ANT_SendBurstTransfer == NULL)
      return FALSE;
   if(ANT_RxExtMesgsEnable == NULL)
      return FALSE;
   if(ANT_AddChannelID == NULL)
      return FALSE;
   if(ANT_ConfigList == NULL)
      return FALSE;
   if(ANT_SetChannelTxPower == NULL)
      return FALSE;
   if(ANT_SetLowPriorityChannelSearchTimeout == NULL)
      return FALSE;
   if(ANT_SetSerialNumChannelId == NULL)
      return FALSE;
   if(ANT_EnableLED == NULL)
      return FALSE;
   if(ANT_CrystalEnable == NULL)
      return FALSE;
   if(ANT_ConfigFrequencyAgility == NULL)
      return FALSE;
   if(ANT_SetProximitySearch == NULL)
      return FALSE;
   if(ANT_OpenRxScanMode == NULL)
      return FALSE;
   if(ANT_SleepMessage == NULL)
      return FALSE;
   if(ANT_InitCWTestMode == NULL)
      return FALSE;
   if(ANT_SetCWTestMode == NULL)
      return FALSE;
   if(ANT_SendExtBroadcastData == NULL)
      return FALSE;
   if(ANT_SendExtAcknowledgedData == NULL)
      return FALSE;
   if(ANT_SendExtBurstTransferPacket == NULL)
      return FALSE;
   if(ANT_SendExtBurstTransfer == NULL)
      return FALSE;
   if(ANT_Nap == NULL)
      return FALSE;

   bLoaded = TRUE;
   return TRUE;
}

///////////////////////////////////////////////////////////////////////
void ANT_UnLoad(void)
{
   if (hANTdll != NULL)
   {
      DYNAMIC_LIB_CLOSE(hANTdll);
   }
   bLoaded = FALSE;
}

///////////////////////////////////////////////////////////////////////
BOOL ANT_LibVersionSupport(void)
{
   BOOL bLibVer = FALSE;

   if(hANTdll != NULL)
   {
      if(ANT_LibVersion != NULL)
         bLibVer = TRUE;
   }

   return bLibVer;
}
