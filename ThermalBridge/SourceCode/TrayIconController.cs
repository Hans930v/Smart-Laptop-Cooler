// TrayIconController.cs
// Owns the system tray NotifyIcon and its context menu.
//
// Readability improvements applied:
//   - `_statusItem` is a named field — no more indexing
//     `ContextMenuStrip.Items[0]` and casting.
//   - Each menu entry is built by a small named factory (~6 lines): one entry
//     per intent, easy to scan.
//   - Status text stored SEPARATELY from tooltip text — eliminates the
//     previous `.Replace("Thermal Bridge — ", "")` string hack.
//   - Subscribes to StatusService so updates flow by event, not push.
//   - `DisposeAndHide()` guards against null instead of using `!` (null-forgiving).

using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Windows.Forms;

namespace ThermalBridge
{
    internal sealed class TrayIconController
    {
        // ---- Co-located (TrayIconController-only) constants ----
        private const string TrayTooltipPrefix = "Thermal Bridge — ";

        private NotifyIcon? _trayIcon;
        private ToolStripMenuItem? _statusItem;
        private bool _disposed;

        // Cached status-so-far so we can update tooltip and menu label together
        // without rewriting both from scratch every time.
        private ConnectionState _lastState = ConnectionState.Initializing;

        // Track which balloons we've already shown this session — most balloons
        // (Connected/Reconnecting) should fire on every transition, but the
        // "Connected for the first time" celebration shouldn't repeat.
        private bool _firstConnectBalloonShown;

        public void Initialize()
        {
            var menu = new ContextMenuStrip();
            _statusItem = BuildStatusItem();
            menu.Items.Add(_statusItem);
            menu.Items.Add(new ToolStripSeparator());
            menu.Items.Add(BuildOpenLogFileItem());
            menu.Items.Add(BuildShowLogViewerItem());
            menu.Items.Add(BuildCopyLogItem());
            menu.Items.Add(BuildResetPreferencesItem());
            menu.Items.Add(new ToolStripSeparator());
            menu.Items.Add(BuildAboutItem());
            menu.Items.Add(new ToolStripSeparator());
            menu.Items.Add(BuildExitItem(this));

            _trayIcon = new NotifyIcon
            {
                Icon = Icon.ExtractAssociatedIcon(Environment.ProcessPath!),
                Visible = true,
                ContextMenuStrip = menu
            };
            _trayIcon.DoubleClick += (_, _) => LogViewerFormService.Show();

            ApplyStatus(_lastState);
            StatusService.StatusChanged += OnStatusChanged;
        }

        private void OnStatusChanged(ConnectionState state)
        {
            if (_disposed || _trayIcon == null) return;
            if (_trayIcon.ContextMenuStrip?.InvokeRequired ?? false)
                _trayIcon.ContextMenuStrip.BeginInvoke(new Action<ConnectionState>(ApplyStatus), state);
            else
                ApplyStatus(state);

            ShowBalloonFor(state);
        }

        /// <summary>
        /// Shows a tray balloon on meaningful state transitions. Not every
        /// transition deserves a balloon — only ones the user actively cares
        /// about: first connect, reconnect, and connection loss.
        /// </summary>
        private void ShowBalloonFor(ConnectionState state)
        {
            if (_disposed || _trayIcon == null) return;
            if (_trayIcon.ContextMenuStrip?.InvokeRequired ?? false)
            {
                _trayIcon.ContextMenuStrip.BeginInvoke(new Action<ConnectionState>(ShowBalloonFor), state);
                return;
            }

            switch (state)
            {
                case ConnectionState.Connected:
                    // Show the "Connected" balloon only on the FIRST connect of
                    // this session — reconnects also fire Connected status, but
                    // spamming balloons on every reconnect dropout gets noisy.
                    if (!_firstConnectBalloonShown)
                    {
                        _firstConnectBalloonShown = true;
                        ShowBalloon("Connected to Smart Laptop Cooler",
                                    "Streaming telemetry. Right-click the tray icon for options.",
                                    ToolTipIcon.Info);
                    }
                    break;

                case ConnectionState.Reconnecting:
                    ShowBalloon("Connection lost",
                                "Searching for the Smart Laptop Cooler...",
                                ToolTipIcon.Warning);
                    break;

                // Initializing and Scanning never balloon — they're noisy and
                // happen at startup when the user might be doing other things.
            }
        }

        private void ShowBalloon(string title, string text, ToolTipIcon icon, int timeoutMs = 3000)
        {
            try
            {
                // NotifyIcon.ShowBalloonTip is a no-op if Visible is false; we
                // already set Visible=true in Initialize(). Timeout on modern
                // Windows is system-minimum (often ~5s) regardless of our value.
                _trayIcon!.ShowBalloonTip(timeoutMs, title, text, icon);
            }
            catch (Exception ex)
            {
                Logger.Log($"[TRAY] Balloon tip failed: {ex.Message}");
            }
        }

        private static ToolStripMenuItem BuildStatusItem()
            => new("Status: Initializing...") { Enabled = false };

        private static ToolStripMenuItem BuildOpenLogFileItem()
        {
            var item = new ToolStripMenuItem("Open Log File");
            item.Click += (_, _) =>
            {
                try
                {
                    Process.Start(new ProcessStartInfo
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
            return item;
        }

        private static ToolStripMenuItem BuildShowLogViewerItem()
        {
            var item = new ToolStripMenuItem("Show Log Viewer");
            item.Click += (_, _) => LogViewerFormService.Show();
            return item;
        }

        private static ToolStripMenuItem BuildCopyLogItem()
        {
            var item = new ToolStripMenuItem("Copy Log to Clipboard");
            item.Click += (_, _) =>
            {
                try
                {
                    var lines = Logger.GetBufferedLines();
                    if (lines.Length == 0) return;
                    Clipboard.SetText(string.Join(Environment.NewLine, lines));
                    Logger.Log($"[TRAY] Copied {lines.Length} log lines to clipboard.");
                }
                catch (Exception ex)
                {
                    Logger.Log($"[TRAY] Copy log failed: {ex.Message}");
                }
            };
            return item;
        }

        private static ToolStripMenuItem BuildResetPreferencesItem()
        {
            var item = new ToolStripMenuItem("Reset Close Preference");
            item.Click += (_, _) =>
            {
                try
                {
                    var path = Path.Combine(AppConstants.AppDataDir, "closeprefs.txt");
                    if (File.Exists(path))
                    {
                        File.Delete(path);
                        Logger.Log("[TRAY] Close preference reset; prompt will reappear on next close/connect.");
                    }
                }
                catch (Exception ex)
                {
                    Logger.Log($"[TRAY] Failed to reset close preference: {ex.Message}");
                }
            };
            return item;
        }

        private static ToolStripMenuItem BuildExitItem(TrayIconController self)
        {
            var item = new ToolStripMenuItem("Exit Thermal Bridge");
            item.Click += (_, _) =>
            {
                Logger.Log("[EXIT] Exit requested from tray menu.");
                self.DisposeAndHide();
                AppShutdown.Request();
            };
            return item;
        }

        public void ApplyStatus(ConnectionState state)
        {
            _lastState = state;
            if (_disposed || _trayIcon == null || _statusItem == null) return;

            string human = state switch
            {
                ConnectionState.Initializing => "Initializing",
                ConnectionState.Scanning => "Searching for cooler",
                ConnectionState.Connected => "Connected",
                ConnectionState.Reconnecting => "Reconnecting",
                _ => state.ToString()
            };

            _trayIcon.Text = TrayTooltipPrefix + human;
            _statusItem.Text = "Status: " + human;
        }

        public void DisposeAndHide()
        {
            if (_disposed) return;
            _disposed = true;
            StatusService.StatusChanged -= OnStatusChanged;

            if (_trayIcon != null)
            {
                try { _trayIcon.Visible = false; } catch { }
                _trayIcon.Dispose();
                _trayIcon = null;
            }
        }
    }
}
