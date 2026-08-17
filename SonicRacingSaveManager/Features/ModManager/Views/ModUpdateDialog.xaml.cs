using System.Windows;
using SonicRacingSaveManager.Features.ModManager.ViewModels;

namespace SonicRacingSaveManager.Features.ModManager.Views
{
    public partial class ModUpdateDialog : Window
    {
        public ModUpdateDialog(ModUpdateDialogViewModel viewModel)
        {
            InitializeComponent();
            DataContext = viewModel;


            viewModel.UpdateCompleted += (s, e) =>
            {
                DialogResult = true;
                Close();
            };

            viewModel.UpdateCancelled += (s, e) =>
            {
                DialogResult = false;
                Close();
            };
        }
    }
}
