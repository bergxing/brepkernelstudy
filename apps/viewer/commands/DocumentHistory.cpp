#include "commands/DocumentHistory.h"

#include "api/Core.h"

namespace brep::viewer::commands
{

void DocumentHistory::push(Entry entry)
{
  if (m_index < int(m_entries.size()))
{
    m_entries.erase(m_entries.begin() + m_index, m_entries.end());
  }
  m_entries.push_back(std::move(entry));
  m_index = int(m_entries.size());
  BREP_INFO("history push '{}' (size={})",
            m_entries.back().label.toStdString(), m_entries.size());
}

void DocumentHistory::clear()
{
  m_entries.clear();
  m_index = 0;
  BREP_INFO("history cleared");
}

QString DocumentHistory::undo_label() const
{
  if (!can_undo()) return {};
  return m_entries[static_cast<std::size_t>(m_index - 1)].label;
}

QString DocumentHistory::redo_label() const
{
  if (!can_redo()) return {};
  return m_entries[static_cast<std::size_t>(m_index)].label;
}

bool DocumentHistory::undo()
{
  if (!can_undo()) return false;
  --m_index;
  auto& e = m_entries[static_cast<std::size_t>(m_index)];
  BREP_INFO("history undo '{}'", e.label.toStdString());
  if (e.undo) e.undo();
  return true;
}

bool DocumentHistory::redo()
{
  if (!can_redo()) return false;
  auto& e = m_entries[static_cast<std::size_t>(m_index)];
  BREP_INFO("history redo '{}'", e.label.toStdString());
  if (e.redo) e.redo();
  ++m_index;
  return true;
}

}  // namespace brep::viewer::commands
