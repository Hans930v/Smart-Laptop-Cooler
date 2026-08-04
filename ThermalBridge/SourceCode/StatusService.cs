// StatusService.cs
// Inverts the previous push-based update flow.
//
// Before: Program._state changed -> Program.UpdateTrayStatus() reached INTO the
//         tray icon and the viewer to mutate them. Tight, brittle coupling.
//
// After:  StatusService.Raise(...) broadcasts the new state; subscribers
//         (TrayIconController, LogViewerForm's status strip) update themselves.
//         One-directional data flow, no cross-class field writes.

using System;

namespace ThermalBridge
{
    internal static class StatusService
    {
        public static event Action<ConnectionState>? StatusChanged;

        public static void Raise(ConnectionState state)
            => StatusChanged?.Invoke(state);
    }
}
