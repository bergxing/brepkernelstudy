#pragma once

#include "brep/topology.hpp"

#include <string>
#include <vector>

namespace brep {

struct ValidationIssue {
  enum class Severity { Info, Warning, Error };
  Severity severity{Severity::Error};
  std::string where;
  std::string message;
};

struct ValidationReport {
  std::vector<ValidationIssue> issues;

  [[nodiscard]] bool ok() const noexcept {
    for (const auto& i : issues) {
      if (i.severity == ValidationIssue::Severity::Error) return false;
    }
    return true;
  }
  void error(std::string where, std::string message) {
    issues.push_back({ValidationIssue::Severity::Error, std::move(where),
                      std::move(message)});
  }
  void warning(std::string where, std::string message) {
    issues.push_back({ValidationIssue::Severity::Warning, std::move(where),
                      std::move(message)});
  }
};

/// Checks coedge cycle closure, partner involution, manifold degree-2 edges,
/// and vertex–curve endpoint proximity within tolerance.
ValidationReport validate_body(const Body& body);

}  // namespace brep
