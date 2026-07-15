using System.Windows;
using System.Windows.Controls;
using ValoAI.App.Controls;
using ValoAI.App.Models;
using ValoAI.App.Services;
using ValoAI.App.Views;

namespace ValoAI.App.Views;

public partial class ClipsView : Page
{
    private List<ClipModel> _allClips = new();

    public ClipsView()
    {
        InitializeComponent();
        Loaded += OnLoaded;
        SearchBox.GotFocus += (_, _) => SearchPlaceholder.Visibility =
            Visibility.Collapsed;
        SearchBox.LostFocus += (_, _) => {
            if (string.IsNullOrEmpty(SearchBox.Text))
                SearchPlaceholder.Visibility = Visibility.Visible;
        };

        RecorderManager.Instance.ClipSaved += _ =>
            Dispatcher.Invoke(LoadClips);
    }

    private void OnLoaded(object sender, RoutedEventArgs e) => LoadClips();

    private void LoadClips()
    {
        _allClips = RecorderManager.Instance.GetAllClips();
        ClipCountLabel.Text = $"{_allClips.Count} clip{(_allClips.Count == 1 ? "" : "s")}";
        RenderClips(_allClips);
    }

    private void RenderClips(List<ClipModel> clips)
    {
        ClipGrid.Children.Clear();

        if (clips.Count == 0)
        {
            EmptyState.Visibility = Visibility.Visible;
            EmptyLabel.Text = string.IsNullOrEmpty(SearchBox.Text)
                ? "No clips yet"
                : "No clips match your search";
            return;
        }

        EmptyState.Visibility = Visibility.Collapsed;

        foreach (var clip in clips)
        {
            var card = new ClipCard();
            card.Width = 260;
            card.SetClip(clip);
            card.PlayRequested += OpenModal;
            ClipGrid.Children.Add(card);
        }
    }

    private void Search_TextChanged(object sender, TextChangedEventArgs e)
    {
        var query = SearchBox.Text.ToLower().Trim();
        var filtered = string.IsNullOrEmpty(query)
            ? _allClips
            : _allClips.Where(c =>
                System.IO.Path.GetFileNameWithoutExtension(c.FilePath)
                    .ToLower().Contains(query) ||
                c.DateDisplay.ToLower().Contains(query)).ToList();

        RenderClips(filtered);
    }

    private void OpenModal(ClipModel clip)
    {
        var modal = new ClipPlayerModal(clip) { Owner = Window.GetWindow(this) };
        modal.ShowDialog();
    }
}