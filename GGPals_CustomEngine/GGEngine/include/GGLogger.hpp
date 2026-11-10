/*************************************************************************
  \file       GGLogger.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       23 November 2025
  \version    2.1
  \brief
    Engine logging system for ImGui console display
  \details
    Captures engine messages for display in ImGui console while preserving
    existing console output functionality.
  \dependencies
    <string>, <vector>
  \see
    DefaultEditorScene
  \history
    - 1.0 (27 September 2025): Initial implementation.
    - 2.0 (23 November 2025): Added file logging support for Release mode debugging.
    - 2.1 (23 November 2025): Added timestamps to file log entries.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <fstream>

/*************************************************************************
  \brief
    Logging system for ImGui console integration
  \details
    Captures log messages for display in ImGui console while preserving
    existing console output. Thread-safe for multi-threaded logging.
*************************************************************************/
class GGLogger
{
public:
    /*************************************************************************
      \brief
        Log an info message
      \param[in] message
        Message to log
    *************************************************************************/
    static void Info(const std::string& message);

    /*************************************************************************
      \brief
        Log a warning message
      \param[in] message
        Warning message to log
    *************************************************************************/
    static void Warning(const std::string& message);

    /*************************************************************************
      \brief
        Log an error message
      \param[in] message
        Error message to log
    *************************************************************************/
    static void Error(const std::string& message);

    /*************************************************************************
      \brief
        Get all logged messages
      \return
        Vector of all logged messages
    *************************************************************************/
    static const std::vector<std::string>& GetMessages();

    /*************************************************************************
      \brief
        Clear all logged messages
    *************************************************************************/
    static void Clear();

    /*************************************************************************
      \brief
        Enable or disable file logging
      \param[in] enable
        True to enable file logging, false to disable
      \param[in] filename
        Optional filename for log file (default: "engine_log.txt")
    *************************************************************************/
    static void SetFileLogging(bool enable, const std::string& filename = "engine_log.txt");

    /*************************************************************************
      \brief
        Flush any pending file writes to disk
    *************************************************************************/
    static void FlushFile();

private:
    static std::vector<std::string> messages;
    static std::mutex messagesMutex;
    static std::ofstream logFile;
    static bool fileLoggingEnabled;

    static void AddMessage(const std::string& prefix, const std::string& message);
};