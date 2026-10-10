using System;
using System.Collections.Generic;
using SonicRacingSaveManager.Common.Services;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Animation;

namespace SonicRacingSaveManager.Common.Seasonal;

/// <summary>
/// Full-window seasonal scenery drawn behind the whole editor UI (dark mode only):
///   Halloween — night sky, big moon, haunted house, graveyard, bare trees, drifting fog, bunting.
///   Summer    — tropical sunset, setting sun over the sea, palm silhouettes, drifting clouds, bunting.
///   Christmas — starry sky with aurora, snowy hills, pine forest, warm cabin, string lights.
/// Nothing is drawn while no season is active. Animation runs through shared animated
/// brushes/transforms, so OnRender only re-runs when the size or the season changes.
/// </summary>
public sealed class SeasonalBackdrop : FrameworkElement
{
    /// <summary>Height of the header bar the garland hangs from.</summary>
    public static readonly DependencyProperty GarlandTopProperty = DependencyProperty.Register(
        nameof(GarlandTop), typeof(double), typeof(SeasonalBackdrop),
        new FrameworkPropertyMetadata(36.0, FrameworkPropertyMetadataOptions.AffectsRender));

    public double GarlandTop
    {
        get => (double)GetValue(GarlandTopProperty);
        set => SetValue(GarlandTopProperty, value);
    }

    // ── Shared animated resources ─────────────────────────────────────────
    private readonly SolidColorBrush _twinkleA = new(Colors.White);
    private readonly SolidColorBrush _twinkleB = new(Colors.White);
    private readonly SolidColorBrush _twinkleC = new(Colors.White);
    /// <summary>Flickering candle light (haunted house / cabin windows).</summary>
    private readonly SolidColorBrush _windowGlow = new(Color.FromRgb(255, 176, 60));
    /// <summary>Slow pulse (aurora mask, sun shimmer on the sea).</summary>
    private readonly SolidColorBrush _pulse = new(Color.FromRgb(255, 214, 150));
    /// <summary>Fog / clouds drifting sideways (repeats every full width).</summary>
    private readonly TranslateTransform _drift = new();
    private readonly TranslateTransform _driftSlow = new();
    /// <summary>Bunting gently swaying.</summary>
    private readonly SkewTransform _sway = new();

    private string _season = "classic";
    private double _animatedWidth = -1;

    public SeasonalBackdrop()
    {
        IsHitTestVisible = false;
        Loaded += (_, _) =>
        {
            ThemeService.Instance.SeasonChanged += OnSeasonChanged;
            Update();
        };
        Unloaded += (_, _) =>
        {
            ThemeService.Instance.SeasonChanged -= OnSeasonChanged;
            StopAnimations();
        };
        SizeChanged += (_, _) => { if (_season != "classic") StartDrift(); };
    }

    private void OnSeasonChanged(object? sender, string season) => Update();

    private void Update()
    {
        // The scenery is a night scene, so it's only shown with the dark seasonal palettes;
        // light mode keeps the plain seasonal background.
        _season = ThemeService.Instance.IsDarkTheme ? ThemeService.Instance.ActiveSeason : "classic";
        StopAnimations();
        Visibility = _season == "classic" ? Visibility.Collapsed : Visibility.Visible;
        if (_season != "classic") StartAnimations();
        InvalidateVisual();
    }

    private void StopAnimations()
    {
        foreach (var b in new[] { _twinkleA, _twinkleB, _twinkleC, _windowGlow, _pulse })
            b.BeginAnimation(Brush.OpacityProperty, null);
        _drift.BeginAnimation(TranslateTransform.XProperty, null);
        _driftSlow.BeginAnimation(TranslateTransform.XProperty, null);
        _sway.BeginAnimation(SkewTransform.AngleXProperty, null);
        _animatedWidth = -1;
    }

    private void StartAnimations()
    {
        var ease = new SineEase { EasingMode = EasingMode.EaseInOut };
        DoubleAnimation Blink(double from, double to, double seconds) =>
            new(from, to, TimeSpan.FromSeconds(seconds)) { AutoReverse = true, RepeatBehavior = RepeatBehavior.Forever, EasingFunction = ease };

        _twinkleA.BeginAnimation(Brush.OpacityProperty, Blink(1, 0.2, 1.7));
        _twinkleB.BeginAnimation(Brush.OpacityProperty, Blink(0.25, 1, 2.3));
        _twinkleC.BeginAnimation(Brush.OpacityProperty, Blink(0.9, 0.35, 3.1));
        _pulse.BeginAnimation(Brush.OpacityProperty, Blink(1, 0.45, 4.5));
        _windowGlow.BeginAnimation(Brush.OpacityProperty, new DoubleAnimationUsingKeyFrames
        {
            RepeatBehavior = RepeatBehavior.Forever,
            KeyFrames =
            {
                new LinearDoubleKeyFrame(1.0,  KeyTime.FromTimeSpan(TimeSpan.Zero)),
                new LinearDoubleKeyFrame(0.6,  KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(160))),
                new LinearDoubleKeyFrame(0.95, KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(300))),
                new LinearDoubleKeyFrame(0.75, KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(900))),
                new LinearDoubleKeyFrame(1.0,  KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(1400))),
                new LinearDoubleKeyFrame(1.0,  KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(2600))),
            },
        });
        _sway.BeginAnimation(SkewTransform.AngleXProperty, Blink(-4, 4, 2.6));
        StartDrift();
    }

    private void StartDrift()
    {
        var w = ActualWidth;
        if (w < 1 || Math.Abs(w - _animatedWidth) < 1) return;
        _animatedWidth = w;
        _drift.BeginAnimation(TranslateTransform.XProperty,
            new DoubleAnimation(0, -w, TimeSpan.FromSeconds(70)) { RepeatBehavior = RepeatBehavior.Forever });
        _driftSlow.BeginAnimation(TranslateTransform.XProperty,
            new DoubleAnimation(-w, 0, TimeSpan.FromSeconds(140)) { RepeatBehavior = RepeatBehavior.Forever });
    }

    // ── Helpers ───────────────────────────────────────────────────────────
    private static SolidColorBrush Frozen(byte r, byte g, byte b, byte a = 255)
    {
        var brush = new SolidColorBrush(Color.FromArgb(a, r, g, b));
        brush.Freeze();
        return brush;
    }

    private static Pen FrozenPen(Brush brush, double thickness)
    {
        var pen = new Pen(brush, thickness) { StartLineCap = PenLineCap.Round, EndLineCap = PenLineCap.Round, LineJoin = PenLineJoin.Round };
        pen.Freeze();
        return pen;
    }

    private static LinearGradientBrush Vertical(params (Color color, double offset)[] stops)
    {
        var brush = new LinearGradientBrush { StartPoint = new Point(0, 0), EndPoint = new Point(0, 1) };
        foreach (var (color, offset) in stops) brush.GradientStops.Add(new GradientStop(color, offset));
        brush.Freeze();
        return brush;
    }

    private static RadialGradientBrush Glow(Color color, byte alpha)
    {
        var brush = new RadialGradientBrush(Color.FromArgb(alpha, color.R, color.G, color.B), Color.FromArgb(0, color.R, color.G, color.B));
        brush.Freeze();
        return brush;
    }

    /// <summary>Closed silhouette whose top edge is a sum of sines (rolling hills), filled down to <paramref name="bottom"/>.</summary>
    private static Geometry Hills(double w, double baseY, double bottom, double amp, double seed)
    {
        var geo = new StreamGeometry();
        using (var ctx = geo.Open())
        {
            ctx.BeginFigure(new Point(0, bottom), true, true);
            const int steps = 80;
            for (int i = 0; i <= steps; i++)
            {
                var x = w * i / steps;
                var t = x / Math.Max(w, 1) * Math.PI * 2;
                var y = baseY - amp * (0.55 * Math.Sin(t * 1.3 + seed) + 0.3 * Math.Sin(t * 3.1 + seed * 2.1) + 0.15 * Math.Sin(t * 7.3 + seed * 0.7));
                ctx.LineTo(new Point(x, y), true, true);
            }
            ctx.LineTo(new Point(w, bottom), true, true);
        }
        geo.Freeze();
        return geo;
    }

    /// <summary>Y of the hills' top edge at x (same formula as <see cref="Hills"/>).</summary>
    private static double HillY(double x, double w, double baseY, double amp, double seed)
    {
        var t = x / Math.Max(w, 1) * Math.PI * 2;
        return baseY - amp * (0.55 * Math.Sin(t * 1.3 + seed) + 0.3 * Math.Sin(t * 3.1 + seed * 2.1) + 0.15 * Math.Sin(t * 7.3 + seed * 0.7));
    }

    private void DrawStars(DrawingContext dc, double w, double h, double maxY, int count, Color color)
    {
        var rnd = new Random(1337);
        var brushes = new Brush[] { _twinkleA, _twinkleB, _twinkleC };
        var fill = new SolidColorBrush(color);
        fill.Freeze();
        for (int i = 0; i < count; i++)
        {
            var x = rnd.NextDouble() * w;
            var y = rnd.NextDouble() * maxY;
            var r = 0.5 + rnd.NextDouble() * 1.3;
            var group = i % 4;
            dc.DrawEllipse(group == 3 ? fill : brushes[group], null, new Point(x, y), r, r);
            // A few bigger stars get a soft cross sparkle.
            if (i % 23 == 0)
            {
                var pen = new Pen(brushes[i % 3], 0.8);
                dc.DrawLine(pen, new Point(x - r * 4, y), new Point(x + r * 4, y));
                dc.DrawLine(pen, new Point(x, y - r * 4), new Point(x, y + r * 4));
            }
        }
    }

    /// <summary>Two copies of a band, one width apart, inside <paramref name="drift"/> so it loops seamlessly.</summary>
    private static void DrawLooping(DrawingContext dc, double w, TranslateTransform drift, Action<double> drawAtOffset)
    {
        dc.PushTransform(drift);
        drawAtOffset(0);
        drawAtOffset(w);
        dc.Pop();
    }

    // ── Render ────────────────────────────────────────────────────────────
    protected override void OnRender(DrawingContext dc)
    {
        double w = ActualWidth, h = ActualHeight;
        if (w < 200 || h < 200) return;
        dc.PushClip(new RectangleGeometry(new Rect(0, 0, w, h)));
        switch (_season)
        {
            case "halloween": DrawHalloween(dc, w, h); break;
            case "summer":    DrawSummer(dc, w, h);    break;
            case "christmas": DrawChristmas(dc, w, h); break;
        }
        dc.Pop();
    }

    // ── Halloween ─────────────────────────────────────────────────────────
    private static readonly Brush HwFarHill  = Frozen(30, 14, 44);
    private static readonly Brush HwMidHill  = Frozen(20, 9, 30);
    private static readonly Brush HwGround   = Frozen(11, 5, 17);
    private static readonly Pen   HwRim      = FrozenPen(Frozen(160, 90, 230, 70), 1.2);
    private static readonly Pen   HwFence    = FrozenPen(Frozen(11, 5, 17), 2.4);
    private static readonly Brush HwStone    = Frozen(24, 14, 34);

    private void DrawHalloween(DrawingContext dc, double w, double h)
    {
        dc.DrawRectangle(Vertical(
            (Color.FromRgb(8, 4, 14), 0),
            (Color.FromRgb(20, 9, 34), 0.45),
            (Color.FromRgb(52, 18, 58), 0.78),
            (Color.FromRgb(16, 7, 22), 1)), null, new Rect(0, 0, w, h));

        // Orange horizon glow.
        dc.DrawEllipse(Glow(Color.FromRgb(255, 110, 30), 70), null, new Point(w * 0.5, h * 0.86), w * 0.65, h * 0.22);

        DrawStars(dc, w, h, h * 0.6, 140, Color.FromRgb(230, 210, 255));

        // Big moon rising behind the hills (below the chapter cards, where the sky is visible), with a bat crossing it.
        var moonR = Math.Clamp(h * 0.075, 40, 110);
        var mx = w * 0.8; var my = h * 0.77;
        dc.DrawEllipse(Glow(Color.FromRgb(255, 200, 120), 55), null, new Point(mx, my), moonR * 4.5, moonR * 3);
        HalloweenArt.DrawMoon(dc, mx, my, moonR);
        HalloweenArt.DrawBat(dc, mx - moonR * 0.3, my - moonR * 0.3, moonR / 32);
        HalloweenArt.DrawBat(dc, mx + moonR * 0.35, my - moonR * 0.6, moonR / 48);

        // Thin clouds drifting across the sky.
        DrawLooping(dc, w, _driftSlow, ox =>
        {
            var cloud = Glow(Color.FromRgb(120, 70, 160), 40);
            dc.DrawEllipse(cloud, null, new Point(ox + w * 0.2, h * 0.14), w * 0.16, h * 0.03);
            dc.DrawEllipse(cloud, null, new Point(ox + w * 0.62, h * 0.09), w * 0.12, h * 0.025);
            dc.DrawEllipse(cloud, null, new Point(ox + w * 0.86, h * 0.26), w * 0.1, h * 0.02);
        });

        // Far hills with the haunted house on the left one.
        var farBase = h * 0.8;
        var houseX = w * 0.15;
        DrawHauntedHouse(dc, houseX, HillY(houseX, w, farBase, h * 0.05, 1.2) + 6, h / 560);
        dc.DrawGeometry(HwFarHill, HwRim, Hills(w, farBase, h, h * 0.05, 1.2));

        // Middle hills with bare trees.
        var midBase = h * 0.87;
        dc.DrawGeometry(HwMidHill, HwRim, Hills(w, midBase, h, h * 0.035, 4.0));
        foreach (var (fx, height, seed) in new[] { (0.035, 0.22, 3), (0.25, 0.12, 8), (0.73, 0.13, 5), (0.965, 0.24, 11) })
        {
            var x = w * fx;
            DrawBareTree(dc, x, HillY(x, w, midBase, h * 0.035, 4.0) + 4, h * height, seed, HwGround);
        }

        // Fog drifting through the valley.
        DrawLooping(dc, w, _drift, ox =>
        {
            var fog = Glow(Color.FromRgb(190, 160, 230), 34);
            for (int i = 0; i < 6; i++)
                dc.DrawEllipse(fog, null, new Point(ox + w * (0.08 + i * 0.17), h * (0.88 + (i % 2) * 0.03)), w * 0.14, h * 0.05);
        });

        // Graveyard ground: tombstones, crosses, a fence and glowing pumpkins.
        var groundBase = h * 0.93;
        dc.DrawGeometry(HwGround, HwRim, Hills(w, groundBase, h, h * 0.015, 2.5));
        var u = h / 1000;
        foreach (var fx in new[] { 0.06, 0.15, 0.31, 0.69, 0.86, 0.93 })
        {
            var x = w * fx;
            var y = HillY(x, w, groundBase, h * 0.015, 2.5) + 6 * u;
            if (((int)(fx * 100)) % 3 == 0) DrawCross(dc, x, y, u);
            else DrawTombstone(dc, x, y, u);
        }
        DrawFence(dc, w * 0.36, w * 0.45, groundBase, w, h, u);
        DrawFence(dc, w * 0.55, w * 0.64, groundBase, w, h, u);
        foreach (var (fx, sc) in new[] { (0.1, 2.2), (0.2, 1.6), (0.75, 1.8), (0.9, 2.4) })
        {
            var x = w * fx;
            HalloweenArt.DrawPumpkin(dc, x, HillY(x, w, groundBase, h * 0.015, 2.5) - 6 * u, sc * u * 1.1);
        }

        DrawBunting(dc, w, GarlandTop, new[]
        {
            Frozen(255, 122, 26), Frozen(106, 47, 181), Frozen(30, 16, 44), Frozen(255, 167, 77), Frozen(155, 93, 229),
        }, FrozenPen(Frozen(80, 50, 110), 1.2), bats: true);
    }

    private void DrawHauntedHouse(DrawingContext dc, double x, double baseY, double s)
    {
        dc.PushTransform(new TranslateTransform(x, baseY));
        dc.PushTransform(new ScaleTransform(s, s));
        var body = Geometry.Parse(
            "M -46,0 L -46,-40 L -54,-40 L -30,-62 L -6,-40 L -14,-40 L -14,0 Z " +       // left wing
            "M -16,0 L -16,-58 L -24,-58 L 4,-92 L 32,-58 L 24,-58 L 24,0 Z " +           // main house
            "M 22,0 L 22,-70 L 18,-70 L 32,-110 L 46,-70 L 42,-70 L 42,0 Z " +            // tower
            "M 0,-80 L 0,-100 L 6,-100 L 6,-86 Z");                                       // chimney
        dc.DrawGeometry(HwFarHill, null, body);
        foreach (var r in new[]
        {
            new Rect(-38, -32, 7, 9), new Rect(-26, -32, 7, 9),
            new Rect(-8, -48, 8, 11), new Rect(8, -48, 8, 11), new Rect(-1, -22, 8, 22),
            new Rect(28.5, -62, 7, 10), new Rect(28.5, -40, 7, 10),
        })
        {
            dc.DrawEllipse(Glow(Color.FromRgb(255, 170, 60), 80), null, new Point(r.X + r.Width / 2, r.Y + r.Height / 2), r.Width * 1.6, r.Height * 1.3);
            dc.DrawRectangle(_windowGlow, null, r);
        }
        dc.DrawEllipse(_windowGlow, null, new Point(4, -68), 4, 4);
        dc.Pop(); dc.Pop();
    }

    /// <summary>Gnarled bare tree silhouette (recursive branches) with its base at (x, y).</summary>
    private static void DrawBareTree(DrawingContext dc, double x, double y, double height, int seed, Brush fill)
    {
        var rnd = new Random(seed);
        var geo = new StreamGeometry();
        using (var ctx = geo.Open())
        {
            void Branch(Point from, double angle, double length, double width, int depth)
            {
                var to = new Point(from.X + Math.Cos(angle) * length, from.Y + Math.Sin(angle) * length);
                // Slight bend so branches look gnarled.
                var bend = (rnd.NextDouble() - 0.5) * length * 0.35;
                var nx = -Math.Sin(angle); var ny = Math.Cos(angle);
                var mid = new Point((from.X + to.X) / 2 + nx * bend, (from.Y + to.Y) / 2 + ny * bend);
                var tip = Math.Max(0.6, width * 0.62);
                ctx.BeginFigure(new Point(from.X + nx * width / 2, from.Y + ny * width / 2), true, true);
                ctx.QuadraticBezierTo(new Point(mid.X + nx * (width + tip) / 4, mid.Y + ny * (width + tip) / 4), new Point(to.X + nx * tip / 2, to.Y + ny * tip / 2), true, true);
                ctx.LineTo(new Point(to.X - nx * tip / 2, to.Y - ny * tip / 2), true, true);
                ctx.QuadraticBezierTo(new Point(mid.X - nx * (width + tip) / 4, mid.Y - ny * (width + tip) / 4), new Point(from.X - nx * width / 2, from.Y - ny * width / 2), true, true);
                if (depth <= 0 || length < 4) return;
                var children = depth > 3 ? 2 : 2 + rnd.Next(2);
                for (int i = 0; i < children; i++)
                {
                    var spread = (i - (children - 1) / 2.0) * (0.55 + rnd.NextDouble() * 0.35);
                    Branch(to, angle + spread + (rnd.NextDouble() - 0.5) * 0.3, length * (0.62 + rnd.NextDouble() * 0.18), tip, depth - 1);
                }
            }
            Branch(new Point(x, y), -Math.PI / 2 + (rnd.NextDouble() - 0.5) * 0.2, height * 0.36, height * 0.075, 5);
        }
        geo.Freeze();
        dc.DrawGeometry(fill, null, geo);
        // Roots / mound.
        dc.DrawEllipse(fill, null, new Point(x, y + 2), height * 0.12, height * 0.025);
    }

    private static void DrawTombstone(DrawingContext dc, double x, double y, double u)
    {
        dc.PushTransform(new TranslateTransform(x, y));
        dc.PushTransform(new ScaleTransform(u * 1.6, u * 1.6));
        dc.DrawGeometry(HwStone, HwRim, Geometry.Parse("M -10,0 L -10,-20 C -10,-32 10,-32 10,-20 L 10,0 Z"));
        dc.DrawLine(HwRim, new Point(-5, -18), new Point(5, -18));
        dc.DrawLine(HwRim, new Point(-4, -13), new Point(4, -13));
        dc.Pop(); dc.Pop();
    }

    private static void DrawCross(DrawingContext dc, double x, double y, double u)
    {
        dc.PushTransform(new TranslateTransform(x, y));
        dc.PushTransform(new ScaleTransform(u * 1.6, u * 1.6));
        dc.PushTransform(new RotateTransform(-6));
        dc.DrawGeometry(HwStone, HwRim, Geometry.Parse("M -2.5,0 L -2.5,-22 L -9,-22 L -9,-27 L -2.5,-27 L -2.5,-34 L 2.5,-34 L 2.5,-27 L 9,-27 L 9,-22 L 2.5,-22 L 2.5,0 Z"));
        dc.Pop(); dc.Pop(); dc.Pop();
    }

    private static void DrawFence(DrawingContext dc, double x0, double x1, double baseY, double w, double h, double u)
    {
        var spacing = 11 * u * 1.4;
        var height = 34 * u * 1.4;
        var top = new List<Point>();
        for (var x = x0; x <= x1; x += spacing)
        {
            var y = HillY(x, w, baseY, h * 0.015, 2.5) + 4;
            dc.DrawLine(HwFence, new Point(x, y), new Point(x, y - height));
            dc.DrawGeometry(HwGround, null, new PathGeometry(new[]
            {
                new PathFigure(new Point(x - 3 * u * 1.4, y - height), new[] { new LineSegment(new Point(x, y - height - 7 * u * 1.4), true), new LineSegment(new Point(x + 3 * u * 1.4, y - height), true) }, true),
            }));
            top.Add(new Point(x, y - height * 0.75));
        }
        for (int i = 1; i < top.Count; i++)
        {
            dc.DrawLine(HwFence, top[i - 1], top[i]);
            dc.DrawLine(HwFence, new Point(top[i - 1].X, top[i - 1].Y + height * 0.5), new Point(top[i].X, top[i].Y + height * 0.5));
        }
    }

    // ── Bunting (Halloween / Summer) ──────────────────────────────────────
    private void DrawBunting(DrawingContext dc, double w, double top, Brush[] flags, Pen stringPen, bool bats)
    {
        var segments = Math.Max(3, (int)Math.Round(w / 320));
        var segW = w / segments;
        var sag = 26.0;
        var flagW = 18.0;
        var flagH = 24.0;
        var perSeg = Math.Max(4, (int)(segW / 34));

        for (int s = 0; s < segments; s++)
        {
            var x0 = s * segW;
            var start = new Point(x0, top - 2);
            var end = new Point(x0 + segW, top - 2);
            var ctrl = new Point(x0 + segW / 2, top - 2 + sag * 2);
            var path = new PathGeometry(new[] { new PathFigure(start, new[] { new QuadraticBezierSegment(ctrl, end, true) }, false) });
            dc.DrawGeometry(null, stringPen, path);

            for (int i = 1; i < perSeg; i++)
            {
                var t = (double)i / perSeg;
                var px = (1 - t) * (1 - t) * start.X + 2 * (1 - t) * t * ctrl.X + t * t * end.X;
                var py = (1 - t) * (1 - t) * start.Y + 2 * (1 - t) * t * ctrl.Y + t * t * end.Y;
                // Tangent so the flags follow the string's angle.
                var dx = 2 * (1 - t) * (ctrl.X - start.X) + 2 * t * (end.X - ctrl.X);
                var dy = 2 * (1 - t) * (ctrl.Y - start.Y) + 2 * t * (end.Y - ctrl.Y);
                var angle = Math.Atan2(dy, dx) * 180 / Math.PI;

                dc.PushTransform(new TranslateTransform(px, py));
                dc.PushTransform(new RotateTransform(angle));
                dc.PushTransform(_sway);
                if (bats && (s * perSeg + i) % 5 == 2)
                {
                    // Little bat hanging upside down from the string.
                    dc.PushTransform(new ScaleTransform(1.1, -1.1));
                    HalloweenArt.DrawBat(dc, 0, -6, 1.0, bob: false);
                    dc.Pop();
                }
                else
                {
                    var flag = flags[(s * perSeg + i) % flags.Length];
                    dc.DrawGeometry(flag, null, new PathGeometry(new[]
                    {
                        new PathFigure(new Point(-flagW / 2, 0), new[] { new LineSegment(new Point(flagW / 2, 0), true), new LineSegment(new Point(0, flagH), true) }, true),
                    }));
                    dc.DrawLine(stringPen, new Point(-flagW / 2, 0.5), new Point(flagW / 2, 0.5));
                }
                dc.Pop(); dc.Pop(); dc.Pop();
            }
        }
    }

    // ── Summer ────────────────────────────────────────────────────────────
    private static readonly Brush SmSilhouette = Frozen(22, 10, 30);
    private static readonly Pen   SmShine     = FrozenPen(Frozen(255, 200, 120, 160), 2.2);
    private static readonly Pen   SmWave      = FrozenPen(Frozen(255, 170, 120, 60), 1.4);

    private void DrawSummer(DrawingContext dc, double w, double h)
    {
        var horizon = h * 0.8;
        dc.DrawRectangle(Vertical(
            (Color.FromRgb(8, 22, 48), 0),
            (Color.FromRgb(46, 28, 84), 0.4),
            (Color.FromRgb(150, 50, 100), 0.66),
            (Color.FromRgb(255, 128, 64), 0.8),
            (Color.FromRgb(255, 128, 64), 0.8001),
            (Color.FromRgb(70, 34, 86), 0.83),
            (Color.FromRgb(14, 22, 48), 1)), null, new Rect(0, 0, w, h));

        DrawStars(dc, w, h, h * 0.35, 70, Color.FromRgb(255, 240, 220));

        // Setting sun: banded disc clipped at the horizon, with a big glow.
        var sunR = Math.Clamp(h * 0.13, 70, 170);
        var sx = w * 0.76; // off-center so the chapter title/PLAY button stay readable
        dc.DrawEllipse(Glow(Color.FromRgb(255, 170, 70), 110), null, new Point(sx, horizon), sunR * 3.4, sunR * 2.2);
        dc.PushClip(new RectangleGeometry(new Rect(0, 0, w, horizon)));
        var sun = new LinearGradientBrush(Color.FromRgb(255, 236, 140), Color.FromRgb(255, 96, 90), 90);
        sun.Freeze();
        var disc = new EllipseGeometry(new Point(sx, horizon + sunR * 0.25), sunR, sunR);
        var bands = new GeometryGroup();
        for (int i = 0; i < 6; i++)
        {
            var bh = 3 + i * 2.2;
            bands.Children.Add(new RectangleGeometry(new Rect(sx - sunR, horizon - sunR * 0.75 + i * sunR * 0.17, sunR * 2, bh)));
        }
        dc.DrawGeometry(sun, null, new CombinedGeometry(GeometryCombineMode.Exclude, disc, bands));
        dc.Pop();

        // Sun reflection shimmering on the sea.
        var shimmer = new Pen(_pulse, 2.4);
        for (int i = 0; i < 9; i++)
        {
            var y = horizon + 8 + i * (h - horizon) / 11;
            var half = sunR * (0.9 - i * 0.07);
            dc.PushOpacity(1 - i * 0.09);
            dc.DrawLine(shimmer, new Point(sx - half, y), new Point(sx + half, y));
            dc.DrawLine(SmShine, new Point(sx - half * 0.5, y + 3), new Point(sx + half * 0.6, y + 3));
            dc.Pop();
        }

        // Waves rolling across the sea.
        DrawLooping(dc, w, _drift, ox =>
        {
            for (int row = 0; row < 5; row++)
            {
                var y = horizon + 16 + row * (h - horizon) / 6;
                for (var x = ox + (row % 2) * 40; x < ox + w; x += 90 + row * 14)
                    dc.DrawGeometry(null, SmWave, new PathGeometry(new[]
                    {
                        new PathFigure(new Point(x, y), new[] { new BezierSegment(new Point(x + 10, y - 5), new Point(x + 20, y - 5), new Point(x + 30, y), true) }, false),
                    }));
            }
        });

        // Pink clouds drifting.
        DrawLooping(dc, w, _driftSlow, ox =>
        {
            var c1 = Glow(Color.FromRgb(255, 150, 170), 80);
            var c2 = Glow(Color.FromRgb(200, 110, 180), 70);
            dc.DrawEllipse(c1, null, new Point(ox + w * 0.18, h * 0.55), w * 0.14, h * 0.025);
            dc.DrawEllipse(c2, null, new Point(ox + w * 0.24, h * 0.5), w * 0.08, h * 0.02);
            dc.DrawEllipse(c1, null, new Point(ox + w * 0.72, h * 0.6), w * 0.12, h * 0.022);
            dc.DrawEllipse(c2, null, new Point(ox + w * 0.82, h * 0.42), w * 0.1, h * 0.02);
        });

        // Islands and palm silhouettes on both sides.
        var u = h / 1000;
        dc.DrawGeometry(SmSilhouette, null, Geometry.Parse(
            FormattableString.Invariant($"M 0,{h} L 0,{h * 0.83} C {w * 0.08},{h * 0.8} {w * 0.18},{h * 0.84} {w * 0.24},{h * 0.9} L {w * 0.28},{h} Z")));
        dc.DrawGeometry(SmSilhouette, null, Geometry.Parse(
            FormattableString.Invariant($"M {w},{h} L {w},{h * 0.81} C {w * 0.92},{h * 0.79} {w * 0.84},{h * 0.84} {w * 0.78},{h * 0.9} L {w * 0.74},{h} Z")));
        DrawPalmSilhouette(dc, w * 0.05, h * 0.84, u * 2.6, 1);
        DrawPalmSilhouette(dc, w * 0.12, h * 0.85, u * 1.9, -1);
        DrawPalmSilhouette(dc, w * 0.94, h * 0.83, u * 2.8, -1);
        DrawPalmSilhouette(dc, w * 0.86, h * 0.85, u * 1.8, 1);

        DrawBunting(dc, w, GarlandTop, new[]
        {
            Frozen(255, 112, 67), Frozen(255, 213, 79), Frozen(38, 198, 218), Frozen(255, 245, 225), Frozen(240, 98, 146),
        }, FrozenPen(Frozen(255, 230, 200, 140), 1.2), bats: false);
    }

    private static readonly Geometry PalmFrondSil = Geometry.Parse("M 0,0 C 10,-10 26,-12 40,-2 C 34,-4 30,-2 28,2 C 24,-2 20,-2 18,3 C 14,-1 10,0 8,4 C 6,1 3,0 0,0 Z");

    private void DrawPalmSilhouette(DrawingContext dc, double x, double baseY, double s, int lean)
    {
        dc.PushTransform(new TranslateTransform(x, baseY));
        dc.PushTransform(new ScaleTransform(s * lean, s));
        dc.DrawGeometry(SmSilhouette, null, Geometry.Parse("M -3,0 C 0,-20 4,-40 14,-58 L 18,-56 C 9,-38 5,-20 4,0 Z"));
        dc.PushTransform(new TranslateTransform(16, -57));
        dc.PushTransform(_sway);
        foreach (var angle in new[] { -170.0, -140.0, -100.0, -60.0, -25.0, 10.0, 160.0 })
        {
            dc.PushTransform(new RotateTransform(angle));
            dc.DrawGeometry(SmSilhouette, null, PalmFrondSil);
            dc.Pop();
        }
        dc.DrawEllipse(SmSilhouette, null, new Point(-2, 3), 3.4, 3.4);
        dc.DrawEllipse(SmSilhouette, null, new Point(3, 4), 3.4, 3.4);
        dc.Pop(); dc.Pop();
        dc.Pop(); dc.Pop();
    }

    // ── Christmas ─────────────────────────────────────────────────────────
    private static readonly Brush XmFarHill  = Frozen(36, 58, 82);
    private static readonly Brush XmPine     = Frozen(10, 34, 26);
    private static readonly Brush XmPineSnow = Frozen(220, 234, 245, 230);
    private static readonly Brush XmCabin    = Frozen(58, 34, 28);
    private static readonly Brush XmRoof     = Frozen(235, 244, 252);

    private void DrawChristmas(DrawingContext dc, double w, double h)
    {
        dc.DrawRectangle(Vertical(
            (Color.FromRgb(4, 8, 22), 0),
            (Color.FromRgb(8, 22, 40), 0.5),
            (Color.FromRgb(16, 44, 58), 0.8),
            (Color.FromRgb(10, 26, 34), 1)), null, new Rect(0, 0, w, h));

        DrawStars(dc, w, h, h * 0.7, 190, Color.FromRgb(230, 245, 255));

        // Aurora: soft green/teal ribbons pulsing slowly.
        dc.PushOpacityMask(_pulse);
        // Low in the sky, in the gap between the chapter cards and the hills.
        DrawAurora(dc, w, h, h * 0.62, Color.FromRgb(80, 255, 170), 80, 0.0);
        DrawAurora(dc, w, h, h * 0.67, Color.FromRgb(60, 200, 255), 55, 2.0);
        dc.Pop();

        var u = h / 1000;

        // Far mountains / hills with a pine forest.
        var farBase = h * 0.8;
        dc.DrawGeometry(XmFarHill, null, Hills(w, farBase, h, h * 0.06, 0.4));
        var rnd = new Random(77);
        for (var x = 0.0; x < w; x += 26 * u + rnd.NextDouble() * 34 * u)
        {
            if (x > w * 0.3 && x < w * 0.7) continue; // keep the middle clear for the title/PLAY button
            var y = HillY(x, w, farBase, h * 0.06, 0.4) + 6;
            DrawPine(dc, x, y, (1.3 + rnd.NextDouble() * 1.1) * u * 1.6, snowy: true);
        }

        // Snowy hills in front, with the cabin and a decorated tree.
        var midBase = h * 0.9;
        var midTop = midBase - h * 0.06;
        var hillGeo = Hills(w, midBase, h, h * 0.03, 3.3);
        var hillFill = new LinearGradientBrush
        {
            MappingMode = BrushMappingMode.Absolute,
            StartPoint = new Point(0, midTop),
            EndPoint = new Point(0, h),
            GradientStops =
            {
                new GradientStop(Color.FromRgb(225, 238, 250), 0),
                new GradientStop(Color.FromRgb(150, 178, 204), 0.35),
                new GradientStop(Color.FromRgb(40, 60, 82), 1),
            },
        };
        hillFill.Freeze();
        dc.DrawGeometry(hillFill, null, hillGeo);

        // Lowest point of the slope under an object, so nothing floats with one corner in the air.
        double Ground(double x, double halfWidth) => Math.Max(HillY(x, w, midBase, h * 0.03, 3.3),
            Math.Max(HillY(x - halfWidth, w, midBase, h * 0.03, 3.3), HillY(x + halfWidth, w, midBase, h * 0.03, 3.3)));

        var cabinX = w * 0.15;
        var cabinS = u * 2.4;
        var cabinY = Ground(cabinX, 22 * cabinS) + 2;
        DrawCabin(dc, cabinX, cabinY, cabinS);
        // Snow drift piled against the cabin's base.
        dc.DrawEllipse(hillFill, null, new Point(cabinX, cabinY + 2 * cabinS), 30 * cabinS, 5 * cabinS);
        foreach (var (fx, sc) in new[] { (0.035, 3.2), (0.27, 2.2), (0.74, 2.4), (0.97, 3.4) })
        {
            var x = w * fx;
            DrawPine(dc, x, HillY(x, w, midBase, h * 0.03, 3.3) + 8, sc * u * 1.6, snowy: true);
        }

        // Decorated tree with the gifts and snowman beside it (not on top of its branches).
        // Tree half-width is 15 units, gifts 9 units, snowman 7.5 units (see ChristmasArt).
        var treeX = w * 0.86;
        var treeS = u * 5;
        ChristmasArt.DrawTree(dc, treeX, Ground(treeX, 15 * treeS) + 2, treeS);
        var redS = u * 2.2;
        var redX = treeX + 15 * treeS + 9 * redS + 6 * u;
        ChristmasArt.DrawGift(dc, redX, Ground(redX, 9 * redS) + 3, redS, Frozen(211, 47, 47));
        var blueS = u * 1.8;
        var blueX = treeX - 15 * treeS - 9 * blueS - 6 * u;
        ChristmasArt.DrawGift(dc, blueX, Ground(blueX, 9 * blueS) + 3, blueS, Frozen(66, 165, 245));
        var snowmanS = u * 3.2;
        var snowmanX = blueX - 9 * blueS - 7.5 * snowmanS - 8 * u;
        ChristmasArt.DrawSnowman(dc, snowmanX, Ground(snowmanX, 7.5 * snowmanS) + 3, snowmanS);

        // Low mist over the snow.
        DrawLooping(dc, w, _drift, ox =>
        {
            var mist = Glow(Color.FromRgb(220, 235, 255), 28);
            for (int i = 0; i < 5; i++)
                dc.DrawEllipse(mist, null, new Point(ox + w * (0.1 + i * 0.2), h * 0.9), w * 0.15, h * 0.04);
        });

        // String lights hanging from the top bar.
        ChristmasArt.DrawLights(dc, 0, w, GarlandTop - 1, 1.7);
    }

    private static void DrawAurora(DrawingContext dc, double w, double h, double y, Color color, byte alpha, double phase)
    {
        var geo = new StreamGeometry();
        using (var ctx = geo.Open())
        {
            const int steps = 60;
            var top = new Point[steps + 1];
            for (int i = 0; i <= steps; i++)
            {
                var x = w * i / steps;
                var t = (double)i / steps * Math.PI * 2;
                top[i] = new Point(x, y + Math.Sin(t * 1.5 + phase) * h * 0.05 + Math.Sin(t * 3.7 + phase) * h * 0.015);
            }
            ctx.BeginFigure(top[0], true, true);
            for (int i = 1; i <= steps; i++) ctx.LineTo(top[i], true, true);
            for (int i = steps; i >= 0; i--) ctx.LineTo(new Point(top[i].X, top[i].Y + h * (0.08 + 0.04 * Math.Sin(i * 0.3 + phase))), true, true);
        }
        geo.Freeze();
        var brush = Vertical(
            (Color.FromArgb(0, color.R, color.G, color.B), 0),
            (Color.FromArgb(alpha, color.R, color.G, color.B), 0.45),
            (Color.FromArgb(0, color.R, color.G, color.B), 1));
        dc.DrawGeometry(brush, null, geo);

        // Vertical "curtain" streaks.
        var rnd = new Random((int)(phase * 100) + 3);
        for (var x = 0.0; x < w; x += 7 + rnd.NextDouble() * 9)
        {
            var t = x / w * Math.PI * 2;
            var top = y + Math.Sin(t * 1.5 + phase) * h * 0.05 + Math.Sin(t * 3.7 + phase) * h * 0.015;
            var len = h * (0.05 + rnd.NextDouble() * 0.07);
            var streak = new LinearGradientBrush(Color.FromArgb((byte)(alpha * (0.4 + rnd.NextDouble() * 0.6)), color.R, color.G, color.B),
                                                 Color.FromArgb(0, color.R, color.G, color.B), 90);
            streak.Freeze();
            dc.DrawRectangle(streak, null, new Rect(x, top, 2 + rnd.NextDouble() * 2.5, len));
        }
    }

    private static void DrawPine(DrawingContext dc, double x, double baseY, double s, bool snowy)
    {
        dc.PushTransform(new TranslateTransform(x, baseY));
        dc.PushTransform(new ScaleTransform(s, s));
        dc.DrawRectangle(XmCabin, null, new Rect(-1.5, -4, 3, 4));
        dc.DrawGeometry(XmPine, null, Geometry.Parse("M 0,-34 L 7,-23 L 4,-23 L 10,-13 L 6,-13 L 12,-3 L -12,-3 L -6,-13 L -10,-13 L -4,-23 L -7,-23 Z"));
        if (snowy)
            dc.DrawGeometry(XmPineSnow, null, Geometry.Parse(
                "M 0,-34 L 3.5,-28.5 C 1,-27.5 -1,-28.5 -3.5,-28.5 Z " +
                "M -4,-23 L 4,-23 L 6,-19.5 C 2,-18.5 -2,-20 -6,-19.5 Z " +
                "M -6,-13 L 6,-13 L 8,-9.5 C 3,-8.5 -3,-10 -8,-9.5 Z"));
        dc.Pop(); dc.Pop();
    }

    private void DrawCabin(DrawingContext dc, double x, double baseY, double s)
    {
        dc.PushTransform(new TranslateTransform(x, baseY));
        dc.PushTransform(new ScaleTransform(s, s));
        // Chimney smoke.
        var smoke = Glow(Color.FromRgb(200, 210, 225), 70);
        dc.DrawEllipse(smoke, null, new Point(14, -46), 5, 4);
        dc.DrawEllipse(smoke, null, new Point(18, -54), 7, 5);
        dc.DrawEllipse(smoke, null, new Point(24, -63), 9, 6);
        dc.DrawRectangle(XmCabin, null, new Rect(11, -42, 6, 12));
        dc.DrawRectangle(XmCabin, null, new Rect(-22, -24, 44, 24));
        dc.DrawGeometry(XmRoof, null, Geometry.Parse("M -27,-22 L 0,-40 L 27,-22 L 24,-19 L 0,-35 L -24,-19 Z"));
        foreach (var r in new[] { new Rect(-16, -17, 8, 8), new Rect(8, -17, 8, 8) })
        {
            dc.DrawRectangle(Glow(Color.FromRgb(255, 190, 90), 110), null, new Rect(r.X - 7, r.Y - 7, r.Width + 14, r.Height + 14));
            dc.DrawRectangle(_windowGlow, null, r);
        }
        dc.DrawRectangle(Frozen(30, 18, 14), null, new Rect(-3.5, -13, 7, 13));
        dc.Pop(); dc.Pop();
    }
}
