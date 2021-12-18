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
    /// Contains all the ANT constants and enumerations for general use.
    /// Note: Where desired, in functions where enumerations are required, a byte type can be
    /// cast to the enumeration to feed the function raw byte values.
    /// IE: <c>ANTDeviceInstance.RequestMessage((RequestMessageID)0x4E));</c> would compile.
    /// </summary>
    public sealed class ANT_ReferenceLibrary
    {
        //removed class scope static modifier because it interferes with some serialization, so a private constructor is needed to enforce static use
        private ANT_ReferenceLibrary()
        {
            throw new Exception("This is a static class, do not create instances");
        }

#pragma warning disable 1591

        /// <summary>
        /// Possible port connection types.
        /// </summary>
        public enum PortType : byte
        {
            USB = 0x00,
            COM = 0x01
        };

        /// <summary>
        /// Possible framing modes.
        /// Use FramerType.basicANT unless you know you need to use another.
        /// </summary>
        public enum FramerType : byte
        {
            basicANT = 0x00,

        };

        /// <summary>
        /// Channel Type flags. A valid channel type is one BASE parameter (Slave XOR Receive)
        /// combined by '|' (bitwise OR) with any desired ADV parameters
        /// </summary>
        [Flags]
        public enum ChannelType : byte
        {
            BASE_Slave_Receive_0x00 = 0x00,
            BASE_Master_Transmit_0x10 = 0x10,
            ADV_Shared_0x20 = 0x20,
            ADV_TxRx_Only_or_RxAlwaysWildCard_0x40 = 0x40,
        };

        [Flags]
        public enum ChannelTypeExtended : byte
        {
            ADV_AlwaysSearch_0x01 = 0x01,
            ADV_FrequencyAgility_0x04 = 0x04,
            ADV_FastStart_0x10 = 0x10,
            ADV_AsyncTx_0x20 = 0x20,
        }

        /// <summary>
        /// The int status codes returned by the acknowledged and broadcast messaging functions.
        /// </summary>
        public enum MessagingReturnCode : int
        {
            Fail = 0,
            Pass = 1,
            Timeout = 2,
            Cancelled = 3,
            InvalidParams = 4,
        }

        /// <summary>
        /// Basic Channel status message codes, the bottom two bits of the received status message
        /// </summary>
        public enum BasicChannelStatusCode : byte
        {
            UNASSIGNED_0x0 = 0x0,
            ASSIGNED_0x1 = 0x1,
            SEARCHING_0x2 = 0x2,
            TRACKING_0x3 = 0x3,
        }

        /// <summary>
        /// Transmit Power offsets
        /// </summary>
        public enum TransmitPower : byte
        {
            RADIO_TX_POWER_MINUS20DB_0x00 = 0x00,
            RADIO_TX_POWER_MINUS10DB_0x01 = 0x01,
            RADIO_TX_POWER_MINUS5DB_0x02 = 0x02,
            RADIO_TX_POWER_0DB_0x03 = 0x03
        };

        /// <summary>
        /// Startup message
        /// </summary>
        public enum StartupMessage : byte
        {
            RESET_POR_0x00 = 0x00,
            RESET_RST_0x01 = 0x01,
            RESET_WDT_0x02 = 0x02,
            RESET_CMD_0x20 = 0x20,
            RESET_SYNC_0x40 = 0x40,
            RESET_SUSPEND_0x80 = 0x80
        };


        /// <summary>
        /// Message ID to request message.
        /// Note: Where desired, raw byte values can be cast to the enum type. IE: <c>(RequestMessageID)0x4E</c> will compile.
        /// </summary>
        public enum RequestMessageID : byte
        {
            VERSION_0x3E = 0x3E,
            CHANNEL_ID_0x51 = 0x51,
            CHANNEL_STATUS_0x52 = 0x52,
            CAPABILITIES_0x54 = 0x54,
            SERIAL_NUMBER_0x61 = 0x61,
            USER_NVM_0x7C = 0x7C,
        };


        /// <summary>
        /// Command Codes for SensRcore operations
        /// </summary>
        public enum SensRcoreScriptCommandCodes : byte
        {
            SCRIPT_CMD_FORMAT_0x00 = 0x00,
            SCRIPT_CMD_DUMP_0x01 = 0x01,
            SCRIPT_CMD_SET_DEFAULT_SECTOR_0x02 = 0x02,
            SCRIPT_CMD_END_SECTOR_0x03 = 0x03,
            SCRIPT_CMD_END_DUMP_0x04 = 0x04,
            SCRIPT_CMD_LOCK_0x05 = 0x05,
        };

        /// <summary>
        /// Flags for configuring device ANT library
        /// </summary>
        [Flags]
        public enum LibConfigFlags
        {
            RADIO_CONFIG_ALWAYS_0x01 = 0x01,
            MESG_OUT_INC_TIME_STAMP_0x20 = 0x20,
            MESG_OUT_INC_RSSI_0x40 = 0x40,
            MESG_OUT_INC_DEVICE_ID_0x80 = 0x80,
        }

        /// <summary>
        /// Flags for configuring advanced bursting features.
        /// </summary>
        [Flags]
        public enum AdvancedBurstConfigFlags : uint
        {
            FREQUENCY_HOP_ENABLE = 0x00000001,
        };

        public enum EncryptedChannelMode : byte
        {
            DISABLE = 0x00,
            ENABLE = 0x01,
            ENABLE_USER_INFO = 0x02,
        };

        public enum EncryptionInfo : byte
        {
            ENCRYPTION_ID = 0x00,
            USER_INFO_STRING = 0x01,
            RANDOM_NUMBER_SEED = 0x02,
        };

        public enum EncryptionNVMOp : byte
        {
            LOAD_KEY_FROM_NVM = 0x00,
            STORE_KEY_TO_NVM = 0x01,
        }

        /// <summary>
        /// Event groups for configuring Event Buffering
        /// </summary>
        [Flags]
        public enum EventBufferConfig : byte
        {
           BUFFER_LOW_PRIORITY_EVENTS = 0x00,
           BUFFER_ALL_EVENTS = 0x01
        };

        /// <summary>
        /// MessageIDs for reference
        /// </summary>
        public enum ANTMessageID : byte
        {
            INVALID_0x00 = 0x00,
            EVENT_0x01 = 0x01,

            VERSION_0x3E = 0x3E,
            RESPONSE_EVENT_0x40 = 0x40,

            UNASSIGN_CHANNEL_0x41 = 0x41,
            ASSIGN_CHANNEL_0x42 = 0x42,
            CHANNEL_MESG_PERIOD_0x43 = 0x43,
            CHANNEL_SEARCH_TIMEOUT_0x44 = 0x44,
            CHANNEL_RADIO_FREQ_0x45 = 0x45,
            NETWORK_KEY_0x46 = 0x46,
            RADIO_TX_POWER_0x47 = 0x47,
            RADIO_CW_MODE_0x48 = 0x48,

            SYSTEM_RESET_0x4A = 0x4A,
            OPEN_CHANNEL_0x4B = 0x4B,
            CLOSE_CHANNEL_0x4C = 0x4C,
            REQUEST_0x4D = 0x4D,

            BROADCAST_DATA_0x4E = 0x4E,
            ACKNOWLEDGED_DATA_0x4F = 0x4F,
            BURST_DATA_0x50 = 0x50,

            CHANNEL_ID_0x51 = 0x51,
            CHANNEL_STATUS_0x52 = 0x52,
            RADIO_CW_INIT_0x53 = 0x53,
            CAPABILITIES_0x54 = 0x54,

            STACKLIMIT_0x55 = 0x55,

            SCRIPT_DATA_0x56 = 0x56,
            SCRIPT_CMD_0x57 = 0x57,

            ID_LIST_ADD_0x59 = 0x59,
            ID_LIST_CONFIG_0x5A = 0x5A,
            OPEN_RX_SCAN_0x5B = 0x5B,


            EXT_BROADCAST_DATA_0x5D = 0x5D,
            EXT_ACKNOWLEDGED_DATA_0x5E = 0x5E,
            EXT_BURST_DATA_0x5F = 0x5F,

            CHANNEL_RADIO_TX_POWER_0x60 = 0x60,
            GET_SERIAL_NUM_0x61 = 0x61,
            GET_TEMP_CAL_0x62 = 0x62,
            SET_LP_SEARCH_TIMEOUT_0x63 = 0x63,
            SERIAL_NUM_SET_CHANNEL_ID_0x65 = 0x65,
            RX_EXT_MESGS_ENABLE_0x66 = 0x66,
            ENABLE_LED_FLASH_0x68 = 0x68,
            XTAL_ENABLE_0x6D = 0x6D,
            STARTUP_MESG_0x6F = 0x6F,
            AUTO_FREQ_CONFIG_0x70 = 0x70,
            PROX_SEARCH_CONFIG_0x71 = 0x71,

            ADV_BURST_DATA_0x72 = 0x72,
            EVENT_BUFFER_CONFIG_0x74 = 0x74,
            SET_SEARCH_PRIORITY_LEVEL_0x75 = 0x75,

            HIGH_DUTY_SEARCH_CONFIG_0x77 = 0x77,
            ADV_BURST_CONFIG_0x78 = 0x78,
            EVENT_FILTER_CONFIG_0x79 = 0x79,
            SDU_CONFIG_0x7A = 0x7A,
            SET_SDU_MASK_0x7B = 0x7B,
            USER_NVM_CONFIG_0x7C = 0x7C,
            ENABLE_ENCRYPTION_0x7D = 0x7D,
            SET_ENCRYPTION_KEY_0x7E = 0x7E,
            SET_ENCRYPTION_INFO_0x7F = 0x7F,

            SET_SEARCH_SHARING_CYCLES_0x81 = 0x81,
            ENCRYPTION_KEY_NVM_OPERATION_0x83 = 0x83,


            FIT1_SET_AGC_0x8F = 0x8F,

            /// <summary>
            /// *** CONFLICT: w/ Sensrcore, Fit1 will never have sensrcore enabled
            /// </summary>
            FIT1_SET_EQUIP_STATE_0x91 = 0x91,

            // SensRcore Messages
            SET_CHANNEL_INPUT_MASK_0x90 = 0x90,
            SET_CHANNEL_DATA_TYPE_0x91 = 0x91,
            READ_PINS_FOR_SECT_0x92 = 0x92,
            TIMER_SELECT_0x93 = 0x93,
            ATOD_SETTINGS_0x94 = 0x94,
            SET_SHARED_ADDRESS_0x95 = 0x95,

            RSSI_POWER_0xC0 = 0xC0,
            RSSI_BROADCAST_DATA_0xC1 = 0xC1,
            RSSI_ACKNOWLEDGED_DATA_0xC2 = 0xC2,
            RSSI_BURST_DATA_0xC3 = 0xC3,
            RSSI_SEARCH_THRESHOLD_0xC4 = 0xC4,

            SLEEP_0xC5 = 0xC5,
            SET_USB_INFO_0xC7 = 0xC7,

        };

        /// <summary>
        /// EventIDs for reference
        /// </summary>
        public enum ANTEventID : byte
        {
            RESPONSE_NO_ERROR_0x00 = 0x00,
            NO_EVENT_0x00 = 0x00,

            EVENT_RX_SEARCH_TIMEOUT_0x01 = 0x01,
            EVENT_RX_FAIL_0x02 = 0x02,
            EVENT_TX_0x03 = 0x03,
            EVENT_TRANSFER_RX_FAILED_0x04 = 0x04,
            EVENT_TRANSFER_TX_COMPLETED_0x05 = 0x05,
            EVENT_TRANSFER_TX_FAILED_0x06 = 0x06,
            EVENT_CHANNEL_CLOSED_0x07 = 0x07,
            EVENT_RX_FAIL_GO_TO_SEARCH_0x08 = 0x08,
            EVENT_CHANNEL_COLLISION_0x09 = 0x09,
            /// <summary>
            /// a pending transmit transfer has begun
            /// </summary>
            EVENT_TRANSFER_TX_START_0x0A = 0x0A,

            EVENT_CHANNEL_ACTIVE_0x0F = 0x0F,

            EVENT_TRANSFER_TX_COMPLETED_RSSI_0x10 = 0x10,


            /// <summary>
            /// returned on attempt to perform an action from the wrong channel state
            /// </summary>
            CHANNEL_IN_WRONG_STATE_0x15 = 0x15,
            /// <summary>
            /// returned on attempt to communicate on a channel that is not open
            /// </summary>
            CHANNEL_NOT_OPENED_0x16 = 0x16,
            /// <summary>
            /// returned on attempt to open a channel without setting the channel ID
            /// </summary>
            CHANNEL_ID_NOT_SET_0x18 = 0x18,
            /// <summary>
            /// returned when attempting to start scanning mode
            /// </summary>
            CLOSE_ALL_CHANNELS_0x19 = 0x19,

            /// <summary>
            /// returned on attempt to communicate on a channel with a TX transfer in progress
            /// </summary>
            TRANSFER_IN_PROGRESS_0x1F = 0x1F,
            /// <summary>
            /// returned when sequence number is out of order on a Burst transfer
            /// </summary>
            TRANSFER_SEQUENCE_NUMBER_ERROR_0x20 = 0x20,
            TRANSFER_IN_ERROR_0x21 = 0x21,
            TRANSFER_BUSY_0x22 = 0x22,

            /// <summary>
            /// returned if a data message is provided that is too large
            /// </summary>
            MESSAGE_SIZE_EXCEEDS_LIMIT_0x27 = 0x27,
            /// <summary>
            /// returned when the message has an invalid parameter
            /// </summary>
            INVALID_MESSAGE_0x28 = 0x28,
            /// <summary>
            /// returned when an invalid network number is provided
            /// </summary>
            INVALID_NETWORK_NUMBER_0x29 = 0x29,
            /// <summary>
            /// returned when the provided list ID or size exceeds the limit
            /// </summary>
            INVALID_LIST_ID_0x30 = 0x30,
            /// <summary>
            /// returned when attempting to transmit on channel 0 when in scan mode
            /// </summary>
            INVALID_SCAN_TX_CHANNEL_0x31 = 0x31,
            /// <summary>
            /// returned when an invalid parameter is specified in a configuration message
            /// </summary>
            INVALID_PARAMETER_PROVIDED_0x33 = 0x33,

            /// <summary>
            /// ANT event que has overflowed and lost 1 or more events
            /// </summary>
            EVENT_QUE_OVERFLOW_0x35 = 0x35,


            /// <summary>
            /// error writing to script
            /// </summary>
            SCRIPT_FULL_ERROR_0x40 = 0x40,
            /// <summary>
            /// error writing to script
            /// </summary>
            SCRIPT_WRITE_ERROR_0x41 = 0x41,
            /// <summary>
            /// error accessing script page
            /// </summary>
            SCRIPT_INVALID_PAGE_ERROR_0x42 = 0x42,
            /// <summary>
            /// the scripts are locked and can't be dumped
            /// </summary>
            SCRIPT_LOCKED_ERROR_0x43 = 0x43,


            /// <summary>
            /// Fit1 only event added for timeout of the pairing state after the Fit module becomes active
            /// </summary>
            FIT_ACTIVE_SEARCH_TIMEOUT_0x60 = 0x60,
            /// <summary>
            /// Fit1 only
            /// </summary>
            FIT_WATCH_PAIR_0x61 = 0x61,
            /// <summary>
            /// Fit1 only
            /// </summary>
            FIT_WATCH_UNPAIR_0x62 = 0x62,
        };


        /// <summary>
        /// PIDs for reference
        /// </summary>
        public enum USB_PID : ushort
        {
            ANT_INTERFACE_BOARD = 0x4102,
            ANT_ARCT = 0x4103
        };

        public const byte MAX_MESG_SIZE = 41; // Must match MARSHALL_MESG_MAX_SIZE_VALUE in unmanagedWrapper dll_exports.cpp
#pragma warning restore 1591
    }
}
