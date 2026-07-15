using System.Configuration;
using System.Data;
using System.Windows;

namespace ValoAI.App
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : System.Windows.Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            // Don't shutdown when last window closes — we live in the tray
            ShutdownMode = ShutdownMode.OnExplicitShutdown;
            base.OnStartup(e);

            var window = new MainWindow();

            // If launched with --minimized (from startup), don't show window
            if (e.Args.Contains("--minimized"))
            {
                // Window loads and starts recorder but stays hidden
                // Tray icon appears automatically
                window.Show();   // must show briefly to trigger Loaded event
                window.Hide();   // then hide immediately
            }
            else
            {
                window.Show();
            }

        }
    }

}
