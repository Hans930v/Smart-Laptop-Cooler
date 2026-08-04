// StreamingWorker.cs
// Background worker thread: drives hardware sampling -> telemetry -> Bluetooth.
//
// Behavior identical to the original RunStreamingWorker in Program.cs.
// Differences are structural:
//   - All literals now come from AppConstants / co-located consts.
//   - Shutdown is observed via AppShutdown.Token (cancellable waits).
//   - Status updates flow through StatusService, not direct UI pushes.

using System;
using System.IO;
using System.IO.Ports;
using System.Threading;
using System.Windows.Forms;

namespace ThermalBridge
{
    internal static class StreamingWorker
    {
        public static void Run()
        {
            Directory.CreateDirectory(AppConstants.AppDataDir);

            Banner.Print();
            Logger.Log("[INIT] Initializing hardware monitoring...");
            Logger.Log("[INIT] Opening LibreHardwareMonitor (this may take a few seconds)...");

            SensorReader reader;
            try
            {
                reader = new SensorReader();
            }
            catch (Exception ex)
            {
                Logger.Log($"[FATAL] Could not initialize hardware monitor: {ex.Message}");
                Logger.Log("[FATAL] This usually means the process is not running as Administrator.");
                AppShutdown.Request();
                return;
            }

            Thread.Sleep(AppConstants.HardwareInitDelayMs);
            reader.DumpAllSensors();

            var (cpuTemp, cpuPower, gpuTemp) = reader.ReadAll();

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
                port = SerialConnection.FindAndConnect(AppShutdown.Token);
            }
            catch (OperationCanceledException)
            {
                Logger.Log("[EXIT] Shutdown requested before connection established.");
                reader.Close();
                return;
            }

            StatusService.Raise(ConnectionState.Connected);
            Logger.Log("[STREAM] Connected. Log viewer will prompt the user shortly.");
            Logger.Log("[STREAM] Right-click the tray icon for status, log access, or to exit.");

            try
            {
                while (!AppShutdown.Token.IsCancellationRequested)
                {
                    var (t, p, g) = reader.ReadAll();

                    string payload = string.Format("{0:F1},{1:F2},{2:F1}\n",
                        t ?? AppConstants.SensorMissing,
                        p ?? AppConstants.SensorMissing,
                        g ?? AppConstants.SensorMissing);

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

                        StatusService.Raise(ConnectionState.Reconnecting);

                        try { port?.Close(); } catch { }

                        try
                        {
                            port = SerialConnection.FindAndConnect(AppShutdown.Token);
                        }
                        catch (OperationCanceledException)
                        {
                            break;
                        }

                        StatusService.Raise(ConnectionState.Connected);
                        Logger.Log("[STREAM] Communication restored.");
                        continue;
                    }

                    Logger.Log($"[STREAM] Sent: {payload.Trim()}");
                    AppShutdown.Token.WaitHandle.WaitOne(AppConstants.StreamIntervalMs);
                }
            }
            catch (Exception ex)
            {
                Logger.Log($"[FATAL] Unhandled error during streaming: {ex.Message}");
            }
            finally
            {
                try { if (port != null && port.IsOpen) port.Close(); } catch { }
                reader.Close();
                Logger.Log("[EXIT] Thermal Bridge stopped.");
            }
        }
    }
}
