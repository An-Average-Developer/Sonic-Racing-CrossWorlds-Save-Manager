using System;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Animation;

namespace SonicRacingSaveManager.Common.Seasonal
{
    /// <summary>
    /// Vector Halloween decorations (pumpkins, witch hats, bats, spiders, trees, ghosts,
    /// cobwebs, moon) drawn straight into a DrawingContext. Every piece is drawn in small
    /// "unit" coordinates around the origin; callers position/scale it with transforms.
    ///
    /// Animation is done with shared, animated transforms/brushes that the drawings
    /// reference, so WPF animates them without re-running OnRender.
    /// </summary>
    public static class HalloweenArt
    {
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

        private static readonly Brush Orange      = Frozen(255, 117, 24);
        private static readonly Brush OrangeLight = Frozen(255, 150, 58);
        private static readonly Brush Stem        = Frozen(91, 123, 30);
        private static readonly Brush HatBrush    = Frozen(43, 15, 71);
        private static readonly Brush HatBand     = Frozen(255, 122, 26);
        private static readonly Brush Night       = Frozen(12, 6, 20);
        private static readonly Brush GhostBrush  = Frozen(245, 240, 255, 235);
        private static readonly Brush MoonBrush   = Frozen(255, 243, 196);
        private static readonly Brush Crater      = Frozen(230, 214, 160, 160);
        private static readonly Brush EyeBrush    = Frozen(255, 170, 40);

        private static readonly Pen PumpkinPen = FrozenPen(Frozen(184, 68, 11), 0.9);
        private static readonly Pen BuckleP    = FrozenPen(Frozen(255, 213, 79), 1.0);
        private static readonly Pen OutlinePen = FrozenPen(Frozen(160, 110, 230), 0.8);
        private static readonly Pen LegPen     = FrozenPen(Frozen(200, 170, 255), 0.9);
        private static readonly Pen ThreadPenDark  = FrozenPen(Frozen(255, 255, 255, 110), 0.6);
        private static readonly Pen WebPenDark     = FrozenPen(Frozen(255, 255, 255, 80), 0.7);
        private static readonly Pen ThreadPenLight = FrozenPen(Frozen(74, 30, 120, 130), 0.6);
        private static readonly Pen WebPenLight    = FrozenPen(Frozen(74, 30, 120, 95), 0.7);

        /// <summary>True when drawing on light backgrounds (light mode): webs and threads turn dark.</summary>
        public static bool LightMode { get; set; }

        private static Pen ThreadPen => LightMode ? ThreadPenLight : ThreadPenDark;
        private static Pen WebPen => LightMode ? WebPenLight : WebPenDark;

        // ── Shared animated resources ─────────────────────────────────────────
        /// <summary>Flickering candle light inside the pumpkins' faces.</summary>
        private static readonly SolidColorBrush Glow = new(Color.FromRgb(255, 214, 79));
        /// <summary>Gentle up/down float (ghosts, bats, hats).</summary>
        private static readonly TranslateTransform Bob = new();
        /// <summary>Pendulum swing (hanging spiders).</summary>
        private static readonly RotateTransform Swing = new();
        /// <summary>Wing flap (bats).</summary>
        private static readonly ScaleTransform Flap = new();

        private static bool _animating;

        /// <summary>Starts or stops the shared animations (stopped while no season is active to save CPU).</summary>
        public static void SetAnimationsRunning(bool running)
        {
            if (running == _animating) return;
            _animating = running;

            if (!running)
            {
                Glow.BeginAnimation(Brush.OpacityProperty, null);
                Bob.BeginAnimation(TranslateTransform.YProperty, null);
                Swing.BeginAnimation(RotateTransform.AngleProperty, null);
                Flap.BeginAnimation(ScaleTransform.ScaleYProperty, null);
                return;
            }

            var ease = new SineEase { EasingMode = EasingMode.EaseInOut };
            Glow.BeginAnimation(Brush.OpacityProperty, new DoubleAnimationUsingKeyFrames
            {
                RepeatBehavior = RepeatBehavior.Forever,
                KeyFrames =
                {
                    new LinearDoubleKeyFrame(1.0,  KeyTime.FromTimeSpan(TimeSpan.Zero)),
                    new LinearDoubleKeyFrame(0.55, KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(180))),
                    new LinearDoubleKeyFrame(0.9,  KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(320))),
                    new LinearDoubleKeyFrame(0.7,  KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(700))),
                    new LinearDoubleKeyFrame(1.0,  KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(1100))),
                    new LinearDoubleKeyFrame(1.0,  KeyTime.FromTimeSpan(TimeSpan.FromMilliseconds(1600))),
                },
            });
            Bob.BeginAnimation(TranslateTransform.YProperty, new DoubleAnimation(0, -3, TimeSpan.FromSeconds(1.4))
            { AutoReverse = true, RepeatBehavior = RepeatBehavior.Forever, EasingFunction = ease });
            Swing.BeginAnimation(RotateTransform.AngleProperty, new DoubleAnimation(-9, 9, TimeSpan.FromSeconds(1.8))
            { AutoReverse = true, RepeatBehavior = RepeatBehavior.Forever, EasingFunction = ease });
            Flap.BeginAnimation(ScaleTransform.ScaleYProperty, new DoubleAnimation(1, 0.3, TimeSpan.FromMilliseconds(170))
            { AutoReverse = true, RepeatBehavior = RepeatBehavior.Forever });
        }

        // ── Geometry ──────────────────────────────────────────────────────────
        private static Geometry Geo(string data)
        {
            var g = Geometry.Parse(data);
            g.Freeze();
            return g;
        }

        private static readonly Geometry PumpkinStem  = Geo("M -1.5,-9 L 1.5,-9 L 3,-14 L 0.5,-14.5 Z");
        private static readonly Geometry PumpkinEyes  = Geo("M -6.5,-2.5 L -2.5,-2.5 L -4.5,-6 Z M 2.5,-2.5 L 6.5,-2.5 L 4.5,-6 Z M -1,0.5 L 1,0.5 L 0,-1.5 Z");
        private static readonly Geometry PumpkinMouth = Geo("M -6.5,2.5 L -4.5,4.5 L -2.5,3 L -0.5,5 L 1.5,3 L 3.5,4.5 L 6.5,2.5 C 5,7.5 -5,7.5 -6.5,2.5 Z");

        private static readonly Geometry HatCone   = Geo("M -8,-1 L 8,-1 L 2.5,-17 C 2,-22 6,-25 9,-26 C 3,-26 -2,-22 -3,-16 Z");
        private static readonly Geometry HatBandG  = Geo("M -7.3,-3 L 7.3,-3 L 6.6,-6.5 L -6.6,-6.5 Z");

        private static readonly Geometry BatWing   = Geo("M 1.8,-1 C 6,-7 11,-8 16,-4 C 13,-3 12.5,-0.5 12.5,1.5 C 10.5,-0.5 8.5,0 7.5,2.5 C 6,0.5 4,0.5 1.8,2 Z");
        private static readonly Geometry BatEars   = Geo("M -2.2,-3 L -1.6,-6.5 L -0.4,-3.6 Z M 2.2,-3 L 1.6,-6.5 L 0.4,-3.6 Z");

        private static readonly Geometry GhostBody = Geo("M -8,9 L -8,-2 C -8,-13 8,-13 8,-2 L 8,9 L 5.3,6.5 L 2.7,9 L 0,6.5 L -2.7,9 L -5.3,6.5 Z");

        private static readonly Geometry TreeShape = Geo(
            "M -4,0 C -3,-8 -2,-14 -2.2,-20 C -6,-23 -10,-26 -14,-32 L -12,-33 C -9,-29 -5,-26 -1.6,-24 " +
            "C -1.2,-30 -3.5,-36 -2.5,-43 L -0.5,-43 C 0,-36 1.2,-31 1.6,-26 C 4,-29 8,-31 11,-37 L 13,-36 " +
            "C 10,-30 6,-26 2.2,-21.5 C 2.4,-14 3,-7 5,0 Z " +
            "M -8.5,-28.5 L -12,-25 L -11,-24.5 L -8,-27.5 Z M 7,-30 L 9,-27 L 10,-27.5 L 8.5,-30.5 Z");
        private static readonly Geometry TreeMound = Geo("M -14,1 C -8,-4 8,-4 14,1 Z");

        private static readonly Geometry Cobweb = BuildCobweb();

        private static Geometry BuildCobweb()
        {
            // Quarter web anchored at the origin, spreading into +X/+Y.
            const double r = 34;
            var angles = new[] { 0.0, 22.5, 45.0, 67.5, 90.0 };
            var rings  = new[] { 9.0, 17.0, 25.0, 33.0 };
            Point P(double radius, double deg) => new(radius * Math.Cos(deg * Math.PI / 180), radius * Math.Sin(deg * Math.PI / 180));

            var geo = new StreamGeometry();
            using (var ctx = geo.Open())
            {
                foreach (var a in angles)
                {
                    ctx.BeginFigure(new Point(0, 0), false, false);
                    ctx.LineTo(P(r, a), true, true);
                }
                foreach (var ring in rings)
                {
                    ctx.BeginFigure(P(ring, angles[0]), false, false);
                    for (int i = 1; i < angles.Length; i++)
                    {
                        var mid = (angles[i - 1] + angles[i]) / 2;
                        ctx.QuadraticBezierTo(P(ring * 0.82, mid), P(ring, angles[i]), true, true);
                    }
                }
            }
            geo.Freeze();
            return geo;
        }

        // ── Pieces ────────────────────────────────────────────────────────────
        private static void At(DrawingContext dc, double x, double y, double scale, double angle, Action draw)
        {
            dc.PushTransform(new TranslateTransform(x, y));
            dc.PushTransform(new RotateTransform(angle));
            dc.PushTransform(new ScaleTransform(scale, scale));
            draw();
            dc.Pop(); dc.Pop(); dc.Pop();
        }

        public static void DrawPumpkin(DrawingContext dc, double x, double y, double scale) => At(dc, x, y, scale, 0, () =>
        {
            dc.DrawEllipse(Orange, PumpkinPen, new Point(-5.5, 0), 7, 9);
            dc.DrawEllipse(Orange, PumpkinPen, new Point(5.5, 0), 7, 9);
            dc.DrawEllipse(OrangeLight, PumpkinPen, new Point(0, 0), 6.5, 9.6);
            dc.DrawGeometry(Stem, null, PumpkinStem);
            dc.DrawGeometry(Glow, null, PumpkinEyes);
            dc.DrawGeometry(Glow, null, PumpkinMouth);
        });

        public static void DrawWitchHat(DrawingContext dc, double x, double y, double scale, double angle) => At(dc, x, y, scale, angle, () =>
        {
            dc.PushTransform(Bob);
            dc.DrawEllipse(HatBrush, OutlinePen, new Point(0, 0), 14, 3.6);
            dc.DrawGeometry(HatBrush, OutlinePen, HatCone);
            dc.DrawGeometry(HatBand, null, HatBandG);
            dc.DrawRectangle(null, BuckleP, new Rect(-2, -6.3, 4, 3.1));
            dc.Pop();
        });

        public static void DrawBat(DrawingContext dc, double x, double y, double scale, bool bob = true) => At(dc, x, y, scale, 0, () =>
        {
            if (bob) dc.PushTransform(Bob);
            dc.PushTransform(Flap);
            dc.DrawGeometry(Night, OutlinePen, BatWing);
            dc.PushTransform(new ScaleTransform(-1, 1));
            dc.DrawGeometry(Night, OutlinePen, BatWing);
            dc.Pop();
            dc.Pop();
            dc.DrawGeometry(Night, OutlinePen, BatEars);
            dc.DrawEllipse(Night, OutlinePen, new Point(0, 0), 2.6, 3.8);
            dc.DrawEllipse(EyeBrush, null, new Point(-0.9, -1.4), 0.55, 0.55);
            dc.DrawEllipse(EyeBrush, null, new Point(0.9, -1.4), 0.55, 0.55);
            if (bob) dc.Pop();
        });

        /// <summary>Spider hanging on a thread from (x, y); <paramref name="length"/> is in unscaled pixels.</summary>
        public static void DrawSpider(DrawingContext dc, double x, double y, double length, double scale)
        {
            dc.PushTransform(new TranslateTransform(x, y));
            dc.PushTransform(Swing);
            dc.DrawLine(ThreadPen, new Point(0, 0), new Point(0, length));
            dc.PushTransform(new TranslateTransform(0, length));
            dc.PushTransform(new ScaleTransform(scale, scale));
            for (int side = -1; side <= 1; side += 2)
            {
                for (int i = 0; i < 4; i++)
                {
                    var y0 = -1.5 + i * 1.3;
                    dc.DrawLine(LegPen, new Point(side * 2, y0), new Point(side * 5.5, y0 - 2.5 + i * 0.8));
                    dc.DrawLine(LegPen, new Point(side * 5.5, y0 - 2.5 + i * 0.8), new Point(side * 7.5, y0 + 1.5 + i * 1.2));
                }
            }
            dc.DrawEllipse(Night, OutlinePen, new Point(0, 1.5), 3.2, 4);
            dc.DrawEllipse(Night, OutlinePen, new Point(0, -3), 2.2, 2);
            dc.DrawEllipse(EyeBrush, null, new Point(-0.8, -3.2), 0.5, 0.5);
            dc.DrawEllipse(EyeBrush, null, new Point(0.8, -3.2), 0.5, 0.5);
            dc.Pop(); dc.Pop(); dc.Pop(); dc.Pop();
        }

        public static void DrawGhost(DrawingContext dc, double x, double y, double scale) => At(dc, x, y, scale, 0, () =>
        {
            dc.PushTransform(Bob);
            dc.DrawGeometry(GhostBrush, OutlinePen, GhostBody);
            dc.DrawEllipse(Night, null, new Point(-2.8, -3), 1.4, 2);
            dc.DrawEllipse(Night, null, new Point(2.8, -3), 1.4, 2);
            dc.DrawEllipse(Night, null, new Point(0, 1.5), 1.6, 1.2);
            dc.Pop();
        });

        /// <summary>Spooky bare tree with its base at (x, y).</summary>
        public static void DrawTree(DrawingContext dc, double x, double y, double scale) => At(dc, x, y, scale, 0, () =>
        {
            dc.DrawGeometry(Night, OutlinePen, TreeMound);
            dc.DrawGeometry(Night, OutlinePen, TreeShape);
        });

        /// <summary>Quarter cobweb in a corner. flipX/flipY choose which corner it spreads from.</summary>
        public static void DrawCobweb(DrawingContext dc, double x, double y, double scale, bool flipX, bool flipY)
        {
            dc.PushTransform(new TranslateTransform(x, y));
            dc.PushTransform(new ScaleTransform(flipX ? -scale : scale, flipY ? -scale : scale));
            dc.DrawGeometry(null, WebPen, Cobweb);
            dc.Pop(); dc.Pop();
        }

        public static void DrawMoon(DrawingContext dc, double x, double y, double radius)
        {
            var halo = new RadialGradientBrush(Color.FromArgb(90, 255, 214, 120), Color.FromArgb(0, 255, 214, 120));
            halo.Freeze();
            dc.DrawEllipse(halo, null, new Point(x, y), radius * 2, radius * 2);
            dc.DrawEllipse(MoonBrush, null, new Point(x, y), radius, radius);
            dc.DrawEllipse(Crater, null, new Point(x - radius * 0.35, y - radius * 0.2), radius * 0.22, radius * 0.22);
            dc.DrawEllipse(Crater, null, new Point(x + radius * 0.3, y + radius * 0.35), radius * 0.15, radius * 0.15);
            dc.DrawEllipse(Crater, null, new Point(x + radius * 0.25, y - radius * 0.4), radius * 0.1, radius * 0.1);
        }

        // ── Layouts ───────────────────────────────────────────────────────────
        /// <summary>Decorates a card of the given size; <paramref name="variant"/> picks the arrangement.</summary>
        public static void DrawCard(DrawingContext dc, Size size, int variant)
        {
            double w = size.Width, h = size.Height;
            if (w < 60 || h < 40) return;
            var s = Math.Clamp(Math.Min(w, h) / 115.0, 1.0, 1.5);

            if (w >= 500)
            {
                // Wide cards (headers, editor panels): richer scene along the edges.
                DrawCobweb(dc, 0, 0, s * 1.1, false, false);
                DrawSpider(dc, w - 46 * s, 0, 20 * s, s);
                DrawSpider(dc, w - 90 * s, 0, 34 * s, s * 0.85);
                DrawPumpkin(dc, w - 20 * s, h - 14 * s, s);
                DrawPumpkin(dc, w - 44 * s, h - 11 * s, s * 0.75);
                DrawTree(dc, 22 * s, h - 2, s * 0.9);
                return;
            }

            switch (variant % 6)
            {
                case 0:
                    DrawWitchHat(dc, 18 * s, 2, s, -18);
                    DrawCobweb(dc, w, 0, s * 0.9, true, false);
                    break;
                case 1:
                    DrawPumpkin(dc, w - 17 * s, h - 14 * s, s);
                    DrawBat(dc, 22 * s, 16 * s, s * 0.9);
                    break;
                case 2:
                    DrawSpider(dc, w - 22 * s, 0, 24 * s, s);
                    DrawTree(dc, 18 * s, h - 2, s * 0.85);
                    break;
                case 3:
                    DrawGhost(dc, w - 18 * s, 18 * s, s);
                    DrawPumpkin(dc, 18 * s, h - 14 * s, s * 0.9);
                    break;
                case 4:
                    DrawCobweb(dc, 0, 0, s * 0.9, false, false);
                    DrawWitchHat(dc, w - 18 * s, 2, s, 18);
                    break;
                default:
                    DrawBat(dc, w - 22 * s, 16 * s, s * 0.9);
                    DrawTree(dc, w - 18 * s, h - 2, s * 0.85);
                    DrawPumpkin(dc, 16 * s, h - 12 * s, s * 0.75);
                    break;
            }
        }

        /// <summary>Decorates the app header bar: moon with bats in the middle, cobweb in the corner.</summary>
        public static void DrawHeader(DrawingContext dc, Size size)
        {
            double w = size.Width, h = size.Height;
            if (w < 200) return;
            var cx = w * 0.5;
            var cy = h * 0.5;
            DrawMoon(dc, cx, cy, h * 0.28);
            DrawBat(dc, cx - h * 0.75, cy - h * 0.12, 1.1);
            DrawBat(dc, cx + h * 0.7, cy + h * 0.05, 0.9);
            DrawBat(dc, cx + h * 0.35, cy - h * 0.3, 0.7);
            DrawCobweb(dc, 0, 0, 0.9, false, false);
        }
    }
}
