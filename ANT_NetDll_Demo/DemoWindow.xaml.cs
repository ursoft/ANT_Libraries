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
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using ANT_Managed_Library;      //Reference the ANT_Managed_Library namespace to make the code easier and more readable


/*
 * Setup instructions for creating .NET application using the managed library:
 * To use the library you must first import ANT_NET.dll as a reference.
 * Then, three additional libraries must also be present in the directory:
 * DSI_CP210xManufacturing_3_1.dll, DSI_SiUSBXp_3_1.dll, and the ANT_WrappedLib.dll
 *
 * That's it! Now start coding. Below is a sample application using the
 * managed library. The comments explain the library's features and some of the
 * basic functions and their usage.
 *
 */

namespace ANT_NetDll_Demo
{
    public partial class DemoWindow : Window
    {
        //This delegate is for using the dispatcher to avoid conflicts of the feedback thread referencing the UI elements
        delegate void dAppendText(String toAppend);

        /*
         * The ANT_Managed_Library has been designed so that if you have a valid reference
         * to an ANTDevice object it is a valid reference to a physical ANT device connected
         * to your computer. This can be broken by uncontrollable events such as physically
         * unplugging the device, but otherwise each ANTDevice object will always represent a
         * physical ANT device connected to your computer.
         *
         * The default behaviour is the device is reset when an instance is created so every time
         * you create a device you will have a fresh instance, unless you disable the automatic
         * reset in the static ANT_Common class.
         *
         * The ANT_Common class also contains the controls for managing debug logs, which are
         * pretty self-explanatory.
         *
         */
        ANT_Device device0;
        ANT_Device device1;

        public DemoWindow()
        {
            InitializeComponent();
        }

        //Calls the startup or shutdown function, and changes the UI accordingly
        private void button_Start_Click(object sender, RoutedEventArgs e)
        {
            if (button_Start.Content.Equals("Start Demo"))
            {
                if (startUp())
                {
                    comboBox_demoMode.IsEnabled = false;
                    border_DemoControls.IsEnabled = true;
                    button_DoAction.IsEnabled = true;
                    button_Start.Background = System.Windows.Media.Brushes.Salmon;
                    button_Start.Content = "Stop Demo";
                    return;
                }
            }
            //If we get here it means we failed startup or are stopping the demo
            shutdown();
            comboBox_demoMode.IsEnabled = true;
            button_DoAction.IsEnabled = false;
            button_Start.Background = System.Windows.Media.Brushes.LightGreen;
            button_Start.Content = "Start Demo";
        }

        //Creates the ANTDevice instances and calls the setupAndOpen routine according to the selected demo mode
        private bool startUp()
        {
            //The managed library will throw ANTExceptions on errors
            //We run this in a try catch because we want to print any errors instead of crash
            try
            {
                //Regardless of selection we need to connect to the first device
                //The library has an automatic constructor to automatically connect to the first available device
                //You can still manually choose which device to connect to by using the parameter constructor,
                // ie: ANTDeviceInstance = new ANTDevice(0, 57600)
                device0 = new ANT_Device();

                //First we want to setup the response functions so we can see the feedback as we setup
                //To do this, the device and each channel have response events which are fired when feedback
                //is received from the device, including command acknowledgements and transmission events.
                device0.deviceResponse += new ANT_Device.dDeviceResponseHandler(device0_deviceResponse);
                device0.getChannel(0).channelResponse += new dChannelResponseHandler(d0channel0_channelResponse);
                textBox_device0.Text = "Device 0 Connected" + Environment.NewLine;

                //Now handle the rest of the setup based on what mode we are in
                switch (comboBox_demoMode.SelectedIndex)
                {
                    case 0: //Dev 0 - Master, Dev1 - Slave
                        textBox_Display.Text = "Starting Mode: d0-Master, d1-Slave" + Environment.NewLine;

                        //In this library we can connect to multiple devices by creating a new instance of an ANTDevice
                        //So, in this mode we need a second device
                        try
                        {
                            device1 = new ANT_Device();
                        }
                        catch (Exception ex)    //Forward the exception with some more info
                        {
                            throw new Exception("Can not connect to second device, " + ex.Message);
                        }
                        textBox_device1.Text = "Device 1 Connected" + Environment.NewLine;
                        textBox_device1.IsEnabled = true;

                        //We need to assign the device1 response function now too
                        device1.deviceResponse += new ANT_Device.dDeviceResponseHandler(device1_deviceResponse);
                        device1.getChannel(0).channelResponse += new dChannelResponseHandler(d1channel0_channelResponse);

                        //To be a little clearer we handle the setup in a common method which takes the device instance
                        //and the channel type enum
                        setupAndOpen(device0, ANT_ReferenceLibrary.ChannelType.BASE_Master_Transmit_0x10);
                        setupAndOpen(device1, ANT_ReferenceLibrary.ChannelType.BASE_Slave_Receive_0x00);
                        break;


                    case 1: //Dev 0 Only - Master
                        textBox_Display.Text = "Starting Mode: d0 only - Master" + Environment.NewLine;
                        textBox_device1.Text = "Device 1 Not Used" + Environment.NewLine;
                        textBox_device1.IsEnabled = false;
                        setupAndOpen(device0, ANT_ReferenceLibrary.ChannelType.BASE_Master_Transmit_0x10);
                        break;

                    case 2: //Dev 0 Only - Slave
                        textBox_Display.Text = "Starting Mode: d0 only - Slave" + Environment.NewLine;
                        textBox_device1.Text = "Device 1 Not Used" + Environment.NewLine;
                        textBox_device1.IsEnabled = false;
                        setupAndOpen(device0, ANT_ReferenceLibrary.ChannelType.BASE_Slave_Receive_0x00);
                        break;
                }
            }
            catch (Exception ex)
            {
                textBox_Display.AppendText("Error: " + ex.Message + Environment.NewLine);
                if (device0 == null)    //We print another message if we didn't connect to any device to be a little more helpful
                    textBox_Display.AppendText("Could not connect to any devices, ensure an ANT device is connected to your system and try again." + Environment.NewLine);
                textBox_Display.AppendText(Environment.NewLine);
                return false;
            }
            return true;
        }

        private void setupAndOpen(ANT_Device deviceToSetup, ANT_ReferenceLibrary.ChannelType channelType)
        {
            //We try-catch and forward exceptions to the calling function to handle and pass the errors to the user
            try
            {
                //To access an ANTChannel on a paticular device we need to get the channel from the device
                //Once again, this ensures you have a valid object associated with a real-world ANTChannel
                //ie: You can only get channels that actually exist
                ANT_Channel channel0 = deviceToSetup.getChannel(0);

                //Almost all functions in the library have two overloads, one with a response wait time and one without
                //If you give a wait time, you can check the return value for success or failure of the command, however
                //the wait time version is blocking. 500ms is usually a safe value to ensure you wait long enough for any response.
                //But with no wait time, the command is simply sent and you have to monitor the device response for success or failure.

                //To setup channels for communication there are three mandatory operations assign, setID, and Open
                //Various other settings such as message period and network key affect communication
                //between two channels as well, see the documentation for further details on these functions.

                //So, first we assign the channel, we have already been passed the channelType which is an enum that has various flags
                //If we were doing something more advanced we could use a bitwise or ie:base|adv1|adv2 here too
                //We also use net 0 which has the public network key by default
                if (channel0.assignChannel(channelType, 0, 500))
                    threadSafePrintLine("Ch assigned to " + channelType + " on net 0.", textBox_Display);
                else
                    throw new Exception("Channel assignment operation failed.");

                //Next we have to set the channel id. Slaves will only communicate with a master device that
                //has the same id unless one or more of the id parameters are set to a wild card 0. If wild cards are included
                //the slave will search until it finds a broadcast that matches all the non-wild card parameters in the id.
                //For now we pick an arbitrary id so that we can ensure we match between the two devices.
                //The pairing bit ensures on a search that you only pair with devices that also are requesting
                //pairing, but we don't need it here so we set it to false
                if(channel0.setChannelID(12345, false, 67, 89, 500))
                    threadSafePrintLine("Set channel ID to 12345, 67, 89", textBox_Display);
                else
                    throw new Exception("Set Channel ID operation failed.");

                //Setting the channel period isn't mandatory, but we set it slower than the default period so messages aren't coming so fast
                //The period parameter is divided by 32768 to set the period of a message in seconds. So here, 16384/32768 = 1/2 sec/msg = 2Hz
                if(channel0.setChannelPeriod(16384, 500))
                    threadSafePrintLine("Message Period set to 16384/32768 seconds per message", textBox_Display);

                //Now we open the channel
                if(channel0.openChannel(500))
                    threadSafePrintLine("Opened Channel" + Environment.NewLine, textBox_Display);
                else
                    throw new Exception("Channel Open operation failed.");
            }
            catch (Exception ex)
            {
                throw new Exception("Setup and Open Failed. " + ex.Message + Environment.NewLine);
            }
        }

        //Print the device response to the textbox
        void device0_deviceResponse(ANT_Response response)
        {
            threadSafePrintLine(decodeDeviceFeedback(response), textBox_device0);
        }

        //Print the channel response to the textbox
        void d0channel0_channelResponse(ANT_Response response)
        {
            threadSafePrintLine(decodeChannelFeedback(response), textBox_device0);
        }

        //Print the device response to the textbox
        void device1_deviceResponse(ANT_Response response)
        {
            threadSafePrintLine(decodeDeviceFeedback(response), textBox_device1);
        }

        //Print the channel response to the textbox
        void d1channel0_channelResponse(ANT_Response response)
        {
            threadSafePrintLine(decodeChannelFeedback(response), textBox_device1);
        }

        //This function decodes the message code into human readable form, shows the error value on failures, and also shows the raw message contents
        String decodeDeviceFeedback(ANT_Response response)
        {
            string toDisplay = "Device: ";

            //The ANTReferenceLibrary contains all the message and event codes in user-friendly enums
            //This allows for more readable code and easy conversion to human readable strings for displays

            // So, for the device response we first check if it is an event, then we check if it failed,
            // and display the failure if that is the case. "Events" use message code 0x40.
            if (response.responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.RESPONSE_EVENT_0x40)
            {
                //We cast the byte to its messageID string and add the channel number byte associated with the message
                toDisplay += (ANT_ReferenceLibrary.ANTMessageID)response.messageContents[1] + ", Ch:" + response.messageContents[0];
                //Check if the eventID shows an error, if it does, show the error message
                if ((ANT_ReferenceLibrary.ANTEventID)response.messageContents[2] != ANT_ReferenceLibrary.ANTEventID.RESPONSE_NO_ERROR_0x00)
                    toDisplay += Environment.NewLine + ((ANT_ReferenceLibrary.ANTEventID)response.messageContents[2]).ToString();
            }
            else   //If the message is not an event, we just show the messageID
                toDisplay += ((ANT_ReferenceLibrary.ANTMessageID)response.responseID).ToString();

            //Finally we display the raw byte contents of the response, converting it to hex
            toDisplay += Environment.NewLine + "::" + Convert.ToString(response.responseID, 16) + ", " + BitConverter.ToString(response.messageContents) + Environment.NewLine;
            return toDisplay;
        }

        String decodeChannelFeedback(ANT_Response response)
        {
            //Decoding channel events is an important consideration when building applications.
            //The channel events will be where broadcast, ack and burst messages are received, you
            //are also notified of messages sent and, for acknowledged and burst messages, their success or failure.
            //In a burst transfer, or if the device is operating at a small message period, then it is important that
            //decoding the messages is done quickly and efficiently.
            //The way the response retrieval works, messages should never be lost. If you decode messages too slow, they are simply queued.
            //However, you will be retrieving messages at a longer and longer delay from when they were actually received by the device.

            //One further consideration when handling channel events is to remember that the feedback functions will be triggered on a different thread
            //which is operating within the managed library to retrieve the feedback from the device. So, if you need to interact with your UI objects
            //you will need to use a Dispatcher invoke with a prority that will not make the UI unresponsive (see the threadSafePrint function used here).

            StringBuilder stringToPrint;    //We use a stringbuilder for speed and better memory usage, but, it doesn't really matter for the demo.
            stringToPrint = new StringBuilder("Channel: ", 100); //Begin the string and allocate some more space

            //In the channel feedback we will get either RESPONSE_EVENTs or receive events,
            //If it is a response event we display what the event was and the error code if it failed.
            //Mostly, these response_events will all be broadcast events from a Master channel.
            if (response.responseID == (byte)ANT_ReferenceLibrary.ANTMessageID.RESPONSE_EVENT_0x40)
                stringToPrint.AppendLine(((ANT_ReferenceLibrary.ANTEventID)response.messageContents[2]).ToString());
            else   //This is a receive event, so display the ID
                stringToPrint.AppendLine("Received " + ((ANT_ReferenceLibrary.ANTMessageID)response.responseID).ToString());

            //Always print the raw contents in hex, with leading '::' for easy visibility/parsing
            //If this is a receive event it will contain the payload of the message
            stringToPrint.Append("  :: ");
            stringToPrint.Append(Convert.ToString(response.responseID, 16));
            stringToPrint.Append(", ");
            stringToPrint.Append(BitConverter.ToString(response.messageContents) + Environment.NewLine);
            return stringToPrint.ToString();
        }

        void threadSafePrintLine(String stringToPrint, TextBox boxToPrintTo)
        {
            //We need to put this on the dispatcher because sometimes it is called by the feedback thread
            //If you set the priority to 'background' then it never interferes with the UI interaction if you have a high message rate (we don't have to worry about it in the demo)
            boxToPrintTo.Dispatcher.BeginInvoke(new dAppendText(boxToPrintTo.AppendText), System.Windows.Threading.DispatcherPriority.Background, stringToPrint + Environment.NewLine);
        }

        private void shutdown()
        {
            //If you need to manually disconnect from a ANTDevice call the shutdownDeviceInstance method,
            //It releases all the resources, the device, and nullifies the object reference
            ANT_Device.shutdownDeviceInstance(ref device0);
            ANT_Device.shutdownDeviceInstance(ref device1);
        }

        //This function just automatically scrolls to the bottom of the textbox
        private void textBox_autoScroll(object sender, TextChangedEventArgs e)
        {
            ((TextBox)sender).ScrollToEnd();
        }

        private void button_DoAction_Click(object sender, RoutedEventArgs e)
        {
            switch ((String)((ComboBoxItem)comboBox_actionList.SelectedItem).Content)
            {
                case "Send Acknowledged Message (d0)":
                    action_SendAcknowledged();
                    break;
                case "Send Burst Message (d0)":
                    action_SendBurst();
                    break;
                case "Set Broadcast to random value (d0)":
                    action_SetRandomBroadcast();
                    break;
                case "Request Capabilities":
                    action_ReqCapabilities();
                    break;
                case "Request Version":
                    action_ReqVersion();
                    break;
                case "Request Channel ID":
                    action_ReqChannelID();
                    break;
                case "Request Status":
                    action_ReqStatus();
                    break;
                default:
                    break;
            }
            threadSafePrintLine("", textBox_Display);
        }

        private void action_SendAcknowledged()
        {
            //Acknowledged messages send an 8 byte payload and listen for an acknowledgement from a receiving device
            //They return a MessagingReturnCode indicating the result of the process.
            //The byte array sent in the transmission functions can be any length and will be automatically padded with zeros
            threadSafePrintLine("Sending Acknowledged Message from device 0", textBox_Display);
            if (device0.getChannel(0).sendAcknowledgedData(new byte[] { 0, 1, 2, 3, 4, 5, 6, 7 }, 500) != ANT_ReferenceLibrary.MessagingReturnCode.Pass)
                threadSafePrintLine("Acknowledged Message Failed", textBox_Display);
            else
                threadSafePrintLine("Acknowledged Message Succeeded", textBox_Display);

        }

        private void action_SendBurst()
        {
            //Burst transfers will send the contents of the given byte array in 8 byte packets, ensuring each packet is
            //acknowledged by a receiving device. Packets will be resent up to five times on failures, if the packet fails again
            //the device will stop the burst and return a failure message.
            //In the managaed library all that is required is packaging the data in a byte array and the rest of the details are handled automatically
            byte[] byteArray = new byte[256];
            for (int i = 0; i < 256; ++i)
                byteArray[i] = (byte)i;

            threadSafePrintLine("Sending Burst Message from device 0", textBox_Display);
            if(device0.getChannel(0).sendBurstTransfer(byteArray, 3000) != ANT_ReferenceLibrary.MessagingReturnCode.Pass)
                threadSafePrintLine("Burst Message Failed", textBox_Display);
            else
                threadSafePrintLine("Burst Message Succeeded", textBox_Display);
        }

        private void action_SetRandomBroadcast()
        {
            //At the given frequency specified by the set message period, a master channel will broadcast the contents of its buffer.
            //This buffer contains whatever was last sent by the device, so it is important to manage it if the master broadcast
            //contains information being used elsewhere. It is recommended to set the broadcast message to what it should be every time
            //you receive an EVENT_TX message
            //The sendBroadcast function sets the buffer. On a slave device, a single broadcast
            //message will be sent.
            threadSafePrintLine("Setting Device 0 Broadcast Buffer to Random Value", textBox_Display);
            byte[] randArray = new byte[8];
            Random generator = new Random();
            generator.NextBytes(randArray);
            if(!device0.getChannel(0).sendBroadcastData(randArray))
                threadSafePrintLine("Broadcast Message Failed", textBox_Display);
            else
                threadSafePrintLine("Broadcast Message Succeeded", textBox_Display);
        }

        private void action_ReqCapabilities()
        {
            //The managed library provides this function which automatically requests and decodes device capabilities.
            //Requesting Capabilities returns a DeviceCapabilities object containing all the parameters decoded.
            //These capabilities are cached, so the message is not requested with the physical device unless the
            //optional forceNewCopy parameter is set to true.
            //Here, we force a new copy show it shows in the device feedback. The function will throw an exception
            //if the operation times out so we watch for that.
            threadSafePrintLine("Requesting Device 0's Capabilities", textBox_Display);
            try
            {
                ANT_DeviceCapabilities currentCaps = device0.getDeviceCapabilities(true, 500);
                //The capabilities object convieniently converts to a formatted string
                threadSafePrintLine("Retrieved Device 0's Capabilites:", textBox_Display);
                threadSafePrintLine(currentCaps.ToString(), textBox_Display);
            }
            catch (Exception)
            {
                threadSafePrintLine("Requesting Capabilities of Device 0 Timed Out", textBox_Display);
            }


            //If device 1 is open, request its capabilities too
            if (device1 != null)
            {
                threadSafePrintLine("Requesting Device 1's Capabilities", textBox_Display);
                try
                {
                    ANT_DeviceCapabilities currentCaps = device1.getDeviceCapabilities(true, 500);
                    threadSafePrintLine("Retrieved Device 1's Capabilites:", textBox_Display);
                    threadSafePrintLine(currentCaps.ToString(), textBox_Display);
                }
                catch (Exception)
                {
                    threadSafePrintLine("Requesting Capabilities of Device 1 Timed Out", textBox_Display);
                }
            }

        }

        private void action_ReqVersion()
        {
            //To request the version we use the requestMessageAndResponse function. To request messages you could
            //also use the requestMessage function, but then you need to monitor the device feedback for the response.
            //The version string is a null terminated ascii string, so we decode it and display it
            threadSafePrintLine("Requesting Device 0 Version", textBox_Display);
            //On failure an exception is thrown so we watch for it
            try
            {
                ANT_Response version = device0.requestMessageAndResponse(ANT_ReferenceLibrary.RequestMessageID.VERSION_0x3E, 500);
                threadSafePrintLine("Device 0 ANT Version: " + new ASCIIEncoding().GetString(version.messageContents, 0, version.messageContents.Length - 1), textBox_Display);
            }
            catch (Exception)
            {
                threadSafePrintLine("Requesting Device 0 Version Timed Out", textBox_Display);
            }

            //if device 1 is open, request its version too
            if (device1 != null)
            {
                threadSafePrintLine("Requesting Device 1 Version", textBox_Display);
                try
                {
                    ANT_Response version = device1.requestMessageAndResponse(ANT_ReferenceLibrary.RequestMessageID.VERSION_0x3E, 500);
                    threadSafePrintLine("Device 1 ANT Version: " + new ASCIIEncoding().GetString(version.messageContents, 0, version.messageContents.Length - 1), textBox_Display);
                }
                catch (Exception)
                {
                    threadSafePrintLine("Requesting Device 1 Version Timed Out", textBox_Display);
                }
            }
        }

        private void action_ReqChannelID()
        {
            //To request the ID we also use the requestMessageAndResponse function
            threadSafePrintLine("Requesting Device 0 Channel ID", textBox_Display);

            //On failure an exception is thrown so we watch for it
            try
            {
                ANT_Response IDresp = device0.requestMessageAndResponse(ANT_ReferenceLibrary.RequestMessageID.CHANNEL_ID_0x51, 500);
                threadSafePrintLine("Device 0 Channel ID: " + "Dev#: " + ((IDresp.messageContents[2] << 8) + IDresp.messageContents[1]) + ", DevType: " + IDresp.messageContents[3] + ", TransType: " + IDresp.messageContents[4], textBox_Display);
            }
            catch (Exception)
            {
                threadSafePrintLine("Requesting Device 0 Channel ID Timed Out", textBox_Display);
            }

            //if device 1 is open, request its version too
            if (device1 != null)
            {
                threadSafePrintLine("Requesting Device 1 Channel ID", textBox_Display);
                try
                {
                    ANT_Response IDresp = device1.requestMessageAndResponse(ANT_ReferenceLibrary.RequestMessageID.CHANNEL_ID_0x51, 500);
                    threadSafePrintLine("Device 1 Channel ID: " + "Dev#: " + ((IDresp.messageContents[2] << 8) + IDresp.messageContents[1]) + ", DevType: " + IDresp.messageContents[3] + ", TransType: " + IDresp.messageContents[4], textBox_Display);
                }
                catch (Exception)
                {
                    threadSafePrintLine("Requesting Device 1 Channel ID Timed Out", textBox_Display);
                }
            }
        }

        private void action_ReqStatus()
        {
            //The managed library has a function to request status that returns a ChannelStatus object
            threadSafePrintLine("Requesting Device 0 Status", textBox_Display);
            try
            {
                ANT_ChannelStatus chStatus = device0.getChannel(0).requestStatus(500);
                //Not all device types return a complete device status, so some of these fields may be inaccurate
                threadSafePrintLine("Device 0 Channel 0 Status: " + chStatus.BasicStatus + ", Network Number: " + chStatus.networkNumber + ", Type: " + chStatus.ChannelType, textBox_Display);
            }
            catch (Exception)
            {
                threadSafePrintLine("Requesting Device 0 Status Failed", textBox_Display);
            }

            //If present, request device 1 status too
            if (device1 != null)
            {
                threadSafePrintLine("Requesting Device 1 Status", textBox_Display);
                try
                {
                    ANT_ChannelStatus chStatus = device1.getChannel(0).requestStatus(500);
                    //Not all device types return a complete device status, so some of these fields may be inaccurate
                    threadSafePrintLine("Device 1 Channel 0 Status: " + chStatus.BasicStatus + ", Network Number: " + chStatus.networkNumber + ", Type: " + chStatus.ChannelType, textBox_Display);
                }
                catch (Exception)
                {
                    threadSafePrintLine("Requesting Device 1 Status Failed", textBox_Display);
                }
            }

        }
    }
}
