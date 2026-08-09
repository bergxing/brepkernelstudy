#include "brep/document.hpp"

#include "brep/log.hpp"

namespace brep {

std::unique_ptr<Document> Document::create(std::string title) {
  return std::make_unique<Document>(std::move(title));
}

Document::Document(std::string title) : IObject(std::move(title)) {
  registry_.add(*this);
  BREP_INFO("Document created name='{}' guid={}", name, guid.to_string());
}

Part& Document::add_part(std::string part_name) {
  auto part = std::make_unique<Part>(std::move(part_name));
  part->set_document(this);
  registry_.add(*part);
  Part& ref = *part;
  parts_.push_back(std::move(part));
  mark_dirty();
  BREP_INFO("Document '{}' added Part '{}' guid={}", name, ref.name,
            ref.guid.to_string());
  return ref;
}

Part* Document::main_part() noexcept {
  return parts_.empty() ? nullptr : parts_.front().get();
}

const Part* Document::main_part() const noexcept {
  return parts_.empty() ? nullptr : parts_.front().get();
}

void Document::clear() {
  registry_.clear();
  parts_.clear();
  assembly_ = asm_::Assembly{};
  registry_.add(*this);
  path_.clear();
  dirty_ = false;
  BREP_INFO("Document '{}' cleared guid={}", name, guid.to_string());
}

}  // namespace brep
