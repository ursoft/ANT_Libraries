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
    /// ANT-FS Device Parameters
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Pack=1)]
    public struct ANTFS_DeviceParameters
    {
        /// <summary>
        /// Remote device ID
        /// </summary>
        public uint DeviceID;
        /// <summary>
        /// Manufacturer ID of remote device
        /// </summary>
        public ushort ManufacturerID;
        /// <summary>
        /// Remote device type
        /// </summary>
        public ushort DeviceType;
        /// <summary>
        /// Authentication type supported by remote device
        /// </summary>
        public byte AuthenticationType;
        /// <summary>
        /// Status byte 1, as described in the ANT-FS Technology specification
        /// </summary>
        public byte StatusByte1;
        /// <summary>
        /// Status byte 2, as described in the ANT-FS Technology specification
        /// </summary>
        public byte StatusByte2;

        /// <summary>
        /// Configure specified device parameters
        /// </summary>
        /// <param name="DevID">Device ID</param>
        /// <param name="usMfgID">Manufacturer ID</param>
        /// <param name="usDevType">Device Type</param>
        /// <param name="AuthType">Authentication Type</param>
        /// <param name="Stat1">Status byte 1, as described in the ANT-FS Technology specification</param>
        /// <param name="Stat2">Status byte 2, as described in the ANT-FS Technology specification</param>
        public ANTFS_DeviceParameters(uint DevID, ushort usMfgID, ushort usDevType, byte AuthType, byte Stat1, byte Stat2)
        {
            DeviceID = DevID;
            ManufacturerID = usMfgID;
            DeviceType = usDevType;
            AuthenticationType = AuthType;
            StatusByte1 = Stat1;
            StatusByte2 = Stat2;
        }

        /// <summary>
        /// Checks if the remote device is capable of the pairing authentication scheme
        /// </summary>
        /// <returns>True if pairing is supported, false otherwise</returns>
        public bool IsPairingEnabled()
        {
            if ((StatusByte1 & (byte)Status1.PairingEnabledBit) != 0)
                return true;
            else
                return false;
        }

        /// <summary>
        /// Checks if the remote device supports uploads
        /// </summary>
        /// <returns>True if uploads are supported, false otherwise</returns>
        public bool IsUploadEnabled()
        {
            if ((StatusByte1 & (byte)Status1.UploadEnabledBit) != 0)
                return true;
            else
                return false;
        }

        /// <summary>
        /// Checks if the remote device has data available for download
        /// </summary>
        /// <returns>True if data is available, false otherwise</returns>
        public bool IsDataAvailable()
        {
            if ((StatusByte1 & (byte)Status1.DataAvailableBit) != 0)
                return true;
            else
                return false;
        }

        /// <summary>
        /// Obtains the beacon period of the remote device
        /// </summary>
        /// <returns>Beacon period of the remote device</returns>
        public BeaconPeriod GetBeaconPeriod()
        {
            return (BeaconPeriod)(StatusByte1 & (byte)Status1.BeaconPeriodBits);
        }

        /// <summary>
        /// Obtains current state of the remote device
        /// </summary>
        /// <returns>State of the remote device</returns>
        public ClientState GetClientState()
        {
            return (ClientState) (StatusByte2 & (byte)Status2.ClientStateBits);
        }

        /// <summary>
        /// Enables/disables the pairing bit in the device parameters
        /// </summary>
        /// <param name="bEnable">Set to true to enable pairing, false otherwise</param>
        public void SetPairingBit(bool bEnable)
        {
            if (bEnable)
                StatusByte1 |= (byte)Status1.PairingEnabledBit;
            else
                StatusByte1 &= ((byte)Status1.PairingEnabledBit ^ 0xFF);    // ~Status1.PairingEnabledBit => ~ Operator converts the value to an Int32
        }

        /// <summary>
        /// Enables/disables the upload bit in the device parameters
        /// </summary>
        /// <param name="bEnable">Set to true to enable uploads, false otherwise</param>
        public void SetUploadBit(bool bEnable)
        {
            if (bEnable)
                StatusByte1 |= (byte)Status1.UploadEnabledBit;
            else
                StatusByte1 &= ((byte)Status1.UploadEnabledBit ^ 0xFF);     // ~Status1.UploadEnabledBit => ~ Operator converts the value to an Int32
        }

        /// <summary>
        /// Enables/disables the data available bit in the device parameters
        /// </summary>
        /// <param name="bEnable">Set to true if there is data available for download, false otherwise</param>
        public void SetDataAvailableBit(bool bEnable)
        {
            if (bEnable)
                StatusByte1 |= (byte)Status1.DataAvailableBit;
            else
                StatusByte1 &= ((byte)Status1.DataAvailableBit ^ 0xFF);     // ~Status1.DataAvailableBit => ~ Operator converts the value to an Int32
        }

        /// <summary>
        /// Returns a string with the decoded device parameters
        /// </summary>
        /// <returns>String with decoded device parameters</returns>
        public override string ToString()
        {
            String strResults = "";
            strResults += "   Device ID: " + DeviceID + Environment.NewLine;
            strResults += "   Manufacturer ID: " + ManufacturerID + Environment.NewLine;
            strResults += "   Device Type: " + DeviceType + Environment.NewLine;
            strResults += "   Authentication Type: " + ((AuthenticationType)AuthenticationType).ToString() + Environment.NewLine;
            if (IsDataAvailable())
                strResults += "   Device has new data" + Environment.NewLine;
            else
                strResults += "   Device does not have new data" + Environment.NewLine;
            if (IsPairingEnabled())
                strResults += "   Device is in pairing mode" + Environment.NewLine;
            else
                strResults += "   Device is not in pairing mode" + Environment.NewLine;
            if (IsUploadEnabled())
                strResults += "   Device has upload enabled" + Environment.NewLine;
            else
                strResults += "   Device does not have upload enabled" + Environment.NewLine;
            return strResults;
        }

    }

    /// <summary>
    /// Parameters retrieved by the host after finding a client matching its search criteria
    /// </summary>
    public class ANTFS_SearchResults
    {
        /// <summary>
        /// Remote device parameters
        /// </summary>
        public ANTFS_DeviceParameters DeviceParameters;
        /// <summary>
        /// Friendly name of the remote device
        /// </summary>
        public String FriendlyName = "";

        /// <summary>
        /// Returns a string with the decoded device parameters and friendly name
        /// </summary>
        /// <returns>String with decoded device parameters and friendly name</returns>
        public override string ToString()
        {
            String strResults = "";
            strResults += "Found remote device: " + FriendlyName + Environment.NewLine;
            strResults += DeviceParameters.ToString();

            return strResults;
        }

    }

}
