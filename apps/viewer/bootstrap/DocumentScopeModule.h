#pragma once

#include <Hypodermic/ContainerBuilder.h>

namespace brep
{
class Document;
}

namespace brep::viewer::bootstrap
{

/// Document-scoped registrations (nested container); not part of application modules.
class DocumentScopeModule
{
 public:
  static void Register(Hypodermic::ContainerBuilder& builder,
                       brep::Document* document);
};

}  // namespace brep::viewer::bootstrap
