// AppShutdown.cs
// Centralized shutdown coordination.
//
// Replaces the previous `volatile bool _shuttingDown` field that was accessed
// from 3 different classes + the awkward `ThreadPool.QueueUserWorkItem(_ => Application.Exit())`
// marshal-to-UI-thread hack. Idiomatic modern .NET: a single CancellationTokenSource
// that any subsystem can `Request()` and any worker loop can observe.
using System;
using System.Threading;
using System.Windows.Forms;

namespace ThermalBridge
{
    internal static class AppShutdown
    {
        private static readonly CancellationTokenSource _cts = new();

        public static CancellationToken Token => _cts.Token;
        public static bool IsRequested => _cts.IsCancellationRequested;

        public static void Request()
        {
            if (_cts.IsCancellationRequested) return;
            _cts.Cancel();

            // Application.Exit must run on the UI thread; the streaming worker calls
            // us from a background thread. If a Form/SyncContext exists, post to it;
            // otherwise fall back to a direct call (works if we're already on UI thread).
            if (System.Windows.Forms.Application.MessageLoop && SynchronizationContext.Current == null)
            {
                // Off-thread: post to the UI thread via an open Form's BeginInvoke,
                // or fall through to direct Application.Exit() if no Form is available.
                var mainForm = System.Windows.Forms.Application.OpenForms.Count > 0
                    ? System.Windows.Forms.Application.OpenForms[0]
                    : null;
                if (mainForm != null && mainForm.IsHandleCreated)
                    mainForm.BeginInvoke(new Action(System.Windows.Forms.Application.Exit));
                else
                    System.Windows.Forms.Application.Exit();
            }
            else
            {
                System.Windows.Forms.Application.Exit();
            }
        }
    }
}
