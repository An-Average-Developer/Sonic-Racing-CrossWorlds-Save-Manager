using System.Windows;
using SonicRacingSaveManager.ViewModels;

namespace SonicRacingSaveManager.Views
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
