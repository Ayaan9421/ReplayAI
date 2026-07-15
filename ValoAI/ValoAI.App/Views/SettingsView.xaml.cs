using Microsoft.Win32;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using ValoAI.App.Controls;
using ValoAI.App.Helpers;
using ValoAI.App.Models;
using ValoAI.App.Services;

namespace ValoAI.App.Views;

public partial class SettingsView : Page
{
    private AppSettings _settings = AppSettings.Load();

    // ── Win32 for display enumeration ──────────────────────────────
    [DllImport("user32.dll")]
    static extern bool EnumDisplayDevices(string? lpDevice, uint iDevNum,
        ref DISPLAY_DEVICE lpDisplayDevice, uint dwFlags);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    struct DISPLAY_DEVICE
    {
        public uint cb;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string DeviceName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string DeviceString;
        public uint StateFlags;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string DeviceID;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string DeviceKey;
    }

    // ── WASAPI mic enumeration ─────────────────────────────────────
    [DllImport("ole32.dll")]
    static extern int CoCreateInstance(ref Guid rclsid, IntPtr pUnkOuter,
        uint dwClsContext, ref Guid riid, out IntPtr ppv);

    public SettingsView()
    {
        InitializeComponent();
        Loaded += OnLoaded;
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        LoadDisplays();
        LoadMicDevices();
        PopulateFromSettings();

        ClipHotkeyCapture.HotkeyChanged += (vk, mods) =>
        {
            _settings.ClipHotkeyVK = vk;
            _settings.ClipHotkeyMods = mods;
        };

        PTTHotkeyCapture.HotkeyChanged += (vk, mods) =>
        {
            _settings.PTTKey1VK = vk;
        };
    }

    // ── Populate UI from saved settings ───────────────────────────
    private void PopulateFromSettings()
    {
        // Clips folder
        ClipsFolderText.Text = _settings.ClipsFolder;

        // Clip size
        SelectComboByTag(ClipSizeCombo, _settings.ClipSizeSec.ToString());

        // FPS
        SelectComboByTag(FpsCombo, _settings.TargetFps.ToString());

        // Resolution
        SelectComboByTag(ResolutionCombo, _settings.Resolution);

        // Bitrate
        BitrateSlider.Value = _settings.BitrateMbps;
        BitrateLabel.Text = $"{_settings.BitrateMbps} Mbps";

        // Display
        for (int i = 0; i < DisplayCombo.Items.Count; i++)
        {
            if (DisplayCombo.Items[i] is ComboBoxItem item &&
                item.Tag?.ToString() == _settings.DisplayIndex.ToString())
            {
                DisplayCombo.SelectedIndex = i;
                break;
            }
        }
        if (DisplayCombo.SelectedIndex < 0 && DisplayCombo.Items.Count > 0)
            DisplayCombo.SelectedIndex = 0;

        // Hotkeys
        ClipHotkeyCapture.SetHotkey(_settings.ClipHotkeyVK, _settings.ClipHotkeyMods);
        PTTHotkeyCapture.SetHotkey(_settings.PTTKey1VK, 0);

        // Mic
        MicEnabledCheck.IsChecked = _settings.MicEnabled;
        SelectComboByContent(MicDeviceCombo, _settings.MicDeviceName);

        // Startup
        StartupCheck.IsChecked = StartupService.IsStartupEnabled();
    }

    // ── Display enumeration via Win32 ─────────────────────────────
    private void LoadDisplays()
    {
        DisplayCombo.Items.Clear();

        uint devNum = 0;
        int displayIndex = 0;

        var dd = new DISPLAY_DEVICE();
        dd.cb = (uint)Marshal.SizeOf(dd);

        while (EnumDisplayDevices(null, devNum, ref dd, 0))
        {
            // Only active displays (DISPLAY_DEVICE_ACTIVE = 0x1)
            if ((dd.StateFlags & 0x1) != 0)
            {
                var item = new ComboBoxItem
                {
                    Content = $"Display {displayIndex + 1} — {dd.DeviceString.Trim()}",
                    Tag = displayIndex.ToString()
                };
                DisplayCombo.Items.Add(item);
                displayIndex++;
            }
            devNum++;
            dd = new DISPLAY_DEVICE();
            dd.cb = (uint)Marshal.SizeOf(dd);
        }

        if (DisplayCombo.Items.Count == 0)
        {
            DisplayCombo.Items.Add(new ComboBoxItem
            { Content = "Primary Display", Tag = "0" });
        }

        DisplayCombo.SelectedIndex = 0;
    }

    // ── Mic enumeration via MMDevice ──────────────────────────────
    private void LoadMicDevices()
    {
        MicDeviceCombo.Items.Clear();
        MicDeviceCombo.Items.Add(new ComboBoxItem
        { Content = "Default", Tag = "Default" });

        try
        {
            // Use NAudio-free approach — WMI or Shell enumeration
            // Since we don't have NAudio, use a lightweight COM approach
            var enumerator = new MMDeviceEnumeratorWrapper();
            var devices = enumerator.GetCaptureDevices();

            foreach (var (id, name) in devices)
            {
                MicDeviceCombo.Items.Add(new ComboBoxItem
                { Content = name, Tag = id });
            }
        }
        catch
        {
            // Fallback — just show Default
        }

        MicDeviceCombo.SelectedIndex = 0;
    }

    // ── Events ────────────────────────────────────────────────────
    private void BrowseFolder_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new System.Windows.Forms.FolderBrowserDialog
        {
            Description = "Select clips folder",
            SelectedPath = _settings.ClipsFolder,
            ShowNewFolderButton = true
        };

        if (dialog.ShowDialog() == System.Windows.Forms.DialogResult.OK)
        {
            _settings.ClipsFolder = dialog.SelectedPath;
            ClipsFolderText.Text = dialog.SelectedPath;
        }
    }

    private void BitrateSlider_ValueChanged(object sender,
        System.Windows.RoutedPropertyChangedEventArgs<double> e)
    {
        if (BitrateLabel == null) return;
        int val = (int)BitrateSlider.Value;
        BitrateLabel.Text = $"{val} Mbps";
        _settings.BitrateMbps = val;
    }

    private void Save_Click(object sender, RoutedEventArgs e)
    {
        // Collect all values from UI into _settings
        if (ClipSizeCombo.SelectedItem is ComboBoxItem clipItem)
            _settings.ClipSizeSec = int.Parse(clipItem.Tag!.ToString()!);

        if (FpsCombo.SelectedItem is ComboBoxItem fpsItem)
            _settings.TargetFps = int.Parse(fpsItem.Tag!.ToString()!);

        if (ResolutionCombo.SelectedItem is ComboBoxItem resItem)
            _settings.Resolution = resItem.Tag!.ToString()!;

        if (DisplayCombo.SelectedItem is ComboBoxItem dispItem &&
            int.TryParse(dispItem.Tag?.ToString(), out int dispIdx))
            _settings.DisplayIndex = dispIdx;

        if (MicDeviceCombo.SelectedItem is ComboBoxItem micItem)
            _settings.MicDeviceName = micItem.Tag?.ToString() ?? "Default";

        _settings.MicEnabled = MicEnabledCheck.IsChecked == true;


        if (StartupCheck.IsChecked == true)
            StartupService.Enable();
        else
            StartupService.Disable();

        _settings.Save();

        SaveStatusText.Text = "Saved. Restart required to apply changes.";

        // Offer restart
        var result = System.Windows.MessageBox.Show(
            "Settings saved. Restart ValoAI to apply changes?",
            "ValoAI",
            MessageBoxButton.YesNo,
            MessageBoxImage.Question);

        if (result == MessageBoxResult.Yes)
        {
            var exe = System.Diagnostics.Process.GetCurrentProcess().MainModule!.FileName;
            System.Diagnostics.Process.Start(exe);
            System.Windows.Application.Current.Shutdown();
        }
    }

    // ── Helpers ───────────────────────────────────────────────────
    private static void SelectComboByTag(System.Windows.Controls.ComboBox combo,
        string tag)
    {
        foreach (ComboBoxItem item in combo.Items)
            if (item.Tag?.ToString() == tag)
            {
                combo.SelectedItem = item;
                return;
            }
        if (combo.Items.Count > 0)
            combo.SelectedIndex = 0;
    }

    private static void SelectComboByContent(System.Windows.Controls.ComboBox combo,
        string content)
    {
        foreach (ComboBoxItem item in combo.Items)
            if (item.Content?.ToString() == content)
            {
                combo.SelectedItem = item;
                return;
            }
        if (combo.Items.Count > 0)
            combo.SelectedIndex = 0;
    }
}