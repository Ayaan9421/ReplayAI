using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using ValoAI.App.Models;

namespace ValoAI.App.Controls;

public partial class ClipCard : System.Windows.Controls.UserControl
{
    public event Action<ClipModel>? PlayRequested;

    private ClipModel? _model;

    public ClipCard()
    {
        InitializeComponent();
        CardBorder.BorderBrush = new System.Windows.Media.SolidColorBrush((System.Windows.Media.Color)FindResource("Separator"));
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
    private void Card_Click(object sender,
        System.Windows.Input.MouseButtonEventArgs e)
    {
        if (_model != null) PlayRequested?.Invoke(_model);
    }


    private void Card_MouseEnter(object sender,
    System.Windows.Input.MouseEventArgs e)
    {
        var dur = new Duration(TimeSpan.FromMilliseconds(160));
        GlowOverlay.BeginAnimation(OpacityProperty, new DoubleAnimation(1, dur));
        HoverOverlay.BeginAnimation(OpacityProperty, new DoubleAnimation(1, dur));

        // Lift border
        var borderAnim = new System.Windows.Media.Animation.ColorAnimation(
            (System.Windows.Media.Color)FindResource("AccentDim"), dur);
        ((System.Windows.Media.SolidColorBrush)CardBorder.BorderBrush)
            .BeginAnimation(
            System.Windows.Media.SolidColorBrush.ColorProperty, borderAnim);
    }

    private void Card_MouseLeave(object sender,
        System.Windows.Input.MouseEventArgs e)
    {
        var dur = new Duration(TimeSpan.FromMilliseconds(220));
        GlowOverlay.BeginAnimation(OpacityProperty, new DoubleAnimation(0, dur));
        HoverOverlay.BeginAnimation(OpacityProperty, new DoubleAnimation(0, dur));

        var borderAnim = new System.Windows.Media.Animation.ColorAnimation(
            (System.Windows.Media.Color)FindResource("Separator"), dur);
        ((System.Windows.Media.SolidColorBrush)CardBorder.BorderBrush)
            .BeginAnimation(
            System.Windows.Media.SolidColorBrush.ColorProperty, borderAnim);
    }
}