using System;
using System.Globalization;
using System.Windows;
using System.Windows.Data;

namespace SonicRacingSaveManager.Common.Converters
{
    // Shows element only when value is not null
    public class NotNullConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return value != null ? Visibility.Visible : Visibility.Collapsed;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
}
