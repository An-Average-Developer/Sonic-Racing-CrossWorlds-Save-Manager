using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Threading;
using SonicRacingSaveManager.Common.Services;

namespace SonicRacingSaveManager.Common.Seasonal
{
    /// <summary>
    /// Click-through overlay for the whole window that plays ambient seasonal animations
    /// (Halloween: bats and ghosts crossing the screen plus rising embers, Summer: seagulls
    /// plus floating sun motes, Christmas: snowfall).
    /// </summary>
    public sealed class SeasonalOverlay : Canvas
    {
        private readonly DispatcherTimer _spawnTimer = new();
        private readonly DispatcherTimer _snowTimer = new() { Interval = TimeSpan.FromMilliseconds(190) };
        private readonly DispatcherTimer _particleTimer = new() { Interval = TimeSpan.FromMilliseconds(420) };
        private const int MaxSnowflakes = 110;
        private const int MaxParticles = 30;
        private int _particles;
        private readonly Random _random = new();

        public SeasonalOverlay()
        {
            IsHitTestVisible = false;
            ClipToBounds = true;

            _spawnTimer.Tick += (_, _) => SpawnFlock();
            _snowTimer.Tick += (_, _) => SpawnSnowflake();
            _particleTimer.Tick += (_, _) => SpawnParticle();
            Loaded += (_, _) =>
            {
                ThemeService.Instance.SeasonChanged += OnSeasonChanged;
                Update();
            };
            Unloaded += (_, _) =>
            {
                ThemeService.Instance.SeasonChanged -= OnSeasonChanged;
                _spawnTimer.Stop();
                _snowTimer.Stop();
                _particleTimer.Stop();
            };
        }

        private void OnSeasonChanged(object? sender, string season) => Update();

        private void Update()
        {
            var season = ThemeService.Instance.ActiveSeason;
            SeasonalDecorations.SyncArt(season);
            _spawnTimer.Stop();
            _snowTimer.Stop();
            _particleTimer.Stop();
            Children.Clear();
            _particles = 0;

            if (season is "halloween" or "summer")
            {
                ScheduleNext(TimeSpan.FromSeconds(2));
                _particleTimer.Start();
            }
            else if (season == "christmas")
                _snowTimer.Start();
        }

        private void ScheduleNext(TimeSpan delay)
        {
            _spawnTimer.Interval = delay;
            _spawnTimer.Start();
        }

        private void SpawnFlock()
        {
            _spawnTimer.Stop();
            if (ActualWidth < 100 || ActualHeight < 100) { ScheduleNext(TimeSpan.FromSeconds(3)); return; }

            var leftToRight = _random.Next(2) == 0;
            if (ThemeService.Instance.ActiveSeason == "halloween" && _random.Next(3) == 0)
            {
                SpawnGhost(leftToRight);
                ScheduleNext(TimeSpan.FromSeconds(_random.Next(8, 14)));
                return;
            }

            var count = _random.Next(1, 4);
            var baseY = ActualHeight * (0.15 + _random.NextDouble() * 0.6);
            for (int i = 0; i < count; i++)
                SpawnBat(leftToRight, baseY + (i * 26) - 20, TimeSpan.FromMilliseconds(i * 350));

            ScheduleNext(TimeSpan.FromSeconds(_random.Next(8, 16)));
        }

        private void SpawnBat(bool leftToRight, double y, TimeSpan delay)
        {
            var bat = new FlyingCritter(ThemeService.Instance.ActiveSeason, 1.4 + _random.NextDouble() * 0.8, leftToRight) { Opacity = 0.9 };
            Children.Add(bat);

            var duration = TimeSpan.FromSeconds(5 + _random.NextDouble() * 3);
            var from = leftToRight ? -60 : ActualWidth + 60;
            var to = leftToRight ? ActualWidth + 60 : -60;

            var fly = new DoubleAnimation(from, to, duration) { BeginTime = delay };
            fly.Completed += (_, _) => Children.Remove(bat);

            // Wavy flight path.
            var wave = new DoubleAnimationUsingKeyFrames { BeginTime = delay, Duration = duration };
            var amplitude = 18 + _random.NextDouble() * 22;
            for (int k = 0; k <= 8; k++)
            {
                var offset = k % 2 == 0 ? 0 : (k % 4 == 1 ? -amplitude : amplitude);
                wave.KeyFrames.Add(new EasingDoubleKeyFrame(y + offset,
                    KeyTime.FromPercent(k / 8.0), new SineEase { EasingMode = EasingMode.EaseInOut }));
            }

            SetLeft(bat, from);
            SetTop(bat, y);
            bat.BeginAnimation(LeftProperty, fly);
            bat.BeginAnimation(TopProperty, wave);
        }

        /// <summary>A faint ghost drifting slowly across the screen, fading in and out.</summary>
        private void SpawnGhost(bool leftToRight)
        {
            var ghost = new FlyingCritter("ghost", 2.2 + _random.NextDouble() * 1.2, leftToRight) { Opacity = 0 };
            Children.Add(ghost);

            var duration = TimeSpan.FromSeconds(16 + _random.NextDouble() * 8);
            var from = leftToRight ? -60 : ActualWidth + 60;
            var to = leftToRight ? ActualWidth + 60 : -60;
            var y = ActualHeight * (0.12 + _random.NextDouble() * 0.5);

            var fly = new DoubleAnimation(from, to, duration);
            fly.Completed += (_, _) => Children.Remove(ghost);

            var wave = new DoubleAnimationUsingKeyFrames { Duration = duration };
            for (int k = 0; k <= 6; k++)
                wave.KeyFrames.Add(new EasingDoubleKeyFrame(y + (k % 2 == 0 ? 0 : (k % 4 == 1 ? -30 : 30)),
                    KeyTime.FromPercent(k / 6.0), new SineEase { EasingMode = EasingMode.EaseInOut }));

            var fade = new DoubleAnimationUsingKeyFrames { Duration = duration };
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(0, KeyTime.FromPercent(0)));
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(0.5, KeyTime.FromPercent(0.2)));
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(0.15, KeyTime.FromPercent(0.5)));
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(0.5, KeyTime.FromPercent(0.75)));
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(0, KeyTime.FromPercent(1)));

            SetLeft(ghost, from);
            SetTop(ghost, y);
            ghost.BeginAnimation(LeftProperty, fly);
            ghost.BeginAnimation(TopProperty, wave);
            ghost.BeginAnimation(OpacityProperty, fade);
        }

        /// <summary>Glowing ember (Halloween) or sun mote (Summer) rising from the bottom and fading out.</summary>
        private void SpawnParticle()
        {
            if (ActualWidth < 100 || ActualHeight < 100 || _particles >= MaxParticles) return;

            var halloween = ThemeService.Instance.ActiveSeason == "halloween";
            var color = halloween
                ? (_random.Next(4) == 0 ? Color.FromRgb(190, 120, 255) : Color.FromRgb(255, 140, 40))
                : (_random.Next(3) == 0 ? Color.FromRgb(255, 255, 255) : Color.FromRgb(255, 214, 110));
            var dot = new GlowDot(color, 1.6 + _random.NextDouble() * 2.4) { Opacity = 0 };
            Children.Add(dot);
            _particles++;

            var x = _random.NextDouble() * ActualWidth;
            var startY = ActualHeight * (0.82 + _random.NextDouble() * 0.15);
            var endY = startY - ActualHeight * (0.25 + _random.NextDouble() * 0.35);
            var duration = TimeSpan.FromSeconds(6 + _random.NextDouble() * 6);

            var rise = new DoubleAnimation(startY, endY, duration);
            rise.Completed += (_, _) => { Children.Remove(dot); _particles--; };

            var drift = new DoubleAnimationUsingKeyFrames { Duration = duration };
            var amplitude = 12 + _random.NextDouble() * 30;
            for (int k = 0; k <= 4; k++)
                drift.KeyFrames.Add(new EasingDoubleKeyFrame(x + (k % 2 == 0 ? 0 : (k == 1 ? amplitude : -amplitude)),
                    KeyTime.FromPercent(k / 4.0), new SineEase { EasingMode = EasingMode.EaseInOut }));

            var fade = new DoubleAnimationUsingKeyFrames { Duration = duration };
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(0, KeyTime.FromPercent(0)));
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(0.9, KeyTime.FromPercent(0.25)));
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(0.5, KeyTime.FromPercent(0.6)));
            fade.KeyFrames.Add(new LinearDoubleKeyFrame(0, KeyTime.FromPercent(1)));

            SetLeft(dot, x);
            SetTop(dot, startY);
            dot.BeginAnimation(TopProperty, rise);
            dot.BeginAnimation(LeftProperty, drift);
            dot.BeginAnimation(OpacityProperty, fade);
        }

        private void SpawnSnowflake()
        {
            if (ActualWidth < 100 || ActualHeight < 100 || Children.Count >= MaxSnowflakes) return;

            var size = 1.6 + _random.NextDouble() * _random.NextDouble() * 5; // mostly small flakes, a few big ones
            var flake = new SnowflakeDot(size) { Opacity = 0.45 + _random.NextDouble() * 0.45 };
            Children.Add(flake);

            var x = _random.NextDouble() * ActualWidth;
            var duration = TimeSpan.FromSeconds(9 + _random.NextDouble() * 7 - size); // bigger flakes fall a bit faster

            var fall = new DoubleAnimation(-10, ActualHeight + 10, duration);
            fall.Completed += (_, _) => Children.Remove(flake);

            // Side-to-side drift.
            var drift = new DoubleAnimationUsingKeyFrames { Duration = duration };
            var amplitude = 10 + _random.NextDouble() * 25;
            var phase = _random.Next(2) == 0 ? 1 : -1;
            for (int k = 0; k <= 6; k++)
            {
                var offset = k % 2 == 0 ? 0 : amplitude * phase * (k % 4 == 1 ? 1 : -1);
                drift.KeyFrames.Add(new EasingDoubleKeyFrame(x + offset,
                    KeyTime.FromPercent(k / 6.0), new SineEase { EasingMode = EasingMode.EaseInOut }));
            }

            SetLeft(flake, x);
            SetTop(flake, -10);
            flake.BeginAnimation(TopProperty, fall);
            flake.BeginAnimation(LeftProperty, drift);
        }

        /// <summary>Soft round snowflake; white in dark mode, pale blue in light mode.</summary>
        private sealed class SnowflakeDot : FrameworkElement
        {
            private static readonly Brush DarkBrush = MakeBrush(Color.FromRgb(255, 255, 255));
            private static readonly Brush LightBrush = MakeBrush(Color.FromRgb(140, 185, 225));
            private readonly double _radius;

            public SnowflakeDot(double radius)
            {
                _radius = radius;
                IsHitTestVisible = false;
            }

            private static Brush MakeBrush(Color c)
            {
                var b = new RadialGradientBrush(c, Color.FromArgb(0, c.R, c.G, c.B)) { GradientOrigin = new Point(0.5, 0.5) };
                b.GradientStops[0].Offset = 0.55;
                b.Freeze();
                return b;
            }

            protected override void OnRender(DrawingContext dc) =>
                dc.DrawEllipse(ThemeService.Instance.IsDarkTheme ? DarkBrush : LightBrush, null, new Point(0, 0), _radius, _radius);
        }

        /// <summary>Soft glowing dot with a bright core.</summary>
        private sealed class GlowDot : FrameworkElement
        {
            private readonly Brush _halo;
            private readonly Brush _core;
            private readonly double _radius;

            public GlowDot(Color color, double radius)
            {
                _radius = radius;
                IsHitTestVisible = false;
                var halo = new RadialGradientBrush(Color.FromArgb(150, color.R, color.G, color.B), Color.FromArgb(0, color.R, color.G, color.B));
                halo.Freeze();
                var core = new SolidColorBrush(Color.FromRgb((byte)Math.Min(255, color.R + 40), (byte)Math.Min(255, color.G + 40), (byte)Math.Min(255, color.B + 40)));
                core.Freeze();
                _halo = halo;
                _core = core;
            }

            protected override void OnRender(DrawingContext dc)
            {
                dc.DrawEllipse(_halo, null, new Point(0, 0), _radius * 3.5, _radius * 3.5);
                dc.DrawEllipse(_core, null, new Point(0, 0), _radius * 0.6, _radius * 0.6);
            }
        }

        /// <summary>A bat (Halloween), ghost (Halloween) or seagull (Summer) crossing the screen.</summary>
        private sealed class FlyingCritter : FrameworkElement
        {
            private readonly string _season;
            private readonly double _scale;
            private readonly bool _facingRight;

            public FlyingCritter(string season, double scale, bool facingRight)
            {
                _season = season;
                _scale = scale;
                _facingRight = facingRight;
                IsHitTestVisible = false;
            }

            protected override void OnRender(DrawingContext dc)
            {
                dc.PushTransform(new ScaleTransform(_facingRight ? 1 : -1, 1));
                if (_season == "summer") SummerArt.DrawSeagull(dc, 0, 0, _scale);
                else if (_season == "ghost") HalloweenArt.DrawGhost(dc, 0, 0, _scale);
                else HalloweenArt.DrawBat(dc, 0, 0, _scale, bob: false);
                dc.Pop();
            }
        }
    }
}
