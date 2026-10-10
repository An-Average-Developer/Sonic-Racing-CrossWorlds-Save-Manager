using System;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Animation;

namespace SonicRacingSaveManager.Common.Seasonal
{
    /// <summary>
    /// Vector summer decorations (sun, palm trees, beach balls, crabs, starfish, shells,
    /// ice creams, umbrellas, waves, seagulls, clouds) drawn into a DrawingContext.
    /// Same approach as <see cref="HalloweenArt"/>: unit-sized pieces around the origin,
    /// animated through shared transforms so WPF animates them without re-rendering.
    /// </summary>
    public static class SummerArt
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

        private static readonly Brush SunCore     = Frozen(255, 213, 79);
        private static readonly Brush SunRay      = Frozen(255, 183, 39);
        private static readonly Brush PalmLeaf    = Frozen(46, 160, 67);
        private static readonly Brush PalmLeafDk  = Frozen(29, 115, 48);
        private static readonly Brush Trunk       = Frozen(160, 110, 60);
        private static readonly Brush Coconut     = Frozen(110, 70, 35);
        private static readonly Brush BallRed     = Frozen(239, 83, 80);
        private static readonly Brush BallBlue    = Frozen(41, 182, 246);
        private static readonly Brush BallYellow  = Frozen(255, 213, 79);
        private static readonly Brush White       = Frozen(255, 255, 255);
        private static readonly Brush CrabBrush   = Frozen(244, 81, 30);
        private static readonly Brush StarBrush   = Frozen(255, 138, 101);
        private static readonly Brush ShellBrush  = Frozen(255, 204, 188);
        private static readonly Brush Cone        = Frozen(214, 160, 90);
        private static readonly Brush ScoopPink   = Frozen(248, 140, 180);
        private static readonly Brush ScoopMint   = Frozen(150, 230, 190);
        private static readonly Brush UmbrellaA   = Frozen(255, 112, 67);
        private static readonly Brush UmbrellaB   = Frozen(255, 245, 225);
        private static readonly Brush SandBrush   = Frozen(240, 210, 150);
        private static readonly Brush WaveBrush   = Frozen(38, 198, 218, 150);
        private static readonly Brush WaveFoam    = Frozen(255, 255, 255, 170);
        private static readonly Brush CloudBrush  = Frozen(255, 255, 255, 230);
        private static readonly Brush Dark        = Frozen(40, 30, 30);

        private static readonly Pen OutlinePen  = FrozenPen(Frozen(90, 60, 30, 170), 0.8);
        private static readonly Pen WhitePen    = FrozenPen(White, 1.0);
        private static readonly Pen StemPen     = FrozenPen(Frozen(120, 90, 60), 1.2);
        private static readonly Pen GullPenDark = FrozenPen(Frozen(255, 255, 255, 230), 1.6);
        private static readonly Pen GullPenLite = FrozenPen(Frozen(55, 71, 79, 220), 1.6);
        private static readonly Pen ShellLine   = FrozenPen(Frozen(200, 120, 100, 200), 0.6);

        /// <summary>True when drawing on light backgrounds: seagulls turn dark grey.</summary>
        public static bool LightMode { get; set; }
        private static Pen GullPen => LightMode ? GullPenLite : GullPenDark;

        // ── Shared animated resources ─────────────────────────────────────────
        /// <summary>Slow full rotation (sun rays, beach balls).</summary>
        private static readonly RotateTransform Spin = new();
        /// <summary>Gentle up/down float (balls, ice creams, clouds).</summary>
        private static readonly TranslateTransform Bob = new();
        /// <summary>Palm leaves swaying in the breeze.</summary>
        private static readonly RotateTransform Sway = new();
        /// <summary>Crabs scuttling sideways.</summary>
        private static readonly TranslateTransform Scuttle = new();
        /// <summary>Waves rolling left/right.</summary>
        private static readonly TranslateTransform Roll = new();
        /// <summary>Seagull wing beat.</summary>
        private static readonly ScaleTransform Flap = new();

        private static bool _animating;

        public static void SetAnimationsRunning(bool running)
        {
            if (running == _animating) return;
            _animating = running;

            if (!running)
            {
                Spin.BeginAnimation(RotateTransform.AngleProperty, null);
                Bob.BeginAnimation(TranslateTransform.YProperty, null);
                Sway.BeginAnimation(RotateTransform.AngleProperty, null);
                Scuttle.BeginAnimation(TranslateTransform.XProperty, null);
                Roll.BeginAnimation(TranslateTransform.XProperty, null);
                Flap.BeginAnimation(ScaleTransform.ScaleYProperty, null);
                return;
            }

            var ease = new SineEase { EasingMode = EasingMode.EaseInOut };
            Spin.BeginAnimation(RotateTransform.AngleProperty, new DoubleAnimation(0, 360, TimeSpan.FromSeconds(24))
            { RepeatBehavior = RepeatBehavior.Forever });
            Bob.BeginAnimation(TranslateTransform.YProperty, new DoubleAnimation(0, -3, TimeSpan.FromSeconds(1.6))
            { AutoReverse = true, RepeatBehavior = RepeatBehavior.Forever, EasingFunction = ease });
            Sway.BeginAnimation(RotateTransform.AngleProperty, new DoubleAnimation(-4, 4, TimeSpan.FromSeconds(2.4))
            { AutoReverse = true, RepeatBehavior = RepeatBehavior.Forever, EasingFunction = ease });
            Scuttle.BeginAnimation(TranslateTransform.XProperty, new DoubleAnimationUsingKeyFrames
            {
                RepeatBehavior = RepeatBehavior.Forever,
                KeyFrames =
                {
                    new EasingDoubleKeyFrame(0,  KeyTime.FromTimeSpan(TimeSpan.Zero)),
                    new EasingDoubleKeyFrame(6,  KeyTime.FromTimeSpan(TimeSpan.FromSeconds(0.6)), ease),
                    new EasingDoubleKeyFrame(6,  KeyTime.FromTimeSpan(TimeSpan.FromSeconds(1.8))),
                    new EasingDoubleKeyFrame(-4, KeyTime.FromTimeSpan(TimeSpan.FromSeconds(2.6)), ease),
                    new EasingDoubleKeyFrame(-4, KeyTime.FromTimeSpan(TimeSpan.FromSeconds(3.6))),
                    new EasingDoubleKeyFrame(0,  KeyTime.FromTimeSpan(TimeSpan.FromSeconds(4.2)), ease),
                },
            });
            Roll.BeginAnimation(TranslateTransform.XProperty, new DoubleAnimation(-5, 5, TimeSpan.FromSeconds(2.2))
            { AutoReverse = true, RepeatBehavior = RepeatBehavior.Forever, EasingFunction = ease });
            Flap.BeginAnimation(ScaleTransform.ScaleYProperty, new DoubleAnimation(1, 0.4, TimeSpan.FromMilliseconds(320))
            { AutoReverse = true, RepeatBehavior = RepeatBehavior.Forever, EasingFunction = ease });
        }

        // ── Geometry ──────────────────────────────────────────────────────────
        private static Geometry Geo(string data)
        {
            var g = Geometry.Parse(data);
            g.Freeze();
            return g;
        }

        private static readonly Geometry SunRays     = BuildSunRays();
        private static readonly Geometry PalmTrunk   = Geo("M -2,0 C -1,-10 0,-20 3,-30 L 5.5,-29.5 C 3,-20 2,-10 2.5,0 Z");
        private static readonly Geometry PalmFrond   = Geo("M 0,0 C 5,-5 12,-5 17,0 C 12,-2 6,-1 0,0 Z");
        private static readonly Geometry PalmIsland  = Geo("M -14,1 C -8,-4 10,-4 16,1 Z");
        private static readonly Geometry CrabBody    = Geo("M -7,0 C -7,-6 7,-6 7,0 C 5,2 -5,2 -7,0 Z");
        private static readonly Geometry CrabClaws   = Geo("M -8,-4 C -12,-7 -12,-11 -9,-11 L -9.5,-8.5 L -7.5,-9.5 C -7,-7 -7,-5.5 -8,-4 Z M 8,-4 C 12,-7 12,-11 9,-11 L 9.5,-8.5 L 7.5,-9.5 C 7,-7 7,-5.5 8,-4 Z");
        private static readonly Geometry Star        = BuildStar();
        private static readonly Geometry ShellShape  = Geo("M 0,4 C -9,2 -9,-7 0,-8 C 9,-7 9,2 0,4 Z M -2.5,4.5 L 2.5,4.5 L 1.5,6.5 L -1.5,6.5 Z");
        private static readonly Geometry ConeShape   = Geo("M -4.5,0 L 4.5,0 L 0,12 Z");
        private static readonly Geometry UmbrellaTop = Geo("M -15,0 C -14,-12 14,-12 15,0 C 12,-2 9,-2 7.5,0 C 5,-2 2.5,-2 0,0 C -2.5,-2 -5,-2 -7.5,0 C -9,-2 -12,-2 -15,0 Z");
        private static readonly Geometry UmbrellaStripes = Geo("M -7.5,0 C -6,-6 -3,-9.5 0,-10 C -2,-7 -3,-4 0,0 C -2.5,-2 -5,-2 -7.5,0 Z M 7.5,0 C 6,-6 3,-9.5 0,-10 C 2,-7 3,-4 0,0 C 2.5,-2 5,-2 7.5,0 Z");
        private static readonly Geometry GullShape   = Geo("M -9,-1 Q -4.5,-6 0,0 Q 4.5,-6 9,-1");
        private static readonly Geometry BallSegA    = Geo("M 0,0 L 0,-9 A 9,9 0 0 1 7.8,-4.5 Z");
        private static readonly Geometry BallSegB    = Geo("M 0,0 L 7.8,4.5 A 9,9 0 0 1 0,9 Z");
        private static readonly Geometry BallSegC    = Geo("M 0,0 L -7.8,4.5 A 9,9 0 0 1 -7.8,-4.5 Z");
        private static readonly Geometry CloudShape  = Geo("M -14,4 C -18,4 -18,-3 -12,-3 C -12,-9 -3,-10 -1,-5 C 1,-11 11,-10 10,-3 C 16,-4 17,4 12,4 Z");

        private static Geometry BuildSunRays()
        {
            var geo = new StreamGeometry();
            using (var ctx = geo.Open())
            {
                for (int i = 0; i < 12; i++)
                {
                    var a = i * Math.PI / 6;
                    var b = a + 0.14;
                    var c = a - 0.14;
                    ctx.BeginFigure(new Point(Math.Cos(c) * 11, Math.Sin(c) * 11), true, true);
                    ctx.LineTo(new Point(Math.Cos(a) * 17, Math.Sin(a) * 17), true, true);
                    ctx.LineTo(new Point(Math.Cos(b) * 11, Math.Sin(b) * 11), true, true);
                }
            }
            geo.Freeze();
            return geo;
        }

        private static Geometry BuildStar()
        {
            var geo = new StreamGeometry();
            using (var ctx = geo.Open())
            {
                for (int i = 0; i < 10; i++)
                {
                    var r = i % 2 == 0 ? 9 : 3.8;
                    var a = -Math.PI / 2 + i * Math.PI / 5;
                    var p = new Point(Math.Cos(a) * r, Math.Sin(a) * r);
                    if (i == 0) ctx.BeginFigure(p, true, true);
                    else ctx.LineTo(p, true, true);
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

        public static void DrawSun(DrawingContext dc, double x, double y, double scale) => At(dc, x, y, scale, 0, () =>
        {
            var halo = new RadialGradientBrush(Color.FromArgb(110, 255, 214, 90), Color.FromArgb(0, 255, 214, 90));
            halo.Freeze();
            dc.DrawEllipse(halo, null, new Point(0, 0), 24, 24);
            dc.PushTransform(Spin);
            dc.DrawGeometry(SunRay, null, SunRays);
            dc.Pop();
            dc.DrawEllipse(SunCore, null, new Point(0, 0), 9.5, 9.5);
        });

        /// <summary>Palm tree on a small sand island, base at (x, y).</summary>
        public static void DrawPalm(DrawingContext dc, double x, double y, double scale) => At(dc, x, y, scale, 0, () =>
        {
            dc.DrawGeometry(SandBrush, null, PalmIsland);
            dc.DrawGeometry(Trunk, OutlinePen, PalmTrunk);
            dc.PushTransform(new TranslateTransform(4.2, -30));
            dc.PushTransform(Sway);
            foreach (var (angle, brush) in new[] { (-165.0, PalmLeafDk), (-120.0, PalmLeaf), (-60.0, PalmLeaf), (-15.0, PalmLeafDk), (20.0, PalmLeaf), (160.0, PalmLeaf) })
            {
                dc.PushTransform(new RotateTransform(angle));
                dc.DrawGeometry(brush, null, PalmFrond);
                dc.Pop();
            }
            dc.DrawEllipse(Coconut, null, new Point(-1.5, 1.5), 1.8, 1.8);
            dc.DrawEllipse(Coconut, null, new Point(1.5, 2), 1.8, 1.8);
            dc.Pop(); dc.Pop();
        });

        public static void DrawBeachBall(DrawingContext dc, double x, double y, double scale) => At(dc, x, y, scale, 0, () =>
        {
            dc.PushTransform(Bob);
            dc.PushTransform(Spin);
            dc.DrawEllipse(White, OutlinePen, new Point(0, 0), 9, 9);
            dc.DrawGeometry(BallRed, null, BallSegA);
            dc.DrawGeometry(BallBlue, null, BallSegB);
            dc.DrawGeometry(BallYellow, null, BallSegC);
            dc.DrawEllipse(White, null, new Point(0, 0), 2, 2);
            dc.Pop(); dc.Pop();
        });

        public static void DrawCrab(DrawingContext dc, double x, double y, double scale) => At(dc, x, y, scale, 0, () =>
        {
            dc.PushTransform(Scuttle);
            var legPen = new Pen(CrabBrush, 1.1);
            legPen.Freeze();
            for (int i = 0; i < 3; i++)
            {
                dc.DrawLine(legPen, new Point(-5 + i, 0), new Point(-9 + i * 0.5, 3.5));
                dc.DrawLine(legPen, new Point(5 - i, 0), new Point(9 - i * 0.5, 3.5));
            }
            dc.DrawGeometry(CrabBrush, null, CrabClaws);
            dc.DrawGeometry(CrabBrush, null, CrabBody);
            dc.DrawLine(legPen, new Point(-2, -4), new Point(-2.5, -7));
            dc.DrawLine(legPen, new Point(2, -4), new Point(2.5, -7));
            dc.DrawEllipse(White, null, new Point(-2.5, -7.5), 1.3, 1.3);
            dc.DrawEllipse(White, null, new Point(2.5, -7.5), 1.3, 1.3);
            dc.DrawEllipse(Dark, null, new Point(-2.5, -7.5), 0.6, 0.6);
            dc.DrawEllipse(Dark, null, new Point(2.5, -7.5), 0.6, 0.6);
            dc.Pop();
        });

        public static void DrawStarfish(DrawingContext dc, double x, double y, double scale, double angle) => At(dc, x, y, scale, angle, () =>
        {
            dc.DrawGeometry(StarBrush, OutlinePen, Star);
            dc.DrawEllipse(ShellBrush, null, new Point(0, -4.5), 0.7, 0.7);
            dc.DrawEllipse(ShellBrush, null, new Point(0, 0), 0.9, 0.9);
        });

        public static void DrawShell(DrawingContext dc, double x, double y, double scale, double angle) => At(dc, x, y, scale, angle, () =>
        {
            dc.DrawGeometry(ShellBrush, OutlinePen, ShellShape);
            for (int i = -2; i <= 2; i++)
                dc.DrawLine(ShellLine, new Point(0, 4), new Point(i * 3, -6.5 + Math.Abs(i) * 0.8));
        });

        public static void DrawIceCream(DrawingContext dc, double x, double y, double scale) => At(dc, x, y, scale, 0, () =>
        {
            dc.PushTransform(Bob);
            dc.DrawGeometry(Cone, OutlinePen, ConeShape);
            dc.DrawLine(StemPen, new Point(-2.5, 2), new Point(1.5, 8));
            dc.DrawLine(StemPen, new Point(2.5, 2), new Point(-1.5, 8));
            dc.DrawEllipse(ScoopMint, null, new Point(0, -3), 5.5, 5);
            dc.DrawEllipse(ScoopPink, null, new Point(0, -8.5), 5, 4.6);
            dc.DrawEllipse(BallRed, null, new Point(0.5, -13.5), 1.6, 1.6);
            dc.Pop();
        });

        /// <summary>Beach umbrella with its pole planted at (x, y).</summary>
        public static void DrawUmbrella(DrawingContext dc, double x, double y, double scale, double angle) => At(dc, x, y, scale, angle, () =>
        {
            dc.DrawLine(StemPen, new Point(0, 0), new Point(0, -22));
            dc.PushTransform(new TranslateTransform(0, -20));
            dc.DrawGeometry(UmbrellaB, OutlinePen, UmbrellaTop);
            dc.DrawGeometry(UmbrellaA, null, UmbrellaStripes);
            dc.Pop();
            dc.DrawEllipse(SandBrush, null, new Point(0, 0.5), 8, 1.8);
        });

        public static void DrawSeagull(DrawingContext dc, double x, double y, double scale) => At(dc, x, y, scale, 0, () =>
        {
            dc.PushTransform(Flap);
            dc.DrawGeometry(null, GullPen, GullShape);
            dc.Pop();
        });

        public static void DrawCloud(DrawingContext dc, double x, double y, double scale) => At(dc, x, y, scale, 0, () =>
        {
            dc.PushTransform(Bob);
            dc.DrawGeometry(CloudBrush, null, CloudShape);
            dc.Pop();
        });

        /// <summary>Rolling waves along the bottom edge of a card, from x0 to x1 at baseline y.</summary>
        public static void DrawWaves(DrawingContext dc, double x0, double x1, double y, double scale)
        {
            const double period = 16;
            var geo = new StreamGeometry();
            var foam = new StreamGeometry();
            using (var ctx = geo.Open())
            using (var fctx = foam.Open())
            {
                // Extra period on each side so the rolling animation never shows an edge.
                var start = x0 - period * scale;
                var end = x1 + period * scale;
                ctx.BeginFigure(new Point(start, y), true, true);
                fctx.BeginFigure(new Point(start, y - 4 * scale), false, false);
                for (var x = start; x < end; x += period * scale)
                {
                    ctx.QuadraticBezierTo(new Point(x + period * scale / 2, y - 9 * scale), new Point(x + period * scale, y - 4 * scale), true, true);
                    fctx.QuadraticBezierTo(new Point(x + period * scale / 2, y - 9 * scale), new Point(x + period * scale, y - 4 * scale), true, true);
                }
                ctx.LineTo(new Point(end, y), true, true);
            }
            geo.Freeze();
            foam.Freeze();

            var foamPen = new Pen(WaveFoam, 1.2 * scale);
            foamPen.Freeze();

            dc.PushClip(new RectangleGeometry(new Rect(x0, y - 12 * scale, Math.Max(0, x1 - x0), 12 * scale)));
            dc.PushTransform(Roll);
            dc.DrawGeometry(WaveBrush, null, geo);
            dc.DrawGeometry(null, foamPen, foam);
            dc.Pop();
            dc.Pop();
        }

        // ── Layouts ───────────────────────────────────────────────────────────
        public static void DrawCard(DrawingContext dc, Size size, int variant)
        {
            double w = size.Width, h = size.Height;
            if (w < 60 || h < 40) return;
            var s = Math.Clamp(Math.Min(w, h) / 115.0, 1.0, 1.5);

            if (w >= 500)
            {
                DrawSun(dc, w - 34 * s, 30 * s, s * 0.9);
                DrawSeagull(dc, w - 80 * s, 20 * s, s * 0.8);
                DrawSeagull(dc, w - 100 * s, 30 * s, s * 0.6);
                DrawWaves(dc, 4, w - 4, h - 3, s * 0.8);
                DrawPalm(dc, 20 * s, h - 6, s * 0.9);
                DrawCrab(dc, w - 26 * s, h - 12 * s, s * 0.9);
                return;
            }

            switch (variant % 6)
            {
                case 0:
                    DrawSun(dc, 4, 4, s * 0.8);
                    DrawStarfish(dc, w - 18 * s, h - 16 * s, s * 1.3, 15);
                    break;
                case 1:
                    DrawBeachBall(dc, w - 18 * s, h - 18 * s, s * 1.2);
                    DrawSeagull(dc, 24 * s, 18 * s, s * 1.1);
                    break;
                case 2:
                    DrawPalm(dc, 18 * s, h - 3, s * 0.85);
                    DrawShell(dc, w - 18 * s, 18 * s, s * 1.3, -20);
                    break;
                case 3:
                    DrawIceCream(dc, w - 18 * s, 26 * s, s * 1.2);
                    DrawCrab(dc, 22 * s, h - 11 * s, s * 1.1);
                    break;
                case 4:
                    DrawUmbrella(dc, w - 20 * s, h - 4, s * 0.85, 10);
                    DrawStarfish(dc, 18 * s, 18 * s, s * 1.1, -10);
                    break;
                default:
                    DrawWaves(dc, 3, w - 3, h - 3, s * 0.7);
                    DrawSeagull(dc, w - 26 * s, 18 * s, s * 1.1);
                    DrawShell(dc, 18 * s, h - 22 * s, s * 1.1, 15);
                    break;
            }
        }

        /// <summary>App header bar: sun with seagulls and clouds in the middle.</summary>
        public static void DrawHeader(DrawingContext dc, Size size)
        {
            double w = size.Width, h = size.Height;
            if (w < 200) return;
            var cx = w * 0.5;
            var cy = h * 0.5;
            DrawSun(dc, cx, cy, h / 48);
            DrawCloud(dc, cx - h * 0.9, cy + h * 0.1, h / 60);
            DrawCloud(dc, cx + h * 0.95, cy - h * 0.05, h / 75);
            DrawSeagull(dc, cx - h * 0.45, cy - h * 0.25, 1.0);
            DrawSeagull(dc, cx + h * 0.55, cy + h * 0.28, 0.8);
        }
    }
}
