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
            if (ViewModel != null && ViewModel.CheckForUpdatesCommand.CanExecute(null))
            {
                ViewModel.CheckForUpdatesCommand.Execute(null);
            }
        }

        private void UpdateStatusButton_Click(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            if (ViewModel != null &&
                ViewModel.IsUpdateAvailable &&
                ViewModel.ShowInstallationViewCommand.CanExecute(null))
            {
                ViewModel.ShowInstallationViewCommand.Execute(null);
            }
        }

        private void TicketEditorCard_Click(object sender, System.Windows.Input.MouseButtonEventArgs e)
        {
            if (ViewModel != null && ViewModel.MemoryEditor.SelectTicketEditorCommand.CanExecute(null))
            {
                ViewModel.MemoryEditor.SelectTicketEditorCommand.Execute(null);
            }
        }
    }
}
