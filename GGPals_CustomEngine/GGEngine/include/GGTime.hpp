/*************************************************************************
  \file       GGTime.hpp
  \author     Tai Xiao Xuan, Zafir Haqqym
  \par        Email: xiaoxuan.tai@digipen.edu, zafir.h@digipen.edu
  \contribute Xiao Xuan(50%), Zafir(50%)
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

#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <cassert>
#include <memory>
#include <array>
#include <queue>
#include <GGMath/Types.hpp>
#include <algorithm>
#include <GGSystemInterface.hpp>
#include <GGEvent.hpp>
#include <thread>


namespace GGTime {

    using GameTicks         = uint64_t;
    using SystemTimePoint   = std::chrono::steady_clock::time_point;
    using Milliseconds      = std::chrono::milliseconds;

    static constexpr GameTicks TICKS_PER_SECOND = 60;

    /*************************************************************************
      \brief
        Simple time manager with time tracking through std::chrono::steady_clock
      \par Usage
      \code
        SystemTimeManager::GetDeltaTime();     // double
        SystemTimeManager::GetSystemTimeNow(); // std::chrono::steady_clock
      \endcode
    *************************************************************************/
    class SystemTimeManager : public SystemManagerInterface {
    public:
        SystemTimeManager() {
            startTime = std::chrono::steady_clock::now();
            lastFrameTime = startTime;
        }

        bool Init();

        void Update() override;

        inline double const GetDeltaTime() const {
            return deltaTime * timeScale;
        }

        inline double const GetSystemDeltaTime() const {
            return systemDeltaTime;
        }

        static inline SystemTimePoint GetSystemTime() {
            return std::chrono::steady_clock::now();
        }

        /*************************************************************************
          \brief
            Sets the target frame rate for manual frame limiting
          \param[in] fps
            Target frames per second (typically 30-240)
          \details
            Updates both the FPS value and the calculated frame duration.
            Called from ImGui slider and config initialization.
            Frame limiter in GGEngine::Update() uses this to cap frame rate.
        *************************************************************************/
        inline void SetTargetFPS(uint32_t fps)
        {
            targetFPS = fps;
            targetFrameDuration = std::chrono::duration<double>(1.0 / static_cast<double>(targetFPS));
        }

        /*************************************************************************
          \brief
            Gets the target frame rate in frames per second
          \return
            Target FPS value (default: 60)
          \details
            Used by the frame limiter in GGEngine::Update() to calculate target
            frame time. Can be adjusted via ImGui slider (30-240 FPS range).
        *************************************************************************/
        inline uint32_t GetTargetFPS() const
        {
            return targetFPS;
        }

        inline double GetFPS() const
        {
            return (deltaTime > 0.0) ? (1.0 / deltaTime) : 0.0;
        }

        /*************************************************************************
          \brief
            Gets the timestamp of the last frame update
          \return
            std::chrono::steady_clock time_point of the last frame
          \details
            Used internally for delta time calculations. Not typically needed
            by external systems.
        *************************************************************************/
        inline SystemTimePoint GetLastFrameTime() const
        {
            return lastFrameTime;
        }

        inline void SetTimeScale(float scale) {
            timeScale = scale;
        }

    private:
        uint32_t targetFPS{ 60 };
        std::chrono::duration<double> targetFrameDuration{ 1.0 / static_cast<double>(targetFPS) };

        SystemTimePoint             lastFrameTime;
        SystemTimePoint             startTime;

        double                      deltaTime{};
        double                      systemDeltaTime{};
        float                       timeScale{ 1.f };
        bool                        isPaused{};
    };

    /*************************************************************************
      \brief
        Simulation time manager with time tracking through std::chrono::steady_clock
        to handle fixed timestep operations

        In case we have to do PhysicsUpdate :v. If we are, then interpolation
        is kinda cooked bro...
      \par Usage
      \code
        
      \endcode
    *************************************************************************/
    class SimulationManager : public SystemManagerInterface {
    public:
        SimulationManager() {
            lastFrameTime = std::chrono::steady_clock::now();
            accumulator = std::chrono::nanoseconds{ 0 };
            simulationSteps = 0;
            manualStepMode = false;
            shouldStepThisFrame = false;
            interpolationAlpha = 0.0;
        }

        bool Init() override {
            return true;
        }

        void Update() override;
        
        // Mutator for imgui
        inline bool& ManualStepMode() {
            return manualStepMode;
        }

        // Mutator for imgui
        inline bool& ShouldStepThisFrame() {
            return shouldStepThisFrame;
        }

        inline double GetFixedDeltaTime() const {
            return std::chrono::duration<double>(FIXED_TIMESTEP).count();
        }

        // accessors/mutators for pause loop (StopPlayToolbar.cpp)
        void Pause() { isPaused = true; }
        void Resume() { isPaused = false; }
        bool IsPaused() const { return isPaused; } // return pause status

    private:
        static constexpr auto       FIXED_TIMESTEP = 
                                    std::chrono::duration<int64_t, std::ratio<1, 60>>{ 1 };
        
        static constexpr int        MAX_PHYSICS_STEPS = 5;
        SystemTimePoint             lastFrameTime;
        std::chrono::nanoseconds    accumulator;
        uint64_t                    simulationSteps;
        double                      interpolationAlpha;
        bool                        manualStepMode;
        bool                        shouldStepThisFrame;
        bool                        isPaused = false;
    };


    /*************************************************************************
      \brief
        Simple timer class with countdown and callback handle. 
        Timer starts counting down on the next frame after it is started.
      \description
        IMPORTANT: Refer to SystemPriority
        enum class SystemPriority : uint16_t {
            TIME = 0,
            INPUT = 50,
            PHYSICS = 100,
            TIMER = 150,
            GAMEPLAY = 200,
            AUDIO = 300,
            RENDERING = 400,
            DEBUG = 500,
        };

        Timer callbacks are called in TIMER time, BEFORE Gameplay and AFTER Physics.
      \par Usage
      \code
        // Normal countdown timer usage example
        GGTime::TimerID = GGEngine::GetEngine().GetSystemsRegistry()
                            .GetSystem<TimerManager>().CreateTimer(
                            5000,   // 5 seconds for eg.
                            []()
                            {
                                    // Whatever implementation here...
                            },
                            true);  // Whether timer repeats!
      \endcode
    *************************************************************************/
    class Timer {
    public:
        using TimerID = uint32_t;

        Timer(TimerID id, std::chrono::milliseconds durVal, Callback<> callback = nullptr, bool repeat = false) 
            : timerID(id), 
              duration(durVal),
              active(true),
              repeating(repeat),
              startTime(std::chrono::steady_clock::now())
        {
            if (callback) {
                onTimerFinished += callback;
            }
        }

        GGEvent<> onTimerFinished;

        double GetTimerProgress() const;

        bool ShouldFire() const;

        void Fire();

        bool IsActive() const;

        TimerID GetID() const;

    private:
        std::chrono::steady_clock::time_point startTime;

        std::chrono::milliseconds duration;
        TimerID timerID;
        bool active = false;
        bool repeating = false;
    };

    /*************************************************************************
      \brief
        Simple timer manager class with helper function to create timers.
      \par Usage
      \code
        // Normal countdown timer usage example
        GGTime::TimerID = GGEngine::GetEngine().GetSystemsRegistry()
                            .GetSystem<TimerManager>().CreateTimer(
                            5000,   // 5 seconds for eg.
                            []()
                            {
                                    // Whatever implementation here...
                            },
                            true);  // Whether timer repeats!
      \endcode
    *************************************************************************/
    class TimerManager : public SystemManagerInterface {
    public:
        TimerManager() = default;

        uint32_t CreateTimer(std::chrono::milliseconds duration,
                            Callback<> callback,
                            bool repeating = false);

        bool Init() override;

        void Update() override;

        double GetTimerProgress(Timer::TimerID id) const;

    private:
        static constexpr size_t MAX_TIMERS = 8192;

        std::queue<uint32_t> timerIDs;
        std::array<std::unique_ptr<Timer>, MAX_TIMERS> timerArray;
    };
    

} // GGTime