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

namespace ANT_Managed_Library
{
    /// <summary>
    /// Control class for an individual ANT channel. Created and accessed through the ANTDevice class.
    /// </summary>
    public class ANT_Channel : IANT_Channel
    {
        #region variables

        readonly ANT_Device creatingDevice = null;

        IntPtr unmanagedANTFramerPointer = IntPtr.Zero;

        private byte channelNumber;

        private bool disposed = false;

        private object deviceNotificationLock = new object();

        private object channelResponseLock = new object();

        #endregion


        #region ChannelEventCallback Variables

        private event dRawChannelResponseHandler rawChannelResponse_internal;
        /// <summary>
        /// The channel callback event for forwarding the raw msg struct. Triggered every time a message is received from the ANT device.
        /// Examples include transmit and receive messages. If you are coding in C# use the other response event version.
        /// </summary>
        public event dRawChannelResponseHandler rawChannelResponse
        {
            add
            {
                lock (channelResponseLock)
                {
                    rawChannelResponse_internal += value;
                }
            }
            remove
            {
                lock (channelResponseLock)
                {
                    rawChannelResponse_internal -= value;
                }
            }
        }

        public event dDeviceNotificationHandler DeviceNotification_internal;
        /// <summary>
        /// This event is fired whenever there are events on the device level that may impact the channel.
        /// Events that currently occur (Event, value of notification info Object):
        ///     Reset, null
        ///     Shutdown, null
        /// </summary>
        public event dDeviceNotificationHandler DeviceNotification
        {
            add
            {
                lock (deviceNotificationLock)
                {
                    DeviceNotification_internal += value;
                }
            }
            remove
            {
                lock (deviceNotificationLock)
                {
                    DeviceNotification_internal -= value;
                }
            }
        }

        public event dChannelResponseHandler channelResponse_internal;
        /// <summary>
        /// The channel callback event. Triggered every time a message is received from the ANT device.
        /// Examples include transmit and receive messages.
        /// </summary>
        public event dChannelResponseHandler channelResponse    //The event to assign callback functions to in a managed application
        {
            add
            {
                lock (channelResponseLock)
                {
                    channelResponse_internal += value;
                }
            }
            remove
            {
                lock (channelResponseLock)
                {
                    channelResponse_internal -= value;
                }
            }
        }

        #endregion



        #region ANT_DLL Imports

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_GetChannelID(IntPtr FramerPtr, byte ucANTChannel_, ref UInt16 pusDeviceNumber_, ref byte pucDeviceType_, ref byte pucTransmitType_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_GetChannelStatus(IntPtr FramerPtr, byte ucANTChannel_, ref byte pucStatus_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_AssignChannel_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_AssignChannel(IntPtr FramerPtr, byte ucANTChannel, byte ucChanType, byte ucNetNumber, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_AssignChannelExt_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_AssignChannelExt(IntPtr FramerPtr, byte ucANTChannel, byte ucChanType, byte ucNetNumber, byte ucExtFlags, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_UnAssignChannel_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_UnAssignChannel(IntPtr FramerPtr, byte ucANTChannel, UInt32 ulResponseTime);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SetChannelId_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_SetChannelId(IntPtr FramerPtr, byte ucANTChannel, UInt16 usDeviceNumber, byte ucDeviceType, byte ucTransmissionType_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SetSerialNumChannelId_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_SetSerialNumChannelId_RTO(IntPtr FramerPtr, byte ucANTChannel_, byte ucDeviceType_, byte ucTransmissionType_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SetChannelPeriod_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_SetChannelPeriod(IntPtr FramerPtr, byte ucANTChannel_, UInt16 usMesgPeriod_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_RSSI_SetSearchThreshold_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_RSSI_SetSearchThreshold(IntPtr FramerPtr, byte ucANTChannel_, byte ucThreshold_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SetChannelRFFreq_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_SetChannelRFFreq(IntPtr FramerPtr, byte ucANTChannel_, byte ucRFFreq_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SetChannelTxPower_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_SetChannelTxPower(IntPtr FramerPtr, byte ucANTChannel_, byte ucTransmitPower_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SetChannelSearchTimeout_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_SetChannelSearchTimeout(IntPtr FramerPtr, byte ucANTChannel_, byte ucSearchTimeout_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_OpenChannel_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_OpenChannel(IntPtr FramerPtr, byte ucANTChannel, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_CloseChannel_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_CloseChannel(IntPtr FramerPtr, byte ucANTChannel, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_SendBroadcastData(IntPtr FramerPtr, byte ucANTChannel, byte[] pucData);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SendAcknowledgedData_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern byte ANT_SendAcknowledgedData(IntPtr FramerPtr, byte ucANTChannel, byte[] pucData, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SendBurstTransfer_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern byte ANT_SendBurstTransfer(IntPtr FramerPtr, byte ucANTChannel_, byte[] pucData_, UInt32 usNumBytes, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SendAdvancedBurstTransfer_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern byte ANT_SendAdvancedBurstTransfer(IntPtr FramerPtr, byte ucANTChannel_, byte[] pucData_, UInt32 usNumBytes, byte numStdPcktsPerSerialMsg_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_SendExtBroadcastData(IntPtr FramerPtr, byte ucANTChannel, UInt16 usDeviceNumber, byte ucDeviceType, byte ucTransmissionType_, byte[] pucData);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SendExtAcknowledgedData_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern byte ANT_SendExtAcknowledgedData(IntPtr FramerPtr, byte ucANTChannel, UInt16 usDeviceNumber, byte ucDeviceType, byte ucTransmissionType_, byte[] pucData, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SendExtBurstTransfer_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern byte ANT_SendExtBurstTransfer(IntPtr FramerPtr, byte ucANTChannel_, UInt16 usDeviceNumber, byte ucDeviceType, byte ucTransmissionType_, byte[] pucData_, UInt32 usNumBytes, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_SetLowPriorityChannelSearchTimeout_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_SetLowPriorityChannelSearchTimeout(IntPtr FramerPtr, byte ucANTChannel_, byte ucSearchTimeout_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_AddChannelID_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_AddChannelID(IntPtr FramerPtr, byte ucANTChannel_, UInt16 usDeviceNumber_, byte ucDeviceType_, byte ucTransmissionType_, byte ucListIndex_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_ConfigList_RTO", CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_ConfigList(IntPtr FramerPtr, byte ucANTChannel_, byte ucListSize_, byte ucExclude_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_EncryptedChannelEnable_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_EncryptedChannelEnable(IntPtr FramerPtr, byte ucANTChannel, byte ucMode, byte ucVolatileKeyIndex, byte ucDecimationRate,  UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_AddCryptoID_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_AddCryptoID(IntPtr FramerPtr, byte ucANTChannel, byte[] pucData, byte ucListIndex, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, EntryPoint = "ANT_ConfigCryptoList_RTO", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_ConfigCryptoList(IntPtr FramerPtr, byte ucANTChannel, byte ucListSize, byte ucBlacklist, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_SetProximitySearch(IntPtr FramerPtr, byte ucANTChannel_, byte ucSearchThreshold_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_ConfigFrequencyAgility(IntPtr FramerPtr, byte ucANTChannel_, byte ucFreq1_, byte ucFreq2_, byte ucFreq3_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_ConfigSelectiveDataUpdate(IntPtr FramerPtr, byte ucANTChannel_, byte ucSduConfig_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_SetSearchSharingCycles(IntPtr FramerPtr, byte ucANTChannel_, byte ucSearchSharingCycles_, UInt32 ulResponseTime_);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_SetChannelSearchPriority(IntPtr FramerPty, byte ucANTChannel_, byte ucPriorityLevel_, UInt32 ulResponseTime_);


        #endregion



        #region Constructor

        //Internal, because a channel can not be created without a device instance. Channels must be created by the device class which assigns the appropriate channel number
        internal ANT_Channel(ANT_Device creatingDevice, byte ucChannelNumber)
        {
            this.creatingDevice = creatingDevice;
            this.unmanagedANTFramerPointer = creatingDevice.getFramerPtr();
            channelNumber = ucChannelNumber;
        }


        #endregion



        #region non-ANTdll Functions


        /// <summary>
        /// Returns the ANTDevice that this channel belongs to
        /// </summary>
        public ANT_Device getParentDevice()
        {
            return creatingDevice;
        }


        /// <summary>
        /// Returns the underlying C++ ANT framer reference that this channel uses for messaging. Useful to pass to unmanaged C++ implementations.
        /// </summary>
        /// <returns>Pointer to the C++ ANT framer for messaging</returns>
        public IntPtr getUnmgdFramer()
        {
            return creatingDevice.getFramerPtr();
        }


        /// <summary>
        /// Returns the channel number of this instance
        /// </summary>
        public byte getChannelNum()
        {
            return channelNumber;
        }


        internal void NotifyDeviceEvent(ANT_Device.DeviceNotificationCode notification, object notificationInfo)
        {
            lock (deviceNotificationLock)
            {
                if (DeviceNotification_internal != null)
                {
                    DeviceNotification_internal(notification, notificationInfo);
                }
            }
        }


        internal void MessageReceived(ANT_Device.ANTMessage newMessage, ushort messageSize)
        {
            lock (channelResponseLock)
            {
                if (channelResponse_internal != null)
                {
                    channelResponse_internal(new ANT_Response(this, channelNumber, DateTime.Now, newMessage.msgID, newMessage.ucharBuf.Take(messageSize).ToArray()));
                }

                if (rawChannelResponse_internal != null)
                {
                    rawChannelResponse_internal(newMessage, messageSize);
                }
            }
        }


        /// <summary>
        /// Dispose this channel.
        /// </summary>
        public void Dispose()
        {
            //There are no unmanaged resources to clean up in this channel implementation
            //It would be nice to close the channel if it is open, but it is really hard to tell if the channel
            //is open or not without requesting channelStatus, and we don't want to issue extraneous commands and clutter the logs
            //We can however nullify our reference in the device list, so that if someone calls getChannel again they get a new non-disposed channel
            creatingDevice.channelDisposed(channelNumber);
            disposed = true;
            GC.SuppressFinalize(this);
        }


        /// <summary>
        /// Returns current channel status.
        /// Throws exception on timeout.
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        public ANT_ChannelStatus requestStatus(UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            byte statusByte = 0;
            if (ANT_GetChannelStatus(unmanagedANTFramerPointer, channelNumber, ref statusByte, responseWaitTime) == 1)
            {
                return new ANT_ChannelStatus((ANT_ReferenceLibrary.BasicChannelStatusCode)(statusByte & 0x03), (byte)((statusByte & 0x0C) >> 2), (ANT_ReferenceLibrary.ChannelType)(statusByte & 0xF0));
            }
            else
            {
                throw new ANT_Exception("Timed out waiting for requested message");
            }
        }


        /// <summary>
        /// Returns the channel ID
        /// Throws exception on timeout
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns></returns>
        public ANT_ChannelID requestID(UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            ushort deviceNumber = 0;
            byte deviceType = 0;
            byte transmitType = 0;

            if (ANT_GetChannelID(unmanagedANTFramerPointer, channelNumber, ref deviceNumber, ref deviceType, ref transmitType, responseWaitTime) == 1)
            {
                return new ANT_ChannelID(deviceNumber, deviceType, transmitType);
            }
            else
            {
                throw new ANT_Exception("Timed out waiting for requested message");
            }
        }

        #endregion



        #region ANT Channel Functions

        /// <overloads>Assign channel</overloads>
        /// <summary>
        /// Assign an ANT channel along with its main parameters.
        /// Throws exception if the network number is invalid.
        /// </summary>
        /// <param name="channelTypeByte">Channel Type byte</param>
        /// <param name="networkNumber">Network to assign to channel, must be less than device's max networks-1</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool assignChannel(ANT_ReferenceLibrary.ChannelType channelTypeByte, byte networkNumber, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            if (networkNumber > creatingDevice.getDeviceCapabilities().maxNetworks - 1)
                throw new ANT_Exception("Network number must be less than device's max networks - 1");
            return (ANT_AssignChannel(unmanagedANTFramerPointer, channelNumber, (byte)channelTypeByte, networkNumber, responseWaitTime) == 1);
        }
        /// <summary>
        /// Assign an ANT channel.
        /// </summary>
        /// <param name="channelTypeByte">Channel Type byte</param>
        /// <param name="networkNumber">Network to assign to channel</param>
        public void assignChannel(ANT_ReferenceLibrary.ChannelType channelTypeByte, byte networkNumber) { assignChannel(channelTypeByte, networkNumber, 0); }


        /// <overloads>Assign channel (extended)</overloads>
        /// <summary>
        /// Assign an ANT channel, using extended channel assignment
        /// Throws exception if the network number is invalid.
        /// </summary>
        /// <param name="channelTypeByte">Channel Type byte</param>
        /// <param name="networkNumber">Network to assign to channel, must be less than device's max netwoks - 1</param>
        /// <param name="extAssignByte">Extended assignment byte</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool assignChannelExt(ANT_ReferenceLibrary.ChannelType channelTypeByte, byte networkNumber, ANT_ReferenceLibrary.ChannelTypeExtended extAssignByte, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            if (networkNumber > creatingDevice.getDeviceCapabilities().maxNetworks - 1)
                throw new ANT_Exception("Network number must be less than device's max networks - 1");
            return (ANT_AssignChannelExt(unmanagedANTFramerPointer, channelNumber, (byte)channelTypeByte, networkNumber, (byte)extAssignByte, responseWaitTime) == 1);
        }
        /// <summary>
        /// Assign an ANT channel, using extended channel assignment
        /// Throws exception if the network number is invalid.
        /// </summary>
        /// <param name="channelTypeByte">Channel Type byte</param>
        /// <param name="networkNumber">Network to assign to channel, must be less than device's max netwoks - 1</param>
        /// <param name="extAssignByte">Extended assignment byte</param>
        public void assignChannelExt(ANT_ReferenceLibrary.ChannelType channelTypeByte, byte networkNumber, ANT_ReferenceLibrary.ChannelTypeExtended extAssignByte) { assignChannelExt(channelTypeByte, networkNumber, extAssignByte, 0); }


        /// <overloads>Unassign channel</overloads>
        /// <summary>
        /// Unassign this channel.
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool unassignChannel(UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            return (ANT_UnAssignChannel(unmanagedANTFramerPointer, channelNumber, responseWaitTime) == 1);
        }
        /// <summary>
        /// Unassigns this channel.
        /// </summary>
        public void unassignChannel() { unassignChannel(0); }


        /// <overloads>Set the Channel ID</overloads>
        /// <summary>
        /// Set the Channel ID of this channel.
        /// Throws exception if device type is > 127.
        /// </summary>
        /// <param name="deviceNumber">Device number to assign to channel. Set to 0 for receiver wild card matching</param>
        /// <param name="pairingEnabled">Device pairing bit.</param>
        /// <param name="deviceTypeID">Device type to assign to channel. Must be less than 128. Set to 0 for receiver wild card matching</param>
        /// <param name="transmissionTypeID">Transmission type to assign to channel. Set to 0 for receiver wild card matching</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setChannelID(UInt16 deviceNumber, bool pairingEnabled, byte deviceTypeID, byte transmissionTypeID, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            if (deviceTypeID > 127)
                throw new ANT_Exception("Device Type ID is larger than 127");
            if (pairingEnabled) //Set the pairing flag
                deviceTypeID |= 0x80;

            return (ANT_SetChannelId(unmanagedANTFramerPointer, channelNumber, deviceNumber, deviceTypeID, transmissionTypeID, responseWaitTime) == 1);
        }
        /// <summary>
        /// Set the Channel ID of this channel.
        /// Throws exception if device type is > 127.
        /// </summary>
        /// <param name="deviceNumber">Device number to assign to channel. Set to 0 for receiver wild card matching</param>
        /// <param name="pairingEnabled">Device pairing bit</param>
        /// <param name="deviceTypeID">Device type to assign to channel. Set to 0 for receiver wild card matching</param>
        /// <param name="transmissionTypeID">Transmission type to assign to channel. Set to 0 for receiver wild card matching</param>
        public void setChannelID(UInt16 deviceNumber, bool pairingEnabled, byte deviceTypeID, byte transmissionTypeID) { setChannelID(deviceNumber, pairingEnabled, deviceTypeID, transmissionTypeID, 0); }


        /// <overloads>Sets the Channel ID, using serial number as device number</overloads>
        /// <summary>
        /// Identical to setChannelID, except last two bytes of serial number are used for device number.
        /// Not available on all ANT devices.
        /// Throws exception if device type is > 127.
        /// </summary>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setChannelID_UsingSerial(bool pairingEnabled, byte deviceTypeID, byte transmissionTypeID, UInt32 waitResponseTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            if (deviceTypeID > 127)
                throw new ANT_Exception("Device Type ID is larger than 127");
            if (pairingEnabled) //Set the pairing flag
                deviceTypeID |= 0x80;

            return (ANT_SetSerialNumChannelId_RTO(unmanagedANTFramerPointer, channelNumber, deviceTypeID, transmissionTypeID, waitResponseTime) == 1);
        }
        /// <summary>
        /// Identical to setChannelID, except last two bytes of serial number are used for device number.
        /// </summary>
        public void setChannelID_UsingSerial(bool pairingEnabled, byte deviceTypeID, byte transmissionTypeID) { setChannelID_UsingSerial(pairingEnabled, deviceTypeID, transmissionTypeID, 0); }


        /// <overloads>Sets channel message period</overloads>
        /// <summary>
        /// Set this channel's messaging period
        /// </summary>
        /// <param name="messagePeriod_32768unitspersecond">Desired period in seconds * 32768</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setChannelPeriod(UInt16 messagePeriod_32768unitspersecond, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            return (ANT_SetChannelPeriod(unmanagedANTFramerPointer, channelNumber, messagePeriod_32768unitspersecond, responseWaitTime) == 1);
        }
        /// <summary>
        /// Set this channel's messaging period
        /// </summary>
        /// <param name="messagePeriod_32768unitspersecond">Desired period in seconds * 32768</param>
        public void setChannelPeriod(UInt16 messagePeriod_32768unitspersecond) { setChannelPeriod(messagePeriod_32768unitspersecond, 0); }


        /// <overloads>Sets the RSSI threshold (ARCT)</overloads>
        /// <summary>
        /// Set this channel's RSSI threshold (ARCT)
        /// </summary>
        /// <param name="thresholdRSSI">Desired RSSI threshold value</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setSearchThresholdRSSI(byte thresholdRSSI, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            return (ANT_RSSI_SetSearchThreshold(unmanagedANTFramerPointer, channelNumber, thresholdRSSI, responseWaitTime) == 1);
        }
        /// <summary>
        /// Set this channel's RSSI threshold (ARCT)
        /// </summary>
        /// <param name="thresholdRSSI">Desired RSSI threshold value</param>
        public void setSearchThresholdRSSI(byte thresholdRSSI) { setSearchThresholdRSSI(thresholdRSSI, 0); }


        /// <overloads>Sets channel RF Frequency</overloads>
        /// <summary>
        /// Set this channel's RF frequency, with the given offset from 2400Mhz.
        /// Note: Changing this frequency may affect the ability to certify the product in certain areas of the world.
        /// </summary>
        /// <param name="RFFreqOffset">Offset to add to 2400Mhz</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setChannelFreq(byte RFFreqOffset, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            return (ANT_SetChannelRFFreq(unmanagedANTFramerPointer, channelNumber, RFFreqOffset, responseWaitTime) == 1);
        }
        /// <summary>
        /// Set this channel's RF frequency, with the given offset from 2400Mhz.
        /// Note: Changing this frequency may affect the ability to certify the product in certain areas of the world.
        /// </summary>
        /// <param name="RFFreqOffset">Offset to add to 2400Mhz</param>
        public void setChannelFreq(byte RFFreqOffset) { setChannelFreq(RFFreqOffset, 0); }


        /// <overloads>Sets the channel transmission power</overloads>
        /// <summary>
        /// Set the transmission power of this channel
        /// Throws exception if device is not capable of per-channel transmit power.
        /// </summary>
        /// <param name="transmitPower">Transmission power to set to</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setChannelTransmitPower(ANT_ReferenceLibrary.TransmitPower transmitPower, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            if (!creatingDevice.getDeviceCapabilities().perChannelTransmitPower)
                throw new ANT_Exception("Device not capable of per-channel transmit power");
            return (ANT_SetChannelTxPower(unmanagedANTFramerPointer, channelNumber, (byte)transmitPower, responseWaitTime) == 1);
        }
        /// <summary>
        /// Set the transmission power of this channel
        /// </summary>
        /// <param name="transmitPower">Transmission power to set to</param>
        public void setChannelTransmitPower(ANT_ReferenceLibrary.TransmitPower transmitPower) { setChannelTransmitPower(transmitPower, 0); }


        /// <overloads>Sets the channel search timeout</overloads>
        /// <summary>
        /// Set the search timeout
        /// </summary>
        /// <param name="searchTimeout">timeout in 2.5 second units (in newer devices 255=infinite)</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setChannelSearchTimeout(byte searchTimeout, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            return (ANT_SetChannelSearchTimeout(unmanagedANTFramerPointer, channelNumber, searchTimeout, responseWaitTime) == 1);
        }
        /// <summary>
        /// Set the search timeout
        /// </summary>
        /// <param name="searchTimeout">timeout in 2.5 second units (in newer devices 255=infinite)</param>
        public void setChannelSearchTimeout(byte searchTimeout) { setChannelSearchTimeout(searchTimeout, 0); }


        /// <overloads>Opens the channel</overloads>
        /// <summary>
        /// Opens this channel
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool openChannel(UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            return (ANT_OpenChannel(unmanagedANTFramerPointer, channelNumber, responseWaitTime) == 1);
        }
        /// <summary>
        /// Opens this channel
        /// </summary>
        public void openChannel() { openChannel(0); }


        /// <overloads>Sends broadcast message</overloads>
        /// <summary>
        /// Sends the given data on the broadcast transmission.
        /// Throws exception if data > 8-bytes in length
        /// </summary>
        /// <param name="data">data to send (length 8 or less)</param>
        public bool sendBroadcastData(byte[] data)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            int padNum = 8 - data.Length;
            if (padNum < 0)
                throw new ANT_Exception("Send data must not be greater than 8 bytes");
            data = data.Concat(new byte[padNum]).ToArray();
            return ANT_SendBroadcastData(unmanagedANTFramerPointer, channelNumber, data) == 1;
        }


        /// <overloads>Sends acknowledged message</overloads>
        /// <summary>
        /// Sends the given data as an acknowledged transmission. Returns: 0=fail, 1=pass, 2=timeout, 3=cancelled
        /// Throws exception if data > 8-bytes in length
        /// </summary>
        /// <param name="data">data to send (length 8 or less)</param>
        /// <param name="ackWaitTime">Time in ms to wait for acknowledgement</param>
        /// <returns>0=fail, 1=pass, 2=timeout, 3=cancelled</returns>
        public ANT_ReferenceLibrary.MessagingReturnCode sendAcknowledgedData(byte[] data, UInt32 ackWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            int padNum = 8 - data.Length;
            if (padNum < 0)
                throw new ANT_Exception("Send data must not be greater than 8 bytes");
            data = data.Concat(new byte[padNum]).ToArray();
            return (ANT_ReferenceLibrary.MessagingReturnCode)ANT_SendAcknowledgedData(unmanagedANTFramerPointer, channelNumber, data, ackWaitTime);
        }
        /// <summary>
        /// Sends the given data as an acknowledged transmission.
        /// Throws exception if data > 8-bytes in length
        /// </summary>
        /// <param name="data">data to send (length 8 or less)</param>
        public void sendAcknowledgedData(byte[] data) { sendAcknowledgedData(data, 0); }


        /// <overloads>Sends burst transfer</overloads>
        /// <summary>
        /// Sends the given data as a burst transmission. Returns: 0=fail, 1=pass, 2=timeout, 3=cancelled
        /// </summary>
        /// <param name="data">data to send, can be any length</param>
        /// <param name="completeWaitTime">Time in ms to wait for completion of transfer</param>
        /// <returns>0=fail, 1=pass, 2=timeout, 3=cancelled</returns>
        public ANT_ReferenceLibrary.MessagingReturnCode sendBurstTransfer(byte[] data, UInt32 completeWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            int padNum = 8 - (data.Length % 8);
            if(padNum != 8)
                data = data.Concat(new byte[padNum]).ToArray();
            return (ANT_ReferenceLibrary.MessagingReturnCode)ANT_SendBurstTransfer(unmanagedANTFramerPointer, channelNumber, data, (uint)data.Length, completeWaitTime);
        }
        /// <summary>
        /// Sends the given data as a burst transmission.
        /// </summary>
        /// <param name="data">data to send, can be any length</param>
        public void sendBurstTransfer(byte[] data) { sendBurstTransfer(data, 0); }




        /// <overloads>Sends extended broadcast message</overloads>
        /// <summary>
        /// Sends the given data as an extended broadcast transmission.
        /// Throws exception if data > 8-bytes in length
        /// </summary>
        /// <param name="deviceNumber">Device number of channel ID to send to</param>
        /// <param name="deviceTypeID">Device type of channel ID to send to</param>
        /// <param name="transmissionTypeID">Transmission type of channel ID to send to</param>
        /// <param name="data">data to send (length 8 or less)</param>
        public bool sendExtBroadcastData(UInt16 deviceNumber, byte deviceTypeID, byte transmissionTypeID, byte[] data)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            int padNum = 8 - data.Length;
            if (padNum < 0)
                throw new ANT_Exception("Send data must not be greater than 8 bytes");
            data = data.Concat(new byte[padNum]).ToArray();
            return ANT_SendExtBroadcastData(unmanagedANTFramerPointer, channelNumber, deviceNumber, deviceTypeID, transmissionTypeID, data) == 1;
        }


        /// <overloads>Sends extended acknowledged message</overloads>
        /// <summary>
        /// Sends the given data as an extended acknowledged transmission. Returns: 0=fail, 1=pass, 2=timeout, 3=cancelled
        /// Throws exception if data > 8-bytes in length
        /// </summary>
        /// <param name="deviceNumber">Device number of channel ID to send to</param>
        /// <param name="deviceTypeID">Device type of channel ID to send to</param>
        /// <param name="transmissionTypeID">Transmission type of channel ID to send to</param>
        /// <param name="data">data to send (length 8 or less)</param>
        /// <param name="ackWaitTime">Time in ms to wait for acknowledgement</param>
        /// <returns>0=fail, 1=pass, 2=timeout, 3=cancelled</returns>
        public ANT_ReferenceLibrary.MessagingReturnCode sendExtAcknowledgedData(UInt16 deviceNumber, byte deviceTypeID, byte transmissionTypeID, byte[] data, UInt32 ackWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            int padNum = 8 - data.Length;
            if (padNum < 0)
                throw new ANT_Exception("Send data must not be greater than 8 bytes");
            data = data.Concat(new byte[padNum]).ToArray();
            return (ANT_ReferenceLibrary.MessagingReturnCode)ANT_SendExtAcknowledgedData(unmanagedANTFramerPointer, channelNumber, deviceNumber, deviceTypeID, transmissionTypeID, data, ackWaitTime);
        }
        /// <summary>
        /// Sends the given data as an extended acknowledged transmission.
        /// Throws exception if data > 8-bytes in length
        /// </summary>
        /// <param name="deviceNumber">Device number of channel ID to send to</param>
        /// <param name="deviceTypeID">Device type of channel ID to send to</param>
        /// <param name="transmissionTypeID">Transmission type of channel ID to send to</param>
        /// <param name="data">data to send (length 8 or less)</param>
        public void sendExtAcknowledgedData(UInt16 deviceNumber, byte deviceTypeID, byte transmissionTypeID, byte[] data) { sendExtAcknowledgedData(deviceNumber, deviceTypeID, transmissionTypeID, data, 0); }


        /// <overloads>Sends extended burst data</overloads>
        /// <summary>
        /// Sends the given data as an extended burst transmission. Returns: 0=fail, 1=pass, 2=timeout, 3=cancelled
        /// </summary>
        /// <param name="deviceNumber">Device number of channel ID to send to</param>
        /// <param name="deviceTypeID">Device type of channel ID to send to</param>
        /// <param name="transmissionTypeID">Transmission type of channel ID to send to</param>
        /// <param name="data">data to send, can be any length</param>
        /// <param name="completeWaitTime">Time in ms to wait for completion of transfer</param>
        /// <returns>0=fail, 1=pass, 2=timeout, 3=cancelled</returns>
        public ANT_ReferenceLibrary.MessagingReturnCode sendExtBurstTransfer(UInt16 deviceNumber, byte deviceTypeID, byte transmissionTypeID, byte[] data, UInt32 completeWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            //The new unmanaged wrapper burst function handles the padding, but we just do it here anyway to keep in line with the other send functions
            int padNum = 8 - (data.Length % 8);
            if(padNum != 8)
                data = data.Concat(new byte[padNum]).ToArray();
            return (ANT_ReferenceLibrary.MessagingReturnCode)ANT_SendExtBurstTransfer(unmanagedANTFramerPointer, channelNumber, deviceNumber, deviceTypeID, transmissionTypeID, data, (uint)data.Length, completeWaitTime);
        }
        /// <summary>
        /// Sends the given data as an extended burst transmission.
        /// </summary>
        /// <param name="deviceNumber">Device number of channel ID to send to</param>
        /// <param name="deviceTypeID">Device type of channel ID to send to</param>
        /// <param name="transmissionTypeID">Transmission type of channel ID to send to</param>
        /// <param name="data">data to send, can be any length</param>
        public void sendExtBurstTransfer(UInt16 deviceNumber, byte deviceTypeID, byte transmissionTypeID, byte[] data) { sendExtBurstTransfer(deviceNumber, deviceTypeID, transmissionTypeID, data, 0); }


        /// <overloads>Closes the channel</overloads>
        /// <summary>
        /// Close this channel
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool closeChannel(UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            return (ANT_CloseChannel(unmanagedANTFramerPointer, channelNumber, responseWaitTime) == 1);
        }
        /// <summary>
        /// Close this channel
        /// </summary>
        public void closeChannel() { closeChannel(0); }



        /// <overloads>Sets the channel low priority search timeout</overloads>
        /// <summary>
        /// Sets the search timeout for the channel's low-priority search, where it will not interrupt other open channels.
        /// When this period expires the channel will drop to high-priority search.
        /// This feature is not available in all ANT devices.
        /// </summary>
        /// <param name="lowPriorityTimeout">Timeout period in 2.5 second units</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setLowPrioritySearchTimeout(byte lowPriorityTimeout, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            return ANT_SetLowPriorityChannelSearchTimeout(unmanagedANTFramerPointer, channelNumber, lowPriorityTimeout, responseWaitTime) == 1;
        }
        /// <summary>
        /// Sets the timeout period for the channel's low-priority search, where it will not interrupt other open channels.
        /// When this period expires the channel will drop to high-priority search.
        /// </summary>
        /// <param name="lowPriorityTimeout">Timeout period in 2.5 second units</param>
        public void setLowPrioritySearchTimeout(byte lowPriorityTimeout) { setLowPrioritySearchTimeout(lowPriorityTimeout, 0); }



        /// <overloads>Adds a channel ID to the device inclusion/exclusion list</overloads>
        /// <summary>
        /// Add the given channel ID to the channel's inclusion/exclusion list.
        /// The channelID is then included or excluded from the wild card search depending on how the list is configured.
        /// Throws exception if listIndex > 3.
        /// </summary>
        /// <param name="deviceNumber">deviceNumber of the channelID to add</param>
        /// <param name="deviceTypeID">deviceType of the channelID to add</param>
        /// <param name="transmissionTypeID">transmissionType of the channelID to add</param>
        /// <param name="listIndex">Position in inclusion/exclusion list to add channel ID at (0..3)</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool includeExcludeList_addChannel(UInt16 deviceNumber, byte deviceTypeID, byte transmissionTypeID, byte listIndex, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            if (listIndex > 3)
                throw new ANT_Exception("listIndex must be 0..3");
            return ANT_AddChannelID(unmanagedANTFramerPointer, channelNumber, deviceNumber, deviceTypeID, transmissionTypeID, listIndex, responseWaitTime) == 1;
        }
        /// <summary>
        /// Add the given channel ID to the channel's inclusion/exclusion list.
        /// The channelID is then included or excluded from the wild card search depending on how the list is configured.
        /// Throws exception if listIndex > 3.
        /// </summary>
        /// <param name="deviceNumber">deviceNumber of the channelID to add</param>
        /// <param name="deviceTypeID">deviceType of the channelID to add</param>
        /// <param name="transmissionTypeID">transmissionType of the channelID to add</param>
        /// <param name="listIndex">Position in inclusion/exclusion list to add channel ID at (0..3)</param>
        public void includeExcludeList_addChannel(UInt16 deviceNumber, byte deviceTypeID, byte transmissionTypeID, byte listIndex)
        {
            includeExcludeList_addChannel(deviceNumber, deviceTypeID, transmissionTypeID, listIndex, 0);
        }


        /// <overloads>Configures the device inclusion/exclusion list</overloads>
        /// <summary>
        /// Configures the inclusion/exclusion list. If isExclusionList is true the channel IDs will be
        /// excluded from any wild card search on this channel. Otherwise the IDs are the only IDs accepted in the search.
        /// Throws exception if list size is greater than 4.
        /// </summary>
        /// <param name="listSize">The desired size of the list, max size is 4, 0=none</param>
        /// <param name="isExclusionList">True = exclusion list, False = inclusion list</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool includeExcludeList_Configure(byte listSize, bool isExclusionList, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            if (listSize > 4)
                throw new ANT_Exception("Inclusion Exclusion List has a maximum size of 4");
            return ANT_ConfigList(unmanagedANTFramerPointer, channelNumber, listSize, Convert.ToByte(isExclusionList), responseWaitTime) == 1;
        }
        /// <summary>
        /// Configures the inclusion/exclusion list. If isExclusionList is true the channel IDs will be
        /// excluded from any wild card search on this channel. Otherwise the IDs are the only IDs accepted in the search.
        /// Throws exception if list size is greater than 4.
        /// </summary>
        /// <param name="listSize">The desired size of the list, max size is 4, 0=none</param>
        /// <param name="isExclusionList">True = exclusion list, False = inclusion list</param>
        public void includeExcludeList_Configure(byte listSize, bool isExclusionList) { includeExcludeList_Configure(listSize, isExclusionList, 0); }

        /// <overloads>Enables channel encryption</overloads>
        /// <summary>
        /// Enables/disables channel encryption. Advanced bursting MUST be enabled before calling this command.
        /// </summary>
        /// <param name="encryptionMode">The desired encryption mode to be used, 1 is Enable, 2 is Enable with User Info String, 0 is Disable</param>
        /// <param name="volatileKeyIndex">The encryption key to be used in volatile memory</param>
        /// <param name="decimationRate">The decimation rate of the (slave channel period)/(master channel period).
        /// Must be 1 or greater on a slave channel, and value is N/A on a master channel.</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool encryptedChannelEnable(ANT_ReferenceLibrary.EncryptedChannelMode encryptionMode, byte volatileKeyIndex, byte decimationRate, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            return ANT_EncryptedChannelEnable(unmanagedANTFramerPointer, channelNumber, (byte)encryptionMode, volatileKeyIndex,
                                              decimationRate, responseWaitTime) == 1;
        }

        /// <summary>
        /// Enables/disables channel encryption. Advanced bursting MUST be enabled before calling this command.
        /// </summary>
        /// <param name="encryptionMode">The desired encryption mode to be used, 1 is Enable, 2 is Enable with User Info String, 0 is Disable</param>
        /// <param name="volatileKeyIndex">The encryption key to be used in volatile memory</param>
        /// <param name="decimationRate">The decimation rate of the (slave channel period)/(master channel period).
        /// Must be 1 or greater on a slave channel, and value is N/A on a master channel.</param>
        public void encryptedChannelEnable(ANT_ReferenceLibrary.EncryptedChannelMode encryptionMode,
                                           byte volatileKeyIndex, byte decimationRate)
        {
            encryptedChannelEnable(encryptionMode, volatileKeyIndex, decimationRate, 0);
        }

        /// <overloads>Add ID to encryption ID list</overloads>
        /// <summary>
        /// Add ID to encryption ID list, which is also used for the search list.
        /// </summary>
        /// <param name="encryptionID">The encryption ID to be added.</param>
        /// <param name="listIndex">Position in inclusion/exclusion list to add encryption ID at (0..3)</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool encryptionIDList_AddID(byte[] encryptionID, byte listIndex, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            if (listIndex > 3)
                throw new ANT_Exception("listIndex must be 0..3");
            return ANT_AddCryptoID(unmanagedANTFramerPointer, channelNumber, encryptionID, listIndex, responseWaitTime) == 1;
        }

        /// <summary>
        /// Add ID to encryption ID list, which is also used for the search list.
        /// </summary>
        /// <param name="encryptionID">The encryption ID to be added.</param>
        /// <param name="listIndex">Position in inclusion/exclusion list to add encryption ID at (0..3)</param>
        public void encryptionIDList_AddID(byte[] encryptionID, byte listIndex)
        {
            encryptionIDList_AddID(encryptionID, listIndex, 0);
        }

        /// <overloads>Configures encryption list</overloads>
        /// <summary>
        /// Configures the white/black list. If isBlackList is true the encryption IDs will be
        /// prevented from connecting to this channel. Otherwise the IDs are the only IDs accepted by connection attempts.
        /// Throws exception if list size is greater than 4.
        /// </summary>
        /// <param name="listSize">The desired size of the list, max size is 4, 0=none</param>
        /// <param name="isBlacklist">True = black list, False = white list</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool encryptionIDList_Configure(byte listSize, bool isBlacklist, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            if (listSize > 4)
                throw new ANT_Exception("Blacklist Whitelist has a maximum size of 4");
            return ANT_ConfigCryptoList(unmanagedANTFramerPointer, channelNumber, listSize, Convert.ToByte(isBlacklist), responseWaitTime) == 1;
        }

        /// <summary>
        /// Configures the white/black list. If isBlackList is true the encryption IDs will be
        /// prevented from connecting to this channel. Otherwise the IDs are the only IDs accepted by connection attempts.
        /// Throws exception if list size is greater than 4.
        /// </summary>
        /// <param name="listSize">The desired size of the list, max size is 4, 0=none</param>
        /// <param name="isBlacklist">True = black list, False = white list</param>
        public void encryptionIDList_Configure(byte listSize, bool isBlacklist)
        {
            encryptionIDList_Configure(listSize, isBlacklist, 0);
        }

        /// <overloads>Configures proximity search</overloads>
        /// <summary>
        /// Enables a one time proximity requirement for searching.  Only ANT devices within the set proximity bin can be acquired.
        /// Search threshold values are not correlated to specific distances as this will be dependent on the system design.
        /// This feature is not available on all ANT devices.
        /// Throws exception if given bin value is > 10.
        /// </summary>
        /// <param name="thresholdBin">Threshold bin. Value from 0-10 (0= disabled). A search threshold value of 1 (i.e. bin 1) will yield the smallest radius search and is generally recommended as there is less chance of connecting to the wrong device. </param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setProximitySearch(byte thresholdBin, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            if (thresholdBin > 10)
                throw new ANT_Exception("Threshold bin must be 0-10");
            return ANT_SetProximitySearch(unmanagedANTFramerPointer, channelNumber, thresholdBin, responseWaitTime) == 1;
        }
        /// <summary>
        /// Enables a one time proximity requirement for searching.  Only ANT devices within the set proximity bin can be acquired.
        /// Search threshold values are not correlated to specific distances as this will be dependent on the system design.
        /// Throws exception if given bin value is > 10.
        /// </summary>
        /// <param name="thresholdBin">Threshold bin. Value from 0-10 (0= disabled). A search threshold value of 1 (i.e. bin 1) will yield the smallest radius search and is generally recommended as there is less chance of connecting to the wrong device. </param>
        public void setProximitySearch(byte thresholdBin) { setProximitySearch(thresholdBin, 0);}



        /// <overloads>Configures the three operating RF frequencies for ANT frequency agility mode</overloads>
        /// <summary>
        /// This function configures the three operating RF frequencies for ANT frequency agility mode
        /// and should be used with the ADV_FrequencyAgility_0x04 extended channel assignment flag.
        /// Should not be used with shared, or Tx/Rx only channel types.
        /// This feature is not available on all ANT devices.
        /// </summary>
        /// <param name="freq1">Operating RF frequency 1</param>
        /// <param name="freq2">Operating RF frequency 2</param>
        /// <param name="freq3">Operating RF frequency 3</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool configFrequencyAgility(byte freq1, byte freq2, byte freq3, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            return ANT_ConfigFrequencyAgility(unmanagedANTFramerPointer, channelNumber, freq1, freq2, freq3, responseWaitTime) == 1;
        }
        /// <summary>
        /// This function configures the three operating RF frequencies for ANT frequency agility mode
        /// and should be used with ADV_FrequencyAgility_0x04 channel assignment flag.
        /// Should not be used with shared, or Tx/Rx only channel types.
        /// </summary>
        /// <param name="freq1">Operating RF frequency 1</param>
        /// <param name="freq2">Operating RF frequency 2</param>
        /// <param name="freq3">Operating RF frequency 3</param>
        public void configFrequencyAgility(byte freq1, byte freq2, byte freq3) { configFrequencyAgility(freq1, freq2, freq3, 0); }

        /// <overloads>Configures Selective Data Updates</overloads>
        /// <summary>
        /// Allows enabling Selective Data Update
        /// </summary>
        /// <param name="sduConfig">Specify desired previously defined SDU mask and which messages it should apply to</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool configSdu(byte sduConfig, UInt32 responseWaitTime)
        {
           if (disposed)
              throw new ObjectDisposedException("This ANTChannel object has been disposed");

           return ANT_ConfigSelectiveDataUpdate(unmanagedANTFramerPointer, channelNumber, sduConfig, responseWaitTime) == 1;
        }
        /// <summary>
        /// Allows enabling Selective Data Update
        /// </summary>
        /// <param name="sduConfig">Specify desired previously defined SDU mask and which messages it should apply to</param>
        public void configSdu(byte sduConfig) { configSdu(sduConfig, 0); }

        /// <overloads>Configures search sharing</overloads>
        /// <summary>
        /// Configures active channels with differing RF channels and/or network keys to share the active search time.
        /// The searchSharingCycles parameter defines the number of consecutive cycles each active search channel will
        /// receive before another search channel is interleaved. A value of 4 or greater is particularly recommended
        /// if search scan is enabled as well.
        /// </summary>
        /// <param name="searchSharingCycles">The number of cycles to run a scan on this channel before switching to
        /// another channel.</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setSearchSharingCycles(byte searchSharingCycles, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            return ANT_SetSearchSharingCycles(unmanagedANTFramerPointer, channelNumber, searchSharingCycles, responseWaitTime) == 1;
        }

        /// <summary>
        /// Configures active channels with differing RF channels and/or network keys to share the active search time.
        /// The searchSharingCycles parameter defines the number of consecutive cycles each active search channel will
        /// receive before another search channel is interleaved. A value of 4 or greater is particularly recommended
        /// if search scan is enabled as well.
        /// </summary>
        /// <param name="searchSharingCycles">The number of cycles to run a scan on this channel before switching to
        /// another channel.</param>
        public void setSearchSharingCycles(byte searchSharingCycles)
        {
            setSearchSharingCycles(searchSharingCycles, 0);
        }


        /// <summary>
        /// Sets the search priority for the channel. Channels with higher prioirities are scheduled for search first.
        /// </summary>
        /// <param name="priorityLevel">Desired priority level relative to other channel priorities. Higher priority values search before lower ones.</param>
        public void setChannelSearchPriority(byte priorityLevel)
        {
            setChannelSearchPriority(priorityLevel, 0);
        }

        /// <overloads>Sets the search priority for the channel.</overloads>
        /// <summary>
        /// Sets the search priority for the channel. Channels with higher prioirities are scheduled for search first.
        /// </summary>
        /// <param name="priorityLevel">Desired priority level relative to other channel priorities. Higher priority values search before lower ones.</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        public bool setChannelSearchPriority(byte priorityLevel, UInt32 responseWaitTime)
        {
            if (disposed)
                throw new ObjectDisposedException("This ANTChannel object has been disposed");

            return ANT_SetChannelSearchPriority(unmanagedANTFramerPointer, channelNumber, priorityLevel, responseWaitTime) == 1;
        }




        #endregion
    }
}
