#pragma once

#include <QString>

#include <functional>
#include <vector>

namespace brep::viewer::commands
{

/// Lightweight undo/redo stack (callback-based; not a full B-Rep snapshot).
class DocumentHistory
{
 public:
  struct Entry
  {
    QString label;
    std::function<void()> undo;
    std::function<void()> redo;
  };

  void push(Entry entry);
  void clear();

  [[nodiscard]] bool can_undo() const noexcept
  {
      return m_index > 0; 
  }
  [[nodiscard]] bool can_redo() const noexcept
  {
    return m_index < int(m_entries.size());
  }

  [[nodiscard]] QString undo_label() const;
  [[nodiscard]] QString redo_label() const;

  bool undo();
  bool redo();

 private:
  std::vector<Entry> m_entries;
  int m_index{0};  // next push index; undo goes to m_index-1
};

}  // namespace brep::viewer::commands
