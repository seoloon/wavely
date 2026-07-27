using Avalonia;
using Avalonia.Media;

namespace Wavely.App.Core;

/// <summary>
/// Builds superellipse ("squircle") clip geometries: |x/a|^n + |y/a|^n = 1, n=5 - noticeably more
/// square than an ellipse, much rounder than a rounded-rect corner. Avalonia has no built-in
/// rounded-rect Geometry type (only Border's own corner-radius clipping, which only produces
/// circular corners, not this curve), so this samples the parametric form of the curve into a
/// closed polygon rather than composing arcs.
/// </summary>
public static class SquircleGeometry
{
    private const double Exponent = 5.0;
    private const int SamplePoints = 64;

    private static readonly Dictionary<double, StreamGeometry> Cache = new();

    /// <summary>Returns a cached squircle geometry for a size x size square, in the owning
    /// Visual's local coordinate space (top-left origin). Cached by size so resizing the widget
    /// (50%-150% scale) doesn't rebuild the polygon every frame.</summary>
    public static StreamGeometry ForSize(double size)
    {
        if (Cache.TryGetValue(size, out var cached))
        {
            return cached;
        }

        var geometry = Build(size);
        Cache[size] = geometry;
        return geometry;
    }

    private static StreamGeometry Build(double size)
    {
        var radius = size / 2.0;
        var geometry = new StreamGeometry();
        using (var context = geometry.Open())
        {
            context.BeginFigure(PointOnCurve(0, radius), isFilled: true);
            for (var i = 1; i <= SamplePoints; i++)
            {
                var t = i * (2.0 * Math.PI / SamplePoints);
                context.LineTo(PointOnCurve(t, radius));
            }
            context.EndFigure(isClosed: true);
        }
        return geometry;

        Point PointOnCurve(double t, double a)
        {
            var cos = Math.Cos(t);
            var sin = Math.Sin(t);
            var x = Math.Sign(cos) * Math.Pow(Math.Abs(cos), 2.0 / Exponent) * a;
            var y = Math.Sign(sin) * Math.Pow(Math.Abs(sin), 2.0 / Exponent) * a;
            return new Point(a + x, a + y);
        }
    }
}
