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
    /// Control class for an ANT-FS client channel.
    /// Handles creating channels and device setup, as well as the ANT-FS client implementation.
    /// </summary>
    public class ANTFS_ClientChannel : IDisposable
    {
        #region Public Definitions

#pragma warning disable 1591
        /// <summary>
        /// ANT-FS Client Responses
        /// </summary>
        /// !!! Must match the enum in antfs_client_channel.hpp
        public enum Response : byte
        {
            [Description("No response")]
            None = 0,
            [Description("Idle")]
            InitPass,
            [Description("Error: Serial failure")]
            SerialFail,
            [Description("Beacon opened successfully")]
            BeaconOpen,
            [Description("Beacon closed successfully")]
            BeaconClosed,
            [Description("Connected to remote device")]
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
            [Description("Pairing request received")]
            PairingRequest,
            [Description("Pairing request timed out")]
            PairingTimeout,
            [Description("Download requested")]
            DownloadRequest,
            [Description("Download complete")]
            DownloadPass,
            [Description("Download rejected")]
            DownloadReject,
            [Description("Download failed, invalid index requested")]
            DownloadInvalidIndex,
            [Description("Download failed, file not readable")]
            DownloadFileNotReadable,
            [Description("Download failed, not ready")]
            DownloadNotReady,
            [Description("Download failed")]
            DownloadFail,
            [Description("Upload requested")]
            UploadRequest,
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
            [Description("Erase requested")]
            EraseRequest,
            [Description("Erased data")]
            ErasePass,
            [Description("Erase rejected")]
            EraseReject,
            [Description("Erase failed")]
            EraseFail,
            [Description("Request canceled")]
            CancelDone
        };

        /// <summary>
        /// ANT-FS Client State
        /// </summary>
        /// !!! Must match the enum in antfs_client_channel.hpp
        public enum State : byte
        {
            [Description("Off, state machine not initialized")]
            Off = 0,
            [Description("ANT-FS Client Idle")]
            Idle,
            [Description("Opening beacon")]
            Opening,
            [Description("Disconnecting")]
            Disconnecting,
            [Description("Beaconing")]  // LINK
            Beaconing,
            [Description("Connected")]  // AUTH
            Connected,
            [Description("Authenticating")]
            Authenticating,
            [Description("Authenticating, waiting for response to pairing request")]
            WaitingForPairingResponse,
            [Description("Transport")]
            Transport,
            [Description("Downloading data")]
            Downloading,
            [Description("Downloading, waiting for data to send")]
            WaitingForDownloadData,
            [Description("Uploading data")]
            Uploading,
            [Description("Waiting for response to an upload request")]
            WaitingForUploadResponse,
            [Description("Erasing data")]
            Erasing
        };

#pragma warning restore 1591

        #endregion

        #region Variables
        bool bInitialized;
        bool bResponseThreadExit;   // Flag response thread to exit
        Thread ResponseThread;      // This thread is in charge of monitoring the device for responses and forwarding them to the appropiate handler

        Object syncToken = new object();

        ushort? usCurrentTransfer = null;           // Current transfer being processed

        IntPtr unmanagedClientPtr = IntPtr.Zero;      // Holder for the pointer to unmanaged ANTFSClientChannel object
        GCHandle? ghDownloadData = null;              // Handle to allow pinning the managed download buffer

        byte networkNumber = 0;     // ANT network number used for the ANT-FS client

        IANT_Channel channel;       // ANT channel ClientChannel is using
        dRawChannelResponseHandler rawChannelResponseHandler;     // ANT channel response handler
        dDeviceNotificationHandler deviceNotificationHandler;     // ANT device notification handler

        #endregion


        #region Events

        /// <summary>
        /// The ANT-FS host callback event, triggered every time a response is received from the ANT-FS client library
        /// </summary>
        public event Action<Response> OnResponse;

        #endregion

        #region DllImports

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_New(ref IntPtr ClientPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_Init(IntPtr ClientPtr, IntPtr FramerPtr, byte ucANTChannel);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_Close(IntPtr ClientPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_Delete(IntPtr ClientPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_Cancel(IntPtr ClientPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_ProcessDeviceNotification(
            IntPtr ClientPtr,
            byte ucCode,
            IntPtr pvParameter);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_ConfigureClientParameters(
            IntPtr ClientPtr,
            ref ANTFS_ClientParameters pstInitParams);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_SetPairingEnabled(IntPtr ClientPtr, bool bEnable);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_SetUploadEnabled(IntPtr ClientPtr, bool bEnable);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_SetDataAvailable(IntPtr ClientPtr, bool bDataAvailable);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_SetBeaconTimeout(IntPtr ClientPtr, byte ucTimeout);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_SetPairingTimeout(IntPtr ClientPtr, byte ucTimeout);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_SetFriendlyName(
            IntPtr ClientPtr,
            [MarshalAs(UnmanagedType.LPArray)] byte[] pucFriendlyName,
            byte ucFriendlyNameSize);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_SetPassKey(
            IntPtr ClientPtr,
            [MarshalAs(UnmanagedType.LPArray)] byte[] pucPassKey,
            byte ucPassKeySize);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_SetChannelID(IntPtr ClientPtr, byte ucDeviceType, byte ucTransmissionType);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_SetChannelPeriod(IntPtr ClientPtr, ushort usChannelPeriod);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_SetNetworkKey(IntPtr ClientPtr, byte networkNumber, byte[] pucKey);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_SetTxPower(IntPtr ClientPtr, byte ucPairingLv, byte ucConnectedLv);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_OpenBeacon(IntPtr ClientPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_CloseBeacon(IntPtr ClientPtr, bool bReturnToBroadcast);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_ProcessMessage(IntPtr ClientPtr, ref ANT_Device.ANTMessage pstANTMessage, ushort usMessageSize);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_SendPairingResponse(IntPtr ClientPtr, bool bAccept);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_SendDownloadResponse(
            IntPtr ClientPtr,
            byte ucResponse,
            ref ANTFS_DownloadParameters pstDownloadInfo,
            uint ulDataLength,
            IntPtr pvData);      // Pointer is cached by unmanaged code for use after this call has ended, so pointer needs to be pinned

        // TODO: Add UploadResponse
        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_SendUploadResponse(
            IntPtr ClientPtr,
            byte ucResponse,
            ref ANTFS_UploadParameters pstUploadInfo,
            uint ulDataLength,
            IntPtr pvData);      // Pointer is cached by unmanaged code for use after this call has ended, so pointer needs to be pinned


        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_SendEraseResponse(IntPtr ClientPtr, byte ucResponse);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_GetEnabled(IntPtr ClientPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_GetChannelNumber(IntPtr ClientPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_GetVersion(
            [MarshalAs(UnmanagedType.LPArray)] byte[] pucVersion,
            byte ucBufferSize);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_GetStatus(IntPtr ClientPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANTFSClient_GetHostName(
            IntPtr ClientPtr,
            [MarshalAs(UnmanagedType.LPArray)] byte[] pucFriendlyName,
            ref byte pucBufferSize);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_GetRequestParameters(
            IntPtr ClientPtr,
            ref ANTFS_RequestParameters pstRequestParams);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_GetRequestedFileIndex(
            IntPtr ClientPtr,
            ref ushort pusIndex);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_GetDownloadStatus(
            IntPtr ClientPtr,
            ref uint pulByteProgress,
            ref uint pulTotalLength);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern uint ANTFSClient_GetTransferSize(IntPtr ClientPtr);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_GetTransferData(
            IntPtr ClientPtr,
            ref ulong pulOffset,
            ref ulong pulDataSize,
            [MarshalAs(UnmanagedType.LPArray)] byte[] pucData);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_GetDisconnectParameters(
            IntPtr ClientPtr,
            ref ANTFS_DisconnectParameters pstDisconnectParams);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANTFSClient_WaitForResponse(IntPtr ClientPtr, uint ulMilliseconds);

        #endregion

        #region Constructor, Finalizer & Dispose

        /// <summary>
        /// Constructor attempts to automatically detect an ANT USB device and open a connection to it
        /// </summary>
        public ANTFS_ClientChannel(IANT_Channel channel_)
        {
            try
            {
                channel = channel_;

                // Create the ANT-FS client object
                if (ANTFSClient_New(ref unmanagedClientPtr) == 0)
                    throw new ANTFS_Exception("Unable to initialize ANT-FS Client");

                if (ANTFSClient_Init(unmanagedClientPtr, channel.getUnmgdFramer(), channel.getChannelNum()) == 0)
                    throw new ANTFS_Exception("Unable to initialize client");

                rawChannelResponseHandler += new dRawChannelResponseHandler((msg,size) => ANTFSClient_ProcessMessage(unmanagedClientPtr, ref msg,size));
                deviceNotificationHandler += new dDeviceNotificationHandler(channel_DeviceNotification);

                channel.rawChannelResponse += rawChannelResponseHandler;
                channel.DeviceNotification += deviceNotificationHandler;

                // Setup thread to handle responses from the ANT-FS library
                bResponseThreadExit = false;
                ResponseThread = new Thread(new ThreadStart(ResponseThreadHandler));
                ResponseThread.Name = "ANT-FS Client Response Thread";
                ResponseThread.IsBackground = true; // Make this a background thread so it will terminate when main program closes
                ResponseThread.Start();

                bInitialized = true;
            }
            catch (Exception)    // Catch so we can cleanup
            {
                this.Dispose();
                throw;   // Forward the error to the caller
            }
        }

        /// <summary>
        /// Destructor closes all opened resources
        /// </summary>
        ~ANTFS_ClientChannel()
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
        /// Close connection, and release all resources
        /// Inherit public finalizer from ANT_Device
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
                if (ResponseThread != null && ResponseThread.IsAlive)
                {
                    bResponseThreadExit = true;
                    if (!ResponseThread.Join(3000))
                        ResponseThread.Abort();
                }

                if (unmanagedClientPtr != IntPtr.Zero)
                {
                    ANTFSClient_Cancel(unmanagedClientPtr); // Cancel any pending operations
                    ANTFSClient_Close(unmanagedClientPtr);  // Close object (incl. unmanaged thread)
                    ANTFSClient_Delete(unmanagedClientPtr); // Delete unmanaged object
                    unmanagedClientPtr = IntPtr.Zero;
                }

                // If upload buffer is still pinned, unpin it
                releaseDownloadBuffer();

                bInitialized = false;
            }
        }


        /// <summary>
        /// Unpin and release buffer holding unmanaged data
        /// </summary>
        private void releaseDownloadBuffer()
        {
            if (ghDownloadData != null)
            {
                ghDownloadData.Value.Free();
                ghDownloadData = null;
            }
        }

        #endregion

        #region ANT-FS Client Methods

        /// <summary>
        /// Cancels any pending actions and returns the library
        /// to the appropriate ANTFS layer
        /// </summary>
        public void Cancel()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            ANTFSClient_Cancel(unmanagedClientPtr);
        }


        void channel_DeviceNotification(ANT_Device.DeviceNotificationCode notification, object notificationInfo)
        {
            ANTFSClient_ProcessDeviceNotification(unmanagedClientPtr, (byte)notification, IntPtr.Zero);
            //TODO: Once an object is defined for specific notification types, need to marshal it - pin it and pass it...
        }


        /// <summary>
        /// Gets the version string of the underlying ANT-FS library
        /// </summary>
        /// <returns>ANT-FS Library Version String</returns>
        public static string GetLibraryVersion()
        {
            byte[] returnVersion = new byte[Byte.MaxValue];   // maximum size
            if (ANTFSClient_GetVersion(returnVersion, (byte)returnVersion.Length) == 0)
                return "";  // blank string
            else
                return (Common.ConvertToString(returnVersion));
        }

        /// <summary>
        /// Returns the current library status
        /// </summary>
        /// <returns>Current library status</returns>
        public State GetStatus()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            return (State) ANTFSClient_GetStatus(unmanagedClientPtr);
        }

        /// <summary>
        /// Gets the host's friendly name string from the most recent session
        /// </summary>
        /// <returns>Host friendly name</returns>
        public string GetHostName()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            byte[] aucFriendlyName = new byte[Byte.MaxValue];
            byte ucSize = (byte)aucFriendlyName.Length;

            if (ANTFSClient_GetHostName(unmanagedClientPtr, aucFriendlyName, ref ucSize) == 0)
                return ""; // blank name

            if (ucSize != 0)
                return Common.ConvertToString(aucFriendlyName);
            else
                return ""; // blank name
        }

        /// <summary>
        /// Gets the transfer data from the pucTransferBufferDynamic.
        /// </summary>
        /// <returns>Data that will be uploaded in the specified file</returns>
        /// <param name="offset">Offset where to write on the client file</param>
        /// <param name="size">Size of the file that will be uploaded</param>
        public byte[] GetTransferData(out ulong offset, out ulong size)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            byte[] aucUploadData = new byte[0xFFFFFF];
            ulong ulSize = 0;
            ulong ulOffset = 0;

            if (ANTFSClient_GetTransferData(unmanagedClientPtr, ref ulOffset, ref ulSize, aucUploadData) == 0)
                throw new ANTFS_Exception("No upload data");

            size = ulSize;
            offset = ulOffset;
            return aucUploadData;
        }

        /// <summary>
        /// Gets the full parameters for a download, upload or erase request
        /// from the host
        /// </summary>
        /// <returns>Request parameters</returns>
        public ANTFS_RequestParameters GetRequestParameters()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            ANTFS_RequestParameters returnRequest = new ANTFS_RequestParameters();
            if (ANTFSClient_GetRequestParameters(unmanagedClientPtr, ref returnRequest) == 0)
                throw new ANTFS_Exception("No request parameters are available, probably in wrong state");

            return returnRequest;
        }

        /// <summary>
        /// Gets the file index for a download, upload or erase request from
        /// the host.  If more details are desired, use GetRequestParameters().
        /// </summary>
        /// <returns>Requested file index</returns>
        public ushort GetRequestedFileIndex()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            ushort index = 0;
            if (ANTFSClient_GetRequestedFileIndex(unmanagedClientPtr, ref index) == 0)
                throw new ANTFS_Exception("No request available, probably in wrong state");

            return index;
        }

        /// <summary>
        /// Gets the transfer progress of a pending or complete download
        /// </summary>
        /// <returns>The transfer status, including the current byte progress,
        /// total expected length of the download, and current percentage.
        /// Returns null if no valid status could be obtained.</returns>
        public int GetDownloadStatus()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            uint totalSize = 10;
            uint byteProgress = 0;

            if (ANTFSClient_GetDownloadStatus(unmanagedClientPtr, ref byteProgress, ref totalSize) == 0)
                throw new ANTFS_Exception("No download in progress");

            return (int) (byteProgress * 100 / totalSize);  // return as percentage
        }

        /// <summary>
        /// Gets the parameters of a disconnect command requested by the host
        /// </summary>
        /// <returns>Disconnect parameters</returns>
        public ANTFS_DisconnectParameters GetDisconnectParameters()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            ANTFS_DisconnectParameters returnRequest = new ANTFS_DisconnectParameters();
            if (ANTFSClient_GetDisconnectParameters(unmanagedClientPtr, ref returnRequest) == 0)
                throw new ANTFS_Exception("No disconnect parameters are available, probably in wrong state");

            return returnRequest;
        }

        /// <summary>
        /// Configures the friendly name for the ANT-FS client
        /// </summary>
        /// <param name="friendlyName">Client friendly name</param>
        public void SetFriendlyName(string friendlyName)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            if (friendlyName == null)
            {
                ANTFSClient_SetFriendlyName(unmanagedClientPtr, null, 0);
            }
            else
            {
                if (friendlyName.Length > Byte.MaxValue)
                    throw new ANTFS_Exception("Invalid friendly name length, maximum value is 255");
                byte[] charArray = Common.ConvertToByteArray(friendlyName);
                ANTFSClient_SetFriendlyName(unmanagedClientPtr, charArray, (byte)friendlyName.Length);
            }
        }

        /// <summary>
        /// Configures the passkey for a client to establish authenticated
        /// sessions with a host device, if passkey authentication is
        /// selected.  If a passkey is not configured, all passkey authentication
        /// requests will be rejected.
        /// </summary>
        /// <param name="passKey">Authentication passkey (max 255 bytes)</param>
        public void SetPassKey(byte[] passKey)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            if (passKey == null)
            {
                ANTFSClient_SetPassKey(unmanagedClientPtr, null, 0);
            }
            else
            {
                if (passKey.Length > Byte.MaxValue)
                    throw new ANTFS_Exception("Invalid pass key length, maximum value is 255");
                ANTFSClient_SetPassKey(unmanagedClientPtr, passKey, (byte)passKey.Length);
            }
        }

        /// <summary>
        /// Set the channel ID of the ANT-FS client
        /// If this function is not used to explicitly configure the channel ID, the ANT-FS host will use the following defaults:
        /// Device type: 1
        /// Transmission type: 5
        /// </summary>
        /// <param name="deviceType">Device type to assign to channel (ANT Channel ID)</param>
        /// <param name="transmissionType">Transmission type to assign to channel (ANT Channel ID)</param>
        public void SetChannelID(byte deviceType, byte transmissionType)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            if(deviceType == 0)
                throw new ANTFS_Exception("Device type cannot be wild carded on the client device!");

            if (transmissionType == 0)
                throw new ANTFS_Exception("Transmission type cannot be wild carded on the client device!");

            ANTFSClient_SetChannelID(unmanagedClientPtr, deviceType, transmissionType);
        }

        /// <summary>
        /// Sets a custom channel period for the ANT-FS beacon
        /// </summary>
        /// <param name="channelPeriod">Channel period, in 1/32768 counts</param>
        public void SetChannelPeriod(ushort channelPeriod)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            ANTFSClient_SetChannelPeriod(unmanagedClientPtr, channelPeriod);
        }

        /// <summary>
        /// Set the network key for the ANT-FS client
        /// If this function is not used to explicitly configure the network key, the ANT-FS client will use the
        /// ANT-FS network key, as set in the base ANT Library, and network number 0.
        /// Configuration is applied when the beacon is open
        /// </summary>
        /// <param name="netNumber">network number</param>
        /// <param name="networkKey">8-byte network key</param>
        public void SetClientNetworkKey(byte netNumber, byte[] networkKey)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            if (networkKey.Length != 8)
                throw new ANTFS_Exception("Network key must be 8 bytes");

            networkNumber = netNumber;
            ANTFSClient_SetNetworkKey(unmanagedClientPtr, netNumber, networkKey);
        }

        /// <summary>
        /// Configures the transmit power for the ANT-FS channel, in case
        /// different power levels are desired in the link and connected states,
        /// for example, to allow the use of proximity search in a host device.
        /// If not configured, the transmit power will be set to 0dBm.
        /// </summary>
        /// <param name="pairingLevel">Transmit power to use while in Link state</param>
        /// <param name="connectedLevel">Transmit power to use after a connection has been established to a host</param>
        public void SetTransmitPower(ANT_ReferenceLibrary.TransmitPower pairingLevel, ANT_ReferenceLibrary.TransmitPower connectedLevel)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            ANTFSClient_SetTxPower(unmanagedClientPtr, (byte) pairingLevel, (byte) connectedLevel);
        }

        /// <summary>
        /// Enable pairing authentication
        /// </summary>
        /// <param name="enable">Selects whether pairing authentication is enabled or not in the device</param>
        public void SetPairingEnabled(bool enable)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            ReturnCode theReturn = (ReturnCode)ANTFSClient_SetPairingEnabled(unmanagedClientPtr, enable);
            if (theReturn != ReturnCode.Pass)
                throw new ANTFS_RequestFailed_Exception("SetPairingEnabled", theReturn);
        }

        /// <summary>
        /// Enable uploads
        /// </summary>
        /// <param name="enable">Selects whether upload functionality is supported</param>
        public void SetUploadEnabled(bool enable)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            ReturnCode theReturn = (ReturnCode)ANTFSClient_SetUploadEnabled(unmanagedClientPtr, enable);
            if (theReturn != ReturnCode.Pass)
                throw new ANTFS_RequestFailed_Exception("SetUploadEnabled", theReturn);
        }

        /// <summary>
        /// Indicate if data is available for download
        /// </summary>
        /// <param name="dataIsAvailable">Selects whether data is available for download</param>
        public void SetDataAvailable(bool dataIsAvailable)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            ReturnCode theReturn = (ReturnCode)ANTFSClient_SetDataAvailable(unmanagedClientPtr, dataIsAvailable);
            if (theReturn != ReturnCode.Pass)
                throw new ANTFS_RequestFailed_Exception("SetDataAvailable", theReturn);
        }

        /// <summary>
        /// Configures the beacon timeout
        /// </summary>
        /// <param name="timeoutSeconds">Timeout, in seconds</param>
        public void SetBeaconTimeout(byte timeoutSeconds)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            if (timeoutSeconds == 0)
                throw new ANTFS_Exception("Invalid beacon timeout");

            ANTFSClient_SetBeaconTimeout(unmanagedClientPtr, timeoutSeconds);
        }

        /// <summary>
        /// Configures the pairing timeout
        /// </summary>
        /// <param name="timeoutSeconds">Timeout, in seconds</param>
        public void SetPairingTimeout(byte timeoutSeconds)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            if (timeoutSeconds == 0)
                throw new ANTFS_Exception("Invalid pairing timeout");

            ANTFSClient_SetPairingTimeout(unmanagedClientPtr, timeoutSeconds);
        }

        /// <summary>
        /// Sets up the ANT-FS client configuration parameters.  This function can only be used
        /// while the beacon is not open.  Individual parameters can be configured while the beacon is open
        /// If this function is not called, default beacon parameters are used.
        /// </summary>
        /// <see>SetPairingEnabled</see>
        /// <see>SetUploadEnabled</see>
        /// <see>SetDataAvailable</see>
        /// <see>SetBeaconTimeout</see>
        /// <see>SetPairingTimeout</see>
        /// <param name="clientParameters">ANT-FS Client parameters</param>
        public void Configure(ANTFS_ClientParameters clientParameters)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            ReturnCode theReturn = (ReturnCode) ANTFSClient_ConfigureClientParameters(unmanagedClientPtr, ref clientParameters);
            if (theReturn != ReturnCode.Pass)
                throw new ANTFS_RequestFailed_Exception("Configure", theReturn);
        }

        /// <summary>
        /// Begins the channel configuration to transmit the ANT-FS beacon.
        /// If the channel has already been configured (ANT-FS broadcast), the
        /// channel period needs to be specified in this function to let the
        /// client know the current period. If a channel period is not specified (0),
        /// the channel configuration is performed as specified in the beacon parameters.
        /// </summary>
        public void OpenBeacon()
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            ReturnCode theReturn = (ReturnCode) ANTFSClient_OpenBeacon(unmanagedClientPtr);
            if (theReturn != ReturnCode.Pass)
                throw new ANTFS_RequestFailed_Exception("OpenBeacon", theReturn);
        }

        /// <summary>
        /// Ends the ANT-FS session.
        /// </summary>
        /// <param name="returnToBroadcast">Set to true to return to broadcast (leave the channel open), and to false to close the channel.</param>
        public void CloseBeacon(bool returnToBroadcast)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            ReturnCode theReturn = (ReturnCode)ANTFSClient_CloseBeacon(unmanagedClientPtr, returnToBroadcast);
            if (theReturn != ReturnCode.Pass)
                throw new ANTFS_RequestFailed_Exception("CloseBeacon", theReturn);
        }

        /// <summary>
        /// Ends the ANT-FS session and closes the channel.
        /// </summary>
        public void CloseBeacon()
        {
            CloseBeacon(false);
        }

        /// <summary>
        /// Sends a response to a pairing request.
        /// </summary>
        /// <param name="acceptRequest">Select whether to accept or reject the request</param>
        public void SendPairingResponse(bool acceptRequest)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            ReturnCode theReturn = (ReturnCode)ANTFSClient_SendPairingResponse(unmanagedClientPtr, acceptRequest);
            if (theReturn != ReturnCode.Pass)
                throw new ANTFS_RequestFailed_Exception("SendPairingResponse", theReturn);
        }

        /// <summary>
        /// Sends a response to a request to erase a file from an
        /// authenticated remote device
        /// </summary>
        /// <param name="response">The response to the erase request</param>
        public void SendEraseResponse(EraseResponse response)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            ReturnCode theReturn = (ReturnCode)ANTFSClient_SendEraseResponse(unmanagedClientPtr, (byte)response);
            if (theReturn != ReturnCode.Pass)
                throw new ANTFS_RequestFailed_Exception("SendEraseRequest", theReturn);
        }

        /// <summary>
        /// Sends a response to a request to download a file to a host device
        /// </summary>
        /// <param name="response">The response to the download request</param>
        /// <param name="fileIndex">The file index to download</param>
        /// <param name="blockSize">The maximum number of bytes to send in a single block</param>
        /// <param name="downloadData">File to download.  The entire file most be provided, even if the host requested an offset.</param>
        public void SendDownloadResponse(DownloadResponse response, ushort fileIndex, uint blockSize, byte[] downloadData)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            IntPtr pvData = IntPtr.Zero;
            uint dataLength = 0;
            ANTFS_DownloadParameters responseParameters;

            releaseDownloadBuffer();    // If we had not freed the handle from a previous download, do it now

            responseParameters.FileIndex = fileIndex;
            responseParameters.MaxBlockSize = blockSize;

            if (response == DownloadResponse.Ok)
            {
                if (downloadData == null)
                    throw new ANT_Exception("Download request accepted, but no data provided");

                ghDownloadData = GCHandle.Alloc(downloadData, GCHandleType.Pinned);    // Pin managed buffer
                pvData = ghDownloadData.Value.AddrOfPinnedObject(); // Get address of managed buffer

                if (pvData == IntPtr.Zero)
                    throw new ANTFS_Exception("Download request failed: Unable to pin managed buffer for download");

                dataLength = (uint) downloadData.Length;
                if(blockSize == 0 || dataLength < blockSize)
                    responseParameters.MaxBlockSize = dataLength;
            }

            ReturnCode theReturn = (ReturnCode)ANTFSClient_SendDownloadResponse(unmanagedClientPtr, (byte)response, ref responseParameters, dataLength, pvData);

            if (theReturn == ReturnCode.Pass)
            {
                usCurrentTransfer = responseParameters.FileIndex;    // Track current transfer
            }
            else
            {
                // If the request was not successful, we should free the handle here
                // Otherwise, buffer needs to remain pinned while download completes (pass/fail/etc...)
                // GCHandle will be freed when we receive an ANT-FS response, or the application is terminated
                releaseDownloadBuffer();
                throw new ANTFS_RequestFailed_Exception("SendDownloadResponse", theReturn);
            }
        }

        /// <summary>
        /// Sends a response to a request to download a file to a host device.
        /// Will attempt to send the entire download in a single block.
        /// </summary>
        /// <param name="response">The response to the download request</param>
        /// <param name="fileIndex">The file index to download</param>
        /// <param name="downloadData">File to download.  The entire file most be provided, even if the host requested an offset.</param>
        public void SendDownloadResponse(DownloadResponse response, ushort fileIndex, byte[] downloadData)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            SendDownloadResponse(response, fileIndex, 0, downloadData);
        }

        /// <summary>
        /// Sends a response to a request to upload a file from a host device
        /// </summary>
        /// <param name="response">The response to the upload request</param>
        /// <param name="fileIndex">The file index to upload</param>
        /// <param name="blockSize">The maximum number of bytes to send in a single block</param>
        /// <param name="maxFileSize">The maximum file size.</param>
        public void SendUploadResponse(UploadResponse response, ushort fileIndex, uint blockSize, uint maxFileSize)
        {
            if (!bInitialized)
                throw new ObjectDisposedException("ANTFSClient object has been disposed");

            //IntPtr pvData = IntPtr.Zero;
            //uint dataLength = 0;
            ANTFS_UploadParameters responseParameters;

            //releaseDownloadBuffer();    // If we had not freed the handle from a previous download, do it now

            responseParameters.FileIndex = fileIndex;
            responseParameters.MaxBlockSize = blockSize;
            responseParameters.MaxFileSize = maxFileSize;

            if (response == UploadResponse.Ok)
            {
                //if (downloadData == null)
                //    throw new ANT_Exception("Download request accepted, but no data provided");

                //ghDownloadData = GCHandle.Alloc(downloadData, GCHandleType.Pinned);    // Pin managed buffer
                //pvData = ghDownloadData.Value.AddrOfPinnedObject(); // Get address of managed buffer

                //if (pvData == IntPtr.Zero)
                //    throw new ANTFS_Exception("Download request failed: Unable to pin managed buffer for download");

                //dataLength = (uint)downloadData.Length;
                if (blockSize == 0 || maxFileSize < blockSize)
                    responseParameters.MaxBlockSize = maxFileSize;
            }

            ReturnCode theReturn = (ReturnCode)ANTFSClient_SendUploadResponse(unmanagedClientPtr, (byte)response, ref responseParameters, 0, IntPtr.Zero);

            if (theReturn == ReturnCode.Pass)
            {
                usCurrentTransfer = responseParameters.FileIndex;    // Track current transfer
            }
            else
            {
                // If the request was not successful, we should free the handle here
                // Otherwise, buffer needs to remain pinned while download completes (pass/fail/etc...)
                // GCHandle will be freed when we receive an ANT-FS response, or the application is terminated
                //releaseDownloadBuffer();
                throw new ANTFS_RequestFailed_Exception("SendUploadResponse", theReturn);
            }
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
                eThreadResponse = (Response)ANTFSClient_WaitForResponse(unmanagedClientPtr, 999);

                if (eThreadResponse != Response.None)    // We got a response
                {
                    // If we were processing a download, release handle
                    releaseDownloadBuffer();

                    // Dispatch response if we got one
                    if (OnResponse != null)
                    {
                        OnResponse(eThreadResponse);
                    }
                }
            }
        }

        #endregion
    }
}
