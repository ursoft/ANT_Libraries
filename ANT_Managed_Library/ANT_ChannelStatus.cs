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
    /// Contains the information returned by a channel status request message
    /// </summary>
    public struct ANT_ChannelStatus
    {
        /// <summary>
        /// Bits 0-1 of the status response
        /// </summary>
        public ANT_ReferenceLibrary.BasicChannelStatusCode BasicStatus;
        /// <summary>
        /// Bits 2-3 of the status response. Invalid on AP1.
        /// </summary>
        public byte networkNumber;
        /// <summary>
        /// Bits 4-7 of the status response. Not a valid channelType on AP1.
        /// </summary>
        public ANT_ReferenceLibrary.ChannelType ChannelType;

        /// <summary>
        /// Creates and fills the ChannelStatus
        /// </summary>
        /// <param name="BasicStatus"></param>
        /// <param name="networkNumber"></param>
        /// <param name="ChannelType"></param>
        public ANT_ChannelStatus(ANT_ReferenceLibrary.BasicChannelStatusCode BasicStatus, byte networkNumber, ANT_ReferenceLibrary.ChannelType ChannelType)
        {
            this.BasicStatus = BasicStatus;
            this.networkNumber = networkNumber;
            this.ChannelType = ChannelType;
        }
    }
}
