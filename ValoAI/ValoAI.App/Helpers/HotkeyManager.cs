using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;

namespace ValoAI.App.Helpers;

public class HotkeyManager : IDisposable
{
    [DllImport("user32.dll")] static extern bool RegisterHotKey(IntPtr hWnd, int id, uint fsModifiers, uint vk);
    [DllImport("user32.dll")] static extern bool UnregisterHotKey(IntPtr hWnd, int id);

    const int WM_HOTKEY = 0x0312;
    const uint MOD_CONTROL = 0x0002;
    const uint MOD_SHIFT = 0x0004;

    private HwndSource? _source;
    private IntPtr _hwnd;
    private int _id = 9001;

    public event Action? ClipHotkeyPressed;

    public void Register(Window window, uint vk, uint mods)
    {
        _hwnd = new WindowInteropHelper(window).Handle;
        _source = HwndSource.FromHwnd(_hwnd);
        _source.AddHook(WndProc);
        RegisterHotKey(_hwnd, _id, mods, vk);
    }

    public void Unregister()
    {
        UnregisterHotKey(_hwnd, _id);
        _source?.RemoveHook(WndProc);
    }

    private IntPtr WndProc(IntPtr hwnd, int msg, IntPtr wParam,
                           IntPtr lParam, ref bool handled)
    {
        if (msg == WM_HOTKEY && wParam.ToInt32() == _id)
        {
            ClipHotkeyPressed?.Invoke();
            handled = true;
        }
        return IntPtr.Zero;
    }

    public void Dispose() => Unregister();
}