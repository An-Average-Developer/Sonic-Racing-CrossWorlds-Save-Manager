using System.Windows;
using System.Windows.Input;
using SonicRacingSaveManager.Features.Backup.ViewModels;

namespace SonicRacingSaveManager.Features.Backup.Views
{
    public partial class AccountSelectionWindow : Window
    {
        public AccountSelectionWindow()
        {
            InitializeComponent();
            DataContext = new AccountSelectionViewModel();

            if (DataContext is AccountSelectionViewModel viewModel)
            {
                viewModel.AccountSelected += (s, e) =>
                {
                    DialogResult = true;
                    Close();
                };
            }
        }

        private void AccountListBox_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            if (DataContext is AccountSelectionViewModel viewModel && viewModel.SelectedAccount != null)
            {
                viewModel.SelectAccountCommand.Execute(null);
            }
        }
    }
}
