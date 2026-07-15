using Microsoft.Win32;

namespace ValoAI.App.Services;

public static class StartupService
{
    private const string AppName = "ValoAI";
    private const string RegistryKey =
        @"SOFTWARE\Microsoft\Windows\CurrentVersion\Run";

    public static bool IsStartupEnabled()
    {
        using var key = Registry.CurrentUser.OpenSubKey(RegistryKey, false);
        return key?.GetValue(AppName) != null;
    }

    public static void Enable()
    {
        string exePath =
            System.Diagnostics.Process.GetCurrentProcess()
                  .MainModule!.FileName;

        using var key = Registry.CurrentUser.OpenSubKey(RegistryKey, true);
        key?.SetValue(AppName, $"\"{exePath}\" --minimized");
    }

    public static void Disable()
    {
        using var key = Registry.CurrentUser.OpenSubKey(RegistryKey, true);
        key?.DeleteValue(AppName, throwOnMissingValue: false);
    }
}