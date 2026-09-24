#include "adapter/DocumentService.h"
#include "adapter/IDocumentService.h"
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

class FakeDocumentService final : public IDocumentService
{
 public:
  [[nodiscard]] LoadResult load(const std::string& path) const override
  {
    LoadResult result;
    if (path.find("missing") != std::string::npos)
    {
      result.error = "injected failure";
      return result;
    }
    result.ok = true;
    result.document = SceneAdapter::CreateBlank("FakeLoad");
    return result;
  }

  [[nodiscard]] SaveResult save(const brep::Document& /*doc*/,
                                const std::string& /*path*/) const override
  {
    return SaveResult{.ok = true};
  }

  [[nodiscard]] MeshCacheLoadResult load_mesh_cache(
      const std::string& /*xl_path*/, const Guid& /*doc_guid*/) const override
  {
    return MeshCacheLoadResult{};
  }
};

TEST(DocumentService, SaveLoadRoundtrip)
{
  auto doc = SceneAdapter::CreateBlank("Roundtrip");
  SceneAdapter scene(doc.get());
  ASSERT_NE(scene.AddPrimitive(BoxSpec{
                .Min = Point3d{0.0, 0.0, 0.0},
                .Max = Point3d{1.0, 2.0, 3.0},
                .Name = "box",
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
  ASSERT_NE(loaded_scene.MainPart(), nullptr);
  EXPECT_EQ(loaded_scene.MainPart()->Model().Bodies().size(), 1u);

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

TEST(DocumentService, FakeInjectedLoad)
{
  FakeDocumentService fake;
  auto missing = fake.load("Z:/missing/file.xl");
  EXPECT_FALSE(missing.ok);
  EXPECT_EQ(missing.document, nullptr);

  auto ok = fake.load("C:/temp/ok.xl");
  EXPECT_TRUE(ok.ok);
  ASSERT_NE(ok.document, nullptr);
  SceneAdapter scene(ok.document.get());
  EXPECT_NE(scene.MainPart(), nullptr);
}

}  // namespace
}  // namespace brep::viewer::adapter
