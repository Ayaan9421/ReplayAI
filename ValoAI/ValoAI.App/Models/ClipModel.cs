namespace ValoAI.App.Models;

public class ClipModel
{
    public string FilePath { get; set; } = "";
    public string? ThumbnailPath { get; set; }
    public DateTime CreatedAt { get; set; }
    public double DurationSec { get; set; }

    public string DurationDisplay =>
        TimeSpan.FromSeconds(DurationSec).ToString(@"m\:ss");

    public string DateDisplay =>
        CreatedAt.ToString("MMM dd, yyyy  HH:mm");
}