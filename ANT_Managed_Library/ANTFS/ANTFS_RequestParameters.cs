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

namespace ANT_Managed_Library.ANTFS
{
    /// <summary>
    /// ANT-FS Request Parameters
    /// These are the parameters received from the host
    /// along with a download/upload/erase request
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct ANTFS_RequestParameters
    {
        /// <summary>
        /// File index of the requested download, upload or erase operation
        /// </summary>
        public ushort FileIndex;
        /// <summary>
        /// Requested offset.  The client library handles offsets internally,
        /// this is available for information purposes.
        /// </summary>
        public uint Offset;
        /// <summary>
        /// Maximum number of bytes the host can send/receive in a single block.
        /// The client library handles this internally, this is available for
        /// information purposes.
        /// </summary>
        public uint BlockSize;
        /// <summary>
        /// Maximum number of bytes that the host is requesting to upload.
        /// This includes the offset plus the total remaining bytes.
        /// This can be used to determine if there is enough space available
        /// in the device to handle the upload.
        /// </summary>
        public uint MaxSize;
        /// <summary>
        /// CRC Seed for the download.
        /// The client library handles this internally, this is available for
        /// information purposes
        /// </summary>
        public ushort CrcSeed;
        /// <summary>
        /// Indicates whether this is an initial download request or an attempt
        /// to resume a previous transfer.
        /// Resuming within a single session is handled by the library, this is
        /// available for information purposes.
        /// </summary>
        [MarshalAs(UnmanagedType.Bool)]
        public bool IsInitialRequest;
    }


    /// <summary>
    /// ANT-FS Disconnect Parameters
    /// These are the parameters received from the host
    /// along with the disconnect command
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct ANTFS_DisconnectParameters
    {
        /// <summary>
        /// Disconnect command type
        /// </summary>
        public byte CommandType;
        /// <summary>
        /// Requested amount in time (in 30 s increments) to become undiscoverable
        /// </summary>
        public byte TimeDuration;
        /// <summary>
        ///  Requested application specific undiscoverable time
        /// </summary>
        public byte ApplicationSpecificDuration;

    }
}
