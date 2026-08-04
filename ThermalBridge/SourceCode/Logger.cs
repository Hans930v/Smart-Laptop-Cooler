// Logger.cs
// Appends timestamped lines to %LocalAppData%\Thermal Bridge\thermalbridge.log
// and broadcasts each line via OnLog so subscribers (LogViewerForm) can render.
//
// Phase 6 change: the in-memory ring buffer is the SOLE owner of retention for
// the viewer. Previously LogViewerForm truncated its own TextBox every 200k chars
// (visible flash). Now the viewer just renders whatever the buffer holds, and
// the head is trimmed here in bulk — no flash.
//
// File retention (5 MB roll) unchanged from original behavior.

using System;
using System.Collections.Generic;
using System.IO;

namespace ThermalBridge
{
    internal static class Logger
    {
        // ---- Co-located (Logger-only) constants ----
        private const long MaxLogBytes = 5 * 1024 * 1024; // 5 MB → roll to .old
        private const int MaxBufferLines = 2000;           // bounded ring for viewers

        private static readonly object _lock = new();
        private static readonly Queue<string> _buffer = new();

        // AppDataPath reuses AppConstants.AppDataDir — single source of truth
        // for the LocalAppData\Thermal Bridge folder. Kept as a field here only
        // because RollIfNeeded() composes the archive path against it.
        private static readonly string AppDataPath = AppConstants.AppDataDir;
        private static readonly string LogPath =
            Path.Combine(AppDataPath, AppConstants.LogFileName);

        public static event Action<string>? OnLog;

        public static string GetLogPath() => LogPath;

        /// <summary>
        /// Returns an immutable snapshot of the bounded in-memory ring buffer
        /// of recent log lines. Used by a freshly-opened viewer to rehydrate.
        /// </summary>
        public static string[] GetBufferedLines()
        {
            lock (_lock)
                return _buffer.ToArray();
        }

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

                _buffer.Enqueue(line);
                while (_buffer.Count > MaxBufferLines)
                    _buffer.Dequeue();
            }

            try { OnLog?.Invoke(line); } catch { }
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
}
