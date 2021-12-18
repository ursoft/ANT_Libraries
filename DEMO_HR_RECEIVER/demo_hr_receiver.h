/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/
#ifndef __HR_RECEIVER_H__
#define __HR_RECEIVER_H__

#include "types.h"
#include "dsi_framer_ant.hpp"
//#include "dsi_thread.h"
#include "dsi_serial_generic.hpp"
//#include "dsi_debug.hpp"
#include <stdio.h>
//#include <stdlib.h>
#include <assert.h>
//#include <string.h>

#define CHANNEL_TYPE_MASTER       (0)
#define CHANNEL_TYPE_SLAVE        (1)
#define CHANNEL_TYPE_INVALID      (2)

#define ENABLE_EXTENDED_MESSAGES
#define USER_BAUDRATE         (57600)  // For AT3/AP2, use 57600

// Page decoding constants
#define TOGGLE_MASK           ((UCHAR)0x80) // For removing the toggle bit for HRM decoding
#define INVALID_TOGGLE_BIT    ((BOOL)0xFF) // Invalid BOOL, can be initialised because BOOL is just int
#define PAGE_0                ((UCHAR)0x00)
#define PAGE_1                ((UCHAR)0x01)
#define PAGE_2                ((UCHAR)0x02)
#define PAGE_3                ((UCHAR)0x03)
#define PAGE_4                ((UCHAR)0x04)
#define MAX_TOGGLE_ATTEMPTS   ((UCHAR)0x06) // 1 extra to provide some margin
#define LEGACY_DEVICE         ((UCHAR)0x10) // Arbitrarily chosen definition
#define CURRENT_DEVICE        ((UCHAR)0x11) // Arbitrarily chosen definition
#define INVALID_DEVICE        ((UCHAR)0xFF) // Arbitrarily chosen definition

// Channel configuration specs for HRM Receiever (including network config defaults)
#define USER_ANTCHANNEL       (0) // Arbitrarily chosen as default
#define USER_NETWORK_KEY      {0xB9, 0xA5, 0x21, 0xFB, 0xBD, 0x72, 0xC3, 0x45} // ANT+ network key
#define USER_RADIOFREQ        (57) // ANT+ spec
#define USER_TRANSTYPE        (0) // Wildcarded for pairing (default)
#define USER_DEVICETYPE       (120) // ANT+ HRM
#define USER_DEVICENUM        (0) // Wildcarded for pairing (default)
#define MESSAGE_TIMEOUT       (12) // = 12*2.5 = 30 seconds
#define USER_NETWORK_NUM      (0) // The network key is assigned to this network number (default)

// Permitted ANT+ HRM Message periods
#define USER_MESSAGE_PERIODS    {(USHORT)8070, (USHORT)16140, (USHORT)32280}

// Indexes into message recieved from ANT
#define MESSAGE_BUFFER_DATA1_INDEX ((UCHAR) 0)
#define MESSAGE_BUFFER_DATA2_INDEX ((UCHAR) 1)
#define MESSAGE_BUFFER_DATA3_INDEX ((UCHAR) 2)
#define MESSAGE_BUFFER_DATA4_INDEX ((UCHAR) 3)
#define MESSAGE_BUFFER_DATA5_INDEX ((UCHAR) 4)
#define MESSAGE_BUFFER_DATA6_INDEX ((UCHAR) 5)
#define MESSAGE_BUFFER_DATA7_INDEX ((UCHAR) 6)
#define MESSAGE_BUFFER_DATA8_INDEX ((UCHAR) 7)
#define MESSAGE_BUFFER_DATA9_INDEX ((UCHAR) 8)
#define MESSAGE_BUFFER_DATA10_INDEX ((UCHAR) 9)
#define MESSAGE_BUFFER_DATA11_INDEX ((UCHAR) 10)
#define MESSAGE_BUFFER_DATA12_INDEX ((UCHAR) 11)
#define MESSAGE_BUFFER_DATA13_INDEX ((UCHAR) 12)
#define MESSAGE_BUFFER_DATA14_INDEX ((UCHAR) 13)

////////////////////////////////////////////////////////////////////////////////
// HRMReceiver (Heart Rate Receiver) class.
////////////////////////////////////////////////////////////////////////////////
class HRMReceiver {
public:
   HRMReceiver();
   virtual ~HRMReceiver();
   BOOL Init(UCHAR ucDeviceNumber_);
   void Start();
   void Close();

private:
   BOOL InitANT();

   // Starts the Message thread.
   static DSI_THREAD_RETURN RunMessageThread(void *pvParameter_);

   // Listens for a response from the module
   void MessageThread();
   // Decodes the received message
   void ProcessMessage(ANT_MESSAGE stMessage, USHORT usSize_);

   // Print user menu
   void PrintMenu();

   // Detect transmitter device type: current or legacy
   void DetectDevice(UCHAR &ucDeviceType_, BOOL &bOldToggleBit_, UCHAR &ucToggleAttempts_, BOOL bToggleBit);

   // Network variables
   UCHAR ucAntChannel;
   UCHAR ucTransType;
   USHORT usDeviceNum;
   UCHAR ucNetworkNum;
   USHORT usMessagePeriod;

   BOOL bBursting; // Holds whether the bursting phase of the test has started
   BOOL bBroadcasting;
   BOOL bMyDone;
   BOOL bDone;
   UCHAR ucChannelType;
   DSISerialGeneric* pclSerialObject;
   DSIFramerANT* pclMessageObject;
   DSI_THREAD_ID uiDSIThread;
   DSI_CONDITION_VAR condTestDone;
   DSI_MUTEX mutexTestDone;

   BOOL bDisplay;
   BOOL bProcessedData;
};

#endif
