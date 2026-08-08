#include "brep/brep.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
  using namespace brep;
  namespace fs = std::filesystem;

  auto doc = Document::create("xl_roundtrip");
  Part& part = doc->add_part("MainPart");
  Body* body = part.add_box(BoxSpec{
      .min = Point3d{0, 0, 0},
      .max = Point3d{2, 1, 3},
      .name = "box",
  });
  if (!body) {
    std::cerr << "add_box failed\n";
    return 1;
  }
  const Guid body_guid = body->guid;
  auto* feature = part.features().find_by_body(body_guid);
  if (!feature) {
    std::cerr << "missing feature\n";
    return 1;
  }
  part.edit_feature_params(feature->id(),
                           {{"Length", 4.0}, {"Width", 5.0}, {"Height", 2.5}});

  const fs::path path = fs::temp_directory_path() / "brep_xl_roundtrip.xl";
  auto saved = io::save_xl(*doc, path);
  if (!saved.ok) {
    std::cerr << "save failed: " << saved.error << "\n";
    return 1;
  }

  auto loaded = io::load_xl(path);
  if (!loaded.ok()) {
    std::cerr << "load failed: " << loaded.error << "\n";
    return 1;
  }

  Part* p2 = loaded.document->main_part();
  if (!p2) {
    std::cerr << "no part\n";
    return 1;
  }
  Body* b2 = p2->find_body(body_guid);
  if (!b2) {
    std::cerr << "body guid not restored\n";
    return 1;
  }
  auto* f2 = p2->features().find_by_body(body_guid);
  if (!f2 || f2->type_name() != "Box") {
    std::cerr << "box feature missing after load\n";
    return 1;
  }
  auto* box = static_cast<feat::BoxFeature*>(f2);
  const double len = p2->parameters().get(box->length_id()).value_or(0);
  const double wid = p2->parameters().get(box->width_id()).value_or(0);
  const double hei = p2->parameters().get(box->height_id()).value_or(0);
  if (std::abs(len - 4.0) > 1e-9 || std::abs(wid - 5.0) > 1e-9 ||
      std::abs(hei - 2.5) > 1e-9) {
    std::cerr << "param mismatch after load\n";
    return 1;
  }

  // Tamper CRC and expect failure.
  {
    std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
    f.seekp(-1, std::ios::end);
    char c = 0;
    f.seekg(-1, std::ios::end);
    f.read(&c, 1);
    c ^= 0x5A;
    f.seekp(-1, std::ios::end);
    f.write(&c, 1);
  }
  auto bad = io::load_xl(path);
  if (bad.ok()) {
    std::cerr << "tampered file should fail CRC\n";
    return 1;
  }

  fs::remove(path);
  std::cout << "xl_roundtrip ok\n";
  return 0;
}
