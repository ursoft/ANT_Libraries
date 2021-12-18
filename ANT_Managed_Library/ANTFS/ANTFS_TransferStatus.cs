/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/

//////////////////////////////////////////////////////////////////////////
// This file contains all the enumerations and constants for general
// use with ANT-FS
//////////////////////////////////////////////////////////////////////////

using System;
using System.Collections.Generic;
using System.Text;


namespace ANT_Managed_Library.ANTFS
{
    /// <summary>
    /// Status of an ongoing or completed data transfer
    /// </summary>
    public class TransferStatus
    {
        private uint myByteProgress;
        private uint myTotalLength;
        private byte myPercentage;

        /// <summary>
        /// Constructor initializes status and obtains percentage from parameters
        /// </summary>
        /// <param name="Progress">Current byte progress</param>
        /// <param name="Length">Expected data length</param>
        public TransferStatus(uint Progress, uint Length)
        {
            this.myByteProgress = Progress;
            this.myTotalLength = Length;
            if (Length != 0)
                this.myPercentage = (byte)Math.Round(((decimal)Progress / (decimal)Length) * 100);
            else
                this.myPercentage = 0;
        }

        /// <summary>
        /// Current byte progress of a data transfer
        /// </summary>
        public uint ByteProgress
        {
            get { return myByteProgress; }
        }

        /// <summary>
        /// Expected length of a data transfer
        /// </summary>
        public uint TotalLength
        {
            get { return myTotalLength; }
        }

        /// <summary>
        /// Current percentage of completion of a data transfer
        /// </summary>
        public byte Percentage
        {
            get { return myPercentage; }
        }

        /// <summary>
        /// Provides a string containing the transfer status
        /// </summary>
        /// <returns>Formatted string with the current byte progress, expected length and percentage</returns>
        public override string ToString()
        {
            return "Transfer Status: (" + myByteProgress + "/" + myTotalLength + ") ... " + myPercentage + '%';
        }

    }

}
