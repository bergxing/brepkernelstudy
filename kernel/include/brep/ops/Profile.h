#pragma once

#include "brep/Math.h"
#include "brep/Model.h"
#include "brep/Plane.h"
#include "brep/sketch/Sketch.h"

#include <string>
#include <vector>

namespace brep::ops
{

struct Profile2d
{
    std::vector<Point2d> Outer;
    std::vector<std::vector<Point2d>> Holes;
};

/// Extract a closed outer polyline from sketch lines (point order heuristic).
Profile2d ExtractProfile(const sketch::Sketch& sketch);

struct ExtrudeSpec
{
    Profile2d Profile;
    Plane Plane{Plane::XzYUp()};
    double Distance{1.0};
    bool Symmetric{false};
    double Tolerance{1e-7};
    std::string Name{"extrude"};
};

/// Extrude a planar profile. Axis-aligned rectangular profiles become boxes;
/// other closed polylines use a prism builder (planar sides).
Body* Extrude(Model& model, const ExtrudeSpec& spec);

}  // namespace brep::ops
