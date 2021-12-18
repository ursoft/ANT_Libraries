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

namespace ANT_Managed_Library
{
    /// <summary>
    /// This is a static class that manages all the functions and variables common to the whole scope of the library.
    /// </summary>
    public static class ANT_Common
    {

        /// <summary>
        /// Enables or disables all devices from resetting on startup, shutdown, and on CWTestMode Failure.
        /// Default = true.
        /// </summary>
        static public bool autoResetIsEnabled = true;

        #region constants

    #if EXT_FUNCTIONALITY
#warning "The Extended build is for internal use only, and even then, only for specific projects, do not use without discussing with the ANT team"
        internal const String ANT_UNMANAGED_WRAPPER = "ANT_WrappedLib_Ext.dll";
    #else
        internal const String ANT_UNMANAGED_WRAPPER = "ANT_WrappedLib.dll";
    #endif

        internal const String ANT_SI_LIBRARY = "DSI_SiUSBXp_3_1.DLL";
        internal const String ANT_SI_LIBRARY2 = "DSI_CP210xManufacturing_3_1.dll";

        #endregion


        #region DLL imports

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern UInt32 ANT_GetNumDevices();

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_EnableDebugLogging();

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern void ANT_DisableDebugLogging();

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANT_SetDebugLogDirectory(string pcDirectory);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_DebugThreadInit(string pucName);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        private static extern int ANT_DebugThreadWrite(string pcMessage);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention = CallingConvention.Cdecl)]
        internal static extern int ANT_DebugResetTime();

        #endregion


        /// <summary>
        /// Returns the number of ANT USB devices currently detected by the system.
        /// </summary>
        public static UInt32 getNumDetectedUSBDevices()
        {
            return ANT_GetNumDevices();
        }

        /// <summary>
        /// Checks if the unmanaged library is present in the application's working directory.
        /// Throws an exception if the library is missing.
        /// </summary>
        public static void checkUnmanagedLibrary()
        {
            // Check the unmanaged wrapper is present
            if (!System.IO.File.Exists(System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, ANT_UNMANAGED_WRAPPER)))
                throw new ANT_Exception(ANT_UNMANAGED_WRAPPER + " not found in working directory");

            //We could also check the version of the unmanaged wrapper if there were compatibility issues we wanted to enforce

        }

        /// <summary>
        /// Checks if device specific libraries are present in the application's working directory.
        /// Throws an exception if any of these is missing.
        /// </summary>
        public static void checkUSBLibraries()
        {
            String missingLibs = null;

            // Check for SiLabs libraries
            if (!System.IO.File.Exists(System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, ANT_SI_LIBRARY)))
            {
                missingLibs = ANT_SI_LIBRARY;
            }
            if (!System.IO.File.Exists(System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, ANT_SI_LIBRARY2)))
            {
                if (missingLibs != null)
                {
                    missingLibs += ", " + ANT_SI_LIBRARY2;
                }
                else
                {
                    missingLibs = ANT_SI_LIBRARY2;
                }
            }

            if (missingLibs != null)
            {
                throw new ANT_Exception(missingLibs + " not found in working directory");
            }
        }

        /// <overloads>Enables debug files</overloads>
        /// <summary>
        /// Initializes and enables debug logs for all devices
        /// Note:  For application specific logs to work properly
        /// (e.g. ANT-FS logs), this needs to be called BEFORE
        /// creating an ANT Device.
        /// </summary>
        public static bool enableDebugLogs()
        {
            return ANT_EnableDebugLogging() == 1;
        }

        /// <summary>
        /// Initializes and enables debug logs for all devices,
        /// and stores the log in the specified path.
        /// Note:  For application specific logs to work properly
        /// (e.g. ANT-FS logs), this needs to be called BEFORE
        /// creating an ANT Device.
        /// </summary>
        /// <param name="debugPath">Debug log directory</param>
        public static void enableDebugLogs(string debugPath)
        {
            enableDebugLogs();
            setDebugLogDirectory(debugPath);
        }

        /// <summary>
        /// Disables and closes the debug logs
        /// </summary>
        public static void disableDebugLogs()
        {
            ANT_DisableDebugLogging();
        }

        /// <summary>
        /// Set the directory the log files are saved to.
        /// This string will prefix the file name so must end with a slash or will be part of the name.
        /// ie: directoryPath='c:\ANT\logs' will result in files being saved to the \ANT directory named logsdevice0.txt.
        /// Throws an exception if directory does not exist.
        /// </summary>
        /// <param name="directoryPath">
        /// Path to directory to save log files in. Default is the running directory.
        /// This string will prefix the file name so must end with a slash or will be part of the name.
        /// ie: directoryPath='c:\ANT\logs' will result in files being saved to the \ANT directory named logsdevice0.txt.
        /// </param>
        public static bool setDebugLogDirectory(string directoryPath)
        {
            if (!System.IO.Directory.Exists(directoryPath))
                throw new ANT_Exception("Path does not exist");

            return ANT_SetDebugLogDirectory(directoryPath) == 1;
        }

        //
        //Note: Access to the following logging functions is required by the current ANT-FS implementation
        //
        /// <summary>
        /// Creates a debug log for the currently executing thread
        /// </summary>
        /// <param name="name">Name of file (will result in ao_debug_name)</param>
        /// <returns>True if successful</returns>
        internal static bool initDebugLogThread(string name)
        {
            return ANT_DebugThreadInit(name) == 1;
        }

        /// <summary>
        /// Adds an application specific message to the log for the current thread
        /// </summary>
        /// <param name="message">Message to write to the log</param>
        /// <returns>True on success</returns>
        internal static bool writeToDebugLog(string message)
        {
            return ANT_DebugThreadWrite(message) == 1;
        }
    }
}
