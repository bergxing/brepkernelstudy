#pragma once

#include "adapter/ISceneService.h"
#include "adapter/SceneAdapter.h"

#include <functional>
#include <memory>

namespace brep
{
class Document;
}

namespace brep::viewer::adapter
{

/// Creates a per-Document ISceneService (1:1 with Document; not a singleton).
class ISceneServiceFactory
{
 public:
  virtual ~ISceneServiceFactory() = default;

  [[nodiscard]] virtual std::unique_ptr<ISceneService> Create(
      brep::Document* doc) = 0;
};

class DefaultSceneServiceFactory final : public ISceneServiceFactory
{
 public:
  [[nodiscard]] std::unique_ptr<ISceneService> Create(
      brep::Document* doc) override;
};

/// Ephemeral scene for undo/redo lambdas when only a factory pointer is captured.
inline void WithScene(
    ISceneServiceFactory* factory, brep::Document* doc,
    const std::function<void(ISceneService&)>& fn)
{
  if (factory != nullptr)
  {
    auto scene = factory->Create(doc);
    fn(*scene);
    return;
  }
  SceneAdapter fallback(doc);
  fn(fallback);
}

}  // namespace brep::viewer::adapter
