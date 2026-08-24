#include "brep/feat/FeatureHistory.h"

#include "brep/feat/BooleanFeature.h"
#include "brep/feat/BoxFeature.h"
#include "brep/feat/ExtrudeFeature.h"
#include "brep/feat/SketchFeature.h"
#include "brep/feat/SphereFeature.h"
#include "brep/Part.h"

namespace brep::feat
{
namespace
{

void Regen(Part& part)
{
    part.Regenerate();
}

}  // namespace

void FeatureHistory::Record(FeatureTransaction tx)
{
    if (m_index < static_cast<int>(m_entries.size()))
{
        m_entries.erase(m_entries.begin() + m_index, m_entries.end());
    }
    m_entries.push_back(std::move(tx));
    m_index = static_cast<int>(m_entries.size());
}

void FeatureHistory::ApplyAndRecord(Part& part, FeatureTransaction tx)
{
    ApplyForward(part, tx);
    Record(std::move(tx));
}

void FeatureHistory::Clear()
{
    m_entries.clear();
    m_index = 0;
}

bool FeatureHistory::Undo(Part& part)
{
    if (!CanUndo())
{
        return false;
    }
    --m_index;
    return ApplyReverse(part, m_entries[static_cast<std::size_t>(m_index)]);
}

bool FeatureHistory::Redo(Part& part)
{
    if (!CanRedo())
{
        return false;
    }
    const bool ok = ApplyForward(part, m_entries[static_cast<std::size_t>(m_index)]);
    ++m_index;
    return ok;
}

bool FeatureHistory::ApplyForward(Part& part, FeatureTransaction& tx)
{
    switch (tx.Kind)
{
        case TxKind::AppendFeature:
{
            if (part.Features().Find(tx.Feature))
{
                Regen(part);
                return true;
            }
            if (tx.FeatureType == "Box")
            {
                auto feature = BoxFeature::Create(part.Parameters(), tx.Box);
                tx.Feature = part.Features().Append(std::move(feature));
                Regen(part);
                return true;
            }
            if (tx.FeatureType == "Sphere")
            {
                auto feature = SphereFeature::Create(part.Parameters(), tx.Sphere);
                tx.Feature = part.Features().Append(std::move(feature));
                Regen(part);
                return true;
            }
            if (tx.FeatureType == "Sketch")
            {
                Point2d min{tx.Box.Min.x(), tx.Box.Min.z()};
                Point2d max{tx.Box.Max.x(), tx.Box.Max.z()};
                auto feature = SketchFeature::CreateRectangle(
                    part.Parameters(), tx.SketchName.empty() ? "Sketch" : tx.SketchName, min, max);
                tx.Feature = part.Features().Append(std::move(feature));
                Regen(part);
                return true;
            }
            if (tx.FeatureType == "Extrude")
            {
                auto feature = ExtrudeFeature::Create(
                    part.Parameters(), tx.SketchName.empty() ? "Extrude" : tx.SketchName,
                    tx.SketchFeatureId, tx.ExtrudeDistance);
                tx.Feature = part.Features().Append(std::move(feature));
                Regen(part);
                return true;
            }
            if (tx.FeatureType == "Boolean")
            {
                auto feature = BooleanFeature::Create(tx.Op, tx.TargetFeatureId, tx.ToolFeatureId,
                                                      tx.SketchName.empty() ? "Boolean" : tx.SketchName);
                tx.Feature = part.Features().Append(std::move(feature));
                Regen(part);
                return true;
            }
            return false;
        }
        case TxKind::RemoveFeature:
            part.RemoveFeature(tx.Feature);
            return true;
        case TxKind::EditParameters:
            for (const auto& [id, value] : tx.ParamAfter)
        {
                part.Parameters().Set(id, value);
            }
            if (auto* f = part.Features().Find(tx.Feature))
            {
                part.Features().MarkDirtyFrom(f->Id());
            }
            else
            {
                part.Features().MarkAllDirty();
            }
            Regen(part);
            return true;
        case TxKind::SuppressFeature:
            if (auto* f = part.Features().Find(tx.Feature))
        {
                f->SetSuppressed(true);
                part.Features().MarkDirtyFrom(tx.Feature);
                Regen(part);
                return true;
            }
            return false;
        case TxKind::UnsuppressFeature:
            if (auto* f = part.Features().Find(tx.Feature))
        {
                f->SetSuppressed(false);
                f->SetStatus(FeatureStatus::Dirty);
                part.Features().MarkDirtyFrom(tx.Feature);
                Regen(part);
                return true;
            }
            return false;
    }
    return false;
}

bool FeatureHistory::ApplyReverse(Part& part, FeatureTransaction& tx)
{
    switch (tx.Kind)
{
        case TxKind::AppendFeature:
            return part.RemoveFeature(tx.Feature);
        case TxKind::RemoveFeature:
{
            FeatureTransaction redo = tx;
            redo.Kind = TxKind::AppendFeature;
            const bool ok = ApplyForward(part, redo);
            tx.Feature = redo.Feature;
            return ok;
        }
        case TxKind::EditParameters:
            for (const auto& [id, value] : tx.ParamBefore)
        {
                part.Parameters().Set(id, value);
            }
            if (auto* f = part.Features().Find(tx.Feature))
            {
                part.Features().MarkDirtyFrom(f->Id());
            }
            else
            {
                part.Features().MarkAllDirty();
            }
            Regen(part);
            return true;
        case TxKind::SuppressFeature:
        {
            FeatureTransaction u = tx;
            u.Kind = TxKind::UnsuppressFeature;
            return ApplyForward(part, u);
        }
        case TxKind::UnsuppressFeature:
        {
            FeatureTransaction s = tx;
            s.Kind = TxKind::SuppressFeature;
            return ApplyForward(part, s);
        }
    }
    return false;
}

}  // namespace brep::feat
