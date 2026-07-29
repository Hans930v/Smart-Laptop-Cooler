// Program.cs
// Requires: dotnet add package LibreHardwareMonitorLib
//           dotnet add package System.IO.Ports
// Run as Administrator (required for sensor access)

using System;
using System.IO;
using System.IO.Ports;
using System.Threading;
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
            Console.WriteLine("\n===== FULL SENSOR DUMP =====");
            foreach (var hardware in _computer.Hardware)
            {
                hardware.Update();
                Console.WriteLine($"\n[Hardware] {hardware.Name}  (Type: {hardware.HardwareType})");
                foreach (var sensor in hardware.Sensors)
                    Console.WriteLine($"    - {sensor.SensorType,-15} {sensor.Name,-35} {sensor.Value?.ToString("F2") ?? "null"}");

                foreach (var sub in hardware.SubHardware)
                {
                    sub.Update();
                    Console.WriteLine($"  [SubHardware] {sub.Name}  (Type: {sub.HardwareType})");
                    foreach (var sensor in sub.Sensors)
                        Console.WriteLine($"      - {sensor.SensorType,-15} {sensor.Name,-35} {sensor.Value?.ToString("F2") ?? "null"}");
                }
            }
            Console.WriteLine("\n===== END SENSOR DUMP =====\n");
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

    class Program
    {
        const string EXPECTED_REPLY = "THERMALBRIDGE";
        const string CONFIG_PATH = "lastcomport.txt";
        const float SENSOR_MISSING = -1f;

        // Opens a port, pings it, and if verified, KEEPS IT OPEN and returns it.
        // If verification fails, closes the port and returns null.
        // This is the only place a connection is ever opened - no double-connect.
        static SerialPort? TryOpenAndVerify(string portName, int baudRate)
        {
            SerialPort? port = null;
            try
            {
                port = new SerialPort(portName, baudRate);

                port.ReadTimeout = 2000;
                port.WriteTimeout = 2000;

                port.Open();

                Thread.Sleep(800); // let the BT link settle before writing

                port.DiscardInBuffer();
                port.DiscardOutBuffer();

                port.WriteLine("PING");

                string reply = port.ReadLine().Trim();

                if (reply == EXPECTED_REPLY)
                {
                    return port; // keep open - this becomes the real connection
                }

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
            // Fast path: try last known port first
            if (File.Exists(CONFIG_PATH))
            {
                string lastPort = File.ReadAllText(CONFIG_PATH).Trim();
                if (!string.IsNullOrWhiteSpace(lastPort))
                {
                    Console.WriteLine($"[DETECT] Trying last known port {lastPort}...");
                    var port = TryOpenAndVerify(lastPort, baudRate);
                    if (port != null)
                    {
                        Console.WriteLine($"[DETECT] Confirmed and connected on {lastPort}.\n");
                        return port;
                    }
                    Console.WriteLine("[DETECT] Last known port did not respond. Scanning...\n");
                }
            }

            while (true)
            {
                string[] ports = SerialPort.GetPortNames();

                if (ports.Length == 0)
                {
                    Console.WriteLine("No COM ports detected. Bluetooth may be turned off.");
                    Console.WriteLine("Please ensure:");
                    Console.WriteLine("  1. Bluetooth is turned ON");
                    Console.WriteLine("  2. Smart Laptop Cooler is powered on and paired to bluetooth");
                    Console.WriteLine();
                    Console.WriteLine("Press ENTER to restart the scan...");
                    Console.ReadLine();
                    continue;
                }

                Console.WriteLine($"Searching for Smart Laptop Cooler across {ports.Length} port(s)...");

                foreach (string portName in ports)
                {
                    Console.WriteLine($"Checking {portName}...");
                    var port = TryOpenAndVerify(portName, baudRate);
                    if (port != null)
                    {
                        Console.WriteLine($"Found and connected to Smart Laptop Cooler on {portName}\n");
                        File.WriteAllText(CONFIG_PATH, portName);
                        return port;
                    }
                }

                Console.WriteLine("Not found. Retrying in 2 seconds...\n");
                Thread.Sleep(2000);
            }
        }

        static void Main(string[] args)
        {
            Console.Title = "ThermalBridge";

            Console.WriteLine("========================================================");
            Console.WriteLine("                     ThermalBridge");
            Console.WriteLine("        Windows Companion for Smart Laptop Cooler");
            Console.WriteLine("========================================================");
            Console.WriteLine();
            Console.WriteLine("Developed by Hansoy");
            Console.WriteLine("GitHub: https://github.com/Hans930v");
            Console.WriteLine();
            Console.WriteLine("This software uses LibreHardwareMonitor");
            Console.WriteLine("Copyright (c) LibreHardwareMonitor Contributors");
            Console.WriteLine("Licensed under the Mozilla Public License 2.0");
            Console.WriteLine("https://github.com/LibreHardwareMonitor/LibreHardwareMonitor");
            Console.WriteLine();
            Console.WriteLine("ThermalBridge Copyright (c) 2026 Hansoy");
            Console.WriteLine("Released as part of the Smart Laptop Cooler project.");
            Console.WriteLine("========================================================");
            Console.WriteLine();

            Console.WriteLine("Initializing hardware monitoring...");
            Console.WriteLine("Opening LibreHardwareMonitor (this may take a few seconds)...\n");

            var reader = new SensorReader();
            Thread.Sleep(1000);

            reader.DumpAllSensors();

            var (cpuTemp, cpuPower, gpuTemp) = reader.ReadAll();

            Console.WriteLine("===== FILTERED READ RESULT =====");
            Console.WriteLine($"CPU Package Temp:  {(cpuTemp.HasValue ? cpuTemp.Value.ToString("F1") + " C" : "NOT FOUND")}");
            Console.WriteLine($"CPU Package Power: {(cpuPower.HasValue ? cpuPower.Value.ToString("F2") + " W" : "NOT FOUND")}");
            Console.WriteLine($"GPU Core Temp:     {(gpuTemp.HasValue ? gpuTemp.Value.ToString("F1") + " C" : "NOT FOUND")}");
            Console.WriteLine("=================================\n");

            bool anyMissing = !cpuTemp.HasValue || !cpuPower.HasValue || !gpuTemp.HasValue;

            if (anyMissing)
            {
                Console.WriteLine("WARNING: One or more required sensors were not found.");
                Console.WriteLine("The firmware now handles missing sensors gracefully (sent as -1),");
                Console.WriteLine("so streaming will continue using whichever sensors ARE available.");
                Console.WriteLine("Check the sensor dump above if you want to fix the name filters in ReadAll().\n");
                Console.WriteLine("Press ENTER to continue, or Ctrl+C to exit and fix the code.");
                Console.ReadLine();
            }
            else
            {
                Console.WriteLine("All required sensors confirmed.\n");
            }

            Console.WriteLine("Please ensure:");
            Console.WriteLine("  1. Bluetooth is turned ON");
            Console.WriteLine("  2. Smart Laptop Cooler is powered on and paired to bluetooth");
            Console.WriteLine();
            Console.WriteLine("Press ENTER to start scanning...");
            Console.ReadLine();

            int baudRate = 115200;
            SerialPort port = FindAndConnect(baudRate);

            Console.WriteLine("Streaming sensor data. Press Ctrl+C or close the window to stop.\n");

            try
            {
                while (true)
                {
                    var (t, p, g) = reader.ReadAll();

                    // Missing sensors are sent as -1 (SENSOR_MISSING) rather than 0,
                    // so the ESP32 can tell "not found" apart from a real 0.0 reading.
                    string payload = string.Format("{0:F1},{1:F2},{2:F1}\n",
                        t.HasValue ? t.Value : SENSOR_MISSING,
                        p.HasValue ? p.Value : SENSOR_MISSING,
                        g.HasValue ? g.Value : SENSOR_MISSING);

                    try
                    {
                        if (port == null || !port.IsOpen)
                            throw new InvalidOperationException("Serial port closed.");

                        port.Write(payload);
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine();
                        Console.WriteLine($"Bluetooth connection lost: {ex.GetType().Name} - {ex.Message}");
                        Console.WriteLine("Waiting for Smart Laptop Cooler...");

                        try { port?.Close(); } catch { }

                        port = FindAndConnect(baudRate);

                        Console.WriteLine("Communication restored.");
                        continue;
                    }

                    Console.WriteLine($"Sent: {payload.Trim()}");
                    Thread.Sleep(1000);
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Error during streaming: {ex.Message}");
            }
            finally
            {
                if (port != null && port.IsOpen) port.Close();
                reader.Close();
            }
        }
    }
}
