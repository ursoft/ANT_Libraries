using System;
using System.Linq;
using System.Windows.Forms;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Bluetooth.Advertisement;
using System.Threading;

namespace WindowsFormsApp1 {
    public partial class Form1 : Form {
        public ulong address;
        public Form1() {
            InitializeComponent();
        }
        private void Form1_Load(object sender, EventArgs e) {
            var watcher = new BluetoothLEAdvertisementWatcher();
            watcher.ScanningMode = BluetoothLEScanningMode.Active;
            watcher.Received += OnAdvertisementReceived;
            watcher.Start();
        }
        int logRepeats = 0;
        void Log(string s) {
            listBoxLog.Invoke((MethodInvoker)delegate {
                var lastIdx = listBoxLog.Items.Count - 1;
                if (lastIdx >= 0) {
                    var strLast = (string)listBoxLog.Items[lastIdx];
                    if (strLast.StartsWith(s)) {
                        logRepeats++;
                        listBoxLog.Items[lastIdx] = string.Format("{0} [{1}]", s, logRepeats);
                        return;
                    }
                }
                logRepeats = 0;
                var cnt = listBoxLog.Items.Add(s);
                if (!listBoxLog.Focused)
                    listBoxLog.SelectedIndex = cnt;
            });
        }
        void Log(Label l, string s) {
            l.Invoke((MethodInvoker)delegate {
                l.Text = s;
            });
        }
        void OnAdvertisementReceived(BluetoothLEAdvertisementWatcher watcher, BluetoothLEAdvertisementReceivedEventArgs eventArgs) {
            bool bFound = eventArgs.Advertisement.LocalName == "Schwinn";
            Log(string.Format("[{0}] ", bFound ? "FOUND" : "SKIPPED"));
            Log(string.Format("BT_ADDR: {0:X}, NAME:{1}", eventArgs.BluetoothAddress, eventArgs.Advertisement.LocalName));
            if (!bFound)
                return;
            address = eventArgs.BluetoothAddress;
            watcher.Stop();
            Invoke((MethodInvoker)delegate { Thread.Sleep(500); Subscribe(); });
        }
        GattCharacteristic chr_resist;
        private async void resist_ValueChanged(object sender, EventArgs e) {
            if (chr_resist == null) {
                Log("No controllable device found");
                return;
            }
            var dw = new Windows.Storage.Streams.DataWriter();
            dw.WriteByte(4); //4=resistance, or 5=power level
            dw.WriteByte((byte)resist.Value);
            dw.WriteByte(0);
            dw.WriteByte(0);
            dw.WriteByte(0);
            dw.WriteByte(0);
            dw.WriteByte(0);
            dw.WriteByte(0);
            await chr_resist.WriteValueAsync(dw.DetachBuffer());
        }
        private async void Subscribe() {
            if (address != 0) {
                Log("Subscribe to notifications...");
                var device = await BluetoothLEDevice.FromBluetoothAddressAsync(address);
                var serv = (await device.GetGattServicesForUuidAsync(BluetoothUuidHelper.FromShortId(0x1826),
                    BluetoothCacheMode.Uncached)).Services.First();
                var chr = (await serv.GetCharacteristicsForUuidAsync(BluetoothUuidHelper.FromShortId(0x2AD2),
                    BluetoothCacheMode.Uncached)).Characteristics.First();
                chr.ValueChanged += Chars_ValueChanged;
                await chr.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue.Notify);
                chr_resist = (await serv.GetCharacteristicsForUuidAsync(BluetoothUuidHelper.FromShortId(0x2AD9),
                    BluetoothCacheMode.Uncached)).Characteristics.First();
                var chr_range = (await serv.GetCharacteristicsForUuidAsync(BluetoothUuidHelper.FromShortId(0x2AD6),
                    BluetoothCacheMode.Uncached)).Characteristics.First();
                var range = await chr_range.ReadValueAsync();
                using (var dataReader = Windows.Storage.Streams.DataReader.FromBuffer(range.Value)) {
                    byte[] packet = new byte[6];
                    dataReader.ReadBytes(packet);
                    resist.Invoke((MethodInvoker)delegate {
                        resist.Maximum = packet[2];
                    });
                }
            } else {
                Log("No controllable device found");
            }
        }
        int old_cad = -1, old_pwr = -1, old_hr = -1;
        private void Chars_ValueChanged(GattCharacteristic sender, GattValueChangedEventArgs args) {
            using (var dataReader = Windows.Storage.Streams.DataReader.FromBuffer(args.CharacteristicValue)) {
                byte[] packet = new byte[7];
                dataReader.ReadBytes(packet);
                var cad = (int)packet[2] + (int)packet[3] * 256;
                var pwr = (int)packet[4] + (int)packet[5] * 256;
                var hr = (int)packet[6];
                Log(string.Format("Notification: cad={0}, pwr={1}, hr={2}", cad, pwr, hr));
                if(cad != old_cad) { Log(cadence, string.Format("{0:00.0} rpm", cad/2.0)); old_cad = cad; }
                if(pwr != old_pwr) { Log(power, string.Format("{0} W", pwr)); old_pwr = pwr; }
                if(hr != old_hr) { Log(heart, string.Format("{0} ♥", hr)); old_hr = hr; }
            }
        }
    }
}
