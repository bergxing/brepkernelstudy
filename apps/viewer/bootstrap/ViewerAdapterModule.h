#pragma once

#include "bootstrap/IModule.h"

namespace brep::viewer::bootstrap
{

/// ISceneService / IDocumentService registration (Phase 3).
class ViewerAdapterModule final : public IModule
{
 public:
  [[nodiscard]] int Order() const override
  {
    return 20;
  }
  void RegisterServices(Hypodermic::ContainerBuilder& builder) override;
};

}  // namespace brep::viewer::bootstrap
