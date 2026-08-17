using System.Windows;

namespace SonicRacingSaveManager.Features.Reports.Views
{
    public partial class DiscordInfoDialog : Window
    {
        public DiscordInfoDialog()
        {
            InitializeComponent();
        }

        private void CloseButton_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
