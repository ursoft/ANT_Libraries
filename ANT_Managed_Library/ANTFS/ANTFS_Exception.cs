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
    /// Exceptions thrown by ANT-FS objects
    /// </summary>
    public class ANTFS_Exception : ANT_Exception
    {
        /// <summary>
        ///  Constructor
        /// </summary>
        /// <param name="exceptionDetail">String to append to exception message</param>
        internal ANTFS_Exception(String exceptionDetail) : base("ANTFS - " + exceptionDetail) { }

        /// <summary>
        /// Prefixes given string with "ANTLibrary Exception: " and propagates inner exception
        /// </summary>
        /// <param name="exceptionDetail">String to prefix</param>
        /// <param name="innerException">Inner exception</param>
        internal ANTFS_Exception(String exceptionDetail, Exception innerException) : base("ANTFS - " + exceptionDetail, innerException) { }
    }


    /// <summary>
    /// Exceptions thrown by ANT-FS objects when a request to perform a specific operation failed
    /// Developers must ensure that these exceptions are handled appropiately to continue with the program execution
    /// </summary>
    public class ANTFS_RequestFailed_Exception : ANTFS_Exception
    {
        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="strOperation">Requested operation that failed</param>
        /// <param name="theReturn">ANT-FS Library return code</param>
        internal ANTFS_RequestFailed_Exception(string strOperation, ReturnCode theReturn) : base(strOperation + " Request Failed: " + Print.AsString(theReturn)) { }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="exceptionDetail">Requested operation that failed</param>
        internal ANTFS_RequestFailed_Exception(string exceptionDetail) : base(exceptionDetail) { }
    }
}
