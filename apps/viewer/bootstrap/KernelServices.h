#pragma once

#include "bootstrap/AppConfig.h"
#include "brep/bool/Evaluator.h"

#include <memory>

namespace Hypodermic
{
class Container;
}

namespace brep::viewer::bootstrap
{

[[nodiscard]] std::shared_ptr<boolean::IBooleanEvaluator> CreateBooleanEvaluator(
    BooleanBackend backend);

/// Resolve Application-scoped boolean evaluator (KernelServicesModule).
[[nodiscard]] std::shared_ptr<boolean::IBooleanEvaluator> ResolveBooleanEvaluator(
    const std::shared_ptr<Hypodermic::Container>& container);

/// Named backend: `"default"` or `"stub"`.
[[nodiscard]] std::shared_ptr<boolean::IBooleanEvaluator> ResolveNamedBooleanEvaluator(
    const std::shared_ptr<Hypodermic::Container>& container,
    const char* name);

}  // namespace brep::viewer::bootstrap
