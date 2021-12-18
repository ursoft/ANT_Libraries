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
    /// Application defined parameters for a download response
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct ANTFS_DownloadParameters
    {
        /// <summary>
        /// File index of requested download
        /// </summary>
        public ushort FileIndex;
        /// <summary>
        /// Maximum number of bytes to send in a single block
        /// </summary>
        public uint MaxBlockSize;
    }

    /// <summary>
    /// Application defined parameters for an upload response
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct ANTFS_UploadParameters
    {
        /// <summary>
        /// File index of requested upload
        /// </summary>
        public ushort FileIndex;
        /// <summary>
        /// Maximum file size that can be stored in requested upload location
        /// </summary>
        public uint MaxFileSize;
        /// <summary>
        /// Maximum number of bytes that can be received in a single block
        /// </summary>
        public uint MaxBlockSize;
    }
}
