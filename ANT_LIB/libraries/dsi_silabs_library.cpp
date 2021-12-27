/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/
#include "types.h"
#if defined(DSI_TYPES_WINDOWS)

#include "dsi_silabs_library.hpp"

#include <memory>

using namespace std;

#define LIBRARY_NAME    "DSI_SiUSBXp_3_1.DLL"

                                                                     //return an auto_ptr?
SiLabsLibrary *SiLabsLibrary::Load()  //!!Should we lose the dependency on auto_ptr and just return the pointer (and let the user make their own auto_ptr)?
{
   HMODULE hLibHandle_ = LoadLibrary(LIBRARY_NAME);
   if (hLibHandle_ == NULL)
       return NULL;

   static std::auto_ptr<SiLabsLibrary> clAutoInstance(new SiLabsLibrary(hLibHandle_));
   if(clAutoInstance->Open == NULL) try {
        SiLabsError::Enum ret = clAutoInstance->LoadFunctions();
        if (ret != SiLabsError::NONE)
            throw ret;
    } catch (...) {
        clAutoInstance.reset(NULL);
    }

   return clAutoInstance.get();
}


SiLabsLibrary::SiLabsLibrary(HMODULE hLibHandle_) //throw(SiLabsError::Enum&)
:
   Open(NULL),
   GetNumDevices(NULL),
   Close(NULL),
   Read(NULL),
   Write(NULL),
   SetTimeouts(NULL),
   GetTimeouts(NULL),
   FlushBuffers(NULL),
   CheckRxQueue(NULL),
   SetBaudRate(NULL),
   SetLineControl(NULL),
   SetFlowControl(NULL),
   GetProductString(NULL),
   hLibHandle(hLibHandle_)
{
}

SiLabsLibrary::~SiLabsLibrary() throw()
{
   FreeFunctions();
}


///////////////////////////////////////////////////////////////////////
// Loads USB interface functions from the DLLs.
///////////////////////////////////////////////////////////////////////
SiLabsError::Enum SiLabsLibrary::LoadFunctions()
{
   if(hLibHandle == NULL)
      return SiLabsError::NO_LIBRARY;

   BOOL bStatus = TRUE;

   Open = (Open_t)GetProcAddress(hLibHandle, "SI_Open");
   if(Open == NULL)
      bStatus = FALSE;

   GetNumDevices = (GetNumDevices_t)GetProcAddress(hLibHandle, "SI_GetNumDevices");
   if(GetNumDevices == NULL)
      bStatus = FALSE;

   Close = (Close_t)GetProcAddress(hLibHandle, "SI_Close");
   if(Close == NULL)
      bStatus = FALSE;

   Read = (Read_t)GetProcAddress(hLibHandle, "SI_Read");
   if(Read == NULL)
      bStatus = FALSE;

   Write = (Write_t)GetProcAddress(hLibHandle, "SI_Write");
   if(Write == NULL)
      bStatus = FALSE;

   SetTimeouts = (SetTimeouts_t)GetProcAddress(hLibHandle, "SI_SetTimeouts");
   if(SetTimeouts == NULL)
      bStatus = FALSE;

   GetTimeouts = (GetTimeouts_t)GetProcAddress(hLibHandle, "SI_GetTimeouts");
   if(GetTimeouts == NULL)
      bStatus = FALSE;

   SetBaudRate = (SetBaudRate_t)GetProcAddress(hLibHandle, "SI_SetBaudRate");
   if(SetBaudRate == NULL)
      bStatus = FALSE;

   SetLineControl = (SetLineControl_t)GetProcAddress(hLibHandle, "SI_SetLineControl");
   if(SetLineControl == NULL)
      bStatus = FALSE;

   SetFlowControl = (SetFlowControl_t)GetProcAddress(hLibHandle, "SI_SetFlowControl");
   if(SetFlowControl == NULL)
      bStatus = FALSE;

   FlushBuffers = (FlushBuffers_t)GetProcAddress(hLibHandle, "SI_FlushBuffers");
   if(FlushBuffers == NULL)
      bStatus = FALSE;

   CheckRxQueue = (CheckRxQueue_t)GetProcAddress(hLibHandle, "SI_CheckRXQueue");
   if(CheckRxQueue == NULL)
      bStatus = FALSE;

   GetProductString = (GetProductString_t)GetProcAddress(hLibHandle, "SI_GetProductString");
   if(GetProductString == NULL)
      bStatus = FALSE;


   if(bStatus == FALSE)
   {
      FreeFunctions();
      return SiLabsError::NO_FUNCTION;
   }

   return SiLabsError::NONE;
}

///////////////////////////////////////////////////////////////////////
// Unloads USB DLLs.
///////////////////////////////////////////////////////////////////////
void SiLabsLibrary::FreeFunctions()
{
   if(hLibHandle != NULL)
   {
      FreeLibrary(hLibHandle);
      hLibHandle = NULL;
   }

   return;
}

#endif //defined(DSI_TYPES_WINDOWS)