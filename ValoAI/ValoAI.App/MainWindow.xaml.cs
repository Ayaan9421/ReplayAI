using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media.Animation;
using ValoAI.App.Helpers;
using ValoAI.App.Services;
using ValoAI.App.Views;
using ValoAI.Core;

namespace ValoAI.App;

public partial class MainWindow : Window
{
    private bool _navExpanded = false;
    private HotkeyManager _hotkey = new();

    public MainWindow()
    {
        InitializeComponent();
        Loaded += OnLoaded;
        Closed += OnClosed;
        ContentFrame.Navigate(new DashboardView());
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        // Start recorder in background
        RecorderManager.Instance.StatusChanged += s =>
            Dispatcher.Invoke(() => RecordingLabel.Text = s);

        Task.Run(() => RecorderManager.Instance.Start());

        // Register system-wide hotkey Ctrl+Shift+F9
        _hotkey.Register(this, 0x78, 0x0002 | 0x0004);
        _hotkey.ClipHotkeyPressed += () =>
        {
            System.Diagnostics.Debug.WriteLine($"[ValoAI-CS] Hotkey fired. IsRunning={RecorderManager.Instance.IsRunning}");
            RecorderManager.Instance.SaveClip();
        };
    }

    private void OnClosed(object? sender, EventArgs e)
    {
        _hotkey.Unregister();
        RecorderManager.Instance.Stop();
    }

    private void TitleBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        => DragMove();

    private void MinimizeButton_Click(object sender, RoutedEventArgs e)
        => WindowState = WindowState.Minimized;

    private void CloseButton_Click(object sender, RoutedEventArgs e)
        => Application.Current.Shutdown();

    private void NavButton_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not System.Windows.Controls.Button btn) return;
        switch (btn.Tag?.ToString())
        {
            case "Dashboard": ContentFrame.Navigate(new DashboardView()); break;
            case "Clips": ContentFrame.Navigate(new ClipsView()); break;
            case "Settings": ContentFrame.Navigate(new SettingsView()); break;
        }
    }

    private void Nav_MouseEnter(object sender, MouseEventArgs e) => ExpandNav();
    private void Nav_MouseLeave(object sender, MouseEventArgs e) => CollapseNav();

    private void ExpandNav()
    {
        if (_navExpanded) return;
        _navExpanded = true;
        AnimateNavWidth(200);
        AnimateNavLabels(1.0);
    }

    private void CollapseNav()
    {
        if (!_navExpanded) return;
        _navExpanded = false;
        AnimateNavWidth(64);
        AnimateNavLabels(0.0);
    }

    private void AnimateNavWidth(double to)
    {
        var anim = new GridLengthAnimation
        {
            From = NavColumn.Width,
            To = new GridLength(to),
            Duration = new Duration(TimeSpan.FromMilliseconds(220)),
            EasingFunction = new CubicEase { EasingMode = EasingMode.EaseInOut }
        };
        NavColumn.BeginAnimation(ColumnDefinition.WidthProperty, anim);
    }

    private void AnimateNavLabels(double opacity)
    {
        var anim = new DoubleAnimation(opacity,
            new Duration(TimeSpan.FromMilliseconds(180)));
        NavDashboardLabel.BeginAnimation(OpacityProperty, anim);
        NavClipsLabel.BeginAnimation(OpacityProperty, anim);
        NavSettingsLabel.BeginAnimation(OpacityProperty, anim);
    }
}