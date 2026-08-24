#include "brep/Document.h"

#include "brep/Log.h"

namespace brep
{

std::unique_ptr<Document> Document::Create(std::string title)
{
  return std::make_unique<Document>(std::move(title));
}

Document::Document(std::string title) : IObject(std::move(title))
{
  m_registry.Add(*this);
  BREP_INFO("Document created name='{}' guid={}", Name, Guid.ToString());
}

Part& Document::AddPart(std::string part_name)
{
  auto part = std::make_unique<Part>(std::move(part_name));
  part->SetDocument(this);
  m_registry.Add(*part);
  Part& ref = *part;
  m_parts.push_back(std::move(part));
  MarkDirty();
  BREP_INFO("Document '{}' added Part '{}' guid={}", Name, ref.Name,
            ref.Guid.ToString());
  return ref;
}

Part* Document::MainPart() noexcept
{
  return m_parts.empty() ? nullptr : m_parts.front().get();
}

const Part* Document::MainPart() const noexcept
{
  return m_parts.empty() ? nullptr : m_parts.front().get();
}

void Document::Clear()
{
  m_registry.Clear();
  m_parts.clear();
  m_assembly = asm_::Assembly{};
  m_registry.Add(*this);
  m_path.clear();
  m_dirty = false;
  BREP_INFO("Document '{}' cleared guid={}", Name, Guid.ToString());
}

}  // namespace brep
