using System.Windows.Forms;
using ValoAI.App.Helpers;

namespace ValoAI.App.Services;

public class TrayService : IDisposable
{
    private NotifyIcon _trayIcon;
    private ContextMenuStrip _menu;

    public event Action? OpenRequested;
    public event Action? SaveClipRequested;
    public event Action? SettingsRequested;
    public event Action? ExitRequested;

    public TrayService()
    {
        // Replace the icon loading block in TrayService constructor:
        

        _menu = new ContextMenuStrip();
        _menu.Renderer = new DarkMenuRenderer();

        var openItem = new ToolStripMenuItem("Open ValoAI");
        var saveItem = new ToolStripMenuItem("Save Clip  Ctrl+Shift+F9");
        var settingsItem = new ToolStripMenuItem("Settings");
        var separator = new ToolStripSeparator();
        var exitItem = new ToolStripMenuItem("Exit");

        openItem.Font = new System.Drawing.Font(openItem.Font, System.Drawing.FontStyle.Bold);
        openItem.Click += (_, _) => OpenRequested?.Invoke();
        saveItem.Click += (_, _) => SaveClipRequested?.Invoke();
        settingsItem.Click += (_, _) => SettingsRequested?.Invoke();
        exitItem.Click += (_, _) => ExitRequested?.Invoke();

        _menu.Items.AddRange(new ToolStripItem[]
            { openItem, saveItem, settingsItem, separator, exitItem });

        _trayIcon = new NotifyIcon
        {
            Text = "ValoAI — Recording",
            Visible = true,
            ContextMenuStrip = _menu,
        };

        // Load icon
        try
        {
            var uri = new Uri("pack://application:,,,/Assets/ValoAI.ico");
            var sri = System.Windows.Application.GetResourceStream(uri);
            if (sri != null)
                _trayIcon?.Icon = new System.Drawing.Icon(sri.Stream);
        }
        catch
        {
            _trayIcon?.Icon = System.Drawing.SystemIcons.Application;
        }

        _trayIcon.DoubleClick += (_, _) => OpenRequested?.Invoke();
    }

    public void SetStatus(bool recording)
    {
        _trayIcon.Text = "ValoAI — Recording";
    }

    public void ShowBalloon(string title, string message)
    {
        _trayIcon.ShowBalloonTip(3000, title, message,
            ToolTipIcon.None);
    }

    public void Dispose()
    {
        _trayIcon.Visible = false;
        _trayIcon.Dispose();
        _menu.Dispose();
    }
}

// Dark themed context menu renderer
public class DarkMenuRenderer : ToolStripProfessionalRenderer
{
    public DarkMenuRenderer() : base(new DarkColorTable()) { }

    protected override void OnRenderItemText(ToolStripItemTextRenderEventArgs e)
    {
        e.TextColor = e.Item.Enabled
            ? System.Drawing.Color.FromArgb(240, 237, 232)
            : System.Drawing.Color.FromArgb(107, 104, 117);
        base.OnRenderItemText(e);
    }
}

public class DarkColorTable : ProfessionalColorTable
{
    static readonly System.Drawing.Color BgBase = System.Drawing.Color.FromArgb(21, 21, 24);
    static readonly System.Drawing.Color BgHover = System.Drawing.Color.FromArgb(30, 30, 35);
    static readonly System.Drawing.Color Accent = System.Drawing.Color.FromArgb(245, 166, 35);
    static readonly System.Drawing.Color Border = System.Drawing.Color.FromArgb(40, 40, 45);

    public override System.Drawing.Color MenuBorder => Border;
    public override System.Drawing.Color MenuItemBorder => System.Drawing.Color.Transparent;
    public override System.Drawing.Color MenuItemSelected => BgHover;
    public override System.Drawing.Color MenuItemSelectedGradientBegin => BgHover;
    public override System.Drawing.Color MenuItemSelectedGradientEnd => BgHover;
    public override System.Drawing.Color MenuItemPressedGradientBegin => Accent;
    public override System.Drawing.Color MenuItemPressedGradientEnd => Accent;
    public override System.Drawing.Color ToolStripDropDownBackground => BgBase;
    public override System.Drawing.Color ImageMarginGradientBegin => BgBase;
    public override System.Drawing.Color ImageMarginGradientMiddle => BgBase;
    public override System.Drawing.Color ImageMarginGradientEnd => BgBase;
    public override System.Drawing.Color SeparatorDark => Border;
    public override System.Drawing.Color SeparatorLight => Border;
}