#pragma once

#include "brep/builder.hpp"
#include "brep/iobject.hpp"
#include "brep/model.hpp"

namespace brep {

class Document;

/// Part: owns B-Rep Model (geometry + topology pools) and Body roots.
class Part final : public IObject {
 public:
  explicit Part(std::string part_name = "Part");

  [[nodiscard]] ObjectKind kind() const noexcept override {
    return ObjectKind::Part;
  }

  [[nodiscard]] Model& model() noexcept { return model_; }
  [[nodiscard]] const Model& model() const noexcept { return model_; }

  [[nodiscard]] Document* document() noexcept { return document_; }
  [[nodiscard]] const Document* document() const noexcept { return document_; }

  /// Build an AABB solid box Body, register its Guid on the owning Document.
  Body* add_box(const BoxSpec& spec = {});

  /// Register an existing Body (created via model()) on the Document registry.
  void register_body(Body& body);

 private:
  friend class Document;
  void set_document(Document* doc) noexcept { document_ = doc; }

  Document* document_{nullptr};
  Model model_;
};

}  // namespace brep
