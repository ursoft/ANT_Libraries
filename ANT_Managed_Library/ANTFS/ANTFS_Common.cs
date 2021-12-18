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

namespace ANT_Managed_Library.ANTFS
{
    /// <summary>
    /// Internal structure
    /// </summary>
    internal class ResponseTimeout<ResponseType>
    {
        /// <summary>
        /// Initial time
        /// </summary>
        internal DateTime timeStart = DateTime.MaxValue;
        /// <summary>
        /// Desired timeout
        /// </summary>
        internal uint timeLeft = UInt32.MaxValue;     // in seconds
        /// <summary>
        /// ID of response we are waiting for
        /// </summary>
        internal ResponseType ResponseID;
        /// <summary>
        /// Flag to indicate whether we are waiting for a response or not
        /// </summary>
        internal bool bWaitingForResponse = false;

        /// <summary>
        /// Configure a timeout to wait for a response
        /// </summary>
        /// <param name="theResponse">Response we are looking for</param>
        /// <param name="theTimeout">Timeout, in miliseconds</param>
        internal void SetTimeout(ResponseType theResponse, uint theTimeout)
        {
            timeStart = DateTime.Now;
            timeLeft = theTimeout;
            ResponseID = theResponse;
            bWaitingForResponse = true;
        }

        /// <summary>
        /// Clear timeout
        /// </summary>
        internal void ClearTimeout()
        {
            timeStart = DateTime.MaxValue;
            timeLeft = UInt32.MaxValue;
            bWaitingForResponse = false;
        }

        /// <summary>
        /// Check if the timeout has expired.
        /// Timeout is not triggered again, until enabled explicitly
        /// </summary>
        /// <returns>True if the timeout has expired, false otherwise</returns>
        internal bool HasTimeoutExpired()
        {
            if (!bWaitingForResponse || (timeStart == DateTime.MaxValue))
            {
                return false;   // We were not waiting for a response
            }

            if (DateTime.Compare(DateTime.Now, timeStart.AddMilliseconds((double)timeLeft)) > 0)
            {
                ClearTimeout();
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    internal class Common
    {
        internal static byte[] ConvertToByteArray(string myString)
        {
            // Convert as UTF-8
            byte[] myArray = System.Text.Encoding.UTF8.GetBytes(myString);
            // Append null character
            Array.Resize(ref myArray, myArray.Length + 1);
            myArray[myArray.Length - 1] = 0;

            return myArray;
        }

        internal static string ConvertToString(byte[] myArray)
        {
            // Convert as UTF-8
            string myString = System.Text.Encoding.UTF8.GetString(myArray);
            // Remove trailing null characters
            myString = myString.Remove(myString.IndexOf('\0'));

            return myString;
        }
    }

}
