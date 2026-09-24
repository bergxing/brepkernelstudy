#pragma once

#include <chrono>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace brep
{

struct AspectEvent
{
    std::string_view Site;
    std::string_view Subject;
    bool Failed{false};
    std::string_view Detail;
    const void* Payload{nullptr};
};

class IAspect
{
 public:
    virtual ~IAspect() = default;

    [[nodiscard]] virtual std::string_view Name() const noexcept = 0;

    virtual void Before(const AspectEvent& event) = 0;
    virtual void After(const AspectEvent& event) = 0;
    virtual void OnError(const AspectEvent& event,
                         const std::exception& error) = 0;
};

/// Ordered observers around one boundary call.
class AspectChain
{
 public:
    void Add(std::unique_ptr<IAspect> aspect);

    [[nodiscard]] std::vector<std::string> Names() const;

    [[nodiscard]] std::vector<std::unique_ptr<IAspect>> Release();

    void Reset(std::vector<std::unique_ptr<IAspect>> aspects);

    template <class F>
    decltype(auto) Invoke(AspectEvent& event, F&& body)
    {
        using Result = std::invoke_result_t<F&>;
        const auto notifyError = [&](std::size_t entered,
                                     const std::exception& error)
        {
            for (std::size_t i = entered; i > 0; --i)
            {
                m_aspects[i - 1]->OnError(event, error);
            }
        };
        const auto notifyAfter = [&]()
        {
            for (std::size_t i = m_aspects.size(); i > 0; --i)
            {
                m_aspects[i - 1]->After(event);
            }
        };
        auto runBody = [&]() -> decltype(auto)
        {
            std::size_t entered = 0;
            try
            {
                for (const auto& aspect : m_aspects)
                {
                    aspect->Before(event);
                    ++entered;
                }
                return std::forward<F>(body)();
            }
            catch (const std::exception& error)
            {
                notifyError(entered, error);
                throw;
            }
        };

        if constexpr (std::is_void_v<Result>)
        {
            runBody();
            notifyAfter();
        }
        else
        {
            decltype(auto) result = runBody();
            notifyAfter();
            return result;
        }
    }

 private:
    std::vector<std::unique_ptr<IAspect>> m_aspects;
};

[[nodiscard]] AspectChain& ProcessAspectChain();

void InstallProcessAspects();

class ScopedProcessAspectChain
{
 public:
    explicit ScopedProcessAspectChain(AspectChain replacement);
    ~ScopedProcessAspectChain();

    ScopedProcessAspectChain(const ScopedProcessAspectChain&) = delete;
    ScopedProcessAspectChain& operator=(const ScopedProcessAspectChain&) = delete;

 private:
    std::vector<std::unique_ptr<IAspect>> m_previous;
};

using AspectTraceSink = void (*)(bool isError, const char* message);

void SetAspectTraceSinkForTest(AspectTraceSink sink);

class LoggingAspect final : public IAspect
{
 public:
    [[nodiscard]] std::string_view Name() const noexcept override;

    void Before(const AspectEvent& event) override;
    void After(const AspectEvent& event) override;
    void OnError(const AspectEvent& event, const std::exception& error) override;
};

class TimingAspect final : public IAspect
{
 public:
    [[nodiscard]] std::string_view Name() const noexcept override;

    void Before(const AspectEvent& event) override;
    void After(const AspectEvent& event) override;
    void OnError(const AspectEvent& event, const std::exception& error) override;

 private:
    void Stop(const AspectEvent& event);

    struct Sample
    {
        std::chrono::steady_clock::time_point Start{};
        std::string Site;
        std::string Subject;
    };
    std::vector<Sample> m_stack;
};

class ErrorAspect final : public IAspect
{
 public:
    [[nodiscard]] std::string_view Name() const noexcept override;

    void Before(const AspectEvent& event) override;
    void After(const AspectEvent& event) override;
    void OnError(const AspectEvent& event, const std::exception& error) override;
};

}  // namespace brep
