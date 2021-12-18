/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/
using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;

namespace ANT_Managed_Library.ANTFS
{
    /// <summary>
    /// ANT-FS Configuration Parameters
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Pack=1)]
    public struct ANTFS_ConfigParameters
    {
        /// <summary>
        /// Authentication Timeout
        /// </summary>
        public uint CfgAuthTimeout;
        /// <summary>
        /// Erase Timeout
        /// </summary>
        public uint CfgEraseTimeout;
        /// <summary>
        /// Upload Request Timeout
        /// </summary>
        public uint CfgUploadRequestTimeout;
        /// <summary>
        /// Upload Response Timeout
        /// </summary>
        public uint CfgUploadResponseTimeout;
        /// <summary>
        /// Burst Check Timeout
        /// </summary>
        public uint CfgBurstCheckTimeout;
    }
}
