using Microsoft.VisualBasic.FileIO;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Management;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Xml.Linq;
using ZwiftHandler.Properties;

namespace ZwiftHandler
{
    //TODO: environment: =1,ZWIFT_EDGE_REMOTE etc
    //
    public partial class Form1 : Form
    {
        private static string hostsFileDir = Environment.ExpandEnvironmentVariables(@"%windir%\System32\drivers\etc");
        private static string hostsFileName = hostsFileDir + @"\HOSTS";
        private static string patchFolderPath;

        public Form1()
        {
            InitializeComponent();
        }

        [System.Flags]
        enum LoadLibraryFlags : uint
        {
            None = 0,
            DONT_RESOLVE_DLL_REFERENCES = 0x00000001,
            LOAD_IGNORE_CODE_AUTHZ_LEVEL = 0x00000010,
            LOAD_LIBRARY_AS_DATAFILE = 0x00000002,
            LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE = 0x00000040,
            LOAD_LIBRARY_AS_IMAGE_RESOURCE = 0x00000020,
            LOAD_LIBRARY_SEARCH_APPLICATION_DIR = 0x00000200,
            LOAD_LIBRARY_SEARCH_DEFAULT_DIRS = 0x00001000,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR = 0x00000100,
            LOAD_LIBRARY_SEARCH_SYSTEM32 = 0x00000800,
            LOAD_LIBRARY_SEARCH_USER_DIRS = 0x00000400,
            LOAD_WITH_ALTERED_SEARCH_PATH = 0x00000008
        }
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hReservedNull, LoadLibraryFlags dwFlags);
        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        static extern bool FreeLibrary(IntPtr hModule);
        [DllImport("kernel32", CharSet = CharSet.Ansi, ExactSpelling = true, SetLastError = true)]
        static extern IntPtr GetProcAddress(IntPtr hModule, string procName);
        [DllImport("Kernel32.dll", CharSet = CharSet.Unicode)]
        static extern bool CreateHardLink(string lpFileName, string lpExistingFileName,IntPtr lpSecurityAttributes);

        private void LazyCopy(string src, string dest)
        {
            var crcSrc = Crc32.CountCrc(File.ReadAllBytes(src));
            var crcDest = File.Exists(dest) ? Crc32.CountCrc(File.ReadAllBytes(dest)) : 0;
            var result = "equal crc";
            if (crcSrc == crcDest)
            {
                result = "equal CRC, skip copy";
            } else
            {
                if(File.Exists(dest))
                    FileSystem.DeleteFile(dest, UIOption.OnlyErrorDialogs, RecycleOption.SendToRecycleBin);
                File.Copy(src, dest);
                result = $"copied {crcSrc}->{crcDest}";
            }
            txtLog.AppendLine($"LazyCopy({src}, {dest}): {result}");
        }

        private void doCreateHardLink(string src, string dest)
        {
            if (File.Exists(dest)) 
                FileSystem.DeleteFile(dest, UIOption.OnlyErrorDialogs, RecycleOption.SendToRecycleBin);
            bool result = CreateHardLink(dest, src, IntPtr.Zero);
            var txt = $"doCreateHardLink({src}, {dest}): {result}";
            txtLog.AppendLine(txt);
            if (result == false)
                throw new InvalidOperationException(txt);
        }

        private void onPatch() {
            LazyCopy(Path.Combine(patchFolderPath, "ANT_DLL.org.dll"), "ANT_DLL.org.dll");
            LazyCopy(Path.Combine(patchFolderPath, "BleWin10Lib.org.dll"), "BleWin10Lib.org.dll");
            LazyCopy(Path.Combine(patchFolderPath, "BleWin10Lib_V2.org.dll"), "BleWin10Lib_V2.org.dll");
            LazyCopy(Path.Combine(patchFolderPath, @"..\ANT_DLL.dll"), "ANT_DLL.dll");
            doCreateHardLink("ANT_DLL.dll", "BleWin10Lib.dll");
            doCreateHardLink("ANT_DLL.dll", "BleWin10Lib_V2.dll");
        }
        private void onOriginal() {
            LazyCopy(Path.Combine(patchFolderPath, "ANT_DLL.org.dll"), "ANT_DLL.dll");
            LazyCopy(Path.Combine(patchFolderPath, "BleWin10Lib.org.dll"), "BleWin10Lib.dll");
            LazyCopy(Path.Combine(patchFolderPath, "BleWin10Lib_V2.org.dll"), "BleWin10Lib_V2.dll");
        }

        enum DllStatus { BrokenByUpdate, Patched, Original };
        private DllStatus GameDllStatus(string manifest /*Zwift_1.0.100353_47ac0231_manifest*/)
        {
            //Original: когда у BleWin10Lib.dll, BleWin10Lib_V2.dll и ANT_DLL.dll суммы совпадают
            var files = new string[][] {
                new string[] { "BleWin10Lib.dll", "BleWin10Lib.org.dll"},
                new string[] { "BleWin10Lib_V2.dll", "BleWin10Lib_V2.org.dll"},
                new string[] { "ANT_DLL.dll", "ANT_DLL.org.dll" }
            };
            var strWhyNot = new string[] { "", "" };
            IntPtr firstHlib = IntPtr.Zero;
            int cntHLibsEqual = 0;
            bool foundOurAntDll = false;
            var toFreeLib = new List<IntPtr>();
            try
            {
                var zwiftManifest = XElement.Load(manifest);
                foreach (var file in files)
                {
                    IntPtr lib = LoadLibraryEx(Path.GetFullPath(file[0]), IntPtr.Zero, LoadLibraryFlags.DONT_RESOLVE_DLL_REFERENCES);
                    if (lib != IntPtr.Zero)
                    {
                        toFreeLib.Add(lib);
                        //our ant_dll: both 'ANT_Close' and 'BLEInitBLEFlags' funcs present
                        foundOurAntDll = GetProcAddress(lib, "ANT_Close") != IntPtr.Zero && GetProcAddress(lib, "BLEInitBLEFlags") != IntPtr.Zero;
                        if (firstHlib == IntPtr.Zero)
                        {
                            firstHlib = lib;
                        } else if (firstHlib == lib)
                        {
                            cntHLibsEqual++;
                        }
                        txtLog.AppendLine($"InitDllStatus: {file[0]} our:{foundOurAntDll}");
                    }
                    IEnumerable<XElement> xfiles = from el in zwiftManifest.Element("files").Descendants("file")
                                                   where (string)el.Element("path") == file[0]
                                                   select el;
                    for (int i = 0; i < 2; i++)
                    {
                        if (strWhyNot[i].Length > 0) continue;
                        strWhyNot[i] = $"{file[i]}: not found by name or size";
                        if (File.Exists(file[i])) foreach (XElement el in xfiles)
                            {
                                var xcrc = (string)el.Element("checksum");
                                if ((new FileInfo(file[i])).Length.ToString() == (string)el.Element("length"))
                                {
                                    int icrc = (int)Crc32.CountCrc(File.ReadAllBytes(file[i]));
                                    if (icrc.ToString() != xcrc)
                                    {
                                        strWhyNot[i] = $"{file[i]}: crc expected: {xcrc}, but {icrc} found";
                                    } else
                                    {
                                        strWhyNot[i] = "";
                                        break;
                                    }
                                }
                            }
                    }
                }
            }
            catch (System.Exception ex)
            {
                txtLog.AppendLine($"Exc(InitDllStatus): {ex.Message}");
                return DllStatus.BrokenByUpdate; //сюда попадать не должны
            }
            finally
            {
                foreach (var l in toFreeLib)
                    FreeLibrary(l);
            }
            if (strWhyNot[0].Length == 0)
            { //оригинальные не переименованы и совпали
                txtLog.AppendLine($"InitDllStatus: Original");
                return DllStatus.Original;
            }
            txtLog.AppendLine($"InitDllStatus: Not Original: {strWhyNot[0]}");
            if (strWhyNot[1].Length != 0) //.org не совпали - порча
            {
                txtLog.AppendLine($"Exc(InitDllStatus): Bad .org: {strWhyNot[1]}");
                return DllStatus.BrokenByUpdate;
            }
            //.org совпали - либо ok-patched, либо BrokenByUpdate:
            //смотрим, указывают ли нормальные имена в один файл и тот ли он
            if (cntHLibsEqual != 2)
            {
                txtLog.AppendLine($"HardLinkCount(InitDllStatus): expected 2 repeats: {cntHLibsEqual}");
                return DllStatus.BrokenByUpdate;
            }
            var ret = foundOurAntDll ? DllStatus.Patched : DllStatus.BrokenByUpdate;
            txtLog.AppendLine($"InitDllStatus: {ret}");
            return ret;
        }
        private void InitDllStatus()
        {
            try
            {
                Cursor.Current = Cursors.WaitCursor;
                Application.UseWaitCursor = true;
                Application.DoEvents();

                var zwiftVerXml = XElement.Load("Zwift_ver_cur.xml");
                var zwiftVer = zwiftVerXml.Attribute("sversion").Value;
                txtLog.AppendLine($"InitDllStatus: {zwiftVer}");
                patchFolderPath = Path.Combine(Settings.Default.PatchSharedPath, $"patch_v{zwiftVer}");
                var dllStatus = GameDllStatus(zwiftVerXml.Attribute("manifest").Value);
                cmdPatch.Enabled = false;
                cmdOriginal.Enabled = false;

                if (!Directory.Exists(patchFolderPath))
                { // поставлен новый дистрибутив
                    switch (dllStatus)
                    {
                        case DllStatus.BrokenByUpdate: //хотя бы что-то не так с состоянием б-к
                            throw new InvalidOperationException($"Обнаружена новая версия {zwiftVer}, но файлы от нее были испорчены обновлением. Попробуйте с другого ПК, предварительно отменив dll patch.");
                        case DllStatus.Patched: //вариант, когда каталога нет, но игра пропатчена, считаем невозможным
                            throw new InvalidOperationException($"Обнаружена новая версия {zwiftVer}, но файлы в ней уже пропатчены - разберитесь вручную.");
                        case DllStatus.Original:
                            if (MessageBox.Show($"Обнаружена новая версия {zwiftVer} - зарегистрировать?", "InitDllStatus", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
                            {
                                Directory.CreateDirectory(patchFolderPath);
                                File.Copy("ANT_DLL.dll", Path.Combine(patchFolderPath, "ANT_DLL.org.dll"));
                                File.Copy("BleWin10Lib.dll", Path.Combine(patchFolderPath, "BleWin10Lib.org.dll"));
                                File.Copy("BleWin10Lib_V2.dll", Path.Combine(patchFolderPath, "BleWin10Lib_V2.org.dll"));
                                cmdPatch.Enabled = true;
                                cmdOriginal.Enabled = true;
                            }
                            break;
                    }
                } else //считаем, что папка уже оформлена как надо и в ней правильные файлы
                {
                    cmdPatch.Enabled = true; //из такого состояния всегда можно
                    cmdOriginal.Enabled = true;
                    switch (dllStatus)
                    {
                        case DllStatus.BrokenByUpdate: //восстанавливаем с подтверждением
                            if (MessageBox.Show($"Обнаружена известная версия {zwiftVer}, испорченная обновлением - восстановить патч?", "InitDllStatus", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
                            {
                                onPatch();
                            }
                            break;
                        case DllStatus.Patched: // возможно, патч можно обновить или откатить
                            if (File.GetLastWriteTime(Path.Combine(Settings.Default.PatchSharedPath, "ANT_DLL.dll")) > File.GetLastWriteTime("ANT_DLL.dll") && 
                                MessageBox.Show($"Обнаружено обновление ANT_DLL - заменить?", "InitDllStatus", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
                            {
                                onPatch();
                            }
                            AcceptButton = cmdOriginal;
                            break;
                        case DllStatus.Original: // возможно, патч требуется накатить
                            AcceptButton = cmdPatch;
                            break;
                    }
                }
            }
            catch (Exception ex)
            {
                txtLog.AppendLine($"Exc(InitDllStatus): {ex.Message}");
                MessageBox.Show(ex.Message, "InitDllStatus", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                Application.UseWaitCursor = false;
                Cursor.Current = Cursors.Default;
                Application.DoEvents();
            }
        }
        private void Form1_Load(object sender, EventArgs e)
        {
            toolTip.SetToolTip(txtZwifOfflineServerIp, "Zwift offline server IP");
            toolTip.SetToolTip(lbHosts, $"Pre-configured HOSTS, dblclick to edit {hostsFileName}");
            toolTip.SetToolTip(chkPatchZwift, "ZWIFT_PATCHES env variable");
            toolTip.SetToolTip(txtSchwinnLog, "ZWIFT_DUMP_SCHWINN env variable");
            toolTip.SetToolTip(txtEdgeRemoteId, "ZWIFT_EDGE_REMOTE env variable");

            fswHosts.Path = hostsFileDir;
            InitHostsListbox();
            InitDllStatus();

            if (Environment.GetEnvironmentVariable("ZWIFT_PATCHES") == "1")
                chkPatchZwift.Checked = true;

            txtSchwinnLog.Text = Environment.GetEnvironmentVariable("ZWIFT_DUMP_SCHWINN");
            txtEdgeRemoteId.Text = Environment.GetEnvironmentVariable("ZWIFT_EDGE_REMOTE");
        }

        private void InitHostsListbox()
        {
            try
            {
                Cursor.Current = Cursors.WaitCursor;
                Application.UseWaitCursor = true;
                Application.DoEvents();

                lbHosts.Items.Clear();
                string curHostsContents = File.ReadAllText(hostsFileName, Encoding.UTF8);
                var sel = -1;
                foreach (var h in Directory.EnumerateFiles(hostsFileDir, "HOSTS.zwift.*"))
                {
                    var idx = lbHosts.Items.Add(Path.GetFileName(h));
                    if (ReadHostsTemplate(h) == curHostsContents) sel = idx;
                }
                lbHosts.SelectedIndex = sel;
            }
            catch (Exception ex)
            {
                txtLog.AppendLine($"Exc(InitHostsListbox): {ex.Message}");
                MessageBox.Show(ex.Message, "InitHostsListbox", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                Application.UseWaitCursor = false;
                Cursor.Current = Cursors.Default;
                Application.DoEvents();
            }
        }

        private static string ReadHostsTemplate(string fn)
        {
            return File.ReadAllText(fn, Encoding.UTF8).Replace("<zwift_offline_ip>", Settings.Default.ZwiftOfflineServerIp);
        }

        private void txtZwifOfflineServerIp_Validating(object sender, CancelEventArgs e)
        {
            try
            {
                Cursor.Current = Cursors.WaitCursor;
                Application.UseWaitCursor = true;
                Application.DoEvents();

                var ping = new Ping();
                var address = IPAddress.Parse(txtZwifOfflineServerIp.Text);
                var pong = ping.Send(address);
                if (pong.Status != IPStatus.Success)
                    throw new InvalidOperationException($"ping до {address} неудачен");

                Settings.Default.ZwiftOfflineServerIp = txtZwifOfflineServerIp.Text;
                Settings.Default.Save();

                InitHostsListbox();

            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message, "OfflineServerIp", MessageBoxButtons.OK, MessageBoxIcon.Error);
                e.Cancel = true;
            }
            finally
            {
                Application.UseWaitCursor = false;
                Cursor.Current = Cursors.Default;
                Application.DoEvents();
            }
        }

        private void lbHosts_DoubleClick(object sender, EventArgs e)
        {
            Process.Start("notepad.exe", hostsFileName);
        }

        private void lbHosts_SelectedIndexChanged(object sender, EventArgs e)
        {
            try
            {
                var curHostsContents = File.ReadAllText(hostsFileName, Encoding.UTF8);
                var newHostsFile = Path.Combine(hostsFileDir, lbHosts.Text);
                var newHostsContents = ReadHostsTemplate(newHostsFile);
                if (curHostsContents != newHostsContents)
                {
                    if (MessageBox.Show($"Заменить файл hosts на {lbHosts.Text} ?", "Внимание!", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
                    {
                        File.WriteAllText(hostsFileName, newHostsContents); //watcher should see this
                    } else
                    {
                        InitHostsListbox();
                    }
                }
            }
            catch (Exception ex)
            {
                txtLog.AppendLine($"Exc(lbHosts_SelectedIndexChanged): {ex.Message}");
                MessageBox.Show(ex.Message, "lbHosts_SelectedIndexChanged", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }

        }

        private void fswHosts_Changed(object sender, FileSystemEventArgs e)
        {
            InitHostsListbox();
        }

        private void fswHosts_Created(object sender, FileSystemEventArgs e)
        {
            InitHostsListbox();
        }

        private void fswHosts_Deleted(object sender, FileSystemEventArgs e)
        {
            InitHostsListbox();
        }

        private void fswHosts_Renamed(object sender, RenamedEventArgs e)
        {
            InitHostsListbox();
        }

        private void cmdPatch_Click(object sender, EventArgs e)
        {
            try
            {
                onPatch();
            }
            catch (Exception ex)
            {
                txtLog.AppendLine($"Exc(cmdPatch_Click): {ex.Message}");
                MessageBox.Show(ex.Message, "cmdPatch_Click", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void cmdOriginal_Click(object sender, EventArgs e)
        {
            try
            {
                onOriginal();
            }
            catch (Exception ex)
            {
                txtLog.AppendLine($"Exc(cmdOriginal_Click): {ex.Message}");
                MessageBox.Show(ex.Message, "cmdOriginal_Click", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private bool ZofflineHere() {
            var host = Dns.GetHostEntry(Dns.GetHostName());
            foreach (var ip in host.AddressList)
            {
                if (ip.AddressFamily == AddressFamily.InterNetwork)
                {
                    if (ip.ToString() == txtZwifOfflineServerIp.Text) {
                        return true;
                    }
                }
            }
            return false;
        }

        private static string GetCommandLine(Process process)
        {
            using (var searcher = new ManagementObjectSearcher("SELECT CommandLine FROM Win32_Process WHERE ProcessId = " + process.Id))
            using (var objects = searcher.Get())
            {
                return objects.Cast<ManagementBaseObject>().SingleOrDefault()?["CommandLine"]?.ToString();
            }
        }
        private void cmdStartZwift_Click(object sender, EventArgs e)
        {
            try
            {
                Environment.SetEnvironmentVariable("ZWIFT_PATCHES", chkPatchZwift.Checked ? "1" : "0");
                Environment.SetEnvironmentVariable("ZWIFT_DUMP_SCHWINN", txtSchwinnLog.Text);
                Environment.SetEnvironmentVariable("ZWIFT_EDGE_REMOTE", txtEdgeRemoteId.Text);

                foreach (var process in Process.GetProcessesByName("ZwiftLauncher"))
                {
                    txtLog.AppendLine($"Stopping launcher: {process.Id}:{process.ProcessName}");
                    process.Kill();
                }
                foreach (var process in Process.GetProcessesByName("ZwiftApp"))
                {
                    if (MessageBox.Show("Zwift работает. Остановить?", "Внимание!", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes) {
                        txtLog.AppendLine($"Stopping game: {process.Id}:{process.ProcessName}");
                        process.Kill();
                    } else {
                        return;
                    }
                }
                if (!ModifierKeys.HasFlag(Keys.Control)) {
                    if (lbHosts.Text.Contains("offline") && ZofflineHere()) {
                        bool bNeedStartZoffline = true;
                        foreach (var process in Process.GetProcessesByName("python"))
                        {
                            if (GetCommandLine(process).Contains("standalone.py"))
                                bNeedStartZoffline = false;
                        }
                        if(bNeedStartZoffline)
                        {
                            Process.Start("cmd.exe", $"/c {Settings.Default.ZofflineBatch}");
                            Thread.Sleep(1000);
                        }
                    }
                    Process.Start("ZwiftLauncher.exe");
                    lastLauncherStart = DateTime.Now;
                }
            }
            catch (Exception ex)
            {
                txtLog.AppendLine($"Exc(cmdStartZwift_Click): {ex.Message}");
                MessageBox.Show(ex.Message, "cmdStartZwift_Click", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
        DateTime lastLauncherStart = DateTime.MinValue;
        private void cmdRefresh_Click(object sender, EventArgs e)
        {
            InitDllStatus();
        }

        private void txtEdgeRemoteId_DoubleClick(object sender, EventArgs e)
        {
            txtEdgeRemoteId.Text = "79221";
        }

        private void txtSchwinnLog_DoubleClick(object sender, EventArgs e)
        {
            Process.Start("notepad.exe", txtSchwinnLog.Text);
        }

        [DllImport("user32.dll", EntryPoint = "FindWindow")]
        private static extern IntPtr FindWindow(string sClass, string sWindow);
        [DllImport("user32.dll", SetLastError = true)]
        static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
        const uint SWP_NOZORDER = 0x0004;
        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
        [StructLayout(LayoutKind.Sequential)]
        public struct RECT
        {
            public int Left;        // x position of upper-left corner
            public int Top;         // y position of upper-left corner
            public int Right;       // x position of lower-right corner
            public int Bottom;      // y position of lower-right corner
        }

        private void MoveZwiftWindowTo(Rectangle pos) {
            string result = "Zwift window not found";
            var hwnd = FindWindow(null, "Zwift");
            if (hwnd != IntPtr.Zero) {
                result = SetWindowPos(hwnd, IntPtr.Zero, pos.X, pos.Y, pos.Width, pos.Height, SWP_NOZORDER).ToString();
            }
            txtLog.AppendLine($"MoveZwiftWindowTo: {pos} res={result}");
        }
        private void MoveZwiftWindowToCursor() 
        {
            MoveZwiftWindowTo(new Rectangle(Cursor.Position.X, Cursor.Position.Y, Width, Height));
        }
        private void moveZwiftHereToolStripMenuItem_Click(object sender, EventArgs e)
        {
            MoveZwiftWindowToCursor();
        }

        private void recallZwiftPositionToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if(Settings.Default.ZwiftWindow.IsEmpty)
            {
                txtLog.AppendLine($"Stored ZwiftWindow Is Empty: moving here");
                MoveZwiftWindowToCursor();
                return;
            }
            MoveZwiftWindowTo(Settings.Default.ZwiftWindow);
        }

        private void cmdStartZwift_MouseDown(object sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
                ctxMenu.Show(Cursor.Position);
        }

        [DllImport("ntdll.dll", PreserveSig = false)]
        public static extern void NtSuspendProcess(IntPtr processHandle);
        
        private void suspendZwiftToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (suspendZwiftToolStripMenuItem.Checked)
            {
                suspendZwiftToolStripMenuItem.Checked = false;
            } else
            {
                if(!SuspendZwift())
                    suspendZwiftToolStripMenuItem.Checked = true;
            }
        }

        private bool SuspendZwift()
        {
            foreach (var process in Process.GetProcessesByName("ZwiftApp"))
            {
                NtSuspendProcess(process.Handle);
                txtLog.AppendLine($"ZwiftApp pid={process.Id} suspended");
                suspendZwiftToolStripMenuItem.Checked = false;
                return true;
            }
            txtLog.AppendLine("No ZwiftApp processes found to suspend - delayed");
            return false;
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            try
            {
                timer1.Enabled = false;
                if (suspendZwiftToolStripMenuItem.Checked)
                {
                    if (SuspendZwift() || suspendZwiftToolStripMenuItem.Checked == false)
                        return;
                }
                if (lastLauncherStart != DateTime.MinValue)
                {
                    //если уже есть окно Zwift, то лончеру хана
                    var hwnd = FindWindow(null, "Zwift");
                    if (hwnd != IntPtr.Zero || (DateTime.Now - lastLauncherStart).TotalSeconds > 18) /* три минуты на ввод пароля*/
                    {
                        lastLauncherStart = DateTime.MinValue;
                        foreach (var process in Process.GetProcessesByName("ZwiftLauncher"))
                        {
                            if (hwnd == IntPtr.Zero)
                            {
                                BringToFront();
                                if (MessageBox.Show("Kill launcher?", "Warning!", MessageBoxButtons.YesNo,
                                    MessageBoxIcon.Question, MessageBoxDefaultButton.Button1, 
                                    MessageBoxOptions.DefaultDesktopOnly) != DialogResult.Yes)
                                    return;
                            }
                            txtLog.AppendLine($"Stopping launcher to prevent unexpected update: {process.Id}:{process.ProcessName}");
                            process.Kill();
                        }
                    }
                }
            } catch (Exception ex)
            {
                txtLog.AppendLine($"Exc(timer1_Tick): {ex.Message}");
            } finally
            {
                timer1.Enabled = true;
            }
        }
        private void zwiftPositionToolStripMenuItem_Click(object sender, EventArgs e)
        {
            string result = "Zwift window not found";
            var hwnd = FindWindow(null, "Zwift");
            if (hwnd != IntPtr.Zero)
            {
                RECT rct;
                if (!GetWindowRect(hwnd, out rct))
                {
                    result = "false";
                } else
                {
                    var myrect = new Rectangle(rct.Left, rct.Top, rct.Right - rct.Left, rct.Bottom - rct.Top);
                    if (myrect.IsEmpty)
                    {
                        result = "empty";
                    } else
                    {
                        Settings.Default.ZwiftWindow = myrect;
                        Settings.Default.Save();
                        result = "true";
                    }
                }
                txtLog.AppendLine($"StoreZwiftWindow: {Settings.Default.ZwiftWindow} res={result}");
            }
        }
    }
    class Crc32
    {
        public static uint CountCrc(byte[] pBuf)
        {
            // Table of CRC-32's of all single byte values
            uint[] crctab = new uint[] {
          0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419,
          0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4,
          0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07,
          0x90bf1d91, 0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
          0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856,
          0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
          0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4,
          0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
          0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3,
          0x45df5c75, 0xdcd60dcf, 0xabd13d59, 0x26d930ac, 0x51de003a,
          0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599,
          0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
          0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190,
          0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f,
          0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e,
          0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
          0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed,
          0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
          0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3,
          0xfbd44c65, 0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
          0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a,
          0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5,
          0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa, 0xbe0b1010,
          0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
          0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17,
          0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6,
          0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615,
          0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
          0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1, 0xf00f9344,
          0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
          0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a,
          0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
          0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1,
          0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c,
          0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef,
          0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
          0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe,
          0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31,
          0x2cd99e8b, 0x5bdeae1d, 0x9b64c2b0, 0xec63f226, 0x756aa39c,
          0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
          0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b,
          0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
          0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1,
          0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
          0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45, 0xa00ae278,
          0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7,
          0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66,
          0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
          0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605,
          0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8,
          0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b,
          0x2d02ef8d
        };

            uint c = 0xffffffff;  // begin at shift register contents 
            int i, n = pBuf.Length;
            for (i = 0; i < n; i++)
            {
                c = crctab[((int)c ^ pBuf[i]) & 0xff] ^ (c >> 8);
            }
            return c ^ 0xffffffff;
        }
    }
    public static class WinFormsExtensions
    {
        public static void AppendLine(this TextBox source, string value)
        {
            var line = $"[{DateTime.Now.ToString("HH:mm:ss")}] {value}";
            if (source.Text.Length == 0)
                source.Text = line;
            else
                source.AppendText("\r\n" + line);
        }
    }
}
