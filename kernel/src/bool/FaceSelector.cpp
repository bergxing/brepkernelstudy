#include "brep/bool/FaceSelector.h"

namespace brep::boolean
{

FaceRegion SolidClassToFaceRegion(SolidClass c) noexcept
{
  switch (c)
  {
  case SolidClass::In:
    return FaceRegion::In;
  case SolidClass::Out:
    return FaceRegion::Out;
  case SolidClass::On:
    return FaceRegion::On;
  }
  return FaceRegion::Unknown;
}

namespace
{

bool KeepA(BooleanOp op, FaceRegion region)
{
  switch (op)
  {
  case BooleanOp::Union:
    return region == FaceRegion::Out || region == FaceRegion::On;
  case BooleanOp::Subtract:
    return region == FaceRegion::Out;
  case BooleanOp::Intersect:
    return region == FaceRegion::In || region == FaceRegion::On;
  }
  return false;
}

bool KeepB(BooleanOp op, FaceRegion region)
{
  switch (op)
  {
  case BooleanOp::Union:
    return region == FaceRegion::Out || region == FaceRegion::On;
  case BooleanOp::Subtract:
    return region == FaceRegion::In;
  case BooleanOp::Intersect:
    return region == FaceRegion::In || region == FaceRegion::On;
  }
  return false;
}

}  // namespace

FaceSelection SelectCsgFaces(BooleanOp op,
                             const std::vector<FaceClassification>& aVsB,
                             const std::vector<FaceClassification>& bVsA)
{
  FaceSelection selection;
  selection.ReverseB = (op == BooleanOp::Subtract);
  for (const FaceClassification& fc : aVsB)
  {
    if (fc.TargetFace && KeepA(op, fc.Region))
    {
      selection.FromA.push_back(fc.TargetFace);
    }
  }
  for (const FaceClassification& fc : bVsA)
  {
    if (fc.TargetFace && KeepB(op, fc.Region))
    {
      selection.FromB.push_back(fc.TargetFace);
    }
  }
  return selection;
}

}  // namespace brep::boolean
