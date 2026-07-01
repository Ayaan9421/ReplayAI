using System.IO;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media.Animation;
using System.Windows.Threading;
using ValoAI.App.Models;

namespace ValoAI.App.Views;

public partial class ClipPlayerModal : Window
{
    private readonly DispatcherTimer _progressTimer;
    private readonly DispatcherTimer _controlsTimer;
    private bool _isDraggingSeek = false;
    private bool _isPlaying = false;

    public ClipPlayerModal(ClipModel clip)
    {
        InitializeComponent();

        ClipTitle.Text = Path.GetFileNameWithoutExtension(clip.FilePath);
        ClipDate.Text = clip.DateDisplay;

        Player.Source = new Uri(clip.FilePath);
        Player.Volume = VolumeSlider.Value;
        Player.Play();
        _isPlaying = true;

        // Progress timer — updates seek bar every 250ms
        _progressTimer = new DispatcherTimer
        { Interval = TimeSpan.FromMilliseconds(250) };
        _progressTimer.Tick += UpdateProgress;
        _progressTimer.Start();

        // Controls hide timer — hides controls after 2.5s of no mouse movement
        _controlsTimer = new DispatcherTimer
        { Interval = TimeSpan.FromSeconds(2.5) };
        _controlsTimer.Tick += HideControls;
        _controlsTimer.Start();
    }

    private void Player_MediaOpened(object sender, RoutedEventArgs e)
    {
        if (Player.NaturalDuration.HasTimeSpan)
            SeekBar.Maximum = Player.NaturalDuration.TimeSpan.TotalSeconds;
    }

    private void Player_MediaEnded(object sender, RoutedEventArgs e)
    {
        Player.Stop();
        Player.Position = TimeSpan.Zero;
        PlayPauseBtn.Content = "▶";
        _isPlaying = false;
    }

    private void UpdateProgress(object? sender, EventArgs e)
    {
        if (_isDraggingSeek || !Player.NaturalDuration.HasTimeSpan) return;

        SeekBar.Value = Player.Position.TotalSeconds;

        var pos = Player.Position;
        var total = Player.NaturalDuration.TimeSpan;
        TimeDisplay.Text =
            $"{(int)pos.TotalMinutes}:{pos.Seconds:D2} / " +
            $"{(int)total.TotalMinutes}:{total.Seconds:D2}";
    }

    private void SeekBar_DragStarted(object sender, EventArgs e)
        => _isDraggingSeek = true;

    private void SeekBar_DragCompleted(object sender, EventArgs e)
    {
        _isDraggingSeek = false;
        Player.Position = TimeSpan.FromSeconds(SeekBar.Value);
    }

    private void SeekBar_ValueChanged(object sender,
        System.Windows.RoutedPropertyChangedEventArgs<double> e)
    {
        if (_isDraggingSeek)
            Player.Position = TimeSpan.FromSeconds(SeekBar.Value);
    }

    private void PlayPause_Click(object sender, RoutedEventArgs e)
    {
        if (_isPlaying) { Player.Pause(); PlayPauseBtn.Content = "▶"; }
        else { Player.Play(); PlayPauseBtn.Content = "⏸"; }
        _isPlaying = !_isPlaying;
    }

    private void Volume_Changed(object sender,
        System.Windows.RoutedPropertyChangedEventArgs<double> e)
        => Player.Volume = VolumeSlider.Value;

    private void Video_MouseMove(object sender, MouseEventArgs e)
    {
        ShowControls();
        _controlsTimer.Stop();
    }

    private void ShowControls()
    {
        var anim = new DoubleAnimation(1,
            new Duration(TimeSpan.FromMilliseconds(200)));
        ControlsOverlay.BeginAnimation(OpacityProperty, anim);
    }

    private void HideControls(object? sender, EventArgs e)
    {
        var anim = new DoubleAnimation(0,
            new Duration(TimeSpan.FromMilliseconds(400)));
        ControlsOverlay.BeginAnimation(OpacityProperty, anim);
    }

    private void Backdrop_Click(object sender, MouseButtonEventArgs e)
        => CloseModal();

    private void Close_Click(object sender, RoutedEventArgs e)
        => CloseModal();

    private void CloseModal()
    {
        _progressTimer.Stop();
        _controlsTimer.Stop();
        Player.Stop();
        Player.Source = null;
        Close();
    }
}