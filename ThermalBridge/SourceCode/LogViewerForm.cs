// LogViewerForm.cs
// The visible UI — a dark-themed log viewer with a status strip at the bottom.
//
// Readability improvements applied:
//   - Magic numbers (Consolas 9F, RGB(30,30,30), clip thresholds) promoted
//     to named private constants.
//   - Constructor split into BuildTextBox() / BuildStatusStrip() / WireEvents()
//     so each visual concern is built by one named factory.
//   - The form NO LONGER owns log retention — Logger's ring buffer does. The
//     form just appends deltas received via OnLog. No more 200k-char
//     truncation flash. Phase 6.
//   - Business logic (close prompt + AppShutdown.Request) extracted to a
//     dedicated HandleUserCloseRequest() method.
//   - Subscribes to StatusService so the bottom status strip updates itself
//     via event — no cross-class field writes.

using System;
using System.Drawing;
using System.Windows.Forms;

namespace ThermalBridge
{
    internal sealed class LogViewerForm : Form
    {
        // ---- Co-located (LogViewerForm-only) constants ----
        private const string LogFontFamily = "Consolas";
        private const float LogFontSize = 12F;
        private static readonly Color BackgroundColor = Color.FromArgb(30, 30, 30);
        private static readonly Color ForegroundColor = Color.LightGray;
        private static readonly Color StatusAccent = Color.FromArgb(120, 230, 150);   // soft mint-green
        private const string StatusPrefix = "Status: ";

        // ---- Window dimensions (sized to comfortable-fit 12pt log text + status strip) ----
        private const int FormWidth = 830;
        private const int FormHeight = 560;

        private readonly RichTextBox _textBox;
        private readonly ToolStripStatusLabel _statusLabel;
        private readonly ToolStripButton _cancelCountdownBtn;
        private readonly ToolStripButton _copyAllBtn;
        private System.Windows.Forms.Timer? _countdownTimer;
        private int _secondsLeft;
        private bool _countdownAlreadyShown;   // suppress countdown on reconnects (per session)

        public LogViewerForm()
        {
            Text = "Thermal Bridge — Log Viewer";
            Width = FormWidth;
            Height = FormHeight;
            ShowInTaskbar = true;
            StartPosition = FormStartPosition.CenterScreen;
            Icon = System.Drawing.Icon.ExtractAssociatedIcon(Environment.ProcessPath!);
            FormClosing += LogViewerForm_FormClosing;

            _textBox = BuildTextBox();
            _statusLabel = new ToolStripStatusLabel { Text = StatusPrefix + "Initializing" };
            _cancelCountdownBtn = BuildCancelButton();
            _copyAllBtn = BuildCopyAllButton();

            Controls.Add(_textBox);
            Controls.Add(BuildStatusStrip(_statusLabel, _cancelCountdownBtn, _copyAllBtn));

            WireEvents();
            WindowSettings.ApplyTo(this);
        }

        private static ToolStripButton BuildCopyAllButton()
        {
            var btn = new ToolStripButton("Copy all")
            {
                ForeColor = StatusAccent,
                Padding = new Padding(8, 0, 8, 0)
            };
            return btn;
        }

        private static ToolStripButton BuildCancelButton()
        {
            var btn = new ToolStripButton("Cancel")
            {
                Visible = false,
                ForeColor = StatusAccent,
                Padding = new Padding(8, 0, 8, 0)
            };
            return btn;
        }

        private static RichTextBox BuildTextBox()
        {
            var tb = new RichTextBox
            {
                Dock = DockStyle.Fill,
                ReadOnly = true,
                BorderStyle = BorderStyle.None,
                Font = new Font(LogFontFamily, LogFontSize),
                BackColor = BackgroundColor,
                ForeColor = ForegroundColor
            };
            return tb;
        }

        private static StatusStrip BuildStatusStrip(ToolStripStatusLabel label, ToolStripButton cancelBtn, ToolStripButton copyAllBtn)
        {
            // Soft mint-green accent — visible on the dark (30,30,30) strip, distinct
            // from the LightGray log text so the eye separates "status" from "log".
            label.ForeColor = cancelBtn.ForeColor = copyAllBtn.ForeColor = StatusAccent;

            var strip = new StatusStrip { BackColor = BackgroundColor };
            strip.Items.Add(label);
            // "Copy all" lives on the left next to the status text so it's
            // visually near the "logs" the user is looking at; Cancel lives on
            // the right because it's a dismissive action.
            strip.Items.Add(new ToolStripSeparator());
            strip.Items.Add(copyAllBtn);
            strip.Items.Add(new ToolStripLabel("    ") { Enabled = false });
            strip.Items.Add(new ToolStripSeparator());
            strip.Items.Add(cancelBtn);
            return strip;
        }

        private void WireEvents()
        {
            Logger.OnLog += Logger_OnLog;
            StatusService.StatusChanged += OnStatusChanged;
            _cancelCountdownBtn.Click += CancelCountdown_Click;
            _copyAllBtn.Click += CopyAll_Click;
        }

        private void CopyAll_Click(object? sender, EventArgs e)
        {
            try
            {
                // Use the Logger's bounded ring buffer (2000 lines) rather than
                // the TextBox contents — works even when the form was hidden
                // and the user is re-opening it to grab a copy.
                var lines = Logger.GetBufferedLines();
                if (lines.Length == 0) return;
                Clipboard.SetText(string.Join(Environment.NewLine, lines));
                _statusLabel.Text = StatusPrefix + "Connected — copied " + lines.Length + " lines to clipboard";
            }
            catch (Exception ex)
            {
                Logger.Log($"[VIEWER] Copy all failed: {ex.Message}");
            }
        }

        private void OnStatusChanged(ConnectionState state)
        {
            if (IsDisposed || !IsHandleCreated) return;
            try
            {
                if (InvokeRequired)
                    BeginInvoke(new Action<ConnectionState>(HandleStatusChanged), state);
                else
                    HandleStatusChanged(state);
            }
            catch { }
        }

        private void HandleStatusChanged(ConnectionState state)
        {
            ApplyStatus(state);

            // Connected triggers the post-connect countdown ONCE per app session.
            // Reconnects (cooler dropouts mid-stream) skip it to avoid pestering
            // the user. Non-Connected states cancel any in-flight countdown.
            if (state == ConnectionState.Connected && !_countdownAlreadyShown)
            {
                _countdownAlreadyShown = true;
                StartPostConnectCountdown();
            }
            else if (state != ConnectionState.Connected)
            {
                StopCountdown();
            }
        }

        private void StartPostConnectCountdown()
        {
            StopCountdown();

            // Honor the user's cached "Don't ask again" choice — if they previously
            // chose "yes, always keep running", skip the countdown and prompt entirely.
            // Sharing closeprefs.txt with the close prompt per design decision.
            var (suppress, keepRunning) = ConfirmExitForm.LoadPreference();
            if (suppress && keepRunning)
            {
                LogViewerFormService.Hide();
                return;
            }

            _secondsLeft = AppConstants.PostConnectCountdownSec;
            _cancelCountdownBtn.Visible = true;

            _countdownTimer = new System.Windows.Forms.Timer { Interval = 1000 };
            _countdownTimer.Tick += CountdownTick;
            _countdownTimer.Start();

            UpdateCountdownLabel();
        }

        private void StopCountdown()
        {
            if (_countdownTimer != null)
            {
                _countdownTimer.Stop();
                _countdownTimer.Dispose();
                _countdownTimer = null;
            }
            _cancelCountdownBtn.Visible = false;
        }

        private void CountdownTick(object? sender, EventArgs e)
        {
            _secondsLeft--;
            if (_secondsLeft <= 0)
            {
                StopCountdown();
                ShowClosePrompt();
            }
            else
            {
                UpdateCountdownLabel();
            }
        }

        private void UpdateCountdownLabel()
            => _statusLabel.Text = $"{StatusPrefix}Connected. Closing in {_secondsLeft}s. Cancel to run in background.";

        private void CancelCountdown_Click(object? sender, EventArgs e)
        {
            StopCountdown();
            _statusLabel.Text = StatusPrefix + "Connected";
            LogViewerFormService.Hide();   // per design: Cancel = hide immediately, app keeps running
        }

        private void ShowClosePrompt()
        {
            _statusLabel.Text = StatusPrefix + "Connected";
            var result = ConfirmExitForm.AskOrUseCached(this);
            Logger.Log($"[VIEWER] Post-connect prompt result: {result}");
            if (result == DialogResult.No)
            {
                Logger.Log("[EXIT] Exit requested from post-connect prompt.");
                AppShutdown.Request();
            }
            else
            {
                LogViewerFormService.Hide();
            }
        }

        private void ApplyStatus(ConnectionState state)
        {
            // Don't overwrite the countdown label while counting down.
            if (_countdownTimer != null && state == ConnectionState.Connected) return;
            _statusLabel.Text = StatusPrefix + state;
        }

        private void Logger_OnLog(string line)
        {
            if (IsDisposed || !IsHandleCreated) return;
            try
            {
                if (InvokeRequired)
                    BeginInvoke(new Action<string>(AppendLine), line);
                else
                    AppendLine(line);
            }
            catch { }
        }

        private void AppendLine(string line)
        {
            // Color the line based on its [LEVEL] prefix. The timestamp [yyyy-..]
            // is NOT one of our prefixes, so plain lines (no bracketed tag) keep
            // the default ForegroundColor via the RichTextBox's own BackColor/
            // ForeColor properties set in BuildTextBox().
            Color color = ColorForLine(line);
            string text = line + Environment.NewLine;

            int start = _textBox.TextLength;
            _textBox.AppendText(text);

            // Color the just-appended range via the SelectionColor API — RichTextBox
            // doesn't accept a color argument to AppendText directly.
            _textBox.SelectionStart  = start;
            _textBox.SelectionLength = text.Length;
            _textBox.SelectionColor  = color;

            // Reset selection to end + scroll so the next append + the view stay clean.
            _textBox.SelectionLength = 0;
            _textBox.SelectionStart  = _textBox.TextLength;
            _textBox.ScrollToCaret();
        }

        /// <summary>
        /// Maps a log line's [LEVEL] prefix to a display color. Lines without a
        /// recognized level prefix use the default ForegroundColor (LightGray).
        /// </summary>
        private static Color ColorForLine(string line)
        {
            // Look for "[LEVEL]" anywhere in the line (it always follows the
            // timestamp, e.g. "[2026-08-05 19:32:09] [DETECT] Checking COM6...").
            foreach (var (tag, color) in LevelColors)
            {
                if (line.Contains(tag, StringComparison.Ordinal))
                    return color;
            }
            return ForegroundColor;
        }

        // Co-located color map for log levels. Standard severity palette —
        // distinct hues, all readable on the dark (30,30,30) background.
        private static readonly (string Tag, Color Color)[] LevelColors =
        {
            ("[FATAL]",  Color.FromArgb(255,  90,  90)),   // red   — fatal errors
            ("[EXIT]",   Color.FromArgb(255, 120, 120)),   // pink  — exit-related
            ("[WARN]",   Color.FromArgb(240, 200,  80)),   // yellow — warnings
            ("[DETECT]", Color.FromArgb( 90, 210, 220)),   // cyan  — port discovery
            ("[DIAG]",   Color.FromArgb(220, 140, 230)),   // magenta — diagnostics
            ("[STREAM]", Color.FromArgb(180, 180, 180)),   // neutral gray — high-volume stream
            ("[TRAY]",   Color.FromArgb(170, 200, 255)),   // soft blue — UI actions
            ("[VIEWER]", Color.FromArgb(170, 200, 255)),   // soft blue — UI actions
            ("[INIT]",   Color.FromArgb(170, 220, 170)),   // pale green — initialization
            ("[SCAN]",   Color.FromArgb(170, 220, 170)),   // pale green — scan start
        };

        /// <summary>Rehydrate from Logger's ring buffer (e.g. on second open).</summary>
        public void Rehydrate()
        {
            _textBox.Clear();
            // Color each line by its level prefix, same as live AppendLine, so
            // visual hierarchy is preserved on re-open.
            foreach (var line in Logger.GetBufferedLines())
            {
                Color color = ColorForLine(line);
                string text = line + Environment.NewLine;
                int start = _textBox.TextLength;
                _textBox.AppendText(text);
                _textBox.SelectionStart  = start;
                _textBox.SelectionLength = text.Length;
                _textBox.SelectionColor  = color;
            }
            _textBox.SelectionLength = 0;
            _textBox.SelectionStart  = _textBox.TextLength;
            _textBox.ScrollToCaret();
        }

        private void LogViewerForm_FormClosing(object? sender, FormClosingEventArgs e)
        {
            // Persist window geometry for next launch, regardless of close reason.
            WindowSettings.SaveFrom(this);

            if (AppShutdown.IsRequested) return;

            if (e.CloseReason == CloseReason.UserClosing || e.CloseReason == CloseReason.TaskManagerClosing)
            {
                if (e.CloseReason == CloseReason.TaskManagerClosing) return;
                HandleUserCloseRequest(e);
            }
            else
            {
                Unsubscribe();
            }
        }

        private void HandleUserCloseRequest(FormClosingEventArgs e)
        {
            e.Cancel = true;

            var result = ConfirmExitForm.AskOrUseCached(this);
            Logger.Log($"[VIEWER] Close prompt result: {result}");

            if (result == DialogResult.Yes)
            {
                Hide();
            }
            else
            {
                Logger.Log("[EXIT] Exit requested from log viewer close prompt.");
                AppShutdown.Request();
            }
        }

        private void Unsubscribe()
        {
            Logger.OnLog -= Logger_OnLog;
            StatusService.StatusChanged -= OnStatusChanged;
        }

        protected override void Dispose(bool disposing)
        {
            if (disposing)
            {
                StopCountdown();
                Unsubscribe();
            }
            base.Dispose(disposing);
        }
    }
}
