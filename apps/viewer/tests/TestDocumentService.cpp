#include "adapter/DocumentService.h"
#include "adapter/SceneAdapter.h"

#include "api/Core.h"
#include "api/Modeling.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace brep::viewer::adapter
{
namespace
{

std::filesystem::path temp_xl_path()
{
  const auto dir = std::filesystem::temp_directory_path();
  return dir / "brep_viewer_adapter_roundtrip.xl";
}

TEST(DocumentService, SaveLoadRoundtrip)
{
  auto doc = SceneAdapter::create_blank("Roundtrip");
  SceneAdapter scene(doc.get());
  ASSERT_NE(scene.add_box(BoxSpec{
                .min = Point3d{0.0, 0.0, 0.0},
                .max = Point3d{1.0, 2.0, 3.0},
                .name = "box",
            }),
            nullptr);

  const auto path = temp_xl_path();
  DocumentService docs;
  auto saved = docs.save(*doc, path.string());
  ASSERT_TRUE(saved.ok) << saved.error;

  auto loaded = docs.load(path.string());
  ASSERT_TRUE(loaded.ok) << loaded.error;
  ASSERT_NE(loaded.document, nullptr);
  SceneAdapter loaded_scene(loaded.document.get());
  ASSERT_NE(loaded_scene.main_part(), nullptr);
  EXPECT_EQ(loaded_scene.main_part()->model().bodies().size(), 1u);

  std::error_code ec;
  std::filesystem::remove(path, ec);
}

TEST(DocumentService, LoadMissingFails)
{
  DocumentService docs;
  auto loaded = docs.load("Z:/definitely/missing/brep_adapter_nope.xl");
  EXPECT_FALSE(loaded.ok);
  EXPECT_EQ(loaded.document, nullptr);
}

}  // namespace
}  // namespace brep::viewer::adapter
