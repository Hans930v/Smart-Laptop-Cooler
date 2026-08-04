// Program.cs
// Entry point. Slim by design — everything else lives in its own file.
//   - AppConstants      : globals (baud, delays, paths, protocol).
//   - AppShutdown       : CancellationToken hub.
//   - StatusService     : one-directional state broadcast.
//   - SensorReader      : LibreHardwareMonitor wrapper.
//   - Logger            : file + ring buffer + OnLog event.
//   - Banner            : startup banner text (licensing-sensitive).
//   - SerialConnection  : Bluetooth handshake + discovery loop.
//   - StreamingWorker   : the background telemetry loop.
//   - TrayIconController: tray icon + context menu.
//   - LogViewerForm     : visible dark log viewer + status strip.
//   - LogViewerFormService : thread-safe single-instance + UI marshalling.
//   - ConfirmExitForm   : "Don't ask again" close prompt.
//
// Deployment: dotnet publish -c Release -r win-x64 --self-contained -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true
// Must run as Administrator (required for hardware sensor access).
using System;
using System.Threading;
using System.Windows.Forms;

namespace ThermalBridge
{
    internal static class Program
    {
        private static TrayIconController? _tray;

        [STAThread]
        private static void Main()
        {
            ApplicationConfiguration.Initialize();

            AppDomain.CurrentDomain.ProcessExit += (_, _) => Logger.Log("[EXIT] Thermal Bridge process exiting.");

            // Tray icon subscribes to StatusService internally so it self-updates.
            _tray = new TrayIconController();
            _tray.Initialize();

            // Viewer shown on startup. Post-connect hide is driven by the viewer's
            // countdown (LogViewerForm subscribes to StatusService.Connected).
            LogViewerFormService.Show();

            var workerThread = new Thread(StreamingWorker.Run)
            {
                IsBackground = true,
                Name = "Thermal Bridge-Streaming"
            };
            workerThread.Start();

            Application.ApplicationExit += OnApplicationExit;
            Application.Run();
        }

        private static void OnApplicationExit(object? sender, EventArgs e)
        {
            try { _tray?.DisposeAndHide(); } catch { }
        }
    }
}
