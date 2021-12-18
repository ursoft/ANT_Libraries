/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using System.Management;

namespace ANT_Managed_Library
{
    /// <summary>
    /// Control class for a given ANT device. An instance of this class is an open connection to the given ANT USB device.
    /// Handles creating channels and device setup.
    /// </summary>
    public class ANT_Device : IDisposable
    {
        #region variables

        ANT_ReferenceLibrary.FramerType frameType;
        ANT_ReferenceLibrary.PortType portType;

        IntPtr unmanagedANTSerialPtr = IntPtr.Zero;
        IntPtr unmanagedANTFramerPtr = IntPtr.Zero;
        IntPtr unmanagedCancelBool = IntPtr.Zero;   //The bool type def in the unmanaged code is an int, so 4 bytes
        bool? cancelTxFlag //Handles accessing the unmanged bool as a normal nullable bool
        {
            get
            {
                if(unmanagedCancelBool == IntPtr.Zero)
                    return null;
                return Convert.ToBoolean(Marshal.ReadInt32(unmanagedCancelBool));
            }
            set //TODO: Should we send out a deviceNotification when this occurs?
            {
                if (unmanagedCancelBool != IntPtr.Zero)
                    Marshal.WriteInt32(unmanagedCancelBool, Convert.ToInt32(value));
            }
        }

        bool initializedUSB = false;
        byte USBDeviceNum = 255;    //Device num of instance
        uint USBBaudRate = 0;

        ANT_DeviceCapabilities capabilities = null;
        bool pollingOn;

        int numDeviceChannels;
        ANT_Channel[] antChannels;


        #endregion


        #region DeviceCallback variables

        //This struct is used by the unmanaged code to pass response messages
        [StructLayout(LayoutKind.Sequential)]
        struct ANTMessageItem
        {
            public byte dataSize;
            public ANTMessage antMsgData;
        }

        /// <summary>
        /// ANTMessage struct as defined in unmanaged code for marshalling ant messages with unmanaged code
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        public struct ANTMessage
        {
            /// <summary>
            /// Message ID byte
            /// </summary>
            public byte msgID;

            /// <summary>
            /// Data buffer
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = ANT_ReferenceLibrary.MAX_MESG_SIZE)] //We have to make sure the marshalling knows the unmanaged equivalent is a fixed array of the maximum ANT message size
            public byte[] ucharBuf; //The size of this should match the SizeConst
        }

        /// <summary>
        /// Codes for the device notification event
        /// </summary>
        public enum DeviceNotificationCode : byte
        {
            /// <summary>
            /// The device is being reset
            /// </summary>
            Reset = 0x01,

            /// <summary>
            /// The device is being shutdown
            /// </summary>
            Shutdown = 0x02,
        }

        System.Threading.Thread responsePoller; //This thread is in charge of monitoring the device for new messages and forwards them to the appropriate handler function

        /// <summary>
        /// Delegate for device response event
        /// </summary>
        /// <param name="response">Message details received from device</param>
        public delegate void dDeviceResponseHandler(ANT_Response response); //The delegate for the managed callback functions

        /// <summary>
        /// The channel callback event. Triggered every time a message is received from the ANT device.
        /// Examples include requested responses and setup status messages.
        /// </summary>
        public event dDeviceResponseHandler deviceResponse;  //The event to assign callback functions to in a managed application

        /// <summary>
        /// Function to handle ANT_Device serial errors
        /// </summary>
        /// <param name="sender">The ANT_Device reporting the error</param>
        /// <param name="error">The serial error that occured</param>
        /// <param name="isCritical">If true, the communication with the device is lost and this device object should be disposed</param>
        public delegate void dSerialErrorHandler(ANT_Device sender, serialErrorCode error, bool isCritical); //The delegate for the serialError callback function

        /// <summary>
        /// This event is triggered when there is a serial communication error with the ANT Device.
        /// If the error is critical all communication with the device is dead and the
        /// device reference is sent in this function to allow the application
        /// to know which device is dead and to dispose of it.
        /// </summary>
        public event dSerialErrorHandler serialError;

        /// <summary>
        /// ANT Device Serial Error Codes
        /// </summary>
        public enum serialErrorCode
        {
            /// <summary>
            /// A write command to the device failed, could be a usb communication issue or due to invalid paramters passed to the write function.
            /// If it is a device communication failure, a serial read error will most likely occur shortly.
            /// </summary>
            SerialWriteError,

            /// <summary>
            /// A failure occured reading data from the device.
            /// </summary>
            SerialReadError,

            /// <summary>
            /// Communication with the device has been lost.
            /// </summary>
            DeviceConnectionLost,

            /// <summary>
            /// A message received by the device failed the crc check and was discarded.
            /// </summary>
            MessageLost_CrcError,

            /// <summary>
            /// The message queue for received messages has overflowed and one or more messages were lost.
            /// </summary>
            MessageLost_QueueOverflow,

            /// <summary>
            /// A message received was over the maximum message size, and the message was discarded.
            /// This is usually due to a communication error or an error in the library or library compilation settings.
            /// </summary>
            MessageLost_TooLarge,

            /// <summary>
            /// A channel event was received for a channel which does not exist and the message was dropped (but will still appear in the logs)
            /// </summary>
            MessageLost_InvalidChannel,

            /// <summary>
            /// Unspecified error, most likely a new or changed error definition
            /// </summary>
            Unknown
        }

        #endregion



        #region ANT_DLL Imports

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_Init(byte ucUSBDeviceNum, UInt32 usBaudrate, ref IntPtr returnSerialPtr, ref IntPtr returnFramerPtr, byte ucPortType, byte ucHCIFrameTpye);

        //Note: Before uncommenting this read todo comment on constructor, we don't use this now because we want to know the device number we opened
        //[DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        //private static extern int ANT_AutoInit(ref IntPtr returnSerialPtr, ref IntPtr returnFramerPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern void ANT_Close(IntPtr SerialPtr, IntPtr FramerPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_USBReset(IntPtr SerialPtr);


        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_ResetSystem(IntPtr FramerPtr, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern void ANT_SetCancelParameter(IntPtr FramerPtr, IntPtr pbCancel);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SetNetworkKey_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_SetNetworkKey(IntPtr FramerPtr, byte ucNetNumber, byte[] pucKey, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_EnableLED_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_EnableLED(IntPtr FramerPtr, byte ucEnable, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_GetDeviceUSBPID(IntPtr FramerPtr, ref UInt16 usbPID);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_GetDeviceUSBVID(IntPtr FramerPtr, ref UInt16 usbVID);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_ConfigureSplitAdvancedBursts(IntPtr FramerPtr, int bEnabelSplitBursts);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_ConfigureAdvancedBurst_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_ConfigureAdvancedBurst(IntPtr FramerPtr, int enable, byte ucMaxPacketLength, UInt32 ulRequiredFields, UInt32 ulOptionalFields, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_ConfigureAdvancedBurst_ext_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_ConfigureAdvancedBurst_ext(IntPtr FramerPtr, int enable, byte ucMaxPacketLength, UInt32 ulRequiredFields, UInt32 ulOptionalFields, UInt16 usStallCount, byte ucRetryCount, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SetCryptoKey_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_SetCryptoKey(IntPtr FramerPtr, byte ucVolatileKeyIndex, byte[] pucKey, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SetCryptoID_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_SetCryptoID(IntPtr FramerPtr, byte[] pucData, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SetCryptoUserInfo_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_SetCryptoUserInfo(IntPtr FramerPtr, byte[] pucData, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SetCryptoRNGSeed_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_SetCryptoRNGSeed(IntPtr FramerPtr, byte[] pucData, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SetCryptoInfo_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_SetCryptoInfo(IntPtr FramerPtr, byte ucParameter, byte[] pucData, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_LoadCryptoKeyNVMOp_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_LoadCryptoKeyNVMOp(IntPtr FramerPtr, byte ucNVMKeyIndex, byte ucVolatileKeyIndex, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_StoreCryptoKeyNVMOp_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_StoreCryptoKeyNVMOp(IntPtr FramerPtr, byte ucNVMKeyIndex, byte[] pucKey, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_CryptoKeyNVMOp_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_CryptoKeyNVMOp(IntPtr FramerPtr, byte ucOperation, byte ucNVMKeyIndex, byte[] pucData, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern uint ANT_GetDeviceSerialNumber(IntPtr SerialPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_GetDeviceUSBInfo(IntPtr FramerPtr, byte ucDeviceNum_,
            [MarshalAs(UnmanagedType.LPArray)] byte[] pucProductString_,
            [MarshalAs(UnmanagedType.LPArray)] byte[] pucSerialString_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SetTransmitPower_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_SetTransmitPower(IntPtr FramerPtr, byte ucTransmitPower_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_ConfigEventFilter(IntPtr FramerPtr, UInt16 usFilter_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_ConfigEventBuffer(IntPtr FramerPtr, byte ucConfig_, UInt16 usSize_, UInt16 usTime_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_ConfigHighDutySearch(IntPtr FramerPtr, byte ucEnable_, byte ucSuppressionCycles_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_SetSelectiveDataUpdateMask(IntPtr FramerPtr, byte ucMaskNumber_, byte[] ucSduMask_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_ConfigUserNVM(IntPtr FramerPtr, UInt16 usAddress_, byte[] pucData_, byte ucSize_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_RequestMessage(IntPtr FramerPtr, byte ucANTChannel, byte ucMessageID, ref ANTMessageItem ANT_MESSAGE_ITEM_response, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_RequestUserNvmMessage(IntPtr FramerPtr, byte ucANTChannel, byte ucMessageID, ref ANTMessageItem ANT_MESSAGE_ITEM_response, UInt16 usAddress, byte ucSize, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern UInt16 ANT_WaitForMessage(IntPtr FramerPtr, UInt32 ulMilliseconds_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern UInt16 ANT_GetMessage(IntPtr FramerPtr, ref ANTMessage ANT_MESSAGE_response);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANT_GetChannelNumber(IntPtr FramerPtr, ref ANTMessage pstANTMessage);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_GetCapabilities(IntPtr FramerPtr,
           [MarshalAs(UnmanagedType.LPArray)] byte[] pucCapabilities_,
           UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_InitCWTestMode_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_InitCWTestMode(IntPtr FramerPtr, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SetCWTestMode_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_SetCWTestMode(IntPtr FramerPtr, byte ucTransmitPower_, byte ucRFChannel_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_OpenRxScanMode_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_OpenRxScanMode(IntPtr FramerPtr, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_Script_Write_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_Script_Write(IntPtr FramerPtr, byte ucSize_, byte[] pucData_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_Script_Clear_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_Script_Clear(IntPtr FramerPtr, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_Script_SetDefaultSector_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_Script_SetDefaultSector(IntPtr FramerPtr, byte ucSectNumber_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_Script_EndSector_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_Script_EndSector(IntPtr FramerPtr, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_Script_Dump_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_Script_Dump(IntPtr FramerPtr, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_Script_Lock_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_Script_Lock(IntPtr FramerPtr, UInt32 ulResponseTimeout_);


        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_RxExtMesgsEnable_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_RxExtMesgsEnable(IntPtr FramerPtr, byte ucEnable_, UInt32 ulResponseTimeout_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_CrystalEnable(IntPtr FramerPtr, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_WriteMessage(IntPtr FramerPtr, ANTMessage pstANTMessage, UInt16 usMessageSize);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_SetLibConfig(IntPtr FramerPtr, byte ucLibConfigFlags_, UInt32 ulResponseTime_);


        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "FIT_SetFEState_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int FIT_SetFEState(IntPtr FramerPtr, byte ucFEState_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "FIT_AdjustPairingSettings_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int FIT_AdjustPairingSettings(IntPtr FramerPtr, byte ucSearchLv_, byte ucPairLv_, byte ucTrackLv_, UInt32 ulResponseTime_);
        #endregion



        #region Constructors and Destructor

        /// <overloads>
        /// Opens a connection to an ANT device attached by USB.
        /// Throws exception if a connection can not be established.
        /// </overloads>
        /// <summary>
        /// Attempts to open a connection to an ANT device attached by USB using the given deviceNum and baud rate
        /// Throws exception if a connection can not be established.
        /// </summary>
        /// <param name="USBDeviceNum">The device number of the ANT USB device (the first connected device starts at 0 and so on)</param>
        /// <param name="baudRate">The baud rate to connect at (AP2/AT3=57600, AP1=50000)</param>
        public ANT_Device(byte USBDeviceNum, uint baudRate)
            :this(ANT_ReferenceLibrary.PortType.USB, USBDeviceNum, baudRate, ANT_ReferenceLibrary.FramerType.basicANT)
        {
        }


        /// <overloads>
        /// Opens a connection to an ANT device attached by USB.
        /// Throws exception if a connection can not be established.
        /// </overloads>
        /// <summary>
        /// Attempts to open a connection to an ANT device attached by USB using the given deviceNum and baud rate
        /// Throws exception if a connection can not be established.
        /// </summary>
        /// <param name="portType">The type of connection to use when talking to the device</param>
        /// <param name="USBDeviceNum">If port type is USB, device number of the ANT USB device.
        /// If port type is COM this is the COM port number</param>
        /// <param name="baudRate">The baud rate to connect at (USB: AP2/AT3=57600, AP1=50000)</param>
        /// <param name="frameType">The framing method to use for the connection to the chip.
        /// Needed for multimode chips and currently only supported on COM connections.</param>
        public ANT_Device(ANT_ReferenceLibrary.PortType portType, byte USBDeviceNum, uint baudRate, ANT_ReferenceLibrary.FramerType frameType)
        {
            try
            {
                ANT_Common.checkUnmanagedLibrary(); // First check if the unmanaged library is there, otherwise, we cannot connect to ANT anyway
                startUp(USBDeviceNum, baudRate, frameType, portType, false);  // Let exceptions propagate
            }
            catch
            {
                // If constructor throws an exception, release acquired resources and suppress finalization
                this.Dispose();
                throw;
            }
        }


        /// <summary>
        /// Automatically connects to first availiable ANTDevice.
        /// Throws exception if a connection can not be established.
        /// Will not auto find COM-connected devices.
        /// </summary>
        public ANT_Device() //TODO: Need to change this to use the c++ library auto open, so it will check device strings, etc. NOT as now where it could open any SI device...the problem is that currently if the autoinit is used the device number is not accessible which we want to keep for ANTWare II
        {
            ANT_Common.checkUnmanagedLibrary(); // First check if the unmanaged library is there, otherwise, we cannot connect to ANT anyway

            ulong numDevices = ANT_Common.getNumDetectedUSBDevices();
            if (numDevices == 0)
            {
                String detail = ": ensure an ANT device is connected to your system and try again";
                try
                {
                    // If we cannot see any devices, it might be because of missing dependencies
                    ANT_Common.checkUSBLibraries();
                }
                catch(ANT_Exception ex)
                {
                    detail = ": " +  ex.Message.Remove(0,21);
                }
                throw new ANT_Exception("No ANT devices detected" + detail);
            }

            bool fail = true;
            for (byte i = 0; i < numDevices && fail; ++i)
            {
                try
                {
                    //Try 57600 baud first
                    startUp(i, 57600, ANT_ReferenceLibrary.FramerType.basicANT, ANT_ReferenceLibrary.PortType.USB, true);  //If this fails it will throw an exception, and we'll try 50000
                    fail = false;   //If no exception is thrown we are good to go
                }
                catch (Exception)
                {
                    //Try 50000 baudRate then
                    try
                    {
                        startUp(i, 50000, ANT_ReferenceLibrary.FramerType.basicANT, ANT_ReferenceLibrary.PortType.USB, true);
                        fail = false;   //If no exception is thrown we are good to go
                    }
                    catch (Exception)
                    {
                        fail = true;    //Both baud rates failed
                    }
                }
            }

            if (fail)
            {
                // If constructor throws an exception, release acquired resources and suppress finalization
                this.Dispose();
                throw new ANT_Exception("Failed to connect to any ANT devices");
            }
        }


        //This is all the initialization, pulled out to this method so it could be called by both constructors
        private void startUp(byte USBDeviceNum, uint baudRate, ANT_ReferenceLibrary.FramerType frameType, ANT_ReferenceLibrary.PortType portType, bool calledByAutoInit)
        {
            // The SI libraries are not required for USB2, so attempt to connect first and only throw an
            // exception if this fails.
            int ret = ANT_Init(USBDeviceNum, baudRate, ref unmanagedANTSerialPtr, ref unmanagedANTFramerPtr, (byte)portType, (byte)frameType);
            if (ret != 0)
            {
                switch(ret)
                {
                    case -1:
                        throw new ANT_Exception("Unexpected init library error. This is typically a problem with the c++ library");
                    case -2:
                        throw new ANT_Exception("Unrecognized type parameters");
                    case -3:
                        if (!calledByAutoInit)  //Auto-init checks only after checking all devices
                            ANT_Common.checkUSBLibraries(); // throws exception if libraries are missing
                        throw new ANT_Exception("Unable to initialize USB:" + USBDeviceNum + " at Baud:" + baudRate + ", probably device not present or already in use, or drivers not installed");
                    default:
                        throw new ANT_Exception("Unrecognized error code received from c++ library");
                }
            }

            initializedUSB = true;  //Keep track of whether device is connected or not
            this.USBDeviceNum = USBDeviceNum;   //Keep the parameters for reference
            this.USBBaudRate = baudRate;
            this.frameType = frameType;
            this.portType = portType;

#if (ANTFS_DEBUGGING)
            //This logging is used in the ANT-FS libraries.  Will not create any files if logging not enabled
            ANT_Common.initDebugLogThread("Device" + USBDeviceNum + "_Application");
            ANT_Common.ANT_DebugResetTime();
            ANT_Common.writeToDebugLog("ANT_NET.DLL " + ANT_VersionInfo.getManagedLibraryVersion() + " with ANT_WrappedLib.DLL " + ANT_VersionInfo.getUnmanagedLibraryVersion() + " - current unix time: " + (DateTime.UtcNow - new DateTime(1970, 1, 1, 0, 0, 0)).TotalSeconds);
#endif

            try
            {
                responsePoller = new System.Threading.Thread(responsePollFunc);
                responsePoller.Name = this.ToString() + " Receive Thread";
                responsePoller.IsBackground = true; //Make this a background thread so it will terminate when a program closes
                responsePoller.Start();

                //Get capabilities so we know how many channels we have
                //If this throws an exception it is probably connected at the wrong baud rate, so we send a descriptive exception
                try
                {
                    getDeviceCapabilities(true, 200);
                }
                catch (ANT_Exception ex)
                {
                    throw new ANT_Exception(ex.Message.Remove(0, 22) + ", probably connecting at wrong baud rate");
                }

                if(ANT_Common.autoResetIsEnabled)
                    ANT_ResetSystem(unmanagedANTFramerPtr, 200); //Send a reset so by the time the user gets this handle the device is viable

                numDeviceChannels = capabilities.maxANTChannels;

                // Set up the channel list to the right size
                antChannels = new ANT_Channel[numDeviceChannels];

                unmanagedCancelBool = Marshal.AllocHGlobal(4);
                cancelTxFlag = false;
                ANT_SetCancelParameter(unmanagedANTFramerPtr, unmanagedCancelBool);
            }
            catch (Exception)    // clean up
            {
                shutdown();
                throw;   //Forward the error to caller
            }
        }

        /// <summary>
        /// Destructor closes all opened resources
        /// </summary>
        ~ANT_Device()
        {
            shutdown();
        }

        /// <summary>
        /// Dispose method for explicit resource cleanup. Same as shutdownDeviceInstance() but doesn't nullify reference.
        /// </summary>
        public void Dispose()
        {
            shutdown();
            GC.SuppressFinalize(this);
        }


        //This function cleans up all opened resources
        //This is accessed externally by calling Dispose()
        private void shutdown()
        {
            try
            {
                lock (this) //To prevent multiple threads calling shutdown concurrently...not sure why because none of our other code is really thread safe
                {
                    if (initializedUSB) //if reference is already disposed, there is no cleanup to do
                    {
                        //Notify and Close the channels first
                        if (antChannels != null)
                        {
                            NotifyDeviceEvent(DeviceNotificationCode.Shutdown, null);
                            foreach (ANT_Channel i in antChannels)
                            {
                                //We dispose any channel references because since the c++ framer is being destroyed dereferencing it later will blow it up,
                                //By disposing the channel the user will know from the ObjectDisposed exceptions instead of an undeterministic c++ explosion
                                if (i != null)
                                    i.Dispose();    //Since this nullifies all the list references, it will stop all further feedback to the channels
                            }
                        }


                        //Now shutdown all the device resources

                        //Set cancel so if we are in a transfer it has a chance to shutdown gracefully
                        cancelTxFlag = true;

                        //Exit the response polling thread
                        pollingOn = false;
                        if (!responsePoller.Join(1500)) //Wait for the poll thread to terminate
                        {
                            responsePoller.Abort(); //Abort poll thread if it doesn't join
                            System.Diagnostics.Debug.Fail("Response poll thread did not join in timeout and was aborted. Was the application slow/stalling, or should we increase the join timeout?");
                        }

                        //We call reset directly, because we don't care about notifying the mgd channels because we just closed them
                        //If capabilities is null, it means init failed and we don't have an open line of communication to send a reset
                        if (capabilities != null && ANT_Common.autoResetIsEnabled)
                            ANT_ResetSystem(unmanagedANTFramerPtr, 0);

                        //This is the most important part: clean up the big unmanged objects
                        ANT_Close(unmanagedANTSerialPtr, unmanagedANTFramerPtr);
                        if (unmanagedCancelBool != IntPtr.Zero)
                        {
                            Marshal.FreeHGlobal(unmanagedCancelBool);
                            unmanagedCancelBool = IntPtr.Zero;
                        }

                        //Mark as disposed
                        initializedUSB = false;
                    }
                }
            }
            catch (Exception ex)
            {
                //Ignore all exceptions because this is only called on destruction or critical failure anyway
                System.Diagnostics.Debug.Fail("Exception on shutdown: " + ex.Message);
            }
        }


        /// <summary>
        /// Shuts down all open resources, calls reset on the physical device, and nullifies the given ANTDevice and all its channels
        /// </summary>
        /// <param name="deviceToShutdown">ANTDevice to shutdown</param>
        public static void shutdownDeviceInstance(ref ANT_Device deviceToShutdown)
        {
            if (deviceToShutdown != null)
            {
                deviceToShutdown.Dispose();
                deviceToShutdown = null;
            }
        }


        //Allows a disposed channel to nullify its list reference, so the next call to getChannel() can get a new un-disposed reference
        internal void channelDisposed(byte channelNumber)
        {
            antChannels[channelNumber] = null;
        }


        #endregion


        #region non-ANTdll Functions

        /// <summary>
        /// Convert instance to a string including the USB device number the connection is on
        /// </summary>
        public override string ToString()
        {
 	         return base.ToString()+ " on USBdeviceNum: " + USBDeviceNum.ToString();
        }

        /// <summary>
        /// Returns the pointer to the underlying C++ ANT Framer used for messaging
        /// </summary>
        /// <returns>Pointer to C++ ANT Framer</returns>
        internal IntPtr getFramerPtr()
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return unmanagedANTFramerPtr;
        }

        /// <summary>
        /// Returns the device number used when this instance was opened
        /// Note: For some device types this number is not static and can change whenever new devices are enumerated in the system
        /// </summary>
        public int getOpenedUSBDeviceNum()
        {
            return USBDeviceNum;
        }

        /// <summary>
        /// Returns the baud rate used when this instance was opened
        /// </summary>
        public uint getOpenedUSBBaudRate()
        {
            return USBBaudRate;
        }

        /// <summary>
        /// Returns the Frame Type used to open the device
        /// </summary>
        public ANT_ReferenceLibrary.FramerType getOpenedFrameType()
        {
            return frameType;
        }

        /// <summary>
        /// Returns the Port Type used to open the device
        /// </summary>
        public ANT_ReferenceLibrary.PortType getOpenedPortType()
        {
            return portType;
        }

        /// <summary>
        /// Returns the requested ANTChannel or throws an exception if channel doesn't exist.
        /// </summary>
        /// <param name="num">Channel number requested</param>
        public ANT_Channel getChannel(int num)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            if(num > antChannels.Length-1 || num < 0)
                throw new ANT_Exception("Channel number invalid");

            if(antChannels[num] == null)
                antChannels[num] = new ANT_Channel(this, (byte)num);

            return antChannels[num];
        }


        /// <summary>
        /// Returns the number of ANTChannels owned by this device
        /// </summary>
        /// <returns>Number of ant channels on device</returns>
        public int getNumChannels()
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return antChannels.Length;
        }

        void NotifyDeviceEvent(DeviceNotificationCode notification, Object notificationInfo)
        {
            foreach (ANT_Channel i in antChannels)
            {
                if(i != null)
                    i.NotifyDeviceEvent(notification, notificationInfo);
            }
        }


        private void responsePollFunc()
        {
            ushort messageSize = 0;
            byte channelNum;
            bool isMsgForChannel;

            pollingOn = true;   //Set to false on shutdown to terminate the thread
#if (ANTFS_DEBUGGING)
            ANT_Common.initDebugLogThread("Device" + USBDeviceNum + "_ANTReceive"); // We still need this for ANT-FS!  Will not be created if debug disabled
#endif
            while (initializedUSB && pollingOn)    //check continuously; this thread is terminated on destruction of the class
            {
                // We only wait in the unmanged code for 100 ms because we want this thread to be responsive on our side.
                // It does mean that we are running through a lot more cycles than we need to
                messageSize = ANT_WaitForMessage(unmanagedANTFramerPtr, 100);

                if (messageSize == (ushort)0xFFFE)   //DSI_FRAMER_TIMEDOUT
                    continue;   //Expected, just keep looping

                ANTMessage newMessage = new ANTMessage();
                messageSize = ANT_GetMessage(unmanagedANTFramerPtr, ref newMessage);

                if (messageSize == (ushort)0xFFFF)  // DSI_FRAMER_ERROR - in current library could be from CRC error, Write error, Read error, or DeviceGone
                {
                    serialErrorCode error;
                    bool isCritical = false;

                    switch(newMessage.msgID)
                    {
                        case 0x02:  //DSI_FRAMER_ANT_ESERIAL
                            switch(newMessage.ucharBuf[0])
                            {
                                case 0x04:  //ESERIAL -> DSI_FRAMER_ANT_CRC_ERROR
                                    error = serialErrorCode.MessageLost_CrcError;
                                    break;
                                case 0x02:  //ESERIAL -> DSI_SERIAL_EWRITE
                                    error = serialErrorCode.SerialWriteError;
                                    break;
                                case 0x03:  //ESERIAL -> DSI_SERIAL_EREAD
                                    error = serialErrorCode.SerialReadError;
                                    isCritical = true;
                                    break;
                                case 0x01:  //ESERIAL -> DSI_SERIAL_DEVICE_GONE
                                    error = serialErrorCode.DeviceConnectionLost;
                                    isCritical = true;
                                    break;
                                default:
                                    error = serialErrorCode.Unknown;
                                    System.Diagnostics.Debug.Fail("Unknown serial failure, why isn't this known?");
                                    break;
                            }
                            break;
                        case 0x01:  //DSI_FRAMER_ANT_EQUEUE_OVERFLOW
                            error = serialErrorCode.MessageLost_QueueOverflow;
                            break;
                        case 0x03:  //DSI_FRAMER_ANT_EINVALID_SIZE
                            error = serialErrorCode.MessageLost_TooLarge;
                            break;
                        default:
                            error = serialErrorCode.Unknown;
                            System.Diagnostics.Debug.Fail("Unknown serial failure, why isn't this known?");
                            break;
                    }

                    if(isCritical)
                    {
                        //System.Threading.ThreadPool.QueueUserWorkItem(new System.Threading.WaitCallback(x => shutdown())); //Clean up all resources on another thread to allow this one to close cleanly
                        pollingOn = false;  //Just stop polling, since we will never get new messages now. Allow the user to dispose of the device.
                    }

                    //If application has subscribed to the event we can inform them
                    if (serialError != null)
                        serialError(this, error, isCritical);
                    else   //Debug.Fail is a no-op in release mode, so we will only see this in debugging, in release mode we don't want to do anything intrusive (ie:message box, exception)
                        System.Diagnostics.Debug.Fail("Device Serial Communication Failure, HandleSerialError not handled by application");

                    if (isCritical)
                        break;  //If the device is dead, exit the polling loop
                    else
                        continue;
                }

                isMsgForChannel = false;
                channelNum = Byte.MaxValue;

                switch (newMessage.msgID)   //Check if we send to channel or protocol response func
                {
                    // Send on Channel event
                    case (byte)ANT_ReferenceLibrary.ANTMessageID.RESPONSE_EVENT_0x40:
                        if (newMessage.ucharBuf[1] == (byte)ANT_ReferenceLibrary.ANTMessageID.EVENT_0x01)
                            isMsgForChannel = true;
                        break;
                    // or any of the transmission events
                    case (byte)ANT_ReferenceLibrary.ANTMessageID.BROADCAST_DATA_0x4E:
                    case (byte)ANT_ReferenceLibrary.ANTMessageID.ACKNOWLEDGED_DATA_0x4F:
                    case (byte)ANT_ReferenceLibrary.ANTMessageID.EXT_BROADCAST_DATA_0x5D:
                    case (byte)ANT_ReferenceLibrary.ANTMessageID.EXT_ACKNOWLEDGED_DATA_0x5E:
                    case (byte)ANT_ReferenceLibrary.ANTMessageID.BURST_DATA_0x50:
                    case (byte)ANT_ReferenceLibrary.ANTMessageID.EXT_BURST_DATA_0x5F:
                    case (byte)ANT_ReferenceLibrary.ANTMessageID.RSSI_BROADCAST_DATA_0xC1:
                    case (byte)ANT_ReferenceLibrary.ANTMessageID.RSSI_ACKNOWLEDGED_DATA_0xC2:
                    case (byte)ANT_ReferenceLibrary.ANTMessageID.RSSI_BURST_DATA_0xC3:
                        isMsgForChannel = true;
                        break;
                }

                ANT_Response newResponse;

                //Now dispatch to appropriate event
                //The messages are buffered in the ant library so we just dispatch with current thread
                //then no matter how long the event call takes we still won't miss messages
                if (isMsgForChannel)
                {
                    channelNum = (byte)(newMessage.ucharBuf[0] & 0x1F); //Mask out what channel this is for. [We can eventually switch to use the c++ code to determine the channel, but we should do it all together with getMessage to avoid extra marshalling and copying and remove the processing in C# above]
                    if (antChannels != null && channelNum < antChannels.Length)
                    {
                        if (antChannels[channelNum] != null)
                            antChannels[channelNum].MessageReceived(newMessage, messageSize);
                    }
                    else
                    {
                        if (serialError != null)
                            serialError(this, serialErrorCode.MessageLost_InvalidChannel, false);
                    }
                }
                else
                {
                    newResponse = new ANT_Response(this, newMessage.ucharBuf[0], DateTime.Now, newMessage.msgID, newMessage.ucharBuf.Take(messageSize).ToArray());

                    if (deviceResponse != null) //Ensure events are assigned before we call the event
                        deviceResponse(newResponse);
                }

            }
        }


        /// <summary>
        /// Sets the cancel flag on all acknowledged and burst transfers in progress for the given amount of time.
        /// When these transmissions see the flag they will abort their operation and return as cancelled.
        /// </summary>
        /// <param name="cancelWaitTime">Time to set cancel flag for</param>
        public void cancelTransfers(int cancelWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            cancelTxFlag = true;
            System.Threading.Thread.Sleep(cancelWaitTime);
            cancelTxFlag = false;
        }

        /// <overloads>Returns the device capabilities</overloads>
        /// <summary>
        /// Returns the capabilities of this device.
        /// Throws an exception if capabilities are not received.
        /// </summary>
        /// <param name="forceNewCopy">Force function to send request message to device</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>Last device capabilities received or a new copy if forceNewCopy is true</returns>
        public ANT_DeviceCapabilities getDeviceCapabilities(bool forceNewCopy, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            if (forceNewCopy || capabilities == null)
            {
                try
                {
                    ANT_DeviceCapabilities newCaps = null;
                    ANT_Response response = requestMessageAndResponse(ANT_ReferenceLibrary.RequestMessageID.CAPABILITIES_0x54, responseWaitTime);
                    if (response != null && response.responseID == (byte)ANT_ReferenceLibrary.RequestMessageID.CAPABILITIES_0x54)
                    {
                        byte[] padding = new byte[16 - response.messageContents.Length];
                        newCaps = new ANT_DeviceCapabilities(response.messageContents.Concat(padding).ToArray());
                    }
                    capabilities = newCaps; //We replace the old capabilities because that is what forceNewCopy means
                }
                catch (Exception)
                {
                    //If we timeout receiving the message or the interpretation fails
                    throw new ANT_Exception("Retrieving Device Capabilities Failed");
                }
            }
            return capabilities;
        }

        /// <summary>
        /// Returns the device capabilities of this device.
        /// Throws an exception if capabilities are not received.
        /// </summary>
        public ANT_DeviceCapabilities getDeviceCapabilities() { return getDeviceCapabilities(false, 1500); }
        /// <summary>
        /// Returns the device capabilities of this device.
        /// Throws an exception if capabilities are not received.
        /// </summary>
        public ANT_DeviceCapabilities getDeviceCapabilities(UInt32 responseWaitTime) { return getDeviceCapabilities(false, responseWaitTime); }


        #endregion



        #region ANT Device Functions

        /// <overloads>Resets the USB device</overloads>
        /// <summary>
        /// Resets this USB device at the driver level
        /// </summary>
        public void ResetUSB()
        {
            if (portType != ANT_ReferenceLibrary.PortType.USB)
                throw new ANT_Exception("Can't call ResetUSB on non-USB devices");

            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            ANT_USBReset(unmanagedANTSerialPtr);
        }



        /// <overloads>Resets the device and all its channels</overloads>
        /// <summary>
        /// Reset this device and all associated channels
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool ResetSystem(UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            // Notify channels
            NotifyDeviceEvent(DeviceNotificationCode.Reset, null);

            return (ANT_ResetSystem(unmanagedANTFramerPtr, responseWaitTime) == 1);
        }
        /// <summary>
        /// Reset this device and all associated channels
        /// </summary>
        public void ResetSystem(){ ResetSystem(500);}



        /// <overloads>Sets a network key</overloads>
        /// <summary>
        /// Set the network key for the given network
        /// Throws exception if net number is invalid or network key is not 8-bytes in length
        /// </summary>
        /// <param name="netNumber">The network number to set the key for</param>
        /// <param name="networkKey">The 8-byte network key</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setNetworkKey(byte netNumber, byte[] networkKey, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            if (capabilities != null)
            {
                if (netNumber >= capabilities.maxNetworks)
                    throw new ANT_Exception("Network number must be less than the maximum capable networks of the device");
            }

            if (networkKey.Length != 8)
                throw new ANT_Exception("Network key must be 8 bytes");
            bool result = (ANT_SetNetworkKey(unmanagedANTFramerPtr, netNumber, networkKey, responseWaitTime) == 1);
            return result;
        }
        /// <summary>
        /// Set the network key for the given network
        /// Throws exception if net number is invalid or network key is not 8-bytes in length
        /// </summary>
        /// <param name="netNumber">The network number to set the key for</param>
        /// <param name="networkKey">The 8-byte network key</param>
        public void setNetworkKey(byte netNumber, byte[] networkKey) { setNetworkKey(netNumber, networkKey, 0); }



        /// <overloads>Sets the transmit power for all channels</overloads>
        /// <summary>
        /// Set the transmit power for all channels of this device
        /// </summary>
        /// <param name="transmitPower">Transmission power to set to</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setTransmitPowerForAllChannels(ANT_ReferenceLibrary.TransmitPower transmitPower, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return (ANT_SetTransmitPower(unmanagedANTFramerPtr, (byte)transmitPower, responseWaitTime) == 1);
        }
        /// <summary>
        /// Set the transmit power for all channels of this device
        /// </summary>
        /// <param name="transmitPower">Transmission power to set to</param>
        public void setTransmitPowerForAllChannels(ANT_ReferenceLibrary.TransmitPower transmitPower) { setTransmitPowerForAllChannels(transmitPower, 0); }



        /// <summary>
        /// When enabled advanced burst messages will be split into standard burst packets when received.
        /// This is disabled by default.
        /// </summary>
        /// <param name="splitBursts">Whether to split advanced burst messages.</param>
        /// <returns>True on success.</returns>
        public bool configureAdvancedBurstSplitting(bool splitBursts)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return (ANT_ConfigureSplitAdvancedBursts(unmanagedANTFramerPtr, (splitBursts ? 1 : 0)) == 1);
        }


        /// <summary>
        /// Configure advanced bursting for this device.
        /// </summary>
        /// <param name="enable">Whether to enable advanced bursting messages</param>
        /// <param name="maxPacketLength">Maximum packet length allowed for bursting messages (valid values are 1-3)</param>
        /// <param name="requiredFields">Features that the application requires the device to use</param>
        /// <param name="optionalFields">Features that the device should use if it supports them</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool configureAdvancedBursting(bool enable, byte maxPacketLength, ANT_ReferenceLibrary.AdvancedBurstConfigFlags requiredFields,
            ANT_ReferenceLibrary.AdvancedBurstConfigFlags optionalFields, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return (ANT_ConfigureAdvancedBurst(unmanagedANTFramerPtr, (enable ? 1 : 0), maxPacketLength,
                (UInt32)requiredFields, (UInt32)optionalFields, responseWaitTime) == 1);
        }


        /// <summary>
        /// Configure advanced bursting for this device.
        /// </summary>
        /// <param name="enable">Whether to enable advanced bursting messages</param>
        /// <param name="maxPacketLength">Maximum packet length allowed for bursting messages (valid values are 1-3)</param>
        /// <param name="requiredFields">Features that the application requires the device to use</param>
        /// <param name="optionalFields">Features that the device should use if it supports them</param>
        public void configureAdvancedBursting(bool enable, byte maxPacketLength, ANT_ReferenceLibrary.AdvancedBurstConfigFlags requiredFields,
            ANT_ReferenceLibrary.AdvancedBurstConfigFlags optionalFields)
        {
            configureAdvancedBursting(enable, maxPacketLength, requiredFields, optionalFields, (UInt32)0);
        }


        /// <summary>
        /// Configure advanced bursting for this device including extended parameters.
        /// </summary>
        /// <param name="enable">Whether to enable advanced bursting messages</param>
        /// <param name="maxPacketLength">Maximum packet length allowed for bursting messages (valid values are 1-3)</param>
        /// <param name="requiredFields">Features that the application requires the device to use</param>
        /// <param name="optionalFields">Features that the device should use if it supports them</param>
        /// <param name="stallCount">Maximum number of burst periods (~3.1ms) to stall for while waiting for the next message</param>
        /// <param name="retryCount">Number of times (multiplied by 5) to retry burst</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool configureAdvancedBursting_ext(bool enable, byte maxPacketLength, ANT_ReferenceLibrary.AdvancedBurstConfigFlags requiredFields,
            ANT_ReferenceLibrary.AdvancedBurstConfigFlags optionalFields, UInt16 stallCount, byte retryCount, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return (ANT_ConfigureAdvancedBurst_ext(unmanagedANTFramerPtr, (enable ? 1 : 0), maxPacketLength,
                (UInt32)requiredFields, (UInt32)optionalFields, stallCount, retryCount, responseWaitTime) == 1);
        }


        /// <summary>
        /// Configure advanced bursting for this device including extended parameters.
        /// </summary>
        /// <param name="enable">Whether to enable advanced bursting messages</param>
        /// <param name="maxPacketLength">Maximum packet length allowed for bursting messages (valid values are 1-3)</param>
        /// <param name="requiredFields">Features that the application requires the device to use</param>
        /// <param name="optionalFields">Features that the device should use if it supports them</param>
        /// <param name="stallCount">Maximum number of burst periods (~3.1ms) to stall for while waiting for the next message</param>
        /// <param name="retryCount">Number of times (multiplied by 5) to retry burst</param>
        public void configureAdvancedBursting_ext(bool enable, byte maxPacketLength, ANT_ReferenceLibrary.AdvancedBurstConfigFlags requiredFields,
            ANT_ReferenceLibrary.AdvancedBurstConfigFlags optionalFields, UInt16 stallCount, byte retryCount)
        {
            configureAdvancedBursting_ext(enable, maxPacketLength, requiredFields, optionalFields, stallCount, retryCount, 0);
        }

        /// <summary>
        /// Set the encryption key in volatile memory.
        /// </summary>
        /// <param name="volatileKeyIndex">The key index in volatile memory to load the memory key into</param>
        /// <param name="encryptionKey">The 128-bit encryption key</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setCryptoKey(byte volatileKeyIndex, byte[] encryptionKey, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return (ANT_SetCryptoKey(unmanagedANTFramerPtr, volatileKeyIndex, encryptionKey, responseWaitTime) == 1);
        }

        /// <summary>
        /// Set the encryption key in volatile memory.
        /// </summary>
        /// <param name="volatileKeyIndex">The key index in volatile memory to load the memory key into</param>
        /// <param name="encryptionKey">The 128-bit encryption key</param>
        public void setCryptoKey(byte volatileKeyIndex, byte[] encryptionKey)
        {
            setCryptoKey(volatileKeyIndex, encryptionKey, 0);
        }

        /// <summary>
        /// Set the 4-byte encryption ID of the device.
        /// </summary>
        /// <param name="encryptionID">4-byte encryption ID</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setCryptoID(byte[] encryptionID, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return (ANT_SetCryptoID(unmanagedANTFramerPtr, encryptionID, responseWaitTime) == 1);
        }

        /// <summary>
        /// Set the 4-byte encryption ID of the device.
        /// </summary>
        /// <param name="encryptionID">4-byte encryption ID</param>
        public void setCryptoID(byte[] encryptionID)
        {
            setCryptoID(encryptionID, 0);
        }

        /// <summary>
        /// Set the 19-byte user information string of the device.
        /// </summary>
        /// <param name="userInfoString">19-byte user information string</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setCryptoUserInfo(byte[] userInfoString, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return (ANT_SetCryptoUserInfo(unmanagedANTFramerPtr, userInfoString, responseWaitTime) == 1);
        }

        /// <summary>
        /// Set the 19-byte user information string of the device.
        /// </summary>
        /// <param name="userInfoString">19-byte user information string</param>
        public void setCryptoUserInfo(byte[] userInfoString)
        {
            setCryptoUserInfo(userInfoString, 0);
        }

        /// <summary>
        /// Set the 16-byte random number seed for the device. To ensure cryptographic security,
        /// some devices require the random number seed to be inputed from a cryptographically secure RNG.
        /// </summary>
        /// <param name="cryptoRNGSeed">Cryptographically secure 16-byte RGN</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setCryptoRNGSeed(byte[] cryptoRNGSeed, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return (ANT_SetCryptoRNGSeed(unmanagedANTFramerPtr, cryptoRNGSeed, responseWaitTime) == 1);
        }

        /// <summary>
        /// Set the 16-byte random number seed for the device. To ensure cryptographic security,
        /// some devices require the random number seed to be inputed from a cryptographically secure RNG.
        /// </summary>
        /// <param name="cryptoRNGSeed">Cryptographically secure 16-byte RGN</param>
        public void setCryptoRNGSeed(byte[] cryptoRNGSeed)
        {
            setCryptoRNGSeed(cryptoRNGSeed, 0);
        }

        /// <summary>
        /// Set encryption information parameters.
        /// </summary>
        /// <param name="encryptionParameter">0 - Encryption ID, 1 - User Information String, 2 - Random Number Seed</param>
        /// <param name="parameterData">4-byte Encryption ID or 19-byte User Information String or 16-byte Random Number Seed</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setCryptoInfo(ANT_ReferenceLibrary.EncryptionInfo encryptionParameter, byte[] parameterData, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return (ANT_SetCryptoInfo(unmanagedANTFramerPtr, (byte)encryptionParameter, parameterData, responseWaitTime) == 1);
        }

        /// <summary>
        /// Set encryption information parameters.
        /// </summary>
        /// <param name="encryptionParameter">0 - Encryption ID, 1 - User Information String, 2 - Random Number Seed</param>
        /// <param name="parameterData">4-byte Encryption ID or 19-byte User Information String or 16-byte Random Number Seed</param>
        public void setCryptoInfo(ANT_ReferenceLibrary.EncryptionInfo encryptionParameter, byte[] parameterData)
        {
            setCryptoInfo(encryptionParameter, parameterData, 0);
        }

        /// <summary>
        /// Load a saved encryption key from NVM into the volatile memory.
        /// </summary>
        /// <param name="nonVolatileKeyIndex">Index of NVM stored encryption key to load from (0..3)</param>
        /// <param name="volatileKeyIndex">Index of volatile stored encryption key to copy to</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool loadCryptoKeyFromNVM(byte nonVolatileKeyIndex, byte volatileKeyIndex, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return (ANT_LoadCryptoKeyNVMOp(unmanagedANTFramerPtr, nonVolatileKeyIndex, volatileKeyIndex, responseWaitTime) == 1);
        }

        /// <summary>
        /// Load a saved encryption key from NVM into the volatile memory.
        /// </summary>
        /// <param name="nonVolatileKeyIndex">Index of NVM stored encryption key to load from (0..3)</param>
        /// <param name="volatileKeyIndex">Index of volatile stored encryption key to copy to</param>
        public void loadCryptoKeyFromNVM(byte nonVolatileKeyIndex, byte volatileKeyIndex)
        {
            loadCryptoKeyFromNVM(nonVolatileKeyIndex, volatileKeyIndex, 0);
        }

        /// <summary>
        /// Save an encryption key to NVM.
        /// </summary>
        /// <param name="nonVolatileKeyIndex">Index of NVM stored encryption key to store to (0..3)</param>
        /// <param name="encryptionKey">16-byte encryption key</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool storeCryptoKeyToNVM(byte nonVolatileKeyIndex, byte[] encryptionKey, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return (ANT_StoreCryptoKeyNVMOp(unmanagedANTFramerPtr, nonVolatileKeyIndex, encryptionKey, responseWaitTime) == 1);
        }

        /// <summary>
        /// Save an encryption key to NVM.
        /// </summary>
        /// <param name="nonVolatileKeyIndex">Index of NVM stored encryption key to store to (0..3)</param>
        /// <param name="encryptionKey">16-byte encryption key</param>
        public void storeCryptoKeyToNVM(byte nonVolatileKeyIndex, byte[] encryptionKey)
        {
            storeCryptoKeyToNVM(nonVolatileKeyIndex, encryptionKey, 0);
        }

        /// <summary>
        /// Load/Store Encryption Key from/in NVM
        /// </summary>
        /// <param name="memoryOperation">0 - Load encryption key from NVM or 1 - Store encryption key in NVM</param>
        /// <param name="nonVolatileKeyIndex">The index of the Encryption Key in NVM to be loaded or stored to
        /// depending on the selected operation (0..3)</param>
        /// <param name="operationData">When Operation is set to 0x00: The index of the volatile key location that should be loaded with the NVM stored encryption key.
        /// When Operation is set to 0x01: The 128-bit Encryption Key to be stored to NVM</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool cryptoKeyNVMOp(ANT_ReferenceLibrary.EncryptionNVMOp memoryOperation, byte nonVolatileKeyIndex, byte[] operationData, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return (ANT_CryptoKeyNVMOp(unmanagedANTFramerPtr, (byte)memoryOperation, nonVolatileKeyIndex, operationData, responseWaitTime) == 1);
        }

        /// <summary>
        /// Load/Store Encryption Key from/in NVM
        /// </summary>
        /// <param name="memoryOperation">0 - Load encryption key from NVM or 1 - Store encryption key in NVM</param>
        /// <param name="nonVolatileKeyIndex">The index of the Encryption Key in NVM to be loaded or stored to
        /// depending on the selected operation (0..3)</param>
        /// <param name="operationData">When Operation is set to 0x00: The index of the volatile key location that should be loaded with the NVM stored encryption key.
        /// When Operation is set to 0x01: The 128-bit Encryption Key to be stored to NVM</param>
        public void cryptoKeyNVMOp(ANT_ReferenceLibrary.EncryptionNVMOp memoryOperation, byte nonVolatileKeyIndex, byte[] operationData)
        {
            cryptoKeyNVMOp(memoryOperation, nonVolatileKeyIndex, operationData, 0);
        }

        /// <overloads>Enables/Disables the device's LED</overloads>
        /// <summary>
        /// Enables/Disables the LED flashing when a transmission event occurs
        /// </summary>
        /// <param name="IsEnabled">Desired state</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool EnableLED(bool IsEnabled, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return (ANT_EnableLED(unmanagedANTFramerPtr, Convert.ToByte(IsEnabled), responseWaitTime) == 1);
        }
        /// <summary>
        /// Enables/Disables the LED flashing when a transmission event occurs
        /// </summary>
        /// <param name="IsEnabled">Desired state</param>
        public void EnableLED(bool IsEnabled) { EnableLED(IsEnabled, 0); }

        /// <overloads>Configures Event Buffering</overloads>
        /// <summary>
        /// Allows buffering of ANT events.  Preset groups of events may be selected for buffering.
        /// Events may be buffered by size or time and buffering can be used in conjunction with filtering.
        /// </summary>
        /// <param name="config">Selects which events are buffered</param>
        /// <param name="size">Number of bytes that will be stored before a buffer flush occurs.  Set to 0 to disable.</param>
        /// <param name="time">Maximum time (in 10ms units) before a buffer flush occurs.  Set to 0 to disable.
        /// Buffer size must also be set to a non zero value.</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool configureEventBuffer(ANT_ReferenceLibrary.EventBufferConfig config, UInt16 size, UInt16 time, UInt32 responseWaitTime)
        {
           if (!initializedUSB)
              throw new ObjectDisposedException("ANTDevice object has been disposed");

           return ANT_ConfigEventBuffer(unmanagedANTFramerPtr, (byte)config, size, time, responseWaitTime) == 1;
        }
        /// <summary>
        /// Allows buffering of ANT events.  Preset groups of events may be selected for buffering.
        /// Events may be buffered by size or time and buffering can be used in conjunction with filtering.
        /// </summary>
        /// <param name="config">Selects which events are buffered</param>
        /// <param name="size">Number of bytes that will be stored before a buffer flush occurs.  Set to 0 to disable.</param>
        /// <param name="time">Maximum time (in 10ms units) before a buffer flush occurs.  Set to 0 to disable.
        /// Buffer size must also be set to a non zero value.</param>
        public void configureEventBuffer(ANT_ReferenceLibrary.EventBufferConfig config, UInt16 size, UInt16 time) { configureEventBuffer(config, size, time, 0); }

        /// <overloads>Configures Event Filtering</overloads>
        /// <summary>
        /// Allows filtering of specified ANT events.  Filtering can be used in conjunction with buffering.
        /// </summary>
        /// <param name="eventFilter">Bitfield of events to filter.  Set Bit0 to filter event 0 etc.</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool configureEventFilter(UInt16 eventFilter, UInt32 responseWaitTime)
        {
           if (!initializedUSB)
              throw new ObjectDisposedException("ANTDevice object has been disposed");

           return ANT_ConfigEventFilter(unmanagedANTFramerPtr, eventFilter, responseWaitTime) == 1;
        }
        /// <summary>
        /// Allows filtering of specified ANT events.  Filtering can be used in conjunction with buffering.
        /// </summary>
        /// <param name="eventFilter">Bitfield of events to filter.  Set Bit0 to filter event 0 etc.</param>
        public void configureEventFilter(UInt16 eventFilter) { configureEventFilter(eventFilter, 0); }

        /// <overloads>Configures High Duty Search</overloads>
        /// <summary>
        /// Allows configuring High Duty Search if no channels have been opened yet
        /// </summary>
        /// <param name="enable">Enable or disable High Duty Search</param>
        /// <param name="suppressionCycles">Search period to suppress high duty search in units of 250ms.  0=Allow full time, 5=Suppress entirely</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool configureHighDutySearch(bool enable, byte suppressionCycles, UInt32 responseWaitTime)
        {
           if (!initializedUSB)
              throw new ObjectDisposedException("ANTDevice object has been disposed");

           return ANT_ConfigHighDutySearch(unmanagedANTFramerPtr, (enable ? (byte)1 : (byte)0), suppressionCycles, responseWaitTime) == 1;
        }
        /// <summary>
        /// Allows configuring High Duty Search if no channels have been opened yet
        /// </summary>
        /// <param name="enable">Enable or disable High Duty Search</param>
        /// <param name="suppressionCycles">Search period to suppress high duty search in units of 250ms.  0=Allow full time, 5=Suppress entirely</param>
        public void configureHighDutySearch(bool enable, byte suppressionCycles) { configureHighDutySearch(enable, suppressionCycles, 0); }

        /// <overloads>Allows defining a new Selective Data Update Mask</overloads>
        /// <summary>
        /// Allows defining a new Selective Data Update Mask
        /// </summary>
        /// <param name="maskNumber">Identifier for the SDU Mask</param>
        /// <param name="mask">Rx Data Message Mask, 0=Ignore, 1=Update on Change</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setSduMask(byte maskNumber, byte[] mask, UInt32 responseWaitTime)
        {
           if (!initializedUSB)
              throw new ObjectDisposedException("ANTDevice object has been disposed");

           return ANT_SetSelectiveDataUpdateMask(unmanagedANTFramerPtr, maskNumber, mask, responseWaitTime) == 1;
        }
        /// <summary>
        /// Allows defining a new Selective Data Update Mask
        /// </summary>
        /// <param name="maskNumber">Identifier for the SDU Mask</param>
        /// <param name="mask">Rx Data Message Mask, 0=Ignore, 1=Update on Change</param>
        public void setSduMask(byte maskNumber, byte[] mask) { setSduMask(maskNumber, mask, 0); }

       /// <overloads>Configures User NVM</overloads>
        /// <summary>
        /// Allows configuring User NVM if available.
        /// </summary>
        /// <param name="address"> Nvm starting address</param>
        /// <param name="data">Data block to write</param>
        /// <param name="size">Size of data block</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool configureUserNvm(UInt16 address, byte[] data, byte size, UInt32 responseWaitTime)
        {
           if (!initializedUSB)
              throw new ObjectDisposedException("ANTDevice object has been disposed");

           return ANT_ConfigUserNVM(unmanagedANTFramerPtr, address, data, size, responseWaitTime) == 1;
        }
        /// <summary>
        /// Allows configuring User Nvm if available.
        /// </summary>
        /// <param name="address"> Nvm starting address</param>
        /// <param name="data">Data block to write</param>
        /// <param name="size">Size of data block</param>
        public void configureUserNvm(UInt16 address, byte[] data, byte size) { configureUserNvm(address, data, size, 0); }

        /// <overloads>Requests a message from the device and returns the response</overloads>
        /// <summary>
        /// Read User Nvm by sending Request Mesg capturing the response.
        /// Throws exception on timeout.
        /// </summary>
        /// <param name="address">NVM Address to read from</param>
        /// <param name="size">Number of bytes to read</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        public ANT_Response readUserNvm(UInt16 address, byte size, UInt32 responseWaitTime)
        {
           if (!initializedUSB)
              throw new ObjectDisposedException("ANTDevice object has been disposed");

           ANTMessageItem response = new ANTMessageItem();
           if (ANT_RequestUserNvmMessage(unmanagedANTFramerPtr, 0, (byte)ANT_ReferenceLibrary.RequestMessageID.USER_NVM_0x7C, ref response, address, size, responseWaitTime) == 0)
              throw new ANT_Exception("Timed out waiting for requested message");
           ANT_Response retVal = new ANT_Response(this, 0, DateTime.Now, response.antMsgData.msgID, response.antMsgData.ucharBuf.Take(response.dataSize).ToArray());
           return retVal;
        }
        /// <summary>
        /// Read User Nvm by sending Request Mesg capturing the response.
        /// Throws exception on timeout.
        /// </summary>
        /// <param name="address">NVM Address to read from</param>
        /// <param name="size">Number of bytes to read</param>
        public ANT_Response readUserNvm(UInt16 address, byte size)
        {
           return readUserNvm(address, size, 500);
        }

        /// <summary>
        /// Obtains the PID (Product ID) of the USB device.
        /// Throws an exception if the PID is not received.
        /// </summary>
        /// <returns>PID of the USB device.</returns>
        public UInt16 getDeviceUSBPID()
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            UInt16 returnPID = 0;

            if(ANT_GetDeviceUSBPID(unmanagedANTFramerPtr, ref returnPID) != 1)
                throw new ANT_Exception("Retrieving Device USB PID failed");
            return (returnPID);
        }


        /// <summary>
        /// Obtains the VID (Vendor ID) of the USB device
        /// </summary>
        /// <returns>VID of the USB device</returns>
        public UInt16 getDeviceUSBVID()
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            UInt16 returnVID = 0;

            if (ANT_GetDeviceUSBVID(unmanagedANTFramerPtr, ref returnVID) != 1)
                throw new ANT_Exception("Retrieving Device USB VID failed");
            return (returnVID);
        }


        /// <summary>
        /// Returns the USB device serial number.
        /// This can be used to figure out the serial number if the option to use the USB device
        /// serial number was selected.
        /// </summary>
        /// <returns>Client serial number</returns>
        public uint getSerialNumber()
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return ANT_GetDeviceSerialNumber(unmanagedANTSerialPtr);
        }


        /// <overloads>Obtains the device USB Information</overloads>
        /// <summary>
        /// Obtains the USB information for the device
        /// Throws an exception if no information is received
        /// </summary>
        /// <param name="deviceNum">USB Device Number</param>
        /// <returns>USB Device Information</returns>
        public ANT_DeviceInfo getDeviceUSBInfo(byte deviceNum)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            byte[] returnProdDescription = new byte[256];   // size should match that defined on the Unmanaged Wrapper
            byte[] returnSerialString = new byte[256];

            if (ANT_GetDeviceUSBInfo(unmanagedANTFramerPtr, deviceNum, returnProdDescription, returnSerialString) != 1)
                throw new ANT_Exception("Retrieving USB device information failed");

            return (new ANT_DeviceInfo(returnProdDescription, returnSerialString));
        }


        /// <summary>
        /// Obtains the USB information for the device
        /// Throws an exception if no information is received
        /// </summary>
        /// <returns>USB Device Information</returns>
        public ANT_DeviceInfo getDeviceUSBInfo()
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return getDeviceUSBInfo(USBDeviceNum);
        }




        /// <overloads>Requests a message from the device and returns the response</overloads>
        /// <summary>
        /// Request a message from device and returns the response.
        /// Throws exception on timeout.
        /// </summary>
        /// <param name="channelNum">Channel to send request on</param>
        /// <param name="messageID">Request to send</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        public ANT_Response requestMessageAndResponse(byte channelNum, ANT_ReferenceLibrary.RequestMessageID messageID, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            ANTMessageItem response = new ANTMessageItem();
            if (ANT_RequestMessage(unmanagedANTFramerPtr, channelNum, (byte)messageID, ref response, responseWaitTime) == 0)
                throw new ANT_Exception("Timed out waiting for requested message");
            ANT_Response retVal = new ANT_Response(this, channelNum, DateTime.Now, response.antMsgData.msgID, response.antMsgData.ucharBuf.Take(response.dataSize).ToArray());
            return retVal;
        }
        /// <summary>
        /// Request a message from device on channel 0 and returns the response.
        /// Throws exception on timeout.
        /// </summary>
        /// <param name="messageID">Request to send</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        public ANT_Response requestMessageAndResponse(ANT_ReferenceLibrary.RequestMessageID messageID, UInt32 responseWaitTime)
        {
            return requestMessageAndResponse(0, messageID, responseWaitTime);
        }

        /// <overloads>Requests a message from the device</overloads>
        /// <summary>
        /// Request a message from device
        /// </summary>
        /// <param name="channelNum">Channel to send request on</param>
        /// <param name="messageID">Request to send</param>
        public void requestMessage(byte channelNum, ANT_ReferenceLibrary.RequestMessageID messageID)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");
            ANTMessageItem dummystruct = new ANTMessageItem();
            ANT_RequestMessage(unmanagedANTFramerPtr, channelNum, (byte)messageID, ref dummystruct, 0);

        }
        /// <summary>
        /// Request a message from device
        /// </summary>
        /// <param name="messageID">Request to send</param>
        public void requestMessage(ANT_ReferenceLibrary.RequestMessageID messageID)
        {
            requestMessage(0, messageID);
        }



        /// <overloads>Set device in continuous scanning mode</overloads>
        /// <summary>
        /// Starts operation in continuous scanning mode.
        /// This allows the device to receive all messages matching the configured channel ID mask in an asynchronous manner.
        /// This feature is not available on all ANT devices.
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool openRxScanMode(UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return ANT_OpenRxScanMode(unmanagedANTFramerPtr, responseWaitTime) == 1;
        }
        /// <summary>
        /// Starts operation in continuous scanning mode.
        /// This allows the device to receive all messages matching the configured channel ID mask in an asynchronous manner.
        /// </summary>
        public void openRxScanMode() { openRxScanMode(0); }



        /// <overloads>Initializes and starts CW test mode</overloads>
        /// <summary>
        /// Initialize and start CW test mode. This mode is to test your implementation for RF frequency requirements.
        /// The device will transmit an unmodulated carrier wave at the RF frequency of 2400Mhz + RFFreqOffset at the specified power level.
        /// This mode can then only be exited by a system reset.
        /// Note: When this function call returns false, the system will be reset automatically.
        /// </summary>
        /// <param name="transmitPower">Transmission power to test at</param>
        /// <param name="RFFreqOffset">Offset to add to 2400Mhz</param>
        /// <param name="responseWaitTime">Time to wait for response, used for both initialization and start command</param>
        /// <returns>False if initialization or starting of CW test mode fails. On false, the system is automatically reset.</returns>
        /// <remarks>
        /// This function encapsulates both ANT_InitCWTestMode and ANT_SetCWTestMode from the old library.
        /// It will automatically reset the system if either call fails.
        /// The given response time is used for both calls and the reset time is 500ms.
        /// So max wait time = responseTime*2 + 500ms
        /// </remarks>
        public bool startCWTest(ANT_ReferenceLibrary.TransmitPower transmitPower, byte RFFreqOffset, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            bool retVal = true;
            ResetSystem();  //CW Mode is only supposed to be set up after a reset
            if (ANT_InitCWTestMode(unmanagedANTFramerPtr, responseWaitTime) != 1)
                retVal = false;
            if (ANT_SetCWTestMode(unmanagedANTFramerPtr, (byte)transmitPower, RFFreqOffset, responseWaitTime) != 1)
                retVal = false;
            if (retVal == false && ANT_Common.autoResetIsEnabled) //If we messed up we want to make sure the module doesn't get messed up in CW mode, so we reset
                ResetSystem(500);
            return retVal;
        }



        /// <overloads>Enables extended message reception</overloads>
        /// <summary>
        /// Enables extended message receiving. When enabled, messages received will contain extended data.
        /// </summary>
        /// <param name="IsEnabled">Desired State</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool enableRxExtendedMessages(bool IsEnabled, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return ANT_RxExtMesgsEnable(unmanagedANTFramerPtr, Convert.ToByte(IsEnabled), responseWaitTime) == 1;
        }
        /// <summary>
        /// Enables extended message receiving. When enabled, messages received will contain extended data.
        /// </summary>
        /// <param name="IsEnabled">Desired State</param>
        public void enableRxExtendedMessages(bool IsEnabled) { enableRxExtendedMessages(IsEnabled, 0); }



        /// <overloads>Enables the use of external 32kHz crystal</overloads>
        /// <summary>
        /// If the use of an external 32kHz crystal input is desired, this message must be sent once, each time a startup message is received
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        /// <remarks> Enabling an external 32kHz crystal input as a low power clock source saves ~85uA while ANT is active when compared to using the internal clock source. </remarks>
        public bool crystalEnable(UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return ANT_CrystalEnable(unmanagedANTFramerPtr, responseWaitTime) == 1;
        }
        /// <summary>
        /// If the use of an external 32kHz crystal input is desired, this message must be sent once, each time a startup message is received
        /// </summary>
        /// <remarks> Enabling an external 32kHz crystal input as a low power clock source saves ~85uA while ANT is active when compared to using the internal clock source. </remarks>
        public void crystalEnable() { crystalEnable(0); }




        /// <summary>
        /// Writes a message to the device, this function allows sending manually formatted messages.
        /// </summary>
        /// <param name="msgID">msgID to write</param>
        /// <param name="msgData">data buffer to write</param>
        /// <returns>False if writing bytes to device fails</returns>
        public bool writeRawMessageToDevice(byte msgID, byte[] msgData)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            //Create ANT_MESSAGE
            ANTMessage aMsg = new ANTMessage();
            aMsg.msgID = msgID;

            //We create an array of max size, because the struct sizing expects a constant size array
            int padNum = ANT_ReferenceLibrary.MAX_MESG_SIZE - msgData.Length;
            if(padNum < 0)
                throw new ANT_Exception("msgData max length is " + ANT_ReferenceLibrary.MAX_MESG_SIZE + " bytes");
            aMsg.ucharBuf = msgData.Concat(new byte[padNum]).ToArray();

            return ANT_WriteMessage(unmanagedANTFramerPtr, aMsg, (UInt16)msgData.Length) == 1;
        }

        /// <overloads>Configure the device ANT library, ie: to send extra msg info</overloads>
        /// <summary>
        /// Configure the device ANT library, ie: to send extra msg info
        /// </summary>
        /// <param name="libConfigFlags">Config flags</param>
        /// <param name="responseWaitTime">Time to wait for response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setLibConfig(ANT_ReferenceLibrary.LibConfigFlags libConfigFlags, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return ANT_SetLibConfig(unmanagedANTFramerPtr, (byte)libConfigFlags, responseWaitTime) == 1;
        }
        /// <summary>
        /// Configure the device ANT library, ie: to send extra msg info
        /// </summary>
        /// <param name="libConfigFlags">Config flags</param>
        public void setLibConfig(ANT_ReferenceLibrary.LibConfigFlags libConfigFlags) { setLibConfig(libConfigFlags, 0); }




        #region SensRcore NVM script commands
        /// <overloads>Writes a SensRCore command to non-volatile memory</overloads>
        /// <summary>
        /// Writes a SensRcore command to non-volatile memory.
        /// Throws exception if command string length > 255, although commands will be much smaller
        /// </summary>
        /// <param name="commandString">SensRcore command to write: [Cmd][CmdData0]...[CmdDataN], must be less than 256 bytes</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool script_Write(byte[] commandString, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            if(!(commandString.Length < 256))   //We are casting to byte, so we need to be safe
                throw new ANT_Exception("commandString max size is 255");
            return ANT_Script_Write(unmanagedANTFramerPtr, (byte)commandString.Length, commandString, responseWaitTime) == 1;
        }
        /// <summary>
        /// Writes a SensRcore command to non-volatile memory.
        /// Throws exception if command string length > 255.
        /// </summary>
        /// <param name="commandString">SensRcore command to write: [Cmd][CmdData0]...[CmdDataN], must be less than 256 bytes</param>
        public void script_Write(byte[] commandString) { script_Write(commandString, 0); }




        /// <overloads>Clears the NVM</overloads>
        /// <summary>
        /// Clears the non-volatile memory. NVM should be cleared before beginning write operations.
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool script_Clear(UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return ANT_Script_Clear(unmanagedANTFramerPtr, responseWaitTime) == 1;
        }
        /// <summary>
        /// Clears the non-volatile memory. NVM should be cleared before beginning write operations.
        /// </summary>
        public void script_Clear() { script_Clear(0); }



        /// <overloads>Sets the default SensRCore sector</overloads>
        /// <summary>
        /// Set the default sector which will be executed after mandatory execution of sector 0.
        /// This command has no effect if it is set to 0 or the Read Pins for Sector command appears in sector 0.
        /// </summary>
        /// <param name="sectorNum">sector number to set as default</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool script_setDefaultSector(byte sectorNum, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return ANT_Script_SetDefaultSector(unmanagedANTFramerPtr, sectorNum, responseWaitTime) == 1;
        }
        /// <summary>
        /// Set the default sector which will be executed after mandatory execution of sector 0.
        /// This command has no effect if it is set to 0 or the Read Pins for Sector command appears in sector 0.
        /// </summary>
        /// <param name="sectorNum">sector number to set as default</param>
        public void script_setDefaultSector(byte sectorNum) { script_setDefaultSector(sectorNum, 0); }




        /// <overloads>Writes a sector break to NVM</overloads>
        /// <summary>
        /// Writes a sector break in the NVM image
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool script_endSector(UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return ANT_Script_EndSector(unmanagedANTFramerPtr, responseWaitTime) == 1;
        }
        /// <summary>
        /// Writes a sector break in the NVM image
        /// </summary>
        public void script_endSector() { script_endSector(0); }




        /// <overloads>Request a dump of the device's script memory</overloads>
        /// <summary>
        /// Requests the device to return the current NVM contents through the device callback function.
        /// The end of the dump is signified by a 0x57 NVM_Cmd msg, which contains 0x04 EndDump code followed by
        /// a byte signifying how many instructions were read and returned.
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool script_requestNVMDump(UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return ANT_Script_Dump(unmanagedANTFramerPtr, responseWaitTime) == 1;
        }
        /// <summary>
        /// Requests the device to return the current NVM contents through the device callback function.
        /// The end of the dump is signified by a 0x57 NVM_Cmd msg, which contains 0x04 EndDump code followed by
        /// a byte signifying how many instructions were read and returned.
        /// </summary>
        public void script_requestNVMDump() { script_requestNVMDump(0); }



        /// <overloads>Locks the NVM contents</overloads>
        /// <summary>
        /// Locks the NVM so that it can not be read by the dump function.
        /// Can only be disabled by clearing the NVM.
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool script_lockNVM(UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            return ANT_Script_Lock(unmanagedANTFramerPtr, responseWaitTime) == 1;
        }
        /// <summary>
        /// Locks the NVM so that it can not be read by the dump function.
        /// Can only be disabled by clearing the NVM.
        /// </summary>
        public void script_lockNVM() { script_lockNVM(0); }



        ///<overloads>Sets the equipment state</overloads>
        /// <summary>
        /// Sets the equipment state.
        /// This command is specifically for use with the FIT1e module.
        /// </summary>
        /// <param name="feState">Fitness equipment state</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success.  Note: Always returns true with a response time of 0</returns>
        public bool fitSetFEState(byte feState, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            if (!capabilities.FIT)  // Check this is a FIT1e module
                return false;

            return FIT_SetFEState(unmanagedANTFramerPtr, feState, responseWaitTime) == 1;
        }

        /// <summary>
        /// Sets the equipment state.
        /// This command is specifically for use with the FIT1e module.
        /// </summary>
        /// <param name="feState">Fitness equipment state</param>
        public void fitSetFEState(byte feState) { fitSetFEState(feState, 0);}

        /// <summary>
        /// Adjusts the pairing distance settings.
        /// This command is specifically for use with the FIT1e module.
        /// </summary>
        /// <param name="searchLv">Minimum signal strength for a signal to be considered for pairing.</param>
        /// <param name="pairLv">Signal strength required for the FIT1e to pair with an ANT+ HR strap or watch</param>
        /// <param name="trackLv">An ANT+ device will unpair if the signal strength drops below this setting while in
        /// READY state or within the first 30 secons of the IN_USE state</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success.  Note: Always returns true with a response time of 0</returns>
        public bool fitAdjustPairingSettings(byte searchLv, byte pairLv, byte trackLv, UInt32 responseWaitTime)
        {
            if (!initializedUSB)
                throw new ObjectDisposedException("ANTDevice object has been disposed");

            if (!capabilities.FIT)  // Check this is a FIT1e module
                return false;

            return FIT_AdjustPairingSettings(unmanagedANTFramerPtr, searchLv, pairLv, trackLv, responseWaitTime) == 1;
        }

        /// <summary>
        /// Adjusts the pairing distance settings.
        /// This command is specifically for use with the FIT1e module.
        /// </summary>
        /// <param name="searchLv">Minimum signal strength for a signal to be considered for pairing.</param>
        /// <param name="pairLv">Signal strength required for the FIT1e to pair with an ANT+ HR strap or watch</param>
        /// <param name="trackLv">An ANT+ device will unpair if the signal strength drops below this setting while in
        /// READY state or within the first 30 secons of the IN_USE state</param>
        public void fitAdjustPairingSettings(byte searchLv, byte pairLv, byte trackLv)
        {
            fitAdjustPairingSettings(searchLv, pairLv, trackLv, 0);
        }
        #endregion

        #endregion
    }
}
