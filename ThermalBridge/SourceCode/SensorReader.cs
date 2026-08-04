// SensorReader.cs
// Wraps LibreHardwareMonitor. Acquires CPU Package temp/power + GPU Core temp.
//
// Unchanged behavior from the original Program.cs, just relocated for
// readability. No magic numbers here — only LibreHardwareMonitor's API.
using System;
using LibreHardwareMonitor.Hardware;

namespace ThermalBridge
{
    internal sealed class SensorReader : IVisitor
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
}
