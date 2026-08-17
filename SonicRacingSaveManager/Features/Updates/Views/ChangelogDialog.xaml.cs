using System.Windows;
using SonicRacingSaveManager.Features.Updates.ViewModels;

namespace SonicRacingSaveManager.Features.Updates.Views
{
    public partial class ChangelogDialog : Window
    {
        public ChangelogDialog(string changelog, string latestVersion, long fileSize)
        {
            InitializeComponent();
            DataContext = new ChangelogDialogViewModel(this, changelog, latestVersion, fileSize);
        }

        public bool UserAccepted { get; set; }
    }
}
