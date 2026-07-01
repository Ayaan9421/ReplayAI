using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using ValoAI.App.Models;

namespace ValoAI.App.Controls;

public partial class ClipCard : UserControl
{
    public event Action<ClipModel>? PlayRequested;

    private ClipModel? _model;

    public ClipCard()
    {
        InitializeComponent();
    }

    public void SetClip(ClipModel model)
    {
        _model = model;

        DurationText.Text = model.DurationDisplay;
        DateText.Text = model.DateDisplay;
        FileNameText.Text = Path.GetFileNameWithoutExtension(model.FilePath);

        if (model.ThumbnailPath != null && File.Exists(model.ThumbnailPath))
        {
            var bmp = new BitmapImage(new Uri(model.ThumbnailPath));
            Thumbnail.Source = bmp;
            Placeholder.Visibility = Visibility.Collapsed;
        }
    }

    private void Card_MouseEnter(object sender, System.Windows.Input.MouseEventArgs e)
    {
        // Animate accent border
        var colorAnim = new ColorAnimation(
            (Color)FindResource("Accent"),
            new Duration(TimeSpan.FromMilliseconds(150)));
        BorderAccent.BeginAnimation(SolidColorBrush.ColorProperty, colorAnim);

        // Fade in hover overlay
        var fadeIn = new DoubleAnimation(1,
            new Duration(TimeSpan.FromMilliseconds(150)));
        HoverOverlay.BeginAnimation(OpacityProperty, fadeIn);
    }

    private void Card_MouseLeave(object sender, System.Windows.Input.MouseEventArgs e)
    {
        var colorAnim = new ColorAnimation(
            (Color)FindResource("BgElevated"),
            new Duration(TimeSpan.FromMilliseconds(200)));
        BorderAccent.BeginAnimation(SolidColorBrush.ColorProperty, colorAnim);

        var fadeOut = new DoubleAnimation(0,
            new Duration(TimeSpan.FromMilliseconds(200)));
        HoverOverlay.BeginAnimation(OpacityProperty, fadeOut);
    }

    private void Card_Click(object sender,
        System.Windows.Input.MouseButtonEventArgs e)
    {
        if (_model != null) PlayRequested?.Invoke(_model);
    }
}