#include "brep/Geometry.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace brep
{

Point3d CircleCurve::Eval(double t) const
{
  return m_center + m_xAxis * (m_radius * std::cos(t)) +
         m_yAxis * (m_radius * std::sin(t));
}

Vector3d CircleCurve::Tangent(double t) const
{
  return (m_xAxis * (-m_radius * std::sin(t)) + m_yAxis * (m_radius * std::cos(t)))
      .normalized();
}

namespace
{

struct HomogPt
{
    double x{0.0};
    double y{0.0};
    double z{0.0};
    double w{1.0};
};

[[nodiscard]] HomogPt LerpHomog(const HomogPt& a, const HomogPt& b, double u,
                                double omt) noexcept
{
    return HomogPt{omt * a.x + u * b.x, omt * a.y + u * b.y,
                   omt * a.z + u * b.z, omt * a.w + u * b.w};
}

[[nodiscard]] HomogPt EvalHomog(std::vector<HomogPt> pts, double t)
{
    if (pts.empty())
    {
        return {};
    }
    const double u = std::clamp(t, 0.0, 1.0);
    const double omt = 1.0 - u;
    while (pts.size() > 1)
    {
        std::vector<HomogPt> next;
        next.reserve(pts.size() - 1);
        for (std::size_t i = 0; i + 1 < pts.size(); ++i)
        {
            next.push_back(LerpHomog(pts[i], pts[i + 1], u, omt));
        }
        pts = std::move(next);
    }
    return pts.front();
}

[[nodiscard]] Point3d ProjectHomog(const HomogPt& h) noexcept
{
    if (std::abs(h.w) < 1e-15)
    {
        return {};
    }
    return Point3d{h.x / h.w, h.y / h.w, h.z / h.w};
}

}  // namespace

Point3d BezierCurve::Eval(double t) const
{
    if (m_cvs.empty())
    {
        return {};
    }
    std::vector<HomogPt> pts;
    pts.reserve(m_cvs.size());
    for (std::size_t i = 0; i < m_cvs.size(); ++i)
    {
        const double w = WeightAt(i);
        pts.push_back(HomogPt{w * m_cvs[i].x(), w * m_cvs[i].y(),
                              w * m_cvs[i].z(), w});
    }
    return ProjectHomog(EvalHomog(std::move(pts), t));
}

Vector3d BezierCurve::Tangent(double t) const
{
    if (m_cvs.size() < 2)
    {
        return Vector3d{1.0, 0.0, 0.0};
    }
    std::vector<HomogPt> homog;
    homog.reserve(m_cvs.size());
    for (std::size_t i = 0; i < m_cvs.size(); ++i)
    {
        const double w = WeightAt(i);
        homog.push_back(HomogPt{w * m_cvs[i].x(), w * m_cvs[i].y(),
                                w * m_cvs[i].z(), w});
    }
    const double n = static_cast<double>(homog.size() - 1U);
    std::vector<HomogPt> deriv;
    deriv.reserve(homog.size() - 1);
    for (std::size_t i = 0; i + 1 < homog.size(); ++i)
    {
        deriv.push_back(HomogPt{n * (homog[i + 1].x - homog[i].x),
                                n * (homog[i + 1].y - homog[i].y),
                                n * (homog[i + 1].z - homog[i].z),
                                n * (homog[i + 1].w - homog[i].w)});
    }
    const HomogPt h = EvalHomog(homog, t);
    const HomogPt dh = EvalHomog(std::move(deriv), t);
    const double w2 = h.w * h.w;
    Vector3d d;
    if (std::abs(w2) < 1e-30)
    {
        d = m_cvs.back() - m_cvs.front();
    }
    else
    {
        d = Vector3d{(dh.x * h.w - h.x * dh.w) / w2,
                     (dh.y * h.w - h.y * dh.w) / w2,
                     (dh.z * h.w - h.z * dh.w) / w2};
    }
    if (d.norm() < 1e-15)
    {
        d = m_cvs.back() - m_cvs.front();
    }
    if (d.norm() < 1e-15)
    {
        return Vector3d{1.0, 0.0, 0.0};
    }
    return d.normalized();
}

namespace
{

[[nodiscard]] int FindSpan(const std::vector<double>& u, int n, double t)
{
    if (t >= u[static_cast<std::size_t>(n + 1)])
    {
        return n;
    }
    int low = 3;
    int high = n + 1;
    int mid = (low + high) / 2;
    while (t < u[static_cast<std::size_t>(mid)] ||
           t >= u[static_cast<std::size_t>(mid + 1)])
    {
        if (t < u[static_cast<std::size_t>(mid)])
        {
            high = mid;
        }
        else
        {
            low = mid;
        }
        mid = (low + high) / 2;
    }
    return mid;
}

// Piegl & Tiller A2.2 — N[0..p] = N_{span-p} .. N_{span}.
void BasisFuns(int span, double t, int p, const std::vector<double>& u,
               std::vector<double>& n)
{
    n.assign(static_cast<std::size_t>(p) + 1U, 0.0);
    n[0] = 1.0;
    std::vector<double> left(static_cast<std::size_t>(p) + 1U, 0.0);
    std::vector<double> right(static_cast<std::size_t>(p) + 1U, 0.0);
    for (int j = 1; j <= p; ++j)
    {
        left[static_cast<std::size_t>(j)] =
            t - u[static_cast<std::size_t>(span + 1 - j)];
        right[static_cast<std::size_t>(j)] =
            u[static_cast<std::size_t>(span + j)] - t;
        double saved = 0.0;
        for (int r = 0; r < j; ++r)
        {
            const double denom =
                right[static_cast<std::size_t>(r + 1)] +
                left[static_cast<std::size_t>(j - r)];
            double temp = 0.0;
            if (std::abs(denom) > 0.0)
            {
                temp = n[static_cast<std::size_t>(r)] / denom;
            }
            n[static_cast<std::size_t>(r)] =
                saved + right[static_cast<std::size_t>(r + 1)] * temp;
            saved = left[static_cast<std::size_t>(j - r)] * temp;
        }
        n[static_cast<std::size_t>(j)] = saved;
    }
}

[[nodiscard]] double WeightAt(const std::vector<double>& weights,
                              std::size_t i) noexcept
{
    if (i < weights.size() && weights[i] > 0.0)
    {
        return weights[i];
    }
    return 1.0;
}

}  // namespace

std::vector<double> ClampedUniformKnots(int cvCount, int degree)
{
    const int n = cvCount - 1;
    const int knotCount = cvCount + degree + 1;
    std::vector<double> u(static_cast<std::size_t>(knotCount), 0.0);
    const int interior = n - degree;
    for (int j = 1; j <= interior; ++j)
    {
        u[static_cast<std::size_t>(degree + j)] =
            static_cast<double>(j) / static_cast<double>(interior + 1);
    }
    for (int i = n + 1; i < knotCount; ++i)
    {
        u[static_cast<std::size_t>(i)] = 1.0;
    }
    return u;
}

NurbsCurve::NurbsCurve(std::vector<Point3d> cvs, std::vector<double> weights,
                       std::vector<double> knots)
    : m_cvs(std::move(cvs)),
      m_weights(std::move(weights)),
      m_knots(std::move(knots))
{
    if (m_weights.empty())
    {
        m_weights.assign(m_cvs.size(), 1.0);
    }
    if (m_knots.empty() && !m_cvs.empty())
    {
        m_knots = ClampedUniformKnots(static_cast<int>(m_cvs.size()), 3);
    }
    if (!m_cvs.empty() && m_knots.size() > m_cvs.size())
    {
        m_degree = static_cast<int>(m_knots.size()) -
                   static_cast<int>(m_cvs.size()) - 1;
    }
}

Point3d NurbsCurve::Eval(double t) const
{
    if (m_cvs.empty() || m_knots.empty())
    {
        return {};
    }
    const int n = static_cast<int>(m_cvs.size()) - 1;
    const int p = m_degree;
    if (n < 0 || p < 0 ||
        static_cast<int>(m_knots.size()) < n + p + 2)
    {
        return {};
    }
    const double u = std::clamp(t, 0.0, 1.0);
    const int span = FindSpan(m_knots, n, u);
    std::vector<double> basis;
    BasisFuns(span, u, p, m_knots, basis);

    double ax = 0.0;
    double ay = 0.0;
    double az = 0.0;
    double wSum = 0.0;
    for (int j = 0; j <= p; ++j)
    {
        const int i = span - p + j;
        if (i < 0 || i > n)
        {
            continue;
        }
        const double ni = basis[static_cast<std::size_t>(j)];
        const double wi = WeightAt(m_weights, static_cast<std::size_t>(i));
        const double nwi = ni * wi;
        ax += nwi * m_cvs[static_cast<std::size_t>(i)].x();
        ay += nwi * m_cvs[static_cast<std::size_t>(i)].y();
        az += nwi * m_cvs[static_cast<std::size_t>(i)].z();
        wSum += nwi;
    }
    if (std::abs(wSum) < 1e-15)
    {
        return {};
    }
    return Point3d{ax / wSum, ay / wSum, az / wSum};
}

Vector3d NurbsCurve::Tangent(double t) const
{
    if (m_cvs.size() < 2 || m_knots.empty())
    {
        return Vector3d{1.0, 0.0, 0.0};
    }
    const int n = static_cast<int>(m_cvs.size()) - 1;
    const int p = m_degree;
    if (p < 1 || static_cast<int>(m_knots.size()) < n + p + 2)
    {
        return Vector3d{1.0, 0.0, 0.0};
    }
    const double u = std::clamp(t, 0.0, 1.0);
    const int span = FindSpan(m_knots, n, u);

    std::vector<double> basisP;
    BasisFuns(span, u, p, m_knots, basisP);
    std::vector<double> basisPm1;
    if (p >= 1)
    {
        BasisFuns(span, u, p - 1, m_knots, basisPm1);
    }

    auto basisPm1At = [&](int i) -> double
    {
        // Degree p-1 non-zeros: N_{span-(p-1)} .. N_{span}.
        const int first = span - (p - 1);
        const int idx = i - first;
        if (idx < 0 || idx >= p)
        {
            return 0.0;
        }
        return basisPm1[static_cast<std::size_t>(idx)];
    };

    auto derivBasis = [&](int i) -> double
    {
        // N'_i,p = p * (N_i,p-1/(u_{i+p}-u_i) - N_{i+1,p-1}/(u_{i+p+1}-u_{i+1}))
        double a = 0.0;
        const double d0 = m_knots[static_cast<std::size_t>(i + p)] -
                          m_knots[static_cast<std::size_t>(i)];
        if (std::abs(d0) > 0.0)
        {
            a = basisPm1At(i) / d0;
        }
        double b = 0.0;
        const double d1 = m_knots[static_cast<std::size_t>(i + p + 1)] -
                          m_knots[static_cast<std::size_t>(i + 1)];
        if (std::abs(d1) > 0.0)
        {
            b = basisPm1At(i + 1) / d1;
        }
        return static_cast<double>(p) * (a - b);
    };

    double ax = 0.0;
    double ay = 0.0;
    double az = 0.0;
    double wSum = 0.0;
    double axp = 0.0;
    double ayp = 0.0;
    double azp = 0.0;
    double wp = 0.0;
    for (int j = 0; j <= p; ++j)
    {
        const int i = span - p + j;
        if (i < 0 || i > n)
        {
            continue;
        }
        const double ni = basisP[static_cast<std::size_t>(j)];
        const double nip = derivBasis(i);
        const double wi = WeightAt(m_weights, static_cast<std::size_t>(i));
        const auto& cv = m_cvs[static_cast<std::size_t>(i)];
        const double nwi = ni * wi;
        const double nwip = nip * wi;
        ax += nwi * cv.x();
        ay += nwi * cv.y();
        az += nwi * cv.z();
        wSum += nwi;
        axp += nwip * cv.x();
        ayp += nwip * cv.y();
        azp += nwip * cv.z();
        wp += nwip;
    }

    Vector3d d;
    const double w2 = wSum * wSum;
    if (std::abs(w2) < 1e-30)
    {
        d = m_cvs.back() - m_cvs.front();
    }
    else
    {
        d = Vector3d{(axp * wSum - ax * wp) / w2,
                     (ayp * wSum - ay * wp) / w2,
                     (azp * wSum - az * wp) / w2};
    }
    if (d.norm() < 1e-15)
    {
        d = m_cvs.back() - m_cvs.front();
    }
    if (d.norm() < 1e-15)
    {
        return Vector3d{1.0, 0.0, 0.0};
    }
    return d.normalized();
}

std::vector<Point3d> SampleBezierPolyline(const BezierCurve& curve,
                                          int uniformSegments)
{
    const int segments = std::max(1, uniformSegments);
    std::vector<Point3d> points;
    points.reserve(static_cast<std::size_t>(segments) + 1U);
    for (int i = 0; i <= segments; ++i)
    {
        const double t =
            static_cast<double>(i) / static_cast<double>(segments);
        points.push_back(curve.Eval(t));
    }
    return points;
}

std::vector<Point3d> SampleNurbsPolyline(const NurbsCurve& curve,
                                         int uniformSegments)
{
    const int segments = std::max(1, uniformSegments);
    std::vector<Point3d> points;
    points.reserve(static_cast<std::size_t>(segments) + 1U);
    for (int i = 0; i <= segments; ++i)
    {
        const double t =
            static_cast<double>(i) / static_cast<double>(segments);
        points.push_back(curve.Eval(t));
    }
    return points;
}

Point2d PolylineCurve2d::Eval(double t) const
{
    if (m_points.empty())
    {
        return {};
    }
    if (m_points.size() == 1U)
    {
        return m_points.front();
    }
    const double scaled =
        std::clamp(t, 0.0, 1.0) * static_cast<double>(m_points.size() - 1U);
    const std::size_t segment = std::min(
        static_cast<std::size_t>(scaled), m_points.size() - 2U);
    const double local = scaled - static_cast<double>(segment);
    const Point2d& a = m_points[segment];
    const Point2d& b = m_points[segment + 1U];
    return Point2d{a.u() + (b.u() - a.u()) * local,
                   a.v() + (b.v() - a.v()) * local};
}

SphereSurface::SphereSurface(Point3d center, double radius)
    : m_center(center), m_radius(radius)
{
  if (!(m_radius > 0.0))
{
    throw std::invalid_argument("SphereSurface: radius must be positive");
  }
}

Point3d SphereSurface::Eval(double u, double v) const
{
  const double cv = std::cos(v);
  const double sv = std::sin(v);
  const double cu = std::cos(u);
  const double su = std::sin(u);
  return Point3d{m_center.x() + m_radius * cv * cu,
                 m_center.y() + m_radius * sv,
                 m_center.z() + m_radius * cv * su};
}

Vector3d SphereSurface::Normal(double u, double v) const
{
  const double cv = std::cos(v);
  const double sv = std::sin(v);
  const double cu = std::cos(u);
  const double su = std::sin(u);
  return Vector3d{cv * cu, sv, cv * su}.normalized();
}

Point2d SphereSurface::ParamOf(const Point3d& p) const
{
  const Vector3d d = p - m_center;
  const double len = d.norm();
  if (len < 1e-15)
  {
    return Point2d{0.0, 0.0};
  }
  const double inv = 1.0 / len;
  const double y = std::clamp(d.y() * inv, -1.0, 1.0);
  const double v = std::asin(y);
  const double horiz = std::sqrt(std::max(0.0, d.x() * d.x() + d.z() * d.z()));
  double u = 0.0;
  if (horiz > 1e-15)
  {
    u = std::atan2(d.z(), d.x());  // (-π, π]
    constexpr double kTwoPi = 2.0 * std::numbers::pi;
    if (u < 0.0) u += kTwoPi;
    if (u >= kTwoPi) u = 0.0;
  }
  return Point2d{u, v};
}

CylinderSurface::CylinderSurface(Point3d origin, Vector3d axis, double radius)
    : m_origin(origin), m_radius(radius)
{
  if (!(m_radius > 0.0))
{
    throw std::invalid_argument("CylinderSurface: radius must be positive");
  }
  const double len = axis.norm();
  if (!(len > 0.0))
  {
    throw std::invalid_argument("CylinderSurface: zero-length axis");
  }
  m_axis = axis / len;
  const Vector3d ref =
      std::abs(m_axis.x()) < 0.9 ? Vector3d{1, 0, 0} : Vector3d{0, 1, 0};
  m_xAxis = m_axis.cross(ref).normalized();
  m_yAxis = m_axis.cross(m_xAxis).normalized();
}

Point3d CylinderSurface::Eval(double u, double v) const
{
  return m_origin + m_axis * v + m_xAxis * (m_radius * std::cos(u)) +
         m_yAxis * (m_radius * std::sin(u));
}

Vector3d CylinderSurface::Normal(double u, double /*v*/) const
{
  return (m_xAxis * std::cos(u) + m_yAxis * std::sin(u)).normalized();
}

Point2d CylinderSurface::ParamOf(const Point3d& p) const
{
  const Vector3d d = p - m_origin;
  const double v = d.dot(m_axis);
  const Vector3d radial = d - m_axis * v;
  const double horiz = radial.norm();
  double u = 0.0;
  if (horiz > 1e-15)
  {
    u = std::atan2(radial.dot(m_yAxis), radial.dot(m_xAxis));
    constexpr double kTwoPi = 2.0 * std::numbers::pi;
    if (u < 0.0) u += kTwoPi;
    if (u >= kTwoPi) u = 0.0;
  }
  return Point2d{u, v};
}

void ApplyTransform(Point& p, const RigidTransform& t)
{
    p.SetXyz(t.TransformPoint(p.Xyz()));
}

bool ApplyTransform(Curve& c, const RigidTransform& t)
{
    switch (c.Kind())
    {
        case CurveKind::Line:
        {
            auto& line = static_cast<LineCurve&>(c);
            line.SetOrigin(t.TransformPoint(line.Origin()));
            const Vector3d dir = t.TransformVector(line.Direction());
            if (dir.norm() < 1e-15)
            {
                return false;
            }
            line.SetDirection(dir);
            return true;
        }
        case CurveKind::Circle:
        {
            auto& circle = static_cast<CircleCurve&>(c);
            circle.SetCenter(t.TransformPoint(circle.Center()));
            const Vector3d normal = t.TransformVector(circle.Normal());
            const Vector3d xAxis = t.TransformVector(circle.XAxis());
            const Vector3d yAxis = t.TransformVector(circle.YAxis());
            if (normal.norm() < 1e-15 || xAxis.norm() < 1e-15 ||
                yAxis.norm() < 1e-15)
            {
                return false;
            }
            circle.SetAxes(normal, xAxis, yAxis);
            return true;
        }
        case CurveKind::Bezier:
        {
            auto& bezier = static_cast<BezierCurve&>(c);
            std::vector<Point3d> cvs = bezier.Cvs();
            if (cvs.empty())
            {
                return false;
            }
            for (Point3d& p : cvs)
            {
                p = t.TransformPoint(p);
            }
            bezier.SetControlPoints(std::move(cvs), bezier.Weights());
            return true;
        }
        default:
            return false;
    }
}

bool ApplyTransform(Surface& s, const RigidTransform& t)
{
    switch (s.Kind())
    {
        case SurfaceKind::Plane:
        {
            auto& plane = static_cast<PlaneSurface&>(s);
            plane.SetOrigin(t.TransformPoint(plane.Origin()));
            const Vector3d uAxis = t.TransformVector(plane.UAxis());
            const Vector3d vAxis = t.TransformVector(plane.VAxis());
            if (uAxis.norm() < 1e-15 || vAxis.norm() < 1e-15)
            {
                return false;
            }
            plane.SetAxes(uAxis, vAxis);
            return true;
        }
        case SurfaceKind::Sphere:
        {
            auto& sphere = static_cast<SphereSurface&>(s);
            sphere.SetCenter(t.TransformPoint(sphere.Center()));
            return true;
        }
        case SurfaceKind::Cylinder:
        {
            auto& cyl = static_cast<CylinderSurface&>(s);
            cyl.SetOrigin(t.TransformPoint(cyl.Origin()));
            const Vector3d axis = t.TransformVector(cyl.Axis());
            const Vector3d xAxis = t.TransformVector(cyl.XAxis());
            const Vector3d yAxis = t.TransformVector(cyl.YAxis());
            if (axis.norm() < 1e-15 || xAxis.norm() < 1e-15 ||
                yAxis.norm() < 1e-15)
            {
                return false;
            }
            cyl.SetFrame(axis, xAxis, yAxis);
            return true;
        }
        default:
            return false;
    }
}

}  // namespace brep
