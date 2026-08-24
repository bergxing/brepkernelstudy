#include "brep/Geometry.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

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

}  // namespace brep
