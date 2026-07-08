using ValoAI.Core;
using ValoAI.App.Models;

namespace ValoAI.App.Services;

public class RecorderManager
{
    private static RecorderManager? _instance;
    public static RecorderManager Instance => _instance ??= new();

    private RecorderService? _service;
    private RecorderSettings? _settings;

    public event Action<ClipModel>? ClipSaved;
    public event Action<string>? StatusChanged;

    public bool IsRunning { get; private set; }

    public void Start(AppSettings? appSettings = null)
    {
        System.Diagnostics.Debug.WriteLine("[ValoAI-CS] Start() begin");

        var s = appSettings ?? AppSettings.Load();

        int w = 1920, h = 1080;
        var parts = s.Resolution.Split('x');
        if (parts.Length == 2)
        {
            int.TryParse(parts[0], out w);
            int.TryParse(parts[1], out h);
        }

        _settings = new RecorderSettings
        {
            ClipsFolder = s.ClipsFolder,
            BufferDurationSec = s.ClipSizeSec,
            TargetFps = s.TargetFps,
            Width = w,
            Height = h,
            BitrateMbps = s.BitrateMbps,
            ClipHotkeyVK = s.ClipHotkeyVK,
            ClipHotkeyMods = s.ClipHotkeyMods,
            PTTKey1VK = s.PTTKey1VK,
            MicEnabled = s.MicEnabled,
            MicDeviceName = s.MicDeviceName,
            DisplayIndex = s.DisplayIndex,
        };

        System.Diagnostics.Debug.WriteLine(
        $"[ValoAI-CS] Start() — {w}x{h} @ {s.TargetFps}fps, " +
        $"{s.BitrateMbps}Mbps, folder={s.ClipsFolder}");

        _service = new RecorderService();

        _service.ClipSaved += OnClipSaved;
        _service.StatusChanged += s => StatusChanged?.Invoke(s);

        bool ok = _service.Initialize(_settings);
        IsRunning = ok;
        System.Diagnostics.Debug.WriteLine($"[ValoAI-CS] Start() finished, ok={ok}");
        if (!ok) StatusChanged?.Invoke("Failed to start recorder");
    }

    public void Stop()
    {
        _service?.Shutdown();
        IsRunning = false;
    }

    public ClipModel? SaveClip()
    {
        System.Diagnostics.Debug.WriteLine($"[ValoAI-CS] SaveClip called. _service null={_service == null}, IsRunning={IsRunning}");
        if (_service == null || !IsRunning) return null;
        _service.SaveClip();   // fires ClipSaved event async
        return null;
    }

    private void OnClipSaved(ClipInfo info)
    {
        var model = new ClipModel
        {
            FilePath = info.FilePath,
            ThumbnailPath = info.ThumbnailPath,
            CreatedAt = info.CreatedAt,
            DurationSec = info.DurationSec,
        };
        App.Current.Dispatcher.Invoke(() => ClipSaved?.Invoke(model));
    }

    public List<ClipModel> GetAllClips()
    {
        if (_service == null) return [];
        return _service.GetClips()
            .Select(c => new ClipModel
            {
                FilePath = c.FilePath,
                ThumbnailPath = c.ThumbnailPath,
                CreatedAt = c.CreatedAt,
                DurationSec = c.DurationSec,
            })
            .OrderByDescending(c => c.CreatedAt)
            .ToList();
    }
}