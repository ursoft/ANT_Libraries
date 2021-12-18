/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/
#ifndef _DEMO_H_
#define _DEMO_H_

#include "types.h"
#include "dsi_framer_ant.hpp"
#include "dsi_thread.h"
#include "dsi_serial_generic.hpp"

BOOL bCaughtSig = FALSE;

class Demo {
public:
   Demo();
   virtual ~Demo();
   BOOL GetUserInfo();
   BOOL Init(UCHAR ucDeviceNumberUSB_);
   void Start();
   void HeartBeat();
   void Close();

private:
   BOOL InitANT();

   //Starts the Message thread.
   static DSI_THREAD_RETURN RunMessageThread(void *pvParameter_);

   //Listens for a response from the module
   void MessageThread();
   //Decodes the received message
   void ProcessMessage(ANT_MESSAGE stMessage, USHORT usSize_);

   //check if we've had a a ctrl-c,
   static BOOL CtrlHandler( DWORD fdwCtrlType );


   BOOL bBroadcasting;
   BOOL bMyDone;
   BOOL bDone;
   BOOL bPrintAllMessages;
   UCHAR ucChannelType;
   INT siDeviceNumberANT;
   DSISerialGeneric* pclSerialObject;
   DSIFramerANT* pclMessageObject;
   DSI_THREAD_ID uiDSIThread;
   DSI_CONDITION_VAR condTestDone;
   DSI_MUTEX mutexTestDone;

   //variables to hold HRM data
   USHORT usHeartBeatEventTime;
   UCHAR  ucHeartBeatCount;
   UCHAR  ucHeartRate;

   ULONGLONG ullTimeSinceStart;
   ULONGLONG ullStartupTime;

   UCHAR aucTransmitBuffer[ANT_STANDARD_DATA_PAYLOAD_SIZE];


// UCHAR pucData[USER_DATA_LENGTH];  //data to transfer in a burst


};

#endif //TEST_H
