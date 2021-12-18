/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/
using System;
using System.Runtime.InteropServices;

namespace ANT_Managed_Library
{
    #region Delegates for channel events

    /// <summary>
    /// Delegate for the DeviceNotification event
    /// </summary>
    /// <param name="notification">The notification code for the current event</param>
    /// <param name="notificationInfo">An object that optionally holds more information about the current event</param>
    public delegate void dDeviceNotificationHandler(ANT_Device.DeviceNotificationCode notification, Object notificationInfo);

    /// <summary>
    /// Delegate for Channel Response Event for forwarding the raw msg struct. If you are coding in C# use the other response event version.
    /// </summary>
    /// <param name="message">Message bytes received from device</param>
    /// <param name="messageSize">Length of data in message structure</param>
    public delegate void dRawChannelResponseHandler(ANT_Device.ANTMessage message, ushort messageSize);

    /// <summary>
    /// Delegate for Channel Response Event
    /// </summary>
    /// <param name="response">Message details received from device</param>
    public delegate void dChannelResponseHandler(ANT_Response response);

    #endregion


    //TODO we need to refactor this interface to change the return type for those functions that are un-'safe' to a return value instead of a bool so it is usable for the device sharing service implementation
    /// <summary>
    /// Interface for an ANT channel. Allows classes to use different channel implementations behind the interface.
    /// </summary>
    [Obsolete("Note: NOT OBSOLETE! This warning is just to inform you that IANT_Channel interface is under development and is subject to change")]
    public interface IANT_Channel: IDisposable
    {
        /// <summary>
        /// Returns the underlying C++ ANT framer reference that this channel uses for messaging. Useful to pass to unmanaged C++ implementations.
        /// </summary>
        /// <returns>Pointer to the C++ ANT framer for messaging</returns>
        IntPtr getUnmgdFramer();

        /// <summary>
        /// Returns the channel number of this chanel
        /// </summary>
        /// <returns>The channel number that is sent with messages on the ANT messaging layer</returns>
        byte getChannelNum();



        #region ChannelEventCallback Variables

        /// <summary>
        /// This event is fired whenever there are events on the device level that may impact the channel.
        /// Events that currently occur (Event, value of notification info Object):
        ///     Reset, null
        ///     Shutdown, null
        /// </summary>
        event dDeviceNotificationHandler DeviceNotification;


        /// <summary>
        /// The channel callback event for forwarding the raw msg struct. Triggered every time a message is received from the ANT device.
        /// Examples include transmit and receive messages. If you are coding in C# use the other response event version.
        /// </summary>
        event dRawChannelResponseHandler rawChannelResponse;


        /// <summary>
        /// The channel callback event. Triggered every time a message is received from the ANT device.
        /// Examples include transmit and receive messages.
        /// </summary>
        event dChannelResponseHandler channelResponse;

        #endregion


        #region ANT Channel Functions

        /// <summary>
        /// Returns current channel status.
        /// Throws exception on timeout.
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        ANT_ChannelStatus requestStatus(UInt32 responseWaitTime);


        /// <summary>
        /// Returns the channel ID
        /// Throws exception on timeout
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns></returns>
        ANT_ChannelID requestID(UInt32 responseWaitTime);


        /// <overloads>Assign channel</overloads>
        /// <summary>
        /// Assign an ANT channel along with its main parameters.
        /// Throws exception if the network number is invalid.
        /// </summary>
        /// <param name="channelTypeByte">Channel Type byte</param>
        /// <param name="networkNumber">Network to assign to channel, must be less than device's max networks-1</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        bool assignChannel(ANT_ReferenceLibrary.ChannelType channelTypeByte, byte networkNumber, UInt32 responseWaitTime);


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
        bool assignChannelExt(ANT_ReferenceLibrary.ChannelType channelTypeByte, byte networkNumber, ANT_ReferenceLibrary.ChannelTypeExtended extAssignByte, UInt32 responseWaitTime);


        /// <overloads>Unassign channel</overloads>
        /// <summary>
        /// Unassign this channel.
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        bool unassignChannel(UInt32 responseWaitTime);


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
        bool setChannelID(UInt16 deviceNumber, bool pairingEnabled, byte deviceTypeID, byte transmissionTypeID, UInt32 responseWaitTime);


        /// <overloads>Sets the Channel ID, using serial number as device number</overloads>
        /// <summary>
        /// Identical to setChannelID, except last two bytes of serial number are used for device number.
        /// Not available on all ANT devices.
        /// Throws exception if device type is > 127.
        /// </summary>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        bool setChannelID_UsingSerial(bool pairingEnabled, byte deviceTypeID, byte transmissionTypeID, UInt32 waitResponseTime);


        /// <overloads>Sets channel message period</overloads>
        /// <summary>
        /// Set this channel's messaging period
        /// </summary>
        /// <param name="messagePeriod_32768unitspersecond">Desired period in seconds * 32768</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        bool setChannelPeriod(UInt16 messagePeriod_32768unitspersecond, UInt32 responseWaitTime);


        /// <overloads>Sets the RSSI threshold (ARCT)</overloads>
        /// <summary>
        /// Set this channel's RSSI threshold (ARCT)
        /// </summary>
        /// <param name="thresholdRSSI">Desired RSSI threshold value</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        bool setSearchThresholdRSSI(byte thresholdRSSI, UInt32 responseWaitTime);


        /// <overloads>Sets channel RF Frequency</overloads>
        /// <summary>
        /// Set this channel's RF frequency, with the given offset from 2400Mhz.
        /// Note: Changing this frequency may affect the ability to certify the product in certain areas of the world.
        /// </summary>
        /// <param name="RFFreqOffset">Offset to add to 2400Mhz</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        bool setChannelFreq(byte RFFreqOffset, UInt32 responseWaitTime);


        /// <overloads>Sets the channel transmission power</overloads>
        /// <summary>
        /// Set the transmission power of this channel
        /// Throws exception if device is not capable of per-channel transmit power.
        /// </summary>
        /// <param name="transmitPower">Transmission power to set to</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        bool setChannelTransmitPower(ANT_ReferenceLibrary.TransmitPower transmitPower, UInt32 responseWaitTime);


        /// <overloads>Sets the channel search timeout</overloads>
        /// <summary>
        /// Set the search timeout
        /// </summary>
        /// <param name="searchTimeout">timeout in 2.5 second units (in newer devices 255=infinite)</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        bool setChannelSearchTimeout(byte searchTimeout, UInt32 responseWaitTime);


        /// <overloads>Opens the channel</overloads>
        /// <summary>
        /// Opens this channel
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        bool openChannel(UInt32 responseWaitTime);


        /// <overloads>Sends broadcast message</overloads>
        /// <summary>
        /// Sends the given data on the broadcast transmission.
        /// Throws exception if data > 8-bytes in length
        /// </summary>
        /// <param name="data">data to send (length 8 or less)</param>
        bool sendBroadcastData(byte[] data);


        /// <overloads>Sends acknowledged message</overloads>
        /// <summary>
        /// Sends the given data as an acknowledged transmission. Returns: 0=fail, 1=pass, 2=timeout, 3=cancelled
        /// Throws exception if data > 8-bytes in length
        /// </summary>
        /// <param name="data">data to send (length 8 or less)</param>
        /// <param name="ackWaitTime">Time in ms to wait for acknowledgement</param>
        /// <returns>0=fail, 1=pass, 2=timeout, 3=cancelled</returns>
        ANT_ReferenceLibrary.MessagingReturnCode sendAcknowledgedData(byte[] data, UInt32 ackWaitTime);


        /// <overloads>Sends burst transfer</overloads>
        /// <summary>
        /// Sends the given data as a burst transmission. Returns: 0=fail, 1=pass, 2=timeout, 3=cancelled
        /// </summary>
        /// <param name="data">data to send, can be any length</param>
        /// <param name="completeWaitTime">Time in ms to wait for completion of transfer</param>
        /// <returns>0=fail, 1=pass, 2=timeout, 3=cancelled</returns>
        ANT_ReferenceLibrary.MessagingReturnCode sendBurstTransfer(byte[] data, UInt32 completeWaitTime);




        /// <overloads>Sends extended broadcast message</overloads>
        /// <summary>
        /// Sends the given data as an extended broadcast transmission.
        /// Throws exception if data > 8-bytes in length
        /// </summary>
        /// <param name="deviceNumber">Device number of channel ID to send to</param>
        /// <param name="deviceTypeID">Device type of channel ID to send to</param>
        /// <param name="transmissionTypeID">Transmission type of channel ID to send to</param>
        /// <param name="data">data to send (length 8 or less)</param>
            bool sendExtBroadcastData(UInt16 deviceNumber, byte deviceTypeID, byte transmissionTypeID, byte[] data);



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
        ANT_ReferenceLibrary.MessagingReturnCode sendExtAcknowledgedData(UInt16 deviceNumber, byte deviceTypeID, byte transmissionTypeID, byte[] data, UInt32 ackWaitTime);


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
        ANT_ReferenceLibrary.MessagingReturnCode sendExtBurstTransfer(UInt16 deviceNumber, byte deviceTypeID, byte transmissionTypeID, byte[] data, UInt32 completeWaitTime);


        /// <overloads>Closes the channel</overloads>
        /// <summary>
        /// Close this channel
        /// </summary>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        bool closeChannel(UInt32 responseWaitTime);


        /// <overloads>Sets the channel low priority search timeout</overloads>
        /// <summary>
        /// Sets the search timeout for the channel's low-priority search, where it will not interrupt other open channels.
        /// When this period expires the channel will drop to high-priority search.
        /// This feature is not available in all ANT devices.
        /// </summary>
        /// <param name="lowPriorityTimeout">Timeout period in 2.5 second units</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        bool setLowPrioritySearchTimeout(byte lowPriorityTimeout, UInt32 responseWaitTime);


        /// <overloads>Adds a channel ID to the device inclusion/exclusion list</overloads>
        /// <summary>
        /// Add the given channel ID to the channel's inclusion/exclusion list.
        /// The channelID is then included or excluded from the wild card search depending on how the list is configured.
        /// Throws exception if listIndex > 3.
        /// </summary>
        /// <param name="deviceNumber">deviceNumber of the channelID to add</param>
        /// <param name="deviceTypeID">deviceType of the channelID to add</param>
        /// <param name="transmissionTypeID">transmissionType of the channelID to add</param>
        /// <param name="listIndex">position in inclusion/exclusion list to add channelID at (Max size of list is 4)</param>
        /// <param name="responseWaitTime">Time to wait for device success response</param>
        /// <returns>True on success. Note: Always returns true with a response time of 0</returns>
        bool includeExcludeList_addChannel(UInt16 deviceNumber, byte deviceTypeID, byte transmissionTypeID, byte listIndex, UInt32 responseWaitTime);



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
        bool includeExcludeList_Configure(byte listSize, bool isExclusionList, UInt32 responseWaitTime);


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
        bool setProximitySearch(byte thresholdBin, UInt32 responseWaitTime);


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
        bool configFrequencyAgility(byte freq1, byte freq2, byte freq3, UInt32 responseWaitTime);





        #endregion
    }
}
