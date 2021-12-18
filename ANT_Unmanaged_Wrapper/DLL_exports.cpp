/*
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
 * This module is a wrapper for the ANT communications library, to expose
 * its functionality to the managed .Net environment.  This module is designed
 * to be used together with the ANT_NET managed library, and it is not
 * intended to be used as a standalone interface.  If you need access to
 * the full features of the ANT Libraries, statically link to the ANT_LIB
 * project.
 *
 **************************************************************************
 */


#include "types.h"
#include "macros.h"
#include "version.h"
#include "usb_device_handle.hpp"
#include "dsi_serial_generic.hpp"
#include "dsi_serial_vcp.hpp"
#include "dsi_framer_ant.hpp"
#include "dsi_debug.hpp"
#include "antfs_host.hpp"
#include "antfs_client_channel.hpp"
#include "antfs_directory.h"


#if defined(DSI_TYPES_WINDOWS)
   #define EXPORT extern "C" __declspec(dllexport)
#elif defined(DSI_TYPES_MACINTOSH) || defined(DSI_TYPES_LINUX)
   #define EXPORT extern "C" __attribute__((visibility("default")))
#endif

#define MARSHALL_MESG_MAX_SIZE_VALUE ((UCHAR) 41) //This is so that the marshalling interface is always consistent, this wrapper will check to ensure that the data moving across is not truncated
//MARSHALL_MESG_MAX_SIZE_VALUE must be >= MESG_MAX_SIZE_VALUE

//Error return codes for reference
#define ANT_INIT_ERROR_NO_ERROR            0
#define ANT_INIT_ERROR_LIBRARY_ERROR      -1
#define ANT_INIT_ERROR_INVALID_TYPE       -2
#define ANT_INIT_ERROR_SERIAL_FAIL        -3

//Port Types: these defines are used to decide what type of connection to connect over
#define PORT_TYPE_USB      0
#define PORT_TYPE_COM      1

//Framer Types: These are used to define which framing type to use
#define FRAMER_TYPE_BASIC            0

//Structs defined here so the marshalling is always a consistent size
typedef struct
{
   UCHAR ucMessageID;
   UCHAR aucData[MARSHALL_MESG_MAX_SIZE_VALUE];
} MARSHALL_ANT_MESSAGE;

typedef struct
{
   UCHAR ucSize;
   MARSHALL_ANT_MESSAGE stANTMessage;
} MARSHALL_ANT_MESSAGE_ITEM;


///////////////////////////////////////////////////////////////////////
// Version info for this wrapper
///////////////////////////////////////////////////////////////////////
EXPORT char* getUnmanagedVersion()
{
   return SW_VER_WRAP;
}

///////////////////////////////////////////////////////////////////////
// Initializes and opens USB connection to the module specifying the device number and baudrate,
// returning the intialized framer and serial objects
///////////////////////////////////////////////////////////////////////
EXPORT INT ANT_Init(UCHAR ucUSBDeviceNum, ULONG ulBaudrate, DSISerial** returnSerialPtr, DSIFramerANT** returnFramerPtr, UCHAR ucPortType_, UCHAR ucSerialFrameType_)
{
   //We need to ensure the MARSHALL_MESG_MAX_SIZE_VALUE is always greater than MESG_MAX_SIZE_VALUE or we will get into some serious memory trouble when we write and read message
   //We have to check this at runtime since the #defines are cast and the preprocessor can't evaluate the casts in a statement like #if(((UCHAR)3) > ((UCHAR)4))
   //We throw a message box because this is a compile-time problem that we want to make sure the developer notices
   if((MARSHALL_MESG_MAX_SIZE_VALUE) < (MESG_MAX_SIZE_VALUE))
   {
      MessageBox(NULL, "MARSHALL_MESG_MAX_SIZE_VALUE must be greater than or equal to MESG_MAX_SIZE_VALUE! The Unmanaged Wrapper must be fixed and recompiled. Also ensure the managed library MAX_SIZE parameter matches that of the wrapper.", "Unmanaged Wrapper Library Error", MB_ICONEXCLAMATION | MB_OK);
      return ANT_INIT_ERROR_LIBRARY_ERROR;
   }

   //Create Serial object.
   DSISerial* pclSerialObject = NULL;

   switch(ucPortType_)
   {
      case PORT_TYPE_USB:
        pclSerialObject = new DSISerialGeneric();
        break;

      case PORT_TYPE_COM:
        pclSerialObject = new DSISerialVCP();
        break;

      default: //Invalid port type selection
         return ANT_INIT_ERROR_INVALID_TYPE;
   }

   if(!pclSerialObject)
      return ANT_INIT_ERROR_LIBRARY_ERROR;

   //Initialize Serial object.
   if(!pclSerialObject->Init(ulBaudrate, ucUSBDeviceNum))
      return ANT_INIT_ERROR_LIBRARY_ERROR;

   //Create Framer object.
   DSIFramerANT* pclMessageObject = NULL;
   switch(ucSerialFrameType_)
   {
      case FRAMER_TYPE_BASIC:
         pclMessageObject = new DSIFramerANT(pclSerialObject);
         break;


      default:
         return ANT_INIT_ERROR_INVALID_TYPE;
   }
   if(!pclMessageObject)
      return ANT_INIT_ERROR_LIBRARY_ERROR;

   //Initialize Framer object.
   if(!pclMessageObject->Init())
      return ANT_INIT_ERROR_LIBRARY_ERROR;

   //Let Serial know about Framer.
   pclSerialObject->SetCallback(pclMessageObject);

   //Open Serial.
   if(!pclSerialObject->Open())
      return ANT_INIT_ERROR_SERIAL_FAIL;

   *returnSerialPtr = pclSerialObject;
   *returnFramerPtr = pclMessageObject;

   return ANT_INIT_ERROR_NO_ERROR;
}


/////////////////////////////////////////////////////////////////////////
//// Initializes and opens USB connection to the module (automatic initialization)
//// returning the intialized framer and serial objects
/////////////////////////////////////////////////////////////////////////
//EXPORT INT ANT_AutoInit(DSISerial** returnSerialPtr, DSIFramerANT** returnFramerPtr)
//{
//   //We need to ensure the MARSHALL_MESG_MAX_SIZE_VALUE is always greater than MESG_MAX_SIZE_VALUE or we will get into some serious memory trouble when we write and read message
//   //We have to check this at runtime since the #defines are cast and the preprocessor can't evaluate the casts in a statement like #if(((UCHAR)3) > ((UCHAR)4))
//   //We throw a message box because this is a compile-time problem that we want to make sure the developer notices
//   if((MARSHALL_MESG_MAX_SIZE_VALUE) < (MESG_MAX_SIZE_VALUE))
//   {
//      MessageBox(NULL, "MARSHALL_MESG_MAX_SIZE_VALUE must be greater than or equal to MESG_MAX_SIZE_VALUE! The Unmanaged Wrapper must be fixed and recompiled. Also ensure the managed library MAX_SIZE parameter matches that of the wrapper.", "Unmanaged Wrapper Library Error", MB_ICONEXCLAMATION | MB_OK);
//      return ANT_INIT_ERROR_LIBRARY_ERROR;
//   }
//
//   //Create Serial object.
// DSISerial* pclSerialObject = new DSISerialGeneric();
//   if(!pclSerialObject)
//      return ANT_INIT_ERROR_LIBRARY_ERROR;
//
//   // Initialize Serial object
//   if(!pclSerialObject->AutoInit())
//      return ANT_INIT_SERIAL_INIT_FAIL;
//
//   //Create Framer object.
// DSIFramerANT* pclMessageObject = new DSIFramerANT(pclSerialObject);
//   if(!pclMessageObject)
//      return ANT_INIT_ERROR_LIBRARY_ERROR;
//
//   // Initialize Framer object
//   if(!pclMessageObject->Init())
//      return ANT_INIT_ERROR_LIBRARY_ERROR;
//
//   //Let Serial know about Framer.
// pclSerialObject->SetCallback(pclMessageObject);
//
//   //Open Serial.
// if(!pclSerialObject->Open())
//      return ANT_INIT_ERROR_SERIAL_OPEN_FAIL;
//
// *returnSerialPtr = pclSerialObject;
// *returnFramerPtr = pclMessageObject;
//
//   return ANT_INIT_ERROR_NO_ERROR;
//}


///////////////////////////////////////////////////////////////////////
// Called by the application to close the usb connection
// Also deletes instantiated objects associated with pointer
// Should be called in destructors only
///////////////////////////////////////////////////////////////////////
EXPORT void ANT_Close(DSISerial* SerialPtr, DSIFramerANT* FramerPtr)
{
   //Close all serial communication before we delete
   if(SerialPtr)
   {
      SerialPtr->Close();
      delete SerialPtr;
   }
   if(FramerPtr)
   {
      delete FramerPtr;
   }
}

///////////////////////////////////////////////////////////////////////
// Called by the application to reset the USB device
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_USBReset(DSISerial* SerialPtr)
{
   DSISerialGeneric *pclCastedSerial = (DSISerialGeneric*)SerialPtr;
   pclCastedSerial->USBReset();
   return(TRUE);
}


///////////////////////////////////////////////////////////////////////
// Called by the application to restart ANT on the module
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_ResetSystem(DSIFramerANT* FramerPtr, ULONG ulResponseTime_)
{
   return(FramerPtr->ResetSystem(ulResponseTime_));
}

///////////////////////////////////////////////////////////////////////
// Called by the application to set the network address for a given
// module channel
//!! This is (should be) a private network function
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetNetworkKey_RTO(DSIFramerANT* FramerPtr, UCHAR ucNetNumber, UCHAR *pucKey, ULONG ulResponseTime_)
{
   return(FramerPtr->SetNetworkKey(ucNetNumber, pucKey, ulResponseTime_));
}


///////////////////////////////////////////////////////////////////////
// Called by the application to assign a channel
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_AssignChannel_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel, UCHAR ucChannelType_, UCHAR ucNetNumber, ULONG ulResponseTime_)
{
   return(FramerPtr->AssignChannel(ucANTChannel, ucChannelType_, ucNetNumber, ulResponseTime_));
}


///////////////////////////////////////////////////////////////////////
// Called by the application to assign a channel using extended assignment
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_AssignChannelExt_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel, UCHAR ucChannelType_, UCHAR ucNetNumber, UCHAR ucExtFlags_, ULONG ulResponseTime_)
{
   UCHAR aucChannelType[] = {ucChannelType_, ucExtFlags_};  // Channel Type + Extended Assignment Byte

   return(FramerPtr->AssignChannelExt(ucANTChannel, aucChannelType, 2, ucNetNumber, ulResponseTime_));
}



///////////////////////////////////////////////////////////////////////
// Called by the application to unassign a channel
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_UnAssignChannel_RTO(DSIFramerANT* FramerPtr,UCHAR ucANTChannel, ULONG ulResponseTime_)
{
   return(FramerPtr->UnAssignChannel(ucANTChannel, ulResponseTime_));
}



///////////////////////////////////////////////////////////////////////
// Called by the application to set the channel ID
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetChannelId_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, USHORT usDeviceNumber_, UCHAR ucDeviceType_, UCHAR ucTransmissionType_, ULONG ulResponseTime_)
{
   return(FramerPtr->SetChannelID(ucANTChannel_, usDeviceNumber_, ucDeviceType_, ucTransmissionType_, ulResponseTime_));
}


///////////////////////////////////////////////////////////////////////
// Called by the application to set the messaging period
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetChannelPeriod_RTO(DSIFramerANT* FramerPtr,UCHAR ucANTChannel_, USHORT usMesgPeriod_, ULONG ulResponseTime_)
{
   return(FramerPtr->SetChannelPeriod(ucANTChannel_, usMesgPeriod_, ulResponseTime_));
}


///////////////////////////////////////////////////////////////////////
// Called by the application to set the RSSI threshold
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_RSSI_SetSearchThreshold_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR ucThreshold_, ULONG ulResponseTime_)
{
   return(FramerPtr->SetRSSISearchThreshold(ucANTChannel_, ucThreshold_, ulResponseTime_));
}


///////////////////////////////////////////////////////////////////////
// Used to set Low Priority Search Timeout. Not available on AP1
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetLowPriorityChannelSearchTimeout_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR ucSearchTimeout_, ULONG ulResponseTime_)
{
   return(FramerPtr->SetLowPriorityChannelSearchTimeout(ucANTChannel_, ucSearchTimeout_, ulResponseTime_));
}


///////////////////////////////////////////////////////////////////////
// Called by the application to set the search timeout for a particular
// channel on the module
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetChannelSearchTimeout_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR ucSearchTimeout_, ULONG ulResponseTime_)
{
   return(FramerPtr->SetChannelSearchTimeout(ucANTChannel_, ucSearchTimeout_, ulResponseTime_));
}


///////////////////////////////////////////////////////////////////////
// Called by the application to set the RF frequency for a given channel
//!! This is (should be) a private network function
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetChannelRFFreq_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR ucRFFreq_, ULONG ulResponseTime_)
{
   return(FramerPtr->SetChannelRFFrequency(ucANTChannel_, ucRFFreq_, ulResponseTime_));
}


///////////////////////////////////////////////////////////////////////
// Called by the application to set the transmit power for the module
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetTransmitPower_RTO(DSIFramerANT* FramerPtr, UCHAR ucTransmitPower_, ULONG ulResponseTime_)
{
   return(FramerPtr->SetAllChannelsTransmitPower(ucTransmitPower_, ulResponseTime_));
}


///////////////////////////////////////////////////////////////////////
// Called by the application to set the transmit power for a channel
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetChannelTxPower_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR ucTransmitPower_, ULONG ulResponseTime_)
{
   return(FramerPtr->SetChannelTransmitPower(ucANTChannel_, ucTransmitPower_, ulResponseTime_));
}

///////////////////////////////////////////////////////////////////////
// Called by the application to configure the splitting of
// advanced burst messages.
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_ConfigureSplitAdvancedBursts(DSIFramerANT *FramerPtr, BOOL bEnableSplitBursts)
{
   FramerPtr->SetSplitAdvBursts(bEnableSplitBursts);
   return(TRUE);
}

///////////////////////////////////////////////////////////////////////
// Called by the application to configure the advanced bursting format
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_ConfigureAdvancedBurst_RTO(DSIFramerANT* FramerPtr, BOOL bEnable_, UCHAR ucMaxPacketLength_, ULONG ulRequiredFields_, ULONG ulOptionalFields_, ULONG ulResponseTime_)
{
   return(FramerPtr->ConfigAdvancedBurst(bEnable_, ucMaxPacketLength_, ulRequiredFields_, ulOptionalFields_, ulResponseTime_));
}

///////////////////////////////////////////////////////////////////////
// Called by the application to configure the advanced bursting
// format with extended details
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_ConfigureAdvancedBurst_ext_RTO(DSIFramerANT* FramerPtr, BOOL bEnable_, UCHAR ucMaxPacketLength_, ULONG ulRequiredFields_, ULONG ulOptionalFields_, USHORT usStallCount_, UCHAR ucRetryCount_, ULONG ulResponseTime_)
{
   return(FramerPtr->ConfigAdvancedBurst_ext(bEnable_, ucMaxPacketLength_, ulRequiredFields_, ulOptionalFields_, usStallCount_, ucRetryCount_, ulResponseTime_));
}

///////////////////////////////////////////////////////////////////////
// Called by the application to request a generic message
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_RequestMessage(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR ucMessageID_, MARSHALL_ANT_MESSAGE_ITEM* stResponse, ULONG ulResponseTime_)
{
   ANT_MESSAGE_ITEM stTempResponse;
   USHORT usReturn = FramerPtr->SendRequest(ucMessageID_, ucANTChannel_, &stTempResponse, ulResponseTime_);

   stResponse->ucSize = stTempResponse.ucSize;
   memcpy(&(stResponse->stANTMessage), &(stTempResponse.stANTMessage), sizeof(ANT_MESSAGE));   //MARSHALL_ANT_MESSAGE will always be big enough (enforced by defines)

   return usReturn;
}

///////////////////////////////////////////////////////////////////////
// Called by the application to request a generic message (User Nvm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_RequestUserNvmMessage(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR ucMessageID_, MARSHALL_ANT_MESSAGE_ITEM* stResponse, USHORT usAddress_, UCHAR ucSize_, ULONG ulResponseTime_)
{
   ANT_MESSAGE_ITEM stTempResponse;
   USHORT usReturn = FramerPtr->SendUserNvmRequest(ucMessageID_, ucANTChannel_, &stTempResponse, usAddress_, ucSize_, ulResponseTime_);

   stResponse->ucSize = stTempResponse.ucSize;
   memcpy(&(stResponse->stANTMessage), &(stTempResponse.stANTMessage), sizeof(ANT_MESSAGE));   //MARSHALL_ANT_MESSAGE will always be big enough (enforced by defines)

   return usReturn;
}


///////////////////////////////////////////////////////////////////////
// Called by the application to open an assigned channel
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_OpenChannel_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, ULONG ulResponseTime_)
{
   return(FramerPtr->OpenChannel(ucANTChannel_, ulResponseTime_));
}


///////////////////////////////////////////////////////////////////////
// Called by the application to close an opened channel
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_CloseChannel_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, ULONG ulResponseTime_)
{
   return(FramerPtr->CloseChannel(ucANTChannel_, ulResponseTime_));
}



///////////////////////////////////////////////////////////////////////
// Called by the application to construct and send a broadcast data message.
// This message will be broadcast on the next synchronous channel period.
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SendBroadcastData(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR *pucData_)
{
   return(FramerPtr->SendBroadcastData(ucANTChannel_, pucData_));
}


///////////////////////////////////////////////////////////////////////
// Called by the application to construct and send an acknowledged data
// mesg.  This message will be transmitted on the next synchronous channel
// period.
// <returns>0=fail, 1=pass, 2=timeout, 3=cancelled
///////////////////////////////////////////////////////////////////////
EXPORT UCHAR ANT_SendAcknowledgedData_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR *pucData_, ULONG ulResponseTime_)
{
   return (UCHAR)FramerPtr->SendAcknowledgedData( ucANTChannel_, pucData_, ulResponseTime_);
}


///////////////////////////////////////////////////////////////////////
// Used to send burst data using a block of data. This function manages
// the entire burst including composing the individual burst packets
// and their sequence numbers.
// <returns>0=fail, 1=pass, 2=timeout, 3=cancelled
///////////////////////////////////////////////////////////////////////
EXPORT UCHAR ANT_SendBurstTransfer_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR *pucData_, ULONG ulNumBytes, ULONG ulResponseTime_)
{
   return (UCHAR)FramerPtr->SendTransfer( ucANTChannel_, pucData_, ulNumBytes, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Used to send advanced burst data using a block of data. This function manages
// the entire burst including composing the individual burst packets
// and their sequence numbers.
// ucStdPcktsPerSerialMsg_ determines how many bytes of data are sent in each packet
// over the serial port in multiples of packet size (the size of the packet payload at the
// wireless level is determined by the pckt size set in the ConfigureAdvancedBurst command).
// <returns>0=fail, 1=pass, 2=timeout, 3=cancelled, 4=invalid param
///////////////////////////////////////////////////////////////////////
EXPORT UCHAR ANT_SendAdvancedBurstTransfer_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR *pucData_, ULONG ulNumBytes, UCHAR ucStdPcktsPerSerialMsg_, ULONG ulResponseTime_)
{
   return (UCHAR)FramerPtr->SendAdvancedTransfer(ucANTChannel_, pucData_, ulNumBytes, ucStdPcktsPerSerialMsg_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Called by the application to construct and send an extended broadcast data message.
// This message will be broadcast on the next synchronous channel period.
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SendExtBroadcastData(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, USHORT usDeviceNumber_, UCHAR ucDeviceType_, UCHAR ucTransmitType_, UCHAR *pucData_)
{
   UCHAR IDandData[MESG_EXT_DATA_SIZE-1];
   IDandData[0] = (UCHAR)(usDeviceNumber_ & 0xFF);
   IDandData[1] = (UCHAR)((usDeviceNumber_ >>8) & 0xFF);
   IDandData[2] = ucDeviceType_;
   IDandData[3] = ucTransmitType_;
   memcpy(IDandData+4, pucData_, ANT_STANDARD_DATA_PAYLOAD_SIZE);

   return(FramerPtr->SendExtBroadcastData(ucANTChannel_, IDandData));
}


///////////////////////////////////////////////////////////////////////
// Called by the application to construct and send an extended acknowledged data
// mesg.  This message will be transmitted on the next synchronous channel
// period.
// <returns>0=fail, 1=pass, 2=timeout, 3=cancelled
///////////////////////////////////////////////////////////////////////
EXPORT UCHAR ANT_SendExtAcknowledgedData_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, USHORT usDeviceNumber_, UCHAR ucDeviceType_, UCHAR ucTransmitType_, UCHAR *pucData_, ULONG ulResponseTime_)
{
   UCHAR IDandData[MESG_EXT_DATA_SIZE-1];
   IDandData[0] = (UCHAR)(usDeviceNumber_ & 0xFF);
   IDandData[1] = (UCHAR)((usDeviceNumber_ >>8) & 0xFF);
   IDandData[2] = ucDeviceType_;
   IDandData[3] = ucTransmitType_;
   memcpy(IDandData+4, pucData_, ANT_STANDARD_DATA_PAYLOAD_SIZE);

   return (UCHAR)FramerPtr->SendExtAcknowledgedData( ucANTChannel_, IDandData, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Used to send extended burst data using a block of data.  Proper sequence number
// of packet is maintained by the function.
// <returns>0=fail, 1=pass, 2=timeout, 3=cancelled
///////////////////////////////////////////////////////////////////////
EXPORT UCHAR ANT_SendExtBurstTransfer_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, USHORT usDeviceNumber_, UCHAR ucDeviceType_, UCHAR ucTransmitType_, UCHAR *pucData_, ULONG ulNumBytes, ULONG ulResponseTime_)
{
   UCHAR* IDandData;
   unsigned int extSize = (unsigned int)( (ulNumBytes/8) *12 );  //We add 4 bytes of ID info to every 8 bytes = 12

   //Handle data that is not sized in 8 byte segments
   int padNum = ulNumBytes%8;
   if(padNum != 0)
      extSize += 12; //Add another payload to fit the last bytes

   IDandData = new UCHAR[extSize];

   //Every 12 bytes must start with the 4 byte ID info
   for(ULONG i = 0; i < (extSize/12); ++i)
   {
      UCHAR* curBlock = IDandData + (i*12);
      curBlock[0] = (UCHAR)(usDeviceNumber_ & 0xFF);
      curBlock[1] = (UCHAR)((usDeviceNumber_ >>8) & 0xFF);
      curBlock[2] = ucDeviceType_;
      curBlock[3] = ucTransmitType_;
      memcpy(curBlock+4, pucData_+(i*8), min(8,ulNumBytes-(i*8)) );  //We copy 8 bytes, or the remaining bytes if we are at the end
   }

   UCHAR returnCode = (UCHAR)FramerPtr->SendExtBurstTransfer( ucANTChannel_, IDandData, extSize, ulResponseTime_);

   delete[] IDandData;

   return returnCode;
}


//////////////////////////////////////////////////////////////////////
// Called by the application to configure and start CW test mode.
// There is no way to turn off CW mode other than to do a reset on the module.
/////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_InitCWTestMode_RTO(DSIFramerANT* FramerPtr, ULONG ulResponseTime_)
{
   return(FramerPtr->InitCWTestMode(ulResponseTime_));
}


//////////////////////////////////////////////////////////////////////
// Called by the application to configure and start CW test mode.
// There is no way to turn off CW mode other than to do a reset on the module.
/////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetCWTestMode_RTO(DSIFramerANT* FramerPtr, UCHAR ucTransmitPower_, UCHAR ucRFChannel_, ULONG ulResponseTime_)
{
   return(FramerPtr->SetCWTestMode(ucTransmitPower_, ucRFChannel_, ulResponseTime_));
}

///////////////////////////////////////////////////////////////////////
// Add a channel ID to a channel's include/exclude ID list
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_AddChannelID_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, USHORT usDeviceNumber_, UCHAR ucDeviceType_, UCHAR ucTransmissionType_, UCHAR ucListIndex_, ULONG ulResponseTime_)
{
   return(FramerPtr->AddChannelID(ucANTChannel_, usDeviceNumber_, ucDeviceType_, ucTransmissionType_, ucListIndex_, ulResponseTime_));
}


///////////////////////////////////////////////////////////////////////
// Configure the size and type of a channel's include/exclude ID list
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_ConfigList_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR ucListSize_, UCHAR ucExclude_, ULONG ulResponseTime_)
{
   return(FramerPtr->ConfigList(ucANTChannel_, ucListSize_, ucExclude_, ulResponseTime_));
}

///////////////////////////////////////////////////////////////////////
// Open Scan Mode
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_OpenRxScanMode_RTO(DSIFramerANT* FramerPtr, ULONG ulResponseTime_)
{
   return(FramerPtr->OpenRxScanMode(ulResponseTime_));
}

///////////////////////////////////////////////////////////////////////
// Called by the application to write NVM data (SensRcore script)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_Script_Write_RTO(DSIFramerANT* FramerPtr, UCHAR ucSize_, UCHAR *pucData_, ULONG ulResponseTime_)
{
   return(FramerPtr->ScriptWrite(ucSize_, pucData_, ulResponseTime_ ));
}



///////////////////////////////////////////////////////////////////////
// Called by the application to clear NVM data (SensRcore script)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_Script_Clear_RTO(DSIFramerANT* FramerPtr, ULONG ulResponseTime_)
{
   return(FramerPtr->ScriptClear(ulResponseTime_));
}



///////////////////////////////////////////////////////////////////////
// Called by the application to set default NVM sector (SensRcore script)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_Script_SetDefaultSector_RTO(DSIFramerANT* FramerPtr, UCHAR ucSectNumber_, ULONG ulResponseTime_)
{
   return(FramerPtr->ScriptSetDefaultSector(ucSectNumber_, ulResponseTime_));
}



///////////////////////////////////////////////////////////////////////
// Called by the application to end NVM sector (SensRcore script)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_Script_EndSector_RTO(DSIFramerANT* FramerPtr, ULONG ulResponseTime_)
{
   return(FramerPtr->ScriptEndSector(ulResponseTime_));
}



///////////////////////////////////////////////////////////////////////
// Called by the application to dump the contents of the
// NVM (SensRcore script)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_Script_Dump_RTO(DSIFramerANT* FramerPtr, ULONG ulResponseTime_)
{
   return(FramerPtr->ScriptDump(ulResponseTime_));
}



///////////////////////////////////////////////////////////////////////
// Called by the application to lock the contents of the NVM
// (SensRcore script)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_Script_Lock_RTO(DSIFramerANT* FramerPtr, ULONG ulResponseTimeout_)
{
   return(FramerPtr->ScriptLock(ulResponseTimeout_));
}


///////////////////////////////////////////////////////////////////////
// Called by the application to set the state of the FE
// (FIT1e only)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL FIT_SetFEState_RTO(DSIFramerANT* FramerPtr, UCHAR ucFEState_, ULONG ulResponseTime_)
{
   return(FramerPtr->FITSetFEState(ucFEState_, ulResponseTime_));
}

///////////////////////////////////////////////////////////////////////
// Called by the application to adjust the pairing distance
// (FIT1e only)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL FIT_AdjustPairingSettings_RTO(DSIFramerANT* FramerPtr, UCHAR ucSearchLv_, UCHAR ucPairLv_, UCHAR ucTrackLv_, ULONG ulResponseTime_)
{
   return(FramerPtr->FITAdjustPairingSettings(ucSearchLv_, ucPairLv_, ucTrackLv_, ulResponseTime_));
}

///////////////////////////////////////////////////////////////////////
// Used to force the module to use extended rx messages all the time
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_RxExtMesgsEnable_RTO(DSIFramerANT* FramerPtr, UCHAR ucEnable_, ULONG ulResponseTimeout_)
{
   return(FramerPtr->RxExtMesgsEnable(ucEnable_, ulResponseTimeout_));
}

///////////////////////////////////////////////////////////////////////
// Used to set a channel device ID to the module serial number
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetSerialNumChannelId_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR ucDeviceType_, UCHAR ucTransmissionType_, ULONG ulResponseTime_)
{
   return(FramerPtr->SetSerialNumChannelId(ucANTChannel_, ucDeviceType_, ucTransmissionType_, ulResponseTime_));
}


///////////////////////////////////////////////////////////////////////
// Enables the module LED to flash on RF activity
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_EnableLED_RTO(DSIFramerANT* FramerPtr, UCHAR ucEnable_, ULONG ulResponseTime_)
{
   return(FramerPtr->EnableLED(ucEnable_, ulResponseTime_));
}


///////////////////////////////////////////////////////////////////////
// Called by the application to get the USB PID
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_GetDeviceUSBPID(DSIFramerANT* FramerPtr, USHORT* pusPID_)
{
   return(FramerPtr->GetDeviceUSBPID(*pusPID_));
}

///////////////////////////////////////////////////////////////////////
// Called by the application to get the USB VID
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_GetDeviceUSBVID(DSIFramerANT* FramerPtr, USHORT* pusVID_)
{
   return(FramerPtr->GetDeviceUSBVID(*pusVID_));
}

/////////////////////////////////////////////////////////////////////////
//// Called by the application to get the product string and serial number string at a particular device number
////  The buffers should have size = USB_MAX_STRLEN
/////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_GetDeviceUSBInfo(DSIFramerANT* FramerPtr, UCHAR ucDeviceNum, UCHAR* pucProductString, UCHAR* pucSerialString)
{
   return(FramerPtr->GetDeviceUSBInfo(ucDeviceNum, pucProductString, pucSerialString, USB_MAX_STRLEN));
}

/////////////////////////////////////////////////////////////////////////
//// Called by the application to get the product serial string of the given device
////  The buffer should have size = USB_MAX_STRLEN
/////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_GetDeviceSerialString(DSISerialGeneric* SerialPtr, UCHAR* pucSerialString)
{
   return SerialPtr->GetDeviceSerialString(pucSerialString, USB_MAX_STRLEN);
}

///////////////////////////////////////////////////////////////////////
// Called by the application to get the serial number
///////////////////////////////////////////////////////////////////////
EXPORT ULONG ANT_GetDeviceSerialNumber(DSISerialGeneric* SerialPtr)
{
   return SerialPtr->GetDeviceSerialNumber();
}


///////////////////////////////////////////////////////////////////////
// Returns the number of ANT devices detected
///////////////////////////////////////////////////////////////////////
EXPORT ULONG ANT_GetNumDevices()
{
   return USBDeviceHandle::GetAllDevices().GetSize();
}


///////////////////////////////////////////////////////////////////////
// Enable use of external 32KHz Crystal (AP2)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_CrystalEnable(DSIFramerANT* FramerPtr, ULONG ulResponseTime_)
{
   return FramerPtr->CrystalEnable(ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetLibConfig(DSIFramerANT* FramerPtr, UCHAR ucLibConfigFlags_, ULONG ulResponseTime_)
{
   return FramerPtr->SetLibConfig(ucLibConfigFlags_, ulResponseTime_);
}



///////////////////////////////////////////////////////////////////////
// Configure proximity search (not on AP1 or AT3)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetProximitySearch(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR ucSearchThreshold_, ULONG ulResponseTime_)
{
   return FramerPtr->SetProximitySearch(ucANTChannel_, ucSearchThreshold_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Configure ANT Frequency Agility Functionality (not on AP1 or AT3)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_ConfigFrequencyAgility(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR ucFreq1_, UCHAR ucFreq2_, UCHAR ucFreq3_, ULONG ulResponseTime_)
{
   return FramerPtr->ConfigFrequencyAgility(ucANTChannel_, ucFreq1_, ucFreq2_, ucFreq3_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Configure ANT Event Buffer Functionality (only on USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_ConfigEventBuffer(DSIFramerANT* FramerPtr, UCHAR ucConfig_, USHORT usSize_, USHORT usTime_, ULONG ulResponseTime_)
{
   return FramerPtr->ConfigEventBuffer(ucConfig_, usSize_, usTime_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Configure ANT Event Filter Functionality (USBm and nRF5)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_ConfigEventFilter(DSIFramerANT* FramerPtr, USHORT usEventFilter_, ULONG ulResponseTime_)
{
   return FramerPtr->ConfigEventFilter(usEventFilter_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Configure ANT High Duty Search (only USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_ConfigHighDutySearch(DSIFramerANT* FramerPtr, UCHAR ucEnable_, UCHAR ucSuppressionCycles_, ULONG ulResponseTime_)
{
   return FramerPtr->ConfigHighDutySearch(ucEnable_, ucSuppressionCycles_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Configure ANT Selective Data Update Functionality (only USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_ConfigSelectiveDataUpdate(DSIFramerANT* FramerPtr, UCHAR ucChannel_, UCHAR ucSduConfig_, ULONG ulResponseTime_)
{
   return FramerPtr->ConfigSelectiveDataUpdate(ucChannel_, ucSduConfig_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Set ANT Selective Data Update Mask (only USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetSelectiveDataUpdateMask(DSIFramerANT* FramerPtr, UCHAR ucMaskNumber_, UCHAR* pucSduMask_, ULONG ulResponseTime_)
{
   return FramerPtr->SetSelectiveDataUpdateMask(ucMaskNumber_, pucSduMask_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Configure ANT User NVM (only USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_ConfigUserNVM(DSIFramerANT* FramerPtr, USHORT usAddress_, UCHAR* pucData_, UCHAR ucSize_, ULONG ulResponseTime_)
{
   return FramerPtr->ConfigUserNVM(usAddress_, pucData_, ucSize_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Configure ANT Search Priority
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetChannelSearchPriority(DSIFramerANT* FramerPtr, UCHAR ucChannel_, UCHAR ucPriorityLevel_, ULONG ulResponseTime_)
{
   return FramerPtr->SetChannelSearchPriority(ucChannel_, ucPriorityLevel_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Configure ANT Search Sharing (only USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetSearchSharingCycles(DSIFramerANT* FramerPtr, UCHAR ucChannel_, UCHAR ucSearchSharingCycles_, ULONG ulResponseTime_)
{
   return FramerPtr->SetSearchSharingCycles(ucChannel_, ucSearchSharingCycles_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Enable channel encryption (only on USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_EncryptedChannelEnable_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR ucMode_, UCHAR ucVolatileKeyIndex_, UCHAR ucDecimationRate_, ULONG ulResponseTime_)
{
   return FramerPtr->EncryptedChannelEnable(ucANTChannel_, ucMode_, ucVolatileKeyIndex_, ucDecimationRate_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Add encryption ID to ANT Lists (only on USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_AddCryptoID_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR* pucData_, UCHAR ucListIndex_, ULONG ulResponseTime_)
{
   return FramerPtr->AddCryptoID(ucANTChannel_, pucData_, ucListIndex_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Configure ANT Encryption Master Blacklist/Whitelist (only on USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_ConfigCryptoList_RTO(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR ucListSize_, UCHAR ucBlacklist_, ULONG ulResponseTime_)
{
   return FramerPtr->ConfigCryptoList(ucANTChannel_, ucListSize_, ucBlacklist_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Set the encryption key in volatile memory (only on USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetCryptoKey_RTO(DSIFramerANT* FramerPtr, UCHAR ucVolatileKeyIndex, UCHAR *pucKey_, ULONG ulResponseTime_)
{
   return FramerPtr->SetCryptoKey(ucVolatileKeyIndex, pucKey_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Set the encryption ID (only on USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetCryptoID_RTO(DSIFramerANT* FramerPtr, UCHAR *pucData_, ULONG ulResponseTime_)
{
   return FramerPtr->SetCryptoID(pucData_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Set the ANT Encrypted Slave User Information String (only on USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetCryptoUserInfo_RTO(DSIFramerANT* FramerPtr, UCHAR *pucData_, ULONG ulResponseTime_)
{
   return FramerPtr->SetCryptoUserInfo(pucData_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Set the Encryption RNG Seed (only on USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetCryptoRNGSeed_RTO(DSIFramerANT* FramerPtr, UCHAR *pucData_, ULONG ulResponseTime_)
{
   return FramerPtr->SetCryptoRNGSeed(pucData_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Set an encryption information parameter (only on USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetCryptoInfo_RTO(DSIFramerANT* FramerPtr, UCHAR ucParameter_, UCHAR *pucData_, ULONG ulResponseTime_)
{
   return FramerPtr->SetCryptoInfo(ucParameter_, pucData_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Load an encryption key into volatile memory from NVM (only on USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_LoadCryptoKeyNVMOp_RTO(DSIFramerANT* FramerPtr, UCHAR ucNVMKeyIndex_, UCHAR ucVolatileKeyIndex_, ULONG ulResponseTime_)
{
   return FramerPtr->LoadCryptoKeyNVMOp(ucNVMKeyIndex_, ucVolatileKeyIndex_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Store an encryption key into NVM (only on USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_StoreCryptoKeyNVMOp_RTO(DSIFramerANT* FramerPtr, UCHAR ucNVMKeyIndex_, UCHAR *pucKey_, ULONG ulResponseTime_)
{
   return FramerPtr->StoreCryptoKeyNVMOp(ucNVMKeyIndex_, pucKey_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Perform an ANT Encryption NVM Operation (only on USBm)
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_CryptoKeyNVMOp_RTO(DSIFramerANT* FramerPtr, UCHAR ucOperation_, UCHAR ucNVMKeyIndex_, UCHAR *pucData_, ULONG ulResponseTime_)
{
   return FramerPtr->CryptoKeyNVMOp(ucOperation_, ucNVMKeyIndex_, pucData_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Sends a generic message to ANT
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_WriteMessage(DSIFramerANT* FramerPtr, MARSHALL_ANT_MESSAGE pstANTMessage, USHORT usMessageSize)
{
   ANT_MESSAGE pstTempANTMessage;
   memcpy(&pstTempANTMessage, &pstANTMessage, sizeof(ANT_MESSAGE));   //Don't need to worry about missing data because if usMessageSize is too big WriteMessage returns false
   return FramerPtr->WriteMessage(&pstTempANTMessage, usMessageSize);
}


///////////////////////////////////////////////////////////////////////
// Set cancel parameter
///////////////////////////////////////////////////////////////////////
EXPORT void ANT_SetCancelParameter(DSIFramerANT* FramerPtr, BOOL *pbCancel)
{
   FramerPtr->SetCancelParameter(pbCancel);
}

///////////////////////////////////////////////////////////////////////
// Get channel number associated to a received ANT message
///////////////////////////////////////////////////////////////////////
EXPORT UCHAR ANT_GetChannelNumber(DSIFramerANT* FramerPtr, MARSHALL_ANT_MESSAGE* pstANTMessage_)
{
   ANT_MESSAGE stTempANTMessage;
   memcpy(&stTempANTMessage, pstANTMessage_, sizeof(ANT_MESSAGE));  // Responses from ANT would typically be shorter
   return FramerPtr->GetChannelNumber(&stTempANTMessage);
}

///////////////////////////////////////////////////////////////////////
// Request capabilities
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_GetCapabilities(DSIFramerANT* FramerPtr, UCHAR* pucCapabilities_, UCHAR ucBufferSize_, ULONG ulResponseTime_)
{
   if(ucBufferSize_ < MESG_CAPABILITIES_SIZE)
   {
      MessageBox(NULL, "ucBufferSize_ must be at least CAPABILITIES_BUFFER_SIZE! The Managed Library must be fixed and recompiled", "Unmanaged Wrapper Library Error", MB_ICONEXCLAMATION | MB_OK);
      return FALSE;
   }

   return FramerPtr->GetCapabilities(pucCapabilities_, ulResponseTime_);
}

///////////////////////////////////////////////////////////////////////
// Request Channel ID
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_GetChannelID(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, USHORT *pusDeviceNumber_, UCHAR *pucDeviceType_, UCHAR *pucTransmitType_, ULONG ulResponseTime_)
{
   return FramerPtr->GetChannelID(ucANTChannel_, pusDeviceNumber_, pucDeviceType_, pucTransmitType_, ulResponseTime_);
}


///////////////////////////////////////////////////////////////////////
// Request Channel Status
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_GetChannelStatus(DSIFramerANT* FramerPtr, UCHAR ucANTChannel_, UCHAR *pucStatus_, ULONG ulResponseTime_)
{
   return FramerPtr->GetChannelStatus(ucANTChannel_, pucStatus_, ulResponseTime_);
}


///////////////////////////////////////////////////////////////////////
//
// ANT Callback Functions
//
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
EXPORT USHORT ANT_WaitForMessage(DSIFramerANT* FramerPtr, ULONG ulMilliseconds_)
{
   return FramerPtr->WaitForMessage(ulMilliseconds_);
}

///////////////////////////////////////////////////////////////////////
EXPORT USHORT ANT_GetMessage(DSIFramerANT* FramerPtr, MARSHALL_ANT_MESSAGE* stResponse)
{
   ANT_MESSAGE stTempResponse;
   USHORT usReturn = FramerPtr->GetMessage(&stTempResponse);
   memcpy(stResponse, &stTempResponse, sizeof(ANT_MESSAGE));   //MARSHALL_ANT_MESSAGE will always be big enough (enforced by defines)

   return usReturn;
}

///////////////////////////////////////////////////////////////////////
//
// ANT Debug Logging Control
//
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Initializes and enables the debug logs
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_EnableDebugLogging()
{
   if(!DSIDebug::Init())
      return FALSE;
   DSIDebug::SetDebug(TRUE);
   return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Disables and closes the debug logs
///////////////////////////////////////////////////////////////////////
EXPORT void ANT_DisableDebugLogging()
{
   DSIDebug::SetDebug(FALSE);
   DSIDebug::Close();
}

///////////////////////////////////////////////////////////////////////
// Set the directory the log files are saved to
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_SetDebugLogDirectory(char* pcDirectory)
{
   return DSIDebug::SetDirectory(pcDirectory);
}


///////////////////////////////////////////////////////////////////////
// Enable application specific logs for the currently executing thread
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_DebugThreadInit(char* pucName_)
{
   return DSIDebug::ThreadInit(pucName_);
}

///////////////////////////////////////////////////////////////////////
// Write a message in the log for the current thread
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_DebugThreadWrite(char* pcMessage_)
{
   return DSIDebug::ThreadWrite(pcMessage_);
}

///////////////////////////////////////////////////////////////////////
// Reset the time in the debug logs
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANT_DebugResetTime()
{
   return DSIDebug::ResetTime();
}


///////////////////////////////////////////////////////////////////////
//
// ANT-FS Host
//
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Version info for the ANT-FS host library
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_GetVersion(ANTFSHostChannel* pclHost, UCHAR* pucVersion_, UCHAR ucBufferSize_)
{
   UCHAR aucLibVersion[255];  // Version of ANT-FS Library
   UCHAR ucSize;
   memset(aucLibVersion, 0, sizeof(aucLibVersion));

   // Get the ANT-FS Library Version
   ucSize = pclHost->GetVersion(aucLibVersion, sizeof(aucLibVersion));
   if(ucSize < ucBufferSize_)
      return STRNCPY((char*) pucVersion_, (char*) aucLibVersion, ucSize);
   else
      return STRNCPY((char*) pucVersion_, (char*) aucLibVersion, ucBufferSize_);
}

///////////////////////////////////////////////////////////////////////
// Creates the ANTFS Host object
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_New(ANTFSHostChannel** returnHostPtr)
{
   // Create ANT-FS Host object
   ANTFSHostChannel* pclHost = new ANTFSHostChannel();
   if(!pclHost)
      return(FALSE);

   *returnHostPtr = pclHost;

   return(TRUE);
}

///////////////////////////////////////////////////////////////////////
// Deletes the ANTFSHost object
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_Delete(ANTFSHostChannel* pclHost)
{
   delete pclHost;
   return(TRUE);
}

///////////////////////////////////////////////////////////////////////
// Initializes an ANT-FS host
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_Init(ANTFSHostChannel* pclHost, DSIFramerANT* pclANT_, UCHAR ucChannel_)
{
   // Initialize ANT-FS Host object
   return pclHost->Init(pclANT_, ucChannel_);
}

///////////////////////////////////////////////////////////////////////
// Obtains configurable ANT-FS host parameter values
///////////////////////////////////////////////////////////////////////
EXPORT void ANTFSHost_GetCurrentConfig(ANTFSHostChannel* pclHost, ANTFS_CONFIG_PARAMETERS* pCfg_)
{
   pclHost->GetCurrentConfig( pCfg_ );
}

///////////////////////////////////////////////////////////////////////
// Configures ANT-FS host parameter values
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_SetCurrentConfig(ANTFSHostChannel* pclHost, const ANTFS_CONFIG_PARAMETERS* pCfg_)
{
   return pclHost->SetCurrentConfig( pCfg_ );
}

///////////////////////////////////////////////////////////////////////
// Stops any pending actions, closes all devices down and cleans
// up any dynamic memory being used by the library.
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_Close(ANTFSHostChannel* pclHost)
{
   pclHost->Close();
   return(TRUE);
}

/////////////////////////////////////////////////////////////////
// Cancels any pending actions and returns the library to the
// appropriate ANTFS layer
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_Cancel(ANTFSHostChannel* pclHost)
{
   pclHost->Cancel();
   return(TRUE);
}

/////////////////////////////////////////////////////////////////
// Process device level notifications
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_ProcessDeviceNotification(ANTFSHostChannel* pclHost, ANT_DEVICE_NOTIFICATION eCode_, void* pvParameter_)
{
   pclHost->ProcessDeviceNotification(eCode_, pvParameter_);
   return(TRUE);
}

///////////////////////////////////////////////////////////////////////
// Called by the application to set the channel ID
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_SetChannelID(ANTFSHostChannel* pclHost, UCHAR ucDeviceType_, UCHAR ucTransmissionType_)
{
   pclHost->SetChannelID(ucDeviceType_, ucTransmissionType_);
   return(TRUE);
}

///////////////////////////////////////////////////////////////////////
// Called by the application to set the channel period
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_SetChannelPeriod(ANTFSHostChannel* pclHost, USHORT usChannelPeriod_)
{
   pclHost->SetChannelPeriod(usChannelPeriod_);
   return(TRUE);
}

///////////////////////////////////////////////////////////////////////
// Called by the application to set the network key for ANT-FS
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_SetNetworkKey(ANTFSHostChannel* pclHost, UCHAR ucNetwork_, UCHAR *pucKey)
{
   pclHost->SetNetworkKey(ucNetwork_, pucKey);
   return(TRUE);
}

/////////////////////////////////////////////////////////////////
// Sets the value for the proximity bin setting for searching
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_SetProximitySearch(ANTFSHostChannel* pclHost, UCHAR ucSearchThreshold_)
{
   pclHost->SetProximitySearch(ucSearchThreshold_);
   return(TRUE);
}


/////////////////////////////////////////////////////////////////
// Sets the serial number of the host
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_SetSerialNumber(ANTFSHostChannel* pclHost, ULONG ulSerialNumber_)
{
   pclHost->SetSerialNumber(ulSerialNumber_);
   return(TRUE);
}

/////////////////////////////////////////////////////////////////
// Enables ping message to be sent to the remote device periodically
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_EnablePing(ANTFSHostChannel* pclHost, BOOL bEnable_)
{
   return pclHost->EnablePing(bEnable_);
}

////////////////////////////////////////////////////////////////////////
// Process ANT messages
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_ProcessMessage(ANTFSHostChannel* pclHost, MARSHALL_ANT_MESSAGE* pstMessage_, USHORT usMesgSize_)
{
   ANT_MESSAGE stTempANTMessage;
   memcpy(&stTempANTMessage, pstMessage_, sizeof(ANT_MESSAGE));
   pclHost->ProcessMessage(&stTempANTMessage, usMesgSize_);
   return TRUE;
}

/////////////////////////////////////////////////////////////////
// Returns ANT channel number assigned
/////////////////////////////////////////////////////////////////
EXPORT UCHAR ANTFSHost_GetChannelNumber(ANTFSHostChannel* pclHost)
{
   return pclHost->GetChannelNumber();
}

/////////////////////////////////////////////////////////////////
// Returns TRUE if the ANT-FS engine is on
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_GetEnabled(ANTFSHostChannel* pclHost)
{
   return pclHost->GetEnabled();
}


/////////////////////////////////////////////////////////////////
// Returns the current host status
/////////////////////////////////////////////////////////////////
EXPORT ANTFS_HOST_STATE ANTFSHost_GetStatus(ANTFSHostChannel* pclHost)
{
   return pclHost->GetStatus();
}

/////////////////////////////////////////////////////////////////
// Returns the connected device beacon antfs state
/////////////////////////////////////////////////////////////////
EXPORT UCHAR ANTFSHost_GetConnectedDeviceBeaconAntfsState(ANTFSHostChannel* pclHost)
{
    return pclHost->GetConnectedDeviceBeaconAntfsState();
}

/////////////////////////////////////////////////////////////////
// Obtains the parameters of the most recently found device
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_GetFoundDeviceParameters(ANTFSHostChannel* pclHost, ANTFS_DEVICE_PARAMETERS* pstDeviceParameters_, UCHAR* aucFriendlyName_, UCHAR* pucBufferSize_)
{
   return pclHost->GetFoundDeviceParameters(pstDeviceParameters_, aucFriendlyName_, pucBufferSize_);
}

/////////////////////////////////////////////////////////////////
// Obtains the ANT channel ID of the most recently found device
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_GetFoundDeviceChannelID(ANTFSHostChannel* pclHost, USHORT *pusDeviceNumber_, UCHAR *pucDeviceType_, UCHAR *pucTransmitType_)
{
   return pclHost->GetFoundDeviceChannelID(pusDeviceNumber_, pucDeviceType_, pucTransmitType_);
}

/////////////////////////////////////////////////////////////////
// Gets the transfer progress of a pending or a complete upload
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_GetUploadStatus(ANTFSHostChannel* pclHost, ULONG *pulByteProgress_, ULONG *pulTotalLength_)
{
   return pclHost->GetUploadStatus(pulByteProgress_, pulTotalLength_);
}

/////////////////////////////////////////////////////////////////
// Gets the transfer progress of a pending or a complete download
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_GetDownloadStatus(ANTFSHostChannel* pclHost, ULONG *pulByteProgress_, ULONG *pulTotalLength_)
{
   return pclHost->GetDownloadStatus(pulByteProgress_, pulTotalLength_);
}

/////////////////////////////////////////////////////////////////
// Gets the received data from a transfer
// If *pvData is NULL, we will just retrieve the size
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_GetTransferData(ANTFSHostChannel* pclHost, ULONG *pulDataSize_, void *pvData_)
{
   return pclHost->GetTransferData(pulDataSize_, pvData_);
}

/////////////////////////////////////////////////////////////////
// Gets the size of the data available from the most recent download
/////////////////////////////////////////////////////////////////
EXPORT ULONG ANTFSHost_GetTransferSize(ANTFSHostChannel* pclHost)
{
   ULONG ulFileSize = 0;

   if(!pclHost->GetTransferData(&ulFileSize, NULL))
      ulFileSize = 0;

   return ulFileSize;
}

/////////////////////////////////////////////////////////////////
// Requests an ANT-FS session from a broadcast device we are
// currently connected to
/////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSHost_RequestSession(ANTFSHostChannel* pclHost, UCHAR ucBroadcastRadioFrequency_, UCHAR ucConnectRadioFrequency_)
{
   return pclHost->RequestSession(ucBroadcastRadioFrequency_, ucConnectRadioFrequency_);
}

/////////////////////////////////////////////////////////////////
// Begins a search for ANTFS remote devices.
/////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSHost_SearchForDevice(ANTFSHostChannel* pclHost, UCHAR ucSearchRadioFrequency_, UCHAR ucConnectRadioFrequency_, USHORT usRadioChannelID_, BOOL bUseRequestPage_)
{
   return pclHost->SearchForDevice(ucSearchRadioFrequency_, ucConnectRadioFrequency_, usRadioChannelID_, bUseRequestPage_);
}

/////////////////////////////////////////////////////////////////
// Request to pair with the connected remote device
/////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSHost_Authenticate(ANTFSHostChannel* pclHost, UCHAR ucAuthenticationType_, UCHAR* pucAuthString_, UCHAR ucLength_, UCHAR* pucResponseBuffer_, UCHAR* pucResponseBufferSize_, ULONG ulResponseTimeout_)
{
   return pclHost->Authenticate(ucAuthenticationType_, pucAuthString_, ucLength_, pucResponseBuffer_, pucResponseBufferSize_, ulResponseTimeout_);
}

/////////////////////////////////////////////////////////////////
// Request a download of a file from the authenticated device
/////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSHost_Download(ANTFSHostChannel* pclHost, USHORT usFileIndex_, ULONG ulDataOffset_, ULONG ulMaxDataLength_, ULONG ulMaxBlockSize_)
{
   return pclHost->Download(usFileIndex_, ulDataOffset_, ulMaxDataLength_, ulMaxBlockSize_);
}

/////////////////////////////////////////////////////////////////
// Request an upload of a file to the authenticated device
/////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSHost_Upload(ANTFSHostChannel* pclHost, USHORT usFileIndex_, ULONG ulDataOffset_, ULONG ulDataLength_, void *pvData_, BOOL bForceOffset_)
{
   return pclHost->Upload(usFileIndex_, ulDataOffset_, ulDataLength_, pvData_, bForceOffset_);
}


/////////////////////////////////////////////////////////////////
// Request the erasure of a file on the authenticated remote device
/////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSHost_EraseData(ANTFSHostChannel* pclHost, USHORT usFileIndex_)
{
   return pclHost->EraseData(usFileIndex_);
}

/////////////////////////////////////////////////////////////////
// Disconnect from a remote device.  Optionally put that device
// on a blackout list for a period of time. Go back to specified state.
/////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSHost_Disconnect(ANTFSHostChannel* pclHost, USHORT usBlackoutTime_, UCHAR ucDisconnectType_, UCHAR ucUndiscoverableDuration_, UCHAR ucAppSpecificDuration_)
{
   return pclHost->Disconnect(usBlackoutTime_, ucDisconnectType_, ucUndiscoverableDuration_, ucAppSpecificDuration_);
}

/////////////////////////////////////////////////////////////////
// Change radio frequency and channel period while in
// transport state
/////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSHost_SwitchFrequency(ANTFSHostChannel* pclHost, UCHAR ucRadioFrequency_, UCHAR ucChannelPeriod_)
{
   return pclHost->SwitchFrequency(ucRadioFrequency_, ucChannelPeriod_);
}

/////////////////////////////////////////////////////////////////
// Adds a set of parameters for which to search to the internal
// search device list.
// Returns a handle the the search device entry.  If the return
// value is NULL, the function failed adding the device entry.
/////////////////////////////////////////////////////////////////
EXPORT USHORT ANTFSHost_AddSearchDevice(ANTFSHostChannel* pclHost, ANTFS_DEVICE_PARAMETERS* pstDeviceSearchMask_, ANTFS_DEVICE_PARAMETERS* pstDeviceParameters_)
{
   return pclHost->AddSearchDevice(pstDeviceSearchMask_, pstDeviceParameters_);
}

/////////////////////////////////////////////////////////////////
// Removes a device entry from the internal search list.
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_RemoveSearchDevice(ANTFSHostChannel* pclHost, USHORT usHandle_)
{
   pclHost->RemoveSearchDevice(usHandle_);
   return(TRUE);
}

/////////////////////////////////////////////////////////////////
// Clears the internal search device list.
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_ClearSearchDeviceList(ANTFSHostChannel* pclHost)
{
   pclHost->ClearSearchDeviceList();
   return(TRUE);
}

/////////////////////////////////////////////////////////////////
// Puts the device on a blackout list for a period of time
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_Blackout(ANTFSHostChannel* pclHost, ULONG ulDeviceID_, USHORT usManufacturerID_, USHORT usDeviceType_, USHORT usBlackoutTime_)
{
   return pclHost->Blackout(ulDeviceID_, usManufacturerID_, usDeviceType_, usBlackoutTime_);
}

/////////////////////////////////////////////////////////////////
// Remove the device from the blackout list
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_RemoveBlackout(ANTFSHostChannel* pclHost, ULONG ulDeviceID_, USHORT usManufacturerID_, USHORT usDeviceType_)
{
   return pclHost->RemoveBlackout(ulDeviceID_, usManufacturerID_, usDeviceType_);
}

/////////////////////////////////////////////////////////////////
// Clears the blackout list
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSHost_ClearBlackoutList(ANTFSHostChannel* pclHost)
{
   pclHost->ClearBlackoutList();
   return TRUE;
}

/////////////////////////////////////////////////////////////////
// Wait for a response from the ANTFSHost object
/////////////////////////////////////////////////////////////////
EXPORT ANTFS_HOST_RESPONSE ANTFSHost_WaitForResponse(ANTFSHostChannel* pclHost, ULONG ulMilliseconds_)
{
   return pclHost->WaitForResponse(ulMilliseconds_);
}

///////////////////////////////////////////////////////////////////////
//
// ANT-FS Client
//
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// Version info for the ANT-FS client library
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_GetVersion(UCHAR* pucVersion_, UCHAR ucBufferSize_)
{
   char* aucLibVersion = getUnmanagedVersion();  // Version is the same as the unmanaged wrapper version
   return STRNCPY((char*) pucVersion_, aucLibVersion, ucBufferSize_);
}

///////////////////////////////////////////////////////////////////////
// Creates the ANTFS Client object
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_New(ANTFSClientChannel** returnClientPtr)
{
   // Create ANT-FS Client object
   ANTFSClientChannel* pclClient = new ANTFSClientChannel();
   if(!pclClient)
      return(FALSE);

   *returnClientPtr = pclClient;

   return(TRUE);
}

///////////////////////////////////////////////////////////////////////
// Deletes the ANTFSClient object
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_Delete(ANTFSClientChannel* pclClient)
{
   if(pclClient)
   {
      delete pclClient;
      return(TRUE);
   }
   return(FALSE);
}

///////////////////////////////////////////////////////////////////////
// Initializes an ANT-FS client device
///////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_Init(ANTFSClientChannel* pclClient, DSIFramerANT* pclANT_, UCHAR ucANTChannel_)
{
   // Initialize ANT-FS Client object
   return pclClient->Init(pclANT_, ucANTChannel_);
}


///////////////////////////////////////////////////////////////////////
// Stops any pending actions, closes all devices down and cleans
// up any dynamic memory being used by the library.
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_Close(ANTFSClientChannel* pclClient)
{
   //Close all communication and stop any pending actions
   pclClient->Close();
   return(TRUE);
}

/////////////////////////////////////////////////////////////////
// Cancels any pending actions and returns the library to the
// appropriate ANTFS layer
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_Cancel(ANTFSClientChannel* pclClient)
{
   pclClient->Cancel();
   return(TRUE);
}

/////////////////////////////////////////////////////////////////
// Process device level notifications
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_ProcessDeviceNotification(ANTFSClientChannel* pclClient, ANT_DEVICE_NOTIFICATION eCode_, void* pvParameter_)
{
   pclClient->ProcessDeviceNotification(eCode_, pvParameter_);
   return(TRUE);
}

////////////////////////////////////////////////////////////////////////
// Set up ANTFSClient configuration parameters
////////////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSClient_ConfigureClientParameters(ANTFSClientChannel* pclClient, ANTFS_CLIENT_PARAMS* pstInitParams_)
{
   return pclClient->ConfigureClientParameters(pstInitParams_);
}

////////////////////////////////////////////////////////////////////////
// Enable handling of pairing authentication requests
////////////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSClient_SetPairingEnabled(ANTFSClientChannel* pclClient, BOOL bEnable_)
{
   return pclClient->SetPairingEnabled(bEnable_);
}

////////////////////////////////////////////////////////////////////////
// Enable uploads
////////////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSClient_SetUploadEnabled(ANTFSClientChannel* pclClient, BOOL bEnable_)
{
   return pclClient->SetUploadEnabled(bEnable_);
}

////////////////////////////////////////////////////////////////////////
// Indicate whether data is available for download
////////////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSClient_SetDataAvailable(ANTFSClientChannel* pclClient, BOOL bDataAvailable_)
{
   return pclClient->SetDataAvailable(bDataAvailable_);
}

////////////////////////////////////////////////////////////////////////
// Set up the time the client will wait without receiving any
// commands from the host before dropping back to the link state
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_SetBeaconTimeout(ANTFSClientChannel* pclClient, UCHAR ucTimeout_)
{
   pclClient->SetBeaconTimeout(ucTimeout_);
   return(TRUE);
}

////////////////////////////////////////////////////////////////////////
// Set up the time the client will wait without receiving any
// response from the application to a pairing request before rejecting it
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_SetPairingTimeout(ANTFSClientChannel* pclClient, UCHAR ucTimeout_)
{
   pclClient->SetPairingTimeout(ucTimeout_);
   return(TRUE);
}

////////////////////////////////////////////////////////////////////////
// Set up a friendly name for the ANT-FS client
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_SetFriendlyName(ANTFSClientChannel* pclClient, UCHAR* pucFriendlyName_, UCHAR ucFriendlyNameSize_)
{
   pclClient->SetFriendlyName(pucFriendlyName_, ucFriendlyNameSize_);
   return(TRUE);
}

////////////////////////////////////////////////////////////////////////
// Set up the pass key for the client to establish authenticated
// sessions with a host device
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_SetPassKey(ANTFSClientChannel* pclClient, UCHAR* pucPassKey_, UCHAR ucPassKeySize_)
{
   pclClient->SetPassKey(pucPassKey_, ucPassKeySize_);
   return(TRUE);
}

////////////////////////////////////////////////////////////////////////
// Set up the ANT Channel ID for the ANT-FS Client
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_SetChannelID(ANTFSClientChannel* pclClient, UCHAR ucDeviceType_, UCHAR ucTransmissionType_)
{
   pclClient->SetChannelID(ucDeviceType_, ucTransmissionType_);
   return(TRUE);
}

////////////////////////////////////////////////////////////////////////
// Set up a custom ANT Channel Period for the ANT-FS Client
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_SetChannelPeriod(ANTFSClientChannel* pclClient, USHORT usChannelPeriod_)
{
   pclClient->SetChannelPeriod(usChannelPeriod_);
   return(TRUE);
}

////////////////////////////////////////////////////////////////////////
// Set up the network key to use with the ANT-FS Client
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_SetNetworkKey(ANTFSClientChannel* pclClient, UCHAR ucNetwork_, UCHAR* pucNetworkKey)
{
   pclClient->SetNetworkKey(ucNetwork_, pucNetworkKey);
   return(TRUE);
}

////////////////////////////////////////////////////////////////////////
// Set up the transmit power for the ANT-FS Client Channel.
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_SetTxPower(ANTFSClientChannel* pclClient, UCHAR ucPairingLv_, UCHAR ucConnectedLv_)
{
   pclClient->SetTxPower(ucPairingLv_, ucConnectedLv_);
   return(TRUE);
}

////////////////////////////////////////////////////////////////////////
// Begins the channel configuration to transmit the ANT-FS beacon
////////////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSClient_OpenBeacon(ANTFSClientChannel* pclClient)
{
   return pclClient->OpenBeacon();
}

////////////////////////////////////////////////////////////////////////
// Begins closing the ANT-FS beacon channel. Returns to specified state.
////////////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSClient_CloseBeacon(ANTFSClientChannel* pclClient, BOOL bReturnToBroadcast_)
{
   return pclClient->CloseBeacon(bReturnToBroadcast_);
}

////////////////////////////////////////////////////////////////////////
// Process ANT messages
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_ProcessMessage(ANTFSClientChannel* pclClient, MARSHALL_ANT_MESSAGE* pstMessage_, USHORT usMesgSize_)
{
   ANT_MESSAGE stTempANTMessage;
   memcpy(&stTempANTMessage, pstMessage_, sizeof(ANT_MESSAGE));
   pclClient->ProcessMessage(&stTempANTMessage, usMesgSize_);
   return TRUE;
}

/////////////////////////////////////////////////////////////////
// Returns TRUE if the ANT-FS engine is on
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_GetEnabled(ANTFSClientChannel* pclClient)
{
   return pclClient->GetEnabled();
}

/////////////////////////////////////////////////////////////////
// Returns ANT channel number assigned
/////////////////////////////////////////////////////////////////
EXPORT UCHAR ANTFSClient_GetChannelNumber(ANTFSClientChannel* pclClient)
{
   return pclClient->GetChannelNumber();
}

////////////////////////////////////////////////////////////////////////
// Returns the current client status.
////////////////////////////////////////////////////////////////////////
EXPORT ANTFS_CLIENT_STATE ANTFSClient_GetStatus(ANTFSClientChannel* pclClient)
{
   return pclClient->GetStatus();
}

////////////////////////////////////////////////////////////////////////
// Copies at most ucBufferSize_ characters from the host's
// friendly name string (for the most recent session) into the
// supplied pucHostFriendlyName_ buffer.
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_GetHostName(ANTFSClientChannel* pclClient, UCHAR *aucHostFriendlyName_, UCHAR *pucBufferSize_)
{
   return pclClient->GetHostName(aucHostFriendlyName_, pucBufferSize_);
}

////////////////////////////////////////////////////////////////////////
// Gets the full parameters for a download, upload or erase request
// received from the host
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_GetRequestParameters(ANTFSClientChannel* pclClient, ANTFS_REQUEST_PARAMS* pstRequestParams_)
{
   return pclClient->GetRequestParameters(pstRequestParams_);
}

////////////////////////////////////////////////////////////////////////
// Gets the requested file index for a download, upload or erase request
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_GetRequestedFileIndex(ANTFSClientChannel* pclClient, USHORT* pusIndex_)
{
   return pclClient->GetRequestedFileIndex(pusIndex_);
}


////////////////////////////////////////////////////////////////////////
// Gets the current status of an ongoing download
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_GetDownloadStatus(ANTFSClientChannel* pclClient, ULONG *pulByteProgress_, ULONG *pulTotalLength_)
{
   return pclClient->GetDownloadStatus(pulByteProgress_, pulTotalLength_);
}

/////////////////////////////////////////////////////////////////
// Gets the size of the data available from the most recent upload
/////////////////////////////////////////////////////////////////
EXPORT ULONG ANTFSClient_GetTransferSize(ANTFSClientChannel* pclClient)
{
   ULONG ulFileSize = 0;

   if(!pclClient->GetTransferData(&ulFileSize, NULL))
      ulFileSize = 0;

   return ulFileSize;
}

////////////////////////////////////////////////////////////////////////
// Gets the received data from a transfer (upload).
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_GetTransferData(ANTFSClientChannel* pclClient, ULONG *pulOffset_, ULONG *pulDataSize_ , void *pvData_)
{
   return pclClient->GetTransferData(pulOffset_, pulDataSize_, pvData_);
}

////////////////////////////////////////////////////////////////////////
// Gets the full parameters for a disconnect request
////////////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSClient_GetDisconnectParameters(ANTFSClientChannel* pclClient, ANTFS_DISCONNECT_PARAMS* pstDisconnectParams_)
{
   return pclClient->GetDisconnectParameters(pstDisconnectParams_);
}

////////////////////////////////////////////////////////////////////////
// Sends a response to a pairing request.
////////////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSClient_SendPairingResponse(ANTFSClientChannel* pclClient, BOOL bAccept_)
{
   return pclClient->SendPairingResponse(bAccept_);
}

////////////////////////////////////////////////////////////////////////
// Sends the response to a download request from an authenticated device
////////////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSClient_SendDownloadResponse(ANTFSClientChannel* pclClient, UCHAR ucResponse_, ANTFS_DOWNLOAD_PARAMS* pstDownloadInfo_, ULONG ulDataLength_, void *pvData_)
{
   return pclClient->SendDownloadResponse(ucResponse_, pstDownloadInfo_, ulDataLength_, pvData_);
}

////////////////////////////////////////////////////////////////////////
// Sends the response to an upload request from an authenticated device
// TODO: Not implemented yet!!!
////////////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSClient_SendUploadResponse(ANTFSClientChannel* pclClient, UCHAR ucResponse_, ANTFS_UPLOAD_PARAMS* pstUploadInfo_, ULONG ulDataLength_, void *pvData_)
{
   return pclClient->SendUploadResponse(ucResponse_, pstUploadInfo_, ulDataLength_, pvData_);
}

////////////////////////////////////////////////////////////////////////
// Sends a response to a request to erase a file from an
// authenticated remote device
////////////////////////////////////////////////////////////////////////
EXPORT ANTFS_RETURN ANTFSClient_SendEraseResponse(ANTFSClientChannel* pclClient, UCHAR ucResponse_)
{
   return pclClient->SendEraseResponse(ucResponse_);
}

////////////////////////////////////////////////////////////////////////
// Wait for a response from the ANTFSClient object.
////////////////////////////////////////////////////////////////////////
EXPORT ANTFS_CLIENT_RESPONSE ANTFSClient_WaitForResponse(ANTFSClientChannel* pclClient, ULONG ulMilliseconds_)
{
   return pclClient->WaitForResponse(ulMilliseconds_);
}

///////////////////////////////////////////////////////////////////////
//
// ANT-FS Directory
//
///////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
// Returns the number of file entries contained in the directory
/////////////////////////////////////////////////////////////////
EXPORT ULONG ANTFSDirectory_GetNumberOfFileEntries(void *pvDirectory_, ULONG ulDirectoryFileLength_)
{
   return ANTFSDir_GetNumberOfFileEntries(pvDirectory_, ulDirectoryFileLength_);
}

/////////////////////////////////////////////////////////////////
// Fills in the directory structure with information from the
// directory file
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSDirectory_LookupFileEntry(void *pvDirectory_, ULONG ulDirectoryFileLength_, ULONG ulFileEntry_, ANTFSP_DIRECTORY *pusDirectoryStruct_)
{
   return ANTFSDir_LookupFileEntry(pvDirectory_, ulDirectoryFileLength_, ulFileEntry_, pusDirectoryStruct_);
}

/////////////////////////////////////////////////////////////////
// Decodes the directory and generates a list of files that needs
// that needs to be downloaded.
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSDirectory_GetNewFileList(void *pvDirectory_, ULONG ulDirectoryFileLength_, USHORT *pusFileIndexList, USHORT * pusListLength)
{
   return ANTFSDir_GetNewFileList(pvDirectory_, ulDirectoryFileLength_, pusFileIndexList, pusListLength);
}

/////////////////////////////////////////////////////////////////
// Fills in the directory structure with information from the
// directory file
/////////////////////////////////////////////////////////////////
EXPORT BOOL ANTFSDirectory_LookupFileIndex(void *pvDirectory_, ULONG ulDirectoryFileLength_, USHORT usFileIndex_, ANTFSP_DIRECTORY *pusDirectoryStruct_)
{
   return ANTFSDir_LookupFileIndex(pvDirectory_, ulDirectoryFileLength_, usFileIndex_, pusDirectoryStruct_);
}

