#include "bootstrap/KernelServices.h"

#include "api/Core.h"

#include <Hypodermic/Container.h>

namespace brep::viewer::bootstrap
{

std::shared_ptr<boolean::IBooleanEvaluator> CreateBooleanEvaluator(
    BooleanBackend backend)
{
  switch (backend)
  {
    case BooleanBackend::Stub:
      return std::shared_ptr<boolean::IBooleanEvaluator>(
          boolean::MakeStubBooleanEvaluator());
    case BooleanBackend::Occt:
      BREP_WARN(
          "Boolean backend 'occt' is not implemented; using default "
          "composite evaluator");
      [[fallthrough]];
    case BooleanBackend::Default:
    default:
      return boolean::MakeDefaultBooleanEvaluator();
  }
}

std::shared_ptr<boolean::IBooleanEvaluator> ResolveBooleanEvaluator(
    const std::shared_ptr<Hypodermic::Container>& container)
{
  if (!container)
  {
    return CreateBooleanEvaluator(BooleanBackend::Default);
  }
  return container->resolve<boolean::IBooleanEvaluator>();
}

std::shared_ptr<boolean::IBooleanEvaluator> ResolveNamedBooleanEvaluator(
    const std::shared_ptr<Hypodermic::Container>& container,
    const char* name)
{
  if (!container || name == nullptr)
  {
    return CreateBooleanEvaluator(BooleanBackend::Default);
  }
  return container->resolveNamed<boolean::IBooleanEvaluator>(name);
}

}  // namespace brep::viewer::bootstrap
