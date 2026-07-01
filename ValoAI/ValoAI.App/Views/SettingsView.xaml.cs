using System;
using System.Collections.Generic;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace ValoAI.App.Views
{
    /// <summary>
    /// Interaction logic for SettingsView.xaml
    /// </summary>
    public partial class SettingsView : Page
    {
        public SettingsView()
        {
            InitializeComponent();
        }

        public void BrowseFolder_Click(object sender, RoutedEventArgs e)
        {

        }

        private void BitrateSlider_ValueChanged( object sender, RoutedPropertyChangedEventArgs<double> e)
        {
        }

        private void Save_Click(object sender, RoutedEventArgs e)
        {
        }
    }
}
