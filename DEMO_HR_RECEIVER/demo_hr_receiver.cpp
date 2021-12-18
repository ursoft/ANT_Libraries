/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/
#include "demo_hr_receiver.h"

////////////////////////////////////////////////////////////////////////////////
// The Demo ANT+ HRM Receiver PC app.
//
// For preventing dependency issues, please launch the ANT_Libraries.sln file in
// Visual Studio 2008 instead of accessing the project on its own.
//
// A simple command line HRM Receiver app built on top of the ANT library that
// demonstrates how to comply with the required ANT+ specifications.
// Please note that other additions can be made while designing an HRM Receiver;
// this is just for demonstration purposes. For details regarding the ANT+ HRM
// specs, please visit www.thisisant.com/developer
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
   printf("********************************************************\n");
   printf("Demo ANT+ Heart Rate Monitor (HRM) Receiver application\n");
   printf("********************************************************\n");

   HRMReceiver* Receiver = new HRMReceiver();

   // Initialising with invalid device.
   // User will be prompted later if it wasn't passed in.
   // ucDeviceNumber = USB device to which the ANT device is connected
   UCHAR ucDeviceNumber = 0xFF;

   if(argc > 1)
   {
      ucDeviceNumber = (UCHAR) atoi(argv[1]);
   }

   if(Receiver->Init(ucDeviceNumber))
      Receiver->Start();
   else
      delete Receiver;
   return 0;
}

////////////////////////////////////////////////////////////////////////////////
// HRMReceiver (Heart Rate Receiver)
//
// Constructor, intializes HRMReceiver class
//
////////////////////////////////////////////////////////////////////////////////
HRMReceiver::HRMReceiver()
{
   ucChannelType = CHANNEL_TYPE_SLAVE; // HRM Receiver is a slave
   pclSerialObject = (DSISerialGeneric*)NULL;
   pclMessageObject = (DSIFramerANT*)NULL;
   uiDSIThread = (DSI_THREAD_ID)NULL;
   bMyDone = FALSE;
   bDone = FALSE;
   bDisplay = TRUE;
   bProcessedData = TRUE;
   bBroadcasting = FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// ~HRMReceiver
//
// Destructor, clean up and loose memory
//
////////////////////////////////////////////////////////////////////////////////
HRMReceiver::~HRMReceiver()
{
   if(pclMessageObject)
      delete pclMessageObject;

   if(pclSerialObject)
      delete pclSerialObject;
}

////////////////////////////////////////////////////////////////////////////////
// Init
//
// Initize the HRMReceiver and ANT Library.
//
// ucDeviceNumber_: USB Device Number (0 for first USB stick plugged and so on)
//                  If not specified on command line, 0xFF is passed in as invalid.
////////////////////////////////////////////////////////////////////////////////
BOOL HRMReceiver::Init(UCHAR ucDeviceNumber_)
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
   printf("USB Number: %d\n", ucDeviceNumber_);

   // Prompting user for network parameters
   // Channel Number
   printf("ANT Channel number? (Press Enter for default: 0)\n"); fflush(stdout);
   char st[1024];
   fgets(st, sizeof(st), stdin);
   ucAntChannel = (UCHAR)atoi(st);
   printf("Ant Channel number: %d\n", ucAntChannel);

   // Transmission type
   printf("ANT Transmission type? (Press Enter for wildcarding)\n"); fflush(stdout);
   char st1[1024];
   fgets(st1, sizeof(st1), stdin);
   ucTransType = (UCHAR)atoi(st1);
   if (ucTransType == (UCHAR)0)
   {
      printf("ANT Transmission type wildcarded\n");
   }
   else
   {
      printf("ANT Transmission type: %d\n", ucTransType);
   }

   // Device Number
   printf("Transmitter Device number? (Press Enter for wildcarding)\n"); fflush(stdout);
   char st2[1024];
   fgets(st2, sizeof(st2), stdin);
   usDeviceNum = (USHORT)atoi(st2);
   if (usDeviceNum == (USHORT)0)
   {
      printf("Transmitter Device Number wildcarded\n");
   }
   else
   {
      printf("Transmitter Device Number: %d\n", usDeviceNum);
   }

   // Network Number
   printf("Network number? (Press Enter for default: 0)\n"); fflush(stdout);
   char st3[1024];
   fgets(st3, sizeof(st3), stdin);
   ucNetworkNum = (UCHAR)atoi(st3);
   printf("Network Number: %d\n", ucNetworkNum);

   // Message Period
   int period_option;
   char st4[10];
   USHORT usMessagePeriods[] = USER_MESSAGE_PERIODS;
   int len_usMessagePeriods = sizeof(usMessagePeriods)/sizeof(USHORT);
   while(1)
   {
      printf("Message Period (in counts)? ");
      for (int i = 0; i < len_usMessagePeriods; i++)
      {
         printf("%d: %d", i, usMessagePeriods[i]);
         if (i != len_usMessagePeriods - 1)
         {
            printf(" , ");
         }
      }
      printf("\n");
      fflush(stdout);
      fgets(st4, sizeof(st4), stdin);
      period_option = atoi(st4);
      if (period_option >= len_usMessagePeriods)
      {
         printf("Invalid Period. Try again\n");
      }
      else
      {
         printf("Message Period: %d\n", usMessagePeriods[period_option]);
         break;
      }
   }
   usMessagePeriod = usMessagePeriods[period_option];


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
      printf("Press any key to continue.\n"); fflush(stdout);
      char st[1024];
      fgets(st, sizeof(st), stdin);
      return FALSE;
   }

   // Create message thread.
   uiDSIThread = DSIThread_CreateThread(&HRMReceiver::RunMessageThread, this);
   assert(uiDSIThread);

   printf("USB device initialisation was successful!\n"); fflush(stdout);

   return TRUE;
}


////////////////////////////////////////////////////////////////////////////////
// Close
//
// Close connection to USB stick.
//
////////////////////////////////////////////////////////////////////////////////
void HRMReceiver::Close()
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
void HRMReceiver::Start()
{
   BOOL bStatus;

   // Print out the menu to start
   PrintMenu();

   // Start ANT channel setup
   bStatus = InitANT();
   if (bStatus)
   {
      printf("ANT initialisation was successful!\n"); fflush(stdout);

      while(!bMyDone)
      {
         //printf("Press Q to exit.\n");
         UCHAR ucChar;
         char st[1024];
         fgets(st, sizeof(st), stdin);
         sscanf(st, "%c", &ucChar);

         switch(ucChar)
         {

            case 'M':
            case 'm':
            {
               PrintMenu();
               break;
            }
            case 'Q':
            case 'q':
            {
               // Quit
               printf("Closing channel...\n");
               bBroadcasting = FALSE;
               pclMessageObject->CloseChannel(ucAntChannel, MESSAGE_TIMEOUT);
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
               pclMessageObject->SendRequest(MESG_CAPABILITIES_ID, ucAntChannel, &stResponse, 0);
               break;
            }
            case 'v':
            case 'V':
            {
               // Request version
               ANT_MESSAGE_ITEM stResponse;
               pclMessageObject->SendRequest(MESG_VERSION_ID, ucAntChannel, &stResponse, 0);
               break;
            }
            case 'S':
            case 's':
            {
               // Request channel status
               ANT_MESSAGE_ITEM stResponse;
               pclMessageObject->SendRequest(MESG_CHANNEL_STATUS_ID, ucAntChannel, &stResponse, 0);
               break;
            }
            case 'I':
            case 'i':
            {
               // Request channel ID
               ANT_MESSAGE_ITEM stResponse;
               pclMessageObject->SendRequest(MESG_CHANNEL_ID_ID, ucAntChannel, &stResponse, 0);
               break;
            }
            case 'd':
            case 'D':
            {
               // Toggle display of data messages
               bDisplay = !bDisplay;
               break;
            }
            case 'p':
            case 'P':
            {
               // Toggle raw-processed HRM data
               bProcessedData = !bProcessedData;
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
   }
   else
   {
      printf("ANT initialisation failed!\n"); fflush(stdout);
   }
   //Disconnecting from module
   printf("Disconnecting module...\n");
   this->Close();

   printf("Closing the Heart Rate Monitor Receiver!\n");

   return;
}

////////////////////////////////////////////////////////////////////////////////
// InitANT
//
// Resets the system and starts the test
//
////////////////////////////////////////////////////////////////////////////////
BOOL HRMReceiver::InitANT(void)
{
   BOOL bStatus; // ANT initialisation status

   // Reset system
   printf("Resetting module...\n");
   bStatus = pclMessageObject->ResetSystem();
   DSIThread_Sleep(1000);

   // Start the test by setting network key
   printf("Setting network key...\n");
   UCHAR ucNetKey[8] = USER_NETWORK_KEY;

   bStatus &= pclMessageObject->SetNetworkKey(ucNetworkNum, ucNetKey, MESSAGE_TIMEOUT);

   return bStatus;
}

////////////////////////////////////////////////////////////////////////////////
// RunMessageThread
//
// Callback function that is used to create the thread. This is a static
// function.
//
////////////////////////////////////////////////////////////////////////////////
DSI_THREAD_RETURN HRMReceiver::RunMessageThread(void *pvParameter_)
{
   ((HRMReceiver*) pvParameter_)->MessageThread();
   return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// MessageThread
//
// Run message thread
////////////////////////////////////////////////////////////////////////////////
void HRMReceiver::MessageThread()
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
void HRMReceiver::ProcessMessage(ANT_MESSAGE stMessage, USHORT usSize_)
{
   BOOL bStatus;
   BOOL bPrintBuffer = FALSE;
   UCHAR ucDataOffset = MESSAGE_BUFFER_DATA2_INDEX;   // For most data messages

   // For decoding device type -- legacy or current
   static UCHAR ucDeviceType = INVALID_DEVICE;

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
               bStatus = pclMessageObject->AssignChannel(ucAntChannel, PARAMETER_RX_NOT_TX, 0, MESSAGE_TIMEOUT);
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
               bStatus = pclMessageObject->SetChannelID(ucAntChannel, usDeviceNum, USER_DEVICETYPE, ucTransType, MESSAGE_TIMEOUT);
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
               bStatus = pclMessageObject->SetChannelRFFrequency(ucAntChannel, USER_RADIOFREQ, MESSAGE_TIMEOUT);
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
               printf("Setting Message Period...\n");
               bStatus = pclMessageObject->SetChannelPeriod(ucAntChannel, usMessagePeriod, MESSAGE_TIMEOUT);
               break;
            }

            case MESG_CHANNEL_MESG_PERIOD_ID:
            {
               if(stMessage.aucData[2] != RESPONSE_NO_ERROR)
               {
                  printf("Error assigning Message Period: Code 0%d\n", stMessage.aucData[2]);
                  break;
               }
               printf("Message period assigned\n");
               printf("Opening channel...\n");
               bBroadcasting = TRUE;
               bStatus = pclMessageObject->OpenChannel(ucAntChannel, MESSAGE_TIMEOUT);
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
                  bStatus = pclMessageObject->UnAssignChannel(ucAntChannel, MESSAGE_TIMEOUT);
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
                     bStatus = pclMessageObject->UnAssignChannel(ucAntChannel, MESSAGE_TIMEOUT);
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
      case MESG_BROADCAST_DATA_ID:
      // Burst not supported by ANT+ HRM
      {
         if (ucDeviceType == INVALID_DEVICE)
         {
            // Detecting current vs. legacy transmitter, only if not detected yet
            static BOOL bOldToggleBit = INVALID_TOGGLE_BIT;
            static UCHAR ucToggleAttempts = 0; // The number of attempts made so far
            BOOL bToggleBit = stMessage.aucData[ucDataOffset + 0] >> 7;
            DetectDevice(ucDeviceType, bOldToggleBit, ucToggleAttempts, bToggleBit);
         }

         if ((bProcessedData) && (ucDeviceType == INVALID_DEVICE))
         {
            // The user wants to see processed data, but device type is not detected yet
            break;
         }

         // The flagged and unflagged data messages have the same
         // message ID. Therefore, we need to check the size to
         // verify if a flag is present at the end of a message.
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

            else if (stMessage.ucMessageID == MESG_BROADCAST_DATA_ID)
               printf("Rx:(%d): ", stMessage.aucData[MESSAGE_BUFFER_DATA1_INDEX]);

            // Burst is not supported by ANT+ HRM
         }
         break;

      }

      case MESG_EXT_BROADCAST_DATA_ID:
      case MESG_EXT_ACKNOWLEDGED_DATA_ID:
      {
         if (ucDeviceType == INVALID_DEVICE)
         {
            // Detecting current vs. legacy transmitter, only if not detected yet
            static BOOL bOldToggleBit = INVALID_TOGGLE_BIT;
            static UCHAR ucToggleAttempts = 0; // The number of attempts made so far
            BOOL bToggleBit = stMessage.aucData[ucDataOffset + 0] >> 7;
            DetectDevice(ucDeviceType, bOldToggleBit, ucToggleAttempts, bToggleBit);
         }

         if ((bProcessedData) && (ucDeviceType == INVALID_DEVICE))
         {
            // The user wants to see processed data, but device type is not detected yet
            break;
         }

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

            else if(stMessage.ucMessageID == MESG_EXT_BROADCAST_DATA_ID)
               printf("- Rx:(%d): ", stMessage.aucData[MESSAGE_BUFFER_DATA1_INDEX]);

            // Burst not supported by ANT+ HRM
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
         // If the user wants to see the decoded information
         if (bProcessedData)
         {
            // HR data, common to all data pages and device types

            // Merge the 2 bytes to form the HRM event time
            USHORT usEventTime = ((USHORT)stMessage.aucData[ucDataOffset + 5] << 8) +
                                  (USHORT)stMessage.aucData[ucDataOffset + 4];
            UCHAR ucHR = stMessage.aucData[ucDataOffset + 7];
            UCHAR ucBeatCount = stMessage.aucData[ucDataOffset + 6];

            printf("HR: %d , Beat Count: %d , Beat Event Time: %d\n", ucHR, ucBeatCount, usEventTime);


            if (ucDeviceType == CURRENT_DEVICE)
            {
               // Current device => Page numbers are enabled
               UCHAR ucPageNum = stMessage.aucData[ucDataOffset + 0] & ~TOGGLE_MASK;

               // Page specific data
               switch(ucPageNum) // Removing the toggle bit
               {

                  case PAGE_0:
                  {
                     // Contains only common data
                     break;
                  }

                  case PAGE_1:
                  {
                     // Decoding cumulative operating time
                     ULONG ulOperatingTime = ((ULONG)stMessage.aucData[ucDataOffset + 3] << 16) +
                                             ((ULONG)stMessage.aucData[ucDataOffset + 2] << 8) +
                                             ((ULONG)stMessage.aucData[ucDataOffset + 1]);
                     ulOperatingTime = 2*ulOperatingTime;
                     printf("Transmitter operating Time: %d seconds\n", ulOperatingTime);
                     break;
                  }

                  case PAGE_2:
                  {
                     // Decoding Manufacturer ID & serial number
                     UCHAR ucManufacturerID = stMessage.aucData[ucDataOffset + 1];
                     USHORT usSerialNum = ((USHORT)stMessage.aucData[ucDataOffset + 3] << 8) +
                                           (USHORT)stMessage.aucData[ucDataOffset + 2];
                     printf("Transmitter manufacturer ID : %d , ", ucManufacturerID);
                     printf("Ser. #: %d\n", usSerialNum);
                     break;
                  }

                  case PAGE_3:
                  {
                     // Decoding Hardware, software version and model number
                     UCHAR ucHWVer = stMessage.aucData[ucDataOffset + 1];
                     UCHAR ucSWVer = stMessage.aucData[ucDataOffset + 2];
                     UCHAR ucModelNum = stMessage.aucData[ucDataOffset + 3];
                     printf("Transmitter HW ver: %d , ", ucHWVer);
                     printf("SF ver: %d , ", ucSWVer);
                     printf("Model #: %d\n", ucModelNum);
                     break;
                  }

                  case PAGE_4:
                  {
                     // Decoding R-R Interval Measurements
                     static UCHAR ucPreviousBeatCount;
                     USHORT usPreviousEventTime = ((USHORT)stMessage.aucData[ucDataOffset + 3] << 8) +
                                                   (USHORT)stMessage.aucData[ucDataOffset + 2];
                     if (ucBeatCount - ucPreviousBeatCount == 1)// Ensure that there is only one beat between time intervals
                     {
                        USHORT usR_R_Interval = usEventTime - usPreviousEventTime; // Subracting the event times gives the R-R interval
                        // Converting the timebase from 1/1024 of a second to milliseconds
                        USHORT usR_R_Interval_ms = (((ULONG) usR_R_Interval) * (ULONG) 1000) / (ULONG) 1024;
                        printf("R-R Interval: %d ms\n", usR_R_Interval_ms);
                     }
                     ucPreviousBeatCount = ucBeatCount;
                     break;
                  }
                  default:
                  {
                     // This should never be the case for current ANT+ HRM transmitter
                     // More pages may be implemented in future
                     printf("Unable to recognise the page number. Please check for an updated version of this app.\n");
                  }

               }

            }

         }

         // If the user wants to see the raw bytes received
         else
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
void HRMReceiver::PrintMenu()
{

   // Printout options
   printf("\n");
   printf("M - Print this menu\n");
   printf("R - Reset\n");
   printf("C - Request Capabilites\n");
   printf("V - Request Version\n");
   printf("I - Request Channel ID\n");
   printf("S - Request Status\n");
   printf("U - Request USB Descriptor\n");
   printf("D - Toggle Display\n");
   printf("P - Toggle Processed-Raw Data\n");
   printf("Q - Quit\n");
   printf("\n");
   fflush(stdout);
}

////////////////////////////////////////////////////////////////////////////////
// DetectDevice
//
// Helper member for detecting the device (type) -- legacy or current.
//
////////////////////////////////////////////////////////////////////////////////
void HRMReceiver::DetectDevice(UCHAR &ucDeviceType_, BOOL &bOldToggleBit_, UCHAR &ucToggleAttempts_, BOOL bToggleBit)
{
   if (ucToggleAttempts_ == 0)
   {
      printf("Detecting transmitter device type...\n");
   }

   if (ucToggleAttempts_ < MAX_TOGGLE_ATTEMPTS)
   {
      // Try to see if the ms bit of the ms byte toggles this time
      // Check MAX_TOGGLE_ATTEMPTS number of times
      if ((bToggleBit != bOldToggleBit_) && (bOldToggleBit_ != INVALID_TOGGLE_BIT))
      {
         ucDeviceType_ = CURRENT_DEVICE; // Toggle Bit toggled => Current device
         printf("Current Device detected\n");
      }
      else
      {
         bOldToggleBit_ = bToggleBit;
         ucToggleAttempts_++;
      }
   }

   if ((ucToggleAttempts_ >= MAX_TOGGLE_ATTEMPTS) && (ucDeviceType_ == INVALID_DEVICE))
   {
      // Toggle Bit didn't toggle for max # of attempts => Legacy device
      ucDeviceType_ = LEGACY_DEVICE;
      printf("Legacy Device detected\n");
   }
}