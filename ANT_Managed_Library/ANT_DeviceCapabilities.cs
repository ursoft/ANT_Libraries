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

namespace ANT_Managed_Library
{
    /// <summary>
    /// Container for all the device capability information, returned from an ANTDevice
    /// </summary>
    public class ANT_DeviceCapabilities
    {
        /// <summary>
        /// Number of channels available
        /// </summary>
        public readonly byte maxANTChannels;   //byte 0

        /// <summary>
        /// Number of simultaneous networks allowed
        /// </summary>
        public readonly byte maxNetworks;      //byte 1

        //TODO: Document all the capabilities
#pragma warning disable 1591
        //Basic Capabilities - byte 2
        public readonly bool NoReceiveChannels;
        public readonly bool NoTransmitChannels;
        public readonly bool NoReceiveMessages;
        public readonly bool NoTransmitMessages;
        public readonly bool NoAckMessages;
        public readonly bool NoBurstMessages;

        //Advanced Capabilities - byte 3
        public readonly bool PrivateNetworks;
        public readonly bool SerialNumber;
        public readonly bool perChannelTransmitPower;
        public readonly bool lowPrioritySearch;
        public readonly bool ScriptSupport;
        public readonly bool SearchList;

        //Advanced Capabilities 2 - byte 4
        public readonly bool OnboardLED;
        public readonly bool ExtendedMessaging;
        public readonly bool ScanModeSupport;
        public readonly bool ExtendedChannelAssignment;
        public readonly bool ProximitySearch;
        public readonly bool FS;
        public readonly bool FIT;

        //Advanced Capabilities 3 - byte 6
        public readonly bool AdvancedBurst;
        public readonly bool EventBuffering;
        public readonly bool EventFiltering;
        public readonly bool HighDutySearch;
        public readonly bool SearchSharing;
        public readonly bool SelectiveDataUpdate;
        public readonly bool SingleChannelEncryption;


#pragma warning restore 1591

        /// <summary>
        /// Number of SensRcore data channels available
        /// </summary>
        public readonly byte maxDataChannels;  //byte 5

        internal ANT_DeviceCapabilities(byte[] capabilitiesData)
        {
            if (capabilitiesData.Length != 16)
                throw new ANT_Exception("This function only decodes capabilities data of exactly length 16");

            maxANTChannels = capabilitiesData[0];
            maxNetworks = capabilitiesData[1];

            //Now decode all the bit fields
            //Basic Capabilities - 3rd byte
            NoReceiveChannels = ((capabilitiesData[2] & (byte)BasicCapabilitiesMasks.NO_RX_CHANNELS) != 0);
            NoTransmitChannels = ((capabilitiesData[2] & (byte)BasicCapabilitiesMasks.NO_TX_CHANNELS) != 0);
            NoReceiveMessages = ((capabilitiesData[2] & (byte)BasicCapabilitiesMasks.NO_RX_MESSAGES) != 0);
            NoTransmitMessages = ((capabilitiesData[2] & (byte)BasicCapabilitiesMasks.NO_TX_MESSAGES) != 0);
            NoAckMessages = ((capabilitiesData[2] & (byte)BasicCapabilitiesMasks.NO_ACKD_MESSAGES) != 0);
            NoBurstMessages = ((capabilitiesData[2] & (byte)BasicCapabilitiesMasks.NO_BURST_TRANSFER) != 0);

            //Advanced Capabilities - 4th byte
            PrivateNetworks = ((capabilitiesData[3] & (byte)AdvancedCapabilitiesMasks.NETWORK_CAPABLE) != 0);
            SerialNumber = ((capabilitiesData[3] & (byte)AdvancedCapabilitiesMasks.SERIAL_NUMBER_CAPABLE) != 0);
            perChannelTransmitPower = ((capabilitiesData[3] & (byte)AdvancedCapabilitiesMasks.PER_CHANNEL_TX_POWER_CAPABLE) != 0);
            lowPrioritySearch = ((capabilitiesData[3] & (byte)AdvancedCapabilitiesMasks.LOW_PRIORITY_SEARCH_CAPABLE) != 0);
            ScriptSupport = ((capabilitiesData[3] & (byte)AdvancedCapabilitiesMasks.SCRIPT_CAPABLE) != 0);
            SearchList = ((capabilitiesData[3] & (byte)AdvancedCapabilitiesMasks.SEARCH_LIST_CAPABLE) != 0);

            //Advanced Capabilities 2 - 5th byte
            OnboardLED = ((capabilitiesData[4] & (byte)AdvancedCapabilities2Masks.LED_CAPABLE) != 0);
            ExtendedMessaging = ((capabilitiesData[4] & (byte)AdvancedCapabilities2Masks.EXT_MESSAGE_CAPABLE) != 0);
            ScanModeSupport = ((capabilitiesData[4] & (byte)AdvancedCapabilities2Masks.SCAN_MODE_CAPABLE) != 0);
            ProximitySearch = ((capabilitiesData[4] & (byte)AdvancedCapabilities2Masks.PROX_SEARCH_CAPABLE) != 0);
            ExtendedChannelAssignment = ((capabilitiesData[4] & (byte)AdvancedCapabilities2Masks.EXT_ASSIGN_CAPABLE) != 0);
            FS = ((capabilitiesData[4] & (byte)AdvancedCapabilities2Masks.FS_ANTFS_ENABLED) != 0);
            FIT = ((capabilitiesData[4] & (byte)AdvancedCapabilities2Masks.FIT1_CAPABLE) != 0);

            //SensorCore Data Channels - 6th byte
            maxDataChannels = capabilitiesData[5];

            //Advanced Capabilities 3 - 7th byte
            AdvancedBurst = ((capabilitiesData[6] & (byte)AdvancedCapabilities3Masks.ADVANCED_BURST_CAPABLE) != 0);
            EventBuffering = ((capabilitiesData[6] & (byte)AdvancedCapabilities3Masks.EVENT_BUFFERING_CAPABLE) != 0);
            EventFiltering = ((capabilitiesData[6] & (byte)AdvancedCapabilities3Masks.EVENT_FILTERING_CAPABLE) != 0);
            HighDutySearch = ((capabilitiesData[6] & (byte)AdvancedCapabilities3Masks.HIGH_DUTY_SEARCH_CAPABLE) != 0);
            SearchSharing = ((capabilitiesData[6] & (byte)AdvancedCapabilities3Masks.ACTIVE_SEARCH_SHARING_CAPABLE) != 0);
            SelectiveDataUpdate = ((capabilitiesData[6] & (byte)AdvancedCapabilities3Masks.SELECTIVE_DATA_UPDATE) != 0);
            SingleChannelEncryption = ((capabilitiesData[6] & (byte)AdvancedCapabilities3Masks.ENCRYPTED_CHANNEL_CAPABLE) != 0);

        }

        /// <summary>
        /// Prints a string containing a formatted, readable version of all the capabilities
        /// </summary>
        public String printCapabilities()
        {
            StringBuilder returnString = new StringBuilder();
            returnString.AppendLine( "Max ANT Channels: " + maxANTChannels);
            returnString.AppendLine( "Max Networks: " + maxNetworks);
            returnString.AppendLine( "Max Data Channels: " + maxDataChannels);
            returnString.AppendLine( "Capabilities:");
            //Basic
            if(NoReceiveChannels)
                returnString.AppendLine("-No Receive Channels");
            if (NoTransmitChannels)
                returnString.AppendLine("-No Transmit Channels");
            if (NoReceiveMessages)
                returnString.AppendLine("-No Receive Messages");
            if (NoTransmitMessages)
                returnString.AppendLine("-No Transmit Messages");
            if (NoAckMessages)
                returnString.AppendLine("-No Acknowledged Messaging");
            if (NoBurstMessages)
                returnString.AppendLine("-No Burst Messaging");
            //Advanced
            if (PrivateNetworks)
                returnString.AppendLine("-Private Networks");
            if (SerialNumber)
                returnString.AppendLine("-Serial Number");
            if (perChannelTransmitPower)
                returnString.AppendLine("-Per Channel Tx Power");
            if (lowPrioritySearch)
                returnString.AppendLine("-Low Priority Search");
            if (ScriptSupport)
                returnString.AppendLine("-Script Support");
            if (SearchList)
                returnString.AppendLine("-Search List");
            //Advanced 2
            if (OnboardLED)
                returnString.AppendLine("-Onboard LED");
            if (ExtendedMessaging)
                returnString.AppendLine("-Extended Messaging");
            if (ScanModeSupport)
                returnString.AppendLine("-Scan Channel Support");
            if (ExtendedChannelAssignment)
                returnString.AppendLine("-Ext Channel Assignment");
            if (ProximitySearch)
                returnString.AppendLine("-Proximity Search");
            if (FS)
                returnString.AppendLine("-FS");
            if (FIT)
                returnString.AppendLine("-FIT");
            //Advanced 3
            if (AdvancedBurst)
                returnString.AppendLine("-Advanced Burst");
            if (EventBuffering)
                returnString.AppendLine("-Event Buffering");
            if (EventFiltering)
                returnString.AppendLine("-Event Filtering");
            if (HighDutySearch)
                returnString.AppendLine("-High Duty Search");
            if (SearchSharing)
                returnString.AppendLine("-Search Sharing");
            if (SelectiveDataUpdate)
                returnString.AppendLine("-Selective Data Update");
            if (SingleChannelEncryption)
                returnString.AppendLine("-Single Channel Encryption");

            return returnString.ToString();
        }

        /// <summary>
        /// Prints a compact string containing a formatted, readable version of just the standard capabilities
        /// </summary>
        public String printStandardCapabilities()
        {
           StringBuilder returnString = new StringBuilder();
           returnString.AppendLine("Max ANT Channels: " + maxANTChannels);
           returnString.AppendLine("Max Networks: " + maxNetworks);
           returnString.AppendLine("Max Data Channels: " + maxDataChannels);
           //Basic
           if (NoReceiveChannels)
              returnString.AppendLine("-No Receive Channels");
           if (NoTransmitChannels)
              returnString.AppendLine("-No Transmit Channels");
           if (NoReceiveMessages)
              returnString.AppendLine("-No Receive Messages");
           if (NoTransmitMessages)
              returnString.AppendLine("-No Transmit Messages");
           if (NoAckMessages)
              returnString.AppendLine("-No Acknowledged Messaging");
           if (NoBurstMessages)
              returnString.AppendLine("-No Burst Messaging");

           return returnString.ToString();
        }

        /// <summary>
        /// Prints a string containing a formatted, readable version of all the advanced capabilities
        /// </summary>
        public String printAdvCapabilities()
        {
           StringBuilder returnString = new StringBuilder();
           returnString.AppendLine("Adv Capabilities:");
           //Basic
           if (NoReceiveChannels)
              returnString.AppendLine("-No Receive Channels");
           if (NoTransmitChannels)
              returnString.AppendLine("-No Transmit Channels");
           if (NoReceiveMessages)
              returnString.AppendLine("-No Receive Messages");
           if (NoTransmitMessages)
              returnString.AppendLine("-No Transmit Messages");
           if (NoAckMessages)
              returnString.AppendLine("-No Acknowledged Messaging");
           if (NoBurstMessages)
              returnString.AppendLine("-No Burst Messaging");
           //Advanced
           if (PrivateNetworks)
              returnString.AppendLine("-Private Networks");
           if (SerialNumber)
              returnString.AppendLine("-Serial Number");
           if (perChannelTransmitPower)
              returnString.AppendLine("-Per Channel Tx Power");
           if (lowPrioritySearch)
              returnString.AppendLine("-Low Priority Search");
           if (ScriptSupport)
              returnString.AppendLine("-Script Support");
           if (SearchList)
              returnString.AppendLine("-Search List");
           //Advanced 2
           if (OnboardLED)
              returnString.AppendLine("-Onboard LED");
           if (ExtendedMessaging)
              returnString.AppendLine("-Extended Messaging");
           if (ScanModeSupport)
              returnString.AppendLine("-Scan Channel Support");
           if (ExtendedChannelAssignment)
              returnString.AppendLine("-Ext Channel Assignment");
           if (ProximitySearch)
              returnString.AppendLine("-Proximity Search");
           if (FS)
              returnString.AppendLine("-FS");
           if (FIT)
              returnString.AppendLine("-FIT");
           //Advanced 3
           if (AdvancedBurst)
              returnString.AppendLine("-Advanced Burst");
           if (EventBuffering)
              returnString.AppendLine("-Event Buffering");
           if (EventFiltering)
              returnString.AppendLine("-Event Filtering");
           if (HighDutySearch)
              returnString.AppendLine("-High Duty Search");
           if (SearchSharing)
               returnString.AppendLine("-Search Sharing");
           if (SelectiveDataUpdate)
              returnString.AppendLine("-Selective Data Update");
           if (SingleChannelEncryption)
              returnString.AppendLine("-Single Channel Encryption");

           return returnString.ToString();
        }

        /// <summary>
        /// Returns a formatted, readable string of all the capabilities
        /// </summary>
        public override String ToString() { return printCapabilities(); }

#pragma warning disable 1591
        /// <summary>
        /// Basic Capabilities Masks (3rd Byte)
        /// </summary>
        public enum BasicCapabilitiesMasks : byte
        {
            NO_RX_CHANNELS = 0x01,
            NO_TX_CHANNELS = 0x02,
            NO_RX_MESSAGES = 0x04,
            NO_TX_MESSAGES = 0x08,
            NO_ACKD_MESSAGES = 0x10,
            NO_BURST_TRANSFER = 0x20
        };

        /// <summary>
        /// Advanced Capabilities Masks 1 (4th Byte)
        /// </summary>
        public enum AdvancedCapabilitiesMasks : byte
        {
            OVERUN_UNDERRUN = 0x01,     // Support for this functionality has been dropped
            NETWORK_CAPABLE = 0x02,
            AP1_VERSION_2 = 0x04,     // This Version of the AP1 does not support transmit and only had a limited release
            SERIAL_NUMBER_CAPABLE = 0x08,
            PER_CHANNEL_TX_POWER_CAPABLE = 0x10,
            LOW_PRIORITY_SEARCH_CAPABLE = 0x20,
            SCRIPT_CAPABLE = 0x40,
            SEARCH_LIST_CAPABLE = 0x80
        };

        /// <summary>
        /// Advanced Capabilities Masks 2 (5th Byte)
        /// </summary>
        public enum AdvancedCapabilities2Masks : byte
        {
            LED_CAPABLE = 0x01,
            EXT_MESSAGE_CAPABLE = 0x02,
            SCAN_MODE_CAPABLE = 0x04,
            RESERVED = 0x08,
            PROX_SEARCH_CAPABLE = 0x10,
            EXT_ASSIGN_CAPABLE = 0x20,
            FS_ANTFS_ENABLED = 0x40,
            FIT1_CAPABLE = 0x80,
        };

        /// <summary>
        /// Advanced Capabilities Masks 3 (7th Byte)
        /// </summary>
        public enum AdvancedCapabilities3Masks : byte
        {
            ADVANCED_BURST_CAPABLE = 0x01,
            EVENT_BUFFERING_CAPABLE = 0x02,
            EVENT_FILTERING_CAPABLE = 0x04,
            HIGH_DUTY_SEARCH_CAPABLE = 0x08,
            ACTIVE_SEARCH_SHARING_CAPABLE = 0x10,
            SELECTIVE_DATA_UPDATE = 0x40,
            ENCRYPTED_CHANNEL_CAPABLE = 0x80,
        };

#pragma warning restore 1591
    };
}
