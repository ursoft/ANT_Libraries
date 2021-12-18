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
using ANT_Managed_Library.ANTFS;
using ANT_Managed_Library;

namespace ANTFS_Demo
{
    /// <summary>
    /// This demo shows how to configure a basic ANT-FS client
    /// For a more featured demo, please refer to the source code of the ANT-FS PC tools
    /// </summary>
    class ClientDemo
    {
        // ANT-FS Client Configuration Parameters
        readonly AuthenticationType ClientAuthenticationType = AuthenticationType.None; // Allow pairing, passkey and passthru
        readonly byte BeaconTimeout = 60;   // The client will drop back to link state if no messages from the host are received within this timeout
        readonly byte PairingTimeout = 5;   // Time to wait for the user to provide a response to a pairing request.  Use a short value here since demo accepts pairing automatically
        readonly bool DataAvailable = true; // Indicate in beacon there is data available for download
        readonly bool PairingEnabled = true;    // Indicate in beacon that pairing is enabled
        readonly bool UploadEnabled = false;    // Indicate in beacon that pairing is not supported
        readonly byte[] PassKey = { 1, 2, 3, 1, 2, 3, 1, 2 };   // Authentication passkey
        readonly string ClientFriendlyName = "ConsoleClient";   // ANT-FS Client friendly name
        readonly uint TestFileSize = 512;   // Size of file listed in the directory.  This demo does not 
                                            // access the file system; it just creates a file of the configured size

        ANT_Channel channel0;               // ANT channel to use
        ANTFS_ClientChannel antfsClient;    // ANT-FS client
        ANTFS_Directory dirFS;              // ANT-FS directory
        bool demoDone = false;              // Flag to allow demo to exit
        byte[] txBuffer = {0,0,0,0,0,0,0,0}; // Tx buffer to use while in broadcast mode
        int cursorIndex = 0;                // index to handle cursor display while transmitting in broadcast mode

        /// <summary>
        /// Setup ANT-FS client to process messages from connected device
        /// </summary>
        public void Start(ANT_Device antDevice)
        {
            PrintMenu();
            try
            {
                // Create the ANT-FS client and attach it to a channel (in this case, 0)
                channel0 = antDevice.getChannel(0);
                antfsClient = new ANTFS_ClientChannel(channel0);    
                                
                // Setup callback to handle ANT-FS response events
                antfsClient.OnResponse += new Action<ANTFS_ClientChannel.Response>(HandleClientResponses);                             
                                
                // Configure the client, and begin beaconing               
                ConfigureClient(antDevice);                

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
                Console.ReadLine();
            }
            finally
            {
                Console.WriteLine("Disconnecting module...");
                antDevice.Dispose();    // Close down the device completely and completely shut down all communication 
                antfsClient.Dispose();  // Release all native resources used by the client
                Console.WriteLine("Demo has completed successfully!");
            }
        }

        /// <summary>
        /// Configure the client beacon parameters, and start transmitting/beaconing
        /// </summary>
        /// <param name="antDevice">ANT USB device</param>
        public void ConfigureClient(ANT_Device antDevice)
        {
            // Configure ANT Channel parameters
            antfsClient.SetClientNetworkKey(0, Demo.NetworkKey);
            antfsClient.SetChannelID(Demo.DeviceType, Demo.TransmissionType);
            antfsClient.SetChannelPeriod(Demo.ChannelPeriod);

            // Setup client configuration parameters.
            // ANTFS_ClientParameters is a struct, so make sure to initialize ALL parameters
            ANTFS_ClientParameters clientConfig = new ANTFS_ClientParameters();
            clientConfig.BeaconDeviceType = Demo.ClientDeviceType;
            clientConfig.BeaconManufacturerID = Demo.ClientManufacturerID;
            clientConfig.BeaconRadioFrequency = Demo.SearchRF;
            clientConfig.LinkPeriod = Demo.LinkPeriod;
            clientConfig.AuthenticationType = ClientAuthenticationType;
            clientConfig.IsDataAvailable = DataAvailable;
            clientConfig.IsPairingEnabled = PairingEnabled;
            clientConfig.IsUploadEnabled = UploadEnabled;            
            clientConfig.BeaconTimeout = BeaconTimeout;
            clientConfig.PairingTimeout = PairingTimeout;
            clientConfig.SerialNumber = antDevice.getSerialNumber(); // Use the serial number of the USB stick to identify the client

            // Apply configuration
            antfsClient.Configure(clientConfig);

            // Configure friendly name and passkey (optional)
            antfsClient.SetFriendlyName(ClientFriendlyName);
            antfsClient.SetPassKey(PassKey);
            
            // Create directory, an add a single entry, of the configured size
            dirFS = new ANTFS_Directory();
            ANTFS_Directory.Entry fileEntry;
            fileEntry.FileIndex = 1;
            fileEntry.FileSize = TestFileSize;
            fileEntry.FileDataType = 1;
            fileEntry.FileNumber = 1;
            fileEntry.FileSubType = 0;
            fileEntry.GeneralFlags = (byte) (ANTFS_Directory.GeneralFlags.Read | ANTFS_Directory.GeneralFlags.Write | ANTFS_Directory.GeneralFlags.Erase);
            fileEntry.SpecificFlags = 0;
            fileEntry.TimeStamp = 0; // TODO: Encode the timestamp properly
            dirFS.AddEntry(fileEntry);

            if (Demo.AntfsBroadcast)
            {
                // If we want to use ANT-FS broadcast mode, and start the channel in broadcast mode, 
                // setup callback to handle responses for channel 0 while in broadcast mode
                channel0.channelResponse += new dChannelResponseHandler(HandleChannelResponses);
                
                // Configure the channel as a master, and look for the request message in the channel callback
                // to switch to ANT-FS mode
                if (!antDevice.setNetworkKey(Demo.NetworkNumber, Demo.NetworkKey, 500))
                    throw new Exception("Error configuring network key");
                if (!channel0.assignChannel(ANT_ReferenceLibrary.ChannelType.BASE_Master_Transmit_0x10, Demo.NetworkNumber, 500))
                    throw new Exception("Error assigning channel");
                if (!channel0.setChannelID((ushort)clientConfig.SerialNumber, false, Demo.DeviceType, Demo.TransmissionType, 500))
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
                // If we want to start in ANT-FS mode, just open the beacon
                antfsClient.OpenBeacon();
            }
        }

        /// <summary>
        /// Create a test file, of the specified size
        /// </summary>
        /// <param name="fileSize">File size, in bytes</param>
        /// <returns></returns>
        public byte[] ReadFile(uint fileSize)
        {
            byte[] testFile = new byte[fileSize];
            for (uint i = 0; i < fileSize; i++)
                testFile[i] = (byte)i;
            return testFile;
        }

        /// <summary>
        /// Handle ANT-FS response events
        /// </summary>
        public void HandleClientResponses(ANTFS_ClientChannel.Response response)
        {
            Console.WriteLine(Print.AsString(response));   // Display response
            switch (response)
            {
                case ANTFS_ClientChannel.Response.ConnectionLost:
                case ANTFS_ClientChannel.Response.DisconnectPass:
                    // Close the channel   
                    if (antfsClient.GetDisconnectParameters().CommandType == (byte)DisconnectType.Broadcast)
                    {
                        channel0.closeChannel();
                    }
                    else
                    {
                        antfsClient.CloseBeacon();
                    }
                    break;
                case ANTFS_ClientChannel.Response.BeaconClosed:
                    demoDone = true;
                    Console.WriteLine("Press enter to exit");
                    break;
                case ANTFS_ClientChannel.Response.PairingRequest:
                    Console.WriteLine("Pairing request from " + antfsClient.GetHostName());
                    antfsClient.SendPairingResponse(true);
                    break;
                case ANTFS_ClientChannel.Response.DownloadRequest:
                    try
                    {
                        ushort index = antfsClient.GetRequestedFileIndex();
                        if(index == 0)
                            antfsClient.SendDownloadResponse(DownloadResponse.Ok, index, dirFS.ToByteArray());
                        else
                            antfsClient.SendDownloadResponse(DownloadResponse.Ok, index, ReadFile(dirFS.GetFileSize(index)));
                    }
                    catch(ANTFS_Exception)
                    {
                        antfsClient.SendDownloadResponse(DownloadResponse.InvalidIndex, 0, null);
                    }
                    break;
                default:
                    break;
            }
        }

        /// <summary>
        /// Handle channel responses, while in broadcast mode
        /// </summary>
        /// <param name="response">ANT channel events</param>
        public void HandleChannelResponses(ANT_Response response)
        {
            // Make sure we are not processing responses if ANT-FS is active
            if(antfsClient.GetStatus() != ANTFS_ClientChannel.State.Idle)
                return;

            if ((response.responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.RESPONSE_EVENT_0x40))
            {
                if(response.getChannelEventCode() == ANT_ReferenceLibrary.ANTEventID.EVENT_TX_0x03)
                {
                    channel0.sendBroadcastData(txBuffer);
                    txBuffer[7]++;                
                    Console.Write("Tx: " + Demo.CursorStrings[cursorIndex++] + "\r");
                    cursorIndex &= 3;
                }
                else if(response.getChannelEventCode() == ANT_ReferenceLibrary.ANTEventID.EVENT_CHANNEL_CLOSED_0x07)
                {
                    Console.WriteLine("Channel Closed");
                    Console.WriteLine("Unassigning Channel...");
                    if (channel0.unassignChannel(500))
                    {
                        Console.WriteLine("Unassigned Channel");
                        Console.WriteLine("Press enter to exit");
                        demoDone = true;
                    }
                }
            }
            else if (response.responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.ACKNOWLEDGED_DATA_0x4F)
            {                
                // Look for request page
                if (response.messageContents[1] == 0x46)
                {
                    if (response.messageContents[6] == 0 &&
                        response.messageContents[7] == 0x43 &&
                        response.messageContents[8] == 0x02)
                        Console.WriteLine("Remote device requesting ANT-FS session");
                        antfsClient.OpenBeacon();
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
                    case "V":
                    case "v":
                        Console.WriteLine("ANT-FS Library Version " + ANTFS_ClientChannel.GetLibraryVersion());
                        break;
                    case "S":
                    case "s":
                        Console.WriteLine("Status: " + Print.AsString(antfsClient.GetStatus()));
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
        }
    }
}
