#pragma once

#include "adapter/IDocumentService.h"
#include "adapter/ISceneServiceFactory.h"

#include <memory>

namespace Hypodermic
{
class Container;
}

namespace brep::viewer::bootstrap
{

/// Application-scoped document persistence (ViewerAdapterModule).
[[nodiscard]] adapter::IDocumentService& ResolveDocumentService(
    const std::shared_ptr<Hypodermic::Container>& container);

/// Per-Document scene factory (not a singleton).
[[nodiscard]] adapter::ISceneServiceFactory& ResolveSceneServiceFactory(
    const std::shared_ptr<Hypodermic::Container>& container);

}  // namespace brep::viewer::bootstrap
