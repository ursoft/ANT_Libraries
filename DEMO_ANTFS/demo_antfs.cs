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
using ANT_Managed_Library;

namespace ANTFS_Demo
{
    class Demo
    {
        // ANT Channel Configuration Parameters
        // We define them here, since these must match between host and client for communication to work
        public readonly static byte SearchRF = (byte)ANT_Managed_Library.ANTFS.RadioFrequency.ANTFSNetwork;  // Radio Frequency to use during link state
        public readonly static ushort ChannelPeriod = 8192; // ANT channel period.  It should match the LinkPeriod below.
        public readonly static byte DeviceType = 1;         // Device Type (ANT Channel ID)
        public readonly static byte TransmissionType = 5;   // Transmission Type (ANT Channel ID)
        public readonly static byte NetworkNumber = 0;
        public readonly static byte[] NetworkKey = { 0xA8, 0xA4, 0x23, 0xB9, 0xF5, 0x5E, 0x63, 0xC1 };
        // ANT-FS Configuration Parameters
        // These must also match between host and client
        public readonly static ushort ClientManufacturerID = 2;      // ANT-FS Client Manufacturer ID
        public readonly static ushort ClientDeviceType = 416;        // ANT-FS Client Device Type
        public readonly static ANT_Managed_Library.ANTFS.BeaconPeriod LinkPeriod = ANT_Managed_Library.ANTFS.BeaconPeriod.FourHz;
        // This is the channel period included in the beacon. Set to match ChannelPeriod above.
        // If using ANT-FS Broadcast and the ChannelPeriod is not a standard value defined in the ANT-FS spec, set to BeaconPeriod.Keep.

        public readonly static string[] CursorStrings = { "|", "/", "_", "\\" };  // To display rotating "cursor" while data is being broadcast

        public static readonly bool AntfsBroadcast = false;   // Set to false to start the session in standard ANT-FS mode.  Set to true to start in broadcast mode.

        static ANT_Device device0;  // ANT USB device
        static byte selection = Byte.MaxValue;

        /// <summary>
        /// Demo program for ANT-FS C# library
        /// </summary>
        /// <remarks>
        /// Usage:
        /// demo_antfs.exe [selection]
        ///
        /// ... where
        /// selection: ANT-FS Host = 0, ANT-FS Client = 1
        ///
        /// ... example
        /// demo_antfs.exe 0
        /// will connect to an ANT USB stick and start an ANT-FS session as a host
        ///
        /// If optional arguments are not supplied, user will be prompted
        /// to entere these after the program starts
        /// </remarks>
        /// <param name="args">User selection: 0 = host, 1 = client</param>
        static void Main(string[] args)
        {
            if (args.Length > 0)
            {
                selection = byte.Parse(args[0]);
            }

            try
            {
                Init();
                Start(selection);
             }
            catch (Exception ex)
            {
                Console.WriteLine("Demo failed with exception: \n" + ex.Message);
            }
        }

        /// <summary>
        /// Initialize the demo parameters
        /// </summary>
        static void Init()
        {
            try
            {
                Console.WriteLine("Attempting to connect to an ANT USB device...");
                ANT_Common.enableDebugLogs();   // Enable debugging first, to enable ANT-FS specific logging
                device0 = new ANT_Device();   // Create a device instance using the automatic constructor (automatic detection of USB device number and baud rate)
                Console.WriteLine("Initialization was successful!");
            }
            catch (Exception ex)
            {
                if (device0 == null)    // Unable to connect to ANT
                {
                    throw new Exception("Could not connect to any device.\n" +
                    "Details: \n   " + ex.Message);
                }
                else
                {
                    throw new Exception("Error connecting to ANT: " + ex.Message);
                }
            }
        }

        /// <summary>
        /// Start the demo program
        /// </summary>
        /// <param name="userSelection">User selection: 0 = host, 1 = client.  255 passed as invalid</param>
        static void Start(byte userSelection)
        {
            selection = userSelection;

            // If the selection between host/client has not been specified, prompt the user
            do
            {
                if (selection == Byte.MaxValue)
                {
                    Console.WriteLine("Please select (Host = 0, Client = 1)");
                    try
                    {
                        selection = byte.Parse(Console.ReadLine());
                        if (selection != 0 && selection != 1)
                            throw new FormatException("Error: Invalid selection");
                    }
                    catch (Exception)
                    {
                        selection = Byte.MaxValue;
                    }
                }
            } while (selection == Byte.MaxValue);

            if (selection == 0)
            {
                HostDemo theDemo = new HostDemo();
                theDemo.Start(device0);
            }
            else if (selection == 1)
            {
                ClientDemo theDemo = new ClientDemo();
                theDemo.Start(device0);
            }
        }

        /// <summary>
        /// Parse user input as a numeric value
        /// </summary>
        /// <param name="selection">String containing user input</param>
        /// <returns>Numeric value</returns>
        public static ushort ParseInput(string selection)
        {
            try
            {
                return UInt16.Parse(selection);
            }
            catch (Exception ex)
            {
                throw new System.ArgumentException("Invalid input", ex);
            }
        }

    }
}
