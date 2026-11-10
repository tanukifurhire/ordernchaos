/*************************************************************************
  \file       SimpleFileDialog.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       16 November 2025
  \version    1.1
  \brief
    Simple file dialog using ImGui for tilemap editor.
  \details
    Lightweight file browser implementation using only ImGui. Supports
    browsing directories, filtering by extension, and selecting files
    for open/save operations. Designed to avoid external dependencies
    and Windows.h conflicts. Version 1.1 adds .meta file filtering
    and cleaner UI presentation.
  \dependencies
    imgui.h, <filesystem>, <string>, <vector>
  \see
    TilemapNode.hpp
  \history
    - 1.0 (16 November 2025): Initial version.
    - 1.1 (23 November 2025): Filter .meta files, hide path display, disable Up when at root.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#ifndef SIMPLE_FILE_DIALOG_HPP
#define SIMPLE_FILE_DIALOG_HPP

#pragma region Includes
#include <imgui.h>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#pragma endregion

/*************************************************************************
  \brief
    Simple file dialog for browsing and selecting files
  \details
    Provides basic file browser functionality using only ImGui. Shows
    directories and files, allows navigation, and returns selected path.
    Designed to replace hardcoded file paths in tilemap editor.
    Automatically filters out .meta files.
*************************************************************************/
class SimpleFileDialog
{
public:
    /*************************************************************************
      \brief
        File dialog modes
    *************************************************************************/
    enum class Mode
    {
        Open,    // Select existing file
        SaveAs   // Specify file to save
    };

    /*************************************************************************
      \brief
        Constructor
      \param[in] startPath
        Initial directory to browse (also used as root boundary)
      \param[in] filter
        File extension filter (e.g., ".tilemap")
    *************************************************************************/
    SimpleFileDialog(const std::string& startPath = "../../../Assets/Maps/",
        const std::string& filter = ".tilemap")
        : m_currentPath(startPath)
        , m_rootPath(startPath)
        , m_filter(filter)
        , m_isOpen(false)
        , m_mode(Mode::Open)
    {
        RefreshDirectory();
        m_filename[0] = '\0';
    }

    /*************************************************************************
      \brief
        Open dialog in specified mode
      \param[in] mode
        Open or SaveAs mode
      \param[in] defaultFilename
        Default filename for SaveAs mode
    *************************************************************************/
    void Open(Mode mode, const std::string& defaultFilename = "")
    {
        m_isOpen = true;
        m_mode = mode;
        m_selectedPath.clear();

        if (mode == Mode::SaveAs && !defaultFilename.empty())
        {
            strncpy_s(m_filename, sizeof(m_filename), defaultFilename.c_str(), _TRUNCATE);
        }
        else
        {
            m_filename[0] = '\0';
        }
    }

    /*************************************************************************
      \brief
        Render file dialog window
      \return
        True if file selected and confirmed
    *************************************************************************/
    bool Render()
    {
        if (!m_isOpen)
            return false;

        bool fileSelected = false;
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("File Browser", &m_isOpen))
        {
            // Navigation buttons - only show Up if not at root
            namespace fs = std::filesystem;
            fs::path currentNormalized = fs::absolute(m_currentPath).lexically_normal();
            fs::path rootNormalized = fs::absolute(m_rootPath).lexically_normal();
            bool canGoUp = currentNormalized != rootNormalized && currentNormalized.has_parent_path();

            if (canGoUp)
            {
                if (ImGui::Button("Up"))
                {
                    NavigateUp();
                }
                ImGui::SameLine();
            }

            if (ImGui::Button("Refresh"))
            {
                RefreshDirectory();
            }

            ImGui::Separator();

            // File/folder list
            ImGui::BeginChild("FileList", ImVec2(0, -60), true);

            for (const auto& entry : m_entries)
            {
                bool isDirectory = entry.is_directory;
                std::string displayName = isDirectory ? "[DIR] " + entry.name : entry.name;

                if (ImGui::Selectable(displayName.c_str(), false))
                {
                    if (isDirectory)
                    {
                        NavigateInto(entry.name);
                    }
                    else
                    {
                        // File selected
                        strncpy_s(m_filename, sizeof(m_filename), entry.name.c_str(), _TRUNCATE);
                    }
                }
            }

            ImGui::EndChild();

            ImGui::Separator();

            // Filename input (for SaveAs mode)
            ImGui::Text("Filename:");
            ImGui::SameLine();
            ImGui::InputText("##filename", m_filename, sizeof(m_filename));

            // Buttons
            ImGui::Separator();

            const char* confirmLabel = (m_mode == Mode::Open) ? "Open" : "Save";
            if (ImGui::Button(confirmLabel, ImVec2(100, 0)))
            {
                if (m_filename[0] != '\0')
                {
                    std::string filename = m_filename;

                    // Auto-append extension if missing
                    if (!m_filter.empty() && filename.find(m_filter) == std::string::npos)
                    {
                        filename += m_filter;
                    }

                    m_selectedPath = m_currentPath + filename;
                    fileSelected = true;
                    m_isOpen = false;
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0)))
            {
                m_isOpen = false;
            }
        }
        ImGui::End();

        return fileSelected;
    }

    /*************************************************************************
      \brief
        Get selected file path
      \return
        Full path to selected file
    *************************************************************************/
    const std::string& GetSelectedPath() const
    {
        return m_selectedPath;
    }

    /*************************************************************************
      \brief
        Check if dialog is open
      \return
        True if dialog is visible
    *************************************************************************/
    bool IsOpen() const
    {
        return m_isOpen;
    }

private:
    /*************************************************************************
      \brief
        Directory entry info
    *************************************************************************/
    struct Entry
    {
        std::string name;
        bool is_directory;
    };

    /*************************************************************************
      \brief
        Refresh directory listing
    *************************************************************************/
    void RefreshDirectory()
    {
        m_entries.clear();

        namespace fs = std::filesystem;

        try
        {
            if (!fs::exists(m_currentPath))
            {
                // Path doesn't exist, create it
                //fs::create_directories(m_currentPath);
                return;
            }

            for (const auto& entry : fs::directory_iterator(m_currentPath))
            {
                std::string filename = entry.path().filename().string();
                bool isDir = entry.is_directory();

                // Skip .meta files entirely
                if (!isDir && filename.size() > 5 && filename.substr(filename.size() - 5) == ".meta")
                {
                    continue;
                }

                // Filter files by extension
                if (!isDir && !m_filter.empty())
                {
                    if (filename.find(m_filter) == std::string::npos)
                        continue;
                }

                m_entries.push_back({ filename, isDir });
            }

            // Sort: directories first, then files
            std::sort(m_entries.begin(), m_entries.end(),
                [](const Entry& a, const Entry& b)
                {
                    if (a.is_directory != b.is_directory)
                        return a.is_directory > b.is_directory;
                    return a.name < b.name;
                });
        }
        catch (const std::filesystem::filesystem_error&)
        {
            // Directory doesn't exist or can't be read
        }
    }

    /*************************************************************************
      \brief
        Navigate up one directory level
    *************************************************************************/
    void NavigateUp()
    {
        namespace fs = std::filesystem;
        fs::path p(m_currentPath);
        fs::path rootNormalized = fs::absolute(m_rootPath).lexically_normal();
        fs::path currentNormalized = fs::absolute(p).lexically_normal();

        // Don't go above root path
        if (currentNormalized != rootNormalized && p.has_parent_path())
        {
            fs::path parentPath = p.parent_path();
            fs::path parentNormalized = fs::absolute(parentPath).lexically_normal();

            // Ensure parent is not above root
            std::string parentStr = parentNormalized.string();
            std::string rootStr = rootNormalized.string();

            if (parentStr.size() >= rootStr.size() &&
                parentStr.substr(0, rootStr.size()) == rootStr)
            {
                m_currentPath = parentPath.string() + "/";
                RefreshDirectory();
            }
        }
    }

    /*************************************************************************
      \brief
        Navigate into subdirectory
      \param[in] dirname
        Directory name to enter
    *************************************************************************/
    void NavigateInto(const std::string& dirname)
    {
        m_currentPath += dirname + "/";
        RefreshDirectory();
    }

    std::string m_currentPath;
    std::string m_rootPath;
    std::string m_filter;
    std::string m_selectedPath;
    std::vector<Entry> m_entries;
    char m_filename[256];
    bool m_isOpen;
    Mode m_mode;
    /*************************************************************************
     \brief
        update tilemapNode when project loads
    *************************************************************************/
    void SetPath(const std::string& newPath)
    {
        m_currentPath = newPath;
        m_rootPath = newPath;
        RefreshDirectory();
    }
};

#endif // SIMPLE_FILE_DIALOG_HPP