#pragma once

#include "brep/Aspect.h"

namespace brep::viewer::commands
{

class StatusAspect final : public brep::IAspect
{
 public:
    [[nodiscard]] std::string_view Name() const noexcept override;

    void Before(const brep::AspectEvent& event) override;
    void After(const brep::AspectEvent& event) override;
    void OnError(const brep::AspectEvent& event,
                 const std::exception& error) override;
};

}  // namespace brep::viewer::commands
