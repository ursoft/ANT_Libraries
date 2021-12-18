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
    /// An exception occuring in the ANT Managed Library
    /// </summary>
    [Serializable]
    public class ANT_Exception: Exception
    {
        /// <summary>
        /// Prefixes given string with "ANTLibrary Exception: "
        /// </summary>
        /// <param name="exceptionDetail">String to prefix</param>
        public ANT_Exception(String exceptionDetail) : base("ANTLibrary Exception: " + exceptionDetail) { }

        /// <summary>
        /// Prefixes given string with "ANTLibrary Exception: " and propates inner exception
        /// </summary>
        /// <param name="exceptionDetail">String to prefix</param>
        /// <param name="innerException">Inner exception</param>
        public ANT_Exception(String exceptionDetail, Exception innerException) : base("ANTLibrary Exception: " + exceptionDetail, innerException) { }

        /// <summary>
        /// Copy constructor
        /// </summary>
        /// <param name="aex">ANTException to copy</param>
        public ANT_Exception(ANT_Exception aex) : base(aex.Message) { }   //C++ exceptions like to have a copy constructor
    }
}
