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

namespace ANT_Managed_Library.ANTFS
{
    /// <summary>
    /// Control class for a given ANT device, with Integrated ANT-FS Client functionality.
    /// An instance of this class is an open connection to the given ANT USB device.
    /// Handles creating channels and device setup, and allows managing the file system (EEPROM)
    /// as well as configuring and controlling ANT-FS sessions.
    /// </summary>
    public class ANT_IntegratedANTFSClient : ANT_Device
    {
        /// <summary>
        /// Queue with the responses received from ANT related to Integrated ANT-FS
        /// </summary>
        public Queue<ANT_Response> responseBuf = new Queue<ANT_Response>();

        /// <summary>
        /// Last received ANT Response
        /// </summary>
        public ANT_Response lastAcceptedResponse;

        /// <summary>
        /// Device events
        /// </summary>
        public byte[] hostdeviceEvents;

        /// <summary>
        /// Flag indicating availabity of device events
        /// </summary>
        public bool flagHostDeviceEvents;

        /// <summary>
        /// Default timeout for responses
        /// </summary>
        const int defaultWait = 3000;

        #region Setup

        /// <summary>
        /// Creates an Integrated ANT-FS Client object, by automatically trying to connect to the USB stick
        /// </summary>
        public ANT_IntegratedANTFSClient() : base()  // 57600 is tried first
        {
            ANT_Common.enableDebugLogs();
            if (!this.getDeviceCapabilities().FS)
            {
                this.Dispose();
                throw new ANT_Exception("Found ANT USB device, but does not support FS"); //TODO This is not a very effective auto init, for example: What happens if FS device is second device and first device isn't? Should we connect to other devices to check?
            }
            deviceResponse += dev_deviceResponse;
            deviceResponse += dev_hostdeviceEvents;
        }

        /// <summary>
        /// Creates an Integrated ANT-FS Client object, specifying the USB device number.  Baud rate is assumed to be 57600bps.
        /// </summary>
        /// <param name="USBDeviceNum">USB device number</param>
        public ANT_IntegratedANTFSClient(byte USBDeviceNum)  : base (USBDeviceNum, 57600) // For FS-enabled parts, Baud rate is 57600
        {
            ANT_Common.enableDebugLogs();
            responseBuf = new Queue<ANT_Response>();
            if (!this.getDeviceCapabilities().FS)
            {
                this.Dispose();
                throw new ANT_Exception("Found ANT USB device, but does not support FS");
            }

        }

        /// <summary>
        /// Close connection to the ANT USB device
        /// </summary>
        public void shutdown()
        {
            this.Dispose();

        }


        //TODO: This buffer gets really big if we sit around for very long
        void dev_deviceResponse(ANT_Response response)
        {
            responseBuf.Enqueue(response);
        }

        void dev_hostdeviceEvents(ANT_Response response)
        {
            hostdeviceEvents = response.messageContents;
            flagHostDeviceEvents = true;
        }

        #endregion

        #region ANT Comm Helper Functions

        internal static byte getByte_usLE(int byteNum, ushort value)
        {
            if (byteNum == 1) //Return low byte
                return (byte)(value & 0x00FF);
            else if (byteNum == 2)  //Return high byte
                return (byte)((value & 0xFF00) >> 8);
            else
                throw new Exception("Bad byte number");
        }

        internal static byte getByte_ulLE(int byteNum, ulong value)
        {
            if (byteNum == 1) //Return low byte
                return (byte)(value & 0x000000FF);
            else if (byteNum == 2)  //Return mid-low byte
                return (byte)((value & 0x0000FF00) >> 8);
            else if (byteNum == 3)  //Return mid-high byte
                return (byte)((value & 0x00FF0000) >> 16);
            else if (byteNum == 4)  //Return high byte
                return (byte)((value & 0xFF000000) >> 24);
            else
                throw new Exception("Bad byte number");
        }

        private byte[] waitForMsg(byte[] leadingBytes)
        {
            return waitForMsg(leadingBytes, defaultWait);
        }

        private byte[] waitForMsg(byte[] leadingBytes, int wait_ms)
        {
            Queue<ANT_Response> msgSave = new Queue<ANT_Response>();
            int i = 0;
            ANT_Response curResponse;

            System.Diagnostics.Stopwatch waitTimer = new System.Diagnostics.Stopwatch();
            waitTimer.Start();
            do
            {
                while (responseBuf.Count > 0)
                {
                    curResponse = responseBuf.Dequeue();
                    msgSave.Enqueue(curResponse);
                    //If the leading bytes match our expected response
                    if (curResponse.responseID.Equals(leadingBytes[0]))
                    {
                        //Check message id matches
                        for (i = 1; i < leadingBytes.Length; ++i)
                        {
                            //Check remaining bytes to check
                            if (!curResponse.messageContents[i - 1].Equals(leadingBytes[i]))
                                break;
                        }
                        //If all bytes matched return the remaining bytes of the message
                        if (i == leadingBytes.Length)
                        {
                            //Save the seen messages back to the response buffer
                            responseBuf = new Queue<ANT_Response>(msgSave.Concat(responseBuf));

                            //Send out the information
                            lastAcceptedResponse = curResponse;
                            i -= 1; //Start at the byte we were at
                            return curResponse.messageContents.Skip(i).ToArray();
                        }
                    }

                }
            } while (waitTimer.ElapsedMilliseconds < wait_ms);

            //If we didn't find a matching response
            responseBuf = new Queue<ANT_Response>(msgSave.Concat(responseBuf));
            lastAcceptedResponse = null;
            throw new Exception("Expected Message Timeout");
        }

        /// <summary>
        /// Get last ANT response
        /// </summary>
        /// <returns>Message code of the last response</returns>
        public byte[] getLastAcceptedResponse()
        {
            return new byte[] { lastAcceptedResponse.responseID }.Concat(lastAcceptedResponse.messageContents).ToArray();
        }

        #endregion

        #region Functions Used by Scripting

        /// <summary>
        /// Send a raw ANT message
        /// </summary>
        /// <param name="msgToWrite">byte array, with the raw ANT message to send</param>
        /// <returns>True if the message was written successfully</returns>
        public bool writeRawMessageToDevice(byte[] msgToWrite)
        {
            return writeRawMessageToDevice(msgToWrite[0], msgToWrite.Skip(1).ToArray());
        }


        /// <summary>
        /// Wait for a message matching the desired pattern
        /// </summary>
        /// <param name="matchPattern">Pattern to match</param>
        /// <param name="wait_ms">time to wait, in ms</param>
        /// <returns>True if the matching message is seen in the configured timeout</returns>
        public bool WaitForNextMatchMsg(int[] matchPattern, int wait_ms)
        {
            Queue<ANT_Response> msgSave = new Queue<ANT_Response>();
            ANT_Response curResponse;
            int i;

            responseBuf.Clear();
            System.Diagnostics.Stopwatch waitTimer = new System.Diagnostics.Stopwatch();
            waitTimer.Start();
            do
            {
                while (responseBuf.Count > 0)
                {
                    System.Threading.Thread.Sleep(10); //Allow time for a new message to arrive
                    curResponse = responseBuf.Dequeue();
                    msgSave.Enqueue(curResponse);
                    //Check each byte with the conditional match function against the given conditional byte match array
                    if (compareConditionalByte(curResponse.responseID, matchPattern[0]) && curResponse.messageContents.Length == (matchPattern.Length - 1))
                    {
                        //Check message id matches
                        for (i = 1; i < matchPattern.Length; ++i)
                        {
                            //Check remaining bytes to check
                            if (!compareConditionalByte(curResponse.messageContents[i - 1], matchPattern[i]))
                                break;
                        }
                        //If all bytes matched then we succeeded
                        if (i == matchPattern.Length)
                        {
                            //Save all the seen messages back to the buffer
                            responseBuf = new Queue<ANT_Response>(msgSave.Concat(responseBuf));
                            lastAcceptedResponse = curResponse;
                            return true;
                        }
                    }
                }
            } while (waitTimer.ElapsedMilliseconds < wait_ms);

            //If we didn't find a match
            responseBuf = new Queue<ANT_Response>(msgSave.Concat(responseBuf));
            lastAcceptedResponse = null;
            return false;
        }

        /// <summary>
        /// Compare a byte with a reference value
        /// </summary>
        /// <param name="b">Byte to check (unsigned)</param>
        /// <param name="condByte">Reference signed value</param>
        /// <returns>Comparison result</returns>
        public static bool compareConditionalByte(byte b, int condByte)
        {
            //WARN This function does not check the int value to ensure it is in the range of the byte

            if (condByte >= 0)          //If it is not a special case
                return b == (byte)condByte;

            if (condByte == int.MinValue)    //this means match anything
                return true;

            //Otherwise the request is to match only set bytes
            condByte = -condByte; //Convert back to original positive value
            return condByte == (b & condByte);
        }

        #endregion

        #region Request Function

        /// <summary>
        /// Get the status of the ANT-FS session
        /// </summary>
        /// <returns></returns>
        public byte[] FS_GetState()
        {
            responseBuf.Clear();
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x30 });
        }

        #endregion


        #region Serial Commands

              #region MEMDev Commands

        /// <summary>
        /// MEMDEV initialization commands MUST be called after reset to set MEMDEV configuration prior to using FS Commands or requests.
        /// Init command must be called prior to issuing any FS command or requests.
        /// Currently, only SPI interface (min 2MHz rate) is supported with EEPROM devices.
        /// Configuration fields should be specified from information found from the EEPROM datasheet.
        /// Successful initialization results in FS_NO_ERROR response code.
        /// </summary>
        /// <param name="pageWriteSize"> Page Write Size is the physical page write boundary of the EEPROM Device.
        /// For EEPROM, this is considered the maximum number of bytes that can be written in one pass and it must be 2^x value. </param>
        /// <param name="AddrBytesCfg">The address bytes configuration field specifies the required number of bytes used to address the physical memory location
        /// on the EEPROM.
        /// For example, a 1MBit EEPROM device requires 3 address bytes. </param>
        /// <returns></returns>
        public byte MemDev_EEPROMInit(ushort pageWriteSize, byte AddrBytesCfg)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x20, getByte_usLE(1, pageWriteSize), getByte_usLE(2, pageWriteSize), AddrBytesCfg });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x20 })[0];
        }

        #endregion MEMDev Commands

              #region FS Commands

        /// <summary>
        /// Initializes existing file system from saved directory information in NVM.
        /// Unsaved information on open files will be lost.
        /// Init command must be called prior to using any FS related commands or requests.
        /// Init command must also be called after issuing FS Format Memory.
        /// Also resets encryption key used for crypto operations.
        /// Successful initialization results in FS_NO_ERROR response code.
        /// </summary>
        /// <returns></returns>
        public byte InitMemory()
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x00 });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x00 })[0];
        }

        /// <summary>
        /// Command used to create a new empty file system.
        /// Any existing directory information and files will be discarded.
        /// Minimum number of sectors must be 2 (1 for directory and 1 for each file).
        /// Successful format operation results in FS_NO_ERROR response code.
        /// Number of Sectors * Num Pages per sector * Page Size must not exceed the size of the memory device.
        /// If inappropriate values are entered, format may succeed, but FS will be unusable.
        /// </summary>
        /// <param name="numSec"> Number of Sectors </param>
        /// <param name="pagesPerSector"> Num Pages per Sector. Defines sector size(X * page Size)
        ///  Page Size is defined in MEMDEV_EEPROM_INIT for EEPROM Device</param>
        /// <returns></returns>
        public byte Format_Memory(ushort numSec, ushort pagesPerSector)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x01, getByte_usLE(1, numSec), getByte_usLE(2, numSec), getByte_usLE(1, pagesPerSector), getByte_usLE(2, pagesPerSector) });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x01 })[0];
        }

        /// <summary>
        /// Save all open file information into the directory NVM.
        /// This should be called before device power off or any unsaved data will be lost.
        /// Successful save operation results in FS_NO_ERROR response code.
        /// </summary>
        /// <returns></returns>
        public byte DirectorySave()
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x07 });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x07 })[0];
        }


        /// <summary>
        /// Rebuilds FS directory and condenses directory size by removing invalidated entries.
        /// Rebuilding directory also updates auto file index counter.
        /// Successful rebuild results in FS_NO_ERROR response code.
        /// </summary>
        /// <returns></returns>
        public byte DirectoryRebuild()
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x09 });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x09 })[0];
        }

        /// <summary>
        /// Delete existing open file. Delete permission must be set on file handle. On successful deletion (FS_NO_ERROR response code), the file handle is freed.
        /// If FS_MEMORY_WRITE_ERROR is returned, memory occupied by file is lost but the handle is freed.
        /// Any other response codes results in file deletion failure and the file handle remains associated to the open file.
        /// </summary>
        /// <param name="handleNum"> File Handle Number</param>
        /// <returns></returns>
        public byte FileDelete(byte handleNum)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x0C, handleNum });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x0C })[0];
        }

        /// <summary>
        /// Close open flag or file. Any open file handle information is saved to the directory.
        /// FS_NO_ERROR response code is returned if file close operation is successful.
        /// Any other response code resulted in file close failure and the file handle still assigned to the file.
        /// </summary>
        /// <param name="handleNum">File Handle Number</param>
        /// <returns></returns>
        public byte FileClose(byte handleNum)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x0D, handleNum });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x0D })[0];
        }

        /// <summary>
        /// Update application defined flags on file, but it is not saved to directory NVM.
        /// On success, FS_NO_ERROR is returned.
        /// </summary>
        /// <param name="handleNum">File Handle Number</param>
        /// <param name="newSpecificFlags">Specific Flags</param>
        /// <returns></returns>
        public byte FileSetSpecificFlags(byte handleNum, byte newSpecificFlags)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x12, handleNum, newSpecificFlags });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x12 })[0];
        }

        /// <summary>
        /// When directory is locked, directory information is prevented from changing.
        /// When directory is unlocked, directory information is allowed to change.
        /// Attempting to lock a directory that is already locked will result in an error.
        /// Attempting to unlock a directory that is already unlocked will result in an error.
        /// </summary>
        /// <param name="locked">1 - Lock, 0 - Unlock</param>
        /// <returns>FS Response</returns>
        public byte DirectoryReadLock(byte locked)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x16, locked });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x16 })[0];
        }


        /// <summary>
        /// When this message ID is used in a command message, the specified starting value of the system time to be used in FS can be set.
        /// If successful, FS_NO_ERROR is returned
        /// </summary>
        /// <param name="seconds">Current Time</param>
        /// <returns></returns>
        public byte SetSystemTime(uint seconds)
        {
            //convert int to USHORT so can use in the getByte_usLE()
            ushort LSBytes;
            ushort MSbytes;

            LSBytes = (ushort)seconds;
            LSBytes |= (ushort)(seconds & 0x0000FF00);

            MSbytes = (ushort)((seconds & 0x00FF0000) >> 16);
            MSbytes |= (ushort)((seconds & 0xFF000000) >> 16);

            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x3D, getByte_usLE(1, LSBytes), getByte_usLE(2, LSBytes), getByte_usLE(1, MSbytes), getByte_usLE(2, MSbytes) });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x3D })[0];
        }
        #endregion FS Commands

              #region FS Requests
        /// <summary>
        /// Returns the FSResponse Byte when reponse != 0x00(NO_ERROR)
        /// </summary>
        /// <param name="FSResponse">FS Response byte from Response Buffer</param>
        /// <returns></returns>
        public byte FS_GetLastError(byte FSResponse)
        {
            return FSResponse;
        }

        /// <summary>
        /// Returns number of used bytes in FS in sector sized increments.
        /// On success, returns FS_NO_ERROR response code and the used space value.
        /// If any other response code is returned, an invalid used space size value is returned (0xFFFFFFFF).
        /// </summary>
        /// <returns>Used Space</returns>
        public ulong GetUsedSpace()
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE1, new byte[] { 0x00, 0xE2, 0x02 });
            byte[] ret = waitForMsg(new byte[] { 0xE2, 0x02 });

            if (ret[0] == 0x00)      //No Error
                return (ulong)ret[1] + ((ulong)ret[2] << 8) + ((ulong)ret[3] << 16) + ((ulong)ret[4] << 24);
            else                     //There is an error
                return FS_GetLastError(ret[0]);
        }

        /// <summary>
        /// Returns number of free bytes in FS in sector sized increments.
        /// On success, returns FS_NO_ERROR response code and the free space value.
        /// If any other response code is returned, an invalid free space size value is returned (0xFFFFFFFF).
        /// </summary>
        /// <returns>Free Space</returns>
        public ulong GetFreeSpace()
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE1, new byte[] { 0x00, 0xE2, 0x03 });
            byte[] ret = waitForMsg(new byte[] { 0xE2, 0x03 });

            if (ret[0] == 0x00)
                return (ulong)ret[1] + ((ulong)ret[2] << 8) + ((ulong)ret[3] << 16) + ((ulong)ret[4] << 24);
            else
                return FS_GetLastError(ret[0]);
        }

        /// <summary>
        /// Return file index of first file in directory that matches specified identifier.
        /// On success, returns FS_NO_ERROR response code and the file index.
        /// If any other response code is returned, an invalid file index is returned (0x0000
        /// </summary>
        /// <param name="FileDataType"> File Data Type</param>
        /// <param name="FileSubType"> File Sub Type </param>
        /// <param name="FileNumber">File Number</param>
        /// <returns>File Index</returns>
        public ulong FindFileIndex(byte FileDataType, byte FileSubType, ushort FileNumber)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE1, new byte[] { 0x00, 0xE2, 0x04, FileDataType, FileSubType, getByte_usLE(1, FileNumber), getByte_usLE(2, FileNumber)});
            byte[] ret = waitForMsg(new byte[] { 0xE2, 0x04 });

            if (ret[0] == 0x00)
                return ((ulong)ret[1]) + ((ulong)ret[2] << 8);
            else
                return FS_GetLastError(ret[0]);
        }


        /// <summary>
        /// Read from absolute offset into directory as if it were an ANTFS directory.
        /// </summary>
        /// <param name="offset"> Offset </param>
        /// <param name="readSize"> Bytes Read Size</param>
        /// <returns></returns>
        public byte[] DirectoryReadAbsolute(ulong offset, byte readSize)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE1, new byte[] { 0x00, 0xE2, 0x05, getByte_ulLE(1, offset), getByte_ulLE(2, offset), getByte_ulLE(3, offset), getByte_ulLE(4, offset), readSize });
            return waitForMsg(new byte[] { 0xE2, 0x05 }, 10000);
        }

        /// <summary>
        /// Returns ANTFS directory entry for the file matching the specified file index
        /// </summary>
        /// <param name="FileIndex"></param>
        /// <returns>ANTFS_DIR_ENTRY</returns>
        public byte[] DirectoryReadEntry(ushort FileIndex)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE1, new byte[] { 0x00, 0xE2, 0x06, getByte_ulLE(1, FileIndex), getByte_ulLE(2, FileIndex)});
            byte[] ret = waitForMsg(new byte[] { 0xE2, 0x06 }, 10000);

            if(ret[0] == 0x00)
                return (new byte[]{ret[1], ret[2], ret[3], ret[4], ret[5], ret[6], ret[7], ret[8], ret[9], ret[10], ret[11], ret[12], ret[13], ret[14], ret[15], ret[16]});
            else
                return (new byte[]{FS_GetLastError(ret[0])});
        }

        /// <summary>
        /// Returns size in bytes as if it were an ANTFS directory (16-byte blocks).
        /// On success, returns FS_NO_ERROR response code and the ANTFS directory size value.
        /// If any other response code is returned, an invalid ANTFS directory size value is returned (0xFFFFFFFF).
        /// </summary>
        /// <returns>Directory Size</returns>
        public ulong DirectoryGetSize()
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE1, new byte[] { 0x00, 0xE2, 0x08 });
            byte[] ret = waitForMsg(new byte[] { 0xE2, 0x08 });

            if (ret[0] == 0x00)
                return (ulong)ret[1] + ((ulong)ret[2] << 8) + ((ulong)ret[3] << 16) + ((ulong)ret[4] << 24);
            else
                return FS_GetLastError(ret[0]);
        }

        /// <summary>
        /// Allocates a free sector and saves directory entry of the new file.
        /// If 0x0000 is supplied as file index, FS will auto generated a valid index for the file,
        /// otherwise a valid index must manually be supplied.
        /// If file creation is successful (FS_NO_ERROR is returned), the file index assigned to the created file is returned.
        /// Any other errors results in an invalid file index being returned (0x0000).
        /// </summary>
        /// <param name="fileIndex">File Index</param>
        /// <param name="ucFileDataType">File Data Type</param>
        /// <param name="ucFileSubType">File Sub Type</param>
        /// <param name="usFileNumber">File Number</param>
        /// <param name="ucFileDataTypeSpecificFlags">File Data Type Specific Flags</param>
        /// <param name="ucFileGeneralFlags">File General Flags</param>
        /// <returns></returns>
        public Int32 FileCreate(ushort fileIndex, byte ucFileDataType, byte ucFileSubType, ushort usFileNumber, byte ucFileDataTypeSpecificFlags, byte ucFileGeneralFlags)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE1, new byte[] { 0x00, 0xE2, 0x0A, getByte_usLE(1, fileIndex), getByte_usLE(2, fileIndex), ucFileDataType, ucFileSubType, getByte_usLE(1, usFileNumber), getByte_usLE(2, usFileNumber), ucFileDataTypeSpecificFlags, ucFileGeneralFlags });

            try
            {
                //Check for the return index
                byte[] retAry = waitForMsg(new byte[] { 0xE2, 0x0A });
                if(retAry[0] == 0x00)
                    return retAry[1] + (retAry[2] << 8);
                else
                    return FS_GetLastError(retAry [0]);

            }
            catch (Exception)
            {
                //Otherwise return an error as a negative number
                return (Int32)(waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x0A })[0]) * -1;
            }
        }

        /// <summary>
        /// Open existing file in FS. By default, read and write pointers are set at the beginning of the file.
        /// If append flag is set in Open Flags parameter, then the write pointer is set to the end of the file.
        /// If file open is successful (FS_NO_ERROR, with the exceptions discussed in .FIT File and Crypto .FIT File),
        /// the file handle number is returned.
        /// Any other response code results in file open failure and the file handle returned being invalid (0xFF).
        /// </summary>
        /// <param name="fileIndex">File Index</param>
        /// <param name="openFlags">Open Flags</param>
        /// <returns></returns>
        public Int32 FileOpen(ushort fileIndex, byte openFlags)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE1, new byte[] { 0x00, 0xE2, 0x0B, getByte_usLE(1, fileIndex), getByte_usLE(2, fileIndex), openFlags });

            try
            {
                //Check for the return index
                byte[] retAry = waitForMsg(new byte[] { 0xE2, 0x0B });

                if (retAry[0] == 0x00)
                    return retAry[1];
                else
                    return FS_GetLastError(retAry [0]);
            }
            catch (Exception)
            {
                //Otherwise return an error as a negative number
                return (Int32)(waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x0B })[0]) * -1;
            }
        }

        /// <summary>
        /// Read from absolute offset into a file. File must be opened for reading beforehand.
        /// After reading, read pointers positioned at the end of the bytes read.
        /// On successful reads (FS_NO_ERROR), the returned number of bytes read as well as the payload is returned.
        /// Reading past the end of the directory results in FS_EOF_REACHED_ERROR, however the number of read bytes and the payload prior to reaching EOF is returned.
        /// </summary>
        /// <param name="handleNum">File Handle Number</param>
        /// <param name="offset">Offset</param>
        /// <param name="readSize">Read Size</param>
        /// <returns></returns>
        public byte[] FileReadAbsolute(byte handleNum, ulong offset, byte readSize)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE1, new byte[] { 0x00, 0xE2, 0x0E, handleNum, getByte_ulLE(1, offset), getByte_ulLE(2, offset), getByte_ulLE(3, offset), getByte_ulLE(4, offset), readSize });
            return waitForMsg(new byte[] { 0xE2, 0x0E });
        }

        /// <summary>
        /// Read from current read pointer position in file. File must be opened for reading beforehand.
        /// After reading, read pointers positioned at the end of the bytes read.
        /// </summary>
        /// <param name="handleNum">File Handle Number</param>
        /// <param name="readSize">Read Size</param>
        /// <returns></returns>
        public byte[] FileReadRelative(byte handleNum, byte readSize)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE1, new byte[] { 0x00, 0xE2, 0x0F, handleNum, readSize });
            return waitForMsg(new byte[] { 0xE2, 0x0F });
        }

        /// <summary>
        /// Write to absolute offset into a file. File cannot be written to if it was opened for reading.
        /// Write absolute cannot be used if file only opened for append operation.
        /// After writing, write positioned at the end of the bytes written.
        /// </summary>
        /// <param name="handleNum">File Handle Number</param>
        /// <param name="offset">Offset</param>
        /// <param name="bytesToWrite">Write Payload</param>
        /// <returns></returns>
        public byte[] FileWriteAbsolute(byte handleNum, ulong offset, byte[] bytesToWrite)
        {
            responseBuf.Clear();
            bytesToWrite = (new byte[] { 0x00, 0xE2, 0x10, handleNum, getByte_ulLE(1, offset), getByte_ulLE(2, offset), getByte_ulLE(3, offset), getByte_ulLE(4, offset), (byte)bytesToWrite.Length }.Concat(bytesToWrite)).ToArray();
            writeRawMessageToDevice(0xE1, bytesToWrite);
            return waitForMsg(new byte[] { 0xE2, 0x10 });
        }

        /// <summary>
        /// Write to current write pointer position in file. File cannot be written to if opened for reading.
        /// </summary>
        /// <param name="handleNum">File Handle Number</param>
        /// <param name="bytesToWrite">Write Payload</param>
        /// <returns></returns>
        public byte[] FileWriteRelative(byte handleNum, byte[] bytesToWrite)
        {
            responseBuf.Clear();
            bytesToWrite = (new byte[] { 0x00, 0xE2, 0x11, handleNum, (byte)bytesToWrite.Length }.Concat(bytesToWrite)).ToArray();
            writeRawMessageToDevice(0xE1, bytesToWrite);
            return waitForMsg(new byte[] { 0xE2, 0x11 });
        }


        /// <summary>
        /// Get size of open file in bytes. If successful, FS_NO_ERROR is returned along with the file size in bytes.
        /// If any other response code is returned, an invalid file size value is returned (0xFFFFFFFF).
        /// </summary>
        /// <param name="handleNum">File Handle Number</param>
        /// <returns>File Size</returns>
        public ulong FileGetSize(byte handleNum)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE1, new byte[] { 0x00, 0xE2, 0x13, handleNum });
            try
            {
                byte[] ret = waitForMsg(new byte[] { 0xE2, 0x13 });
                if(ret[0] == 0x00)
                    return (ulong)ret[1] + ((ulong)ret[2] << 8) + ((ulong)ret[3] << 16) + ((ulong)ret[4] << 24);
                else
                    return FS_GetLastError(ret[0]);
            }
            catch (Exception)
            {
                return 0xFFFFFF00 + waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x13 })[0];
            }
        }

        /// <summary>
        /// Get size of the file in terms of the number of total bytes allocated to the file in the FS (size in memory).
        /// If successful, FS_NO_ERROR is returned along with the size in bytes.
        /// If any other response code is returned, an invalid file size value is returned (0xFFFFFFFF).
        /// </summary>
        /// <param name="handleNum">File handle Number</param>
        /// <returns></returns>
        public ulong FileGetSizeInMem(byte handleNum)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE1, new byte[] { 0x00, 0xE2, 0x15, handleNum });
            try
            {
                byte[] ret = waitForMsg(new byte[] { 0xE2, 0x15 });
                if (ret[0] == 0x00)
                    return (ulong)ret[1] + ((ulong)ret[2] << 8) + ((ulong)ret[3] << 16) + ((ulong)ret[4] << 24);
                else
                    return FS_GetLastError(ret[0]);
            }
            catch (Exception)
            {
                return 0xFFFFFF00 + waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x13 })[0];
            }
        }



        /// <summary>
        /// Gets the application defined flags of opened file.
        /// If successful, FS_NO_ERROR is returned along with the application defined flags on the file.
        /// If any other response code is returned, flag value of 0x00 is returned.
        /// </summary>
        /// <param name="handleNum">File Handle Number</param>
        /// <returns>File Flags</returns>
        public ulong FileGetSpecificFileFlags(byte handleNum)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE1, new byte[] { 0x00, 0xE2, 0x14, handleNum });
            try
            {
                byte[] ret = waitForMsg(new byte[] { 0xE2, 0x14 });
                if(ret[0] == 0x00)
                    return (ulong)ret[1] + ((ulong)ret[2] << 8);
                else
                    return FS_GetLastError(ret[0]);
            }
            catch (Exception)
            {
                return 0xFFFFFF00 + waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x14 })[0];
            }
        }

        /// <summary>
        /// When this message ID is used in a request message, the current system time used in FS is returned.
        /// </summary>
        /// <param name="seconds">Current Time</param>
        /// <returns></returns>
        public ulong SystemTime(uint seconds)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x3D});

            byte[] ret = waitForMsg(new byte[] {0xE2, 0x3D });

            return (ulong)ret[0] + ((ulong)ret[1] << 8) + ((ulong)ret[2] << 16) + ((ulong)ret[3] << 24);
        }

        #endregion

              #region FS-Crypto Commands

        /// <summary>
        /// Adds specified user key to be stored in internal memory.
        /// Keys are enumerated by Key Index. Up to 10 keys can be used.
        /// If successfully stored, FS_NO_ERROR is returned.
        /// </summary>
        /// <param name="keyindex">Key Index</param>
        /// <param name="CK">User Crypto Key</param>
        /// <returns>FS Response</returns>
        public byte Crypto_AddUserKeyIndex(byte keyindex, byte[] CK)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x45, keyindex, CK[0], CK[1], CK[2], CK[3], CK[4], CK[5], CK[6], CK[7], CK[8], CK[9], CK[10], CK[1], CK[12], CK[13],
            CK[14], CK[15],CK[16], CK[17],CK[18], CK[19],CK[20], CK[21],CK[22], CK[23],CK[24], CK[25],CK[26], CK[27],CK[28], CK[29],CK[30], CK[31]});
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x45 })[0];
        }

        /// <summary>
        /// Specify stored user key (specified by Key Index) to be used by FS Encryption/Decryption process.
        /// Key remains active until reset, memory re-initialization via MESG_FS_INIT MEMORY or another key is specified.
        /// If key successfully selected, FS_NO_ERROR is returned
        /// </summary>
        /// <param name="keyindex">User Key Index</param>
        /// <returns>FS Response</returns>
        public byte Crypto_SetUserKeyIndex(byte keyindex)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x46, keyindex });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x46 })[0];
        }

        /// <summary>
        /// Specify non-stored user key to be used by FS Encryption/Decryption process.
        /// Key remains active until reset, memory re-initialization via MESG_FS_INIT_MEMORY or another key is specified.
        /// If key successfully set, FS_NO_ERROR is returned.d
        /// </summary>
        /// <param name="CK">User Crypto Key</param>
        /// <returns>FS Response</returns>
        public byte Crypto_SetUserKeyValue(byte[] CK)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x47, CK[0], CK[1], CK[2], CK[3], CK[4], CK[5], CK[6], CK[7], CK[8], CK[9], CK[10], CK[1], CK[12], CK[13],
            CK[14], CK[15],CK[16], CK[17],CK[18], CK[19],CK[20], CK[21],CK[22], CK[23],CK[24], CK[25],CK[26], CK[27],CK[28], CK[29],CK[30], CK[31]});
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x47 })[0];
        }

        #endregion

              #region FS_FIT Commands

        /// <summary>
        ///  When this command is issued, a file integrity check is performed on the selected .FIT file
        ///  by calculating the file 2 byte CRC and comparing it versus the appended 2 byte CRC.
        ///  If file integrity is intact, then FS_NO_ERROR is returned.Provided file handle must be pointing to a .FIT file (0x80 data type in file directory entry) as well as opened as read-only.
        ///  Performing an integrity check on a non .FIT file and/or a write handle (write/erase/append open flags) is not allowed.
        /// </summary>
        /// <param name="handle">File Handle Number</param>
        /// <returns>FS Response</returns>
        public byte FIT_FileIntegrityCheck(byte handle)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x50, handle });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x50 })[0];
        }

        #endregion

              #region ANTFS Commands

        /// <summary>
        /// Start ANTFS beacon.
        /// </summary>
        /// <returns>FS Response</returns>
        public byte ANTFS_Open()
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x31 });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x31 })[0];
        }

        /// <summary>
        /// Stop ANTFS beacon.
        /// </summary>
        /// <returns>FS Response</returns>
        public byte ANTFS_Close()
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x32 });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x32 })[0];
        }

        /// <summary>
        /// Configures the ANTFS Beacon properties.
        /// </summary>
        /// <param name="BeaconDeviceType">Beacon Device Type ID</param>
        /// <param name="ManID">Manufacturers ID</param>
        /// <param name="AuthKey">Authentication Type</param>
        /// <param name="Status">Beacon Status</param>
        /// <returns>FS Response</returns>
        public byte ANTFS_ConfigBeacon(ushort BeaconDeviceType, ushort ManID, byte AuthKey, byte Status)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x33, getByte_usLE(1, BeaconDeviceType), getByte_usLE(2, BeaconDeviceType), getByte_usLE(1, ManID), getByte_usLE(2, ManID), AuthKey, Status });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x33 })[0];
        }

        /// <summary>
        /// Set the Friendlyname
        /// </summary>
        /// <param name="friendlyname">Friendlyname as a string</param>
        /// <returns>FS Response</returns>
        public byte ANTFS_SetFriendlyName(string friendlyname)
        {
            responseBuf.Clear();
            int stringlength = friendlyname.Length;

            byte[] temp = System.Text.Encoding.ASCII.GetBytes(friendlyname);
            byte[] asciibytes = new byte[16];

            for (int i = 0; i < stringlength; i++)
                asciibytes[i] = System.Convert.ToByte(temp[i]);

            writeRawMessageToDevice(0xE2, new byte[] {0x34, 0x00, asciibytes[0],asciibytes[1],asciibytes[2],asciibytes[3],asciibytes[4],asciibytes[5],asciibytes[6],
                                                            asciibytes[7],asciibytes[8],asciibytes[9],asciibytes[10],asciibytes[11],asciibytes[12],asciibytes[13],asciibytes[14],asciibytes[15]});
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x34 })[0];
        }

        /// <summary>
        /// Sets the Passkey
        /// </summary>
        /// <param name="passkey">Passkey as a string</param>
        /// <returns>FS Response</returns>
        public byte ANTFS_SetPasskey(string passkey)
        {
            responseBuf.Clear();

            System.Text.RegularExpressions.Regex regex = new System.Text.RegularExpressions.Regex(",");
            String[] DataArray = regex.Split(passkey);
            int arraylength = DataArray.Length;
            byte[] passkeybytes = new byte[16];

            for (int i = 0; i < arraylength; i++)
                passkeybytes[i] = System.Convert.ToByte(DataArray[i], 16);

            writeRawMessageToDevice(0xE2, new byte[] {0x34,0x01,passkeybytes[0],passkeybytes[1],passkeybytes[2],passkeybytes[3],passkeybytes[4],passkeybytes[5],passkeybytes[6],
                                                            passkeybytes[7],passkeybytes[8],passkeybytes[9],passkeybytes[10],passkeybytes[11],passkeybytes[12],passkeybytes[13],passkeybytes[14],passkeybytes[15]});
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x34 })[0];
        }

        /// <summary>
        /// Sets the ANTFS beacon status byte.
        /// </summary>
        /// <param name="Status">Bits 0 -2 are invalid</param>
        /// <returns>FS Response</returns>
        public byte ANTFS_SetBeaconState(byte Status)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x35, Status});
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x35 })[0];
        }

        /// <summary>
        /// Command to respond to pairing request.
        /// </summary>
        /// <param name="decision">0 - Reject , 1 - Accept</param>
        /// <returns>FS Response</returns>
        public byte ANTFS_PairResponse(byte decision)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x36, decision });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x36 })[0];
        }

        /// <summary>
        /// Set the Beacon/Link RF frequency of the ANTFS connection
        /// </summary>
        /// <param name="channelnum">Channel Number</param>
        /// <param name="frequency">24xxMhz, xx as byte, 0xFF is to Disable</param>
        /// <returns>FS Response</returns>
        public byte ANTFS_SetLinkFrequency(byte channelnum, byte frequency)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x37, channelnum, frequency });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x37 })[0];
        }


        /// <summary>
        /// Configure timeout for ANTFS beacon.
        /// </summary>
        /// <param name="timeout">Timeout in seconds</param>
        /// <returns>FS Response</returns>
        public byte ANTFS_SetBeaconTimeout(byte timeout)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x38, timeout });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x38 })[0];
        }

        /// <summary>
        /// Configure timeout for ANTFS pairing process
        /// </summary>
        /// <param name="PairTimeout">Timeout in seconds</param>
        /// <returns>FS Response</returns>
        public byte ANTFS_SetPairingTimeout(byte PairTimeout)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x39, PairTimeout });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x39 })[0];
        }

        /// <summary>
        /// Enables or disables file creation in FS through ANTFS.
        /// </summary>
        /// <param name="enable">1 - Enable, 0 - Disable</param>
        /// <returns>FS Response</returns>
        public byte ANTFS_RemoteFileCreateEnable(byte enable)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE2, new byte[] { 0x3A, enable });
            return waitForMsg(new byte[] { 0xE0, 0x00, 0xE2, 0x3A })[0];
        }

        #endregion

              #region ANTFS Requests

        /// <summary>
        /// Command to read ANTFS command pipe data.
        /// </summary>
        /// <param name="offset">Offset</param>
        /// <param name="readsize">Read Size</param>
        /// <returns>Read Size and Read Payload</returns>
        public byte[] ANTFS_GetCmdPipe(byte offset, byte readsize)
        {
            responseBuf.Clear();
            writeRawMessageToDevice(0xE1, new byte[] { 0x00, 0xE2, 0x3B, offset, readsize });
            return waitForMsg(new byte[] { 0xE2, 0x3B }, 10000);
        }

        /// <summary>
        /// Command to write ANTFS command pipe data.
        /// </summary>
        /// <param name="offset">Offset</param>
        /// <param name="bytesToWrite">Bytes to be written</param>
        /// <returns>Size written</returns>
        public byte ANTFS_SetCmdPipe(byte offset, byte[] bytesToWrite)
        {
            responseBuf.Clear();
            bytesToWrite = (new byte[] { 0x00, 0xE2, 0x3C, offset, (byte)bytesToWrite.Length }.Concat(bytesToWrite)).ToArray();
            writeRawMessageToDevice(0xE1, bytesToWrite);
            return waitForMsg(new byte[] { 0xE2, 0x3C })[0];
        }
        #endregion

        #endregion Serial Commands
    }
}


