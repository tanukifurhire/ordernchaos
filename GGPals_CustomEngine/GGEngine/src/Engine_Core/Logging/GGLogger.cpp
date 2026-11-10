/*************************************************************************
  \file       GGLogger.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       23 November 2025
  \version    2.1
  \brief
    Engine logging system implementation
  \details
    Logging system that captures messages for ImGui display
  \dependencies
    GGLogger.hpp, <iostream>, <mutex>
  \see
    GGLogger.hpp
  \history
    - 1.0 (27 September 2025): Initial implementation.
    - 2.0 (23 November 2025): Added file logging support for Release mode debugging.
    - 2.1 (23 November 2025): Added timestamps to file log entries.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <GGLogger.hpp>
#include <iostream>
#include <mutex>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

// Static member definitions - shared across all engine systems
std::vector<std::string> GGLogger::messages;
std::mutex GGLogger::messagesMutex;
std::ofstream GGLogger::logFile;
bool GGLogger::fileLoggingEnabled = false;

/*************************************************************************
  \brief
    Get current timestamp as formatted string
  \return
    Timestamp string in format [YYYY-MM-DD HH:MM:SS]
  \details
    Helper function for adding timestamps to log messages
*************************************************************************/
static std::string GetTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm now_tm;
#ifdef _WIN32
    localtime_s(&now_tm, &now_time_t);
#else
    localtime_r(&now_time_t, &now_tm);
#endif

    std::ostringstream oss;
    oss << "["
        << std::setfill('0') << std::setw(4) << (now_tm.tm_year + 1900) << "-"
        << std::setfill('0') << std::setw(2) << (now_tm.tm_mon + 1) << "-"
        << std::setfill('0') << std::setw(2) << now_tm.tm_mday << " "
        << std::setfill('0') << std::setw(2) << now_tm.tm_hour << ":"
        << std::setfill('0') << std::setw(2) << now_tm.tm_min << ":"
        << std::setfill('0') << std::setw(2) << now_tm.tm_sec << "."
        << std::setfill('0') << std::setw(3) << now_ms.count()
        << "]";

    return oss.str();
}

/*************************************************************************
  \brief
    Log an info message
  \details
    Stores message for ImGui console display and outputs to standard console.
    Thread-safe for multi-system logging.
*************************************************************************/
void GGLogger::Info(const std::string& message)
{
    AddMessage("[INFO] ", message);
    std::cout << "[INFO] " << message << std::endl;
}

/*************************************************************************
  \brief
    Log a warning message
  \details
    Creates warning-level log entry. ImGui console displays these in yellow.
*************************************************************************/
void GGLogger::Warning(const std::string& message)
{
    AddMessage("[WARNING] ", message);
    std::cout << "[WARNING] " << message << std::endl;
}

/*************************************************************************
  \brief
    Log an error message
  \details
    Creates error-level log entry. ImGui console displays these in red.
*************************************************************************/
void GGLogger::Error(const std::string& message)
{
    AddMessage("[ERROR] ", message);
    std::cout << "[ERROR] " << message << std::endl;
}

/*************************************************************************
  \brief
    Retrieve all logged messages for ImGui display
  \details
    Thread-safe access to message vector. Called by ImGui console each frame.
*************************************************************************/
const std::vector<std::string>& GGLogger::GetMessages()
{
    std::lock_guard<std::mutex> lock(messagesMutex);
    return messages;
}

/*************************************************************************
  \brief
    Clear all logged messages
  \details
    Thread-safe message clearing. Triggered by ImGui "Clear Console" button.
*************************************************************************/
void GGLogger::Clear()
{
    std::lock_guard<std::mutex> lock(messagesMutex);
    messages.clear();
}

/*************************************************************************
  \brief
    Internal message storage with thread safety and memory management
  \details
    Handles concurrent message addition with mutex protection. Implements
    automatic memory management by limiting message history to 1000 entries.
*************************************************************************/
void GGLogger::AddMessage(const std::string& prefix, const std::string& message)
{
    std::lock_guard<std::mutex> lock(messagesMutex);
    std::string fullMessage = prefix + message;
    messages.push_back(fullMessage);

    // Write to file if enabled (with timestamp)
    if (fileLoggingEnabled && logFile.is_open())
    {
        std::string timestamp = GetTimestamp();
        logFile << timestamp << " " << fullMessage << std::endl;
        logFile.flush(); // Ensure immediate write
    }

    // Prevent unbounded memory growth by removing oldest messages
    if (messages.size() > 1000)
    {
        messages.erase(messages.begin(), messages.begin() + 100);
    }
}

/*************************************************************************
  \brief
    Enable or disable file logging
  \details
    Opens or closes the log file. When enabled, all subsequent log messages
    are written to the specified file in addition to console/ImGui output.
*************************************************************************/
void GGLogger::SetFileLogging(bool enable, const std::string& filename)
{
    std::lock_guard<std::mutex> lock(messagesMutex);

    if (enable && !fileLoggingEnabled)
    {
        // Open file in append mode
        logFile.open(filename, std::ios::out | std::ios::app);

        if (logFile.is_open())
        {
            fileLoggingEnabled = true;

            // Write header with timestamp
            std::string timestamp = GetTimestamp();
            logFile << "========================================" << std::endl;
            logFile << timestamp << " Engine Log Started" << std::endl;
            logFile << "========================================" << std::endl;
            logFile.flush();

            std::cout << "[Logger] File logging enabled: " << filename << std::endl;
        }
        else
        {
            std::cerr << "[Logger] Failed to open log file: " << filename << std::endl;
        }
    }
    else if (!enable && fileLoggingEnabled)
    {
        // Close file
        if (logFile.is_open())
        {
            std::string timestamp = GetTimestamp();
            logFile << "========================================" << std::endl;
            logFile << timestamp << " Engine Log Ended" << std::endl;
            logFile << "========================================" << std::endl;
            logFile.close();
        }

        fileLoggingEnabled = false;
        std::cout << "[Logger] File logging disabled" << std::endl;
    }
}

/*************************************************************************
  \brief
    Flush any pending file writes to disk
  \details
    Forces any buffered log messages to be written to disk immediately.
    Useful before critical operations or shutdown.
*************************************************************************/
void GGLogger::FlushFile()
{
    std::lock_guard<std::mutex> lock(messagesMutex);

    if (fileLoggingEnabled && logFile.is_open())
    {
        logFile.flush();
    }
}