#include "api/Core.h"
#include "api/Modeling.h"
#include "api/Persistence.h"
#include "brep/Aspect.h"
#include "brep/bool/Pipeline.h"
#include "brep/build/PrimitiveBuild.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace brep
{
namespace
{

class RecordingAspect final : public IAspect
{
 public:
    explicit RecordingAspect(std::string name, std::vector<std::string>* log)
        : m_name(std::move(name))
        , m_log(log)
    {
    }

    std::string_view Name() const noexcept override
    {
        return m_name;
    }

    void Before(const AspectEvent& event) override
    {
        m_log->push_back(m_name + ":Before:" + std::string(event.Site));
    }

    void After(const AspectEvent& event) override
    {
        std::string line = m_name + ":After:" + std::string(event.Site);
        if (!event.Subject.empty())
        {
            line += ":" + std::string(event.Subject);
        }
        if (event.Failed)
        {
            line += ":Failed";
        }
        m_log->push_back(std::move(line));
    }

    void OnError(const AspectEvent& event, const std::exception& error) override
    {
        m_log->push_back(m_name + ":OnError:" + std::string(event.Site) + ":" +
                         error.what());
    }

 private:
    std::string m_name;
    std::vector<std::string>* m_log;
};

std::vector<std::unique_ptr<IAspect>> TwoRecorders(std::vector<std::string>* log)
{
    std::vector<std::unique_ptr<IAspect>> aspects;
    aspects.push_back(std::make_unique<RecordingAspect>("A", log));
    aspects.push_back(std::make_unique<RecordingAspect>("B", log));
    return aspects;
}

TEST(AspectChain, EmptyChainReturnsBodyResult)
{
    AspectChain chain;
    AspectEvent event;
    event.Site = "demo";
    const int value = chain.Invoke(event, [] { return 7; });
    EXPECT_EQ(value, 7);
}

TEST(AspectChain, BeforeForwardAfterReverseAndFailed)
{
    std::vector<std::string> log;
    AspectChain chain;
    chain.Reset(TwoRecorders(&log));
    AspectEvent event;
    event.Site = "demo";
    chain.Invoke(event, [&] {
        event.Failed = true;
        return 1;
    });
    ASSERT_EQ(log.size(), 4u);
    EXPECT_EQ(log[0], "A:Before:demo");
    EXPECT_EQ(log[1], "B:Before:demo");
    EXPECT_EQ(log[2], "B:After:demo:Failed");
    EXPECT_EQ(log[3], "A:After:demo:Failed");
}

TEST(AspectChain, ExceptionReversesOnErrorAndRethrows)
{
    std::vector<std::string> log;
    AspectChain chain;
    chain.Reset(TwoRecorders(&log));
    AspectEvent event;
    event.Site = "demo";
    EXPECT_THROW(chain.Invoke(event,
                              []() -> int { throw std::runtime_error("boom"); }),
                 std::runtime_error);
    ASSERT_EQ(log.size(), 4u);
    EXPECT_EQ(log[0], "A:Before:demo");
    EXPECT_EQ(log[1], "B:Before:demo");
    EXPECT_EQ(log[2], "B:OnError:demo:boom");
    EXPECT_EQ(log[3], "A:OnError:demo:boom");
}

TEST(ProcessAspectChain, ScopeRestoresPreviousChain)
{
    AspectChain previous;
    previous.Add(std::make_unique<RecordingAspect>("outer", nullptr));
    std::vector<std::string> ignored;
    ProcessAspectChain().Reset(previous.Release());

    {
        AspectChain replacement;
        std::vector<std::string> log;
        replacement.Add(std::make_unique<RecordingAspect>("inner", &log));
        ScopedProcessAspectChain guard(std::move(replacement));
        const std::vector<std::string> names = ProcessAspectChain().Names();
        ASSERT_EQ(names.size(), 1u);
        EXPECT_EQ(names[0], "inner");
    }

    const std::vector<std::string> restored = ProcessAspectChain().Names();
    ASSERT_EQ(restored.size(), 1u);
    EXPECT_EQ(restored[0], "outer");
    ProcessAspectChain().Reset({});
}

std::vector<std::string>* g_infoLines = nullptr;
std::vector<std::string>* g_errorLines = nullptr;

void TraceLines(bool isError, const char* message)
{
    std::vector<std::string>* target = isError ? g_errorLines : g_infoLines;
    if (target != nullptr && message != nullptr)
    {
        target->emplace_back(message);
    }
}

TEST(ProcessAspectChain, InstallIsIdempotent)
{
    auto saved = ProcessAspectChain().Release();
    InstallProcessAspects();
    EXPECT_EQ(ProcessAspectChain().Names(),
              (std::vector<std::string>{"Timing", "Logging", "Error"}));
    InstallProcessAspects();
    EXPECT_EQ(ProcessAspectChain().Names(),
              (std::vector<std::string>{"Timing", "Logging", "Error"}));
    ProcessAspectChain().Reset(std::move(saved));
}

TEST(ProcessAspectChain, DefaultAspectsTraceFailure)
{
    auto saved = ProcessAspectChain().Release();
    InstallProcessAspects();
    std::vector<std::string> info;
    std::vector<std::string> errors;
    g_infoLines = &info;
    g_errorLines = &errors;
    SetAspectTraceSinkForTest(&TraceLines);

    AspectEvent event;
    event.Site = "part.regenerate";
    event.Subject = "box";
    ProcessAspectChain().Invoke(event, [&] {
        event.Failed = true;
        event.Detail = "nope";
    });
    SetAspectTraceSinkForTest(nullptr);
    g_infoLines = nullptr;
    g_errorLines = nullptr;
    ProcessAspectChain().Reset(std::move(saved));

    bool sawTiming = false;
    for (const std::string& line : info)
    {
        if (line.find("part.regenerate") != std::string::npos &&
            line.find("elapsed_ms=") != std::string::npos)
        {
            sawTiming = true;
        }
    }
    EXPECT_TRUE(sawTiming);
    ASSERT_FALSE(errors.empty());
    EXPECT_NE(errors.front().find("box"), std::string::npos);
}

class SiteAspect final : public IAspect
{
 public:
    explicit SiteAspect(std::vector<std::string>* log)
        : m_log(log)
    {
    }

    std::string_view Name() const noexcept override
    {
        return "sites";
    }

    void Before(const AspectEvent&) override {}

    void After(const AspectEvent& event) override
    {
        m_log->push_back(std::string(event.Site) + ":" + std::string(event.Subject));
    }

    void OnError(const AspectEvent&, const std::exception&) override {}

 private:
    std::vector<std::string>* m_log;
};

TEST(KernelBoundaries, RegenerateTessellateBooleanAndXl)
{
    std::vector<std::string> log;
    AspectChain chain;
    chain.Add(std::make_unique<SiteAspect>(&log));
    ScopedProcessAspectChain guard(std::move(chain));

    auto doc = Document::Create("aspect");
    Part& part = doc->AddPart("Main");
    part.Regenerate();

    Model& model = part.Model();
    Body* boxA = MakeBox(model, BoxSpec{.Min = {0, 0, 0},
                                        .Max = {1, 1, 1},
                                        .Name = "A"});
    Body* boxB = MakeBox(model, BoxSpec{.Min = {0.2, 0.2, 0.2},
                                        .Max = {1.2, 1.2, 1.2},
                                        .Name = "B"});
    ASSERT_NE(boxA, nullptr);
    ASSERT_NE(boxB, nullptr);
    auto pipeline = boolean::MakeDefaultBooleanPipeline();
    pipeline->Evaluate(boolean::BooleanOp::Union, model, *boxA, *boxB, {});

    const TriangleMesh mesh = TessellateBody(*boxA);
    EXPECT_FALSE(mesh.Vertices.empty());
    int tessellateCount = 0;
    for (const std::string& line : log)
    {
        if (line.rfind("mesh.tessellate:", 0) == 0)
        {
            ++tessellateCount;
        }
    }
    EXPECT_EQ(tessellateCount, 1);

    const auto path =
        std::filesystem::temp_directory_path() / "brep_aspect_boundary.xl";
    const io::XlSaveResult saved = io::SaveXl(*doc, path);
    ASSERT_TRUE(saved.Ok) << saved.Error;
    const io::XlLoadResult loaded = io::LoadXl(path);
    ASSERT_TRUE(loaded.Ok()) << loaded.Error;

    auto countSite = [&](std::string_view site) {
        int count = 0;
        for (const std::string& line : log)
        {
            if (line.rfind(std::string(site) + ":", 0) == 0)
            {
                ++count;
            }
        }
        return count;
    };
    EXPECT_GE(countSite("part.regenerate"), 1);
    // SaveXl writes a mesh cache and tessellates every body again.
    EXPECT_GE(countSite("mesh.tessellate"), 1);
    EXPECT_EQ(countSite("io.xl.save"), 1);
    EXPECT_EQ(countSite("io.xl.load"), 1);

    std::vector<std::string> stages;
    for (const std::string& line : log)
    {
        if (line.rfind("boolean.pipeline:", 0) == 0)
        {
            stages.push_back(line.substr(std::string("boolean.pipeline:").size()));
        }
    }
    ASSERT_GE(stages.size(), 6u);
    EXPECT_EQ(stages[0], "Preprocess");
    EXPECT_EQ(stages[1], "Intersect");
    EXPECT_EQ(stages[2], "Imprint");
    EXPECT_EQ(stages[3], "Classify");
    EXPECT_EQ(stages[4], "Select");
    EXPECT_EQ(stages[5], "Build");
}

}  // namespace
}  // namespace brep
