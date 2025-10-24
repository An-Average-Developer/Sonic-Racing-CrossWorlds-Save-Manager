using System.Windows;
using SonicRacingSaveManager.ViewModels;

namespace SonicRacingSaveManager
{
    public partial class MainWindow : Window
    {
        private MainViewModel? ViewModel => DataContext as MainViewModel;

        public MainWindow()
        {
            InitializeComponent();
            DataContext = new MainViewModel();
        }

        private void CheckForUpdatesBanner_Click(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            // Check for updates when clicking the gray banner
            if (ViewModel != null && ViewModel.CheckForUpdatesCommand.CanExecute(null))
            {
                ViewModel.CheckForUpdatesCommand.Execute(null);
            }
        }

        private void UpdateStatusButton_Click(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            // Show the installation view if update is available
            if (ViewModel != null &&
                ViewModel.IsUpdateAvailable &&
                ViewModel.ShowInstallationViewCommand.CanExecute(null))
            {
                ViewModel.ShowInstallationViewCommand.Execute(null);
            }
        }
    }
}
