using System.Windows;

namespace SonicRacingSaveManager;

public static class ButtonHelper
{
    public static readonly DependencyProperty CornerRadiusProperty =
        DependencyProperty.RegisterAttached(
            "CornerRadius",
            typeof(CornerRadius),
            typeof(ButtonHelper),
            new PropertyMetadata(new CornerRadius(3)));

    public static CornerRadius GetCornerRadius(DependencyObject obj)
        => (CornerRadius)obj.GetValue(CornerRadiusProperty);

    public static void SetCornerRadius(DependencyObject obj, CornerRadius value)
        => obj.SetValue(CornerRadiusProperty, value);
}
