/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/

//////////////////////////////////////////////////////////////////////////
// To use the managed library, you must:
// 1. Import ANT_NET.dll as a reference
// 2. Reference the ANT_Managed_Library namespace
// 3. Include the following files in the working directory of your application:
//  - DSI_CP310xManufacturing_3_1.dll
//  - DSI_SiUSBXp_3_1.dll
//  - ANT_WrappedLib.dll
//  - ANT_NET.dll
//////////////////////////////////////////////////////////////////////////

#define ENABLE_EXTENDED_MESSAGES // Un - coment to enable extended messages

using System;
using System.Runtime.InteropServices;
using System.Text;
using ANT_Managed_Library;
using GregsStack.InputSimulatorStandard;
using GregsStack.InputSimulatorStandard.Native;

namespace ANT_Console_Demo
{
    class demo
    {
        static readonly byte CHANNEL_TYPE_INVALID = 2;

        static readonly byte USER_ANT_CHANNEL = 0;         // ANT Channel to use

        //static readonly ushort USER_DEVICENUM = 20228;    // Device number: HR receiver
        //static readonly byte USER_DEVICETYPE = 120;       // Device type: HR receiver
        //static readonly byte USER_TRANSTYPE = 1;          // Transmission type: HR receiver
        //13685+65536=79221 - из master Edge 1030+
        //D00001307_-_ANT+_Device_Profile_-_Controls_-_2.0.pdf
        //1_tx. Received BROADCAST_DATA_0x4E :: 4e, 00-02-00-FF-00-00-00-00-10
        //  00 - channel#
        //  02 – Control Device Availability Page#
        //  00 - specific device notifications (потом будет 0х80: connection limit reached)
        //  FF-00-00-00-00 - reserved (почему 1-й не 0 ?)
        //  0x10 - Device Capabilities: ничего, кроме Keypad control
        //1_rx. Ответ пульта: Chan ID(13685,16,21) - Acked Rx:(0): 49-0D-56-01-00-01-FF-FF 
        //  0x49(73) - Generic Command Data Page
        //  0x0001560d=87565 - edge remote ID & vendor (Garmin=0001)
        //  0x01 - seq#
        //  0xFFFF - command number "no command"

        /* button command numbers: 9.4.8
        49-0D-56-01-00-02-24-00 - quick lap press (lap = 36 dec). Длинного и повтора нет.
        49-0D-56-01-00-0E-00-80 - quick blue press (custom command 32768+0).
        49-0D-56-01-00-0E-01-80 - long blue press (custom command 32768+1). Повтора нет.
        49-0D-56-01-00-14-01-00 - quick screen press (menu down = 1 dec).
        49-0D-56-01-00-14-00-00 - long screen press (menu up = 0 dec). Можно зажать, будет повторяться периодически.
        */
        /* еще получали от 1030-го Chan ID(13685,16,21) - 
        Rx:(0): 50-FF-FF-05-01-00-F2-0D
                0x50(80) – Manufacturer’s Identification common page
                0xFFFF - reserved
                5 - HW Revision
                0001 - garmin vendor id
                0xdf2 - model number (д.б. 1030+, но не совпадает)

        Rx:(0): 52-FF-FF-4E-2D-00-C9-B3 
                0x52(82) - battery status

                46-0D-56-FF-FF-02-50-03 
                0x46(70) – Request Data Page
                0x560D - edge remote ID 
                0xFFFF - invalid descriptor
                0x02 - повтори 2 раза (зачем ?)
                0x50(80) - номер запрашиваемой страницы
                0x03 - запрашиваю у Slave
        */
        static readonly ushort USER_DEVICENUM = 13685;      // Device number: Edge Remote controllable device (нужно установить тот же номер 1030-го, с которым Remote спарился до этого)
        static readonly byte USER_DEVICETYPE = 16;          // Device type: control
        static readonly byte USER_TRANSTYPE = 5+16;         // Transmission type: Remote controllable device 

        static readonly byte USER_RADIOFREQ = 57;          // RF Frequency + 2400 MHz
        static readonly ushort USER_CHANNELPERIOD = 8192;  // Channel Period (8192/32768)s period = 4Hz
        
        static readonly byte[] USER_NETWORK_KEY = { 0xB9, 0xA5, 0x21, 0xFB, 0xBD, 0x72, 0xC3, 0x45 };
        static readonly byte USER_NETWORK_NUM = 0;         // The network key is assigned to this network number

        static ANT_Device device0;
        static ANT_Channel channel0;
        static ANT_ReferenceLibrary.ChannelType channelType;
        static byte[] txBuffer = { 2, 0, 0xff, 0, 0, 0, 0, 0x10 };
        static bool bDone;
        static bool bDisplay;
        static bool bBroadcasting;
        static int iIndex = 0;

        ////////////////////////////////////////////////////////////////////////////////
        // Main
        //
        // Usage:
        //
        // c:\demo_net.exe [channel_type]
        //
        // ... where
        // channel_type:  Master = 0, Slave = 1
        //
        // ... example
        //
        // c:\demo_net.exe 0
        // 
        // will connect to an ANT USB stick open a Master channel
        //
        // If optional arguements are not supplied, user will 
        // be prompted to enter these after the program starts
        //
        ////////////////////////////////////////////////////////////////////////////////
        static void Main(string[] args)
        {
            //byte ucChannelType = 1;//we are slave //HR receiver or edge remote simulator CHANNEL_TYPE_INVALID;
            byte ucChannelType = 0; //we are master controllable by edge remote - 1030 simulator

            if (args.Length > 0)
            {
                ucChannelType = byte.Parse(args[0]);
            }

            try
            {
                Init();
                Start(ucChannelType);
             }
            catch (Exception ex)
            {
                Console.WriteLine("Demo failed with exception: \n" + ex.Message);
            }
        }

        ////////////////////////////////////////////////////////////////////////////////
        // Init
        //
        // Initialize demo parameters.
        //
        ////////////////////////////////////////////////////////////////////////////////
        static void Init()
        {
            try
            {
                Console.WriteLine("Attempting to connect to an ANT USB device...");
                device0 = new ANT_Device();   // Create a device instance using the automatic constructor (automatic detection of USB device number and baud rate)
                device0.deviceResponse += new ANT_Device.dDeviceResponseHandler(DeviceResponse);    // Add device response function to receive protocol event messages

                channel0 = device0.getChannel(USER_ANT_CHANNEL);    // Get channel from ANT device
                channel0.channelResponse += new dChannelResponseHandler(ChannelResponse);  // Add channel response function to receive channel event messages
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

        ////////////////////////////////////////////////////////////////////////////////
        // Start
        //
        // Start the demo program.
        // 
        // ucChannelType_:  ANT Channel Type. 0 = Master, 1 = Slave
        //                  If not specified, 2 is passed in as invalid.
        ////////////////////////////////////////////////////////////////////////////////
        static void Start(byte ucChannelType_)
        {
            byte ucChannelType = ucChannelType_;
            bDone = false;
            bDisplay = true;
            bBroadcasting = false;

            PrintMenu();

            // If a channel type has not been set at the command line,
            // prompt the user to specify one now
            do
            {
                if (ucChannelType == CHANNEL_TYPE_INVALID)
                {
                    Console.WriteLine("Channel Type? (Master = 0, Slave = 1)");
                    try
                    {
                        ucChannelType = byte.Parse(Console.ReadLine());
                    }
                    catch (Exception)
                    {
                        ucChannelType = CHANNEL_TYPE_INVALID;
                    }
                }

                if (ucChannelType == 0)
                {
                    channelType = ANT_ReferenceLibrary.ChannelType.BASE_Master_Transmit_0x10;
                }
                else if (ucChannelType == 1)
                {
                    channelType = ANT_ReferenceLibrary.ChannelType.BASE_Slave_Receive_0x00;
                }
                else
                {
                    ucChannelType = CHANNEL_TYPE_INVALID;
                    Console.WriteLine("Error: Invalid channel type");
                }
            } while (ucChannelType == CHANNEL_TYPE_INVALID);

            try
            {
                ConfigureANT();

                while (!bDone)
                {
                    string command = Console.ReadLine();
                    switch (command)
                    {
                        case "e": case "E":
                            { 
                            }
                            break;
                        case "M":
                        case "m":
                        {
                            PrintMenu();
                            break;
                        }
                        case "Q":
                        case "q":
                        {
                            // Quit
                            Console.WriteLine("Closing Channel");
                            bBroadcasting = false;
                            channel0.closeChannel();
                            break;
                        }
                        case "A":
                        case "a":
                        {
                            // Send Acknowledged Data
                            byte[] myTxBuffer = { 1, 2, 3, 4, 5, 6, 7, 8 };
                            channel0.sendAcknowledgedData(myTxBuffer);
                            break;
                        }
                        case "B":
                        case "b":
                        {
                            // Send Burst Data (10 packets)
                            byte[] myTxBuffer = new byte[8 * 10];
                            for (byte i = 0; i < 8 * 10; i++)
                                myTxBuffer[i] = i;
                            channel0.sendBurstTransfer(myTxBuffer);
                            break;
                        }

                        case "R":
                        case "r":
                        {
                            // Reset the system and start over the test
                            ConfigureANT();
                            break;
                        }

                        case "C":
                        case "c":
                        {
                            // Request capabilities
                            ANT_DeviceCapabilities devCapab = device0.getDeviceCapabilities(500);
                            Console.Write(devCapab.printCapabilities() + Environment.NewLine);
                            break;
                        }
                        case "V":
                        case "v":
                        {
                            // Request version
                            // As this is not available in all ANT parts, we should not wait for a response, so
                            // we do not specify a timeout
                            // The response - if available - will be processed in DeviceResponse
                            device0.requestMessage(ANT_ReferenceLibrary.RequestMessageID.VERSION_0x3E);                    
                            break;
                        }
                        case "S":
                        case "s":
                        {
                            // Request channel status
                            ANT_ChannelStatus chStatus = channel0.requestStatus(500);

                            string[] allStatus = { "STATUS_UNASSIGNED_CHANNEL",
                                                    "STATUS_ASSIGNED_CHANNEL",
                                                    "STATUS_SEARCHING_CHANNEL",
                                                    "STATUS_TRACKING_CHANNEL"};
                            Console.WriteLine("STATUS: " + allStatus[(int)chStatus.BasicStatus]);
                            break;
                        }
                        case "I":
                        case "i":
                        {
                            // Request channel ID
                            ANT_Response respChID = device0.requestMessageAndResponse(ANT_ReferenceLibrary.RequestMessageID.CHANNEL_ID_0x51, 500);
                            ushort usDeviceNumber = (ushort) ((respChID.messageContents[2] << 8) + respChID.messageContents[1]);
                            byte ucDeviceType = respChID.messageContents[3];
                            byte ucTransmissionType = respChID.messageContents[4];
                            Console.WriteLine("CHANNEL ID: (" + usDeviceNumber.ToString() + "," + ucDeviceType.ToString() + "," + ucTransmissionType.ToString() + ")");
                            break;
                        }
                        case "D":
                        case "d":
                        {
                            bDisplay = !bDisplay;
                            break;
                        }
			            case "U":
			            case "u":
		                {
				            // Print out information about the device we are connected to
				            Console.WriteLine("USB Device Description");
				            
                            // Retrieve info
                            Console.WriteLine(String.Format("   VID: 0x{0:x}", device0.getDeviceUSBVID()));
                            Console.WriteLine(String.Format("   PID: 0x{0:x}", device0.getDeviceUSBPID()));
                            Console.WriteLine(String.Format("   Product Description: {0}", device0.getDeviceUSBInfo().printProductDescription()));
                            Console.WriteLine(String.Format("   Serial String: {0}", device0.getDeviceUSBInfo().printSerialString()));	
				            break;
                        }
                        default:
                        {
                            break;
                        }
                    }
                    System.Threading.Thread.Sleep(0);
                }
                // Clean up ANT
                Console.WriteLine("Disconnecting module...");
                ANT_Device.shutdownDeviceInstance(ref device0);  // Close down the device completely and completely shut down all communication
                Console.WriteLine("Demo has completed successfully!");
                return;
            }
            catch (Exception ex)
            {
                throw new Exception("Demo failed: " + ex.Message + Environment.NewLine);
            }
        }

        ////////////////////////////////////////////////////////////////////////////////
        // ConfigureANT
        //
        // Resets the system, configures the ANT channel and starts the demo
        ////////////////////////////////////////////////////////////////////////////////
        private static void ConfigureANT()
        {
            Console.WriteLine("Resetting module...");
            device0.ResetSystem();     // Soft reset
            System.Threading.Thread.Sleep(500);    // Delay 500ms after a reset

            // If you call the setup functions specifying a wait time, you can check the return value for success or failure of the command
            // This function is blocking - the thread will be blocked while waiting for a response.
            // 500ms is usually a safe value to ensure you wait long enough for any response
            // If you do not specify a wait time, the command is simply sent, and you have to monitor the protocol events for the response,
            Console.WriteLine("Setting network key...");
            if (device0.setNetworkKey(USER_NETWORK_NUM, USER_NETWORK_KEY, 500))
                Console.WriteLine("Network key set");
            else
                throw new Exception("Error configuring network key");

            Console.WriteLine("Assigning channel...");
            if (channel0.assignChannel(channelType, USER_NETWORK_NUM, 500))
                Console.WriteLine("Channel assigned");
            else
                throw new Exception("Error assigning channel");

            Console.WriteLine("Setting Channel ID...");
            if (channel0.setChannelID(USER_DEVICENUM, false, USER_DEVICETYPE, USER_TRANSTYPE, 500))  // Not using pairing bit
                Console.WriteLine("Channel ID set");
            else
                throw new Exception("Error configuring Channel ID");

            Console.WriteLine("Setting Radio Frequency...");
            if (channel0.setChannelFreq(USER_RADIOFREQ, 500))
                Console.WriteLine("Radio Frequency set");
            else
                throw new Exception("Error configuring Radio Frequency");

            Console.WriteLine("Setting Channel Period...");
            if (channel0.setChannelPeriod(USER_CHANNELPERIOD, 500))
                Console.WriteLine("Channel Period set");
            else 
                throw new Exception("Error configuring Channel Period");

            Console.WriteLine("Opening channel...");
            bBroadcasting = true;
            if (channel0.openChannel(500))
            {
                Console.WriteLine("Channel opened");
            }
            else
            {
                bBroadcasting = false;
                throw new Exception("Error opening channel");
            }

#if (ENABLE_EXTENDED_MESSAGES)
            // Extended messages are not supported in all ANT devices, so
            // we will not wait for the response here, and instead will monitor 
            // the protocol events
            Console.WriteLine("Enabling extended messages...");
            device0.enableRxExtendedMessages(true);
#endif
        }

        ////////////////////////////////////////////////////////////////////////////////
        // ChannelResponse
        //
        // Called whenever a channel event is recieved. 
        // 
        // response: ANT message
        ////////////////////////////////////////////////////////////////////////////////
        static void ChannelResponse(ANT_Response response)
        {
            try
            {
                switch ((ANT_ReferenceLibrary.ANTMessageID)response.responseID)
                {
                    case ANT_ReferenceLibrary.ANTMessageID.RESPONSE_EVENT_0x40:
                    {
                        switch (response.getChannelEventCode())
                        {
                            // This event indicates that a message has just been
                            // sent over the air. We take advantage of this event to set
                            // up the data for the next message period.   
                            case ANT_ReferenceLibrary.ANTEventID.EVENT_TX_0x03:
                            {
                                //txBuffer[0]++;  // Increment the first byte of the buffer

                                // Broadcast data will be sent over the air on
                                // the next message period
                                if (bBroadcasting)
                                {
                                    channel0.sendBroadcastData(txBuffer);
                                    
                                    if (bDisplay)
                                    {
                                        // Echo what the data will be over the air on the next message period
                                        if (Console.CursorSize >= 96) Console.CursorSize = 20;
                                        else Console.CursorSize += 5;
                                        //Console.WriteLine("Tx: (" + response.antChannel.ToString() + ")" + BitConverter.ToString(txBuffer));
                                    }
                                }
                                else
                                {
                                    string[] ac = { "|", "/", "_", "\\" };
                                    Console.Write("Tx: " + ac[iIndex++] + "\r");
                                    iIndex &= 3;
                                }
                                break;
                            }
                            case ANT_ReferenceLibrary.ANTEventID.EVENT_RX_SEARCH_TIMEOUT_0x01:
                            {
                                Console.WriteLine("Search Timeout");
                                break;
                            }
                            case ANT_ReferenceLibrary.ANTEventID.EVENT_RX_FAIL_0x02:
                            {
                                Console.WriteLine("Rx Fail");
                                break;
                            }
                            case ANT_ReferenceLibrary.ANTEventID.EVENT_TRANSFER_RX_FAILED_0x04:
                            {
                                Console.WriteLine("Burst receive has failed");
                                break;
                            }
                            case ANT_ReferenceLibrary.ANTEventID.EVENT_TRANSFER_TX_COMPLETED_0x05:
                            {
                                Console.WriteLine("Transfer Completed");
                                break;
                            }
                            case ANT_ReferenceLibrary.ANTEventID.EVENT_TRANSFER_TX_FAILED_0x06:
                            {
                                Console.WriteLine("Transfer Failed");
                                break;
                            }
                            case ANT_ReferenceLibrary.ANTEventID.EVENT_CHANNEL_CLOSED_0x07:
                            {
                                // This event should be used to determine that the channel is closed.
                                Console.WriteLine("Channel Closed");
                                Console.WriteLine("Unassigning Channel...");
                                if (channel0.unassignChannel(500))
                                {
                                    Console.WriteLine("Unassigned Channel");
                                    Console.WriteLine("Press enter to exit");
                                    bDone = true;
                                }
                                break;
                            }
                            case ANT_ReferenceLibrary.ANTEventID.EVENT_RX_FAIL_GO_TO_SEARCH_0x08:
                            {
                                Console.WriteLine("Go to Search");
                                break;
                            }
                            case ANT_ReferenceLibrary.ANTEventID.EVENT_CHANNEL_COLLISION_0x09:
                            {
                                Console.WriteLine("Channel Collision");
                                break;
                            }
                            case ANT_ReferenceLibrary.ANTEventID.EVENT_TRANSFER_TX_START_0x0A:
                            {
                                Console.WriteLine("Burst Started");
                                break;
                            }
                            default:
                            {
                                Console.WriteLine("Unhandled Channel Event " + response.getChannelEventCode());
                                break;
                            }
                        }
                        break;
                    }
                    case ANT_ReferenceLibrary.ANTMessageID.BROADCAST_DATA_0x4E:
                    case ANT_ReferenceLibrary.ANTMessageID.ACKNOWLEDGED_DATA_0x4F:
                    case ANT_ReferenceLibrary.ANTMessageID.BURST_DATA_0x50:
                    case ANT_ReferenceLibrary.ANTMessageID.EXT_BROADCAST_DATA_0x5D:
                    case ANT_ReferenceLibrary.ANTMessageID.EXT_ACKNOWLEDGED_DATA_0x5E:
                    case ANT_ReferenceLibrary.ANTMessageID.EXT_BURST_DATA_0x5F:
                    {
                        if (bDisplay)
                        {
                            StringBuilder stringBuilder = new StringBuilder();
                            byte[] payload = response.getDataPayload();
                            if (payload.Length == 8 && payload[0] == 0x49) {
                                /*
                                        49-0D-56-01-00-02-24-00 - quick lap press (lap = 36 dec). Длинного и повтора нет.
                                        49-0D-56-01-00-0E-00-80 - quick blue press (custom command 32768+0).
                                        49-0D-56-01-00-0E-01-80 - long blue press (custom command 32768+1). Повтора нет.
                                        49-0D-56-01-00-14-01-00 - quick screen press (menu down = 1 dec).
                                        49-0D-56-01-00-14-00-00 - long screen press (menu up = 0 dec). Можно зажать, будет повторяться периодически.
                                */
                                switch (payload[6] + 256 * payload[7]) {
                                case 0:
                                    Console.WriteLine("Screen: long press");
                                    EmitZwiftKeyPress(VirtualKeyCode.RETURN);
                                    return;
                                case 1:
                                    Console.WriteLine("Screen: quick press");
                                    EmitZwiftKeyPress(VirtualKeyCode.RIGHT);
                                    return;
                                case 32768:
                                    Console.WriteLine("Blue: quick press");
                                    EmitZwiftKeyPress(VirtualKeyCode.LEFT);
                                    return;
                                case 32769:
                                    Console.WriteLine("Blue: long press");
                                    EmitZwiftKeyPress(VirtualKeyCode.ESCAPE);
                                    return;
                                case 36:
                                    Console.WriteLine("Lap: quick press");
                                    EmitZwiftKeyPress(VirtualKeyCode.UP);
                                    return;
                                default:
                                    break;
                                }
                            }
                            if (response.isExtended()) // Check if we are dealing with an extended message
                            {
                                ANT_ChannelID chID = response.getDeviceIDfromExt();    // Channel ID of the device we just received a message from
                                stringBuilder.AppendFormat($"Chan ID({chID.deviceNumber},{chID.deviceTypeID},{chID.transmissionTypeID}) - ");
                            }
                            if (response.responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.BROADCAST_DATA_0x4E
                                || response.responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.EXT_BROADCAST_DATA_0x5D)
                                stringBuilder.AppendFormat($"Rx:({response.antChannel}): ");
                            else if (response.responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.ACKNOWLEDGED_DATA_0x4F
                                || response.responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.EXT_ACKNOWLEDGED_DATA_0x5E)
                                stringBuilder.AppendFormat($"Acked Rx:({response.antChannel}): ");
                            else
                                stringBuilder.AppendFormat($"Burst({response.getBurstSequenceNumber():X2}) Rx:({response.antChannel}): ");

                            stringBuilder.Append(BitConverter.ToString(payload));
                            Console.WriteLine(stringBuilder);
                        }
                        else
                        {
                            string[] ac = { "|", "/", "_", "\\" };
                            Console.Write("Rx: " + ac[iIndex++] + "\r");
                            iIndex &= 3;
                        }
                        break;
                    }
                    default:
                    {
                        Console.WriteLine("Unknown Message " + response.responseID);
                        break;
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("Channel response processing failed with exception: " + ex.Message);
            }
        }

        [DllImport("USER32.DLL", CharSet = CharSet.Unicode)]
        public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);

        [DllImport("USER32.DLL")]
        public static extern bool SetForegroundWindow(IntPtr hWnd);

        static InputSimulator sim = new InputSimulator();
        private static void EmitZwiftKeyPress(VirtualKeyCode key)
        {
            IntPtr zwiftWindow = FindWindow(null, "Zwift");

            if (SetForegroundWindow(zwiftWindow)) {
                sim.Keyboard.KeyPress(key);
            }
        }

        ////////////////////////////////////////////////////////////////////////////////
        // DeviceResponse
        //
        // Called whenever a message is received from ANT unless that message is a 
        // channel event message. 
        // 
        // response: ANT message
        ////////////////////////////////////////////////////////////////////////////////
        static void DeviceResponse(ANT_Response response)
        {
            switch ((ANT_ReferenceLibrary.ANTMessageID) response.responseID)
            {
                case ANT_ReferenceLibrary.ANTMessageID.STARTUP_MESG_0x6F:
                {
                    Console.Write("RESET Complete, reason: ");

                    byte ucReason = response.messageContents[0];

                    if(ucReason == (byte) ANT_ReferenceLibrary.StartupMessage.RESET_POR_0x00)
                        Console.WriteLine("RESET_POR");
                    if(ucReason == (byte) ANT_ReferenceLibrary.StartupMessage.RESET_RST_0x01)
                        Console.WriteLine("RESET_RST");
                    if(ucReason == (byte) ANT_ReferenceLibrary.StartupMessage.RESET_WDT_0x02)
                        Console.WriteLine("RESET_WDT");
                    if(ucReason == (byte) ANT_ReferenceLibrary.StartupMessage.RESET_CMD_0x20)
                        Console.WriteLine("RESET_CMD");
                    if(ucReason == (byte) ANT_ReferenceLibrary.StartupMessage.RESET_SYNC_0x40)
                        Console.WriteLine("RESET_SYNC");
                    if(ucReason == (byte) ANT_ReferenceLibrary.StartupMessage.RESET_SUSPEND_0x80)
                        Console.WriteLine("RESET_SUSPEND");
                    break;
                }
                case ANT_ReferenceLibrary.ANTMessageID.VERSION_0x3E:
                {
                    Console.WriteLine("VERSION: " + new ASCIIEncoding().GetString(response.messageContents));
                    break;
                }
                case ANT_ReferenceLibrary.ANTMessageID.RESPONSE_EVENT_0x40:
                {
                    switch (response.getMessageID())
                    {
                        case ANT_ReferenceLibrary.ANTMessageID.CLOSE_CHANNEL_0x4C:
                        {
                            if (response.getChannelEventCode() == ANT_ReferenceLibrary.ANTEventID.CHANNEL_IN_WRONG_STATE_0x15)
                            {
                                Console.WriteLine("Channel is already closed");
                                Console.WriteLine("Unassigning Channel...");
                                if (channel0.unassignChannel(500))
                                {
                                    Console.WriteLine("Unassigned Channel");
                                    Console.WriteLine("Press enter to exit");
                                    bDone = true;
                                }
                            }
                            break;
                        }
                        case ANT_ReferenceLibrary.ANTMessageID.NETWORK_KEY_0x46:
                        case ANT_ReferenceLibrary.ANTMessageID.ASSIGN_CHANNEL_0x42:
                        case ANT_ReferenceLibrary.ANTMessageID.CHANNEL_ID_0x51:
                        case ANT_ReferenceLibrary.ANTMessageID.CHANNEL_RADIO_FREQ_0x45:
                        case ANT_ReferenceLibrary.ANTMessageID.CHANNEL_MESG_PERIOD_0x43:
                        case ANT_ReferenceLibrary.ANTMessageID.OPEN_CHANNEL_0x4B:
                        case ANT_ReferenceLibrary.ANTMessageID.UNASSIGN_CHANNEL_0x41:
                        {
                            if (response.getChannelEventCode() != ANT_ReferenceLibrary.ANTEventID.RESPONSE_NO_ERROR_0x00)
                            {
                                Console.WriteLine(String.Format("Error {0} configuring {1}", response.getChannelEventCode(), response.getMessageID()));
                            }
                            break;
                        }
                        case ANT_ReferenceLibrary.ANTMessageID.RX_EXT_MESGS_ENABLE_0x66:
                        {
                            if (response.getChannelEventCode() == ANT_ReferenceLibrary.ANTEventID.INVALID_MESSAGE_0x28)
                            {
                                Console.WriteLine("Extended messages not supported in this ANT product");
                                break;
                            }
                            else if(response.getChannelEventCode() != ANT_ReferenceLibrary.ANTEventID.RESPONSE_NO_ERROR_0x00)
                            {
                                Console.WriteLine(String.Format("Error {0} configuring {1}", response.getChannelEventCode(), response.getMessageID()));
                                break;
                            }
                            Console.WriteLine("Extended messages enabled");
                            break;
                        }
                        case ANT_ReferenceLibrary.ANTMessageID.REQUEST_0x4D:
                        {
                            if (response.getChannelEventCode() == ANT_ReferenceLibrary.ANTEventID.INVALID_MESSAGE_0x28)
                            {
                                Console.WriteLine("Requested message not supported in this ANT product");
                                break;
                            }
                            break;
                        }
                        default:
                        {
                            Console.WriteLine("Unhandled response " + response.getChannelEventCode() + " to message " + response.getMessageID());                            break;
                        }
                    }
                    break;
                }
            }
        }

        ////////////////////////////////////////////////////////////////////////////////
        // PrintMenu
        //
        // Display demo menu
        // 
        ////////////////////////////////////////////////////////////////////////////////
        static void PrintMenu()
        {
            // Print out options  
            Console.WriteLine("M - Print this menu");
            Console.WriteLine("A - Send Acknowledged message");
            Console.WriteLine("B - Send Burst message");
            Console.WriteLine("R - Reset");
            Console.WriteLine("C - Request Capabilites");
            Console.WriteLine("V - Request Version");
            Console.WriteLine("I - Request Channel ID");
            Console.WriteLine("S - Request Status");
	        Console.WriteLine("U - Request USB Descriptor");
            Console.WriteLine("D - Toggle Display");
            Console.WriteLine("Q - Quit");
        }

    }
}
