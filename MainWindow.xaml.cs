using System.Windows;
using SonicRacingSaveManager.ViewModels;

namespace SonicRacingSaveManager
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            DataContext = new MainViewModel();
        }
    }
}
