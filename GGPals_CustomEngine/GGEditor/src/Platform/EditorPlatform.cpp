/*************************************************************************
  \file       EditorPlatform.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 December 2025
  \version    1.0
  \brief
    Helper Utilities to open .lua code files in Visual Studio Code

  \dependencies
    <string>           - Used for lua script paths
  \history
    - 1.0 (10 December 2025): Initial version.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <EditorPlatform.hpp>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <GGEngine.hpp>

// Cross-platform implementation to open .lua files
bool Platform::OpenInEditor(const std::string& filePath)
{
    if (filePath.empty())
        return false;

    std::string resolvedPath = GetEngineInstance().GetAssetManager()->ResolvePath(filePath);
    std::filesystem::path absPath = std::filesystem::absolute(resolvedPath);
    std::string absolutePath = absPath.string();

#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD result = SearchPathA(nullptr, "code.cmd", nullptr, MAX_PATH, buffer, nullptr);

    std::string codePath = (result > 0) ? buffer : "C:\\Program Files\\Microsoft VS Code\\Code.exe";
    std::string command = "\"" + codePath + "\" \"" + absolutePath + "\"";

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    BOOL success = CreateProcessA(
        nullptr,
        const_cast<char*>(command.c_str()),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    if (success)
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return true;
    }
    return false;

#elif defined(__APPLE__)
    std::string command = "open -a \"Visual Studio Code\" \"" + absolutePath + "\" &";
    return system(command.c_str()) == 0;

#else // Linux
    std::string command = "code \"" + absolutePath + "\" &";
    return system(command.c_str()) == 0;
#endif
}