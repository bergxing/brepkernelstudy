#pragma once

// Umbrella header for boolean evaluation types and evaluator interface.

#include "brep/bool/context.hpp"
#include "brep/bool/evaluator.hpp"
#include "brep/bool/intersect_plane_plane.hpp"
#include "brep/bool/intersect_plane_sphere.hpp"
#include "brep/bool/intersect_sphere_sphere.hpp"
#include "brep/bool/intersect_plane_cylinder.hpp"
#include "brep/bool/intersect_sphere_cylinder.hpp"
#include "brep/bool/box_boolean.hpp"
#include "brep/bool/planar_boolean.hpp"
#include "brep/bool/planar_recognize.hpp"
#include "brep/bool/sphere_box_boolean.hpp"
#include "brep/bool/sphere_recognize.hpp"
#include "brep/bool/classify.hpp"
#include "brep/bool/broadphase.hpp"
#include "brep/bool/result.hpp"
#include "brep/bool/types.hpp"
