#pragma once

namespace Hypodermic
{
class ContainerBuilder;
}

namespace brep::viewer::bootstrap
{

/// One registration unit in the Composition Root (order via Order()).
class IModule
{
 public:
  virtual ~IModule() = default;

  [[nodiscard]] virtual int Order() const = 0;
  virtual void RegisterServices(Hypodermic::ContainerBuilder& builder) = 0;
};

}  // namespace brep::viewer::bootstrap
