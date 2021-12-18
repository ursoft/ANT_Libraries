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
    /// Container for all the USB Device information, returned from an ANTDevice
    /// </summary>
    public class ANT_DeviceInfo
    {
        /// <summary>
        /// USB Device Product Description
        /// </summary>
        public byte[] productDescription;

        /// <summary>
        /// USB Device Serial String
        /// </summary>
        public byte[] serialString;

        internal ANT_DeviceInfo(byte[] productDescription, byte[] serialString)
        {
            this.productDescription = productDescription;
            this.serialString = serialString;
        }

        /// <summary>
        /// Returns a formatted, readable string for the product description
        /// </summary>
        public String printProductDescription()
        {
           return printBytes(productDescription);
        }

        /// <summary>
        /// Returns a formatted, readable string for the serial string
        /// </summary>
        public String printSerialString()
        {
            return printBytes(serialString);
        }

        private String printBytes(byte[] rawBytes)
        {
            // Decode as null terminated ASCII string
            string formattedString = System.Text.Encoding.ASCII.GetString(rawBytes);
            return (formattedString.Remove(formattedString.IndexOf('\0')));
        }

    };

}
