// LogViewerFormService.cs
// Thread-safe single-instance management + UI-thread marshalling for the
// LogViewerForm. Encapsulates what used to be a verbose _instance +
// _instanceLock + ShowInstance/HideInstance/ShowOnInstance dance inside
// the form itself.

using System;
using System.Threading;
using System.Windows.Forms;

namespace ThermalBridge
{
    internal static class LogViewerFormService
    {
        private static LogViewerForm? _instance;
        private static readonly object _lock = new();

        public static void Show()
        {
            LogViewerForm? snapshot;
            lock (_lock)
            {
                if (_instance == null || _instance.IsDisposed)
                    _instance = new LogViewerForm();
                snapshot = _instance;
            }

            if (snapshot.InvokeRequired)
                snapshot.BeginInvoke(new Action(() => ShowOnInstance(snapshot)));
            else
                ShowOnInstance(snapshot);
        }

        private static void ShowOnInstance(LogViewerForm inst)
        {
            if (inst == null || inst.IsDisposed) return;
            if (!inst.Visible)
                inst.Rehydrate();
            inst.Show();
            inst.WindowState = FormWindowState.Normal;
            inst.Activate();
        }

        public static void Hide()
        {
            var inst = _instance;
            if (inst == null || inst.IsDisposed) return;
            if (inst.InvokeRequired)
                inst.BeginInvoke(new Action(inst.Hide));
            else
                inst.Hide();
        }
    }
}
