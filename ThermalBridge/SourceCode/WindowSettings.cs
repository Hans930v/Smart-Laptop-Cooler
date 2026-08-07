// WindowSettings.cs
// Persists the log viewer's window location, size, and state across launches
// so the user doesn't have to reposition it every time. Stored as a tiny
// INI-style text file in AppDataDir (one line per setting) — no JSON
// dependency, no app.config, no schema drift.

using System;
using System.IO;
using System.Windows.Forms;

namespace ThermalBridge
{
    internal static class WindowSettings
    {
        private static readonly string FilePath =
            Path.Combine(AppConstants.AppDataDir, "viewerstate.txt");

        private const string Sentinel = "state=";
        private const string WidthKey  = "width=";
        private const string HeightKey = "height=";
        private const string LeftKey   = "left=";
        private const string TopKey    = "top=";

        /// <summary>
        /// Loads settings from disk, applies them to the form. If anything is
        /// missing / unparseable / off-screen, leaves the form's current
        /// (designer-default) values alone.
        /// </summary>
        public static void ApplyTo(Form form)
        {
            try
            {
                if (!File.Exists(FilePath)) return;

                int? w = null, h = null, l = null, t = null;
                FormWindowState? state = null;

                foreach (var raw in File.ReadAllLines(FilePath))
                {
                    var line = raw.Trim();
                    if (line.Length == 0 || line.StartsWith("#")) continue;

                    if (line.StartsWith(Sentinel) &&
                        Enum.TryParse(line[Sentinel.Length..], out FormWindowState s))
                        state = s;
                    else if (line.StartsWith(WidthKey)  && int.TryParse(line[WidthKey.Length..],  out int wv))  w = wv;
                    else if (line.StartsWith(HeightKey) && int.TryParse(line[HeightKey.Length..], out int hv))  h = hv;
                    else if (line.StartsWith(LeftKey)   && int.TryParse(line[LeftKey.Length..],   out int lv))  l = lv;
                    else if (line.StartsWith(TopKey)    && int.TryParse(line[TopKey.Length..],    out int tv))  t = tv;
                }

                if (w.HasValue) form.Width  = w.Value;
                if (h.HasValue) form.Height = h.Value;
                if (l.HasValue && t.HasValue && IsOnScreen(l.Value, t.Value))
                {
                    form.StartPosition = FormStartPosition.Manual;
                    form.Left = l.Value;
                    form.Top  = t.Value;
                }
                if (state.HasValue) form.WindowState = state.Value;
            }
            catch { /* never let settings failure prevent the app from running */ }
        }

        /// <summary>Saves the form's current state to disk.</summary>
        public static void SaveFrom(Form form)
        {
            try
            {
                Directory.CreateDirectory(AppConstants.AppDataDir);

                // When maximized / minimized, Location/Size reflect the "restored"
                // bounds only on some Windows versions. Capture RestoreBounds
                // (the user's chosen size) so the next launch matches their
                // intent — not the maximized geometry.
                int left, top, width, height;
                if (form.WindowState == FormWindowState.Normal)
                {
                    left   = form.Left;
                    top    = form.Top;
                    width  = form.Width;
                    height = form.Height;
                }
                else
                {
                    left   = form.RestoreBounds.X;
                    top    = form.RestoreBounds.Y;
                    width  = form.RestoreBounds.Width;
                    height = form.RestoreBounds.Height;
                }

                File.WriteAllText(FilePath,
                    $"{Sentinel}{form.WindowState}\n" +
                    $"{WidthKey}{width}\n" +
                    $"{HeightKey}{height}\n" +
                    $"{LeftKey}{left}\n" +
                    $"{TopKey}{top}\n");
            }
            catch (Exception ex)
            {
                Logger.Log($"[SETTINGS] Failed to save window state: {ex.Message}");
            }
        }

        /// <summary>
        /// True if the point is visible on at least one screen — prevents restoring
        /// a window to a disconnected second monitor where the user can't reach it.
        /// </summary>
        private static bool IsOnScreen(int x, int y)
        {
            foreach (var screen in Screen.AllScreens)
                if (screen.Bounds.Contains(x, y)) return true;
            return false;
        }
    }
}
