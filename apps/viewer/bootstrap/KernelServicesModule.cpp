#include "bootstrap/KernelServicesModule.h"

#include "bootstrap/KernelServices.h"

#include "brep/bool/CompositeEvaluator.h"
#include "brep/bool/Evaluator.h"

#include <Hypodermic/ComponentContext.h>
#include <Hypodermic/ContainerBuilder.h>
#include <Hypodermic/SingleInstance.h>

namespace brep::viewer::bootstrap
{

KernelServicesModule::KernelServicesModule(AppConfig config)
    : m_config(std::move(config))
{
}

void KernelServicesModule::RegisterServices(
    Hypodermic::ContainerBuilder& builder)
{
  const BooleanBackend backend = m_config.booleanBackend;

  // Named backends for tests / future explicit resolveNamed("default"|"stub").
  builder.registerInstanceFactory(
      [](Hypodermic::ComponentContext&) -> std::shared_ptr<boolean::IBooleanEvaluator> {
        return boolean::MakeDefaultBooleanEvaluator();
      })
      .named("default");

  builder.registerInstanceFactory(
      [](Hypodermic::ComponentContext&) -> std::shared_ptr<boolean::IBooleanEvaluator> {
        return CreateBooleanEvaluator(BooleanBackend::Stub);
      })
      .named("stub");

  // Application Singleton — selected by AppConfig.booleanBackend.
  builder.registerInstanceFactory(
      [backend](Hypodermic::ComponentContext&) -> std::shared_ptr<boolean::IBooleanEvaluator> {
        return CreateBooleanEvaluator(backend);
      })
      .singleInstance();
}

}  // namespace brep::viewer::bootstrap
