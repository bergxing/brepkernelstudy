#pragma once

#include "brep/bool/Context.h"
#include "brep/bool/Result.h"
#include "brep/bool/Types.h"
#include "brep/Model.h"
#include "brep/Topology.h"

#include <memory>
#include <string_view>
#include <vector>

namespace brep::boolean
{

/// Optional analytic shortcut (AnalyticPair). Must not claim ops it cannot complete.
class IAnalyticFastPath
{
 public:
  virtual ~IAnalyticFastPath() = default;

  [[nodiscard]] virtual std::string_view Name() const = 0;

  [[nodiscard]] virtual bool CanHandle(BooleanOp op, const Body& a, const Body& b,
                                       const BooleanContext& ctx) const = 0;

  [[nodiscard]] virtual BooleanResult Evaluate(BooleanOp op, Model& model,
                                               const Body& a, const Body& b,
                                               const BooleanContext& ctx) = 0;
};

class AnalyticFastPathRegistry
{
 public:
  void Add(std::unique_ptr<IAnalyticFastPath> path);

  [[nodiscard]] const std::vector<std::unique_ptr<IAnalyticFastPath>>& Paths()
      const noexcept
  {
    return m_paths;
  }

 private:
  std::vector<std::unique_ptr<IAnalyticFastPath>> m_paths;
};

[[nodiscard]] AnalyticFastPathRegistry MakeDefaultFastPathRegistry();

}  // namespace brep::boolean
