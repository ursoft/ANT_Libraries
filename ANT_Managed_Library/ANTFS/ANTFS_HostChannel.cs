/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/
using System;
using System.Threading;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;
using System.ComponentModel;
using System.Reflection;

using ANT_Managed_Library;

namespace ANT_Managed_Library.ANTFS
{
    /// <summary>
    /// ANT-FS Host
    /// </summary>
    public class ANTFS_HostChannel : IDisposable
    {
        #region Public Definitions

#pragma warning disable 1591

        /// <summary>
        /// ANT-FS Responses
        /// </summary>
        /// !!! Must match the enum in antfs_host_channel.hpp
        public enum Response : byte
        {
            [Description("No response")]
            None = 0,
            [Description("ANT-FS Host Idle")]
            InitPass,
            [Description("Error: Serial failure")]
            SerialFail,
            [Description("Failure requesting ANT-FS session from broadcast device")]
            RequestSessionFail,
            [Description("Found remote device")]
            ConnectPass,
            [Description("Disconnected successfully")]
            DisconnectPass,
            [Description("Lost connection")]
            ConnectionLost,
            [Description("Request for serial number successful")]
            AuthenticateNA,
            [Description("Authentication passed")]
            AuthenticatePass,
            [Description("Authentication rejected")]
            AuthenticateReject,
            [Description("Authentication failed")]
            AuthenticateFail,
            [Description("Downloaded data")]
            DownloadPass,
            [Description("Download rejected")]
            DownloadReject,
            [Description("Download failed, invalid index requested")]
            DownloadInvalidIndex,
            [Description("Download failed, file not readable")]
            DownloadFileNotReadable,
            [Description("Download failed, not ready")]
            DownloadNotReady,
            [Description("Download failed, CRC rejected")]
            DownloadCRCRejected,
            [Description("Download failed")]
            DownloadFail,
            [Description("Uploaded data")]
            UploadPass,
            [Description("Upload rejected")]
            UploadReject,
            [Description("Upload failed, invalid index requested")]
            UploadInvalidIndex,
            [Description("Upload failed, file not writeable")]
            UploadFileNotWriteable,
            [Description("Upload failed, insufficient space")]
            UploadInsufficientSpace,
            [Description("Upload failed")]
            UploadFail,
            [Description("Erased data")]
            ErasePass,
            [Description("Erase failed")]
            EraseFail,
            [Description("Manual transfer complete")]
            ManualTransferPass,
            [Description("Manual transmit failed")]
            ManualTransferTransmitFail,
            [Description("Manual response failed")]
            ManualTransferResponseFail,
            [Description("Request canceled")]
            CancelDone
        };

        /// <summary>
        /// ANT-FS State
        /// </summary>
        /// !!! Must match the enum in antfs_host_channel.hpp
        public enum State : byte
        {
            [Description("Off, state machine not initialized")]
            Off = 0,
            [Description("Idle")]
            Idle,
            [Description("Disconnecting")]
            Disconnecting,
            [Description("Requesting an ANT-FS Session from broadcast device")]
            RequestingSession,
            [Description("Searching")]  // LINK
            Searching,
            [Description("Connected")]  // AUTH
            Connected,
            [Description("Authenticating")]
            Authenticating,
            [Description("Transport")]
            Transport,
            [Description("Downloading data")]
            Downloading,
            [Description("Uploading data")]
            Uploading,
            [Description("Erasing data")]
            Erasing,
            [Description("Sending")]
            Sending,
            [Description("Receiving")]
            Receiving
        };

        /// <summary>
        /// Beacon ANT-FS State
        /// </summary>
        /// !!! Must match the defs in antfsmessage.h
        public enum BeaconAntFsState : byte
        {
            [Description("Link State")]
            Link = 0x00,
            [Description("Link State")]
            Auth = 0x01,
            [Description("Link State")]
            Trans = 0x02,
            [Description("Link State")]
            Busy = 0x03,
            [Description("Beacon not found")]
            BeaconNotFound = 0x80   //Valid states are in range 0x0F, so this avoids all valid states
        };

#pragma warning restore 1591

        #endregion

        #region Variables
        bool bInitialized;
        bool bResponseThreadExit;   // Flag response thread to exit
        Thread ResponseThread;      // This thread is in charge of monitoring the device for responses and forwarding them to the appropiate handler

        Object syncToken = new object();

        ushort? usCurrentTransfer = null;           // Current transfer being processed

        IntPtr unmanagedHostPtr = IntPtr.Zero;      // Holder for the pointer to unmanaged ANTFSHost object
        IntPtr unmanagedAuthBuffer = IntPtr.Zero;   // Holder for pointer to unmanaged buffer for Authentication Response
        IntPtr unmanagedAuthSize = IntPtr.Zero;     // Holder for pointer to the size of the Authentication Response
        GCHandle? ghUploadData = null;              // Handle to allow pinning the managed upload buffer.

        byte networkNumber = 0;     // ANT network number used for the ANT-FS host

        IANT_Channel channel;       // ANT channel HostChannel is using
        dRawChannelResponseHandler rawChannelResponseHandler;     // ANT channel response handler
        dDeviceNotificationHandler deviceNotificationHandler;     // ANT device notification handler

        private object onResponseLock = new object();

        #endregion

        #region Events

        private event Action<Response> onResponse_internal;
        /// <summary>
        /// The ANT-FS host callback event, triggered every time a response is received from the ANT-FS host library
        /// </summary>
        public event Action<Response> OnResponse
        {
            add
            {
                lock (onResponseLock)
                {
                    onResponse_internal += value;
                }
            }
            remove
            {
                lock (onResponseLock)
                {
                    onResponse_internal -= value;
                }
            }
        }

        #endregion

        #region DLLImports

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_New(ref IntPtr HostPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_Init(IntPtr HostPtr, IntPtr FramerPtr, byte ucANTChannel);
        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern void ANTFSHost_GetCurrentConfig(IntPtr HostPtr, ref ANTFS_ConfigParameters pstCfgParam);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_SetCurrentConfig(IntPtr HostPtr, ref ANTFS_ConfigParameters pstCfgParam);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_Close(IntPtr HostPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_Delete(IntPtr HostPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_ProcessDeviceNotification(
            IntPtr HostPtr,
            byte ucCode,
            IntPtr pvParameter);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_ProcessMessage(IntPtr ClientPtr, ref ANT_Device.ANTMessage pstANTMessage, ushort usMessageSize);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_SetChannelID(IntPtr HostPtr, byte ucDeviceType, byte ucTransmissionType);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_SetNetworkKey(IntPtr HostPtr, byte networkNumber, byte[] pucKey);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_SetChannelPeriod(IntPtr HostPtr, ushort usChannelPeriod);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_SetProximitySearch(IntPtr HostPtr, byte ucSearchThreshold);


        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_SetSerialNumber(IntPtr HostPtr, uint ulSerialNumber);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_Cancel(IntPtr HostPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_ClearSearchDeviceList(IntPtr HostPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern ushort ANTFSHost_AddSearchDevice(
            IntPtr HostPtr,
            ref ANTFS_DeviceParameters pstDeviceSearchMask,
            ref ANTFS_DeviceParameters pstDeviceParameters);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_RemoveSearchDevice(IntPtr HostPtr, ushort usHandle);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSHost_RequestSession(IntPtr HostPtr, byte ucBroadcastFreq, byte ucConnectFreq);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSHost_SearchForDevice(
            IntPtr HostPtr,
            byte ucSearchRadioFrequency,
            byte ucConnectRadioFrequency,
            ushort usRadioChannelID,
            bool bUseRequestPage);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSHost_Disconnect(
            IntPtr HostPtr,
            ushort usBlackoutTime,
            byte ucDisconnectType,
            byte ucUndiscoverableDuration,
            byte ucAppSpecificDuration);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSHost_SwitchFrequency(IntPtr HostPtr, byte ucRadioFreq, byte ucChannelPeriod);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_Blackout(
            IntPtr HostPtr,
            uint ulDeviceID,
            ushort usManufacturerID,
            ushort usDeviceType,
            ushort usBlackoutTime);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_RemoveBlackout(
            IntPtr HostPtr,
            uint ulDeviceID,
            ushort usManufactuerID,
            ushort usDeviceType);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_ClearBlackoutList(IntPtr HostPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_EnablePing(IntPtr HostPtr, bool bEnable);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSHost_Authenticate(
            IntPtr HostPtr,
            byte ucAuthenticationType,
            byte[] pucAuthString,
            byte ucLength,
            IntPtr pucResponseBuffer,   // Buffer allocated in unmanaged heap
            IntPtr pucResponseBufferSize, // Buffer size allocated in unmanaged heap
            uint ulResponseTimeut);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSHost_Download(
            IntPtr HostPtr,
            ushort usFileIndex,
            uint ulDataOffset,
            uint ulMaxDataLength,
            uint ulMaxBlockSize);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSHost_EraseData(IntPtr HostPtr, ushort usFileIndex);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSHost_Upload(
            IntPtr HostPtr,
            ushort usFileIndex,
            uint ulDataOffset,
            uint ulDataLength,
            IntPtr pvData,      // Pointer is cached by unmanaged code for use after this call has ended, so pointer needs to be pinned
            bool bForceOffset);


        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_GetEnabled(IntPtr HostPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSHost_GetChannelNumber(IntPtr HostPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSHost_GetStatus(IntPtr HostPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSHost_GetConnectedDeviceBeaconAntfsState(IntPtr HostPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_GetFoundDeviceParameters(
            IntPtr HostPtr,
            ref ANTFS_DeviceParameters pstDeviceParameters,
            [MarshalAs(UnmanagedType.LPArray)] byte[] pucFriendlyName,
            ref byte pucBufferSize);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_GetFoundDeviceChannelID(
            IntPtr HostPtr,
            ref ushort pusDeviceNumber,
            ref byte pucDeviceType,
            ref byte pucTransmitType);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_GetDownloadStatus(
            IntPtr HostPtr,
            ref uint pulByteProgress,
            ref uint pulTotalLength);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern uint ANTFSHost_GetTransferSize(IntPtr HostPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_GetTransferData(
            IntPtr HostPtr,
            ref uint pulDataSize,
            [MarshalAs(UnmanagedType.LPArray)] byte[] pucData);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_GetUploadStatus(
            IntPtr HostPtr,
            ref uint pulByteProgress,
            ref uint pulTotalLength);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSHost_GetVersion(
            IntPtr HostPtr,
            [MarshalAs(UnmanagedType.LPArray)] byte[] pucVersion,
            byte ucBufferSize);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSHost_WaitForResponse(IntPtr HostPtr, uint ulMilliseconds);

        #endregion

        #region Constructor, Finalizer & Dispose
        /// <summary>
        /// Creates ANT-FS object, response thread, and allocates unmanaged buffer for use with library
        /// </summary>
        public ANTFS_HostChannel(IANT_Channel channel_)
        {
            try
            {
                channel = channel_;

                // Create the ANT-FS host object
                if (ANTFSHost_New(ref unmanagedHostPtr) == 0)
                    throw new ANTFS_Exception("Unable to initialize ANT-FS Host");

                if (ANTFSHost_Init(unmanagedHostPtr, channel.getUnmgdFramer(), channel.getChannelNum()) == 0)
                    throw new ANTFS_Exception("Unable to initialize host");

                bInitialized = true;

                rawChannelResponseHandler = new dRawChannelResponseHandler( ( msg, size ) => ANTFSHost_ProcessMessage( unmanagedHostPtr, ref msg, size ) );
                deviceNotificationHandler = new dDeviceNotificationHandler( channel_DeviceNotification );

                channel.rawChannelResponse += rawChannelResponseHandler;
                channel.DeviceNotification += deviceNotificationHandler;

                // Allocate buffer to hold authentication response in native heap
                try
                {
                    this.unmanagedAuthBuffer = Marshal.AllocHGlobal(Byte.MaxValue);
                    this.unmanagedAuthSize = Marshal.AllocHGlobal(1);
                }
                catch (Exception ex)
                {
                    throw new ANTFS_Exception("Unable to initialize ANT-FS Host: " + ex.Message);
                }

                // Setup thread to handle responses from the ANT-FS library
                bResponseThreadExit = false;
                ResponseThread = new Thread(new ThreadStart(ResponseThreadHandler));
                ResponseThread.Name = "ANT-FS Host Response Thread";
                ResponseThread.IsBackground = true; // Make this a background thread so it will terminate when main program closes
                ResponseThread.Start();
            }
            catch
            {
                this.Dispose();
                throw;
            }
        }

        /// <summary>
        /// Destructor closes all opened resources
        /// </summary>
        ~ANTFS_HostChannel()
        {
            Dispose(false);
        }

        /// <summary>
        /// Dispose method for explicit resource cleanup
        /// </summary>
        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        /// <summary>
        /// Copy back the current configuration parameters
        /// </summary>
        public void GetCurrentConfig(ref ANTFS_ConfigParameters stConfigParamaters)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ANTFSHost_GetCurrentConfig(unmanagedHostPtr, ref stConfigParamaters);
        }

        /// <summary>
        /// Set current configuration parameters
        /// </summary>
        public bool SetCurrentConfig(ref ANTFS_ConfigParameters stConfigParamaters)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            return 0 != ANTFSHost_SetCurrentConfig(unmanagedHostPtr, ref stConfigParamaters);
        }

        /// <summary>
        /// Close connection, and release all resources
        /// </summary>
        protected virtual void Dispose(bool bDisposing)
        {
            if (bInitialized)
            {
                if (bDisposing)
                {
                    // Dispose of other managed resources here
                    channel.rawChannelResponse -= rawChannelResponseHandler;
                    channel.DeviceNotification -= deviceNotificationHandler;
                }

                // Terminate the response handler thread
                if (ResponseThread.IsAlive)
                {
                    bResponseThreadExit = true;
                    if (!ResponseThread.Join(3000))
                        ResponseThread.Abort();
                }

                if (unmanagedHostPtr != IntPtr.Zero)
                {
                    ANTFSHost_Cancel(unmanagedHostPtr); // Cancel any pending operations
                    ANTFSHost_Close(unmanagedHostPtr);  // Close object (incl. unmanaged thread)
                    ANTFSHost_Delete(unmanagedHostPtr); // Delete unmanaged object
                    unmanagedHostPtr = IntPtr.Zero;
                }

                // Free memory allocated in native heap
                if (unmanagedAuthBuffer != IntPtr.Zero)
                    Marshal.FreeHGlobal(unmanagedAuthBuffer);
                if (unmanagedAuthSize != IntPtr.Zero)
                    Marshal.FreeHGlobal(unmanagedAuthSize);

                // If upload buffer is still pinned, unpin it
                ReleaseUploadBuffer();

                bInitialized = false;
            }
        }

        /// <summary>
        /// Unpin and release buffer holding unmanaged data
        /// </summary>
        private void ReleaseUploadBuffer()
        {
            if (ghUploadData != null)
            {
                ghUploadData.Value.Free();
                ghUploadData = null;
            }
        }

        #endregion

        #region ANT-FS Host Functions

        void channel_DeviceNotification(ANT_Device.DeviceNotificationCode notification, object notificationInfo)
        {
            ANTFSHost_ProcessDeviceNotification(unmanagedHostPtr, (byte) notification, IntPtr.Zero);
            //TODO Once we define an object for a notification type, marshal it - pin it, etc
        }

        /// <summary>
        /// Set the channel ID of the ANT-FS host
        /// If this function is not used to explicitly configure the channel ID, the ANT-FS host will use the following defaults:
        /// Device type: 1
        /// Transmission type: 5
        /// </summary>
        /// <param name="ucDeviceType">Device type to assign to channel (ANT Channel ID). Set to 0 for receiver wild card matching</param>
        /// <param name="ucTransmissionType">Transmission type to assign to channel (ANT Channel ID).  Set to 0 for receiver wild card matching</param>
        public void SetChannelID(byte ucDeviceType, byte ucTransmissionType)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ANTFSHost_SetChannelID(unmanagedHostPtr, ucDeviceType, ucTransmissionType);
        }

        /// <summary>
        /// Set the channel period of the ANT-FS host.
        /// If this function is not used to explicitly configure the channel period, the ANT-FS host will use the default value of 8Hz
        /// </summary>
        /// <param name="usChannelPeriod">Desired period in seconds * 32768</param>
        public void SetChannelPeriod(ushort usChannelPeriod)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ANTFSHost_SetChannelPeriod(unmanagedHostPtr, usChannelPeriod);
        }

        /// <summary>
        /// Set the network key for the ANT-FS host
        /// If this function is not used to explicitly configure the network key, the ANT-FS host will use the
        /// ANT-FS network key, as set in the base ANT Library
        /// </summary>
        /// <param name="netNumber">network number</param>
        /// <param name="networkKey">8-byte network key</param>
        public void SetNetworkKey(byte netNumber, byte[] networkKey)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            if (networkKey.Length != 8)
                throw new ANTFS_Exception("Network key must be 8 bytes");

            networkNumber = netNumber;
            ANTFSHost_SetNetworkKey(unmanagedHostPtr, networkNumber, networkKey);
        }

        /// <summary>
        /// Sets the value for the proximity bin setting for searching.
        /// If applying this value fails when attempting to start search,
        /// it is ignored to maintain compatibility with devices that
        /// do not support this feature. This means that a real failure can occur
        /// on a device that actually does support it, and it will be missed. The
        /// debug log will show if this command fails.
        /// </summary>
        /// <param name="searchThreshold">Desired proximity bin from 0-10</param>
        public void SetProximitySearch(byte searchThreshold)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

           if(searchThreshold > 10)
              throw new ANTFS_Exception("Proximity search bin must be between 0 - 10");

           ANTFSHost_SetProximitySearch(unmanagedHostPtr, searchThreshold);
        }


        /// <summary>
        /// Sets the serial number of the host device.
        /// </summary>
        /// <param name="serialNumber">4-byte host serial number</param>
        public void SetSerialNumber(uint serialNumber)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ANTFSHost_SetSerialNumber(unmanagedHostPtr, serialNumber);
        }

        /// <summary>
        /// Clears the internal search device list
        /// </summary>
        public void ClearSearchDeviceList()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ANTFSHost_ClearSearchDeviceList(unmanagedHostPtr);
        }

        /// <overloads>
        /// Adds a set of parameters for which to search to the internal search device list
        /// </overloads>
        /// <summary>
        /// Adds a set of parameters for which to search to the internal search device list, explicitly specifying
        /// all parameters and search mask.
        /// </summary>
        /// <param name="stSearchMask">Device parameter search mask. Set a member to zero (0) to wildcard search for it.
        /// Otherwise, set the bits that you want to be matched to 1 in each member.
        /// Note that the default search masks or wildcards should normally be applied to the ucStatusByte1 and ucStatusByte2
        /// members of the search mask. Setting bits outside the masks, specially reserved bits, may lead to undesired
        /// behavior.</param>
        /// <param name="stDeviceParameters">Device Parameters to include in a search.  Set the member to the desired search value.
        /// A member in this structure is ignored if the associated member in the Search Mask is set to zero (0) for wildcard.</param>
        /// <returns>A handle to the search device entry.  If the return value is zero (0), the function failed adding the device entry.
        /// This means that the device list is already full</returns>
        public ushort AddSearchDevice(ref ANTFS_DeviceParameters stSearchMask, ref ANTFS_DeviceParameters stDeviceParameters)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            return(ANTFSHost_AddSearchDevice(unmanagedHostPtr, ref stSearchMask, ref stDeviceParameters));
        }

        /// <summary>
        /// Adds a set of parameters for which to search to the internal search device list, matching only the Device ID,
        /// Manufacturer ID and Device Type
        /// </summary>
        /// <param name="ulDeviceID">Device ID to match</param>
        /// <param name="usManufacturerID">Manufacturer ID to match</param>
        /// <param name="usDeviceType">Device type to match</param>
        /// <returns>A handle to the search device entry.  If the return value is zero (0), the function failed adding the device entry.
        /// This means that the device list is already full</returns>
        public ushort AddSearchDevice(uint ulDeviceID, ushort usManufacturerID, ushort usDeviceType)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ANTFS_DeviceParameters stDeviceParams;
            ANTFS_DeviceParameters stSearchMask;

            stSearchMask.DeviceID = (ulDeviceID != 0) ? (uint) 0xFFFFFFFF : (uint) 0;     // Match entire Device ID, if set
            stSearchMask.ManufacturerID = (usManufacturerID != 0) ? (ushort) 0xFFFF : (ushort) 0;   // Match entire Mfg ID, if set
            stSearchMask.DeviceType = (usDeviceType != 0) ? (ushort) 0xFFFF : (ushort) 0;       // Match entire Device Type, if set
            stSearchMask.AuthenticationType = 0;    // Ignore auth type
            stSearchMask.StatusByte1 = 0;           // Ignore status 1 byte
            stSearchMask.StatusByte2 = 0;           // Ignore status 2 byte

            // TODO: Fix wildcards
            stDeviceParams.DeviceID = ulDeviceID;
            stDeviceParams.ManufacturerID = usManufacturerID;
            stDeviceParams.DeviceType = usDeviceType;
            stDeviceParams.AuthenticationType = 0;
            stDeviceParams.StatusByte1 = 0;
            stDeviceParams.StatusByte2 = 0;

            return (ANTFSHost_AddSearchDevice(unmanagedHostPtr, ref stSearchMask, ref stDeviceParams));
        }

        /// <summary>
        /// Removes a device entry from the internal search list
        /// </summary>
        /// <param name="usDeviceHandle">Handle to the device entry to be removed from the list</param>
        public void RemoveSearchDevice(ushort usDeviceHandle)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ANTFSHost_RemoveSearchDevice(unmanagedHostPtr, usDeviceHandle);
        }

        /// <summary>
        /// Requests an ANT-FS session, from an already connected broadcast device.
        /// </summary>
        /// <param name="ucBroadcastRadioFrequency">The frequency of currently connected device
        /// This frequency is calculated as (ucSearchRadioFrequency_ * 1 MHz + 2400 MHz).</param>
        /// <param name="ucConnectRadioFrequency">The frequency on which the connection communication will occur.
        /// This frequency is calculated as (ucSearchRadioFrequency_ * 1 MHz + 2400 MHz).</param>
        public void RequestSession(byte ucBroadcastRadioFrequency, byte ucConnectRadioFrequency)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ReturnCode theReturn = (ReturnCode)ANTFSHost_RequestSession(unmanagedHostPtr, ucBroadcastRadioFrequency, ucConnectRadioFrequency);
            if (theReturn != ReturnCode.Pass)
                throw new ANTFS_RequestFailed_Exception("RequestSession", theReturn);
        }

        /// <overloads>
        /// Begins a search for ANT-FS remote devices
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </overloads>
        /// <summary>
        /// Begins a search for ANT-FS remote devices
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </summary>
        /// <param name="ucSearchRadioFrequency">The frequency on which to search for devices.
        /// This frequency is calculated as (ucSearchRadioFrequency_ * 1 MHz + 2400 MHz).</param>
        /// <param name="ucConnectRadioFrequency">The frequency on which the connection communication will occur.
        /// This frequency is calculated as (ucSearchRadioFrequency_ * 1 MHz + 2400 MHz)</param>
        /// <param name="usRadioChannelID">Device number to assign to channel (ANT Channel ID)</param>
        /// <param name="bUseRequestPage">Selects whether to search for broadcast devices, and request the beacon</param>
        public void SearchForDevice(byte ucSearchRadioFrequency, byte ucConnectRadioFrequency, ushort usRadioChannelID, bool bUseRequestPage)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

           ReturnCode theReturn =  (ReturnCode) ANTFSHost_SearchForDevice(unmanagedHostPtr, ucSearchRadioFrequency, ucConnectRadioFrequency, usRadioChannelID, bUseRequestPage);
           if (theReturn != ReturnCode.Pass)
           {
               throw new ANTFS_RequestFailed_Exception("Search", theReturn);
           }
        }

        /// <summary>
        /// Begins a search for ANT-FS remote devices.  The search will continue until a device is found,
        /// the Cancel() function is called, an error occurs, or the library is closed.
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </summary>
        /// <param name="ucSearchRadioFrequency">The frequency on which to search for devices.
        /// This frequency is calculated as (ucSearchRadioFrequency_ * 1 MHz + 2400 MHz).</param>
        /// <param name="ucConnectRadioFrequency">The frequency on which the connection communication will occur.
        /// This frequency is calculated as (ucSearchRadioFrequency_ * 1 MHz + 2400 MHz)</param>
        /// <param name="usRadioChannelID">Device number to assign to channel (ANT Channel ID)</param>
        public void SearchForDevice(byte ucSearchRadioFrequency, byte ucConnectRadioFrequency, ushort usRadioChannelID)
        {
            SearchForDevice(ucSearchRadioFrequency, ucConnectRadioFrequency, usRadioChannelID, false);
        }

        /// <summary>
        /// Begins a search for ANT-FS remote devices, using the default ANT-FS search frequency (2.450GHz) and
        /// an adaptive frequency hopping scheme when the connection is established.
        /// The host will continue to search for devices until a device is found, the Cancel() function is called,
        /// an error occurs, or the library is closed.
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </summary>
        public void SearchForDevice()
        {
            SearchForDevice((byte)RadioFrequency.ANTFSNetwork, (byte) RadioFrequency.Auto, 0, false);
        }


        /// <overloads>
        /// Disconnect from a remote device, going back to the state specified by the disconnect type.
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </overloads>
        /// <summary>
        /// Disconnect from a remote device.  Optionally put that device on a blackout
        /// list for a period of time
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </summary>
        /// <param name="usBlackoutTime">Number of seconds the device ID should remain on
        /// the blackout list. If set to BlackoutTime.None, the device is not put in a blackout list.
        /// If set to BlackoutTime.Infinite, the device will remain in the list until explicitly
        /// removed or until the blackout list is reset</param>
        /// <param name="disconnectType">Disconnect Command type</param>
        /// <param name="undiscoverableTimeDuration">Time, in 30 seconds increments, the client
        /// device will remain undiscoverable after disconnect has been requested. Set to 0 to disable.</param>
        /// <param name="undiscoverableAppSpecificDuration">Application specific duration the client
        /// shall remain undiscoverable after disconnection</param>
        public void Disconnect(ushort usBlackoutTime, byte disconnectType, byte undiscoverableTimeDuration, byte undiscoverableAppSpecificDuration)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ReturnCode theReturn = (ReturnCode) ANTFSHost_Disconnect(unmanagedHostPtr, usBlackoutTime, (byte) disconnectType, undiscoverableTimeDuration, undiscoverableAppSpecificDuration);
            if (theReturn != ReturnCode.Pass)
                throw new ANTFS_RequestFailed_Exception("Disconnect", theReturn);
        }

        /// <summary>
        /// Disconnect from a remote device, without putting it in the blackout list
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </summary>
        public void Disconnect()
        {
            Disconnect((ushort) BlackoutTime.None, 0, 0, 0);
        }

        /// <summary>
        /// Request the connected remote device to switch to the specified
        /// radio frequency and channel period
        /// </summary>
        /// <param name="ucRadioFrequency">New radio frequency</param>
        /// <param name="beaconPeriod">New beacon period</param>
        public void SwitchFrequency(byte ucRadioFrequency, BeaconPeriod beaconPeriod)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ReturnCode theReturn = (ReturnCode)ANTFSHost_SwitchFrequency(unmanagedHostPtr, ucRadioFrequency, (byte) beaconPeriod);
            if (theReturn != ReturnCode.Pass)
                throw new ANTFS_RequestFailed_Exception("SwitchFrequency", theReturn);
        }

        /// <summary>
        /// Puts the device on a blackout list for a period of time.
        /// A device in the blackout list will not show up in any search results.
        /// A wildcard parameter (0) is not allowed for any of the device ID parameters.
        /// An exception is thrown if the device can not be added to the blackout list,
        /// either because the list is full, or the device ID is invalid
        /// </summary>
        /// <param name="ulDeviceID">The device ID of a specific device</param>
        /// <param name="usManufacturerID">The specific manufacturer ID</param>
        /// <param name="usDeviceType">The specific device type</param>
        /// <param name="usBlackoutTime">Number of seconds the device ID should remain on
        /// the blackout list. If set to BlackoutTime.None, the device is not put in a blackout list.
        /// If set to BlackoutTime.Infinite, the device will remain in the list until explicitly
        /// removed or until the blackout list is reset</param>
        public void Blackout(uint ulDeviceID, ushort usManufacturerID, ushort usDeviceType, ushort usBlackoutTime)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            if ((ulDeviceID == 0) || (usManufacturerID == 0) || (usDeviceType == 0))
                throw new ANTFS_RequestFailed_Exception("Unable to add device to blackout list, wildcards not allowed");
            if (ANTFSHost_Blackout(unmanagedHostPtr, ulDeviceID, usManufacturerID, usDeviceType, usBlackoutTime) == 0)
                throw new ANTFS_RequestFailed_Exception("Unable to add device to blackout list, list is full");    // List size is 2048, so this should be very rare
        }

        /// <summary>
        /// Remove the device from the blackout list
        /// A wildcard parameter (0) is not allowed for any of the device ID parameters,
        /// and will result in returning False.
        /// Throws an exception if the device can not be removed from the list (e.g. device
        /// was not in list)
        /// </summary>
        /// <param name="ulDeviceID">The device ID of a specific device</param>
        /// <param name="usManufacturerID">The specific manufacturer ID</param>
        /// <param name="usDeviceType">The specific device type</param>
        public void RemoveBlackout(uint ulDeviceID, ushort usManufacturerID, ushort usDeviceType)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            if (ANTFSHost_RemoveBlackout(unmanagedHostPtr, ulDeviceID, usManufacturerID, usDeviceType) == 0)
                throw new ANTFS_RequestFailed_Exception("Unable to remove device from blackout list");
        }

        /// <summary>
        /// Clears the blackout list
        /// </summary>
        public void ClearBlackoutList()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ANTFSHost_ClearBlackoutList(unmanagedHostPtr);
        }

        /// <summary>
        /// Enables ping message to be sent to the remote device periodically.
        /// This can be used to keep the remote device from timing out during operations
        /// that wait for user input.
        /// </summary>
        /// <param name="bEnable">Periodic ping enable</param>
        public void EnablePing(bool bEnable)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ANTFSHost_EnablePing(unmanagedHostPtr, bEnable);
        }

        /// <overloads>
        /// Requests to pair with the connected remote device
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </overloads>
        /// <summary>
        /// Request to pair with the connected remote device
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </summary>
        /// <param name="authType">The type of authentication to execute on the remote device</param>
        /// <param name="authString">String to be used in conjunction with the particular authentication type (e.g. passkey)</param>
        /// <param name="ulTimeout">Number of miliseconds to wait for a response after the authenticate command is set</param>
        public void Authenticate(AuthenticationType authType, byte[] authString, uint ulTimeout)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            Marshal.WriteByte(unmanagedAuthSize, Byte.MaxValue);   // Initialize size to max value
            ReturnCode theReturn = (ReturnCode) ANTFSHost_Authenticate(unmanagedHostPtr, (byte)authType, authString, (byte)authString.Length, unmanagedAuthBuffer, unmanagedAuthSize, ulTimeout);
            if (theReturn != ReturnCode.Pass)
                throw new ANTFS_RequestFailed_Exception("Authentication", theReturn);
        }

        /// <summary>
        /// Request to pair with the connected remote device
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </summary>
        /// <param name="authType">The type of authentication to execute on the remote device</param>
        /// <param name="authString">String to be used in conjunction with the particular authentication type (e.g. friendly name)</param>
        /// <param name="ulTimeout">Number of miliseconds to wait for a response after the authenticate command is sent</param>
        public void Authenticate(AuthenticationType authType, string authString, uint ulTimeout)
        {
            byte[] myFriendlyName = Common.ConvertToByteArray(authString);
            Authenticate(authType, myFriendlyName, ulTimeout);
        }

        /// <summary>
        /// Request to pair with the connected remote device, without specifying an authentication string
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </summary>
        /// <param name="authType">The type of authentication to execute on the remote device</param>
        /// <param name="ulTimeout">Number of miliseconds to wait for a response after the authenticate command is sent</param>
        public void Authenticate(AuthenticationType authType, uint ulTimeout)
        {
            byte[] emptyString = new byte[0];
            Authenticate(authType, emptyString, ulTimeout);
        }

        /// <overloads>
        /// Request a download of a file from the authenticated device
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </overloads>
        /// <summary>
        /// Request a download of a file from the authenticated device
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </summary>
        /// <param name="usFileIndex">The file number to be downloaded</param>
        /// <param name="ulDataOffset">Byte offset from where to begin transferring the data</param>
        /// <param name="ulMaxDataLength">Maximum number of bytes to download.  Set to zero (0) if
        /// the host does not wish to limit the total size of the download</param>
        /// <param name="ulMaxBlockSize">Maximum number of bytes to download in a single block. Set
        /// to zero (0) if the host does not wish to limit the block size</param>
        public void Download(ushort usFileIndex, uint ulDataOffset, uint ulMaxDataLength, uint ulMaxBlockSize)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ReturnCode theReturn = (ReturnCode) ANTFSHost_Download(unmanagedHostPtr, usFileIndex, ulDataOffset, ulMaxDataLength, ulMaxBlockSize);
            if (theReturn == ReturnCode.Pass)
                usCurrentTransfer = usFileIndex;    // Track current transfer
            else
                throw new ANTFS_RequestFailed_Exception("Download", theReturn);
        }

        /// <summary>
        /// Request a download of a file from the authenticated device
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </summary>
        /// <param name="usFileIndex">The file number to be downloaded</param>
        /// <param name="ulDataOffset">Byte offset from where to begin transferring the data</param>
        /// <param name="ulMaxDataLength">Maximum number of bytes to download.  Set to zero (0) if
        /// the host does not wish to limit the total size of the download</param>
        public void Download(ushort usFileIndex, uint ulDataOffset, uint ulMaxDataLength)
        {
            Download(usFileIndex, ulDataOffset, ulMaxDataLength, 0);
        }

        /// <summary>
        /// Requests a download of the directory file from the authenticated device
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </summary>
        public void DownloadDirectory()
        {
           Download(0, 0, 0, 0);
        }

        /// <summary>
        /// Requests the erasure of a file on the authenticated remote device
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </summary>
        /// <param name="usFileIndex">The file number of the file to erase</param>
        public void EraseData(ushort usFileIndex)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ReturnCode theReturn = (ReturnCode) ANTFSHost_EraseData(unmanagedHostPtr, usFileIndex);
            if (theReturn == ReturnCode.Pass)
                usCurrentTransfer = usFileIndex;
            else
                throw new ANTFS_RequestFailed_Exception("Erase", theReturn);
        }

        /// <overloads>
        /// Requests an upload of a file to the authenticated device
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </overloads>
        /// <summary>
        /// Requests an upload of a file to the authenticated device
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </summary>
        /// <param name="usFileIndex">The file number to be uploaded</param>
        /// <param name="ulDataOffset">The byte offset from where to begin transferring the data</param>
        /// <param name="uploadData">Buffer where data to be sent is stored</param>
        /// <param name="bForceOffset">Force the offset</param>
        public void Upload(ushort usFileIndex, uint ulDataOffset, byte[] uploadData, bool bForceOffset)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ReleaseUploadBuffer();    // If we had not freed the handle, do it now
            ghUploadData = GCHandle.Alloc(uploadData, GCHandleType.Pinned);    // Pin managed buffer
            IntPtr pvData = ghUploadData.Value.AddrOfPinnedObject(); // Get address of managed buffer

            if (pvData == IntPtr.Zero)
            {
                throw new ANTFS_Exception("Upload request failed: Unable to allocate response buffer");
            }
            else
            {
                ReturnCode theReturn = (ReturnCode)ANTFSHost_Upload(unmanagedHostPtr, usFileIndex, ulDataOffset, (uint)uploadData.Length, pvData, bForceOffset);

                if (theReturn == ReturnCode.Pass)
                {
                    usCurrentTransfer = usFileIndex;    // Track current transfer
                }
                else
                {
                    // If the request was not successful, we should free the handle here
                    // Otherwise, buffer needs to remain pinned while upload completes (until UploadPass, UploadFail ...)
                    // GCHandle will be freed when we receive an ANT-FS response, or the application is terminated
                    ReleaseUploadBuffer();
                    throw new ANTFS_RequestFailed_Exception("Upload", theReturn);
                }
            }
        }

        /// <summary>
        /// Requests a new upload of a file to the authenticated device
        /// Throws an exception if the library is in the wrong state or busy with another request
        /// </summary>
        /// <param name="usFileIndex">The file number to be uploaded</param>
        /// <param name="uploadData">Buffer where data to be sent is stored</param>
        public void Upload(ushort usFileIndex, byte[] uploadData)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            Upload(usFileIndex, 0, uploadData, true);
        }


        /// <summary>
        /// Retrieves the parameters of the most recently found device
        /// </summary>
        /// <returns>Parameters of the most recently found device, or null if no parameters could be retrieved</returns>
        public ANTFS_SearchResults GetFoundDeviceParameters()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ANTFS_SearchResults searchResults = new ANTFS_SearchResults();

            byte[] aucFriendlyName = new byte[Byte.MaxValue];
            byte ucSize = (byte) aucFriendlyName.Length;

            if (ANTFSHost_GetFoundDeviceParameters(unmanagedHostPtr, ref searchResults.DeviceParameters, aucFriendlyName, ref ucSize) == 0)
                return null;

            if (ucSize != 0)
                searchResults.FriendlyName = Common.ConvertToString(aucFriendlyName);

            return searchResults;
        }

        /// <summary>
        /// Retrieves the channel ID of the most recently found device.
        /// An exception will be thrown if no device has been found
        /// </summary>
        /// <returns>The Channel ID of the most recently found device.</returns>
        public ANT_ChannelID GetFoundDeviceChannelID()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

           ushort foundDeviceNumber = 0;
           byte foundDeviceType = 0;
           byte foundTransmissionType = 0;

           if (ANTFSHost_GetFoundDeviceChannelID(unmanagedHostPtr, ref foundDeviceNumber, ref foundDeviceType, ref foundTransmissionType) == 0)
              throw new ANTFS_Exception("Unable to retrieve the channel ID, no device has been found yet");

           return new ANT_ChannelID(foundDeviceNumber, foundDeviceType, foundTransmissionType);
        }

        /// <summary>
        /// Obtains the additional data received on an Authentication Response (e.g. PassKey), if available
        /// </summary>
        /// <returns>Authentication response additional parameters, or an empty array if no additional parameters were received</returns>
        public byte[] GetAuthResponse()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            if (unmanagedAuthSize != IntPtr.Zero)
            {
                byte ucSize = Marshal.ReadByte(unmanagedAuthSize);  // Retrieve size from native heap
                byte[] authString = new byte[ucSize];

                if (ucSize > 0)
                    Marshal.Copy(unmanagedAuthBuffer, authString, 0, ucSize);   // Retrieve buffer contents from native heap
                return authString;
            }
            else
            {
                return new byte[0];
            }
        }

        /// <summary>
        /// Gets the transfer progress of a pending or complete download
        /// </summary>
        /// <returns>The transfer status, including the current byte progress,
        /// total expected length of the download, and current percentage.
        /// Returns null if no valid status could be obtained.</returns>
        public TransferStatus GetDownloadStatus()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            uint ulProgress = 0;
            uint ulLength = 0;

            if (ANTFSHost_GetDownloadStatus(unmanagedHostPtr, ref ulProgress, ref ulLength) != 0)
            {
                return new TransferStatus(ulProgress, ulLength);
            }

            else
                return null;
        }

        /// <summary>
        /// Gets the transfer progress of a pending or complete upload
        /// </summary>
        /// <returns>The transfer status, including the current byte progress,
        /// total expected length of the upload, and current percentage.
        /// Returns null if no valid status could be obtained.</returns>
        public TransferStatus GetUploadStatus()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            uint ulProgress = 0;
            uint ulLength = 0;

            if (ANTFSHost_GetUploadStatus(unmanagedHostPtr, ref ulProgress, ref ulLength) != 0)
            {
                return new TransferStatus(ulProgress, ulLength);
            }

            else
                return null;
        }

        /// <summary>
        /// Gets the received data from a transfer (download)
        /// </summary>
        /// <returns>Buffer containing the downloaded data.
        /// Returns an empty buffer if no data is available or the download size is 0</returns>
        public byte[] GetTransferData()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            uint ulDataSize = GetDownloadSize();    // Get the size of the latest download
            byte[] returnData = new byte[ulDataSize];   // Create buffer
            if(ulDataSize > 0)  // If there is data to retrieve
            {
                if (ANTFSHost_GetTransferData(unmanagedHostPtr, ref ulDataSize, returnData) == 0)
                    return new byte[0];  // Transfer failed, return empty buffer

                Array.Resize(ref returnData, (int) ulDataSize); // Update buffer size
            }
            return returnData;
        }

        /// <summary>
        /// Retrieves the size of a downloaded file
        /// </summary>
        /// <returns>Download size (in bytes)</returns>
        public uint GetDownloadSize()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            return ANTFSHost_GetTransferSize(unmanagedHostPtr);
        }

        /// <summary>
        /// Returns the current library status
        /// </summary>
        /// <returns>Current library status</returns>
        public State GetStatus()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            return (State) ANTFSHost_GetStatus(unmanagedHostPtr);
        }

        /// <summary>
        /// Returns the current ANTFS state from the last seen ANTFS beacon of the
        /// connected device.
        /// </summary>
        /// <returns>Ant-Fs State from last received beacon or BeaconNotFound if no device is connected</returns>
        public BeaconAntFsState GetConnectedDeviceBeaconAntfsState()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            return (BeaconAntFsState)ANTFSHost_GetConnectedDeviceBeaconAntfsState(unmanagedHostPtr);
        }

        /// <summary>
        /// Gets the version string of the underlying ANT-FS library
        /// </summary>
        /// <returns>ANT-FS Library Version String</returns>
        public string GetLibraryVersion()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            byte[] returnVersion = new byte[Byte.MaxValue];   // maximum size
            if (ANTFSHost_GetVersion(unmanagedHostPtr, returnVersion, (byte) returnVersion.Length) == 0)
                return "";  // blank string
            else
                return (Common.ConvertToString(returnVersion));
        }

        /// <summary>
        /// Cancels any pending actions and returns the library to the appropiate ANT-FS layer if possible,
        /// i.e. if the library was executing a download command in the transport layer, the library would
        /// be returned to the Transport State.
        /// A response of CancelDone will be sent back to the application when the cancel operation is complete.
        /// </summary>
        public void Cancel()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSHost object has been disposed");

            ANTFSHost_Cancel(unmanagedHostPtr);
        }


        #endregion

        #region Private Methods

        /// <summary>
        /// Processing of incoming ANT-FS responses
        /// </summary>
        private void ResponseThreadHandler()
        {
            bResponseThreadExit = false;

            Response eThreadResponse = Response.None;
            while (!bResponseThreadExit)
            {
                eThreadResponse = (Response) ANTFSHost_WaitForResponse(unmanagedHostPtr, 999);

                if (eThreadResponse != Response.None)    // We got a response
                {
                    // If we were processing an upload, release handle
                    ReleaseUploadBuffer();
                    lock (onResponseLock)
                    {
                        // Dispatch response if we got one
                        if (onResponse_internal != null)
                        {
                            onResponse_internal(eThreadResponse);
                        }
                    }
                }
            }
        }

        #endregion
    }
}
