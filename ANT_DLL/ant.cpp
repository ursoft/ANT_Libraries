﻿/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/

/*
 **************************************************************************
 *
 * DESCRIPTION:
 *
 * This module is an example wrapper for the ANT communications library
 * It sets up and maintains the link, and provides a simplified API to the
 * application.
 *
 **************************************************************************
 */

#include "defines.h"
#include <string.h>  // memcpy
#include <stdio.h>
#include <assert.h>

#include "ant.h"
#include "version.h"
#include "types.h"
#include "antdefines.h"
#include "usb_device_handle.hpp"
#include "dsi_serial_generic.hpp"
#if defined(DSI_TYPES_WINDOWS)
   #include "dsi_serial_vcp.hpp"
#endif
#include "dsi_framer_ant.hpp"
#include "dsi_thread.h"
#if defined(DEBUG_FILE)
   #include "dsi_debug.hpp"
#endif


#define MAX_CHANNELS ((UCHAR) 8)

#define MESG_CHANNEL_OFFSET                  0
#define MESG_EVENT_ID_OFFSET                 1
#define MESG_EVENT_CODE_OFFSET               2

// Struct to define channel event callback functions
// and recieve buffer.
typedef struct
{
   CHANNEL_EVENT_FUNC pfLinkEvent;
   UCHAR *pucRxBuffer;
} CHANNEL_LINK;



// Local variables.
static DSISerial* pclSerialObject = NULL;
static DSIFramerANT* pclMessageObject = NULL;
static DSI_THREAD_ID uiDSIThread;
static DSI_CONDITION_VAR condTestDone;
static DSI_MUTEX mutexTestDone;



// Local Data
static RESPONSE_FUNC pfResponseFunc = NULL;  //pointer to main response callback function
static UCHAR *pucResponseBuffer = NULL;            //pointer to buffer used to hold data from the response message
static CHANNEL_LINK sLink[MAX_CHANNELS];             //array of pointer for each channel
static BOOL bInitialized = FALSE;
static UCHAR ucAutoTransferChannel = 0xFF;
static USHORT usNumDataPackets = 0;
static BOOL bGoThread = FALSE;
static DSI_THREAD_IDNUM eTheThread;



// Local funcs
static DSI_THREAD_RETURN MessageThread(void *pvParameter_);
static void SerialHaveMessage(ANT_MESSAGE& stMessage_, USHORT usSize_);
static void MemoryCleanup(); //Deletes internal objects from memory

extern "C" EXPORT
BOOL ANT_Init(UCHAR ucUSBDeviceNum, ULONG ulBaudrate, UCHAR ucPortType, UCHAR ucSerialFrameType)
{
    return ANT_InitExt(ucUSBDeviceNum, ulBaudrate, ucPortType, ucSerialFrameType);
}

#ifdef ENABLE_EDGE_REMOTE
int glbZWIFT_EDGE_REMOTE = 0; //need also environment variable "ZWIFT_EDGE_REMOTE=20_bit_edge_id"
BOOL glbEdgeRemoteBroadcasting = 0;
#endif

//Initializes and opens USB connection to the module
extern "C" EXPORT
BOOL ANT_InitExt(UCHAR ucUSBDeviceNum, ULONG ulBaudrate, UCHAR ucPortType_, UCHAR ucSerialFrameType_)
{
   DSI_THREAD_IDNUM eThread = DSIThread_GetCurrentThreadIDNum();

   assert(eTheThread != eThread); // CANNOT CALL THIS FUNCTION FROM DLL THREAD (INSIDE DLL CALLBACK ROUTINES).

   assert(!bInitialized);         // IF ANT WAS ALREADY INITIALIZED, DO NOT CALL THIS FUNCTION BEFORE CALLING ANT_Close();


#if defined(DEBUG_FILE)
   DSIDebug::Init();
   DSIDebug::SetDebug(TRUE);
   DSIDebug::ThreadInit("ant_api_calls.txt");
   DSIDebug::ThreadEnable(TRUE);
   DSIDebug::ThreadPrintf("ANT_InitExt(UCHAR ucUSBDeviceNum=%d, ULONG ulBaudrate=%d, UCHAR ucPortType_=%d, UCHAR ucSerialFrameType_=%d)", ucUSBDeviceNum, ulBaudrate, ucPortType_, ucSerialFrameType_);
#endif
#ifdef ENABLE_EDGE_REMOTE
   const char *edge_id_str = getenv("ZWIFT_EDGE_REMOTE");
   if(edge_id_str && *edge_id_str)
       glbZWIFT_EDGE_REMOTE = atoi(edge_id_str);
   glbEdgeRemoteBroadcasting = 0;
#if defined(DEBUG_FILE)
   DSIDebug::ThreadPrintf("glbZWIFT_EDGE_REMOTE=%d (%s)", glbZWIFT_EDGE_REMOTE, edge_id_str);
#endif //DEBUG_FILE
#endif //ENABLE_EDGE_REMOTE

   //Create Serial object.
   pclSerialObject = NULL;

   switch(ucPortType_)
   {
      case PORT_TYPE_USB:
        pclSerialObject = new DSISerialGeneric();
        break;
#if defined(DSI_TYPES_WINDOWS)
      case PORT_TYPE_COM:
        pclSerialObject = new DSISerialVCP();
        break;
#endif
      default: //Invalid port type selection
         return(FALSE);
   }

   if(!pclSerialObject)
      return(FALSE);

   //Initialize Serial object.
   //NOTE: Will fail if the module is not available.
   if(!pclSerialObject->Init(ulBaudrate, ucUSBDeviceNum))
   {
      MemoryCleanup();
      return(FALSE);
   }

   //Create Framer object.
   pclMessageObject = NULL;
   switch(ucSerialFrameType_)
   {
      case FRAMER_TYPE_BASIC:
         pclMessageObject = new DSIFramerANT(pclSerialObject);
         break;


      default:
         MemoryCleanup();
         return(FALSE);
   }

   if(!pclMessageObject)
   {
      MemoryCleanup();
      return(FALSE);
   }

   //Initialize Framer object.
   if(!pclMessageObject->Init())
   {
      MemoryCleanup();
      return(FALSE);
   }

   //Let Serial know about Framer.
   //for(int i = 1; i < 100; i++) Sleep(1000);
   pclSerialObject->SetCallback(pclMessageObject);

   //Open Serial.
   if(!pclSerialObject->Open())
   {
      MemoryCleanup();
      return(FALSE);
   }

   //Create message thread.
   UCHAR ucCondInit= DSIThread_CondInit(&condTestDone);
   assert(ucCondInit == DSI_THREAD_ENONE);

   UCHAR ucMutexInit = DSIThread_MutexInit(&mutexTestDone);
   assert(ucMutexInit == DSI_THREAD_ENONE);

   bGoThread = TRUE;
   uiDSIThread = DSIThread_CreateThread(MessageThread, NULL);
   if(!uiDSIThread)
   {
      MemoryCleanup();
      bGoThread = FALSE;
      return(FALSE);
   }

   bInitialized = TRUE;
   return(TRUE);

}

///////////////////////////////////////////////////////////////////////
// Called by the application to close the usb connection
// MUST NOT BE CALLED IN THE CONTEXT OF THE MessageThread. That is,
// At the application level it must not be called within the
// callback functions into this library.
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
void ANT_Close(void)
{
   DSI_THREAD_IDNUM eThread = DSIThread_GetCurrentThreadIDNum();
#if defined(DEBUG_FILE)
   DSIDebug::ThreadPrintf("ANT_Close()");
   DSIDebug::Close();
#endif

   assert(eTheThread != eThread); // CANNOT CALL THIS FUNCTION FROM DLL THREAD (INSIDE DLL CALLBACK ROUTINES).

   if (!bInitialized)
      return;

   bInitialized = FALSE;

   DSIThread_MutexLock(&mutexTestDone);
   bGoThread = FALSE;

   UCHAR ucWaitResult = DSIThread_CondTimedWait(&condTestDone, &mutexTestDone, DSI_THREAD_INFINITE);
   assert(ucWaitResult == DSI_THREAD_ENONE);
   DSIThread_MutexUnlock(&mutexTestDone);

   //Destroy mutex and condition var
   DSIThread_MutexDestroy(&mutexTestDone);
   DSIThread_CondDestroy(&condTestDone);

   MemoryCleanup();
}


#ifdef ENABLE_EDGE_REMOTE
bool IsExtendedKey(DWORD key) {
    switch (key) {
    case 0x4000000D: //NUMPAD_RETURN:
    case VK_MENU:
    case VK_RMENU:
    case VK_CONTROL:
    case VK_RCONTROL:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_RIGHT:
    case VK_UP:
    case VK_LEFT:
    case VK_DOWN:
    case VK_NUMLOCK:
    case VK_CANCEL:
    case VK_SNAPSHOT:
    case VK_DIVIDE:
        return true;
    }
    return false;
}
void EmitZwiftKeyPress(WORD key) {
    static HWND zwiftWindow = NULL;
    if(zwiftWindow == NULL)
        zwiftWindow = FindWindow(NULL, "Zwift");

    /*if (*/SetForegroundWindow(zwiftWindow);/*)*/ { // may be full screen and no window
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = key;
        inputs[0].ki.wScan = MapVirtualKey(key, MAPVK_VK_TO_VSC) & 0xFF;
        inputs[0].ki.wVk = key;
        inputs[0].ki.dwFlags = IsExtendedKey(key) ? KEYEVENTF_EXTENDEDKEY : 0;
        inputs[1] = inputs[0];
        inputs[1].ki.dwFlags |= KEYEVENTF_KEYUP;
        SendInput(2, inputs, sizeof(INPUT));
    }
}

static UCHAR glbEdgeRemoteChannelRxBuffer[/*MAX_CHANNEL_EVENT_SIZE*/ 41];
BOOL EdgeRemoteLinkEvent(UCHAR ucANTChannel_, UCHAR ucEvent) {
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("EdgeRemoteLinkEvent(ucANTChannel_=%d, ucEvent=%d) glbEdgeRemoteBroadcasting=%d", ucANTChannel_, ucEvent, glbEdgeRemoteBroadcasting);
#endif
    if (glbZWIFT_EDGE_REMOTE && ucANTChannel_ == 7) {
        int ucDataOffset = 0;
        switch (ucEvent) {
        case EVENT_RX_FLAG_ACKNOWLEDGED: case EVENT_RX_FLAG_BURST_PACKET: case EVENT_RX_FLAG_BROADCAST:
        case EVENT_RX_BROADCAST: case EVENT_RX_ACKNOWLEDGED: case EVENT_RX_BURST_PACKET:
            ucDataOffset = 1 /*MESSAGE_BUFFER_DATA2_INDEX*/;
            break;
        case EVENT_RX_EXT_BROADCAST: case EVENT_RX_EXT_ACKNOWLEDGED: case EVENT_RX_EXT_BURST_PACKET:
            ucDataOffset = 5 /*MESSAGE_BUFFER_DATA6_INDEX*/;
            break;
        case EVENT_TX:
            if (glbEdgeRemoteBroadcasting) {
                static UCHAR tx[] = "\2\0\xff\0\0\0\0\x10";
                if (pclMessageObject) {
#if defined(DEBUG_FILE)
                    DSIDebug::ThreadPrintf("ANT_SendBroadcastDataEdgeRemote(ucANTChannel_=%d)", ucANTChannel_);
#endif
                    pclMessageObject->SendBroadcastData(ucANTChannel_, tx);
                }
                return(FALSE);
            }
            break;
        case EVENT_CHANNEL_CLOSED:
            glbEdgeRemoteBroadcasting = FALSE;
            if (pclMessageObject) {
#if defined(DEBUG_FILE)
            DSIDebug::ThreadPrintf("ANT_UnAssignChannelEdgeRemote(ucANTChannel_=%d)", ucANTChannel_);
#endif
                pclMessageObject->UnAssignChannel(ucANTChannel_, 0);
            }
            break;
        }
        if (ucDataOffset) {
#if defined(DEBUG_FILE)
            DSIDebug::ThreadPrintf("EdgeRemoteRx: [%02x],[%02x],[%02x],[%02x],[%02x],[%02x],[%02x],[%02x]\n",
                glbEdgeRemoteChannelRxBuffer[ucDataOffset + 0],
                glbEdgeRemoteChannelRxBuffer[ucDataOffset + 1],
                glbEdgeRemoteChannelRxBuffer[ucDataOffset + 2],
                glbEdgeRemoteChannelRxBuffer[ucDataOffset + 3],
                glbEdgeRemoteChannelRxBuffer[ucDataOffset + 4],
                glbEdgeRemoteChannelRxBuffer[ucDataOffset + 5],
                glbEdgeRemoteChannelRxBuffer[ucDataOffset + 6],
                glbEdgeRemoteChannelRxBuffer[ucDataOffset + 7]);
#endif
            if (glbEdgeRemoteChannelRxBuffer[ucDataOffset + 0] == 0x49) {
                /*
                        49-0D-56-01-00-02-24-00 - quick lap press (lap = 36 dec). Длинного и повтора нет.
                        49-0D-56-01-00-0E-00-80 - quick blue press (custom command 32768+0).
                        49-0D-56-01-00-0E-01-80 - long blue press (custom command 32768+1). Повтора нет.
                        49-0D-56-01-00-14-01-00 - quick screen press (menu down = 1 dec).
                        49-0D-56-01-00-14-00-00 - long screen press (menu up = 0 dec). Можно зажать, будет повторяться периодически.
                */
                extern HANDLE glbWakeSteeringThread;
                extern void OnSteeringKeyPress(DWORD key, bool bFastKeyboard);

                switch (glbEdgeRemoteChannelRxBuffer[ucDataOffset + 6] + 256 * glbEdgeRemoteChannelRxBuffer[ucDataOffset + 7]) {
                case 0:
#if defined(DEBUG_FILE)
                    DSIDebug::ThreadPrintf("Screen: long press");
#endif
                    EmitZwiftKeyPress(VK_RETURN);
                    break;
                case 1:
#if defined(DEBUG_FILE)
                    DSIDebug::ThreadPrintf("Screen: quick press");
#endif
                    OnSteeringKeyPress(VK_RIGHT, false);
                    SetEvent(glbWakeSteeringThread);
                    EmitZwiftKeyPress(VK_RIGHT);
                    break;
                case 32768:
#if defined(DEBUG_FILE)
                    DSIDebug::ThreadPrintf("Blue: quick press");
#endif
                    OnSteeringKeyPress(VK_LEFT, false);
                    SetEvent(glbWakeSteeringThread);
                    EmitZwiftKeyPress(VK_LEFT);
                    break;
                case 32769:
#if defined(DEBUG_FILE)
                    DSIDebug::ThreadPrintf("Blue: long press");
#endif
                    EmitZwiftKeyPress(VK_ESCAPE);
                    break;
                case 36:
#if defined(DEBUG_FILE)
                    DSIDebug::ThreadPrintf("Lap: quick press");
#endif
                    EmitZwiftKeyPress(VK_UP);
                    break;
                default:
                    break;
                }
            }
        }
    }
    return TRUE;
}
#endif
///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to initialize the main callback funcation,
// the main callback funcation must be initialized before the application
// can receive any reponse messages.
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
void ANT_AssignResponseFunction(RESPONSE_FUNC pfResponse_, UCHAR* pucResponseBuffer_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_AssignResponseFunction(RESPONSE_FUNC pfResponse_, UCHAR* pucResponseBuffer_)");
#endif
   pfResponseFunc = pfResponse_;
   pucResponseBuffer = pucResponseBuffer_;
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to initialize the callback funcation and
// data buffers for a particular channel.  This must be done in order
// for a channel to function properly.
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
void ANT_AssignChannelEventFunction(UCHAR ucLink, CHANNEL_EVENT_FUNC pfLinkEvent, UCHAR *pucRxBuffer)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_AssignChannelEventFunction(UCHAR ucLink=%d, CHANNEL_EVENT_FUNC pfLinkEvent, UCHAR *pucRxBuffer)", ucLink);
#endif
   if(ucLink < MAX_CHANNELS)
   {
      sLink[ucLink].pfLinkEvent = pfLinkEvent;
      sLink[ucLink].pucRxBuffer = pucRxBuffer;
   }
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Unassigns response function. Important for memory management of
// higher layer applications to avoid this library calling invalid pointers
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
void ANT_UnassignAllResponseFunctions()
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_UnassignAllResponseFunctions()");
#endif
   pfResponseFunc = NULL;
   pucResponseBuffer = NULL;
   for(int i=0; i< MAX_CHANNELS; ++i)
   {
      sLink[i].pfLinkEvent = NULL;
      sLink[i].pucRxBuffer = NULL;
   }
}



///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Returns a pointer to a string constant containing the core library
// version.
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
const char* ANT_LibVersion(void)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_LibVersion()");
#endif
   return(SW_VER);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to restart ANT on the module
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ResetSystem(void)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_ResetSystem()");
#endif
   if(pclMessageObject)
      return(pclMessageObject->ResetSystem());

   return(FALSE);
}


///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to set the network address for a given
// module channel
//!! This is (should be) a private network function
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetNetworkKey(UCHAR ucNetNumber_, UCHAR *pucKey)
{
   return ANT_SetNetworkKey_RTO(ucNetNumber_, pucKey, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetNetworkKey_RTO(UCHAR ucNetNumber_, UCHAR *pucKey, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SetNetworkKey_RTO(ucNetNumber_=%d)", ucNetNumber_);
#endif
   if(pclMessageObject)
   {
      return pclMessageObject->SetNetworkKey(ucNetNumber_, pucKey, ulResponseTime_);
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to assign a channel
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_AssignChannel(UCHAR ucANTChannel_, UCHAR ucChannelType_, UCHAR ucNetNumber_)
{
   return ANT_AssignChannel_RTO(ucANTChannel_, ucChannelType_, ucNetNumber_, 0);
}

bool ActionEnabled(UCHAR ucANTChannel_) {
    if (nullptr == pclMessageObject) return FALSE;
#ifdef ENABLE_EDGE_REMOTE
    if (glbZWIFT_EDGE_REMOTE && ucANTChannel_ == 7) //забираем 7-й канал себе
        return FALSE;
    else
#endif
        return TRUE;
}
///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_AssignChannel_RTO(UCHAR ucANTChannel_, UCHAR ucChannelType_, UCHAR ucNetNumber_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_AssignChannel_RTO(UCHAR ucANTChannel_=%d, UCHAR ucChannelType_=%d, UCHAR ucNetNumber_=%d, ULONG ulResponseTime_=%d)", ucANTChannel_, ucChannelType_, ucNetNumber_, ulResponseTime_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
       return(pclMessageObject->AssignChannel(ucANTChannel_, ucChannelType_, ucNetNumber_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to assign a channel using extended assignment
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_AssignChannelExt(UCHAR ucANTChannel_, UCHAR ucChannelType_, UCHAR ucNetNumber_, UCHAR ucExtFlags_)
{
   return ANT_AssignChannelExt_RTO(ucANTChannel_, ucChannelType_, ucNetNumber_, ucExtFlags_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_AssignChannelExt_RTO(UCHAR ucANTChannel_, UCHAR ucChannelType_, UCHAR ucNetNumber_, UCHAR ucExtFlags_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_AssignChannelExt_RTO(UCHAR ucANTChannel_=%d, UCHAR ucChannelType_=%d, UCHAR ucNetNumber_=%d)", ucANTChannel_, ucChannelType_, ucNetNumber_);
#endif
#ifdef ENABLE_EDGE_REMOTE
   if (pclMessageObject && ucANTChannel_ == 0 && glbEdgeRemoteBroadcasting == FALSE) {
       UCHAR edgeRemoteAntChannel = 7;
       sLink[edgeRemoteAntChannel].pfLinkEvent = EdgeRemoteLinkEvent;
       sLink[edgeRemoteAntChannel].pucRxBuffer = glbEdgeRemoteChannelRxBuffer;
       if (pclMessageObject->AssignChannel(edgeRemoteAntChannel, PARAMETER_TX_NOT_RX, 0/*USER_NETWORK_NUM*/, 500)) {
           if (pclMessageObject->SetChannelID(edgeRemoteAntChannel, (USHORT)glbZWIFT_EDGE_REMOTE, 16 /*USER_DEVICETYPE=control*/, 5 + ((glbZWIFT_EDGE_REMOTE >> 16) << 4)/*USER_TRANSTYPE*/, 500)) {
               if (pclMessageObject->SetChannelRFFrequency(edgeRemoteAntChannel, 57 /*ANT+ USER_RADIOFREQ*/, 500)) {
                   if (pclMessageObject->SetChannelPeriod(edgeRemoteAntChannel, 8192 /*4Hz*/, 500)) {
                       glbEdgeRemoteBroadcasting = TRUE;
                       if (pclMessageObject->OpenChannel(edgeRemoteAntChannel, 500)) {
                           ANT_RxExtMesgsEnable(TRUE); //точно нужен?
                       } else {
                           glbEdgeRemoteBroadcasting = FALSE;
#if defined(DEBUG_FILE)
                            DSIDebug::ThreadPrintf("OpenChannel(edgeRemoteAntChannel=%d, 500)=FALSE", edgeRemoteAntChannel);
#endif
                       }
                   }
#if defined(DEBUG_FILE)
                   else DSIDebug::ThreadPrintf("SetChannelPeriod(edgeRemoteAntChannel=%d, 8192, 500)=FALSE", edgeRemoteAntChannel);
#endif
               }
#if defined(DEBUG_FILE)
           else DSIDebug::ThreadPrintf("SetChannelRFFrequency(edgeRemoteAntChannel=%d, 57, 500)=FALSE", edgeRemoteAntChannel);
#endif
           }
#if defined(DEBUG_FILE)
           else DSIDebug::ThreadPrintf("SetChannelID(edgeRemoteAntChannel=%d, glbZWIFT_EDGE_REMOTE=%d, 5, 500)=FALSE", edgeRemoteAntChannel, glbZWIFT_EDGE_REMOTE);
#endif
       }
#if defined(DEBUG_FILE)
       else DSIDebug::ThreadPrintf("AssignChannel(edgeRemoteAntChannel=%d, PARAMETER_TX_NOT_RX, 0, 500)=FALSE", edgeRemoteAntChannel);
#endif
   }
#endif //ENABLE_EDGE_REMOTE
   if(ActionEnabled(ucANTChannel_))
   {
      UCHAR aucChannelType[] = {ucChannelType_, ucExtFlags_};  // Channel Type + Extended Assignment Byte

      return(pclMessageObject->AssignChannelExt(ucANTChannel_, aucChannelType, 2, ucNetNumber_, ulResponseTime_));
   } 

   return FALSE;
}
extern "C" EXPORT
BOOL ANT_LibConfigCustom(UCHAR ucLibConfigFlags_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_LibConfigCustom()");
#endif
    if (pclMessageObject)
        return pclMessageObject->SetLibConfig(ucLibConfigFlags_, ulResponseTime_);
    return FALSE;
}



///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to unassign a channel
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_UnAssignChannel(UCHAR ucANTChannel_)
{
   return ANT_UnAssignChannel_RTO(ucANTChannel_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_UnAssignChannel_RTO(UCHAR ucANTChannel_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_UnAssignChannel_RTO(ucANTChannel_=%d)", ucANTChannel_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->UnAssignChannel(ucANTChannel_, ulResponseTime_));
   }
   return FALSE;
}



///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to set the channel ID
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetChannelId(UCHAR ucANTChannel_, USHORT usDeviceNumber_, UCHAR ucDeviceType_, UCHAR ucTransmissionType_)
{
   return ANT_SetChannelId_RTO(ucANTChannel_, usDeviceNumber_, ucDeviceType_, ucTransmissionType_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetChannelId_RTO(UCHAR ucANTChannel_, USHORT usDeviceNumber_, UCHAR ucDeviceType_, UCHAR ucTransmissionType_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SetChannelId_RTO(ucANTChannel_=%d, usDeviceNumber_=%d, ucDeviceType_=%d, ucTransmissionType_=%d, ulResponseTime_=%d)", ucANTChannel_, usDeviceNumber_, ucDeviceType_, ucTransmissionType_, ulResponseTime_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->SetChannelID(ucANTChannel_, usDeviceNumber_, ucDeviceType_, ucTransmissionType_, ulResponseTime_));
   }
   return(FALSE);
}


///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to set the messaging period
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetChannelPeriod(UCHAR ucANTChannel_, USHORT usMesgPeriod_)
{
   return ANT_SetChannelPeriod_RTO(ucANTChannel_, usMesgPeriod_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetChannelPeriod_RTO(UCHAR ucANTChannel_, USHORT usMesgPeriod_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SetChannelPeriod_RTO(ucANTChannel_=%d, usMesgPeriod_=%d, ulResponseTime_=%d)", ucANTChannel_, usMesgPeriod_, ulResponseTime_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->SetChannelPeriod(ucANTChannel_, usMesgPeriod_, ulResponseTime_));
   }
   return FALSE;

}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to set the messaging period
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_RSSI_SetSearchThreshold(UCHAR ucANTChannel_, UCHAR ucThreshold_)
{
   return ANT_RSSI_SetSearchThreshold_RTO(ucANTChannel_, ucThreshold_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_RSSI_SetSearchThreshold_RTO(UCHAR ucANTChannel_, UCHAR ucThreshold_, ULONG ulResponseTime_)
{
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->SetRSSISearchThreshold(ucANTChannel_, ucThreshold_, ulResponseTime_));
   }
   return(FALSE);
}


///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Used to set Low Priority Search Timeout. Not available on AP1
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetLowPriorityChannelSearchTimeout(UCHAR ucANTChannel_, UCHAR ucSearchTimeout_)
{
   return ANT_SetLowPriorityChannelSearchTimeout_RTO(ucANTChannel_, ucSearchTimeout_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetLowPriorityChannelSearchTimeout_RTO(UCHAR ucANTChannel_, UCHAR ucSearchTimeout_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SetLowPriorityChannelSearchTimeout_RTO(ucANTChannel_=%d, ucSearchTimeout_=%d, ulResponseTime_=%d)", ucANTChannel_, ucSearchTimeout_, ulResponseTime_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->SetLowPriorityChannelSearchTimeout(ucANTChannel_, ucSearchTimeout_, ulResponseTime_));
   }
   return(FALSE);
}


///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to set the search timeout for a particular
// channel on the module
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetChannelSearchTimeout(UCHAR ucANTChannel_, UCHAR ucSearchTimeout_)
{
   return ANT_SetChannelSearchTimeout_RTO(ucANTChannel_, ucSearchTimeout_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetChannelSearchTimeout_RTO(UCHAR ucANTChannel_, UCHAR ucSearchTimeout_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SetChannelSearchTimeout_RTO(ucANTChannel_=%d, ucSearchTimeout_=%d, ulResponseTime_=%d)", ucANTChannel_, ucSearchTimeout_, ulResponseTime_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->SetChannelSearchTimeout(ucANTChannel_, ucSearchTimeout_, ulResponseTime_));
   }
   return(FALSE);
}


///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to set the RF frequency for a given channel
//!! This is (should be) a private network function
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetChannelRFFreq(UCHAR ucANTChannel_, UCHAR ucRFFreq_)
{
   return ANT_SetChannelRFFreq_RTO(ucANTChannel_, ucRFFreq_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetChannelRFFreq_RTO(UCHAR ucANTChannel_, UCHAR ucRFFreq_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SetChannelRFFreq_RTO(ucANTChannel_=%d, ucRFFreq_=%d, ulResponseTime_=%d)", ucANTChannel_, ucRFFreq_, ulResponseTime_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->SetChannelRFFrequency(ucANTChannel_, ucRFFreq_, ulResponseTime_));
   }
   return(FALSE);
}


///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to set the transmit power for the module
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetTransmitPower(UCHAR ucTransmitPower_)
{
   return ANT_SetTransmitPower_RTO(ucTransmitPower_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetTransmitPower_RTO(UCHAR ucTransmitPower_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SetTransmitPower_RTO(ucTransmitPower_=%d, ulResponseTime_=%d)", ucTransmitPower_, ulResponseTime_);
#endif
   if(pclMessageObject)
   {
      return(pclMessageObject->SetAllChannelsTransmitPower(ucTransmitPower_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to configure advanced bursting
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigureAdvancedBurst(BOOL bEnable_, UCHAR ucMaxPacketLength_, ULONG ulRequiredFields_, ULONG ulOptionalFields_)
{
   return ANT_ConfigureAdvancedBurst_RTO(bEnable_, ucMaxPacketLength_, ulRequiredFields_, ulOptionalFields_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigureAdvancedBurst_RTO(BOOL bEnable_, UCHAR ucMaxPacketLength_, ULONG ulRequiredFields_, ULONG ulOptionalFields_, ULONG ulResponseTime_)
{
   if(pclMessageObject)
   {
      return(pclMessageObject->ConfigAdvancedBurst(bEnable_, ucMaxPacketLength_, ulRequiredFields_, ulOptionalFields_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Stall count version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigureAdvancedBurst_ext(BOOL bEnable_, UCHAR ucMaxPacketLength_, ULONG ulRequiredFields_, ULONG ulOptionalFields_, USHORT usStallCount_, UCHAR ucRetryCount_)
{
   return ANT_ConfigureAdvancedBurst_ext_RTO(bEnable_, ucMaxPacketLength_, ulRequiredFields_, ulOptionalFields_, usStallCount_, ucRetryCount_, 0);
}

///////////////////////////////////////////////////////////////////////
// Stall count version with response timeout
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigureAdvancedBurst_ext_RTO(BOOL bEnable_, UCHAR ucMaxPacketLength_, ULONG ulRequiredFields_, ULONG ulOptionalFields_, USHORT usStallCount_, UCHAR ucRetryCount_, ULONG ulResponseTime_)
{
   if(pclMessageObject)
   {
      return(pclMessageObject->ConfigAdvancedBurst_ext(bEnable_, ucMaxPacketLength_, ulRequiredFields_, ulOptionalFields_, usStallCount_, ucRetryCount_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to set the transmit power for the module
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetChannelTxPower(UCHAR ucANTChannel_, UCHAR ucTransmitPower_)
{
   return ANT_SetChannelTxPower_RTO(ucANTChannel_, ucTransmitPower_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetChannelTxPower_RTO(UCHAR ucANTChannel_, UCHAR ucTransmitPower_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SetChannelTxPower_RTO(ucANTChannel_=%d, ucTransmitPower_=%d, ulResponseTime_=%d)", ucANTChannel_, ucTransmitPower_, ulResponseTime_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->SetChannelTransmitPower(ucANTChannel_, ucTransmitPower_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to request a generic message
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_RequestMessage(UCHAR ucANTChannel_, UCHAR ucMessageID_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_RequestMessage(ucANTChannel_=%d, ucMessageID_=%d)", ucANTChannel_, ucMessageID_);
#endif
   if(ActionEnabled(ucANTChannel_)){
      ANT_MESSAGE_ITEM stResponse;
      return pclMessageObject->SendRequest(ucMessageID_, ucANTChannel_, &stResponse, 0);
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to send a generic message
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_WriteMessage(UCHAR ucMessageID, UCHAR* aucData, USHORT usMessageSize)
{
   if(pclMessageObject){
      ANT_MESSAGE pstTempANTMessage;
      pstTempANTMessage.ucMessageID = ucMessageID;
      memcpy(pstTempANTMessage.aucData, aucData, MIN(usMessageSize, MESG_MAX_SIZE_VALUE));
      return pclMessageObject->WriteMessage(&pstTempANTMessage, usMessageSize);
   }
   return FALSE;
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to open an assigned channel
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_OpenChannel(UCHAR ucANTChannel_)
{
   return ANT_OpenChannel_RTO(ucANTChannel_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_OpenChannel_RTO(UCHAR ucANTChannel_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_OpenChannel_RTO(ucANTChannel_=%d, ulResponseTime_=%d)", ucANTChannel_, ulResponseTime_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->OpenChannel(ucANTChannel_, ulResponseTime_));
   }
   return(FALSE);
}


///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to close an opend channel
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_CloseChannel(UCHAR ucANTChannel_)
{
   return ANT_CloseChannel_RTO(ucANTChannel_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_CloseChannel_RTO(UCHAR ucANTChannel_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_CloseChannel_RTO(ucANTChannel_=%d, ulResponseTime_=%d)", ucANTChannel_, ulResponseTime_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->CloseChannel(ucANTChannel_, ulResponseTime_));
   }
   return(FALSE);
}



///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to construct and send a broadcast data message.
// This message will be broadcast on the next synchronous channel period.
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SendBroadcastData(UCHAR ucANTChannel_, UCHAR *pucData_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SendBroadcastData(ucANTChannel_=%d)", ucANTChannel_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->SendBroadcastData(ucANTChannel_, pucData_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to construct and send an acknowledged data
// mesg.  This message will be transmitted on the next synchronous channel
// period.
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SendAcknowledgedData(UCHAR ucANTChannel_, UCHAR *pucData_)
{
   return ANT_SendAcknowledgedData_RTO(ucANTChannel_, pucData_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SendAcknowledgedData_RTO(UCHAR ucANTChannel_, UCHAR *pucData_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SendAcknowledgedData_RTO(ucANTChannel_=%d)", ucANTChannel_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->SendAcknowledgedData( ucANTChannel_, pucData_, ulResponseTime_));
   }
   return(FALSE);
}


///////////////////////////////////////////////////////////////////////
// Used to send burst data using a block of data.  Proper sequence number
// of packet is maintained by the function.  Useful for testing purposes.
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SendBurstTransfer(UCHAR ucANTChannel_, UCHAR *pucData_, USHORT usNumDataPackets_)
{
   return ANT_SendBurstTransfer_RTO(ucANTChannel_, pucData_, usNumDataPackets_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SendBurstTransfer_RTO(UCHAR ucANTChannel_, UCHAR *pucData_, USHORT usNumDataPackets_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SendBurstTransfer_RTO(ucANTChannel_=%d)", ucANTChannel_);
#endif
   ULONG ulSize = usNumDataPackets_*8;   // Pass the number of bytes.
   ANTFRAMER_RETURN eStatus;

   if(ActionEnabled(ucANTChannel_))
   {
      eStatus = pclMessageObject->SendTransfer( ucANTChannel_, pucData_, ulSize, ulResponseTime_);

      if( eStatus == ANTFRAMER_PASS )
         return(TRUE);
   }
   return(FALSE);
}




//////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to configure and start CW test mode.
// There is no way to turn off CW mode other than to do a reset on the module.
/////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_InitCWTestMode(void)
{
   return ANT_InitCWTestMode_RTO(0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_InitCWTestMode_RTO(ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_InitCWTestMode_RTO(ulResponseTime_=%d)", ulResponseTime_);
#endif
   if(pclMessageObject)
   {
      return(pclMessageObject->InitCWTestMode(ulResponseTime_));
   }
   return(FALSE);
}


//////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to configure and start CW test mode.
// There is no way to turn off CW mode other than to do a reset on the module.
/////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetCWTestMode(UCHAR ucTransmitPower_, UCHAR ucRFChannel_)
{
   return ANT_SetCWTestMode_RTO(ucTransmitPower_, ucRFChannel_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetCWTestMode_RTO(UCHAR ucTransmitPower_, UCHAR ucRFChannel_, ULONG ulResponseTime_)
{
   if(pclMessageObject)
   {
      return(pclMessageObject->SetCWTestMode(ucTransmitPower_, ucRFChannel_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Add a channel ID to a channel's include/exclude ID list
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_AddChannelID(UCHAR ucANTChannel_, USHORT usDeviceNumber_, UCHAR ucDeviceType_, UCHAR ucTransmissionType_, UCHAR ucListIndex_)
{
   return ANT_AddChannelID_RTO(ucANTChannel_, usDeviceNumber_, ucDeviceType_, ucTransmissionType_, ucListIndex_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_AddChannelID_RTO(UCHAR ucANTChannel_, USHORT usDeviceNumber_, UCHAR ucDeviceType_, UCHAR ucTransmissionType_, UCHAR ucListIndex_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_AddChannelID_RTO(ucANTChannel_=%d, usDeviceNumber_=%d, ucDeviceType_=%d, ucTransmissionType_=%d, ucListIndex_=%d, ulResponseTime_=%d)", ucANTChannel_, usDeviceNumber_, ucDeviceType_, ucTransmissionType_, ucListIndex_, ulResponseTime_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->AddChannelID(ucANTChannel_, usDeviceNumber_, ucDeviceType_, ucTransmissionType_, ucListIndex_, ulResponseTime_));
   }
   return(FALSE);
}


///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Configure the size and type of a channel's include/exclude ID list
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigList(UCHAR ucANTChannel_, UCHAR ucListSize_, UCHAR ucExclude_)
{
   return ANT_ConfigList_RTO(ucANTChannel_, ucListSize_, ucExclude_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigList_RTO(UCHAR ucANTChannel_, UCHAR ucListSize_, UCHAR ucExclude_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_ConfigList_RTO(ucANTChannel_=%d, ucListSize_=%d, ucExclude_=%d, ulResponseTime_=%d)", ucANTChannel_, ucListSize_, ucExclude_, ulResponseTime_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->ConfigList(ucANTChannel_, ucListSize_, ucExclude_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Open Scan Mode
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_OpenRxScanMode()
{
   return ANT_OpenRxScanMode_RTO(0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_OpenRxScanMode_RTO(ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_OpenRxScanMode_RTO(ulResponseTime_=%d)", ulResponseTime_);
#endif
   if(pclMessageObject)
   {
      return(pclMessageObject->OpenRxScanMode(ulResponseTime_));
   }
   return(FALSE);
}


///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Configure ANT Frequency Agility Functionality (not on AP1 or AT3)
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigFrequencyAgility(UCHAR ucANTChannel_, UCHAR ucFreq1_, UCHAR ucFreq2_, UCHAR ucFreq3_)
{
   return(ANT_ConfigFrequencyAgility_RTO(ucANTChannel_, ucFreq1_, ucFreq2_, ucFreq3_, 0));
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigFrequencyAgility_RTO(UCHAR ucANTChannel_, UCHAR ucFreq1_, UCHAR ucFreq2_, UCHAR ucFreq3_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_ConfigFrequencyAgility_RTO(ucANTChannel_=%d, ucFreq1_=%d, ucFreq2_=%d, ucFreq3_=%d, ulResponseTime_=%d)", ucANTChannel_, ucFreq1_, ucFreq2_, ucFreq3_, ulResponseTime_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->ConfigFrequencyAgility(ucANTChannel_, ucFreq1_, ucFreq2_, ucFreq3_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Configure proximity search (not on AP1 or AT3)
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetProximitySearch(UCHAR ucANTChannel_, UCHAR ucSearchThreshold_)
{
   return(ANT_SetProximitySearch_RTO(ucANTChannel_, ucSearchThreshold_, 0));
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetProximitySearch_RTO(UCHAR ucANTChannel_, UCHAR ucSearchThreshold_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SetProximitySearch_RTO(ucANTChannel_=%d, ucSearchThreshold_=%d, ulResponseTime_=%d)", ucANTChannel_, ucSearchThreshold_, ulResponseTime_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->SetProximitySearch(ucANTChannel_, ucSearchThreshold_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Configure Event Filter (USBm and nRF5 only)
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigEventFilter(USHORT usEventFilter_)
{
   return(ANT_ConfigEventFilter_RTO(usEventFilter_, 0));
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigEventFilter_RTO(USHORT usEventFilter_, ULONG ulResponseTime_)
{
   if(pclMessageObject)
   {
      return(pclMessageObject->ConfigEventFilter(usEventFilter_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Configure Event Buffer (USBm only)
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigEventBuffer(UCHAR ucConfig_, USHORT usSize_, USHORT usTime_)
{
   return(ANT_ConfigEventBuffer_RTO(ucConfig_, usSize_, usTime_, 0));
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigEventBuffer_RTO(UCHAR ucConfig_, USHORT usSize_, USHORT usTime_, ULONG ulResponseTime_)
{
   if(pclMessageObject)
   {
      return(pclMessageObject->ConfigEventBuffer(ucConfig_, usSize_, usTime_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Configure High Duty Search (USBm only)
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigHighDutySearch(UCHAR ucEnable_, UCHAR ucSuppressionCycles_)
{
   return(ANT_ConfigHighDutySearch_RTO(ucEnable_, ucSuppressionCycles_, 0));
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigHighDutySearch_RTO(UCHAR ucEnable_, UCHAR ucSuppressionCycles_, ULONG ulResponseTime_)
{
   if(pclMessageObject)
   {
      return(pclMessageObject->ConfigHighDutySearch(ucEnable_, ucSuppressionCycles_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Configure Selective Data Update (USBm only)
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigSelectiveDataUpdate(UCHAR ucChannel_, UCHAR ucSduConfig_)
{
   return(ANT_ConfigSelectiveDataUpdate_RTO(ucChannel_, ucSduConfig_, 0));
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigSelectiveDataUpdate_RTO(UCHAR ucChannel_, UCHAR ucSduConfig_, ULONG ulResponseTime_)
{
   if(ActionEnabled(ucChannel_))
   {
      return(pclMessageObject->ConfigSelectiveDataUpdate(ucChannel_, ucSduConfig_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Set Selective Data Update Mask (USBm only)
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetSelectiveDataUpdateMask(UCHAR ucMaskNumber_, UCHAR* pucSduMask_)
{
   return(ANT_SetSelectiveDataUpdateMask_RTO(ucMaskNumber_, pucSduMask_, 0));
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetSelectiveDataUpdateMask_RTO(UCHAR ucMaskNumber_, UCHAR* pucSduMask_, ULONG ulResponseTime_)
{
   if(pclMessageObject)
   {
      return(pclMessageObject->SetSelectiveDataUpdateMask(ucMaskNumber_, pucSduMask_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Configure User NVM (USBm only)
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigUserNVM(USHORT usAddress_, UCHAR* pucData_, UCHAR ucSize_)
{
   return(ANT_ConfigUserNVM_RTO(usAddress_, pucData_, ucSize_, 0));
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_ConfigUserNVM_RTO(USHORT usAddress_, UCHAR* pucData_, UCHAR ucSize_, ULONG ulResponseTime_)
{
   if(pclMessageObject)
   {
      return(pclMessageObject->ConfigUserNVM(usAddress_, pucData_, ucSize_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Message to put into DEEP SLEEP (not on AP1 or AT3)
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SleepMessage()
{
   return(ANT_SleepMessage_RTO(0));
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SleepMessage_RTO(ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SleepMessage_RTO(ulResponseTime_=%d)", ulResponseTime_);
#endif
   if(pclMessageObject)
   {
      return(pclMessageObject->SleepMessage(ulResponseTime_));
   }
   return(FALSE);
}


///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Message to put into DEEP SLEEP (not on AP1 or AT3)
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_CrystalEnable()
{
   return(ANT_CrystalEnable_RTO(0));
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_CrystalEnable_RTO(ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_CrystalEnable_RTO(ulResponseTime_=%d)", ulResponseTime_);
#endif
   if(pclMessageObject)
   {
      return(pclMessageObject->CrystalEnable(ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to write NVM data
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_NVM_Write(UCHAR ucSize_, UCHAR *pucData_)
{
   return ANT_NVM_Write_RTO(ucSize_, pucData_ , 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_NVM_Write_RTO(UCHAR ucSize_, UCHAR *pucData_, ULONG ulResponseTime_)
{
   if(pclMessageObject)
   {
      return(pclMessageObject->ScriptWrite(ucSize_,pucData_, ulResponseTime_ ));
   }
   return(FALSE);
}



///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to clear NVM data
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_NVM_Clear(UCHAR ucSectNumber_)
{
   return ANT_NVM_Clear_RTO(ucSectNumber_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_NVM_Clear_RTO(UCHAR ucSectNumber_, ULONG ulResponseTime_)
//Sector number is useless here, but is still here for backwards compatibility
{
   if(pclMessageObject)
   {
      return(pclMessageObject->ScriptClear(ulResponseTime_));
   }
   return(FALSE);
}



///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to set default NVM sector
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_NVM_SetDefaultSector(UCHAR ucSectNumber_)
{
   return ANT_NVM_SetDefaultSector_RTO(ucSectNumber_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_NVM_SetDefaultSector_RTO(UCHAR ucSectNumber_, ULONG ulResponseTime_)
{
   if(pclMessageObject)
   {
      return(pclMessageObject->ScriptSetDefaultSector(ucSectNumber_, ulResponseTime_));
   }
   return(FALSE);
}



///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to end NVM sector
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_NVM_EndSector()
{
   return ANT_NVM_EndSector_RTO(0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_NVM_EndSector_RTO(ULONG ulResponseTime_)
{
   if(pclMessageObject)
   {
      return(pclMessageObject->ScriptEndSector(ulResponseTime_));
   }
   return(FALSE);
}



///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to dump the contents of the NVM
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_NVM_Dump()
{
   return ANT_NVM_Dump_RTO(0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_NVM_Dump_RTO(ULONG ulResponseTime_)
//Response time is useless here, but is kept for backwards compatibility
{
   if(pclMessageObject)
   {
      pclMessageObject->ScriptDump();
     return TRUE;
   }
   return(FALSE);
}



///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to lock the contents of the NVM
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_NVM_Lock()
{
   return ANT_NVM_Lock_RTO(0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_NVM_Lock_RTO(ULONG ulResponseTimeout_)
{
   if(pclMessageObject)
   {
      return(pclMessageObject->ScriptLock(ulResponseTimeout_));
   }
   return(FALSE);
}


///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to set the state of the FE (FIT1e)
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL FIT_SetFEState(UCHAR ucFEState_)
{
   return FIT_SetFEState_RTO(ucFEState_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL FIT_SetFEState_RTO(UCHAR ucFEState_, ULONG ulResponseTime_)
{
   if(pclMessageObject)
   {
      return(pclMessageObject->FITSetFEState(ucFEState_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to set the pairing distance (FIT1e)
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL FIT_AdjustPairingSettings(UCHAR ucSearchLv_, UCHAR ucPairLv_, UCHAR ucTrackLv_)
{
   return FIT_AdjustPairingSettings_RTO(ucSearchLv_, ucPairLv_, ucTrackLv_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL FIT_AdjustPairingSettings_RTO(UCHAR ucSearchLv_, UCHAR ucPairLv_, UCHAR ucTrackLv_, ULONG ulResponseTime_)
{
   if(pclMessageObject)
   {
      return(pclMessageObject->FITAdjustPairingSettings(ucSearchLv_, ucPairLv_, ucTrackLv_, ulResponseTime_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to construct and send an extended broadcast data message.
// This message will be broadcast on the next synchronous channel period.
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SendExtBroadcastData(UCHAR ucANTChannel_, UCHAR *pucData_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SendExtBroadcastData(ucANTChannel_=%d)", ucANTChannel_);
#endif
    if (ActionEnabled(ucANTChannel_))
       return pclMessageObject->SendExtBroadcastData(ucANTChannel_, pucData_);
    return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to construct and send an extended acknowledged data
// mesg.  This message will be transmitted on the next synchronous channel
// period.
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SendExtAcknowledgedData(UCHAR ucANTChannel_, UCHAR *pucData_)
{
   return ANT_SendExtAcknowledgedData_RTO(ucANTChannel_, pucData_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SendExtAcknowledgedData_RTO(UCHAR ucANTChannel_, UCHAR *pucData_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SendExtAcknowledgedData_RTO(ucANTChannel_=%d, ulResponseTime_=%d)", ucANTChannel_, ulResponseTime_);
#endif
    if (ActionEnabled(ucANTChannel_))
       return (ANTFRAMER_PASS == pclMessageObject->SendExtAcknowledgedData(ucANTChannel_, pucData_, ulResponseTime_));
    return(FALSE);
}


///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Called by the application to construct and send extended burst data
// mesg.
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Used to send extended burst data with individual packets.  Proper sequence number
// of packet is maintained by the application.
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SendExtBurstTransferPacket(UCHAR ucANTChannelSeq_, UCHAR *pucData_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SendExtBurstTransferPacket(ucANTChannelSeq_=%d)", ucANTChannelSeq_);
#endif
   if(pclMessageObject)
   {
      ANT_MESSAGE stMessage;

      stMessage.ucMessageID = MESG_EXT_BURST_DATA_ID;
      stMessage.aucData[0] = ucANTChannelSeq_;
      memcpy(&stMessage.aucData[1],pucData_, MESG_EXT_DATA_SIZE-1);

      return pclMessageObject->WriteMessage(&stMessage, MESG_EXT_DATA_SIZE);
   }
   return(FALSE);
}
///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Used to send extended burst data using a block of data.  Proper sequence number
// of packet is maintained by the function.  Useful for testing purposes.
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
USHORT ANT_SendExtBurstTransfer(UCHAR ucANTChannel_, UCHAR *pucData_, USHORT usDataPackets_)
{
   return ANT_SendExtBurstTransfer_RTO(ucANTChannel_, pucData_, usDataPackets_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
USHORT ANT_SendExtBurstTransfer_RTO(UCHAR ucANTChannel_, UCHAR *pucData_, USHORT usDataPackets_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SendExtBurstTransfer_RTO(ucANTChannel_=%d, usDataPackets_=%d, ulResponseTime_=%d)", ucANTChannel_, usDataPackets_, ulResponseTime_);
#endif
    if (ActionEnabled(ucANTChannel_))
       return (ANTFRAMER_PASS == pclMessageObject->SendExtBurstTransfer(ucANTChannel_, pucData_, usDataPackets_*8, ulResponseTime_));
   return(FALSE);
}


///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Used to force the module to use extended rx messages all the time
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_RxExtMesgsEnable(UCHAR ucEnable_)
{
   return ANT_RxExtMesgsEnable_RTO(ucEnable_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_RxExtMesgsEnable_RTO(UCHAR ucEnable_, ULONG ulResponseTimeout_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_RxExtMesgsEnable_RTO(ucEnable_=%d, ulResponseTimeout_=%d)", ucEnable_, ulResponseTimeout_);
#endif
   if(pclMessageObject)
   {
      return(pclMessageObject->RxExtMesgsEnable(ucEnable_, ulResponseTimeout_));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Used to set a channel device ID to the module serial number
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetSerialNumChannelId(UCHAR ucANTChannel_, UCHAR ucDeviceType_, UCHAR ucTransmissionType_)
{
   return ANT_SetSerialNumChannelId_RTO(ucANTChannel_, ucDeviceType_, ucTransmissionType_, 0);
}


///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetSerialNumChannelId_RTO(UCHAR ucANTChannel_, UCHAR ucDeviceType_, UCHAR ucTransmissionType_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_SetSerialNumChannelId_RTO(ucANTChannel_=%d, ucDeviceType_=%d, ucTransmissionType_=%d, ulResponseTime_=%d)", ucANTChannel_, ucDeviceType_, ucTransmissionType_, ulResponseTime_);
#endif
   if(ActionEnabled(ucANTChannel_))
   {
      return(pclMessageObject->SetSerialNumChannelId(ucANTChannel_, ucDeviceType_, ucTransmissionType_, ulResponseTime_));
   }
   return(FALSE);
}


///////////////////////////////////////////////////////////////////////
// Priority: Any
//
// Enables the module LED to flash on RF activity
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_EnableLED(UCHAR ucEnable_)
{
   return ANT_EnableLED_RTO(ucEnable_, 0);
}

///////////////////////////////////////////////////////////////////////
// Response TimeOut Version
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_EnableLED_RTO(UCHAR ucEnable_, ULONG ulResponseTime_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_EnableLED_RTO(ucEnable_=%d, ulResponseTime_=%d)", ucEnable_, ulResponseTime_);
#endif
   if(pclMessageObject)
   {
      return(pclMessageObject->EnableLED(ucEnable_, ulResponseTime_));
   }
   return(FALSE);
}



///////////////////////////////////////////////////////////////////////
// Called by the application to get the product string and serial number string (four bytes) of a particular device
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_GetDeviceUSBInfo(UCHAR ucDeviceNum, UCHAR* pucProductString, UCHAR* pucSerialString)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_GetDeviceUSBInfo(ucDeviceNum=%d)", ucDeviceNum);
#endif
   if(pclMessageObject)
   {
      return(pclMessageObject->GetDeviceUSBInfo(ucDeviceNum, pucProductString, pucSerialString, USB_MAX_STRLEN));
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Called by the application to get the USB PID
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_GetDeviceUSBPID(USHORT* pusPID_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_GetDeviceUSBPID()");
#endif
   if(pclMessageObject)
   {
      return(pclMessageObject->GetDeviceUSBPID(*pusPID_));
   }
   return (FALSE);
}

///////////////////////////////////////////////////////////////////////
// Called by the application to get the USB VID
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_GetDeviceUSBVID(USHORT* pusVID_)
{
   if(pclMessageObject)
   {
      return(pclMessageObject->GetDeviceUSBVID(*pusVID_));
   }
   return (FALSE);
}



////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////
// The following are the Integrated ANTFS_Client functions
////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////

//Memory Device Commands/////////////
extern "C" EXPORT
BOOL ANTFS_InitEEPROMDevice(USHORT usPageSize_, UCHAR ucAddressConfig_)
{
   return (pclMessageObject->InitEEPROMDevice(usPageSize_, ucAddressConfig_, 3000));
}

//File System Commands//////////////
extern "C" EXPORT
BOOL ANTFS_InitFSMemory()
{
   return (pclMessageObject->InitFSMemory(3000));
}

extern "C" EXPORT
BOOL ANTFS_FormatFSMemory(USHORT usNumberOfSectors_, USHORT usPagesPerSector_)
{
   return (pclMessageObject->FormatFSMemory(usNumberOfSectors_, usPagesPerSector_, 3000));
}

extern "C" EXPORT
BOOL ANTFS_SaveDirectory()
{
   return (pclMessageObject->SaveDirectory(3000));
}

extern "C" EXPORT
BOOL ANTFS_DirectoryRebuild()
{
   return (pclMessageObject->DirectoryRebuild(3000));
}

extern "C" EXPORT
BOOL ANTFS_FileDelete(UCHAR ucFileHandle_)
{
   return (pclMessageObject->FileDelete(ucFileHandle_, 3000));
}

extern "C" EXPORT
BOOL ANTFS_FileClose(UCHAR ucFileHandle_)
{
   return (pclMessageObject->FileClose(ucFileHandle_, 3000));
}

extern "C" EXPORT
BOOL ANTFS_SetFileSpecificFlags(UCHAR ucFileHandle_, UCHAR ucFlags_)
{
   return (pclMessageObject->SetFileSpecificFlags(ucFileHandle_, ucFlags_, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_DirectoryReadLock(BOOL bLock_)
{
   return (pclMessageObject->DirectoryReadLock(bLock_, 3000));
}

extern "C" EXPORT
BOOL ANTFS_SetSystemTime(ULONG ulTime_)
{
   return (pclMessageObject->SetSystemTime(ulTime_, 3000));
}



//File System Requests////////////
extern "C" EXPORT
ULONG ANTFS_GetUsedSpace()
{
   return (pclMessageObject->GetUsedSpace(3000));
}

extern "C" EXPORT
ULONG ANTFS_GetFreeSpace()
{
   return (pclMessageObject->GetFreeFSSpace(3000));
}

extern "C" EXPORT
USHORT ANTFS_FindFileIndex(UCHAR ucFileDataType_, UCHAR ucFileSubType_, USHORT usFileNumber_)
{
   return (pclMessageObject->FindFileIndex(ucFileDataType_, ucFileSubType_, usFileNumber_, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_ReadDirectoryAbsolute(ULONG ulOffset_, UCHAR ucSize_, UCHAR* pucBuffer_)
{
   return (pclMessageObject->ReadDirectoryAbsolute(ulOffset_, ucSize_, pucBuffer_,3000));
}

extern "C" EXPORT
UCHAR ANTFS_DirectoryReadEntry (USHORT usFileIndex_, UCHAR* ucFileDirectoryBuffer_)
{
   return (pclMessageObject->DirectoryReadEntry (usFileIndex_, ucFileDirectoryBuffer_, 3000));
}

extern "C" EXPORT
ULONG  ANTFS_DirectoryGetSize()
{
   return (pclMessageObject->DirectoryGetSize(3000));
}

extern "C" EXPORT
USHORT ANTFS_FileCreate(USHORT usFileIndex_, UCHAR ucFileDataType_, ULONG ulFileIdentifier_, UCHAR ucFileDataTypeSpecificFlags_, UCHAR ucGeneralFlags)
{
   return (pclMessageObject->FileCreate(usFileIndex_, ucFileDataType_, ulFileIdentifier_, ucFileDataTypeSpecificFlags_, ucGeneralFlags, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_FileOpen(USHORT usFileIndex_, UCHAR ucOpenFlags_)
{
   return (pclMessageObject->FileOpen(usFileIndex_, ucOpenFlags_, 3000));
}


extern "C" EXPORT
UCHAR ANTFS_FileReadAbsolute(UCHAR ucFileHandle_, ULONG ulOffset_, UCHAR ucReadSize_, UCHAR* pucReadBuffer_)
{
   return (pclMessageObject->FileReadAbsolute(ucFileHandle_, ulOffset_, ucReadSize_, pucReadBuffer_, 3000));
}


extern "C" EXPORT
UCHAR ANTFS_FileReadRelative(UCHAR ucFileHandle_, UCHAR ucReadSize_, UCHAR* pucReadBuffer_)
{
   return (pclMessageObject->FileReadRelative(ucFileHandle_, ucReadSize_, pucReadBuffer_, 3000));
}


extern "C" EXPORT
UCHAR ANTFS_FileWriteAbsolute(UCHAR ucFileHandle_, ULONG ulFileOffset_, UCHAR ucWriteSize_, const UCHAR* pucWriteBuffer_, UCHAR* ucBytesWritten_)
{
   return (pclMessageObject->FileWriteAbsolute(ucFileHandle_, ulFileOffset_, ucWriteSize_, pucWriteBuffer_, ucBytesWritten_, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_FileWriteRelative(UCHAR ucFileHandle_, UCHAR ucWriteSize_, const UCHAR* pucWriteBuffer_, UCHAR* ucBytesWritten_)
{
   return (pclMessageObject->FileWriteRelative(ucFileHandle_, ucWriteSize_, pucWriteBuffer_, ucBytesWritten_, 3000));
}

extern "C" EXPORT
ULONG ANTFS_FileGetSize(UCHAR ucFileHandle_)
{
   return (pclMessageObject->FileGetSize(ucFileHandle_, 3000));
}

extern "C" EXPORT
ULONG ANTFS_FileGetSizeInMem(UCHAR ucFileHandle_)
{
   return (pclMessageObject->FileGetSizeInMem(ucFileHandle_, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_FileGetSpecificFlags(UCHAR ucFileHandle_)
{
   return (pclMessageObject->FileGetSpecificFlags(ucFileHandle_, 3000));
}

extern "C" EXPORT
ULONG ANTFS_FileGetSystemTime()
{
   return (pclMessageObject->FileGetSystemTime(3000));
}


//FS-Crypto Commands/////////////
extern "C" EXPORT
UCHAR ANTFS_CryptoAddUserKeyIndex(UCHAR ucIndex_,  UCHAR* pucKey_)
{
   return (pclMessageObject->CryptoAddUserKeyIndex(ucIndex_, pucKey_, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_CryptoSetUserKeyIndex(UCHAR ucIndex_)
{
   return (pclMessageObject->CryptoSetUserKeyIndex(ucIndex_, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_CryptoSetUserKeyVal(UCHAR* pucKey_)
{
   return (pclMessageObject->CryptoSetUserKeyVal(pucKey_, 3000));
}


//FIT Commands///////////////////////
extern "C" EXPORT
UCHAR ANTFS_FitFileIntegrityCheck(UCHAR ucFileHandle_)
{
   return (pclMessageObject->FitFileIntegrityCheck(ucFileHandle_, 3000));
}


//ANT-FS Commands////////////////////
extern "C" EXPORT
UCHAR ANTFS_OpenBeacon()
{
   return (pclMessageObject->OpenBeacon(3000));
}

extern "C" EXPORT
UCHAR ANTFS_CloseBeacon()
{
   return (pclMessageObject->CloseBeacon(3000));
}

extern "C" EXPORT
UCHAR ANTFS_ConfigBeacon(USHORT usDeviceType_, USHORT usManufacturer_, UCHAR ucAuthType_, UCHAR ucBeaconStatus_)
{
   return (pclMessageObject->ConfigBeacon(usDeviceType_, usManufacturer_, ucAuthType_, ucBeaconStatus_, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_SetFriendlyName(UCHAR ucLength_, const UCHAR* pucString_)
{
   return (pclMessageObject->SetFriendlyName(ucLength_, pucString_, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_SetPasskey(UCHAR ucLength_, const UCHAR* pucString_)
{
   return (pclMessageObject->SetPasskey(ucLength_, pucString_, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_SetBeaconState(UCHAR ucBeaconStatus_)
{
   return (pclMessageObject->SetBeaconState(ucBeaconStatus_, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_PairResponse(BOOL bAccept_)
{
   return (pclMessageObject->PairResponse(bAccept_, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_SetLinkFrequency(UCHAR ucChannelNumber_, UCHAR ucFrequency_)
{
   return (pclMessageObject->SetLinkFrequency(ucChannelNumber_, ucFrequency_, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_SetBeaconTimeout(UCHAR ucTimeout_)
{
   return (pclMessageObject->SetBeaconTimeout(ucTimeout_, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_SetPairingTimeout(UCHAR ucTimeout_)
{
   return (pclMessageObject->SetPairingTimeout(ucTimeout_, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_EnableRemoteFileCreate(BOOL bEnable_)
{
   return (pclMessageObject->EnableRemoteFileCreate(bEnable_, 3000));
}


//ANT-FS Responses////////////////////
extern "C" EXPORT
UCHAR ANTFS_GetCmdPipe(UCHAR ucOffset_, UCHAR ucReadSize_, UCHAR* pucReadBuffer_)
{
   return (pclMessageObject->GetCmdPipe(ucOffset_, ucReadSize_, pucReadBuffer_, 3000));
}

extern "C" EXPORT
UCHAR ANTFS_SetCmdPipe(UCHAR ucOffset_, UCHAR ucWriteSize_, const UCHAR* pucWriteBuffer_)
{
   return (pclMessageObject->SetCmdPipe(ucOffset_, ucWriteSize_, pucWriteBuffer_, 3000));
}


//GetFSResponse/////////////////////////
extern "C" EXPORT
UCHAR ANTFS_GetLastError()
{
   return (pclMessageObject->GetLastError());
}


///////////////////////////////////////////////////////////////////////
// Set the directory the log files are saved to
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
BOOL ANT_SetDebugLogDirectory(char* pcDirectory)
{
#if defined(DEBUG_FILE)
   return DSIDebug::SetDirectory(pcDirectory);
#else
   return FALSE;
#endif
}


///////////////////////////////////////////////////////////////////////
// Put current thread to sleep for the specified number of milliseconds
///////////////////////////////////////////////////////////////////////
extern "C" EXPORT
void ANT_Nap(ULONG ulMilliseconds_)
{
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("ANT_Nap(ulMilliseconds_=%d)", ulMilliseconds_);
#endif
   DSIThread_Sleep(ulMilliseconds_);
}

// Local functions ****************************************************//
static DSI_THREAD_RETURN MessageThread(void *pvParameter_)
{
   ANT_MESSAGE stMessage;
   USHORT usSize;

   eTheThread = DSIThread_GetCurrentThreadIDNum();
#if defined(DEBUG_FILE)
   DSIDebug::ThreadInit("ant_msg_thread.txt");
   DSIDebug::ThreadEnable(TRUE);
   DSIDebug::ThreadPrintf("Started");
#endif

   while(bGoThread)
   {
      if(pclMessageObject->WaitForMessage(1000/*DSI_THREAD_INFINITE*/))
      {
         usSize = pclMessageObject->GetMessage(&stMessage);

         if(usSize == DSI_FRAMER_ERROR)
         {
            // Get the message to clear the error
            usSize = pclMessageObject->GetMessage(&stMessage, MESG_MAX_SIZE_VALUE);
            continue;
         }

         if(usSize != 0 && usSize != DSI_FRAMER_ERROR && usSize != DSI_FRAMER_TIMEDOUT)
         {
            SerialHaveMessage(stMessage, usSize);
         }
      }
   }

   DSIThread_MutexLock(&mutexTestDone);
   UCHAR ucCondResult = DSIThread_CondSignal(&condTestDone);
   assert(ucCondResult == DSI_THREAD_ENONE);
   DSIThread_MutexUnlock(&mutexTestDone);

   return(NULL);
}

//Called internally to delete objects from memory
static void MemoryCleanup(void)
{
   if(pclSerialObject)
   {
      //Close all stuff
      pclSerialObject->Close();
      delete pclSerialObject;
      pclSerialObject = NULL;
   }

   if(pclMessageObject)
   {
      delete pclMessageObject;
      pclMessageObject = NULL;
   }
}

typedef BOOL(*RESPONSE_FUNC)(UCHAR ucANTChannel_, UCHAR ucResponseMsgID);
void CallResponseFunction(UCHAR ucANTChannel_, UCHAR ucResponseMsgID) {
#ifdef ENABLE_EDGE_REMOTE
    if (glbZWIFT_EDGE_REMOTE && ucANTChannel_ == 7) {
#if defined(DEBUG_FILE)
        DSIDebug::ThreadPrintf("CallResponseFunction(edge,glbEdgeRemoteBroadcasting=%d)(ucANTChannel_=%d,ucResponseMsgID=%d)", glbEdgeRemoteBroadcasting, ucANTChannel_, ucResponseMsgID);
#endif
        switch (ucResponseMsgID) {
        case MESG_RESPONSE_EVENT_ID:
            switch (glbEdgeRemoteChannelRxBuffer[1/*MESSAGE_BUFFER_DATA2_INDEX*/]) {
            case MESG_CLOSE_CHANNEL_ID:
                if (glbEdgeRemoteChannelRxBuffer[2/*MESSAGE_BUFFER_DATA3_INDEX*/] == CHANNEL_IN_WRONG_STATE) {
                    ANT_UnAssignChannel(ucANTChannel_);
                    break;
                }
                break;
#if 0
            case MESG_CHANNEL_RADIO_FREQ_ID:
                if (glbEdgeRemoteChannelRxBuffer[2/*MESSAGE_BUFFER_DATA3_INDEX*/] != RESPONSE_NO_ERROR) {
#if defined(DEBUG_FILE)
                    DSIDebug::ThreadPrintf("Error configuring Radio Frequency: Code 0%d", glbEdgeRemoteChannelRxBuffer[2/*MESSAGE_BUFFER_DATA3_INDEX*/]);
#endif
                    break;
                }
                glbEdgeRemoteBroadcasting = TRUE;
                ANT_OpenChannel(ucANTChannel_);
                break;
            case MESG_NETWORK_KEY_ID:
                if (glbEdgeRemoteChannelRxBuffer[2/*MESSAGE_BUFFER_DATA3_INDEX*/] != RESPONSE_NO_ERROR) {
#if defined(DEBUG_FILE)
                    DSIDebug::ThreadPrintf("Error configuring network key: Code 0%d", glbEdgeRemoteChannelRxBuffer[2/*MESSAGE_BUFFER_DATA3_INDEX*/]);
#endif
                    break;
                }
                //if (ucChannelType == CHANNEL_TYPE_MASTER)                {
                /*    bSuccess =*/ ANT_AssignChannel(ucANTChannel_, PARAMETER_TX_NOT_RX, 0/*USER_NETWORK_NUM*/);
                //}
                //else if (ucChannelType == CHANNEL_TYPE_SLAVE)                {
                //    bSuccess = ANT_AssignChannel(USER_ANTCHANNEL, 0, USER_NETWORK_NUM);
                //}
                break;
            case MESG_ASSIGN_CHANNEL_ID:
                if (glbEdgeRemoteChannelRxBuffer[2/*MESSAGE_BUFFER_DATA3_INDEX*/] != RESPONSE_NO_ERROR) {
#if defined(DEBUG_FILE)
                    DSIDebug::ThreadPrintf("Error assigning channel: Code 0%d", glbEdgeRemoteChannelRxBuffer[2/*MESSAGE_BUFFER_DATA3_INDEX*/]);
#endif
                    break;
                }
                ANT_SetChannelId(ucANTChannel_, USER_DEVICENUM, USER_DEVICETYPE, USER_TRANSTYPE);
                break;
            case MESG_CHANNEL_ID_ID:
                if (glbEdgeRemoteChannelRxBuffer[2/*MESSAGE_BUFFER_DATA3_INDEX*/] != RESPONSE_NO_ERROR) {
#if defined(DEBUG_FILE)
                    DSIDebug::ThreadPrintf("Error configuring Channel ID: Code 0%d", glbEdgeRemoteChannelRxBuffer[2/*MESSAGE_BUFFER_DATA3_INDEX*/]);
#endif
                    break;
                }
                ANT_SetChannelRFFreq(ucANTChannel_, USER_RADIOFREQ);
                break;
            case MESG_OPEN_CHANNEL_ID:
                if (glbEdgeRemoteChannelRxBuffer[2/*MESSAGE_BUFFER_DATA3_INDEX*/] != RESPONSE_NO_ERROR) {
                    glbEdgeRemoteBroadcasting = FALSE;
                    break;
                }
                ANT_RxExtMesgsEnable(TRUE);
                break;
            }
#endif
            }
        }
    } else
#endif //ENABLE_EDGE_REMOTE
        if (pfResponseFunc) {
#if defined(DEBUG_FILE)
        DSIDebug::ThreadPrintf("pfResponseFunc(ucANTChannel_=%d,ucResponseMsgID=%d)", ucANTChannel_, ucResponseMsgID);
#endif
            pfResponseFunc(ucANTChannel_, ucResponseMsgID);
            return;
        }
#if defined(DEBUG_FILE)
    DSIDebug::ThreadPrintf("pfResponseFunc == nullptr");
#endif
}
///////////////////////////////////////////////////////////////////////
//
// Processes a received serial message.  This function is intended to be
// called by the serial message driver code, to be defined by the user,
// when a serial message is received from the ANT module.
///////////////////////////////////////////////////////////////////////
static void SerialHaveMessage(ANT_MESSAGE& stMessage_, USHORT usSize_)
{
   UCHAR ucANTChannel_;

   //If no response function has been assigned, ignore the message and unlock
   //the receive buffer
   if (pfResponseFunc == NULL)
      return;


   ucANTChannel_ = stMessage_.aucData[MESG_CHANNEL_OFFSET] & CHANNEL_NUMBER_MASK;


   //Process the message to determine whether it is a response event or one
   //of the channel events and call the appropriate event function.
   switch (stMessage_.ucMessageID)
   {
      case MESG_RESPONSE_EVENT_ID:
      {
         if (stMessage_.aucData[MESG_EVENT_ID_OFFSET] != MESG_EVENT_ID) // this is a response
         {
            if (pucResponseBuffer)
            {
               memcpy(pucResponseBuffer, stMessage_.aucData, MESG_RESPONSE_EVENT_SIZE);
               CallResponseFunction(ucANTChannel_, MESG_RESPONSE_EVENT_ID);
            }
         }
         else // this is an event
         {
            // If we are in auto transfer mode, stop sending packets
            if ((stMessage_.aucData[MESG_EVENT_CODE_OFFSET] == EVENT_TRANSFER_TX_FAILED) && (ucAutoTransferChannel == ucANTChannel_))
               usNumDataPackets = 0;

            if (sLink[ucANTChannel_].pfLinkEvent == NULL)
               break;

            memcpy(sLink[ucANTChannel_].pucRxBuffer, stMessage_.aucData, usSize_);
            sLink[ucANTChannel_].pfLinkEvent(ucANTChannel_, stMessage_.aucData[MESG_EVENT_CODE_OFFSET]); // pass through any events not handled here
         }
         break;
      }
      case MESG_BROADCAST_DATA_ID:
      {
         if (  sLink[ucANTChannel_].pfLinkEvent == NULL ||
               sLink[ucANTChannel_].pucRxBuffer == NULL)
         {
            break;
         }

         // If size is greater than the standard data message size, then assume
         // that this is a data message with a flag at the end. Set the event accordingly.
         if(usSize_ > MESG_DATA_SIZE)
         {
            //Call channel event function with Broadcast message code
            memcpy(sLink[ucANTChannel_].pucRxBuffer, stMessage_.aucData, usSize_);
            sLink[ucANTChannel_].pfLinkEvent(ucANTChannel_, EVENT_RX_FLAG_BROADCAST);                 // process the event
         }
         else
         {
            //Call channel event function with Broadcast message code
            memcpy(sLink[ucANTChannel_].pucRxBuffer, stMessage_.aucData, ANT_STANDARD_DATA_PAYLOAD_SIZE + MESG_CHANNEL_NUM_SIZE);
            sLink[ucANTChannel_].pfLinkEvent(ucANTChannel_, EVENT_RX_BROADCAST);                 // process the event
         }
         break;

      }

      case MESG_ACKNOWLEDGED_DATA_ID:
      {
         if (sLink[ucANTChannel_].pfLinkEvent == NULL)
            break;


         if(usSize_ > MESG_DATA_SIZE)
         {
            //Call channel event function with Broadcast message code
            memcpy(sLink[ucANTChannel_].pucRxBuffer, stMessage_.aucData, usSize_);
            sLink[ucANTChannel_].pfLinkEvent(ucANTChannel_, EVENT_RX_FLAG_ACKNOWLEDGED);                 // process the event
         }
         else
         {
            //Call channel event function with Acknowledged message code
            memcpy(sLink[ucANTChannel_].pucRxBuffer, stMessage_.aucData, ANT_STANDARD_DATA_PAYLOAD_SIZE + MESG_CHANNEL_NUM_SIZE);
            sLink[ucANTChannel_].pfLinkEvent(ucANTChannel_, EVENT_RX_ACKNOWLEDGED);                 // process the message
         }
         break;
      }
      case MESG_BURST_DATA_ID:
      {
         if (sLink[ucANTChannel_].pfLinkEvent == NULL)
            break;

         if(usSize_ > MESG_DATA_SIZE)
         {
            //Call channel event function with Broadcast message code
            memcpy(sLink[ucANTChannel_].pucRxBuffer, stMessage_.aucData, usSize_);
            sLink[ucANTChannel_].pfLinkEvent(ucANTChannel_, EVENT_RX_FLAG_BURST_PACKET);                 // process the event
         }
         else
         {
            //Call channel event function with Burst message code
            memcpy(sLink[ucANTChannel_].pucRxBuffer, stMessage_.aucData, ANT_STANDARD_DATA_PAYLOAD_SIZE + MESG_CHANNEL_NUM_SIZE);
            sLink[ucANTChannel_].pfLinkEvent(ucANTChannel_, EVENT_RX_BURST_PACKET);                 // process the message
         }
         break;
      }
      case MESG_EXT_BROADCAST_DATA_ID:
      {
         if (sLink[ucANTChannel_].pfLinkEvent == NULL)
            break;

         //Call channel event function with Broadcast message code
         memcpy(sLink[ucANTChannel_].pucRxBuffer, stMessage_.aucData, MESG_EXT_DATA_SIZE);
         sLink[ucANTChannel_].pfLinkEvent(ucANTChannel_, EVENT_RX_EXT_BROADCAST);                 // process the event
         break;
      }
      case MESG_EXT_ACKNOWLEDGED_DATA_ID:
      {
         if (sLink[ucANTChannel_].pfLinkEvent == NULL)
            break;

         //Call channel event function with Acknowledged message code
         memcpy(sLink[ucANTChannel_].pucRxBuffer, stMessage_.aucData, MESG_EXT_DATA_SIZE);
         sLink[ucANTChannel_].pfLinkEvent(ucANTChannel_, EVENT_RX_EXT_ACKNOWLEDGED);              // process the message
         break;
      }
      case MESG_EXT_BURST_DATA_ID:
      {
         if (sLink[ucANTChannel_].pfLinkEvent == NULL)
            break;

         //Call channel event function with Burst message code
         memcpy(sLink[ucANTChannel_].pucRxBuffer, stMessage_.aucData, MESG_EXT_DATA_SIZE);
         sLink[ucANTChannel_].pfLinkEvent(ucANTChannel_, EVENT_RX_EXT_BURST_PACKET);                 // process the message
         break;
      }
      case MESG_RSSI_BROADCAST_DATA_ID:
      {
         if (sLink[ucANTChannel_].pfLinkEvent == NULL)
            break;

         //Call channel event function with Broadcast message code
         memcpy(sLink[ucANTChannel_].pucRxBuffer, stMessage_.aucData, MESG_RSSI_DATA_SIZE);
         sLink[ucANTChannel_].pfLinkEvent(ucANTChannel_, EVENT_RX_RSSI_BROADCAST);                 // process the event
         break;
      }
      case MESG_RSSI_ACKNOWLEDGED_DATA_ID:
      {
         if (sLink[ucANTChannel_].pfLinkEvent == NULL)
            break;

         //Call channel event function with Acknowledged message code
         memcpy(sLink[ucANTChannel_].pucRxBuffer, stMessage_.aucData, MESG_RSSI_DATA_SIZE);
         sLink[ucANTChannel_].pfLinkEvent(ucANTChannel_, EVENT_RX_RSSI_ACKNOWLEDGED);                 // process the message
         break;
      }
      case MESG_RSSI_BURST_DATA_ID:
      {
         if (sLink[ucANTChannel_].pfLinkEvent == NULL)
            break;

         //Call channel event function with Burst message code
         memcpy(sLink[ucANTChannel_].pucRxBuffer, stMessage_.aucData, MESG_RSSI_DATA_SIZE);
         sLink[ucANTChannel_].pfLinkEvent(ucANTChannel_, EVENT_RX_RSSI_BURST_PACKET);                 // process the message
         break;
      }

      case MESG_SCRIPT_DATA_ID:
      {
         if (pucResponseBuffer)
         {
            memcpy(pucResponseBuffer, stMessage_.aucData, usSize_);
            pucResponseBuffer[10] = (UCHAR)usSize_;
            CallResponseFunction(ucANTChannel_, MESG_SCRIPT_DATA_ID);
         }
         break;
      }
      case MESG_SCRIPT_CMD_ID:
      {
         if (pucResponseBuffer)
         {
            memcpy(pucResponseBuffer, stMessage_.aucData, MESG_SCRIPT_CMD_SIZE);
            CallResponseFunction(ucANTChannel_, MESG_SCRIPT_CMD_ID);
         }
         break;
      }
      default:
      {
         if (pucResponseBuffer)                     // can we process this link
         {
            memcpy(pucResponseBuffer, stMessage_.aucData, usSize_);
            CallResponseFunction(ucANTChannel_, stMessage_.ucMessageID );
         }
         break;
      }
   }

   return;
}
