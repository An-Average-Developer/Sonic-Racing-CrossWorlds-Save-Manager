using System.Windows;
using SonicRacingSaveManager.Features.ModManager.ViewModels;

namespace SonicRacingSaveManager.Features.ModManager.Views
{
    public partial class FileSelectionDialog : Window
    {
        public FileSelectionDialog(FileSelectionDialogViewModel viewModel)
        {
            InitializeComponent();
            DataContext = viewModel;

            viewModel.Confirmed += (s, e) => DialogResult = true;
            viewModel.Cancelled += (s, e) => DialogResult = false;
        }
    }
}
