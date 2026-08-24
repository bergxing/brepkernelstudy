#pragma once

#include "brep/Topology.h"

#include <string>
#include <utility>
#include <vector>

namespace brep
{

struct ValidationIssue
{
  enum class IssueSeverity
  {
    Info,
    Warning,
    Error
  };
  IssueSeverity Severity{IssueSeverity::Error};
  std::string Where;
  std::string Message;
};

struct ValidationReport
{
  std::vector<ValidationIssue> Issues;

  [[nodiscard]] bool Ok() const noexcept
  {
    for (const auto& issue : Issues)
    {
      if (issue.Severity == ValidationIssue::IssueSeverity::Error)
      {
        return false;
      }
    }
    return true;
  }

  void Error(std::string where, std::string message)
  {
    Issues.push_back({ValidationIssue::IssueSeverity::Error, std::move(where),
                      std::move(message)});
  }

  void Warning(std::string where, std::string message)
  {
    Issues.push_back({ValidationIssue::IssueSeverity::Warning, std::move(where),
                      std::move(message)});
  }
};

/// Checks coedge cycle closure, partner involution, manifold degree-2 edges,
/// and vertex–curve endpoint proximity within tolerance.
ValidationReport ValidateBody(const Body& body);

}  // namespace brep
