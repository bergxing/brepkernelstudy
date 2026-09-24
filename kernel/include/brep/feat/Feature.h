#pragma once

#include "brep/feat/PrimitiveSpecs.h"
#include "brep/Guid.h"
#include "brep/param/Parameter.h"

#include <optional>
#include <string>
#include <string_view>

namespace brep
{
class Part;
}

namespace brep::feat
{

enum class FeatureStatus
{
    Ok,
    Suppressed,
    Failed,
    Dirty
};

struct FeatureId
{
    Guid Guid{};

    [[nodiscard]] friend bool operator==(const FeatureId& a,
                                         const FeatureId& b) noexcept
    {
        return a.Guid == b.Guid;
    }
    [[nodiscard]] friend bool operator!=(const FeatureId& a,
                                         const FeatureId& b) noexcept
    {
        return !(a == b);
    }
    [[nodiscard]] bool IsValid() const noexcept
    {
        return Guid.IsValid();
    }
};

class IFeature
{
public:
    virtual ~IFeature() = default;

    [[nodiscard]] virtual FeatureId Id() const = 0;
    [[nodiscard]] virtual std::string_view TypeName() const = 0;
    [[nodiscard]] virtual FeatureStatus Status() const = 0;
    virtual void SetStatus(FeatureStatus s) = 0;
    virtual void SetSuppressed(bool suppressed) = 0;
    [[nodiscard]] virtual bool Suppressed() const = 0;

    virtual void CollectParameters(param::ParameterStore& store) = 0;
    virtual bool Rebuild(Part& part, param::ParameterStore& params) = 0;

    [[nodiscard]] virtual Guid BodyGuid() const = 0;
    virtual void SetBodyGuid(Guid g) = 0;

    [[nodiscard]] virtual std::string DisplayName() const = 0;

    /// Reconstructable primitive payload, or nullopt for Boolean / Extrude /
    /// Sketch / etc. New primitive types override this; Viewer copy uses
    /// ISceneService::SpecFor, not per-type adapter methods.
    [[nodiscard]] virtual std::optional<PrimitiveSpec> ToPrimitiveSpec(
        const param::ParameterStore& params) const
    {
        (void)params;
        return std::nullopt;
    }
};

}  // namespace brep::feat

namespace std
{
template <>
struct hash<brep::feat::FeatureId>
{
    size_t operator()(const brep::feat::FeatureId& id) const noexcept
    {
        return hash<brep::Guid>{}(id.Guid);
    }
};
}  // namespace std
