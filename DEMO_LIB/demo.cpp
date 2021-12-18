/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/
#include "demo.h"

#include "types.h"
#include "dsi_framer_ant.hpp"
#include "dsi_thread.h"
#include "dsi_serial_generic.hpp"
#include "dsi_debug.hpp"

#include <stdio.h>
#include <assert.h>
#include <string.h>

#define ENABLE_EXTENDED_MESSAGES

#define USER_BAUDRATE         (50000)  // For AT3/AP2, use 57600
#define USER_RADIOFREQ        (35)

#define USER_ANTCHANNEL       (0)
#define USER_DEVICENUM        (49)
#define USER_DEVICETYPE       (1)
#define USER_TRANSTYPE        (1)

#define USER_NETWORK_KEY      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,}
#define USER_NETWORK_NUM      (0)      // The network key is assigned to this network number

#define MESSAGE_TIMEOUT       (1000)

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
// main
//
// Usage:
//
// c:\DEMO_LIB.exe [device_no] [channel_type]
//
// ... where
//
// device_no:     USB Device port, starting at 0
// channel_type:  Master = 0, Slave = 1
//
// ... example
//
// c:\Demo_LIB.exe 0 0
//
// Comment to USB port 0 and open a Master channel
//
// If optional arguements are not supplied, user will
// be prompted to enter these after the program starts.
//
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
   Demo* pclDemo = new Demo();

   UCHAR ucDeviceNumber = 0xFF;
   UCHAR ucChannelType = CHANNEL_TYPE_INVALID;

   if(argc > 2)
   {
      ucDeviceNumber = (UCHAR) atoi(argv[1]);
      ucChannelType = (UCHAR) atoi(argv[2]);

   }

   if(pclDemo->Init(ucDeviceNumber,ucChannelType))
      pclDemo->Start();
   else
      delete pclDemo;
   return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Demo
//
// Constructor, intializes Demo class
//
////////////////////////////////////////////////////////////////////////////////
Demo::Demo()
{
   ucChannelType = CHANNEL_TYPE_INVALID;
   pclSerialObject = (DSISerialGeneric*)NULL;
   pclMessageObject = (DSIFramerANT*)NULL;
   uiDSIThread = (DSI_THREAD_ID)NULL;
   bMyDone = FALSE;
   bDone = FALSE;
   bDisplay = TRUE;
   bBroadcasting = FALSE;

   memset(aucTransmitBuffer,0,ANT_STANDARD_DATA_PAYLOAD_SIZE);
}

////////////////////////////////////////////////////////////////////////////////
// ~Demo
//
// Destructor, clean up and loose memory
//
////////////////////////////////////////////////////////////////////////////////
Demo::~Demo()
{
   if(pclMessageObject)
      delete pclMessageObject;

   if(pclSerialObject)
      delete pclSerialObject;
}

////////////////////////////////////////////////////////////////////////////////
// Init
//
// Initize the Demo and ANT Library.
//
// ucDeviceNumber_: USB Device Number (0 for first USB stick plugged and so on)
//                  If not specified on command line, 0xFF is passed in as invalid.
// ucChannelType_:  ANT Channel Type. 0 = Master, 1 = Slave
//                  If not specified, 2 is passed in as invalid.
//
////////////////////////////////////////////////////////////////////////////////
BOOL Demo::Init(UCHAR ucDeviceNumber_, UCHAR ucChannelType_)
{

   BOOL bStatus;

   // Initialize condition var and mutex
   UCHAR ucCondInit = DSIThread_CondInit(&condTestDone);
   assert(ucCondInit == DSI_THREAD_ENONE);

   UCHAR ucMutexInit = DSIThread_MutexInit(&mutexTestDone);
   assert(ucMutexInit == DSI_THREAD_ENONE);

#if defined(DEBUG_FILE)
   // Enable logging
   DSIDebug::Init();
   DSIDebug::SetDebug(TRUE);
#endif

   // Create Serial object.
   pclSerialObject = new DSISerialGeneric();
   assert(pclSerialObject);

   // NOTE: Will fail if the module is not available.
   // If no device number was specified on the command line,
   // prompt the user for input.

   if(ucDeviceNumber_ == 0xFF)
   {
      printf("USB Device number?\n"); fflush(stdout);
      char st[1024];
      fgets(st, sizeof(st), stdin);
      sscanf(st, "%u", &ucDeviceNumber_);
   }

   ucChannelType = ucChannelType_;

   // Initialize Serial object.
   // The device number depends on how many USB sticks have been
   // plugged into the PC. The first USB stick plugged will be 0
   // the next 1 and so on.
   //
   // The Baud Rate depends on the ANT solution being used. AP1
   // is 50000, all others are 57600
   bStatus = pclSerialObject->Init(USER_BAUDRATE, ucDeviceNumber_);
   assert(bStatus);

   // Create Framer object.
   pclMessageObject = new DSIFramerANT(pclSerialObject);
   assert(pclMessageObject);

   // Initialize Framer object.
   bStatus = pclMessageObject->Init();
   assert(bStatus);

   // Let Serial know about Framer.
   pclSerialObject->SetCallback(pclMessageObject);

   // Open Serial.
   bStatus = pclSerialObject->Open();

   // If the Open function failed, most likely the device
   // we are trying to access does not exist, or it is connected
   // to another program
   if(!bStatus)
   {
      printf("Failed to connect to device at USB port %d\n", ucDeviceNumber_);
      return FALSE;
   }

   // Create message thread.
   uiDSIThread = DSIThread_CreateThread(&Demo::RunMessageThread, this);
   assert(uiDSIThread);

   printf("Initialization was successful!\n"); fflush(stdout);

   return TRUE;
}


////////////////////////////////////////////////////////////////////////////////
// Close
//
// Close connection to USB stick.
//
////////////////////////////////////////////////////////////////////////////////
void Demo::Close()
{
   //Wait for test to be done
   DSIThread_MutexLock(&mutexTestDone);
   bDone = TRUE;

   UCHAR ucWaitResult = DSIThread_CondTimedWait(&condTestDone, &mutexTestDone, DSI_THREAD_INFINITE);
   assert(ucWaitResult == DSI_THREAD_ENONE);

   DSIThread_MutexUnlock(&mutexTestDone);

   //Destroy mutex and condition var
   DSIThread_MutexDestroy(&mutexTestDone);
   DSIThread_CondDestroy(&condTestDone);

   //Close all stuff
   if(pclSerialObject)
      pclSerialObject->Close();

#if defined(DEBUG_FILE)
   DSIDebug::Close();
#endif

}

////////////////////////////////////////////////////////////////////////////////
// Start
//
// Starts the Demo
//
////////////////////////////////////////////////////////////////////////////////
void Demo::Start()
{
   BOOL bStatus;

   // Print out the menu to start
   PrintMenu();

   // If the channel type has not been set at the command line,
   // prompt the user to specify now.
   do
   {
      if(ucChannelType == CHANNEL_TYPE_INVALID)
      {
         printf("Channel Type? (Master = 0, Slave = 1)\n"); fflush(stdout);
         char st[1024];
         fgets(st, sizeof(st), stdin);
         sscanf(st, "%u", &ucChannelType);
      }
      if(ucChannelType != 0 && ucChannelType != 1)
      {
         ucChannelType = CHANNEL_TYPE_INVALID;
         printf("Error: invalid channel type\n");
      }
   } while(ucChannelType == CHANNEL_TYPE_INVALID);

   // Start ANT channel setup
   bStatus = InitANT();

   while(!bMyDone)
   {

      UCHAR ucChar;
      char st[1024];
      fgets(st, sizeof(st), stdin);
      sscanf(st, "%c", &ucChar);

      switch(ucChar)
      {
         case 'M':
         case 'm':
         {
            // Printout options
            PrintMenu();
            break;
         }
         case 'Q':
         case 'q':
         {
            // Quit
            printf("Closing channel...\n");
            bBroadcasting = FALSE;
            pclMessageObject->CloseChannel(USER_ANTCHANNEL, MESSAGE_TIMEOUT);
            break;
         }
         case 'A':
         case 'a':
         {
            // Send Acknowledged data
            UCHAR aucTempBuffer[] = {1,2,3,4,5,6,7,8};
            pclMessageObject->SendAcknowledgedData( USER_ANTCHANNEL, aucTempBuffer, MESSAGE_TIMEOUT);
            break;
         }
         case 'B':
         case 'b':
         {
            // Send Burst Data (10 packets)
            UCHAR ucSizeBytes = 8*10;
            UCHAR aucTempBuffer[8*10];

            for (int i = 0; i < ucSizeBytes; i++)
               aucTempBuffer[i] = i;

            pclMessageObject->SendTransfer( USER_ANTCHANNEL, aucTempBuffer, ucSizeBytes, MESSAGE_TIMEOUT);
            break;
         }
         case 'r':
         case 'R':
         {
            // Reset the system and start over the test
            bStatus = InitANT();
            break;
         }
         case 'c':
         case 'C':
         {
            // Request capabilites.
            ANT_MESSAGE_ITEM stResponse;
            pclMessageObject->SendRequest(MESG_CAPABILITIES_ID, USER_ANTCHANNEL, &stResponse, 0);
            break;
         }
         case 'v':
         case 'V':
         {
            // Request version
            ANT_MESSAGE_ITEM stResponse;
            pclMessageObject->SendRequest(MESG_VERSION_ID, USER_ANTCHANNEL, &stResponse, 0);
            break;
         }
         case 'S':
         case 's':
         {
            // Request channel status
            ANT_MESSAGE_ITEM stResponse;
            pclMessageObject->SendRequest(MESG_CHANNEL_STATUS_ID, USER_ANTCHANNEL, &stResponse, 0);
            break;
         }
         case 'I':
         case 'i':
         {
            // Request channel ID
            ANT_MESSAGE_ITEM stResponse;
            pclMessageObject->SendRequest(MESG_CHANNEL_ID_ID, USER_ANTCHANNEL, &stResponse, 0);
            break;
         }
         case 'd':
         case 'D':
         {
            // Toggle display of data messages
            bDisplay = !bDisplay;
            break;
         }
         case 'u':
         case 'U':
         {
            // Print out information about the device we are connected to
            printf("USB Device Description\n");
            USHORT usDevicePID;
            USHORT usDeviceVID;
            UCHAR aucDeviceDescription[USB_MAX_STRLEN];
            UCHAR aucDeviceSerial[USB_MAX_STRLEN];
            // Retrieve info
            if(pclMessageObject->GetDeviceUSBVID(usDeviceVID))
            {
               printf("  VID: 0x%X\n", usDeviceVID);
            }
            if(pclMessageObject->GetDeviceUSBPID(usDevicePID))
            {
               printf("  PID: 0x%X\n", usDevicePID);
            }
            if(pclMessageObject->GetDeviceUSBInfo(pclSerialObject->GetDeviceNumber(), aucDeviceDescription, aucDeviceSerial, USB_MAX_STRLEN))
            {
               printf("  Product Description: %s\n", aucDeviceDescription);
               printf("  Serial String: %s\n", aucDeviceSerial);
            }
            break;
         }
         default:
         {
            break;
         }
      }
      DSIThread_Sleep(0);
   }

   //Disconnecting from module
   printf("Disconnecting module...\n");
   this->Close();

   printf("Demo has completed successfully!\n");

   return;
}


////////////////////////////////////////////////////////////////////////////////
// InitANT
//
// Resets the system and starts the test
//
////////////////////////////////////////////////////////////////////////////////
BOOL Demo::InitANT(void)
{
   BOOL bStatus;

   // Reset system
   printf("Resetting module...\n");
   bStatus = pclMessageObject->ResetSystem();
   DSIThread_Sleep(1000);

   // Start the test by setting network key
   printf("Setting network key...\n");
   UCHAR ucNetKey[8] = USER_NETWORK_KEY;

   bStatus = pclMessageObject->SetNetworkKey(USER_NETWORK_NUM, ucNetKey, MESSAGE_TIMEOUT);

   return bStatus;
}

////////////////////////////////////////////////////////////////////////////////
// RunMessageThread
//
// Callback function that is used to create the thread. This is a static
// function.
//
////////////////////////////////////////////////////////////////////////////////
DSI_THREAD_RETURN Demo::RunMessageThread(void *pvParameter_)
{
   ((Demo*) pvParameter_)->MessageThread();
   return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// MessageThread
//
// Run message thread
////////////////////////////////////////////////////////////////////////////////
void Demo::MessageThread()
{
   ANT_MESSAGE stMessage;
   USHORT usSize;
   bDone = FALSE;

   while(!bDone)
   {
      if(pclMessageObject->WaitForMessage(1000))
      {
         usSize = pclMessageObject->GetMessage(&stMessage);

         if(bDone)
            break;

         if(usSize == DSI_FRAMER_ERROR)
         {
            // Get the message to clear the error
            usSize = pclMessageObject->GetMessage(&stMessage, MESG_MAX_SIZE_VALUE);
            continue;
         }

         if(usSize != DSI_FRAMER_ERROR && usSize != DSI_FRAMER_TIMEDOUT && usSize != 0)
         {
            ProcessMessage(stMessage, usSize);
         }
      }
   }

   DSIThread_MutexLock(&mutexTestDone);
   UCHAR ucCondResult = DSIThread_CondSignal(&condTestDone);
   assert(ucCondResult == DSI_THREAD_ENONE);
   DSIThread_MutexUnlock(&mutexTestDone);

}


////////////////////////////////////////////////////////////////////////////////
// ProcessMessage
//
// Process ALL messages that come from ANT, including event messages.
//
// stMessage: Message struct containing message recieved from ANT
// usSize_:
////////////////////////////////////////////////////////////////////////////////
void Demo::ProcessMessage(ANT_MESSAGE stMessage, USHORT usSize_)
{
   BOOL bStatus;
   BOOL bPrintBuffer = FALSE;
   UCHAR ucDataOffset = MESSAGE_BUFFER_DATA2_INDEX;   // For most data messages


   switch(stMessage.ucMessageID)
   {
      //RESPONSE MESG
      case MESG_RESPONSE_EVENT_ID:
      {
         //RESPONSE TYPE
         switch(stMessage.aucData[1])
         {
            case MESG_NETWORK_KEY_ID:
            {
               if(stMessage.aucData[2] != RESPONSE_NO_ERROR)
               {
                  printf("Error configuring network key: Code 0%d\n", stMessage.aucData[2]);
                  break;
               }
               printf("Network key set.\n");
               printf("Assigning channel...\n");
               if(ucChannelType == CHANNEL_TYPE_MASTER)
               {
                  bStatus = pclMessageObject->AssignChannel(USER_ANTCHANNEL, PARAMETER_TX_NOT_RX, 0, MESSAGE_TIMEOUT);
               }
               else if(ucChannelType == CHANNEL_TYPE_SLAVE)
               {
                  bStatus = pclMessageObject->AssignChannel(USER_ANTCHANNEL, 0, 0, MESSAGE_TIMEOUT);
               }
               break;
            }

            case MESG_ASSIGN_CHANNEL_ID:
            {
               if(stMessage.aucData[2] != RESPONSE_NO_ERROR)
               {
                  printf("Error assigning channel: Code 0%d\n", stMessage.aucData[2]);
                  break;
               }
               printf("Channel assigned\n");
               printf("Setting Channel ID...\n");
               bStatus = pclMessageObject->SetChannelID(USER_ANTCHANNEL, USER_DEVICENUM, USER_DEVICETYPE, USER_TRANSTYPE, MESSAGE_TIMEOUT);
               break;
            }

            case MESG_CHANNEL_ID_ID:
            {
               if(stMessage.aucData[2] != RESPONSE_NO_ERROR)
               {
                  printf("Error configuring Channel ID: Code 0%d\n", stMessage.aucData[2]);
                  break;
               }
               printf("Channel ID set\n");
               printf("Setting Radio Frequency...\n");
               bStatus = pclMessageObject->SetChannelRFFrequency(USER_ANTCHANNEL, USER_RADIOFREQ, MESSAGE_TIMEOUT);
               break;
            }

            case MESG_CHANNEL_RADIO_FREQ_ID:
            {
               if(stMessage.aucData[2] != RESPONSE_NO_ERROR)
               {
                  printf("Error configuring Radio Frequency: Code 0%d\n", stMessage.aucData[2]);
                  break;
               }
               printf("Radio Frequency set\n");
               printf("Opening channel...\n");
               bBroadcasting = TRUE;
               bStatus = pclMessageObject->OpenChannel(USER_ANTCHANNEL, MESSAGE_TIMEOUT);
               break;
            }

            case MESG_OPEN_CHANNEL_ID:
            {
               if(stMessage.aucData[2] != RESPONSE_NO_ERROR)
               {
                  printf("Error opening channel: Code 0%d\n", stMessage.aucData[2]);
                  bBroadcasting = FALSE;
                  break;
               }
               printf("Chanel opened\n");
#if defined (ENABLE_EXTENDED_MESSAGES)
               printf("Enabling extended messages...\n");
               pclMessageObject->RxExtMesgsEnable(TRUE);
#endif
               break;
            }

            case MESG_RX_EXT_MESGS_ENABLE_ID:
            {
               if(stMessage.aucData[2] == INVALID_MESSAGE)
               {
                  printf("Extended messages not supported in this ANT product\n");
                  break;
               }
               else if(stMessage.aucData[2] != RESPONSE_NO_ERROR)
               {
                  printf("Error enabling extended messages: Code 0%d\n", stMessage.aucData[2]);
                  break;
               }
               printf("Extended messages enabled\n");
               break;
            }

            case MESG_UNASSIGN_CHANNEL_ID:
            {
               if(stMessage.aucData[2] != RESPONSE_NO_ERROR)
               {
                  printf("Error unassigning channel: Code 0%d\n", stMessage.aucData[2]);
                  break;
               }
               printf("Channel unassigned\n");
               printf("Press enter to exit\n");
               bMyDone = TRUE;
               break;
            }

            case MESG_CLOSE_CHANNEL_ID:
            {
               if(stMessage.aucData[2] == CHANNEL_IN_WRONG_STATE)
               {
                  // We get here if we tried to close the channel after the search timeout (slave)
                  printf("Channel is already closed\n");
                  printf("Unassigning channel...\n");
                  bStatus = pclMessageObject->UnAssignChannel(USER_ANTCHANNEL, MESSAGE_TIMEOUT);
                  break;
               }
               else if(stMessage.aucData[2] != RESPONSE_NO_ERROR)
               {
                  printf("Error closing channel: Code 0%d\n", stMessage.aucData[2]);
                  break;
               }
               // If this message was successful, wait for EVENT_CHANNEL_CLOSED to confirm channel is closed
               break;
            }

            case MESG_REQUEST_ID:
            {
               if(stMessage.aucData[2] == INVALID_MESSAGE)
               {
                  printf("Requested message not supported in this ANT product\n");
               }
               break;
            }

            case MESG_EVENT_ID:
            {
               switch(stMessage.aucData[2])
                  {
                  case EVENT_CHANNEL_CLOSED:
                  {
                     printf("Channel Closed\n");
                     printf("Unassigning channel...\n");
                     bStatus = pclMessageObject->UnAssignChannel(USER_ANTCHANNEL, MESSAGE_TIMEOUT);
                     break;
                  }
                  case EVENT_TX:
                  {
                     // This event indicates that a message has just been
                     // sent over the air. We take advantage of this event to set
                     // up the data for the next message period.
                     static UCHAR ucIncrement = 0;      // Increment the first byte of the buffer

                     aucTransmitBuffer[0] = ucIncrement++;

                     // Broadcast data will be sent over the air on
                     // the next message period.
                     if(bBroadcasting)
                     {
                        pclMessageObject->SendBroadcastData(USER_ANTCHANNEL, aucTransmitBuffer);

                        // Echo what the data will be over the air on the next message period.
                        if(bDisplay)
                        {
                           printf("Tx:(%d): [%02x],[%02x],[%02x],[%02x],[%02x],[%02x],[%02x],[%02x]\n",
                              USER_ANTCHANNEL,
                              aucTransmitBuffer[MESSAGE_BUFFER_DATA1_INDEX],
                              aucTransmitBuffer[MESSAGE_BUFFER_DATA2_INDEX],
                              aucTransmitBuffer[MESSAGE_BUFFER_DATA3_INDEX],
                              aucTransmitBuffer[MESSAGE_BUFFER_DATA4_INDEX],
                              aucTransmitBuffer[MESSAGE_BUFFER_DATA5_INDEX],
                              aucTransmitBuffer[MESSAGE_BUFFER_DATA6_INDEX],
                              aucTransmitBuffer[MESSAGE_BUFFER_DATA7_INDEX],
                              aucTransmitBuffer[MESSAGE_BUFFER_DATA8_INDEX]);
                        }
                        else
                        {
                           static int iIndex = 0;
                           static char ac[] = {'|','/','-','\\'};
                           printf("Tx: %c\r",ac[iIndex++]); fflush(stdout);
                           iIndex &= 3;
                        }
                     }
                     break;

                  }
                  case EVENT_RX_SEARCH_TIMEOUT:
                  {
                     printf("Search Timeout\n");
                     break;
                  }
                  case EVENT_RX_FAIL:
                  {
                     printf("Rx Fail\n");
                     break;
                  }
                  case EVENT_TRANSFER_RX_FAILED:
                  {
                     printf("Burst receive has failed\n");
                     break;
                  }
                  case EVENT_TRANSFER_TX_COMPLETED:
                  {
                     printf("Tranfer Completed\n");
                     break;
                  }
                  case EVENT_TRANSFER_TX_FAILED:
                  {
                     printf("Tranfer Failed\n");
                     break;
                  }
                  case EVENT_RX_FAIL_GO_TO_SEARCH:
                  {
                     printf("Go to Search\n");
                     break;
                  }
                  case EVENT_CHANNEL_COLLISION:
                  {
                     printf("Channel Collision\n");
                     break;
                  }
                  case EVENT_TRANSFER_TX_START:
                  {
                     printf("Burst Started\n");
                     break;
                  }
                  default:
                  {
                     printf("Unhandled channel event: 0x%X\n", stMessage.aucData[2]);
                     break;
                  }

               }

               break;
            }

            default:
            {
               printf("Unhandled response 0%d to message 0x%X\n", stMessage.aucData[2], stMessage.aucData[1]);
               break;
            }
         }
         break;
      }

      case MESG_STARTUP_MESG_ID:
      {
         printf("RESET Complete, reason: ");

         UCHAR ucReason = stMessage.aucData[MESSAGE_BUFFER_DATA1_INDEX];

         if(ucReason == RESET_POR)
            printf("RESET_POR");
         if(ucReason & RESET_SUSPEND)
            printf("RESET_SUSPEND ");
         if(ucReason & RESET_SYNC)
            printf("RESET_SYNC ");
         if(ucReason & RESET_CMD)
            printf("RESET_CMD ");
         if(ucReason & RESET_WDT)
            printf("RESET_WDT ");
         if(ucReason & RESET_RST)
            printf("RESET_RST ");
         printf("\n");

         break;
      }

      case MESG_CAPABILITIES_ID:
      {
         printf("CAPABILITIES:\n");
         printf("   Max ANT Channels: %d\n",stMessage.aucData[MESSAGE_BUFFER_DATA1_INDEX]);
         printf("   Max ANT Networks: %d\n",stMessage.aucData[MESSAGE_BUFFER_DATA2_INDEX]);

         UCHAR ucStandardOptions = stMessage.aucData[MESSAGE_BUFFER_DATA3_INDEX];
         UCHAR ucAdvanced = stMessage.aucData[MESSAGE_BUFFER_DATA4_INDEX];
         UCHAR ucAdvanced2 = stMessage.aucData[MESSAGE_BUFFER_DATA5_INDEX];

         printf("Standard Options:\n");
         if( ucStandardOptions & CAPABILITIES_NO_RX_CHANNELS )
            printf("CAPABILITIES_NO_RX_CHANNELS\n");
         if( ucStandardOptions & CAPABILITIES_NO_TX_CHANNELS )
            printf("CAPABILITIES_NO_TX_CHANNELS\n");
         if( ucStandardOptions & CAPABILITIES_NO_RX_MESSAGES )
            printf("CAPABILITIES_NO_RX_MESSAGES\n");
         if( ucStandardOptions & CAPABILITIES_NO_TX_MESSAGES )
            printf("CAPABILITIES_NO_TX_MESSAGES\n");
         if( ucStandardOptions & CAPABILITIES_NO_ACKD_MESSAGES )
            printf("CAPABILITIES_NO_ACKD_MESSAGES\n");
         if( ucStandardOptions & CAPABILITIES_NO_BURST_TRANSFER )
            printf("CAPABILITIES_NO_BURST_TRANSFER\n");

         printf("Advanced Options:\n");
         if( ucAdvanced & CAPABILITIES_OVERUN_UNDERRUN )
            printf("CAPABILITIES_OVERUN_UNDERRUN\n");
         if( ucAdvanced & CAPABILITIES_NETWORK_ENABLED )
            printf("CAPABILITIES_NETWORK_ENABLED\n");
         if( ucAdvanced & CAPABILITIES_AP1_VERSION_2 )
            printf("CAPABILITIES_AP1_VERSION_2\n");
         if( ucAdvanced & CAPABILITIES_SERIAL_NUMBER_ENABLED )
            printf("CAPABILITIES_SERIAL_NUMBER_ENABLED\n");
         if( ucAdvanced & CAPABILITIES_PER_CHANNEL_TX_POWER_ENABLED )
            printf("CAPABILITIES_PER_CHANNEL_TX_POWER_ENABLED\n");
         if( ucAdvanced & CAPABILITIES_LOW_PRIORITY_SEARCH_ENABLED )
            printf("CAPABILITIES_LOW_PRIORITY_SEARCH_ENABLED\n");
         if( ucAdvanced & CAPABILITIES_SCRIPT_ENABLED )
            printf("CAPABILITIES_SCRIPT_ENABLED\n");
         if( ucAdvanced & CAPABILITIES_SEARCH_LIST_ENABLED )
            printf("CAPABILITIES_SEARCH_LIST_ENABLED\n");

         if(usSize_ > 4)
         {
            printf("Advanced 2 Options 1:\n");
            if( ucAdvanced2 & CAPABILITIES_LED_ENABLED )
               printf("CAPABILITIES_LED_ENABLED\n");
            if( ucAdvanced2 & CAPABILITIES_EXT_MESSAGE_ENABLED )
               printf("CAPABILITIES_EXT_MESSAGE_ENABLED\n");
            if( ucAdvanced2 & CAPABILITIES_SCAN_MODE_ENABLED )
               printf("CAPABILITIES_SCAN_MODE_ENABLED\n");
            if( ucAdvanced2 & CAPABILITIES_RESERVED )
               printf("CAPABILITIES_RESERVED\n");
            if( ucAdvanced2 & CAPABILITIES_PROX_SEARCH_ENABLED )
               printf("CAPABILITIES_PROX_SEARCH_ENABLED\n");
            if( ucAdvanced2 & CAPABILITIES_EXT_ASSIGN_ENABLED )
               printf("CAPABILITIES_EXT_ASSIGN_ENABLED\n");
            if( ucAdvanced2 & CAPABILITIES_FS_ANTFS_ENABLED)
               printf("CAPABILITIES_FREE_1\n");
            if( ucAdvanced2 & CAPABILITIES_FIT1_ENABLED )
               printf("CAPABILITIES_FIT1_ENABLED\n");
         }
         break;
      }
      case MESG_CHANNEL_STATUS_ID:
      {
         printf("Got Status\n");

         char astrStatus[][32] = {  "STATUS_UNASSIGNED_CHANNEL",
                                    "STATUS_ASSIGNED_CHANNEL",
                                    "STATUS_SEARCHING_CHANNEL",
                                    "STATUS_TRACKING_CHANNEL"   };

         UCHAR ucStatusByte = stMessage.aucData[MESSAGE_BUFFER_DATA2_INDEX] & STATUS_CHANNEL_STATE_MASK; // MUST MASK OFF THE RESERVED BITS
         printf("STATUS: %s\n",astrStatus[ucStatusByte]);
         break;
      }
      case MESG_CHANNEL_ID_ID:
      {
         // Channel ID of the device that we just recieved a message from.
         USHORT usDeviceNumber = stMessage.aucData[MESSAGE_BUFFER_DATA2_INDEX] | (stMessage.aucData[MESSAGE_BUFFER_DATA3_INDEX] << 8);
         UCHAR ucDeviceType =  stMessage.aucData[MESSAGE_BUFFER_DATA4_INDEX];
         UCHAR ucTransmissionType = stMessage.aucData[MESSAGE_BUFFER_DATA5_INDEX];

         printf("CHANNEL ID: (%d/%d/%d)\n", usDeviceNumber, ucDeviceType, ucTransmissionType);
         break;
      }
      case MESG_VERSION_ID:
      {
         printf("VERSION: %s\n", (char*) &stMessage.aucData[MESSAGE_BUFFER_DATA1_INDEX]);
         break;
      }
      case MESG_ACKNOWLEDGED_DATA_ID:
      case MESG_BURST_DATA_ID:
      case MESG_BROADCAST_DATA_ID:
      {
         // The flagged and unflagged data messages have the same
         // message ID. Therefore, we need to check the size to
         // verify of a flag is present at the end of a message.
         // To enable flagged messages, must call ANT_RxExtMesgsEnable first.
         if(usSize_ > MESG_DATA_SIZE)
         {
            UCHAR ucFlag = stMessage.aucData[MESSAGE_BUFFER_DATA10_INDEX];

            if(bDisplay && ucFlag & ANT_EXT_MESG_BITFIELD_DEVICE_ID)
            {
               // Channel ID of the device that we just recieved a message from.
               USHORT usDeviceNumber = stMessage.aucData[MESSAGE_BUFFER_DATA11_INDEX] | (stMessage.aucData[MESSAGE_BUFFER_DATA12_INDEX] << 8);
               UCHAR ucDeviceType =  stMessage.aucData[MESSAGE_BUFFER_DATA13_INDEX];
               UCHAR ucTransmissionType = stMessage.aucData[MESSAGE_BUFFER_DATA14_INDEX];

               printf("Chan ID(%d/%d/%d) - ", usDeviceNumber, ucDeviceType, ucTransmissionType);
            }
         }

         // Display recieved message
         bPrintBuffer = TRUE;
         ucDataOffset = MESSAGE_BUFFER_DATA2_INDEX;   // For most data messages

         if(bDisplay)
         {
            if(stMessage.ucMessageID == MESG_ACKNOWLEDGED_DATA_ID )
               printf("Acked Rx:(%d): ", stMessage.aucData[MESSAGE_BUFFER_DATA1_INDEX]);
            else if(stMessage.ucMessageID == MESG_BURST_DATA_ID)
               printf("Burst(0x%02x) Rx:(%d): ", ((stMessage.aucData[MESSAGE_BUFFER_DATA1_INDEX] & 0xE0) >> 5), stMessage.aucData[MESSAGE_BUFFER_DATA1_INDEX] & 0x1F );
            else
               printf("Rx:(%d): ", stMessage.aucData[MESSAGE_BUFFER_DATA1_INDEX]);
         }
         break;
      }
      case MESG_EXT_BROADCAST_DATA_ID:
      case MESG_EXT_ACKNOWLEDGED_DATA_ID:
      case MESG_EXT_BURST_DATA_ID:
      {

         // The "extended" part of this message is the 4-byte channel
         // id of the device that we recieved this message from. This message
         // is only available on the AT3. The AP2 uses flagged versions of the
         // data messages as shown above.

         // Channel ID of the device that we just recieved a message from.
         USHORT usDeviceNumber = stMessage.aucData[MESSAGE_BUFFER_DATA2_INDEX] | (stMessage.aucData[MESSAGE_BUFFER_DATA3_INDEX] << 8);
         UCHAR ucDeviceType =  stMessage.aucData[MESSAGE_BUFFER_DATA4_INDEX];
         UCHAR ucTransmissionType = stMessage.aucData[MESSAGE_BUFFER_DATA5_INDEX];

         bPrintBuffer = TRUE;
         ucDataOffset = MESSAGE_BUFFER_DATA6_INDEX;   // For most data messages

         if(bDisplay)
         {
            // Display the channel id
            printf("Chan ID(%d/%d/%d) ", usDeviceNumber, ucDeviceType, ucTransmissionType );

            if(stMessage.ucMessageID == MESG_EXT_ACKNOWLEDGED_DATA_ID)
               printf("- Acked Rx:(%d): ", stMessage.aucData[MESSAGE_BUFFER_DATA1_INDEX]);
            else if(stMessage.ucMessageID == MESG_EXT_BURST_DATA_ID)
               printf("- Burst(0x%02x) Rx:(%d): ", ((stMessage.aucData[MESSAGE_BUFFER_DATA1_INDEX] & 0xE0) >> 5), stMessage.aucData[MESSAGE_BUFFER_DATA1_INDEX] & 0x1F );
            else
               printf("- Rx:(%d): ", stMessage.aucData[MESSAGE_BUFFER_DATA1_INDEX]);
         }

         break;
      }

      default:
      {
         break;
      }
   }

   // If we recieved a data message, diplay its contents here.
   if(bPrintBuffer)
   {
      if(bDisplay)
      {
         printf("[%02x],[%02x],[%02x],[%02x],[%02x],[%02x],[%02x],[%02x]\n",
            stMessage.aucData[ucDataOffset + 0],
            stMessage.aucData[ucDataOffset + 1],
            stMessage.aucData[ucDataOffset + 2],
            stMessage.aucData[ucDataOffset + 3],
            stMessage.aucData[ucDataOffset + 4],
            stMessage.aucData[ucDataOffset + 5],
            stMessage.aucData[ucDataOffset + 6],
            stMessage.aucData[ucDataOffset + 7]);
      }
      else
      {
         static int iIndex = 0;
         static char ac[] = {'|','/','-','\\'};
         printf("Rx: %c\r",ac[iIndex++]); fflush(stdout);
         iIndex &= 3;

      }
   }

   return;
}



////////////////////////////////////////////////////////////////////////////////
// PrintMenu
//
// Start the Test program.
//
////////////////////////////////////////////////////////////////////////////////
void Demo::PrintMenu()
{
   // Printout options
   printf("\n");
   printf("M - Print this menu\n");
   printf("A - Send Acknowledged message\n");
   printf("B - Send Burst message\n");
   printf("R - Reset\n");
   printf("C - Request Capabilites\n");
   printf("V - Request Version\n");
   printf("I - Request Channel ID\n");
   printf("S - Request Status\n");
   printf("U - Request USB Descriptor\n");
   printf("D - Toggle Display\n");
   printf("Q - Quit\n");
   printf("\n");
   fflush(stdout);
}
