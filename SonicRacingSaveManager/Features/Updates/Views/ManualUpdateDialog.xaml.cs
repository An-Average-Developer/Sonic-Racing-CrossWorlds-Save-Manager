using System.Windows.Controls;
using SonicRacingSaveManager.Features.Updates.Services;
using SonicRacingSaveManager.Features.Updates.ViewModels;

namespace SonicRacingSaveManager.Features.Updates.Views
{
    public partial class ManualUpdateDialog : UserControl
    {
        public ManualUpdateDialog(UpdateService updateService)
        {
            InitializeComponent();
            DataContext = new ManualUpdateDialogViewModel(updateService);
        }
    }
}
