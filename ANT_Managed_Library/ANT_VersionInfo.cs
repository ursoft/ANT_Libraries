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
using System.Reflection;
using System.Runtime.InteropServices;

namespace ANT_Managed_Library
{
    /// <summary>
    /// The information for this version of the ANT Managed Library
    /// </summary>
    public static class ANT_VersionInfo
    {
        private static string applicationCode = "AMO";

            private static string versionSuffix = "";

        /// <summary>
        /// This string shows the date the library received its current version number
        /// </summary>
        public static string versionNumberLastChangedOn = "15 Apr 2016";

        /// <summary>
        /// Returns the version information as a string
        /// </summary>
        /// <returns>Managed Library Version String</returns>
        public static string getManagedLibraryVersion()
        {
            return applicationCode + Assembly.GetExecutingAssembly().GetName().Version.ToString(4) + versionSuffix;
        }


        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern IntPtr getUnmanagedVersion();

        /// <summary>
        /// Gets the version string of the underlying unmanaged wrapper library, ANT_WrappedLib.dll
        /// </summary>
        /// <returns>Unmanaged Wrapper Version String</returns>
        public static string getUnmanagedLibraryVersion()
        {
            IntPtr pVerStr = getUnmanagedVersion();
            return Marshal.PtrToStringAnsi(pVerStr);
        }
    }
}
