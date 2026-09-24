#include "bootstrap/ApplicationContainer.h"
#include "bootstrap/DocumentScope.h"
#include "bootstrap/ViewerAdapterServices.h"

#include "adapter/SceneAdapter.h"

#include <gtest/gtest.h>

namespace brep::viewer::bootstrap
{
namespace
{

TEST(DocumentScope, SceneOneToOneWithDocument)
{
  const auto app = BuildApplicationContainer({});
  ASSERT_NE(app, nullptr);

  auto doc_a = adapter::SceneAdapter::CreateBlank("DocA");
  auto doc_b = adapter::SceneAdapter::CreateBlank("DocB");
  ASSERT_NE(doc_a, nullptr);
  ASSERT_NE(doc_b, nullptr);

  auto scope_a = DocumentScope::Create(app, doc_a.get());
  auto scope_b = DocumentScope::Create(app, doc_b.get());
  ASSERT_NE(scope_a, nullptr);
  ASSERT_NE(scope_b, nullptr);

  EXPECT_EQ(scope_a->Document(), doc_a.get());
  EXPECT_EQ(scope_b->Document(), doc_b.get());
  EXPECT_EQ(scope_a->scene().Document(), doc_a.get());
  EXPECT_EQ(scope_b->scene().Document(), doc_b.get());
  EXPECT_NE(&scope_a->scene(), &scope_b->scene());
}

TEST(DocumentScope, RebindUpdatesSceneDocument)
{
  const auto app = BuildApplicationContainer({});
  auto doc_first = adapter::SceneAdapter::CreateBlank("First");
  auto doc_second = adapter::SceneAdapter::CreateBlank("Second");

  auto scope = DocumentScope::Create(app, doc_first.get());
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->scene().Document(), doc_first.get());

  scope->Rebind(doc_second.get());
  EXPECT_EQ(scope->Document(), doc_second.get());
  EXPECT_EQ(scope->scene().Document(), doc_second.get());
}

TEST(DocumentScope, NestedContainerInheritsApplicationServices)
{
  const auto app = BuildApplicationContainer({});
  auto doc = adapter::SceneAdapter::CreateBlank("Nested");
  auto scope = DocumentScope::Create(app, doc.get());
  ASSERT_NE(scope, nullptr);

  auto& docs = ResolveDocumentService(app);
  (void)docs;
  EXPECT_NE(scope->scene().MainPart(), nullptr);
}

}  // namespace
}  // namespace brep::viewer::bootstrap
