#pragma once

// Kernel-internal primitive topology builders.
// Product code should create solids via Part + IFeature (BoxFeature, SphereFeature, …).
// Boolean fast paths and feature Rebuild() may include this header.

#include "brep/feat/PrimitiveSpecs.h"
#include "brep/Model.h"

namespace brep
{

Body* MakeBox(Model& model, const BoxSpec& spec = {});

Body* MakeSphere(Model& model, const SphereSpec& spec = {});

Body* MakeBezierWire(Model& model, const BezierSpec& spec = {});

Body* MakeNurbsCurveWire(Model& model, const NurbsCurveSpec& spec = {});

}  // namespace brep
