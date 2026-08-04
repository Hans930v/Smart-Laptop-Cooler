// ConfirmExitForm.cs
// Replaces the recurring MessageBox.YesNo close prompt with a small custom
// dialog that offers a "Don't ask again" checkbox. Once checked, the user's
// choice is persisted and future closes skip the prompt entirely.

using System;
using System.IO;
using System.Windows.Forms;

namespace ThermalBridge
{
    internal sealed class ConfirmExitForm : Form
    {
        private static readonly string PreferencePath =
            Path.Combine(AppConstants.AppDataDir, "closeprefs.txt");

        public DialogResult Result { get; private set; } = DialogResult.Cancel;
        public bool SuppressFuture { get; private set; }

        public ConfirmExitForm()
        {
            Text = "Thermal Bridge";
            FormBorderStyle = FormBorderStyle.FixedDialog;
            StartPosition = FormStartPosition.CenterParent;
            MaximizeBox = false;
            MinimizeBox = false;
            Width = 500;
            Height = 250;
            ControlBox = false;
            Icon = System.Drawing.Icon.ExtractAssociatedIcon(Environment.ProcessPath!);

            // Scale the dialog so its label/checkbox/buttons read comfortably on
            // modern DPI displays. Parent font cascades to child controls in WinForms.
            Font = new System.Drawing.Font("Segoe UI", 10F);

            BuildControls();
        }

        private void BuildControls()
        {
            var label = new Label
            {
                Text = "Do you want to keep Thermal Bridge running in the background?\n\n" +
                       "Yes — hide log viewer, continue running from tray\n" +
                       "No  — exit Thermal Bridge completely",
                Left = 15,
                Top = 12,
                Width = 450,           // wider to fit the larger 10pt text without wrapping
                Height = 80
            };

            var suppress = new CheckBox
            {
                Text = "Don't ask again (always keep running)",
                Left = 15,
                Top = 100,
                Width = 450
            };

            // ---- Yes / No as auto-sized, bottom-right anchored buttons ----
            // AutoSize + GrowAndShrink lets the button auto-fit its Font, so the
            // parent Font bump (10pt) is reflected without us hand-tuning Width.
            // Anchor to Bottom|Right keeps them glued to the bottom-right corner
            // even if the dialog resizes on high-DPI displays.

            var yesBtn = new Button
            {
                Text = "Yes",
                DialogResult = DialogResult.Yes,
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                Padding = new Padding(24, 8, 24, 8),
                Anchor = AnchorStyles.Bottom | AnchorStyles.Right
            };

            var noBtn = new Button
            {
                Text = "No",
                DialogResult = DialogResult.No,
                AutoSize = true,
                AutoSizeMode = AutoSizeMode.GrowAndShrink,
                Padding = new Padding(24, 8, 24, 8),
                Anchor = AnchorStyles.Bottom | AnchorStyles.Right
            };

            // Add first so AutoSize has a Font + handle to measure against.
            Controls.AddRange(label, suppress, yesBtn, noBtn);
            AcceptButton = yesBtn;

            // Position the buttons against the bottom-right after they're auto-sized.
            // Hook Load (fires once after the form + children are realized).
            Load += (_, _) =>
            {
                const int margin = 16;
                const int gap = 8;
                noBtn.Left = ClientSize.Width - noBtn.Width - margin;
                noBtn.Top = ClientSize.Height - noBtn.Height - margin;
                yesBtn.Left = noBtn.Left - yesBtn.Width - gap;
                yesBtn.Top = noBtn.Top;
            };

            yesBtn.Click += (_, _) =>
            {
                Result = DialogResult.Yes;
                SuppressFuture = suppress.Checked;
                SavePreference(suppress.Checked, keepRunning: true);
                DialogResult = DialogResult.Yes;
            };

            noBtn.Click += (_, _) =>
            {
                Result = DialogResult.No;
                SuppressFuture = suppress.Checked;
                SavePreference(suppress.Checked, keepRunning: false);
                DialogResult = DialogResult.No;
            };
        }

        // ---- Preference persistence ----
        // Format: "suppress\tyes|no" (single line)

        public static (bool suppress, bool keepRunning) LoadPreference()
        {
            try
            {
                if (!File.Exists(PreferencePath)) return (false, true);
                var line = File.ReadAllText(PreferencePath).Trim();
                var parts = line.Split('\t');
                if (parts.Length == 2 &&
                    bool.TryParse(parts[0], out bool s) &&
                    bool.TryParse(parts[1], out bool k))
                    return (s, k);
            }
            catch { }
            return (false, true);
        }

        private static void SavePreference(bool suppress, bool keepRunning)
        {
            try
            {
                Directory.CreateDirectory(AppConstants.AppDataDir);
                File.WriteAllText(PreferencePath, $"{suppress}\t{keepRunning}");
            }
            catch { }
        }

        /// <summary>
        /// If the user previously chose to suppress the prompt, returns the
        /// cached choice directly; otherwise pops the dialog modally.
        /// </summary>
        public static DialogResult AskOrUseCached(IWin32Window owner)
        {
            var (suppress, keepRunning) = LoadPreference();
            if (suppress)
                return keepRunning ? DialogResult.Yes : DialogResult.No;

            using var dlg = new ConfirmExitForm();
            dlg.ShowDialog(owner);
            return dlg.Result;
        }
    }
}
