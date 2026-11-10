/*************************************************************************
  \file       GGTime.cpp
  \author     Tai Xiao Xuan, Zafir Haqqym
  \par        Email: xiaoxuan.tai@digipen.edu, zafir.h@digipen.edu
  \date       30 September 2025
  \version    1.1
  \brief
    Declares GGTime namespace for time management
  \details
    Provides game timing utilities optimized for turn-based multiplayer.
    Includes configurable frame rate limiting for network consistency.

    Key components:
    - SystemTimeManager: Delta time and FPS control
    - SimulationManager: Fixed timestep physics updates
    - Timer/TimerManager: Callback-based countdown timers

    Frame limiting is always active for consistent timing in windowed
    multiplayer games. Target FPS configurable via SetTargetFPS()
    (default: 60, range: 30-240).
  \dependencies
    <chrono>, <cstdint>, <functional>, <thread>, <memory>, <array>,
    <queue>, GGMath/Types.hpp, GGECSPhysics.hpp, GGEvent.hpp
  \see
    SystemManagerInterface, GGEngine
  \history
    - 1.0 (15 September 2025): Initial version with time managers and timers.
    - 1.1 (30 September 2025): Removed frame limiting toggle - frame limiting
      now always active. Added GetTargetFPS() and GetLastFrameTime() accessors
      for configurable frame rate control system.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGTime.hpp>
#include <GGEngine.hpp>
#include <GGSystemManagers.hpp>

#include <GGJSONConfig.hpp>

namespace GGTime {
    bool SystemTimeManager::Init()
    {
        targetFPS = GGEngineConfig::GGEngineGraphics::GGEngineRenderer::FrameRateCap();
        return true;
    }

	void SimulationManager::Update() 
    {
        // pause logic
        if (isPaused && !manualStepMode) {
            return;
        }

        if (manualStepMode)
        {
            if (shouldStepThisFrame)
            {
                // Update simulation...
                std::cout << "Updating Simulation! (Step-By-Step Mode)" << std::endl;
                Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSPhysics>().Update
                (
                    GetEngineInstance().GetActiveWorld()
                );
                shouldStepThisFrame = false;
            }

            return;
        }

        auto currentTime = std::chrono::steady_clock::now();
        auto frameDuration = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        if (frameDuration > std::chrono::nanoseconds{ 250000000 }) {
            frameDuration = std::chrono::nanoseconds{ 250000000 };
        }

        accumulator += frameDuration;

        auto fixedTimeStepNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(FIXED_TIMESTEP);

        int32_t physicsSteps = 0;
        while (accumulator >= fixedTimeStepNanos && physicsSteps < MAX_PHYSICS_STEPS) {
            Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSPhysics>().Update
            (
                GetEngineInstance().GetActiveWorld()
            );
            accumulator -= fixedTimeStepNanos;
            physicsSteps++;
        }
    }

    void SystemTimeManager::Update()
    {
        auto currentTime = std::chrono::steady_clock::now();
        auto frameDuration = currentTime - lastFrameTime;

        systemDeltaTime = std::chrono::duration<double>(frameDuration).count();
        deltaTime = systemDeltaTime * timeScale;
        deltaTime = std::min(deltaTime, 0.1);
        lastFrameTime = currentTime;
    }

    bool Timer::ShouldFire() const 
    {
        if (!active) return false;

        return (std::chrono::steady_clock::now() - startTime) >= duration;
    }

    void Timer::Fire() 
    {
        onTimerFinished.Emit();

        if (repeating) 
        {
            startTime = std::chrono::steady_clock::now();
        }
        else 
        {
            active = false;
        }
    }

    bool Timer::IsActive() const 
    {
        return active;
    }

    uint32_t Timer::GetID() const 
    {
        return timerID;
    }

    double Timer::GetTimerProgress() const 
    {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        double elapsedMs = std::chrono::duration<double, std::milli>(elapsed).count();
        double durationMs = static_cast<double>(duration.count());
        return std::min(elapsedMs / durationMs, 1.0);
    }

    uint32_t TimerManager::CreateTimer(std::chrono::milliseconds duration,
                                        Callback<> callback,
                                        bool repeating) 
    {
        if (timerIDs.empty()) {
            assert(false && "Timer limit reached!");
            return 0;
        }

        uint32_t id = timerIDs.front();
        timerIDs.pop();
        timerArray[id] = std::make_unique<Timer>(id, duration, callback, repeating);

        return id;
    }

    bool TimerManager::Init() 
    {
        for (uint32_t i{1}; i < MAX_TIMERS; i++) 
        {
            timerIDs.push(i);
        }

        return true;
    }

    void TimerManager::Update() 
    {
        for (auto& timer : timerArray) 
        {
            if (!timer) continue;
            
            if (timer->ShouldFire()) timer->Fire();

            if (!timer->IsActive())
            {
                timerIDs.push(timer->GetID());
                timer.reset();
                continue;
            }

        }
    }

    double TimerManager::GetTimerProgress(Timer::TimerID id) const 
    {
        if (!timerArray[id]) return 0.0;

        else if (timerArray[id]->IsActive()) {
            return timerArray[id]->GetTimerProgress();
        }

        else return 1.0;
    }
}