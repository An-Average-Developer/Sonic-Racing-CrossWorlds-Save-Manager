using System;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Animation;

namespace SonicRacingSaveManager.Common.Seasonal
{
    /// <summary>
    /// Vector Christmas decorations (Santa hats, trees, gifts, candy canes, snowmen, ornaments,
    /// holly, gingerbread, string lights, snowflakes) drawn into a DrawingContext.
    /// Same approach as <see cref="HalloweenArt"/>: unit-sized pieces around the origin,
    /// animated through shared transforms/brushes so WPF animates them without re-rendering.
    /// </summary>
    public static class ChristmasArt
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

        private static readonly Brush Red        = Frozen(211, 47, 47);
        private static readonly Brush RedDark    = Frozen(160, 25, 30);
        private static readonly Brush Green      = Frozen(46, 125, 50);
        private static readonly Brush GreenDark  = Frozen(27, 94, 32);
        private static readonly Brush Gold       = Frozen(255, 202, 40);
        private static readonly Brush White      = Frozen(255, 255, 255);
        private static readonly Brush Snow       = Frozen(245, 250, 255);
        private static readonly Brush SnowShaded = Frozen(214, 230, 245);
        private static readonly Brush TrunkBrush = Frozen(121, 85, 72);
        private static readonly Brush Ginger     = Frozen(196, 128, 64);
        private static readonly Brush Coal       = Frozen(40, 40, 45);
        private static readonly Brush Carrot     = Frozen(255, 138, 40);
        private static readonly Brush Blue       = Frozen(66, 165, 245);
        private static readonly Brush Purple     = Frozen(171, 71, 188);
        private static readonly Brush Berry      = Frozen(229, 57, 53);

        private static readonly Pen OutlinePen = FrozenPen(Frozen(30, 30, 40, 120), 0.7);
        private static readonly Pen RibbonPen  = FrozenPen(Frozen(255, 202, 40), 1.6);
        private static readonly Pen IcingPen   = FrozenPen(White, 0.8);
        private static readonly Pen WirePen    = FrozenPen(Frozen(40, 90, 50), 1.0);
        private static readonly Pen StripePen  = FrozenPen(Red, 2.2);
        private static readonly Pen CaneOutline = FrozenPen(Frozen(200, 200, 210), 4.2);
        private static readonly Pen CaneWhite  = FrozenPen(White, 3.2);
        private static readonly Pen FlakePenDark  = FrozenPen(Frozen(255, 255, 255, 220), 1.0);
        private static readonly Pen FlakePenLight = FrozenPen(Frozen(120, 170, 215, 230), 1.0);
        private static readonly Pen ThreadPenDark  = FrozenPen(Frozen(255, 255, 255, 110), 0.6);
        private static readonly Pen ThreadPenLight = FrozenPen(Frozen(80, 80, 90, 120), 0.6);

        /// <summary>True when drawing on light backgrounds: snowflakes and threads turn darker.</summary>
        public static bool LightMode { get; set; }
        private static Pen FlakePen => LightMode ? FlakePenLight : FlakePenDark;
        private static Pen ThreadPen => LightMode ? ThreadPenLight : ThreadPenDark;

        // ── Shared animated resources ─────────────────────────────────────────
        /// <summary>Two alternating sets of twinkling light bulbs.</summary>
        private static readonly SolidColorBrush TwinkleA = new(Color.FromRgb(255, 235, 59));
        private static readonly SolidColorBrush TwinkleB = new(Color.FromRgb(255, 82, 82));
        private static readonly SolidColorBrush TwinkleC = new(Color.FromRgb(105, 240, 174));
        private static readonly SolidColorBrush TwinkleD = new(Color.FromRgb(64, 196, 255));
        /// <summary>Tree-top star glow pulse.</summary>
        private static readonly SolidColorBrush StarGlow = new(Color.FromArgb(90, 255, 214, 60));
        /// <summary>Hanging ornament swing.</summary>
        private static readonly RotateTransform Swing = new();
        /// <summary>Gentle up/down float.</summary>
        private static readonly TranslateTransform Bob = new();
        /// <summary>Slow snowflake rotation.</summary>
        private static readonly RotateTransform Spin = new();
        /// <summary>Santa hat pom-pom wobble.</summary>
        private static readonly RotateTransform Wobble = new();

        private static bool _animating;

        public static void SetAnimationsRunning(bool running)
        {
            if (running == _animating) return;
            _animating = running;

            if (!running)
            {
                foreach (var b in new[] { TwinkleA, TwinkleB, TwinkleC, TwinkleD, StarGlow })
                    b.BeginAnimation(Brush.OpacityProperty, null);
                Swing.BeginAnimation(RotateTransform.AngleProperty, null);
                Bob.BeginAnimation(TranslateTransform.YProperty, null);
                Spin.BeginAnimation(RotateTransform.AngleProperty, null);
                Wobble.BeginAnimation(RotateTransform.AngleProperty, null);
                return;
            }

            var ease = new SineEase { EasingMode = EasingMode.EaseInOut };
            DoubleAnimation Blink(double from, double to, double seconds, double delay) =>
                new(from, to, TimeSpan.FromSeconds(seconds))
                { AutoReverse = true, RepeatBehavior = RepeatBehavior.Forever, BeginTime = TimeSpan.FromSeconds(delay) };

            TwinkleA.BeginAnimation(Brush.OpacityProperty, Blink(1, 0.25, 0.7, 0));
            TwinkleB.BeginAnimation(Brush.OpacityProperty, Blink(0.25, 1, 0.7, 0));
            TwinkleC.BeginAnimation(Brush.OpacityProperty, Blink(1, 0.3, 0.9, 0.35));
            TwinkleD.BeginAnimation(Brush.OpacityProperty, Blink(0.3, 1, 0.9, 0.35));
            StarGlow.BeginAnimation(Brush.OpacityProperty, new DoubleAnimation(0.4, 1, TimeSpan.FromSeconds(1.2))
            { AutoReverse = true, RepeatBehavior = RepeatBehavior.Forever, EasingFunction = ease });
            Swing.BeginAnimation(RotateTransform.AngleProperty, new DoubleAnimation(-7, 7, TimeSpan.FromSeconds(1.9))
            { AutoReverse = true, RepeatBehavior = RepeatBehavior.Forever, EasingFunction = ease });
            Bob.BeginAnimation(TranslateTransform.YProperty, new DoubleAnimation(0, -2.5, TimeSpan.FromSeconds(1.5))
            { AutoReverse = true, RepeatBehavior = RepeatBehavior.Forever, EasingFunction = ease });
            Spin.BeginAnimation(RotateTransform.AngleProperty, new DoubleAnimation(0, 360, TimeSpan.FromSeconds(18))
            { RepeatBehavior = RepeatBehavior.Forever });
            Wobble.BeginAnimation(RotateTransform.AngleProperty, new DoubleAnimation(-10, 10, TimeSpan.FromSeconds(1.1))
            { AutoReverse = true, RepeatBehavior = RepeatBehavior.Forever, EasingFunction = ease });
        }

        private static Brush TwinkleBrush(int i) => (i % 4) switch
        {
            0 => TwinkleA,
            1 => TwinkleC,
            2 => TwinkleB,
            _ => TwinkleD,
        };

        // ── Geometry ──────────────────────────────────────────────────────────
        private static Geometry Geo(string data)
        {
            var g = Geometry.Parse(data);
            g.Freeze();
            return g;
        }

        private static readonly Geometry HatCone   = Geo("M -10,0 C -8,-10 -2,-18 6,-20 C 12,-21 15,-17 15,-12 C 12,-15 8,-15 5,-12 C 3,-8 4,-4 10,0 Z");
        private static readonly Geometry TreeShape = Geo("M 0,-30 L 8,-19 L 5,-19 L 12,-9 L 8,-9 L 15,1 L -15,1 L -8,-9 L -12,-9 L -5,-19 L -8,-19 Z");
        private static readonly Geometry StarShape = BuildStar(5, 5.5, 2.3);
        private static readonly Geometry FlakeShape = BuildFlake();
        private static readonly Geometry CaneShape = Geo("M 0,14 L 0,-6 C 0,-13 9,-13 9,-6 L 9,-3");
        private static readonly Geometry GingerShape = Geo(
            "M 0,-13 C 4,-13 4,-7 1.8,-6.5 L 7,-5 C 9,-4.5 9,-1.5 7,-1.5 L 3.5,-2 L 3.5,3 L 6.5,10 C 7,12 4.5,13 3.5,11 L 0,5 " +
            "L -3.5,11 C -4.5,13 -7,12 -6.5,10 L -3.5,3 L -3.5,-2 L -7,-1.5 C -9,-1.5 -9,-4.5 -7,-5 L -1.8,-6.5 C -4,-7 -4,-13 0,-13 Z");
        private static readonly Geometry HollyLeaf = Geo("M 0,0 C 2,-2 3,-1 4.5,-3 C 5,-1.5 6.5,-1.5 8,-3 C 8,-1 9.5,0 11,0 C 9.5,0 8,1 8,3 C 6.5,1.5 5,1.5 4.5,3 C 3,1 2,2 0,0 Z");

        private static Geometry BuildStar(int points, double outer, double inner)
        {
            var geo = new StreamGeometry();
            using (var ctx = geo.Open())
            {
                for (int i = 0; i < points * 2; i++)
                {
                    var r = i % 2 == 0 ? outer : inner;
                    var a = -Math.PI / 2 + i * Math.PI / points;
                    var p = new Point(Math.Cos(a) * r, Math.Sin(a) * r);
                    if (i == 0) ctx.BeginFigure(p, true, true);
                    else ctx.LineTo(p, true, true);
                }
            }
            geo.Freeze();
            return geo;
        }

        private static Geometry BuildFlake()
        {
            var geo = new StreamGeometry();
            using (var ctx = geo.Open())
            {
                for (int i = 0; i < 6; i++)
                {
                    var a = i * Math.PI / 3;
                    Point P(double r, double da) => new(Math.Cos(a + da) * r, Math.Sin(a + da) * r);
                    ctx.BeginFigure(new Point(0, 0), false, false);
                    ctx.LineTo(P(8, 0), true, true);
                    ctx.BeginFigure(P(4.5, 0), false, false);
                    ctx.LineTo(P(6.5, 0.35), true, true);
                    ctx.BeginFigure(P(4.5, 0), false, false);
                    ctx.LineTo(P(6.5, -0.35), true, true);
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

        /// <summary>Santa hat with its brim centered at (x, y).</summary>
        public static void DrawSantaHat(DrawingContext dc, double x, double y, double scale, double angle) => At(dc, x, y, scale, angle, () =>
        {
            dc.DrawGeometry(Red, OutlinePen, HatCone);
            dc.PushTransform(new TranslateTransform(15, -12));
            dc.PushTransform(Wobble);
            dc.DrawEllipse(Snow, OutlinePen, new Point(0, 0), 3.4, 3.4);
            dc.Pop(); dc.Pop();
            dc.DrawRoundedRectangle(Snow, OutlinePen, new Rect(-12, -2.5, 24, 5.5), 2.7, 2.7);
        });

        /// <summary>Christmas tree with twinkling lights and a glowing star; base at (x, y).</summary>
        public static void DrawTree(DrawingContext dc, double x, double y, double scale) => At(dc, x, y, scale, 0, () =>
        {
            dc.DrawRectangle(TrunkBrush, null, new Rect(-2.5, 0, 5, 4));
            dc.DrawGeometry(Green, FrozenPen(GreenDark, 0.8), TreeShape);
            var bulbs = new[] { new Point(-3, -21), new Point(3, -17), new Point(-6, -12), new Point(5, -9),
                                new Point(0, -13), new Point(-9, -3), new Point(-2, -5), new Point(8, -2), new Point(3, -1) };
            for (int i = 0; i < bulbs.Length; i++)
                dc.DrawEllipse(TwinkleBrush(i), null, bulbs[i], 1.3, 1.3);
            dc.DrawEllipse(StarGlow, null, new Point(0, -31), 5, 5);
            dc.PushTransform(new TranslateTransform(0, -31));
            dc.DrawGeometry(Gold, null, StarShape);
            dc.Pop();
        });

        public static void DrawGift(DrawingContext dc, double x, double y, double scale, Brush box) => At(dc, x, y, scale, 0, () =>
        {
            dc.DrawRectangle(box, OutlinePen, new Rect(-8, -12, 16, 12));
            dc.DrawRectangle(box, OutlinePen, new Rect(-9, -15, 18, 4));
            dc.DrawLine(RibbonPen, new Point(0, -15), new Point(0, 0));
            dc.DrawLine(RibbonPen, new Point(-9, -13), new Point(9, -13));
            dc.DrawEllipse(null, RibbonPen, new Point(-2.8, -17), 2.8, 1.8);
            dc.DrawEllipse(null, RibbonPen, new Point(2.8, -17), 2.8, 1.8);
        });

        public static void DrawCandyCane(DrawingContext dc, double x, double y, double scale, double angle) => At(dc, x, y, scale, angle, () =>
        {
            dc.DrawGeometry(null, CaneOutline, CaneShape);
            dc.DrawGeometry(null, CaneWhite, CaneShape);
            dc.PushClip(new RectangleGeometry(new Rect(-3, -14, 15, 29)));
            for (int i = -2; i < 6; i++)
                dc.DrawLine(StripePen, new Point(-2, 12 - i * 4.5), new Point(2, 9.5 - i * 4.5));
            dc.DrawLine(StripePen, new Point(3, -11.5), new Point(4, -8.5));
            dc.DrawLine(StripePen, new Point(7.5, -10.5), new Point(6.5, -7.5));
            dc.Pop();
        });

        /// <summary>Snowman standing at (x, y).</summary>
        public static void DrawSnowman(DrawingContext dc, double x, double y, double scale) => At(dc, x, y, scale, 0, () =>
        {
            // Light mode: slightly blue-tinted snow so the snowman shows up on pale cards.
            var body = LightMode ? SnowShaded : Snow;
            dc.DrawEllipse(body, OutlinePen, new Point(0, -6), 7.5, 6.5);
            dc.DrawEllipse(body, OutlinePen, new Point(0, -16.5), 5.2, 5);
            dc.DrawRectangle(Red, null, new Rect(-5, -12.5, 10, 2.2));
            dc.DrawRectangle(Red, null, new Rect(2, -12, 2.2, 5));
            dc.DrawEllipse(Coal, null, new Point(-1.8, -17.5), 0.8, 0.8);
            dc.DrawEllipse(Coal, null, new Point(1.8, -17.5), 0.8, 0.8);
            dc.DrawGeometry(Carrot, null, Geo("M 0,-16 L 5,-15.2 L 0,-14.6 Z"));
            dc.DrawEllipse(Coal, null, new Point(0, -7.5), 0.8, 0.8);
            dc.DrawEllipse(Coal, null, new Point(0, -4.5), 0.8, 0.8);
            dc.DrawRectangle(Coal, null, new Rect(-4.5, -22.5, 9, 1.4));
            dc.DrawRectangle(Coal, null, new Rect(-3, -28, 6, 5.8));
        });

        /// <summary>Bauble hanging on a thread from (x, y).</summary>
        public static void DrawOrnament(DrawingContext dc, double x, double y, double length, double scale, Brush ball)
        {
            dc.PushTransform(new TranslateTransform(x, y));
            dc.PushTransform(Swing);
            dc.DrawLine(ThreadPen, new Point(0, 0), new Point(0, length));
            dc.PushTransform(new TranslateTransform(0, length));
            dc.PushTransform(new ScaleTransform(scale, scale));
            dc.DrawRectangle(Gold, null, new Rect(-1.8, -0.5, 3.6, 2.5));
            dc.DrawEllipse(ball, OutlinePen, new Point(0, 7), 5.8, 5.8);
            dc.DrawEllipse(Frozen(255, 255, 255, 150), null, new Point(-2, 5), 1.6, 1.2);
            dc.Pop(); dc.Pop(); dc.Pop(); dc.Pop();
        }

        public static void DrawHolly(DrawingContext dc, double x, double y, double scale, double angle) => At(dc, x, y, scale, angle, () =>
        {
            dc.PushTransform(new RotateTransform(-150));
            dc.DrawGeometry(Green, OutlinePen, HollyLeaf);
            dc.Pop();
            dc.PushTransform(new RotateTransform(-30));
            dc.DrawGeometry(GreenDark, OutlinePen, HollyLeaf);
            dc.Pop();
            dc.DrawEllipse(Berry, OutlinePen, new Point(-1.5, 0.5), 2, 2);
            dc.DrawEllipse(Berry, OutlinePen, new Point(1.8, 0.8), 2, 2);
            dc.DrawEllipse(Berry, OutlinePen, new Point(0, -2), 2, 2);
        });

        public static void DrawGingerbread(DrawingContext dc, double x, double y, double scale, double angle) => At(dc, x, y, scale, angle, () =>
        {
            dc.PushTransform(Bob);
            dc.DrawGeometry(Ginger, OutlinePen, GingerShape);
            dc.DrawEllipse(White, null, new Point(-1.3, -10.3), 0.6, 0.6);
            dc.DrawEllipse(White, null, new Point(1.3, -10.3), 0.6, 0.6);
            dc.DrawGeometry(null, IcingPen, Geo("M -1.5,-8.3 Q 0,-7.3 1.5,-8.3"));
            dc.DrawEllipse(Red, null, new Point(0, -3.5), 0.9, 0.9);
            dc.DrawEllipse(GreenDark, null, new Point(0, -0.5), 0.9, 0.9);
            dc.DrawGeometry(null, IcingPen, Geo("M -6.3,-4 L -5,-2.4 M 6.3,-4 L 5,-2.4 M -5.5,9.5 L -3.8,9 M 5.5,9.5 L 3.8,9"));
            dc.Pop();
        });

        public static void DrawSnowflake(DrawingContext dc, double x, double y, double scale, bool spin = true) => At(dc, x, y, scale, 0, () =>
        {
            if (spin) dc.PushTransform(Spin);
            dc.DrawGeometry(null, FlakePen, FlakeShape);
            if (spin) dc.Pop();
        });

        /// <summary>Sagging string of twinkling bulbs hanging between (x0, y) and (x1, y).</summary>
        public static void DrawLights(DrawingContext dc, double x0, double x1, double y, double scale)
        {
            var span = x1 - x0;
            if (span < 30) return;
            var spacing = 22 * scale;
            var segments = Math.Max(1, (int)Math.Round(span / (spacing * 4)));
            var segWidth = span / segments;
            var sag = 7 * scale;

            var wire = new StreamGeometry();
            using (var ctx = wire.Open())
            {
                ctx.BeginFigure(new Point(x0, y), false, false);
                for (int s = 0; s < segments; s++)
                    ctx.QuadraticBezierTo(new Point(x0 + segWidth * (s + 0.5), y + sag * 2), new Point(x0 + segWidth * (s + 1), y), true, true);
            }
            wire.Freeze();
            dc.DrawGeometry(null, WirePen, wire);

            var count = (int)(span / spacing);
            for (int i = 1; i < count; i++)
            {
                var bx = x0 + i * span / count;
                // Quadratic sag within the bulb's segment.
                var t = ((bx - x0) % segWidth) / segWidth;
                var by = y + sag * 2 * 2 * t * (1 - t);
                dc.PushTransform(new TranslateTransform(bx, by));
                dc.PushTransform(new ScaleTransform(scale, scale));
                dc.DrawRectangle(Coal, null, new Rect(-1.3, -0.5, 2.6, 2));
                dc.DrawEllipse(TwinkleBrush(i), null, new Point(0, 4), 2.2, 3.2);
                dc.Pop(); dc.Pop();
            }
        }

        // ── Layouts ───────────────────────────────────────────────────────────
        public static void DrawCard(DrawingContext dc, Size size, int variant)
        {
            double w = size.Width, h = size.Height;
            if (w < 60 || h < 40) return;
            var s = Math.Clamp(Math.Min(w, h) / 115.0, 1.0, 1.5);

            if (w >= 500)
            {
                DrawLights(dc, 10, w - 10, 1, s * 0.9);
                DrawTree(dc, 22 * s, h - 5, s * 1.1);
                DrawGift(dc, 46 * s, h - 3, s * 0.8, Red);
                DrawGift(dc, w - 22 * s, h - 3, s * 0.9, Blue);
                DrawSnowman(dc, w - 48 * s, h - 3, s * 0.9);
                return;
            }

            switch (variant % 6)
            {
                case 0:
                    DrawSantaHat(dc, 18 * s, 3, s, -18);
                    DrawHolly(dc, w - 16 * s, h - 14 * s, s * 1.1, 0);
                    break;
                case 1:
                    DrawOrnament(dc, w - 22 * s, 0, 16 * s, s, Red);
                    DrawOrnament(dc, w - 40 * s, 0, 8 * s, s * 0.8, Gold);
                    DrawGift(dc, 18 * s, h - 3, s, Green);
                    break;
                case 2:
                    DrawTree(dc, 18 * s, h - 5, s * 0.9);
                    DrawSnowflake(dc, w - 18 * s, 18 * s, s * 1.1);
                    break;
                case 3:
                    DrawCandyCane(dc, w - 20 * s, 22 * s, s * 0.9, 15);
                    DrawSnowman(dc, 18 * s, h - 3, s * 0.9);
                    break;
                case 4:
                    DrawHolly(dc, 16 * s, 14 * s, s * 1.1, 180);
                    DrawSantaHat(dc, w - 18 * s, 3, s, 18);
                    DrawGingerbread(dc, w - 18 * s, h - 18 * s, s * 1.1, 8);
                    break;
                default:
                    DrawGift(dc, w - 18 * s, h - 3, s, Purple);
                    DrawGingerbread(dc, 18 * s, 20 * s, s * 1.1, -8);
                    DrawSnowflake(dc, w - 20 * s, 18 * s, s * 0.9);
                    break;
            }
        }

        /// <summary>App header bar: string lights along the bottom edge and snowflakes around the middle.</summary>
        public static void DrawHeader(DrawingContext dc, Size size)
        {
            double w = size.Width, h = size.Height;
            if (w < 200) return;
            var cx = w * 0.5;
            DrawLights(dc, 0, w, h - 14, 0.9);
            DrawSnowflake(dc, cx, h * 0.42, h / 40);
            DrawSnowflake(dc, cx - h * 0.8, h * 0.3, h / 70);
            DrawSnowflake(dc, cx + h * 0.75, h * 0.35, h / 60);
            DrawSnowflake(dc, cx + h * 0.35, h * 0.15, h / 110);
            DrawSnowflake(dc, cx - h * 0.4, h * 0.6, h / 110);
        }
    }
}
