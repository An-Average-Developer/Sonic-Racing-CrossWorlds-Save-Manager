using System;
using System.Windows;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Media;
using SonicRacingSaveManager.Common.Services;

namespace SonicRacingSaveManager.Common.Seasonal
{
    /// <summary>
    /// Attached property that adds seasonal decorations on top of an element
    /// (drawn in an adorner, so it never affects layout or blocks clicks).
    ///   local:SeasonalDecorations.Decoration="Card"   — corner decorations for cards
    ///   local:SeasonalDecorations.Decoration="Header" — scene for the app header bar
    /// Nothing is drawn while no season is active.
    /// </summary>
    public static class SeasonalDecorations
    {
        public static readonly DependencyProperty DecorationProperty = DependencyProperty.RegisterAttached(
            "Decoration", typeof(string), typeof(SeasonalDecorations), new PropertyMetadata(null, OnDecorationChanged));

        public static string? GetDecoration(DependencyObject d) => (string?)d.GetValue(DecorationProperty);
        public static void SetDecoration(DependencyObject d, string? value) => d.SetValue(DecorationProperty, value);

        private static readonly DependencyProperty AdornerProperty = DependencyProperty.RegisterAttached(
            "Adorner", typeof(SeasonalAdorner), typeof(SeasonalDecorations));

        // Variant stays fixed per element so its decorations don't reshuffle when a tab is reloaded.
        private static readonly DependencyProperty VariantProperty = DependencyProperty.RegisterAttached(
            "Variant", typeof(int), typeof(SeasonalDecorations), new PropertyMetadata(-1));

        private static int _nextVariant;

        static SeasonalDecorations()
        {
            SyncArt(ThemeService.Instance.ActiveSeason);
            ThemeService.Instance.SeasonChanged += (_, season) => SyncArt(season);
        }

        /// <summary>Runs only the active season's animations and tells the art which mode it's drawn in.</summary>
        internal static void SyncArt(string season)
        {
            var light = !ThemeService.Instance.IsDarkTheme;
            HalloweenArt.SetAnimationsRunning(season == "halloween");
            HalloweenArt.LightMode = light;
            SummerArt.SetAnimationsRunning(season == "summer");
            SummerArt.LightMode = light;
            ChristmasArt.SetAnimationsRunning(season == "christmas");
            ChristmasArt.LightMode = light;
        }

        private static void OnDecorationChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is not FrameworkElement fe) return;

            fe.Loaded -= OnLoaded;
            fe.Unloaded -= OnUnloaded;
            Detach(fe);

            if (string.IsNullOrEmpty(e.NewValue as string)) return;

            fe.Loaded += OnLoaded;
            fe.Unloaded += OnUnloaded;
            if (fe.IsLoaded) Attach(fe);
        }

        private static void OnLoaded(object sender, RoutedEventArgs e) => Attach((FrameworkElement)sender);
        private static void OnUnloaded(object sender, RoutedEventArgs e) => Detach((FrameworkElement)sender);

        private static void Attach(FrameworkElement fe)
        {
            if (fe.GetValue(AdornerProperty) != null) return;
            var layer = AdornerLayer.GetAdornerLayer(fe);
            if (layer == null) return;

            var variant = (int)fe.GetValue(VariantProperty);
            if (variant < 0)
            {
                variant = _nextVariant++;
                fe.SetValue(VariantProperty, variant);
            }

            var adorner = new SeasonalAdorner(fe, GetDecoration(fe) ?? "Card", variant);
            layer.Add(adorner);
            fe.SetValue(AdornerProperty, adorner);
        }

        private static void Detach(FrameworkElement fe)
        {
            if (fe.GetValue(AdornerProperty) is not SeasonalAdorner adorner) return;
            AdornerLayer.GetAdornerLayer(fe)?.Remove(adorner);
            adorner.Release();
            fe.ClearValue(AdornerProperty);
        }

        private sealed class SeasonalAdorner : Adorner
        {
            private readonly string _kind;
            private readonly int _variant;

            public SeasonalAdorner(FrameworkElement adorned, string kind, int variant) : base(adorned)
            {
                _kind = kind;
                _variant = variant;
                IsHitTestVisible = false;

                // Follow the card's own fade-in animation.
                SetBinding(OpacityProperty, new Binding(nameof(Opacity)) { Source = adorned });

                ThemeService.Instance.SeasonChanged += OnSeasonChanged;
                adorned.IsVisibleChanged += OnAdornedVisibleChanged;
                UpdateVisibility();
            }

            public void Release()
            {
                ThemeService.Instance.SeasonChanged -= OnSeasonChanged;
                AdornedElement.IsVisibleChanged -= OnAdornedVisibleChanged;
            }

            private void OnSeasonChanged(object? sender, string season)
            {
                UpdateVisibility();
                InvalidateVisual();
            }

            private void OnAdornedVisibleChanged(object sender, DependencyPropertyChangedEventArgs e) => UpdateVisibility();

            private void UpdateVisibility() =>
                Visibility = ThemeService.Instance.ActiveSeason != "classic" && AdornedElement.IsVisible
                    ? Visibility.Visible
                    : Visibility.Collapsed;

            protected override void OnRender(DrawingContext dc)
            {
                var size = AdornedElement.RenderSize;
                var header = _kind == "Header";
                switch (ThemeService.Instance.ActiveSeason)
                {
                    case "halloween":
                        if (header) HalloweenArt.DrawHeader(dc, size);
                        else HalloweenArt.DrawCard(dc, size, _variant);
                        break;
                    case "summer":
                        if (header) SummerArt.DrawHeader(dc, size);
                        else SummerArt.DrawCard(dc, size, _variant);
                        break;
                    case "christmas":
                        if (header) ChristmasArt.DrawHeader(dc, size);
                        else ChristmasArt.DrawCard(dc, size, _variant);
                        break;
                }
            }
        }
    }
}
