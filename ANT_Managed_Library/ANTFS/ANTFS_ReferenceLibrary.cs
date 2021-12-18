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
using System.ComponentModel;
using System.Reflection;

#pragma warning disable 1591

namespace ANT_Managed_Library.ANTFS
{

    #region Constants

    /// <summary>
    /// Return codes for ANT-FS operations
    /// </summary>
    public enum ReturnCode : byte
    {
        [Description("Library in wrong state")]
        Fail = 0,
        [Description("Operation successful")]
        Pass,
        [Description("Library is busy with another request")]
        Busy
    };

    /// <summary>
    /// ANT-FS authentication types
    /// </summary>
    public enum AuthenticationType : byte
    {
        [Description("No authentication required")]
        None = 0,
        [Description("Not applicable (used for getting ID)")]
        NA,
        [Description("Pairing only authentication available")]
        Pairing,
        [Description("Passkey exchange and pairing supported")]
        PassKey
    }

    public enum AuthenticationCommand : byte
    {
        GotoTransport = 0x00,
        RequestSerialNumber = 0x01,
        Pair = 0x02,
        PassKey = 0x03
    }

    public enum RadioFrequency : byte
    {
        ANTFSNetwork = 50,
        ANTPlusNetwork = 57,
        Auto = 255
    }

    /// <summary>
    /// ANT-FS disconnect command type
    /// </summary>
    public enum DisconnectType : byte
    {
        [Description("Go back to Link state")]
        Link = 0,
        [Description("Return to broadcast")]
        Broadcast = 1
    }

    public enum BlackoutTime : ushort
    {
        None = 0,
        Infinite = 0xFFFF
    }

    [Flags]
    public enum Status1 : byte
    {
        DataAvailableBit = 0x20,
        UploadEnabledBit = 0x10,
        PairingEnabledBit = 0x08,
        BeaconPeriodBits = 0x07,
        DefaultSearchMask = 0x38
    }

    [Flags]
    public enum Status2 : byte
    {
        ClientStateBits = 0x0F,
        DefaultSearchMask = 0x00
    }

    public enum BeaconPeriod : byte
    {
        [Description("0.5 Hz (65535 counts)")]
        HalfHz = 0x00,
        [Description("1 Hz (32768 counts)")]
        OneHz = 0x01,
        [Description("2 Hz (16384 counts)")]
        TwoHz = 0x02,
        [Description("4 Hz (8192 counts)")]
        FourHz = 0x03,
        [Description("8 Hz (4096 counts)")]
        EightHz = 0x04,
        [Description("Keep assigned channel period")]
        Keep = 0x07
    }

    public enum ClientState : byte
    {
        Link = 0x00,
        Authenticate = 0x01,
        Transport = 0x02,
        Busy = 0x03
    }

    public enum DownloadResponse : byte
    {
        Ok = 0x00,
        InvalidIndex = 0x01,
        FileNotReadable = 0x02,
        NotReady = 0x03,
        InvalidRequest = 0x04,
        BadCrc = 0x05
    }

    public enum EraseResponse : byte
    {
        Ok = 0x00,
        Reject = 0x01
    }

    public enum UploadResponse : byte
    {
        Ok = 0x00,
        InvalidIndex = 0x01,
        FileNotWriteable = 0x02,
        InsufficientSpace = 0x03,
        InvalidRequest = 0x04,
        NotReady = 0x05
    }


    #endregion

    #region Helper Methods

    /// <summary>
    /// Helper class that prints human readable versions of the constants
    /// </summary>
    public static class Print
    {
        /// <summary>
        /// Prints the description attribute of an enumeration value
        /// </summary>
        /// <param name="eMyEnum">Enumeration value to print</param>
        /// <returns>Description string, e.g. "Operation successful"</returns>
        public static string AsString(Enum eMyEnum)
        {
            FieldInfo myField = eMyEnum.GetType().GetField(eMyEnum.ToString());
            if (myField != null)
            {
                DescriptionAttribute[] myAttributes = (DescriptionAttribute[]) myField.GetCustomAttributes(typeof(DescriptionAttribute), false);
                if((myAttributes != null) && (myAttributes.Length > 0))
                {
                    return myAttributes[0].Description; // If available, print the Description attribute
                }
            }

            return Print.AsEnum(eMyEnum);   // Otherwise, just print the enum value
        }

        /// <summary>
        /// Prints the name of an enumeration value
        /// </summary>
        /// <param name="eMyEnum">Enumeration value to print</param>
        /// <returns>Named enumeration string, e.g. "ANTFS.ReturnCode.Pass"</returns>
        public static string AsEnum(Enum eMyEnum)
        {
            return "ANTFS." + eMyEnum.GetType().Name + "." + eMyEnum.ToString(); // String representation of the enum, or numeric value, if not defined in enum
        }
    }

    #endregion


}
