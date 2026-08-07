// AboutBox.cs
// Small modal dialog with version, author, repo URL, and license/PawnIO
// attribution. Replaces "the user has to scroll the startup banner to read
// licensing info" with a tidy contextual dialog.

using System.Diagnostics;
using System.Reflection;
using System.Windows.Forms;

namespace ThermalBridge
{
    internal sealed class AboutBox : Form
    {
        public AboutBox()
        {
            Text = "About Thermal Bridge";
            FormBorderStyle = FormBorderStyle.FixedDialog;
            StartPosition = FormStartPosition.CenterParent;
            MaximizeBox = false;
            MinimizeBox = false;
            Width = 520;
            Height = 380;
            ControlBox = true;
            Icon = System.Drawing.Icon.ExtractAssociatedIcon(Environment.ProcessPath!);
            Font = new System.Drawing.Font("Segoe UI", 10F);

            BuildControls();
        }

        private void BuildControls()
        {
            var version = Assembly.GetExecutingAssembly().GetName().Version?.ToString() ?? "1.0.0";

            var label = new Label
            {
                Text =
                    "Thermal Bridge" + "\r\n" +
                    $"Version {version}" + "\r\n" +
                    "Windows companion for Smart Laptop Cooler" + "\r\n" +
                    "Streams CPU/GPU sensor data over Bluetooth" + "\r\n\r\n" +
                    "Developed by Hansoy" + "\r\n" +
                    "GitHub: https://github.com/Hansoy" + "\r\n\r\n" +
                    "This software uses LibreHardwareMonitor" + "\r\n" +
                    "Copyright (c) LibreHardwareMonitor Contributors" + "\r\n" +
                    "Licensed under the Mozilla Public License 2.0" + "\r\n" +
                    "https://github.com/LibreHardwareMonitor/LibreHardwareMonitor" + "\r\n\r\n" +
                    "Thermal Bridge Copyright (c) 2026 Hansoy" + "\r\n" +
                    "Released as part of the Smart Laptop Cooler project.",
                Left = 16,
                Top = 14,
                Width = 480,
                Height = 260
            };

            var pawnIoBtn = new Button
            {
                Text = "Get PawnIO Driver",
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                Padding = new Padding(16, 8, 16, 8),
                Anchor = AnchorStyles.Bottom | AnchorStyles.Left,
                Top = 290,
                Left = 16
            };
            pawnIoBtn.Click += (_, _) =>
            {
                try
                {
                    Process.Start(new ProcessStartInfo
                    {
                        FileName = AppConstants.PawnIOPage,
                        UseShellExecute = true
                    });
                }
                catch (System.Exception ex)
                {
                    Logger.Log($"[ABOUT] Could not open PawnIO page: {ex.Message}");
                }
            };

            var okBtn = new Button
            {
                Text = "Close",
                DialogResult = DialogResult.OK,
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                Padding = new Padding(16, 8, 16, 8),
                Anchor = AnchorStyles.Bottom | AnchorStyles.Right,
                Top = 290
            };
            Load += (_, _) =>
            {
                const int margin = 16;
                okBtn.Left = ClientSize.Width - okBtn.Width - margin;
            };

            Controls.AddRange(label, pawnIoBtn, okBtn);
            AcceptButton = okBtn;
        }
    }
}
