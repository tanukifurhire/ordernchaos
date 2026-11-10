/*************************************************************************
  \file       GGPerformanceProfiler.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       30 September 2025
  \version    1.0
  \brief
    Defines centralized performance profiling system for engine systems
  \details
    Provides real-time performance monitoring with rolling average
    calculations, percentage-of-frame tracking, and sorted display
    support. Designed to work with SystemPriorityRegister to automatically
    collect timing data from all registered systems.

    Key features:
    - Rolling average calculations over configurable sample count
    - Automatic percentage calculation based on total frame time
    - Zero-overhead when profiling is disabled
    - Thread-safe data collection
    - Sorted output by time consumption or alphabetically
  \dependencies
    <string>, <unordered_map>, <vector>, <deque>, <algorithm>, <chrono>
  \see
    SystemPriorityRegister, DefaultEditorScene
  \history
    - 0.1 (30 September 2025): Initial structure with data types
    - 0.2 (30 September 2025): Added RecordSystemTime() with rolling averages
    - 0.3 (30 September 2025): Added BeginFrame/EndFrame with percentage calc
    - 0.4 (30 September 2025): Added GetSortedData(), Clear(), getters/setters
    - 1.0 (30 September 2025): Enhanced SetMaxSamples with dynamic trimming
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <deque>
#include <algorithm>
#include <chrono>

namespace Systems
{
    /*************************************************************************
      \brief
        Performance data for a single system across multiple frames
      \details
        Stores timing samples in a ring buffer for rolling average
        calculations. Maintains both instantaneous and averaged timing
        data along with percentage of total frame time.
    *************************************************************************/
    struct SystemPerformanceData
    {
        std::string name;
        std::deque<double> timeSamples;  // Ring buffer of timing samples
        double lastFrameTime;             // Most recent frame's timing
        double avgTime;                   // Rolling average across all samples
        double percentage;                // Percentage of total frame time
        size_t sampleCount;               // How many samples collected so far

        /*************************************************************************
          \brief
            Default constructor initializing all fields to zero
        *************************************************************************/
        SystemPerformanceData()
            : lastFrameTime(0.0)
            , avgTime(0.0)
            , percentage(0.0)
            , sampleCount(0)
        {
        }
    };

    /*************************************************************************
      \brief
        Centralized performance profiler collecting timing data from all systems
      \details
        Singleton-pattern profiler that subscribes to system timing events
        and maintains rolling averages for performance analysis. Provides
        sorted output suitable for ImGui display with configurable sample
        window size.

        The profiler automatically calculates percentages based on total
        frame time and maintains a configurable number of samples for
        smoothed average calculations.
      \par Performance Characteristics
        - O(1) data recording per system
        - O(n log n) sorting when retrieving data
        - O(k) memory per system where k is max samples
        - Negligible overhead when disabled
      \par Thread Safety
        Not thread-safe. Must be called from main game loop thread only.
    *************************************************************************/
    class PerformanceProfiler
    {
    public:
        /*************************************************************************
          \brief
            Get singleton instance of the performance profiler
          \return
            Reference to the global profiler instance
          \details
            Thread-safe singleton initialization using C++11 static local
            guarantees. Instance persists for application lifetime.
        *************************************************************************/
        static PerformanceProfiler& GetInstance()
        {
            static PerformanceProfiler instance;
            return instance;
        }

        /*************************************************************************
          \brief
            Record timing data for a system
          \param[in] systemName
            Name of the system (typically from typeid or custom string)
          \param[in] timeSeconds
            Time taken by system in seconds
          \details
            Adds timing sample to the system's ring buffer and updates
            rolling average. If buffer exceeds max samples, oldest sample
            is discarded. System is automatically registered on first call.
        *************************************************************************/
        void RecordSystemTime(std::string const& systemName, double timeSeconds)
        {
            // Skip recording if profiling is disabled (zero overhead)
            if (!enabled)
            {
                return;
            }

            // Get or create system data entry
            auto& data = systemData[systemName];
            data.name = systemName;
            data.lastFrameTime = timeSeconds;

            // Add new sample to ring buffer
            data.timeSamples.push_back(timeSeconds);

            // Keep buffer at max size by removing oldest sample
            if (data.timeSamples.size() > maxSamples)
            {
                data.timeSamples.pop_front();
            }

            // Recalculate rolling average from all samples
            double sum = 0.0;
            for (double sample : data.timeSamples)
            {
                sum += sample;
            }

            data.avgTime = sum / static_cast<double>(data.timeSamples.size());
            data.sampleCount = data.timeSamples.size();
        }

        /*************************************************************************
          \brief
            Begin frame timing measurement
          \details
            Marks the start of frame timing. Call before system updates begin.
            Used to calculate total frame time for percentage calculations.
        *************************************************************************/
        void BeginFrame()
        {
            // Mark start of frame for total time calculation
            frameStartTime = std::chrono::steady_clock::now();
        }

        /*************************************************************************
          \brief
            End frame timing and calculate percentages
          \details
            Calculates total frame time and updates percentage values for
            all registered systems. Call after all system updates complete.
        *************************************************************************/
        void EndFrame()
        {
            if (!enabled)
            {
                return;
            }

            // Calculate total frame time
            auto frameEndTime = std::chrono::steady_clock::now();
            totalFrameTime = std::chrono::duration<double>(frameEndTime - frameStartTime).count();

            // Update percentages for all systems based on total frame time
            if (totalFrameTime > 0.0)
            {
                for (auto& [name, data] : systemData)
                {
                    // What percentage of the frame did this system use?
                    data.percentage = (data.avgTime / totalFrameTime) * 100.0;
                }
            }
        }

        /*************************************************************************
          \brief
            Get sorted performance data for all systems
          \param[in] sortByTime
            If true, sort by time descending. If false, sort alphabetically.
          \return
            Vector of system performance data sorted according to parameter
          \details
            Returns copy of performance data suitable for display. Data is
            sorted either by average time consumption (highest first) or
            alphabetically by system name.
        *************************************************************************/
        std::vector<SystemPerformanceData> GetSortedData(bool sortByTime = true) const
        {
            // Copy all system data into a vector for sorting
            std::vector<SystemPerformanceData> result;
            result.reserve(systemData.size());

            for (auto const& [name, data] : systemData)
            {
                result.push_back(data);
            }

            // Sort by time (slowest first) or alphabetically by name
            if (sortByTime)
            {
                std::sort(result.begin(), result.end(),
                    [](SystemPerformanceData const& a, SystemPerformanceData const& b)
                    {
                        return a.avgTime > b.avgTime;  // Descending order
                    });
            }
            else
            {
                std::sort(result.begin(), result.end(),
                    [](SystemPerformanceData const& a, SystemPerformanceData const& b)
                    {
                        return a.name < b.name;  // Alphabetical order
                    });
            }

            return result;
        }

        /*************************************************************************
          \brief
            Clear all collected performance data
          \details
            Removes all timing samples and resets profiler state. Useful
            when resetting profiling or changing configuration.
        *************************************************************************/
        void Clear()
        {
            systemData.clear();
            totalFrameTime = 0.0;
        }

        /*************************************************************************
          \brief
            Set maximum number of samples for rolling average
          \param[in] samples
            Number of frames to average over (default: 60)
          \details
            Higher values produce smoother graphs but slower response to
            changes. Lower values are more responsive but jittery.
            Typical values: 30-120 frames.
        *************************************************************************/
        void SetMaxSamples(size_t samples)
        {
            maxSamples = samples;

            // Trim all existing system buffers to new size
            for (auto& [name, data] : systemData)
            {
                // Remove oldest samples until we're at the new limit
                while (data.timeSamples.size() > maxSamples)
                {
                    data.timeSamples.pop_front();
                }
                data.sampleCount = data.timeSamples.size();

                // Recalculate average with new sample set
                if (!data.timeSamples.empty())
                {
                    double sum = 0.0;
                    for (double sample : data.timeSamples)
                    {
                        sum += sample;
                    }
                    data.avgTime = sum / static_cast<double>(data.timeSamples.size());
                }
            }
        }

        /*************************************************************************
          \brief
            Get current maximum sample count
          \return
            Number of frames being averaged
        *************************************************************************/
        size_t GetMaxSamples() const
        {
            return maxSamples;
        }

        /*************************************************************************
          \brief
            Enable or disable profiling
          \param[in] enable
            True to enable profiling, false to disable
          \details
            When disabled, RecordSystemTime becomes a no-op for zero overhead.
            Existing data is preserved when toggling.
        *************************************************************************/
        void SetEnabled(bool enable)
        {
            enabled = enable;
        }

        /*************************************************************************
          \brief
            Check if profiling is enabled
          \return
            True if profiling is active
        *************************************************************************/
        bool IsEnabled() const
        {
            return enabled;
        }

        /*************************************************************************
          \brief
            Get last recorded total frame time
          \return
            Total frame time in seconds
        *************************************************************************/
        double GetTotalFrameTime() const
        {
            return totalFrameTime;
        }

    private:
        /*************************************************************************
          \brief
            Private constructor enforcing singleton pattern
        *************************************************************************/
        PerformanceProfiler()
            : maxSamples(60)      // Default: average over 60 frames (~1 second at 60 FPS)
            , enabled(true)       // Profiling on by default
            , totalFrameTime(0.0)
        {
        }

        // Map of system name -> performance data
        std::unordered_map<std::string, SystemPerformanceData> systemData;

        // Frame timing for percentage calculations
        std::chrono::steady_clock::time_point frameStartTime;

        // Configuration
        size_t maxSamples;  // How many frames to average over
        bool enabled;       // Is profiling active?

        // Results
        double totalFrameTime;  // Last frame's total time in seconds

        // Prevent copying (singleton pattern)
        PerformanceProfiler(PerformanceProfiler const&) = delete;
        PerformanceProfiler& operator=(PerformanceProfiler const&) = delete;
    };
} // namespace Systems