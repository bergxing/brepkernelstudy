#include "commands/document_history.hpp"

#include "brep/log.hpp"

namespace brep::viewer::commands {

void DocumentHistory::push(Entry entry) {
  if (index_ < int(entries_.size())) {
    entries_.erase(entries_.begin() + index_, entries_.end());
  }
  entries_.push_back(std::move(entry));
  index_ = int(entries_.size());
  BREP_INFO("history push '{}' (size={})",
            entries_.back().label.toStdString(), entries_.size());
}

void DocumentHistory::clear() {
  entries_.clear();
  index_ = 0;
  BREP_INFO("history cleared");
}

QString DocumentHistory::undo_label() const {
  if (!can_undo()) return {};
  return entries_[static_cast<std::size_t>(index_ - 1)].label;
}

QString DocumentHistory::redo_label() const {
  if (!can_redo()) return {};
  return entries_[static_cast<std::size_t>(index_)].label;
}

bool DocumentHistory::undo() {
  if (!can_undo()) return false;
  --index_;
  auto& e = entries_[static_cast<std::size_t>(index_)];
  BREP_INFO("history undo '{}'", e.label.toStdString());
  if (e.undo) e.undo();
  return true;
}

bool DocumentHistory::redo() {
  if (!can_redo()) return false;
  auto& e = entries_[static_cast<std::size_t>(index_)];
  BREP_INFO("history redo '{}'", e.label.toStdString());
  if (e.redo) e.redo();
  ++index_;
  return true;
}

}  // namespace brep::viewer::commands
