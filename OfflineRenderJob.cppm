module;

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

export module Kairo.Foundation.RayTracer.OfflineRenderJob;

import Kairo.Foundation.RayTracer.Film;
import Kairo.Foundation.RayTracer.Renderer;
import Kairo.Foundation.RayTracer.Scene;
import Kairo.Foundation.RayTracer.Types;

export namespace kairo::foundation::raytracer
{
    struct OfflineRenderDiagnostic final
    {
        bool Error = false;
        std::string Message;
    };

    struct OfflineRenderCapabilityReport final
    {
        bool Supported = true;
        std::vector<OfflineRenderDiagnostic> Diagnostics;
    };

    [[nodiscard]] inline OfflineRenderCapabilityReport InspectOfflineRenderScene(const Scene& scene)
    {
        OfflineRenderCapabilityReport report;
        try { ValidateRenderSettings(scene.Settings); }
        catch (const std::exception& error)
        {
            report.Supported = false;
            report.Diagnostics.push_back({ true, error.what() });
        }
        if (scene.Primitives.empty())
            report.Diagnostics.push_back({ false, "Scene has no geometric primitives; only the environment/background will render." });
        if (scene.Materials.empty() && !scene.Primitives.empty())
        {
            report.Supported = false;
            report.Diagnostics.push_back({ true, "Scene contains geometry without any material table." });
        }
        if (scene.Lights.empty() && scene.AreaLights.empty() && !scene.Environment.Enabled)
            report.Diagnostics.push_back({ false, "Scene has no explicit light or enabled environment." });
        return report;
    }

    enum class OfflineRenderJobState : std::uint8_t
    {
        Idle,
        Running,
        Completed,
        Cancelled,
        Failed
    };

    struct OfflineRenderProgress final
    {
        OfflineRenderJobState State = OfflineRenderJobState::Idle;
        std::uint32_t CompletedPasses = 0u;
        std::uint32_t TotalPasses = 0u;
        double Fraction = 0.0;
        std::string Error;
    };

    class OfflineRenderJob final
    {
    public:
        OfflineRenderJob(Scene scene, std::uint32_t passes)
            : m_Scene(std::move(scene)), m_TotalPasses(passes)
        {
            if (passes == 0u) throw std::invalid_argument("Offline render requires at least one pass.");
            const auto report = InspectOfflineRenderScene(m_Scene);
            if (!report.Supported)
                throw std::invalid_argument(report.Diagnostics.empty() ? "Offline render scene is unsupported." : report.Diagnostics.front().Message);
        }

        OfflineRenderJob(const OfflineRenderJob&) = delete;
        OfflineRenderJob& operator=(const OfflineRenderJob&) = delete;
        ~OfflineRenderJob() { Cancel(); }

        void Start()
        {
            OfflineRenderJobState expected = OfflineRenderJobState::Idle;
            if (!m_State.compare_exchange_strong(expected, OfflineRenderJobState::Running))
                throw std::logic_error("Offline render job can only be started once.");
            m_Worker = std::jthread([this](std::stop_token token) { Run(token); });
        }

        void Cancel() noexcept
        {
            if (m_Worker.joinable())
            {
                m_Worker.request_stop();
                m_Worker.join();
            }
        }

        void Wait()
        {
            if (m_Worker.joinable()) m_Worker.join();
        }

        [[nodiscard]] OfflineRenderProgress Progress() const
        {
            OfflineRenderProgress result;
            result.State = m_State.load();
            result.CompletedPasses = m_Completed.load();
            result.TotalPasses = m_TotalPasses;
            result.Fraction = static_cast<double>(result.CompletedPasses) / static_cast<double>(m_TotalPasses);
            std::scoped_lock lock(m_Mutex);
            result.Error = m_Error;
            return result;
        }

        [[nodiscard]] std::optional<Film> Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            if (!m_Accumulated.has_value() || m_Completed.load() == 0u) return std::nullopt;
            Film image = *m_Accumulated;
            const float divisor = static_cast<float>(m_Completed.load());
            for (auto& pixel : image.Pixels()) pixel = pixel / divisor;
            return image;
        }

    private:
        Scene m_Scene;
        std::uint32_t m_TotalPasses = 0u;
        std::atomic<std::uint32_t> m_Completed{ 0u };
        std::atomic<OfflineRenderJobState> m_State{ OfflineRenderJobState::Idle };
        mutable std::mutex m_Mutex;
        std::optional<Film> m_Accumulated;
        std::string m_Error;
        std::jthread m_Worker;

        void Run(std::stop_token token) noexcept
        {
            try
            {
                for (std::uint32_t pass = 0u; pass < m_TotalPasses; ++pass)
                {
                    if (token.stop_requested())
                    {
                        m_State.store(OfflineRenderJobState::Cancelled);
                        return;
                    }
                    Scene passScene = m_Scene;
                    passScene.Settings.SamplesPerPixel = 1u;
                    passScene.Settings.SampleSeed = m_Scene.Settings.SampleSeed + pass;
                    const RenderResult rendered = Renderer{}.Render(passScene);
                    {
                        std::scoped_lock lock(m_Mutex);
                        if (!m_Accumulated.has_value())
                            m_Accumulated.emplace(rendered.Image.Width(), rendered.Image.Height());
                        for (std::size_t i = 0u; i < rendered.Image.Pixels().size(); ++i)
                            m_Accumulated->Pixels()[i] += rendered.Image.Pixels()[i];
                    }
                    m_Completed.store(pass + 1u);
                }
                m_State.store(OfflineRenderJobState::Completed);
            }
            catch (const std::exception& error)
            {
                {
                    std::scoped_lock lock(m_Mutex);
                    m_Error = error.what();
                }
                m_State.store(OfflineRenderJobState::Failed);
            }
            catch (...)
            {
                {
                    std::scoped_lock lock(m_Mutex);
                    m_Error = "Unknown offline render failure.";
                }
                m_State.store(OfflineRenderJobState::Failed);
            }
        }
    };
}
