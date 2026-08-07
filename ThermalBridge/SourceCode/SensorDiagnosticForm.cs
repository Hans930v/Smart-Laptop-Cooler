// SensorDiagnosticForm.cs
// Pops a friendly dialog when deep CPU sensors (Package temp/power) are missing
// on first read — the most common cause is the PawnIO kernel-mode driver not
// being installed. We never auto-install the driver (silent kernel-driver
// install at runtime = instant AV red flag); instead we inform the user and
// offer a button that opens the PawnIO download page in their default browser.

using System.Diagnostics;
using System.Windows.Forms;

namespace ThermalBridge
{
    internal sealed class SensorDiagnosticForm : Form
    {
        public SensorDiagnosticForm(string summary)
        {
            Text = "Thermal Bridge — Sensors Unavailable";
            FormBorderStyle = FormBorderStyle.FixedDialog;
            StartPosition = FormStartPosition.CenterParent;
            MaximizeBox = false;
            MinimizeBox = false;
            Width = 540;
            Height = 280;
            ControlBox = true;
            Icon = System.Drawing.Icon.ExtractAssociatedIcon(Environment.ProcessPath!);
            Font = new System.Drawing.Font("Segoe UI", 10F);

            BuildControls(summary);
        }

        private void BuildControls(string summary)
        {
            var label = new Label
            {
                Text =
                    "Some hardware sensors could not be read:\r\n\r\n" +
                    summary + "\r\n\r\n" +
                    "CPU Package temperature and power require the PawnIO kernel-mode driver.\r\n" +
                    "If you haven't installed it yet, click the button below to open the\r\n" +
                    "download page in your browser. Run the PawnIO installer, then restart\r\n" +
                    "Thermal Bridge.",
                Left = 16,
                Top = 14,
                Width = 508,
                Height = 160
            };

            var openBtn = new Button
            {
                Text = "Open PawnIO download page",
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                Padding = new Padding(20, 8, 20, 8),
                Anchor = AnchorStyles.Bottom | AnchorStyles.Left
            };
            openBtn.Left = 16;
            openBtn.Top = ClientSize.Height - openBtn.Height - 16;
            openBtn.Click += (_, _) =>
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
                    Logger.Log($"[DIAG] Could not open PawnIO page: {ex.Message}");
                }
            };

            var continueBtn = new Button
            {
                Text = "Continue anyway",
                DialogResult = DialogResult.OK,
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                Padding = new Padding(20, 8, 20, 8),
                Anchor = AnchorStyles.Bottom | AnchorStyles.Right
            };
            continueBtn.Top = ClientSize.Height - continueBtn.Height - 16;

            // Position Continue after we know openBtn's width (AutoSize).
            Load += (_, _) =>
            {
                const int margin = 16;
                continueBtn.Left = ClientSize.Width - continueBtn.Width - margin;
                openBtn.Left = margin;
                // keep openBtn on the left, continueBtn on the right
                openBtn.Top = continueBtn.Top;
            };

            Controls.AddRange(label, openBtn, continueBtn);
            AcceptButton = continueBtn;
        }
    }
}
