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
    /// Container for all the information passed from an ANT device callback function
    /// </summary>
    public class ANT_Response
    {
        /// <summary>
        /// The object that created this response (ie: The corresponding ANTChannel or ANTDevice instance).
        /// </summary>
        public object sender;

        /// <summary>
        /// The channel parameter received in the message. Note: For some messages this is not applicable.
        /// </summary>
        public byte antChannel;

        /// <summary>
        /// The time the message was received.
        /// </summary>
        public DateTime timeReceived;

        /// <summary>
        /// The MessageID of the response
        /// </summary>
        public byte responseID;

        /// <summary>
        /// The raw contents of the response message
        /// </summary>
        public byte[] messageContents;

        internal ANT_Response(object sender, byte antChannel, DateTime timeReceived, byte IDcode, byte[] messageContents)
        {
            this.sender = sender;
            this.antChannel = antChannel;
            this.timeReceived = timeReceived;
            this.responseID = IDcode;
            this.messageContents = messageContents;
        }


        #region Accessors

        /// <summary>
        /// Returns messageContents[2] cast to an ANTEventID. Throws an exception if this is not a channel event.
        /// </summary>
        public ANT_ReferenceLibrary.ANTEventID getChannelEventCode()
        {
            if (responseID != (byte)ANT_ReferenceLibrary.ANTMessageID.RESPONSE_EVENT_0x40)
                throw new ANT_Exception("Response is not a channel event");
            return (ANT_ReferenceLibrary.ANTEventID)messageContents[2];
        }


        /// <summary>
        /// Returns messageContents[1] cast to an ANTMessageID. Throws an exception if this is not a response event.
        /// </summary>
        public ANT_ReferenceLibrary.ANTMessageID getMessageID()
        {
            if (responseID != (byte)ANT_ReferenceLibrary.ANTMessageID.RESPONSE_EVENT_0x40)
                throw new ANT_Exception("Response is not a response event");
            return (ANT_ReferenceLibrary.ANTMessageID)messageContents[1];
        }


        /// <summary>
        /// Returns the 8-byte data payload of an ANT message. Throws an exception if this is not a received message.
        /// </summary>
        /// <returns></returns>
        public byte[] getDataPayload()
        {
            if (messageContents.Length == 9
                && (responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.BROADCAST_DATA_0x4E
                     || responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.ACKNOWLEDGED_DATA_0x4F
                     || responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.BURST_DATA_0x50
               ))
                return messageContents.Skip(1).ToArray();   //Just skip the channel byte and return the payload
            else
                return splitExtMessage(extMsgParts.DataPayload);    // Extended message
        }



        /// <summary>
        /// Returns the burst sequence number (upper three bits of channel number). Throws exception if this is not a burst event.
        /// </summary>
        public byte getBurstSequenceNumber()
        {
            if (responseID != (byte)ANT_ReferenceLibrary.ANTMessageID.BURST_DATA_0x50
                && responseID != (byte)ANT_ReferenceLibrary.ANTMessageID.EXT_BURST_DATA_0x5F
               )
                throw new ANT_Exception("Response is not a burst event");
            else
                return (byte)((messageContents[0] & 0xE0) >> 5);
        }



        /// <summary>
        /// Returns the channel ID portion of an extended message. Throws an exception if this is not an extended message.
        /// </summary>
        public ANT_ChannelID getDeviceIDfromExt()
        {
            ANT_ChannelID extID = new ANT_ChannelID();
            byte[] rawID = splitExtMessage(extMsgParts.DeviceID);

            extID.deviceNumber = (UInt16)(rawID[0] + (rawID[1] << 8));
            extID.pairingBit = Convert.ToBoolean(rawID[2] & 0x80);
            extID.deviceTypeID = (byte)(rawID[2] & 0x7F);
            extID.transmissionTypeID = rawID[3];

            return extID;
        }

        /// <summary>
        /// Returns true if this is an extended message, false otherwise
        /// </summary>
        public bool isExtended()
        {
            if (messageContents.Length < 13
                )
                return false;
            else
                return true;
        }


        /// <summary>
        /// Splits and returns the requested part of an extended message. Throws an exception if this is not an extended message.
        /// </summary>
        /// <param name="whichPart">The desired part of the message</param>
        private byte[] splitExtMessage(extMsgParts whichPart)
        {
            if (!isExtended())
                throw new ANT_Exception("Response is not an extended message");

            byte[] dataPayload;
            byte[] deviceID;

            // Legacy extended message
            // The "extended" part of this message is the 4-byte channel ID of the device that we recieved this message from.
            // This event is only available on the AT3
            if (responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.EXT_BROADCAST_DATA_0x5D
                    || responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.EXT_ACKNOWLEDGED_DATA_0x5E
                    || responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.EXT_BURST_DATA_0x5F)
            {
                deviceID = messageContents.Skip(1).Take(4).ToArray(); //Skip channel byte
                dataPayload = messageContents.Skip(5).Take(8).ToArray();    //Skip channel byte and 4 bytes of id parameters
            }
            // "Flag" extended message. The 4-byte channel ID  is included if the flag byte includes 0x80
            else if (responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.BROADCAST_DATA_0x4E
                    || responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.ACKNOWLEDGED_DATA_0x4F
                    || responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.BURST_DATA_0x50)
            {
                dataPayload = messageContents.Skip(1).Take(8).ToArray();    //Skip channel byte
                if ((messageContents[9] & 0x80) == 0)   // Check flag byte
                    throw new ANT_Exception("Response does not contain a channel ID");
                deviceID = messageContents.Skip(10).Take(4).ToArray();   //Skip channel byte, 8 bytes of data, and flag byte
            }
            else
                throw new ANT_Exception("Response is not an extended message");

            switch (whichPart)
            {
                case extMsgParts.DataPayload:
                    return dataPayload;
                case extMsgParts.DeviceID:
                    return deviceID;
                default:
                    throw new ANT_Exception("Invalid extMsgPart");
            }
        }


        //This enum makes the code more readable when calling splitExtMessage()
        private enum extMsgParts
        {
            DataPayload,
            DeviceID
        }


        #endregion
    }
}
