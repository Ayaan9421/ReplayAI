using System.Windows;
using System.Windows.Controls;
using ValoAI.App.Controls;
using ValoAI.App.Services;
using ValoAI.App.Views;

namespace ValoAI.App.Views;

public partial class DashboardView : Page
{
    public DashboardView()
    {
        InitializeComponent();
        Loaded += OnLoaded;

        // Listen for new clips saved during this session
        RecorderManager.Instance.ClipSaved += model =>
        {
            Dispatcher.Invoke(LoadRecentClips);
        };
    }

    private void OnLoaded(object sender, System.Windows.RoutedEventArgs e)
    {
        Task.Run(async () =>
        {
            int waited = 0;
            while(!RecorderManager.Instance.IsRunning && waited < 5000)
            {
                await Task.Delay(100);
                waited += 100;
            }
            Dispatcher.Invoke(LoadRecentClips);
        });
    }

    private void LoadRecentClips()
    {
        ClipGrid.Children.Clear();

        var clips = RecorderManager.Instance
            .GetAllClips()
            .Take(4)
            .ToList();

        foreach (var clip in clips)
        {
            var card = new ClipCard();
            card.SetClip(clip);
            card.PlayRequested += OpenModal;
            ClipGrid.Children.Add(card);
        }
    }

    private void OpenModal(ValoAI.App.Models.ClipModel clip)
    {
        var modal = new ClipPlayerModal(clip)
        {
            Owner = Window.GetWindow(this)
        };
        modal.ShowDialog();
    }
}