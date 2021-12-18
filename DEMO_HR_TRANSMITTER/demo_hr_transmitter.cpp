/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/
#include <windows.h>

#include "demo_hr_transmitter.h"

#include "types.h"
#include "dsi_framer_ant.hpp"
#include "dsi_thread.h"
#include "dsi_serial_generic.hpp"
#include "dsi_debug.hpp"

#include <stdio.h>
#include <assert.h>
#include <string.h>

//#define ENABLE_EXTENDED_MESSAGES

#define USER_BAUDRATE         (57600)  // For AT3/AP2, use 57600
#define USER_RADIOFREQ        (57)     // HRM Profile specified

#define USER_ANTCHANNEL       (0)
#define USER_DEVICENUM        (49)
#define USER_DEVICETYPE       0x78     // HRM Profile specified
#define USER_TRANSTYPE        (1)      // HRM Profile specified
#define USER_CHANNELTYPE      (0)      // 0 for master

#define USER_NETWORK_KEY      {0xB9, 0xA5, 0x21, 0xFB, 0xBD, 0x72, 0xC3, 0x45} //ANT+ Key
#define USER_NETWORK_NUM      (0)      // The network key is assigned to this network number

#define USER_PERIOD          (8070)   // HRM Profile specified

#define MESSAGE_TIMEOUT       (1000)

//Simulated device information - would be set by manufacturer for specific devices
#define HRM_MANUFACTURER_ID            (0x01)
#define HRM_UPPER_SERIAL_NUMBER        (0x3412)
#define HRM_HARDWARE_VERSION        (0x02)
#define HRM_SOFTWARE_VERSION        (0x42)
#define HRM_MODEL_NUMBER            (0x02)

#define HRM_DATA_PAGE_0             (0x00)
#define HRM_DATA_PAGE_1             (0x01)
#define HRM_DATA_PAGE_2             (0x02)
#define HRM_DATA_PAGE_3             (0x03)

#define HRM_RESERVED             (0xFF)

#define MASK_TOGGLE_BIT             (0x80)
#define MASK_HEARTBEAT_EVENT_TIME_LSB  (0x00FF)
#define MASK_SERIAL_NUMBER_LSB         (0xFF)

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
// c:\heartrateTxDemo.exe [device_no]
//
// ... where
//
// device_no:     USB Device port, starting at 0
//
// ... example
//
// c:\heartrateTxDemo.exe 0
//
// Comment to USB port 0
//
// If optional arguements are not supplied, user will
// be prompted to enter these after the program starts.
//
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
   Demo* pclDemo = new Demo();
   UCHAR ucDeviceNumberUSB = 0xFF;

   if(argc > 1)
   {
      ucDeviceNumberUSB = (UCHAR) atoi(argv[0]);
   }

   if(pclDemo->Init(ucDeviceNumberUSB))
   {
      pclDemo->Start();
   }
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
   ucChannelType = USER_CHANNELTYPE;
   pclSerialObject = (DSISerialGeneric*)NULL;
   pclMessageObject = (DSIFramerANT*)NULL;
   uiDSIThread = (DSI_THREAD_ID)NULL;
   bMyDone = FALSE;
   bPrintAllMessages = TRUE;
   bDone = FALSE;
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
// Init the Demo and ANT Library.
//
// ucDeviceNumberUSB_: USB Device Number (0 for first USB stick plugged and so on)
//                  If not specified on command line, 0xFF is passed in as invalid.
//
////////////////////////////////////////////////////////////////////////////////
BOOL Demo::Init(UCHAR ucDeviceNumberUSB_)
{

    BOOL bStatus;


   //Initialize HRM variables
    ucHeartBeatCount = 0;
   usHeartBeatEventTime = 0;

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
   assert(pclSerialObject);  // NOTE: Will fail if the module is not available.

   //Print greeting
   printf("*****************************************\n* Heart Rate Monitior Transmitter Demo  *\n");
   printf("*                                       *\n");
   printf("* Press Ctrl-c to exit                  *\n");
   printf("*****************************************\n");



   // If no device number was specified on the command line,
   // prompt the user for input.
   if(ucDeviceNumberUSB_ == 0xFF)
   {
      printf("Enter a USB Device number: "); fflush(stdout);
      char st[1024];
      fgets(st, sizeof(st), stdin);
      sscanf(st, "%u", &ucDeviceNumberUSB_);
   }

   // Initialize Serial object
   // The device number depends on how many USB sticks have been
   // plugged into the PC. The first USB stick plugged will be 0
   // the next 1 and so on.
   //
   // The Baud Rate depends on the ANT solution being used. AP1
   // is 50000, all others are 57600
   bStatus = pclSerialObject->Init(USER_BAUDRATE, ucDeviceNumberUSB_);
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
      printf("Failed to connect to device at USB port %d\n", ucDeviceNumberUSB_);
      return FALSE;
   }

   // Create message thread.
   uiDSIThread = DSIThread_CreateThread(&Demo::RunMessageThread, this);
   assert(uiDSIThread);

   printf("Initialization was successful!\n"); fflush(stdout);

   //Get the device configuration information and simulated HR from user
   GetUserInfo();

   return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// GetUserInfo
//
// Get a configuration information from the user
//
////////////////////////////////////////////////////////////////////////////////
BOOL Demo::GetUserInfo()
{
   //get a HRM device number (in reality this would be have to be selected to minimize confilicts of device numbers
   do
   {
       printf("HRM device number (range 0 - 65535): "); fflush(stdout);
      char st[1024];
      fgets(st, sizeof(st), stdin);
      sscanf(st, "%i", &siDeviceNumberANT);
      fflush(stdin);
      if((siDeviceNumberANT >=65535) || (siDeviceNumberANT < 1))
         printf("Invalid device number...\n"); fflush(stdout);
   }while  ((siDeviceNumberANT > 6553) || (siDeviceNumberANT < 1));

   //Get a simulated test heartrate
   INT  siTempHeartRate;
   BOOL bSuccess;
   do{
    printf("Enter a heartrate (range 1-255 BPM): "); fflush(stdout);
   char st[1024];
   fgets(st, sizeof(st), stdin); fflush(stdin);
   bSuccess = sscanf(st, "%i", &siTempHeartRate);
   if((siTempHeartRate > 255) || (siTempHeartRate <= 0))
      printf("Invalid heartrate...\n");
   }while ( !bSuccess || (siTempHeartRate > 255) || (siTempHeartRate <= 0) );
    ucHeartRate = (UCHAR)siTempHeartRate;
   return (TRUE);
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

   // Start ANT channel setup
   bStatus = InitANT();


   SetConsoleCtrlHandler( (PHANDLER_ROUTINE) CtrlHandler, TRUE );

   while(!bMyDone)
   {
    if( bCaughtSig == TRUE )
       bMyDone = TRUE;
    HeartBeat();
    DSIThread_Sleep(0);
   }

   //Disconnecting from module
   printf("Disconnecting module...\n");
   this->Close();

   printf("Demo has completed successfully!\n");
   system("PAUSE");


   return;
}

////////////////////////////////////////////////////////////////////////////////
// CtrlHandler
//
// Allows the user to exit by using ctrl-c
// Closes the usb device properly.
//
////////////////////////////////////////////////////////////////////////////////
BOOL Demo::CtrlHandler( DWORD fdwCtrlType )
{
   if(fdwCtrlType == CTRL_C_EVENT)
   {
      bCaughtSig = TRUE;
   }
   return TRUE;
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
// HeartBeat
//
// Check if it is time for another heartbeat (based on user heartrate) and if it is, simulate one
////////////////////////////////////////////////////////////////////////////////
void Demo::HeartBeat()
{
      static ULONGLONG ullCurrentTime = ullStartupTime;
      static ULONGLONG ullLastTime    = 0;

      ULONGLONG ullTempTime = GetTickCount64();
      if( ucHeartRate*(ullTempTime - ullLastTime) > 60000 )
      {
         usHeartBeatEventTime = ((ullTempTime - ullStartupTime)*1024)/1000 ; //Calculate when the heartbeat happened and convert to 1/1024 instead of ms from the system clock
         //printf("!! %llu ms in 1/1024 is %hu !! \n", ullTempTime - ullStartTime, usHeartBeatEventTime);
         ucHeartBeatCount++; //Count total number of heartbeats
         ullLastTime = ullTempTime; //reset for next beat
      }
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

   static ULONGLONG ullStartTime = GetTickCount64();


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

            bStatus = pclMessageObject->AssignChannel(USER_ANTCHANNEL, PARAMETER_TX_NOT_RX, 0, MESSAGE_TIMEOUT);
               break;
            }


         case MESG_RX_EXT_MESGS_ENABLE_ID:
            printf("Extended messeges enabled.\n");
            break;

            case MESG_ASSIGN_CHANNEL_ID:
            {
               if(stMessage.aucData[2] != RESPONSE_NO_ERROR)
               {
                  printf("Error assigning channel: Code 0%d\n", stMessage.aucData[2]);
                  break;
               }
               printf("Channel assigned\n");
               printf("Setting Channel ID for ");
               bStatus = pclMessageObject->SetChannelID(USER_ANTCHANNEL, (USHORT)siDeviceNumberANT, USER_DEVICETYPE, USER_TRANSTYPE, MESSAGE_TIMEOUT);
            printf("device number: %hu\n",(USHORT)siDeviceNumberANT);
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

            printf("Setting period to %d... \n", USER_PERIOD);
            bStatus = pclMessageObject->SetChannelPeriod(USER_ANTCHANNEL, USER_PERIOD, MESSAGE_TIMEOUT);
            break;

            }


         case MESG_CHANNEL_MESG_PERIOD_ID:
         {
            if(stMessage.aucData[2] != RESPONSE_NO_ERROR)
            {
               printf("Error setting period: Code 0%d\n", stMessage.aucData[2]);
               break;
            }
               printf("Set period to %d. \n", USER_PERIOD);
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
               printf("Channel opened\n");
#if defined (ENABLE_EXTENDED_MESSAGES)
               printf("Enabling extended messages...\n");
               pclMessageObject->RxExtMesgsEnable(TRUE);
#endif
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

               if(stMessage.aucData[2] != RESPONSE_NO_ERROR)
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

               static USHORT usTxCounter = 0; //Counts number of messages sent
               static UCHAR  ucbackgroundDataPageToSend = HRM_DATA_PAGE_1;

               //Messages which always go out regardless of the data page
               aucTransmitBuffer[MESSAGE_BUFFER_DATA8_INDEX] = ucHeartRate; //Byte 7 (big endian)
               aucTransmitBuffer[MESSAGE_BUFFER_DATA7_INDEX] = ucHeartBeatCount;
               aucTransmitBuffer[MESSAGE_BUFFER_DATA6_INDEX] = (UCHAR)(usHeartBeatEventTime >> 8); //msb
               aucTransmitBuffer[MESSAGE_BUFFER_DATA5_INDEX] = (UCHAR)(usHeartBeatEventTime & MASK_HEARTBEAT_EVENT_TIME_LSB) ; //lsb

               if ( (usTxCounter >= 0) && usTxCounter < 4 ) //Time to send a background page ( must send 4 background pages in a row to accomadate 2 and 1hz receivers
               {
                  ULONGLONG ullTimeRightNow = GetTickCount64();


                  switch(ucbackgroundDataPageToSend){
                     case HRM_DATA_PAGE_1:   //send data page 1

                        ullTimeSinceStart = (ullTimeRightNow - ullStartupTime)/1000/2;

                        //Page Number
                        aucTransmitBuffer[MESSAGE_BUFFER_DATA1_INDEX] = HRM_DATA_PAGE_1;

                        //Cumulative Operating Time
                        aucTransmitBuffer[MESSAGE_BUFFER_DATA2_INDEX] = ullTimeSinceStart & 0xFF;
                        aucTransmitBuffer[MESSAGE_BUFFER_DATA3_INDEX] = (ullTimeSinceStart>>8) & 0xFF;
                        aucTransmitBuffer[MESSAGE_BUFFER_DATA4_INDEX] = (ullTimeSinceStart>>16) & 0xFF;

                        if(usTxCounter == 3)
                           ucbackgroundDataPageToSend = HRM_DATA_PAGE_2;
                        break;

                     case HRM_DATA_PAGE_2:
                        //Page Number
                        aucTransmitBuffer[MESSAGE_BUFFER_DATA1_INDEX] = HRM_DATA_PAGE_2;

                        //Manufacture ID
                        aucTransmitBuffer[MESSAGE_BUFFER_DATA2_INDEX] = HRM_MANUFACTURER_ID;

                        //Serial Number
                        aucTransmitBuffer[MESSAGE_BUFFER_DATA3_INDEX] = HRM_UPPER_SERIAL_NUMBER & MASK_SERIAL_NUMBER_LSB;
                        aucTransmitBuffer[MESSAGE_BUFFER_DATA4_INDEX] = (HRM_UPPER_SERIAL_NUMBER>>8) & MASK_SERIAL_NUMBER_LSB;

                        if(usTxCounter == 3)
                           ucbackgroundDataPageToSend = HRM_DATA_PAGE_3;
                        break;
                     case HRM_DATA_PAGE_3:
                        //Page number
                        aucTransmitBuffer[MESSAGE_BUFFER_DATA1_INDEX] = HRM_DATA_PAGE_3;

                        //Hardware Version
                        aucTransmitBuffer[MESSAGE_BUFFER_DATA2_INDEX] = HRM_HARDWARE_VERSION;

                        //Software Version
                        aucTransmitBuffer[MESSAGE_BUFFER_DATA3_INDEX] = HRM_SOFTWARE_VERSION;

                        //Model Number
                        aucTransmitBuffer[MESSAGE_BUFFER_DATA4_INDEX] = HRM_MODEL_NUMBER;

                        if(usTxCounter == 3)
                           ucbackgroundDataPageToSend = HRM_DATA_PAGE_1;
                        break;
                  }
               }
               else //Time to send a regular page (Page 0)
               {
                  aucTransmitBuffer[MESSAGE_BUFFER_DATA1_INDEX] = HRM_DATA_PAGE_0;
                  for(UCHAR i = MESSAGE_BUFFER_DATA2_INDEX; i <= MESSAGE_BUFFER_DATA4_INDEX; i++){
                     aucTransmitBuffer[i] = HRM_RESERVED;
                  }
               }

               //Handle the toggle bit
               static UCHAR ucTogglePattern = MASK_TOGGLE_BIT;
               if(usTxCounter%4 == 0)
                  ucTogglePattern = ~ucTogglePattern;

               if(ucTogglePattern == MASK_TOGGLE_BIT)
                  aucTransmitBuffer[MESSAGE_BUFFER_DATA1_INDEX] = aucTransmitBuffer[MESSAGE_BUFFER_DATA1_INDEX] | ucTogglePattern;
               else
                  aucTransmitBuffer[MESSAGE_BUFFER_DATA1_INDEX] = aucTransmitBuffer[MESSAGE_BUFFER_DATA1_INDEX] & ucTogglePattern;

               if(++usTxCounter == 68)// reset broadcast counter (NOTE: not 65 because we still want to send 64 main pages before 4 background pages
                  usTxCounter = 0;

                     // Broadcast data will be sent over the air on
                     // the next message period.
                     if(bBroadcasting)
                     {
                        pclMessageObject->SendBroadcastData(USER_ANTCHANNEL, aucTransmitBuffer);

                        // Echo what the data will be over the air on the next message period.
                if(bPrintAllMessages == TRUE)
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

              case EVENT_QUE_OVERFLOW:
              case EVENT_SERIAL_QUE_OVERFLOW:
                 break;


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


     case MESG_RX_EXT_MESGS_ENABLE_ID:
     break;


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
         ullStartupTime = GetTickCount64();  //On reset, the device should start counting uptime again
         break;
      }

      default:
      {
         break;
      }
   }
   return;
}
