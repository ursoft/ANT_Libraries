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
    /// ANT-FS Client Parameters.
    /// The application should initialize ALL fields of the configuration struct,
    /// otherwise, they will all be zero
    /// </summary>
    [StructLayout(LayoutKind.Sequential)]
    public struct ANTFS_ClientParameters
    {
        /// <summary>
        /// Device serial number.  Set to 0 to use the serial number of the USB device
        /// </summary>
        public uint SerialNumber;
        /// <summary>
        /// Client device type (transmitted in beacon)
        /// </summary>
        public ushort BeaconDeviceType;
        /// <summary>
        /// Client manufacturing ID (transmitted in beacon)
        /// </summary>
        public ushort BeaconManufacturerID;
        /// <summary>
        /// Radio frequency to use while in Link state.
        /// </summary>
        public byte BeaconRadioFrequency;
        /// <summary>
        /// Beacon Period, as described in the ANT-FS Technology specification
        /// </summary>
        [MarshalAs(UnmanagedType.U1)]
        public BeaconPeriod LinkPeriod;
        /// <summary>
        /// Indicates whether pairing authentication is enabled
        /// </summary>
        [MarshalAs(UnmanagedType.Bool)]
        public bool IsPairingEnabled;
        /// <summary>
        /// Indicates whether upload functionality is supported
        /// </summary>
        [MarshalAs(UnmanagedType.Bool)]
        public bool IsUploadEnabled;
        /// <summary>
        /// Indicates whether there is data available for download
        /// </summary>
        [MarshalAs(UnmanagedType.Bool)]
        public bool IsDataAvailable;
        /// <summary>
        /// Authentication type to include in beacon, as described in the ANT-FS Technology specification
        /// </summary>
        [MarshalAs(UnmanagedType.U1)]
        public AuthenticationType AuthenticationType;
        /// <summary>
        /// Time, in seconds, that the client will wait without receiving any commands from the host before dropping to Link state.
        /// Set to 255 to disable.  Zero is NOT a valid value.
        /// </summary>
        public byte BeaconTimeout;
        /// <summary>
        /// Time, in seconds, that the client library will wait for a response from the application to a pairing request.
        /// </summary>
        public byte PairingTimeout;

        /// <summary>
        /// ANTFS_ClientParameters, with the default beacon parameters
        /// </summary>
        /// <param name="deviceType">Client device type</param>
        /// <param name="manufacturerID">Client manufacturing ID</param>
        public ANTFS_ClientParameters(ushort deviceType, ushort manufacturerID)
        {
            // Default beacon configuration: Match unmanaged ANTFSClient::SetDefaultBeacon
            BeaconDeviceType = deviceType;
            BeaconManufacturerID = manufacturerID;
            SerialNumber = 0;   // Use the USB device serial number by default
            BeaconRadioFrequency = 50;  // ANT-FS RF Frequency
            LinkPeriod = BeaconPeriod.EightHz;
            IsPairingEnabled = true;
            IsUploadEnabled = false;
            IsDataAvailable = false;
            AuthenticationType = AuthenticationType.Pairing;
            BeaconTimeout = 60; // In seconds
            PairingTimeout = 5; // In seconds

        }
    }
}
