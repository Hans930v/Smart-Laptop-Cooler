// Program.cs
// Requires: dotnet add package LibreHardwareMonitorLib
//           dotnet add package System.IO.Ports
// Must run as Administrator (required for hardware sensor access)
//
// ============================================================
// Deployment notes:
// - Build in Release mode: dotnet publish -c Release -r win-x64 --self-contained -p:PublishSingleFile=true
// - Recommended: place a shortcut to the published .exe in
//     Win+R -> shell:startup
//   so it appears under Settings -> Apps -> Startup and auto-launches at login.
// - Right-click the shortcut -> Properties -> Shortcut -> Advanced ->
//     "Run as administrator" (required for sensor access). This will prompt
//     UAC once at each login unless elevation is otherwise pre-approved.
// - Behavior: a console window is shown briefly on startup for visibility
//   into initialization/scanning, then automatically hidden once the
//   Bluetooth connection to Smart Laptop Cooler succeeds. The app then
//   continues running silently from the system tray. Right-click the
//   tray icon for status and to exit.
// ============================================================

using System;
using System.IO;
using System.IO.Ports;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;
using LibreHardwareMonitor.Hardware;

namespace ThermalBridge
{
    public class SensorReader : IVisitor
    {
        private readonly Computer _computer;

        public SensorReader()
        {
            _computer = new Computer
            {
                IsCpuEnabled = true,
                IsGpuEnabled = true,
                IsMemoryEnabled = false,
                IsMotherboardEnabled = false,
                IsStorageEnabled = false,
                IsNetworkEnabled = false
            };
            _computer.Open();
            _computer.Accept(this);
        }

        public void VisitComputer(IComputer computer) => computer.Traverse(this);

        public void VisitHardware(IHardware hardware)
        {
            hardware.Update();
            foreach (var sub in hardware.SubHardware)
                sub.Accept(this);
        }

        public void VisitSensor(ISensor sensor) { }
        public void VisitParameter(IParameter parameter) { }

        public void DumpAllSensors()
        {
            Logger.Log("\n===== FULL SENSOR DUMP =====");
            foreach (var hardware in _computer.Hardware)
            {
                hardware.Update();
                Logger.Log($"\n[Hardware] {hardware.Name}  (Type: {hardware.HardwareType})");
                foreach (var sensor in hardware.Sensors)
                    Logger.Log($"    - {sensor.SensorType,-15} {sensor.Name,-35} {sensor.Value?.ToString("F2") ?? "null"}");

                foreach (var sub in hardware.SubHardware)
                {
                    sub.Update();
                    Logger.Log($"  [SubHardware] {sub.Name}  (Type: {sub.HardwareType})");
                    foreach (var sensor in sub.Sensors)
                        Logger.Log($"      - {sensor.SensorType,-15} {sensor.Name,-35} {sensor.Value?.ToString("F2") ?? "null"}");
                }
            }
            Logger.Log("\n===== END SENSOR DUMP =====\n");
        }

        public (float? cpuTemp, float? cpuPackagePower, float? gpuTemp) ReadAll()
        {
            float? cpuTemp = null;
            float? cpuPower = null;
            float? gpuTemp = null;

            foreach (var hardware in _computer.Hardware)
            {
                hardware.Update();

                if (hardware.HardwareType == HardwareType.Cpu)
                {
                    foreach (var sensor in hardware.Sensors)
                    {
                        if (sensor.SensorType == SensorType.Temperature && sensor.Name == "CPU Package")
                            cpuTemp = sensor.Value;
                        if (sensor.SensorType == SensorType.Power && sensor.Name == "CPU Package")
                            cpuPower = sensor.Value;
                    }
                }

                if (hardware.HardwareType == HardwareType.GpuNvidia)
                {
                    foreach (var sensor in hardware.Sensors)
                    {
                        if (sensor.SensorType == SensorType.Temperature && sensor.Name == "GPU Core")
                            gpuTemp = sensor.Value;
                    }
                }
            }

            return (cpuTemp, cpuPower, gpuTemp);
        }

        public void Close() => _computer.Close();
    }

    static class Logger
    {
        private static readonly string AppDataPath =
            Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "ThermalBridge");

        private static readonly string LogPath =
            Path.Combine(AppDataPath, "thermalbridge.log");

        private static readonly object _lock = new();
        private const long MaxLogBytes = 5 * 1024 * 1024;

        public static string GetLogPath() => LogPath;

        public static void Log(string message)
        {
            string line = $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss}] {message}";

            try { Console.WriteLine(message); } catch { }

            lock (_lock)
            {
                try
                {
                    RollIfNeeded();
                    File.AppendAllText(LogPath, line + Environment.NewLine);
                }
                catch { }
            }
        }

        private static void RollIfNeeded()
        {
            if (File.Exists(LogPath) && new FileInfo(LogPath).Length > MaxLogBytes)
            {
                string archive = Path.Combine(
                    AppDataPath,
                    $"thermalbridge_{DateTime.Now:yyyyMMdd_HHmmss}.log.old");
                File.Move(LogPath, archive, overwrite: true);
            }
        }
    }

    static class ConsoleManager
    {
        [DllImport("kernel32.dll")]
        private static extern bool AllocConsole();

        [DllImport("kernel32.dll")]
        private static extern bool FreeConsole();

        [DllImport("kernel32.dll")]
        private static extern IntPtr GetConsoleWindow();

        [DllImport("user32.dll")]
        private static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

        [DllImport("kernel32.dll")]
        private static extern bool SetConsoleCtrlHandler(ConsoleCtrlDelegate? handler, bool add);

        private delegate bool ConsoleCtrlDelegate(int sig);

        private static ConsoleCtrlDelegate? _ctrlHandler;

        private const int SW_HIDE = 0;
        private const int SW_SHOW = 5;

        public static void ShowStartupConsole()
        {
            IntPtr handle = GetConsoleWindow();
            if (handle != IntPtr.Zero)
            {
                ShowWindow(handle, SW_SHOW);
                try { Console.Title = "ThermalBridge"; } catch { }
                return;
            }

            AllocConsole();
            try { Console.Title = "ThermalBridge — Starting..."; } catch { }
        }

        public static void HideConsole()
        {
            IntPtr handle = GetConsoleWindow();
            if (handle != IntPtr.Zero)
                ShowWindow(handle, SW_HIDE);
        }

        private static void PreventConsoleKill()
        {
            _ctrlHandler = (sig) =>
            {
                Thread dialogThread = new Thread(() =>
                {
                    var result = MessageBox.Show(
                        "Do you want to keep ThermalBridge running in the background?\n\n" +
                        "Yes — hide console, continue running from tray\n" +
                        "No — exit ThermalBridge completely",
                        "ThermalBridge",
                        MessageBoxButtons.YesNo,
                        MessageBoxIcon.Question);

                    if (result == DialogResult.Yes)
                        HideConsole();
                    else
                        Program.RequestShutdown();
                });
                dialogThread.SetApartmentState(ApartmentState.STA);
                dialogThread.IsBackground = true;
                dialogThread.Start();

                return true;
            };
            SetConsoleCtrlHandler(_ctrlHandler, true);
        }

        public static void RegisterCloseHandler()
        {
            PreventConsoleKill();
        }
    }

    enum ConnectionState
    {
        Initializing,
        Scanning,
        Connected,
        Reconnecting
    }

    class Program
    {
        const string EXPECTED_REPLY = "THERMALBRIDGE";
        const float SENSOR_MISSING = -1f;
        const int BAUD_RATE = 115200;

        static readonly string ConfigPath =
            Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "ThermalBridge",
                "lastcomport.txt");

        internal static volatile bool _shuttingDown = false;
        private static volatile ConnectionState _state = ConnectionState.Initializing;

        internal static NotifyIcon? _trayIcon;
        private static SensorReader? _reader;

        private static readonly string StartupShortcutPath =
    Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.Startup),
        "ThermalBridge.lnk");

        private static bool IsInStartup() => File.Exists(StartupShortcutPath);

        private static void AddToStartup()
        {
            try
            {
                string exePath = Environment.ProcessPath!;
                string script = $@"
            $ws = New-Object -ComObject WScript.Shell
            $s = $ws.CreateShortcut('{StartupShortcutPath}')
            $s.TargetPath = '{exePath}'
            $s.Save()
        ";
                var psi = new System.Diagnostics.ProcessStartInfo
                {
                    FileName = "powershell",
                    Arguments = $"-NoProfile -Command \"{script}\"",
                    CreateNoWindow = true,
                    UseShellExecute = false
                };
                System.Diagnostics.Process.Start(psi)?.WaitForExit();
                Logger.Log("[STARTUP] Added to Windows startup.");
            }
            catch (Exception ex)
            {
                Logger.Log($"[STARTUP] Failed to add to startup: {ex.Message}");
            }
        }

        private static void RemoveFromStartup()
        {
            try
            {
                if (File.Exists(StartupShortcutPath))
                    File.Delete(StartupShortcutPath);
                Logger.Log("[STARTUP] Removed from Windows startup.");
            }
            catch (Exception ex)
            {
                Logger.Log($"[STARTUP] Failed to remove from startup: {ex.Message}");
            }
        }

        public static void RequestShutdown()
        {
            Logger.Log("[EXIT] Shutdown requested from console close.");
            _shuttingDown = true;

            // Marshal to UI thread via ThreadPool since NotifyIcon has no Invoke
            ThreadPool.QueueUserWorkItem(_ =>
            {
                try
                {
                    if (_trayIcon != null)
                        _trayIcon.Visible = false;
                    Application.Exit();
                }
                catch { }
            });
        }

        static SerialPort? TryOpenAndVerify(string portName, int baudRate)
        {
            SerialPort? port = null;
            try
            {
                port = new SerialPort(portName, baudRate)
                {
                    ReadTimeout = 2000,
                    WriteTimeout = 2000
                };

                port.Open();
                Thread.Sleep(800);

                port.DiscardInBuffer();
                port.DiscardOutBuffer();
                port.WriteLine("PING");

                string reply = port.ReadLine().Trim();

                if (reply == EXPECTED_REPLY)
                    return port;

                port.Close();
                return null;
            }
            catch
            {
                try { port?.Close(); } catch { }
                return null;
            }
        }

        static SerialPort FindAndConnect(int baudRate)
        {
            _state = ConnectionState.Scanning;
            UpdateTrayStatus();

            if (File.Exists(ConfigPath))
            {
                string lastPort = File.ReadAllText(ConfigPath).Trim();
                if (!string.IsNullOrWhiteSpace(lastPort))
                {
                    Logger.Log($"[DETECT] Trying last known port {lastPort}...");
                    var port = TryOpenAndVerify(lastPort, baudRate);
                    if (port != null)
                    {
                        Logger.Log($"[DETECT] Confirmed and connected on {lastPort}.");
                        return port;
                    }
                    Logger.Log("[DETECT] Last known port did not respond. Scanning...");
                }
            }

            int retryDelayMs = 2000;
            const int maxRetryDelayMs = 30000;

            while (!_shuttingDown)
            {
                string[] ports = SerialPort.GetPortNames();

                if (ports.Length == 0)
                {
                    Logger.Log("[DETECT] No COM ports detected. Bluetooth may be off, " +
                               "or Smart Laptop Cooler is not paired/powered. Retrying...");
                    Thread.Sleep(retryDelayMs);
                    retryDelayMs = Math.Min(retryDelayMs * 2, maxRetryDelayMs);
                    continue;
                }

                Logger.Log($"[DETECT] Searching for Smart Laptop Cooler across {ports.Length} port(s)...");

                foreach (string portName in ports)
                {
                    if (_shuttingDown) break;

                    Logger.Log($"[DETECT] Checking {portName}...");
                    var port = TryOpenAndVerify(portName, baudRate);
                    if (port != null)
                    {
                        Logger.Log($"[DETECT] Found and connected to Smart Laptop Cooler on {portName}");
                        try { File.WriteAllText(ConfigPath, portName); } catch { }
                        return port;
                    }
                }

                Logger.Log($"[DETECT] Not found. Retrying in {retryDelayMs / 1000}s...");
                Thread.Sleep(retryDelayMs);
                retryDelayMs = Math.Min(retryDelayMs * 2, maxRetryDelayMs);
            }

            throw new OperationCanceledException("Shutdown requested during port discovery.");
        }

        static void PrintBanner()
        {
            Logger.Log("========================================================");
            Logger.Log("                     ThermalBridge");
            Logger.Log("        Windows Companion for Smart Laptop Cooler");
            Logger.Log("========================================================");
            Logger.Log("Developed by Hansoy");
            Logger.Log("GitHub: https://github.com/Hans930v");
            Logger.Log("");
            Logger.Log("This software uses LibreHardwareMonitor");
            Logger.Log("Copyright (c) LibreHardwareMonitor Contributors");
            Logger.Log("Licensed under the Mozilla Public License 2.0");
            Logger.Log("https://github.com/LibreHardwareMonitor/LibreHardwareMonitor");
            Logger.Log("");
            Logger.Log("ThermalBridge Copyright (c) 2026 Hansoy");
            Logger.Log("Released as part of the Smart Laptop Cooler project.");
            Logger.Log("========================================================");
        }

        static void InitTrayIcon()
        {
            var menu = new ContextMenuStrip();

            var statusItem = new ToolStripMenuItem("Status: Initializing...") { Enabled = false };
            menu.Items.Add(statusItem);
            menu.Items.Add(new ToolStripSeparator());

            var viewLogItem = new ToolStripMenuItem("Open Log File");
            viewLogItem.Click += (_, _) =>
            {
                try
                {
                    System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
                    {
                        FileName = Logger.GetLogPath(),
                        UseShellExecute = true
                    });
                }
                catch (Exception ex)
                {
                    Logger.Log($"[TRAY] Failed to open log file: {ex.Message}");
                }
            };
            menu.Items.Add(viewLogItem);

            var showConsoleItem = new ToolStripMenuItem("Show Console Window");
            showConsoleItem.Click += (_, _) => ConsoleManager.ShowStartupConsole();
            menu.Items.Add(showConsoleItem);

            var startupItem = new ToolStripMenuItem(
                IsInStartup() ? "✓ Run at Windows Startup" : "Run at Windows Startup");
            startupItem.Click += (_, _) =>
            {
                if (IsInStartup())
                {
                    RemoveFromStartup();
                    startupItem.Text = "Run at Windows Startup";
                }
                else
                {
                    AddToStartup();
                    startupItem.Text = "✓ Run at Windows Startup";
                }
            };
            menu.Items.Add(startupItem);

            menu.Items.Add(new ToolStripSeparator());

            var exitItem = new ToolStripMenuItem("Exit ThermalBridge");
            exitItem.Click += (_, _) =>
            {
                Logger.Log("[EXIT] Exit requested from tray menu.");
                _shuttingDown = true;
                _trayIcon!.Visible = false;
                Application.Exit();
            };
            menu.Items.Add(exitItem);

            _trayIcon = new NotifyIcon
            {
                Icon = System.Drawing.Icon.ExtractAssociatedIcon(Environment.ProcessPath!),
                Text = "ThermalBridge — Initializing...",
                Visible = true,
                ContextMenuStrip = menu
            };

            _trayIcon.DoubleClick += (_, _) => ConsoleManager.ShowStartupConsole();
        }

        static void UpdateTrayStatus()
        {
            if (_trayIcon == null) return;

            string text = _state switch
            {
                ConnectionState.Initializing => "ThermalBridge — Initializing...",
                ConnectionState.Scanning => "ThermalBridge — Searching for cooler...",
                ConnectionState.Connected => "ThermalBridge — Connected",
                ConnectionState.Reconnecting => "ThermalBridge — Reconnecting...",
                _ => "ThermalBridge"
            };

            _trayIcon.Text = text;

            if (_trayIcon.ContextMenuStrip?.Items.Count > 0 &&
                _trayIcon.ContextMenuStrip.Items[0] is ToolStripMenuItem statusItem)
            {
                statusItem.Text = $"Status: {text.Replace("ThermalBridge — ", "")}";
            }
        }

        static void RunStreamingWorker()
        {
            Directory.CreateDirectory(
                Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "ThermalBridge"));

            PrintBanner();
            Logger.Log("[INIT] Initializing hardware monitoring...");
            Logger.Log("[INIT] Opening LibreHardwareMonitor (this may take a few seconds)...");

            try
            {
                _reader = new SensorReader();
            }
            catch (Exception ex)
            {
                Logger.Log($"[FATAL] Could not initialize hardware monitor: {ex.Message}");
                Logger.Log("[FATAL] This usually means the process is not running as Administrator.");
                Application.Exit();
                return;
            }

            Thread.Sleep(1000);
            _reader.DumpAllSensors();

            var (cpuTemp, cpuPower, gpuTemp) = _reader.ReadAll();

            Logger.Log("===== FILTERED READ RESULT =====");
            Logger.Log($"CPU Package Temp:  {(cpuTemp.HasValue ? cpuTemp.Value.ToString("F1") + " C" : "NOT FOUND")}");
            Logger.Log($"CPU Package Power: {(cpuPower.HasValue ? cpuPower.Value.ToString("F2") + " W" : "NOT FOUND")}");
            Logger.Log($"GPU Core Temp:     {(gpuTemp.HasValue ? gpuTemp.Value.ToString("F1") + " C" : "NOT FOUND")}");
            Logger.Log("=================================");

            bool anyMissing = !cpuTemp.HasValue || !cpuPower.HasValue || !gpuTemp.HasValue;
            if (anyMissing)
            {
                Logger.Log("[WARN] One or more required sensors were not found.");
                Logger.Log("[WARN] The firmware handles missing sensors gracefully (sent as -1),");
                Logger.Log("[WARN] so streaming will continue using whichever sensors ARE available.");
            }
            else
            {
                Logger.Log("[INIT] All required sensors confirmed.");
            }

            Logger.Log("Please ensure:");
            Logger.Log("  1. Bluetooth is turned ON");
            Logger.Log("  2. Smart Laptop Cooler is powered on and paired to bluetooth");
            Logger.Log("[SCAN] Starting scan automatically...");

            SerialPort? port;
            try
            {
                port = FindAndConnect(BAUD_RATE);
            }
            catch (OperationCanceledException)
            {
                Logger.Log("[EXIT] Shutdown requested before connection established.");
                _reader.Close();
                return;
            }

            _state = ConnectionState.Connected;
            UpdateTrayStatus();
            Logger.Log("[STREAM] Connected. Hiding console window — running from tray now.");
            Logger.Log("[STREAM] Right-click the tray icon for status, log access, or to exit.");

            ConsoleManager.HideConsole();

            try
            {
                while (!_shuttingDown)
                {
                    var (t, p, g) = _reader.ReadAll();

                    string payload = string.Format("{0:F1},{1:F2},{2:F1}\n",
                        t ?? SENSOR_MISSING,
                        p ?? SENSOR_MISSING,
                        g ?? SENSOR_MISSING);

                    try
                    {
                        if (port == null || !port.IsOpen)
                            throw new InvalidOperationException("Serial port closed.");

                        port.Write(payload);
                    }
                    catch (Exception ex)
                    {
                        Logger.Log($"[STREAM] Bluetooth connection lost: {ex.GetType().Name} - {ex.Message}");
                        Logger.Log("[STREAM] Waiting for Smart Laptop Cooler...");

                        _state = ConnectionState.Reconnecting;
                        UpdateTrayStatus();

                        try { port?.Close(); } catch { }

                        try
                        {
                            port = FindAndConnect(BAUD_RATE);
                        }
                        catch (OperationCanceledException)
                        {
                            break;
                        }

                        _state = ConnectionState.Connected;
                        UpdateTrayStatus();
                        Logger.Log("[STREAM] Communication restored.");
                        continue;
                    }

                    Logger.Log($"[STREAM] Sent: {payload.Trim()}");
                    Thread.Sleep(1000);
                }
            }
            catch (Exception ex)
            {
                Logger.Log($"[FATAL] Unhandled error during streaming: {ex.Message}");
            }
            finally
            {
                try { if (port != null && port.IsOpen) port.Close(); } catch { }
                _reader?.Close();
                Logger.Log("[EXIT] ThermalBridge stopped.");
            }
        }

        [STAThread]
        static void Main()
        {
            ApplicationConfiguration.Initialize();

            ConsoleManager.ShowStartupConsole();
            ConsoleManager.RegisterCloseHandler();

            AppDomain.CurrentDomain.ProcessExit += (_, _) => Logger.Log("[EXIT] ThermalBridge process exiting.");

            InitTrayIcon();

            var workerThread = new Thread(RunStreamingWorker)
            {
                IsBackground = true,
                Name = "ThermalBridge-Streaming"
            };
            workerThread.Start();

            Application.Run();
        }
    }
}
