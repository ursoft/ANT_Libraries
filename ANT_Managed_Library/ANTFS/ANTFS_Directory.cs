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
using System.Linq;
using System.ComponentModel;

#pragma warning disable 1591

namespace ANT_Managed_Library.ANTFS
{
    /// <summary>
    /// ANT-FS Directory
    /// Contains information about the files available on a remote device
    /// </summary>
    public class ANTFS_Directory
    {
        #region Variables

        /// <summary>
        /// Directory Header
        /// </summary>
        public Header dirHeader;
        /// <summary>
        /// Number of entries contained in directory
        /// </summary>
        public uint dirSize;
        /// <summary>
        /// Indexed entries in the directory
        /// </summary>
        public Dictionary<ushort, Entry> dirFiles; // TODO: This should probably not be public, as we might decide later on to change the implementation

        #endregion

        #region Constants

        public static readonly byte DirectoryVersion= 1;
        public static readonly byte EntryLength = 16;

        public static readonly byte FitDataType = 0x80;

        /// <summary>
        /// Format of time used in directory
        /// </summary>
        public enum TimeFormat : byte
        {
            [Description("Device will use the time as described in the Date field; if the correct time is not known, it will use system time")]
            Auto = 0,
            [Description("Device will only use system time (seconds since power up)")]
            System = 1,
            [Description("Device will only use the Date parameter as counter")]
            Date = 2
        }

        /// <summary>
        /// Bit mapped flags of file permissions
        /// </summary>
        [Flags]
        public enum GeneralFlags : byte
        {
            [Description("File can be downloaded")]
            Read = 0x80,
            [Description("File can be uploaded")]
            Write = 0x40,
            [Description("File can be erased")]
            Erase = 0x20,
            [Description("File has been previously downloaded")]
            Archive = 0x10,
            [Description("Can append data to file")]
            Append = 0x08,
            [Description("File is encrypted")]
            Crypto = 0x04
        }

        [Flags]
        public enum FitSpecificFlags : byte
        {
            [Description("File is selected for download")]
            Selected = 0x01
        }

        #endregion

        #region Data Structures

        /// <summary>
        /// Directory header structure
        /// </summary>
        [StructLayout(LayoutKind.Sequential, Pack=1, Size=16)]
        public struct Header
        {
            /// <summary>
            /// The version of the Directory File Structure.  The most significant 4 bits
            /// indicate major revision while the least significant 4 bits indicate a minor
            /// revision
            /// </summary>
            public byte Version;
            /// <summary>
            /// The length of each structure, in bytes
            /// </summary>
            public byte ElementLength;
            /// <summary>
            /// Defines how the system will keep track of Date/Time stamps
            /// </summary>
            public byte TimeFormat;
            /// <summary>
            /// Reserved bytes
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 5)] public byte[] Reserved;
            /// <summary>
            /// Number of seconds elapsed since the system was powered up
            /// </summary>
            public uint SystemTime;
            /// <summary>
            /// The number of seconds elapsed since 00:00 in the morning of December 31, 1989.
            /// The value zero (0) specifies an unknown date.
            /// Values of less than 0x0FFFFFFF will be interpreted as being system time or
            /// some other custom time format (e.g. counter incremented every time directory
            /// entries are modified)
            /// </summary>
            public uint Date;
        }

        /// <summary>
        /// Directory file entry structure
        /// </summary>
        [StructLayout(LayoutKind.Sequential, Pack=1, Size=16)]
        public struct Entry
        {
            /// <summary>
            /// Data file index
            /// </summary>
            public ushort FileIndex;
            /// <summary>
            /// Data type of the file, which informs how to interpret the file
            /// </summary>
            public byte FileDataType;
            /// <summary>
            /// Part of the identifier field, used to uniquely identify a file.
            /// In .FIT, used to identify a sub type of the .FIT file type.
            /// </summary>
            public byte FileSubType;
            /// <summary>
            /// Part of the identifier field, used to uniquely identify a file.
            /// In .FIT, used to identify a particular instance of a file sub type.
            /// </summary>
            public ushort FileNumber;
            /// <summary>
            /// File data type specific bit mapped flags
            /// </summary>
            public byte SpecificFlags;
            /// <summary>
            /// Bit mapped flags of file permissions
            /// </summary>
            public byte GeneralFlags;
            /// <summary>
            /// Size of file in bytes
            /// </summary>
            public uint FileSize;
            /// <summary>
            /// The number of seconds elapsed since 00:00 in the morning of December 31, 1989.
            /// The value zero (0) specifies an unknown date.
            /// Values of less than 0x0FFFFFFF will be interpreted as being system time or
            /// some other custom time format.
            /// </summary>
            public uint TimeStamp;
        }

        #endregion

        #region DLLImports

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern uint ANTFSDirectory_GetNumberOfFileEntries(
            [MarshalAs(UnmanagedType.LPArray)] byte[] pvDirectory,
            uint ulDirectoryFileLength);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANTFSDirectory_LookupFileEntry(
            [MarshalAs(UnmanagedType.LPArray)] byte[] pvDirectory,
            uint ulDirectoryFileLength,
            uint ulFileEntry,
            ref Entry pstDirStruct);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANTFSDirectory_GetNewFileList(
            [MarshalAs(UnmanagedType.LPArray)] byte[] pvDirectory,
            uint ulDirectoryFileLength,
            [MarshalAs(UnmanagedType.LPArray)] ushort[] pusFileIndexList,
            ref ushort pusListLength);

        [DllImport(ANT_Common.ANT_UNMANAGED_WRAPPER, CallingConvention=CallingConvention.Cdecl)]
        private static extern int ANTFSDirectory_LookupFileIndex(
            [MarshalAs(UnmanagedType.LPArray)] byte[] pvDirectory,
            uint ulDirectoryFileLength,
            ushort usFileIndex,
            ref Entry pstDirStruct);

        #endregion

        #region Constructor

        /// <summary>
        /// Creates an empty ANT-FS Directory structure
        /// </summary>
        public ANTFS_Directory()
        {
            // Initialize header
            dirHeader.Version = DirectoryVersion;
            dirHeader.ElementLength = EntryLength;
            dirHeader.TimeFormat = (byte) TimeFormat.Auto;
            dirHeader.Reserved = new byte[] {0, 0, 0, 0, 0};
            dirHeader.SystemTime = 0;
            dirHeader.Date = 0;

            // Empty directory: initial size is 0
            dirSize = 0;
            dirFiles = new Dictionary<ushort,Entry>();
        }

        /// <summary>
        /// Creates an ANTFS_Directory object from downlaoded data
        /// </summary>
        /// <param name="DirectoryFile">Directory file received on a download</param>
        public ANTFS_Directory(byte[] DirectoryFile)
        {
            // Check endianness
            if (!BitConverter.IsLittleEndian)
            {
                Array.Reverse(DirectoryFile);
            }

            // Parse header
            this.dirHeader = GetHeader(DirectoryFile);

            // Get number of file entries
            this.dirSize = GetNumberOfFileEntries(DirectoryFile);

            // Populate list of files
            this.dirFiles = new Dictionary<ushort,Entry>();
            if (this.dirSize > 0)
            {
                for (uint i = 0; i < this.dirSize; i++)
                {
                    Entry? newEntry = LookupFileEntry(DirectoryFile, i);
                    if (newEntry != null)
                    {
                        dirFiles.Add(newEntry.Value.FileIndex, newEntry.Value);
                    }
                }
            }
        }



        #endregion

        #region Interface for ANT-FS library functions (unmanaged)
        // These functions are static, and operate on raw directory data

        /// <summary>
        /// Obtains the number of file entries contained in the directory
        /// </summary>
        /// <param name="pvDirectory">Directory file</param>
        /// <returns>Number of file entries contained in directory</returns>
        public static uint GetNumberOfFileEntries(byte[] pvDirectory)
        {
            return ANTFSDirectory_GetNumberOfFileEntries(pvDirectory, (uint) pvDirectory.Length);
        }

        /// <summary>
        /// Decodes the directory and gets a list of files that need to be downloaded
        /// </summary>
        /// <param name="pvDirectory">Directory file</param>
        /// <returns>Array containing the file indexes that need to be downloaded.
        /// Returns an empty array if there are no new files.</returns>
        public static ushort[] GetNewFileList(byte[] pvDirectory)
        {
            ushort usListLength = UInt16.MaxValue;
            ushort[] pusFileList = null;

            // Set array to null to retrieve the size first
            if (ANTFSDirectory_GetNewFileList(pvDirectory, (uint)pvDirectory.Length, pusFileList, ref usListLength) == 0)
                return new ushort[0];

            // Allocate array of correct size, and request list
            pusFileList = new ushort[usListLength];
            if (usListLength > 0)
            {
                if (ANTFSDirectory_GetNewFileList(pvDirectory, (uint)pvDirectory.Length, pusFileList, ref usListLength) == 0)
                    return new ushort[0];
            }
            return pusFileList;
        }

        /// <summary>
        /// Looks up the requested directory entry
        /// </summary>
        /// <param name="pvDirectory">Directory file</param>
        /// <param name="ulFileEntry">Zero-based entry number of the requested file (based on the order in which files are written in directory)</param>
        /// <returns>Requested directory entry, or null if entry is not valid</returns>
        public static Entry? LookupFileEntry(byte[] pvDirectory, uint ulFileEntry)
        {
            Entry myEntry = new Entry();

            if (ANTFSDirectory_LookupFileEntry(pvDirectory, (uint)pvDirectory.Length, ulFileEntry, ref myEntry) == 0)
                return null;

            return myEntry;
        }

        /// <summary>
        ///  Looks up the requested directory entry
        /// </summary>
        /// <param name="pvDirectory">Directory file</param>
        /// <param name="usFileIndex">Index of file to be looked up</param>
        /// <returns>Requested directory entry, or null if entry is not valid</returns>
        public static Entry? LookupFileIndex(byte[] pvDirectory, ushort usFileIndex)
        {
            Entry myEntry = new Entry();

            if (ANTFSDirectory_LookupFileIndex(pvDirectory, (uint)pvDirectory.Length, usFileIndex, ref myEntry) == 0)
                return null;

            return myEntry;
        }

        #endregion

        #region Managed Directory Functions

        /// <summary>
        /// Retrieves the directory file header
        /// </summary>
        /// <param name="pvDirectory">Directory file</param>
        /// <returns>Directory header structure.  An exception is thrown if the file is too small to contain a header</returns>
        public Header GetHeader(byte[] pvDirectory)
        {
            // Make sure data is at least the size of the header
            if (pvDirectory.Length < Marshal.SizeOf(typeof(Header)))
                throw new ANT_Exception("Error: Invalid directory file");

            // Fill header struct
            GCHandle pinnedDirectory = GCHandle.Alloc(pvDirectory, GCHandleType.Pinned);    // Pin directory while we parse header
            Header myHeader = (Header)Marshal.PtrToStructure(pinnedDirectory.AddrOfPinnedObject(), typeof(Header)); // Marshal byte array to C# structure
            pinnedDirectory.Free(); // Unpin directory
            return myHeader;
        }

        /// <summary>
        /// Print directory
        /// </summary>
        /// <returns>Formatted string with decoded directory</returns>
        public override string ToString()
        {
            string strPrint = "";

            // Header Info
            strPrint += "ANT-FS Directory Version: " + GetVersion() + Environment.NewLine;

            if (dirSize > 0)
            {
                strPrint += "Index".PadRight(7);
                strPrint += "Data Type".PadRight(11);
                strPrint += "Identifier".PadRight(12);
                strPrint += "Flags".PadRight(14);
                strPrint += "File Size".PadRight(13);
                strPrint += "Timestamp";
                strPrint += Environment.NewLine;
            }

            // Files
            var sortedDir = (from entry in dirFiles orderby entry.Key ascending select entry);  // Sort by index
            foreach(KeyValuePair<ushort, Entry> kvp in sortedDir)
            {
                strPrint += kvp.Key.ToString().PadRight(7);
                strPrint += kvp.Value.FileDataType.ToString().PadRight(11);
                if (kvp.Value.FileDataType == 128) // .FIT file
                {
                    strPrint += (kvp.Value.FileSubType + "-" + kvp.Value.FileNumber).PadRight(12);
                }
                else
                {
                    strPrint += (((uint)kvp.Value.FileSubType << 16) + (uint)kvp.Value.FileNumber).ToString().PadRight(12);
                }

                strPrint += ParseFlags(kvp.Value.GeneralFlags).PadRight(14);
                strPrint += kvp.Value.FileSize.ToString().PadRight(13);
                uint ulDate = kvp.Value.TimeStamp;
                if (ulDate == 0)
                {
                    strPrint += "Unknown";
                }
                else if (ulDate < 0x0FFFFFFF)
                {
                    strPrint += ulDate;  // System Time
                }
                else
                {
                    DateTime dateBase = new DateTime(1989, 12, 31, 0, 0, 0);    // December 31, 1989, 00:00 hrs
                    dateBase = dateBase.AddSeconds(ulDate);
                    strPrint += dateBase.ToString();
                }
                strPrint += Environment.NewLine;
            }

            strPrint += "   " + dirSize + " File(s)" + Environment.NewLine;

            return strPrint;
        }

        /// <summary>
        /// Obtains version of the directory
        /// </summary>
        /// <returns>Formatted string with ANT-FS directory version</returns>
        public string GetVersion()
        {
            byte ucMajorRev = (byte)((dirHeader.Version & (byte)0xF0) >> 4);
            byte ucMinorRev = (byte)(dirHeader.Version & (byte)0x0F);

            return (ucMajorRev + "." + ucMinorRev);
        }

        public void Clear()
        {
            dirFiles.Clear();
            dirSize = 0;
        }

        public void AddEntry(Entry newEntry)
        {
            if (newEntry.FileIndex == 0)
                throw new ArgumentException("File index 0 is reserved for the directory");

            if (dirFiles.ContainsKey(newEntry.FileIndex))
                throw new ArgumentException("Specified file index is already in use");

            dirFiles.Add(newEntry.FileIndex, newEntry);
            dirSize++;
        }

        public void AddOrReplaceEntry(Entry newEntry)
        {
            if (newEntry.FileIndex == 0)
                throw new ArgumentException("File index 0 is reserved for the directory");

            if (!dirFiles.ContainsKey(newEntry.FileIndex))
            {
                dirFiles.Add(newEntry.FileIndex, newEntry);
                dirSize++;
            }
            else
            {
                dirFiles[newEntry.FileIndex] = newEntry;
            }
        }

        public void DeleteEntry(ushort fileIndex)
        {
            if (fileIndex == 0)
                throw new UnauthorizedAccessException("Directory cannot be erased");

            if (!dirFiles.ContainsKey(fileIndex))
                throw new ArgumentException("Invalid Index: File does not exist");

            Entry tempEntry = dirFiles[fileIndex];

            if ((tempEntry.GeneralFlags & (byte)GeneralFlags.Erase) == 0)
                throw new UnauthorizedAccessException("Not enough permissions to erase the file");

            dirFiles.Remove(fileIndex);
            dirSize--;
        }

        public void ForceDeleteEntry(ushort fileIndex)
        {
            if (fileIndex == 0)
                throw new UnauthorizedAccessException("Directory cannot be erased");

            if (!dirFiles.ContainsKey(fileIndex))
                throw new ArgumentException("Invalid Index: File does not exist");

            dirFiles.Remove(fileIndex);
            dirSize--;
        }

        public Entry GetEntry(ushort fileIndex)
        {
            if (fileIndex == 0)
                throw new ArgumentException("There is no entry associated with the directory");

            if (!dirFiles.ContainsKey(fileIndex))
                throw new ArgumentException("Invalid Index: File does not exist");

            return dirFiles[fileIndex];
        }

        public List<ushort> GetAllIndexes()
        {
            return dirFiles.Keys.ToList();
        }

        public List<ushort> GetIndexes(Func<Entry, bool> filter)    // TODO: or array? or something else?
        {
            return dirFiles.Where(kvp => filter(kvp.Value)).Select(kvp => kvp.Key).ToList();
        }

        public byte[] ToByteArray()
        {
            // Size of the entire directory structure:
            int offset = 0;
            uint dirLength = EntryLength * (dirSize + 1);   // Entries + header
            byte[] rawDir = new byte[dirLength];

            // First convert the header
            IntPtr elementPtr = Marshal.AllocHGlobal(EntryLength);
            Marshal.StructureToPtr(dirHeader, elementPtr, true);
            Marshal.Copy(elementPtr, rawDir, offset, EntryLength);
            Marshal.FreeHGlobal(elementPtr);

            // Now walk through the entries
            foreach (Entry fileEntry in dirFiles.Values)
            {
                offset += EntryLength;
                elementPtr = Marshal.AllocHGlobal(EntryLength);
                Marshal.StructureToPtr(fileEntry, elementPtr, true);
                Marshal.Copy(elementPtr, rawDir, offset, EntryLength);
                Marshal.FreeHGlobal(elementPtr);
            }

            return rawDir;
        }

        public uint GetFileSize(ushort fileIndex)
        {
            if(fileIndex == 0)
                return EntryLength * (dirSize + 1);

            if (dirFiles.ContainsKey(fileIndex))
            {
                return dirFiles[fileIndex].FileSize;
            }
            else
                throw new ArgumentException("Invalid Index: File does not exist");
        }

        // Convenience methods for checking permissions
        public static bool IsFileReadable(Entry entry)
        {
            return (entry.GeneralFlags & (byte)GeneralFlags.Read) != 0;
        }

        public bool IsFileReadable(ushort fileIndex)
        {
            if (fileIndex == 0)
                return true;

            if (!dirFiles.ContainsKey(fileIndex))
                throw new ArgumentException("Invalid Index: File does not exist");

            return IsFileReadable(dirFiles[fileIndex]);
        }

        public static bool IsFileWriteable(Entry entry)
        {
            return (entry.GeneralFlags & (byte)ANTFS_Directory.GeneralFlags.Write) != 0;
        }

        public bool IsFileWriteable(ushort fileIndex)
        {
            if (fileIndex == 0)
                return false;

            if (!dirFiles.ContainsKey(fileIndex))
                throw new ArgumentException("Invalid Index: File does not exist");

            return IsFileWriteable(dirFiles[fileIndex]);
        }

        public static bool IsFileEraseable(Entry entry)
        {
            return (entry.GeneralFlags & (byte)GeneralFlags.Erase) != 0;
        }

        public bool IsFileEraseable(ushort fileIndex)
        {
            if (fileIndex == 0)
                return false;

            if (!dirFiles.ContainsKey(fileIndex))
                throw new ArgumentException("Invalid Index: File does not exist");

            return IsFileEraseable(dirFiles[fileIndex]);
        }

        public static bool IsFileArchived(Entry entry)
        {
            return (entry.GeneralFlags & (byte)GeneralFlags.Archive) != 0;
        }

        public bool IsFileArchived(ushort fileIndex)
        {
            if (fileIndex == 0)
                return false;

            if (!dirFiles.ContainsKey(fileIndex))
                throw new ArgumentException("Invalid Index: File does not exist");

            return IsFileArchived(dirFiles[fileIndex]);
        }

        public static bool IsFileEncrypted(Entry entry)
        {
            return (entry.GeneralFlags & (byte)GeneralFlags.Crypto) != 0;
        }

        public bool IsFileEncrypted(ushort fileIndex)
        {
            if (fileIndex == 0)
                return false;

            if (!dirFiles.ContainsKey(fileIndex))
                throw new ArgumentException("Invalid Index: File does not exist");

            return IsFileEncrypted(dirFiles[fileIndex]);
        }

        public static bool IsFileSelected(Entry entry)
        {
            return (entry.SpecificFlags & (byte)FitSpecificFlags.Selected) != 0;
        }

        public bool IsFileSelected(ushort fileIndex)
        {
            if (fileIndex == 0)
                return false;

            if (!dirFiles.ContainsKey(fileIndex))
                throw new ArgumentException("Invalid Index: File does not exist");

            return IsFileSelected(dirFiles[fileIndex]);
        }

        /// <summary>
        /// Parses general flags into a string
        /// </summary>
        /// <param name="ucFlags">Flag byte</param>
        /// <returns>Formatted string with decoded flags</returns>
        public static string ParseFlags(byte ucFlags)
        {
            bool bFirst = true;
            string strFlags = "";

            if ((ucFlags & (byte)GeneralFlags.Read) != 0)
            {
                if (!bFirst)
                    strFlags += "|";
                else
                    bFirst = false;
                strFlags += "Re";
            }

            if ((ucFlags & (byte)GeneralFlags.Write) != 0)
            {
                if (!bFirst)
                    strFlags += "|";
                else
                    bFirst = false;
                strFlags += "Wr";
            }

            if ((ucFlags & (byte)GeneralFlags.Erase) != 0)
            {
                if (!bFirst)
                    strFlags += "|";
                else
                    bFirst = false;
                strFlags += "Er";
            }

            if ((ucFlags & (byte)GeneralFlags.Archive) != 0)
            {
                if (!bFirst)
                    strFlags += "|";
                else
                    bFirst = false;
                strFlags += "Ar";
            }

            if ((ucFlags & (byte)GeneralFlags.Append) != 0)
            {
                if (!bFirst)
                    strFlags += "|";
                else
                    bFirst = false;
                strFlags += "Ap";
            }

            if ((ucFlags & (byte)GeneralFlags.Crypto) != 0)
            {
                if (!bFirst)
                    strFlags += "|";
                else
                    bFirst = false;
                strFlags += "Cr";
            }

            return strFlags;

        }
        #endregion

    }

}
