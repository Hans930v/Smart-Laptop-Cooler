// SerialConnection.cs
// Bluetooth serial discovery + handshake for Smart Laptop Cooler.
//
// Behavior identical to the original TryOpenAndVerify / FindAndConnect in
// Program.cs; relocated and parameterized. Shutdown now observes
// AppShutdown.Token instead of the prior `volatile bool _shuttingDown` flag.

using System;
using System.IO;
using System.IO.Ports;
using System.Threading;

namespace ThermalBridge
{
    internal static class SerialConnection
    {
        /// <summary>
        /// Opens a port, waits PortStabilizeMs for the Bluetooth stack to warm
        /// up, sends PING, and returns the port only if it replies with
        /// AppConstants.ExpectedReply. Returns null on any failure.
        /// </summary>
        public static SerialPort? TryOpenAndVerify(string portName, int baudRate)
        {
            SerialPort? port = null;
            try
            {
                port = new SerialPort(portName, baudRate)
                {
                    ReadTimeout  = AppConstants.SerialReadTimeoutMs,
                    WriteTimeout = AppConstants.SerialWriteTimeoutMs
                };

                port.Open();
                // Bluetooth serial ports have a non-deterministic warm-up.
                // See AppConstants.PortStabilizeMs comment — do not reduce.
                Thread.Sleep(AppConstants.PortStabilizeMs);

                port.DiscardInBuffer();
                port.DiscardOutBuffer();
                port.WriteLine("PING");

                string reply = port.ReadLine().Trim();

                if (reply == AppConstants.ExpectedReply)
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

        /// <summary>
        /// Tries the last known port first (fast path), then enumerates all COM
        /// ports with exponential backoff (capped at MaxRetryDelayMs).
        /// Throws OperationCanceledException if shutdown is requested mid-scan.
        /// </summary>
        public static SerialPort FindAndConnect(CancellationToken cancel)
        {
            StatusService.Raise(ConnectionState.Scanning);

            if (File.Exists(AppConstants.ConfigPath))
            {
                string lastPort = File.ReadAllText(AppConstants.ConfigPath).Trim();
                if (!string.IsNullOrWhiteSpace(lastPort))
                {
                    Logger.Log($"[DETECT] Trying last known port {lastPort}...");
                    var port = TryOpenAndVerify(lastPort, AppConstants.BaudRate);
                    if (port != null)
                    {
                        Logger.Log($"[DETECT] Confirmed and connected on {lastPort}.");
                        return port;
                    }
                    Logger.Log("[DETECT] Last known port did not respond. Scanning...");
                }
            }

            int retryDelayMs = AppConstants.InitialRetryDelayMs;

            while (!cancel.IsCancellationRequested)
            {
                string[] ports = SerialPort.GetPortNames();

                if (ports.Length == 0)
                {
                    Logger.Log("[DETECT] No COM ports detected. Bluetooth may be off, " +
                               "or Smart Laptop Cooler is not paired/powered. Retrying...");
                    SleepInterruptible(retryDelayMs, cancel);
                    retryDelayMs = Math.Min(retryDelayMs * 2, AppConstants.MaxRetryDelayMs);
                    continue;
                }

                Logger.Log($"[DETECT] Searching for Smart Laptop Cooler across {ports.Length} port(s)...");

                foreach (string portName in ports)
                {
                    if (cancel.IsCancellationRequested) break;

                    Logger.Log($"[DETECT] Checking {portName}...");
                    var port = TryOpenAndVerify(portName, AppConstants.BaudRate);
                    if (port != null)
                    {
                        Logger.Log($"[DETECT] Found and connected to Smart Laptop Cooler on {portName}");
                        try { File.WriteAllText(AppConstants.ConfigPath, portName); } catch { }
                        return port;
                    }
                }

                Logger.Log($"[DETECT] Not found. Retrying in {retryDelayMs / 1000}s...");
                SleepInterruptible(retryDelayMs, cancel);
                retryDelayMs = Math.Min(retryDelayMs * 2, AppConstants.MaxRetryDelayMs);
            }

            throw new OperationCanceledException("Shutdown requested during port discovery.");
        }

        private static void SleepInterruptible(int ms, CancellationToken cancel)
        {
            // Cancellable sleep — wakes immediately on shutdown.
            try { cancel.WaitHandle.WaitOne(ms); }
            catch (ObjectDisposedException) { /* shutting down */ }
        }
    }
}
