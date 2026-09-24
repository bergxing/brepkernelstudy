#include "brep/Aspect.h"

#include "brep/Log.h"

#include <spdlog/fmt/fmt.h>

namespace brep
{
namespace
{

AspectTraceSink g_traceSink = nullptr;

void WriteAspectTrace(bool isError, std::string_view message)
{
    const std::string owned(message);
    if (isError)
    {
        BREP_ERROR("{}", owned);
    }
    else
    {
        BREP_INFO("{}", owned);
    }
    if (g_traceSink != nullptr)
    {
        g_traceSink(isError, owned.c_str());
    }
}

}  // namespace

void AspectChain::Add(std::unique_ptr<IAspect> aspect)
{
    if (aspect)
    {
        m_aspects.push_back(std::move(aspect));
    }
}

std::vector<std::string> AspectChain::Names() const
{
    std::vector<std::string> names;
    names.reserve(m_aspects.size());
    for (const auto& aspect : m_aspects)
    {
        names.emplace_back(aspect->Name());
    }
    return names;
}

std::vector<std::unique_ptr<IAspect>> AspectChain::Release()
{
    std::vector<std::unique_ptr<IAspect>> aspects;
    aspects.swap(m_aspects);
    return aspects;
}

void AspectChain::Reset(std::vector<std::unique_ptr<IAspect>> aspects)
{
    m_aspects = std::move(aspects);
}

AspectChain& ProcessAspectChain()
{
    static AspectChain chain;
    return chain;
}

void InstallProcessAspects()
{
    AspectChain& chain = ProcessAspectChain();
    chain.Reset({});
    chain.Add(std::make_unique<TimingAspect>());
    chain.Add(std::make_unique<LoggingAspect>());
    chain.Add(std::make_unique<ErrorAspect>());
}

ScopedProcessAspectChain::ScopedProcessAspectChain(AspectChain replacement)
    : m_previous(ProcessAspectChain().Release())
{
    ProcessAspectChain().Reset(replacement.Release());
}

ScopedProcessAspectChain::~ScopedProcessAspectChain()
{
    ProcessAspectChain().Reset(std::move(m_previous));
}

void SetAspectTraceSinkForTest(AspectTraceSink sink)
{
    g_traceSink = sink;
}

std::string_view LoggingAspect::Name() const noexcept
{
    return "Logging";
}

void LoggingAspect::Before(const AspectEvent&) {}

void LoggingAspect::After(const AspectEvent& event)
{
    WriteAspectTrace(false, fmt::format("site '{}' subject '{}'", event.Site,
                                        event.Subject));
}

void LoggingAspect::OnError(const AspectEvent&, const std::exception&) {}

std::string_view TimingAspect::Name() const noexcept
{
    return "Timing";
}

void TimingAspect::Before(const AspectEvent& event)
{
    m_stack.push_back(Sample{std::chrono::steady_clock::now(),
                             std::string(event.Site),
                             std::string(event.Subject)});
}

void TimingAspect::After(const AspectEvent& event)
{
    Stop(event);
}

void TimingAspect::OnError(const AspectEvent& event, const std::exception&)
{
    Stop(event);
}

void TimingAspect::Stop(const AspectEvent& event)
{
    if (m_stack.empty())
    {
        return;
    }
    const Sample sample = std::move(m_stack.back());
    m_stack.pop_back();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - sample.Start);
    WriteAspectTrace(false,
                     fmt::format("site '{}' subject '{}' elapsed_ms={}",
                                 event.Site, event.Subject, elapsed.count()));
}

std::string_view ErrorAspect::Name() const noexcept
{
    return "Error";
}

void ErrorAspect::Before(const AspectEvent&) {}

void ErrorAspect::After(const AspectEvent& event)
{
    if (!event.Failed)
    {
        return;
    }
    WriteAspectTrace(true, fmt::format("site '{}' subject '{}' failed: {}",
                                       event.Site, event.Subject, event.Detail));
}

void ErrorAspect::OnError(const AspectEvent& event, const std::exception& error)
{
    WriteAspectTrace(true, fmt::format("site '{}' subject '{}' exception: {}",
                                       event.Site, event.Subject, error.what()));
}

}  // namespace brep
