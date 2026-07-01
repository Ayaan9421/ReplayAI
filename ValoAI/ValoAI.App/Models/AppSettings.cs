using System.IO;
using System.Text.Json;

namespace ValoAI.App.Models;

public class AppSettings
{
    public string ClipsFolder { get; set; } = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),
        "ValoAI", "Clips");

    public int ClipSizeSec { get; set; } = 60;   // 30/60/90/120
    public int TargetFps { get; set; } = 60;
    public string Resolution { get; set; } = "1920x1080";
    public int BitrateMbps { get; set; } = 20;
    public string Encoder { get; set; } = "NVENC";
    public int DisplayIndex { get; set; } = 0;

    public int ClipHotkeyVK { get; set; } = 0x78; // F9
    public int ClipHotkeyMods { get; set; } = 0x06; // CTRL+SHIFT
    public int PTTKey1VK { get; set; } = 0x54; // T
    public int PTTKey2VK { get; set; } = 0x55; // U (secondary, optional)

    public bool MicEnabled { get; set; } = true;
    public string MicDeviceName { get; set; } = "Default";

    private static string FilePath =>
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "ValoAI", "settings.json");

    public static AppSettings Load()
    {
        try
        {
            if (File.Exists(FilePath))
            {
                var json = File.ReadAllText(FilePath);
                var settings = JsonSerializer.Deserialize<AppSettings>(json);
                if (settings != null) return settings;
            }
        }
        catch { /* fall through to defaults */ }
        return new AppSettings();
    }

    public void Save()
    {
        var dir = Path.GetDirectoryName(FilePath)!;
        if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);
        var json = JsonSerializer.Serialize(this,
            new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(FilePath, json);
    }
}