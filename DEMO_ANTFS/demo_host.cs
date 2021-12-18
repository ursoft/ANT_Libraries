/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/

//////////////////////////////////////////////////////////////////////////
// To use the ANT-FS functionality in the managed library, you must:
// 1. Import ANT_NET.dll as a reference
// 2. Reference the ANT_Managed_Library and ANT_Managed_Library.ANTFS namespaces
// 3. Include the following files in the working directory of your application:
//  - DSI_CP310xManufacturing_3_1.dll
//  - DSI_SiUSBXp_3_1.dll
//  - ANT_WrappedLib.dll
//  - ANT_NET.dll
// 4. You will also need to copy ANT_NET.xml into your project for
//    Intellisense to work properly
//////////////////////////////////////////////////////////////////////////

using System;
using System.Text;
using System.IO;
using System.Collections.Generic;
using System.Threading;
using ANT_Managed_Library;
using ANT_Managed_Library.ANTFS;

namespace ANTFS_Demo
{
    /// <summary>
    /// This demo shows how to configure a basic ANT-FS host
    /// For a more featured demo, please refer to the source code of the ANT-FS PC tools (however, the host source code is in C++)
    /// </summary>
    public class HostDemo
    {
        // ANT-FS Configuration Parameters
        readonly string HostFriendlyName = "ConsoleHost"; // ANT-FS Host Friendly Name
        readonly byte ConnectRF = (byte)ANT_Managed_Library.ANTFS.RadioFrequency.Auto;  // Radio Frequency to use once a connection is established (set by host)
        readonly uint ClientDeviceID = 0;   // ANT-FS Client Device ID (Serial Number): Wildcard to match any client ID
        readonly ushort DeviceNumber = 0;   // Device Number (ANT Channel ID): Wildcard to match any client Device Number
        readonly byte SearchTimeout = 60;   // How long to search for a client device, in seconds

        ANT_Channel channel0;               // ANT channel to use
        ANTFS_HostChannel antfsHost;        // ANT-FS host          
        ANTFS_Directory dirFS;              // ANT-FS directory
        byte[] clientPassKey;               // Used to store pass key retrieved from the client during pairing
        bool demoDone = false;              // Signal when the demo is done to exit
        int cursorIndex = 0;                // Index to handle cursor display while transmitting in broadcast mode
        ushort currentDownloadIndex = 0;    // Current index being downloaded.  Start with directory (0)
        Timer searchTimer;                  // Timer to limit search time, in case there are no client devices around

        /// <summary>
        /// Setup ANT-FS host to process messages from connected USB device
        /// </summary>
        public void Start(ANT_Device antDevice)
        {
            PrintMenu();
            try
            {
                // Create the ANT-FS host and attach it to a channel (in this case, 0)
                channel0 = antDevice.getChannel(0);
                antfsHost = new ANTFS_HostChannel(channel0);
                                
                // Setup callback to handle ANT-FS response events
                antfsHost.OnResponse += new Action<ANTFS_HostChannel.Response>(HandleHostResponses);

                // Configure the host, and begin searching
                ConfigureHost(antDevice);

                while (!demoDone)
                {
                    string command = Console.ReadLine();
                    HandleUserInput(command);
                    Thread.Sleep(0);
                }                
            }
            catch(ANTFS_Exception antEx)  // Handle exceptions thrown by ANT Managed library
            {
                Console.WriteLine("ANT-FS Exception: " + antEx.Message);
            }
            catch (Exception ex)  // Handle other exceptions
            {
                Console.WriteLine("Demo failed: " + ex.Message);    
            }
            finally
            {
                Console.WriteLine("Disconnecting module...");
                antDevice.Dispose();    // Close down the device completely and completely shut down all communication 
                antfsHost.Dispose();    // Release all native resources used by the host 
                Console.WriteLine("Demo has completed successfully!");
            }
        }

        /// <summary>
        /// Configure the host search parameters, and begin the search
        /// </summary>
        /// <param name="antDevice">ANT USB device</param>
        public void ConfigureHost(ANT_Device antDevice)
        {
            // Configure ANT channel parameters
            antfsHost.SetNetworkKey(Demo.NetworkNumber, Demo.NetworkKey);
            antfsHost.SetChannelID(Demo.DeviceType, Demo.TransmissionType);
            antfsHost.SetChannelPeriod(Demo.ChannelPeriod);
            
            // Configure search parameters
            if (antfsHost.AddSearchDevice(ClientDeviceID, Demo.ClientManufacturerID, Demo.ClientDeviceType) == 0)
                throw new Exception("Error adding search device: ");

            if (Demo.AntfsBroadcast)
            {  
                // If we want to use ANT-FS broadcast mode, and start the channel in broadcast mode, 
                // setup callback to handle responses for channel 0 while in broadcast mode
                channel0.channelResponse += new dChannelResponseHandler(HandleChannelResponses);

                // Configure the channel as a slave, and look for messages from the master in the channel callback
                // This demo will automatically switch to ANT-FS mode after a few seconds
                if (!antDevice.setNetworkKey(Demo.NetworkNumber, Demo.NetworkKey, 500))
                    throw new Exception("Error configuring network key");
                if (!channel0.assignChannel(ANT_ReferenceLibrary.ChannelType.BASE_Slave_Receive_0x00, Demo.NetworkNumber, 500))
                    throw new Exception("Error assigning channel");
                if (!channel0.setChannelID(DeviceNumber, false, Demo.DeviceType, Demo.TransmissionType, 500))
                    throw new Exception("Error configuring Channel ID");
                if (!channel0.setChannelFreq(Demo.SearchRF, 500))
                    throw new Exception("Error configuring radio frequency");
                if (!channel0.setChannelPeriod(Demo.ChannelPeriod, 500))
                    throw new Exception("Error configuring channel period");
                if (!channel0.openChannel(500))
                    throw new Exception("Error opening channel");
            }
            else
            {
                // Start searching directly for an ANT-FS client matching the search criteria
                // NOTE: If not interested in the payload while in broadcast mode and intend to start a session right away,
                // you can specify useRequestPage = true to search for broadcast devices, and automatically request them
                // to switch to ANT-FS mode
                antfsHost.SearchForDevice(Demo.SearchRF, ConnectRF, DeviceNumber);
            }
            
            Console.WriteLine("Searching for devices...");           

            // Setup a timer, so that we can cancel the search if no device is found
            searchTimer = new Timer(SearchExpired, null, SearchTimeout * 1000, Timeout.Infinite); // convert time to milliseconds
        }
      

        /// <summary>
        /// Handle ANT-FS response events
        /// </summary>
        public void HandleHostResponses(ANTFS_HostChannel.Response response)
        {
            Console.WriteLine(Print.AsString(response));   // Display response
            switch (response)
            {
                case ANTFS_HostChannel.Response.ConnectPass:
                    // A device matching the search criteria was found
                    // Disable the search timeout
                    searchTimer.Change(Timeout.Infinite, Timeout.Infinite);
                    // Obtain and display the device parameters
                    ANTFS_SearchResults foundDevice = antfsHost.GetFoundDeviceParameters();
                    if (foundDevice != null)
                        Console.WriteLine(foundDevice.ToString());
                    else
                        Console.WriteLine("Error obtaining device parameters");
                    break;
                case ANTFS_HostChannel.Response.AuthenticatePass:
                    // The authentication request was accepted
                    // If using Pairing authentication mode, the client will send a Passkey after accepting the request
                    byte[] authString = antfsHost.GetAuthResponse();
                    if (authString.Length > 0)
                    {
                        Console.WriteLine("Received Passkey: Stored for use in next session");
                        clientPassKey = authString;    // Store Passkey for future use
                    }           
                    break;
                case ANTFS_HostChannel.Response.DownloadPass:
                    // Download completed successfully
                    // Retrieve downloaded data
                    byte[] dlData = antfsHost.GetTransferData();
                    if ((dlData.Length > 0))
                    {
                        if (currentDownloadIndex == 0)   // Directory
                        {
                            // Parse downloaded directory
                            Console.WriteLine("Received Directory");
                            dirFS = new ANTFS_Directory(dlData);
                            Console.WriteLine(dirFS.ToString());
                        }
                        else
                        {
                            // Store downloaded file
                            Console.WriteLine("Downloaded file " + currentDownloadIndex + ", Download size: " + antfsHost.GetDownloadSize());
                            if (dlData != null)
                            {
                                File.WriteAllBytes("rawdataout.txt", dlData);
                                Console.WriteLine("Saved to: rawdataout.txt");
                            }
                            else
                            {
                                Console.WriteLine("No data available");
                            }
                        }
                    }
                    else
                    {
                        Console.WriteLine("No data available");
                    }
                    break;
                case ANTFS_HostChannel.Response.DisconnectPass:
                case ANTFS_HostChannel.Response.ConnectionLost:
                case ANTFS_HostChannel.Response.CancelDone:
                    Console.WriteLine("Press any key to exit");
                    demoDone = true;
                    break;
                default:
                    break;
            }
        }

        /// <summary>
        /// Handle ANT channel responses, while in broadcast mode
        /// </summary>
        /// <param name="response">ANT channel event</param>
        public void HandleChannelResponses(ANT_Response response)
        {
            // Make sure we are not processing responses if ANT-FS is active
            if (antfsHost.GetStatus() != ANTFS_HostChannel.State.Idle)
                return;

            if (response.responseID == (byte) ANT_ReferenceLibrary.ANTMessageID.BROADCAST_DATA_0x4E)
            {
                Console.Write("Rx: " + Demo.CursorStrings[cursorIndex++] + "\r");
                cursorIndex &= 3;
                if (response.getDataPayload()[7] >= 40)  // Receive data for about 10 seconds
                {
                    antfsHost.RequestSession(Demo.SearchRF, ConnectRF); // Request ANT-FS session
                }
            }            
        }

        /// <summary>
        /// Cancel the search after the timeout has expired
        /// </summary>
        /// <param name="state">timer param</param>
        private void SearchExpired(Object state)
        {
            lock (this)
            {
                ANTFS_HostChannel.State currentState = antfsHost.GetStatus();
                if (currentState == ANTFS_HostChannel.State.Searching)
                {
                    Console.WriteLine("No devices found...");
                    antfsHost.Cancel();
                }
            }
        }    

        /// <summary>
        /// Display user menu
        /// </summary>
        public void PrintMenu()
        {
            Console.WriteLine(Environment.NewLine);
            Console.WriteLine("M - Print this menu");
            Console.WriteLine("A - Authenticate");
            Console.WriteLine("D - Download");
            Console.WriteLine("U - Upload");
            Console.WriteLine("E - Erase");
            Console.WriteLine("X - Disconnect");
            Console.WriteLine("C - Cancel");
            Console.WriteLine("P - Print directory");
            Console.WriteLine("V - Request version");
            Console.WriteLine("S - Request status");
            Console.WriteLine("Q - Quit");
            Console.WriteLine(Environment.NewLine);
        }

        /// <summary>
        /// Process user input and execute requested commands
        /// </summary>
        /// <param name="command">User command</param>
        public void HandleUserInput(string command)
        {
            try
            {
                switch (command)
                {
                    case "M":
                    case "m":
                        PrintMenu();
                        break;
                    case "A":
                    case "a":
                        // Send authentication request based on user selection
                        // The library will wait for the configured timeout to receive a response to the request
                        Console.WriteLine("Select authentication mode:");
                        Console.WriteLine("1 - Skip authentication");
                        Console.WriteLine("2 - Request pairing");
                        Console.WriteLine("3 - Send passkey");
                        string authChoice = Console.ReadLine();
                        switch (authChoice)
                        {
                            case "1":
                                antfsHost.Authenticate(AuthenticationType.None, 60000); // Passthru authentication. Timeout = 60 sec
                                break;
                            case "2":
                                antfsHost.Authenticate(AuthenticationType.Pairing, HostFriendlyName, 60000); // Request pairing. Timeout = 60 sec
                                break;
                            case "3":
                                if (clientPassKey != null)
                                    antfsHost.Authenticate(AuthenticationType.PassKey, clientPassKey, 60000);   // Use passkey, if we have one. Timeout = 60 sec
                                else
                                    Console.WriteLine("Try pairing first");
                                break;
                            default:
                                Console.WriteLine("Invalid authentication type.");
                                break;
                        }
                        break;
                    case "D":
                    case "d":
                        // Send a download request based on user selection
                        Console.WriteLine("Select the file index to download.");
                        Console.WriteLine("Choose 0 for the directory");
                        string downloadChoice = Console.ReadLine();
                        currentDownloadIndex = Demo.ParseInput(downloadChoice);
                        antfsHost.Download(currentDownloadIndex, 0, 0);   // Start download from the beginning, and do not limit size
                        break;
                    case "U":
                    case "u":
                        // Send an upload request based on user selection
                        Console.WriteLine("Select the file index to upload.");
                        string uploadChoice = Console.ReadLine();
                        byte[] ulData;
                        if (File.Exists("rawdataout.txt"))
                            ulData = File.ReadAllBytes("rawdataout.txt");
                        else
                            ulData = new byte[200];
                        antfsHost.Upload(Demo.ParseInput(uploadChoice), ulData);
                        Console.WriteLine("Uploading: {0} bytes", ulData.Length);
                        break;
                    case "E":
                    case "e":
                        // Send an erase request depending on user selection
                        Console.WriteLine("Select the file index to erase");
                        string eraseChoice = Console.ReadLine();
                        antfsHost.EraseData(Demo.ParseInput(eraseChoice));
                        break;
                    case "X":
                    case "x":
                        // Disconnect from the remote device
                        antfsHost.Disconnect();
                        break;
                    case "C":
                    case "c":
                        // Cancel pending operations
                        antfsHost.Cancel();
                        break;
                    case "P":
                    case "p":
                        if (dirFS != null)
                            Console.WriteLine(dirFS.ToString());
                        else
                            Console.WriteLine("Try requesting the directory first");
                        break;
                    case "V":
                    case "v":
                        Console.WriteLine("ANT-FS Library Version " + antfsHost.GetLibraryVersion());
                        break;
                    case "S":
                    case "s":
                        Console.WriteLine("Status: " + Print.AsString(antfsHost.GetStatus()));
                        break;
                    case "Q":
                    case "q":
                        demoDone = true;
                        break;
                    default:
                        break;
                }
            }
            catch (ANTFS_RequestFailed_Exception antEx)
            {
                // Inform the user that the operation requested failed, and resume operation
                Console.WriteLine(antEx.Message);
            }
            catch (System.ArgumentException argEx)
            {
                // Inform the user about the invalid input, and resume operation
                Console.WriteLine(argEx.Message);
            }
        }

    }
}
