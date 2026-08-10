#include "brep/feat/feature_history.hpp"

#include "brep/feat/boolean_feature.hpp"
#include "brep/feat/box_feature.hpp"
#include "brep/feat/extrude_feature.hpp"
#include "brep/feat/sketch_feature.hpp"
#include "brep/feat/sphere_feature.hpp"
#include "brep/part.hpp"

namespace brep::feat {
namespace {

void regen(Part& part) { part.regenerate(); }

}  // namespace

void FeatureHistory::record(FeatureTransaction tx) {
  if (index_ < static_cast<int>(entries_.size())) {
    entries_.erase(entries_.begin() + index_, entries_.end());
  }
  entries_.push_back(std::move(tx));
  index_ = static_cast<int>(entries_.size());
}

void FeatureHistory::apply_and_record(Part& part, FeatureTransaction tx) {
  apply_forward(part, tx);
  record(std::move(tx));
}

void FeatureHistory::clear() {
  entries_.clear();
  index_ = 0;
}

bool FeatureHistory::undo(Part& part) {
  if (!can_undo()) return false;
  --index_;
  return apply_reverse(part, entries_[static_cast<std::size_t>(index_)]);
}

bool FeatureHistory::redo(Part& part) {
  if (!can_redo()) return false;
  const bool ok =
      apply_forward(part, entries_[static_cast<std::size_t>(index_)]);
  ++index_;
  return ok;
}

bool FeatureHistory::apply_forward(Part& part, FeatureTransaction& tx) {
  switch (tx.kind) {
    case TxKind::AppendFeature: {
      if (part.features().find(tx.feature)) {
        regen(part);
        return true;
      }
      if (tx.feature_type == "Box") {
        auto feature = BoxFeature::create(part.parameters(), tx.box_spec);
        tx.feature = part.features().append(std::move(feature));
        regen(part);
        return true;
      }
      if (tx.feature_type == "Sphere") {
        auto feature = SphereFeature::create(part.parameters(), tx.sphere_spec);
        tx.feature = part.features().append(std::move(feature));
        regen(part);
        return true;
      }
      if (tx.feature_type == "Sketch") {
        Point2d min{tx.box_spec.min.x(), tx.box_spec.min.z()};
        Point2d max{tx.box_spec.max.x(), tx.box_spec.max.z()};
        auto feature = SketchFeature::create_rectangle(
            part.parameters(),
            tx.sketch_name.empty() ? "Sketch" : tx.sketch_name, min, max);
        tx.feature = part.features().append(std::move(feature));
        regen(part);
        return true;
      }
      if (tx.feature_type == "Extrude") {
        auto feature = ExtrudeFeature::create(
            part.parameters(),
            tx.sketch_name.empty() ? "Extrude" : tx.sketch_name,
            tx.sketch_feature, tx.extrude_distance);
        tx.feature = part.features().append(std::move(feature));
        regen(part);
        return true;
      }
      if (tx.feature_type == "Boolean") {
        auto feature = BooleanFeature::create(
            tx.boolean_op, tx.target_feature_id, tx.tool_feature_id,
            tx.sketch_name.empty() ? "Boolean" : tx.sketch_name);
        tx.feature = part.features().append(std::move(feature));
        regen(part);
        return true;
      }
      return false;
    }
    case TxKind::RemoveFeature: {
      part.remove_feature(tx.feature);
      return true;
    }
    case TxKind::EditParameters: {
      for (const auto& [id, value] : tx.param_after) {
        part.parameters().set(id, value);
      }
      if (auto* f = part.features().find(tx.feature)) {
        part.features().mark_dirty_from(f->id());
      } else {
        part.features().mark_all_dirty();
      }
      regen(part);
      return true;
    }
    case TxKind::SuppressFeature: {
      if (auto* f = part.features().find(tx.feature)) {
        f->set_suppressed(true);
        part.features().mark_dirty_from(tx.feature);
        regen(part);
        return true;
      }
      return false;
    }
    case TxKind::UnsuppressFeature: {
      if (auto* f = part.features().find(tx.feature)) {
        f->set_suppressed(false);
        f->set_status(FeatureStatus::Dirty);
        part.features().mark_dirty_from(tx.feature);
        regen(part);
        return true;
      }
      return false;
    }
  }
  return false;
}

bool FeatureHistory::apply_reverse(Part& part, FeatureTransaction& tx) {
  switch (tx.kind) {
    case TxKind::AppendFeature:
      return part.remove_feature(tx.feature);
    case TxKind::RemoveFeature: {
      FeatureTransaction redo = tx;
      redo.kind = TxKind::AppendFeature;
      const bool ok = apply_forward(part, redo);
      tx.feature = redo.feature;
      return ok;
    }
    case TxKind::EditParameters: {
      for (const auto& [id, value] : tx.param_before) {
        part.parameters().set(id, value);
      }
      if (auto* f = part.features().find(tx.feature)) {
        part.features().mark_dirty_from(f->id());
      } else {
        part.features().mark_all_dirty();
      }
      regen(part);
      return true;
    }
    case TxKind::SuppressFeature: {
      FeatureTransaction u = tx;
      u.kind = TxKind::UnsuppressFeature;
      return apply_forward(part, u);
    }
    case TxKind::UnsuppressFeature: {
      FeatureTransaction s = tx;
      s.kind = TxKind::SuppressFeature;
      return apply_forward(part, s);
    }
  }
  return false;
}

}  // namespace brep::feat
