#include "brep/part.hpp"

#include "brep/document.hpp"
#include "brep/log.hpp"

namespace brep {

Part::Part(std::string part_name) : IObject(std::move(part_name)) {}

Body* Part::add_box(const BoxSpec& spec) {
  Body* body = make_box(model_, spec);
  register_body(*body);
  return body;
}

void Part::register_body(Body& body) {
  if (!document_) {
    BREP_WARN("Part::register_body: part '{}' has no Document; Guid {} not "
              "registered",
              name, body.guid.to_string());
    return;
  }
  document_->registry().add(body);
  document_->mark_dirty();
  BREP_INFO("registered Body '{}' guid={} on Document '{}'", body.name,
            body.guid.to_string(), document_->name);
}

}  // namespace brep
