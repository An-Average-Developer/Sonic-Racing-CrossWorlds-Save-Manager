using System.Windows;

namespace SonicRacingSaveManager.Views
{
    public partial class ValueAppliedDialog : Window
    {
        public ValueAppliedDialog()
        {
            InitializeComponent();
        }

        private void OkButton_Click(object sender, RoutedEventArgs e)
        {
            DialogResult = true;
            Close();
        }
    }
}
