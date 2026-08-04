// AppConstants.cs
// Centralized global constants for Thermal Bridge.
//
// Convention:
//   - Values used by MORE than one subsystem live here as `public const`.
//   - Values used by exactly ONE class stay co-located as `private const`
//     near their owner (Logger, LogViewerForm, TrayIconController).
//   - Two delays are behavior-sensitive (Bluetooth warm-up + telemetry cadence)
//     and carry an explanatory comment so nobody "tidies them up".
using System;
using System.IO;

namespace ThermalBridge
{
    internal static class AppConstants
    {
        // ===== Bluetooth / serial protocol =====
        public const string ExpectedReply = "THERMALBRIDGE";
        public const float SensorMissing = -1f;
        public const int BaudRate = 115200;

        // ===== Timing (milliseconds) =====
        //
        // PortStabilizeMs : delay after SerialPort.Open() before sending PING.
        //                   Bluetooth serial ports have a non-deterministic
        //                   warm-up; shortening this can break the handshake
        //                   on slow devices. DO NOT reduce without testing.
        //
        // StreamIntervalMs: telemetry sample cadence the firmware expects.
        //                   Changing it changes the cooler's update rate.
        public const int PortStabilizeMs = 800;
        public const int StreamIntervalMs = 1000;
        public const int SerialReadTimeoutMs = 2000;
        public const int SerialWriteTimeoutMs = 2000;
        public const int InitialRetryDelayMs = 2000;
        public const int MaxRetryDelayMs = 30000;
        public const int HardwareInitDelayMs = 1000;

        // ===== Post-connect UX =====
        //
        // PostConnectCountdownSec: seconds the log viewer's status strip counts down
        //                          before auto-popping the "keep running in background?"
        //                          prompt. Tunable without code changes elsewhere.

        public const int PostConnectCountdownSec = 10;

        // ===== FileSystem paths =====
        public static readonly string AppDataDir =
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                         "Thermal Bridge");

        public static readonly string ConfigPath = Path.Combine(AppDataDir, "lastcomport.txt");
        public static readonly string LogFileName = "thermalbridge.log";
    }
}
