// Banner.cs
// Prints the startup banner. LICENSING-SENSITIVE: the LibreHardwareMonitor
// attribution lines (MPL-2.0) are required attribution. Preserve verbatim.

namespace ThermalBridge
{
    internal static class Banner
    {
        public static void Print()
        {
            Logger.Log("========================================================");
            Logger.Log("                    Thermal Bridge");
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
            Logger.Log("Thermal Bridge Copyright (c) 2026 Hansoy");
            Logger.Log("Released as part of the Smart Laptop Cooler project.");
            Logger.Log("========================================================");
        }
    }
}
