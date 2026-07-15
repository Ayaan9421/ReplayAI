using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using ValoAI.App.Helpers;
using ValoAI.App.Models;
using ValoAI.App.Services;
using ValoAI.App.Views;
using ValoAI.Core;

namespace ValoAI.App;

public partial class MainWindow : Window
{
    private bool _navExpanded = false;
    private HotkeyManager _hotkey = new();
    private TrayService? _tray;

    public MainWindow()
    {
        InitializeComponent();
        Loaded += OnLoaded;
        Closing += OnClosing;
    }

    private void OnLoaded(object sender, RoutedEventArgs e)
    {
        // Tray
        string iconPath = System.IO.Path.Combine(
            AppDomain.CurrentDomain.BaseDirectory, "Assets", "ValoAI.ico");        
        _tray = new TrayService();

        _tray.OpenRequested += BringToFront;
        _tray.SaveClipRequested += () => RecorderManager.Instance.SaveClip();
        _tray.SettingsRequested += () => {
            BringToFront();
            ContentFrame.Navigate(new SettingsView());
        };
        _tray.ExitRequested += ExitApp;

        // Recorder
        RecorderManager.Instance.StatusChanged += s =>
            Dispatcher.Invoke(() => {
                RecordingLabel.Text = s;
                _tray?.SetStatus(RecorderManager.Instance.IsRunning);
            });

        RecorderManager.Instance.ClipSaved += clip =>
            Dispatcher.Invoke(() =>
                _tray?.ShowBalloon("Clip Saved",
                    System.IO.Path.GetFileNameWithoutExtension(clip.FilePath)));

        Task.Run(() => RecorderManager.Instance.Start(AppSettings.Load()));

        // Hotkey
        _hotkey.Register(this, 0x78, 0x0002 | 0x0004);
        _hotkey.ClipHotkeyPressed += () =>
            RecorderManager.Instance.SaveClip();

        ContentFrame.Navigate(new DashboardView());
    }

    // Close button → minimize to tray, don't close
    private void OnClosing(object? sender,
        System.ComponentModel.CancelEventArgs e)
    {
        e.Cancel = true;
        Hide();
        _tray?.ShowBalloon("ValoAI",
            "Still running in the background. Right-click the tray icon to exit.");
    }

    private void BringToFront()
    {
        Dispatcher.Invoke(() => {
            Show();
            WindowState = WindowState.Normal;
            Activate();
            Focus();
        });
    }

    private void ExitApp()
    {
        _hotkey.Unregister();
        RecorderManager.Instance.Stop();
        _tray?.Dispose();
        System.Windows.Application.Current.Shutdown();
    }

    private void TitleBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        => DragMove();

    private void MinimizeButton_Click(object sender, RoutedEventArgs e)
        => WindowState = WindowState.Minimized;

    // Close button now hides to tray
    private void CloseButton_Click(object sender, RoutedEventArgs e)
    {
        Hide();
        _tray?.ShowBalloon("ValoAI",
            "Still running in the background. Right-click the tray icon to exit.");
    }

    private void Nav_MouseEnter(object sender, System.Windows.Input.MouseEventArgs e) => ExpandNav();
    private void Nav_MouseLeave(object sender, System.Windows.Input.MouseEventArgs e) => CollapseNav();

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

    private string _activeNav = "Dashboard";

    // Map nav name to indicator Y offset (based on 48px button height, 12px top margin)
    private double NavIndicatorY(string nav) => nav switch
    {
        "Dashboard" => 26,
        "Clips" => 74,
        "Settings" => 122,
        _ => 26
    };

    private void NavButton_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not System.Windows.Controls.Button btn) return;
        var tag = btn.Tag?.ToString() ?? "Dashboard";

        // Animate indicator to new position
        var anim = new DoubleAnimation(NavIndicatorY(tag),
            new Duration(TimeSpan.FromMilliseconds(200)))
        {
            EasingFunction = new CubicEase { EasingMode = EasingMode.EaseInOut }
        };
        IndicatorTranslate.BeginAnimation(TranslateTransform.YProperty, anim);

        // Update icon colors
        NavDashboardIcon.Foreground = tag == "Dashboard"
            ? (System.Windows.Media.Brush)FindResource("AccentBrush")
            : (System.Windows.Media.Brush)FindResource("TextMutedBrush");
        NavClipsIcon.Foreground = tag == "Clips"
            ? (System.Windows.Media.Brush)FindResource("AccentBrush")
            : (System.Windows.Media.Brush)FindResource("TextMutedBrush");
        NavSettingsIcon.Foreground = tag == "Settings"
            ? (System.Windows.Media.Brush)FindResource("AccentBrush")
            : (System.Windows.Media.Brush)FindResource("TextMutedBrush");

        _activeNav = tag;

        switch (tag)
        {
            case "Dashboard": ContentFrame.Navigate(new DashboardView()); break;
            case "Clips": ContentFrame.Navigate(new ClipsView()); break;
            case "Settings": ContentFrame.Navigate(new SettingsView()); break;
        }
    }

    private void AnimateNavWidth(double to)
    {
        var anim = new GridLengthAnimation
        {
            From = NavColumn.Width,
            To = new GridLength(to),
            Duration = new Duration(TimeSpan.FromMilliseconds(180)),
            EasingFunction = new CubicEase { EasingMode = EasingMode.EaseOut }
        };
        NavColumn.BeginAnimation(ColumnDefinition.WidthProperty, anim);
    }

    private void AnimateNavLabels(double opacity)
    {
        var dur = new Duration(TimeSpan.FromMilliseconds(140));
        var anim = new DoubleAnimation(opacity, dur);
        NavDashboardLabel.BeginAnimation(OpacityProperty, anim);
        NavClipsLabel.BeginAnimation(OpacityProperty, anim);
        NavSettingsLabel.BeginAnimation(OpacityProperty, anim);
    }

    public void NavigateTo(string page)
    {
        NavButton_Click(page == "Clips" ? NavClips : NavDashboard,
            new System.Windows.RoutedEventArgs());
    }

}