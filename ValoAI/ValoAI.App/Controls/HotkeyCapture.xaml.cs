using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace ValoAI.App.Controls;

public partial class HotkeyCapture : UserControl
{
    public event Action<int, int>? HotkeyChanged; // (vk, mods)

    private bool _capturing = false;
    private int _vk;
    private int _mods;

    public HotkeyCapture()
    {
        InitializeComponent();
    }

    public void SetHotkey(int vk, int mods)
    {
        _vk = vk;
        _mods = mods;
        DisplayText.Text = FormatHotkey(vk, mods);
    }

    private void Border_Click(object sender, MouseButtonEventArgs e)
    {
        Focus();
        StartCapture();
    }

    private void OnGotFocus(object sender, RoutedEventArgs e) => StartCapture();
    private void OnLostFocus(object sender, RoutedEventArgs e) => StopCapture();

    private void StartCapture()
    {
        _capturing = true;
        DisplayText.Text = "Press a key combo...";
        CaptureBorder.BorderBrush = (System.Windows.Media.Brush)FindResource("AccentBrush");
    }

    private void StopCapture()
    {
        _capturing = false;
        CaptureBorder.BorderBrush = (System.Windows.Media.Brush)FindResource("AccentDimBrush");
        DisplayText.Text = FormatHotkey(_vk, _mods);
    }

    private void OnKeyDown(object sender, KeyEventArgs e)
    {
        if (!_capturing) return;
        e.Handled = true;

        // Ignore pure modifier presses — wait for the actual key
        if (e.Key is Key.LeftCtrl or Key.RightCtrl or
                     Key.LeftShift or Key.RightShift or
                     Key.LeftAlt or Key.RightAlt or
                     Key.System)
            return;

        int mods = 0;
        if (Keyboard.Modifiers.HasFlag(ModifierKeys.Control)) mods |= 0x0002;
        if (Keyboard.Modifiers.HasFlag(ModifierKeys.Alt)) mods |= 0x0001;
        if (Keyboard.Modifiers.HasFlag(ModifierKeys.Shift)) mods |= 0x0004;

        int vk = KeyInterop.VirtualKeyFromKey(e.Key);

        _vk = vk;
        _mods = mods;

        DisplayText.Text = FormatHotkey(vk, mods);
        HotkeyChanged?.Invoke(vk, mods);

        StopCapture();
        Keyboard.ClearFocus();
    }

    private static string FormatHotkey(int vk, int mods)
    {
        var parts = new List<string>();
        if ((mods & 0x0002) != 0) parts.Add("Ctrl");
        if ((mods & 0x0001) != 0) parts.Add("Alt");
        if ((mods & 0x0004) != 0) parts.Add("Shift");

        var key = KeyInterop.KeyFromVirtualKey(vk);
        parts.Add(key.ToString());

        return string.Join(" + ", parts);
    }
}