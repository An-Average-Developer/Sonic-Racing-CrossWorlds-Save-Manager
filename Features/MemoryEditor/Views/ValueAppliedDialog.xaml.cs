using System.Windows;

namespace SonicRacingSaveManager.Features.MemoryEditor.Views
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
