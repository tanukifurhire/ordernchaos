/*************************************************************************
  \file       BuildSizeAnalyzer.hpp
  \author     Nelson Tok
  \par        Email: t.duanjienelson@digipen.edu
  \date       26 March 2026
  \version    1.0
  \brief
    Declares the BuildSizeAnalyzer editor window for GGEngine.
  \details
    Provides an ImGui-based window that lists all discovered project
    assets with their file sizes, allows selective export via checkboxes
    (including Select All / Deselect All), displays a running total of
    selected asset sizes, persists selections across window open/close,
    and exports chosen assets to a user-selected destination folder.
    Accessible from the StopPlayToolbar via a dedicated button.
*************************************************************************/
#pragma once

#include "EditorSceneBase.hpp"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <cstdint>
#include <memory>

/*************************************************************************
  \struct AssetEntry
  \brief
    Holds display data for a single discoverable asset.
*************************************************************************/
struct AssetEntry
{
    std::string relativePath;       // Relative path from Assets/ root
    std::string displayName;        // Filename only, used in the UI list
    std::string category;           // e.g. "Textures", "Audio", etc.
    uintmax_t   sizeBytes{ 0 };     // File size in bytes (0 if unreadable)
    bool        selected{ false };  // Whether this asset is checked for export
};

/*************************************************************************
  \class BuildSizeAnalyzer
  \brief
    Editor window for reviewing project assets, selecting assets for
    export, and monitoring the total size of the selected export.
  \details
    Rubric compliance:
      1. Lists all assets in the project with their file sizes.
      2. Checkbox selection per asset + Select All / Deselect All.
      3. Dynamically updated total size of selected assets.
      4. Persists selections while the window is alive; restores on reopen
         via the static selection-persistence map.
      5. Export button: folder picker to copies selected assets.
      6. Opened from the toolbar button added to StopPlayToolbar.
*************************************************************************/
class BuildSizeAnalyzer
{
public:

    /*************************************************************************
      \brief
        Constructor clears state and marks list as needing a refresh.
    *************************************************************************/
    BuildSizeAnalyzer();

    /*************************************************************************
      \brief
        Renders the full Build Size Analyzer ImGui window.
      \details
        Call this once per frame when isOpen is true.  Internally calls
        RefreshAssetList() on first use or after a project change, then
        renders the asset table, totals bar, and action buttons.
    *************************************************************************/
    void Render();

    /*************************************************************************
      \brief
        Controls whether the window is visible.
    *************************************************************************/
    bool isOpen{ false };

    /*************************************************************************
      \brief
        Call after a project is loaded / changed so the list rebuilds.
    *************************************************************************/
    void MarkDirty() { needsRefresh = true; }

private:
    
    /*************************************************************************
      \brief
        Scans GGProjects/ for valid project folders (containing Assets/).
      \details
        Populates availableProjects. Called once when the window opens.
    *************************************************************************/
    void ScanForProjects();

    /*************************************************************************
      \brief
        Renders the top project-picker row (combo + Load button).
      \details
        Allows loading any GGProjects project independently of which project
        is currently open in the engine.
    *************************************************************************/
    void RenderProjectPicker();

    /*************************************************************************
      \brief
        Walks the current AssetManager discovered path lists, queries each
        file size via std::filesystem, and populates assetEntries.
      \details
        Also restores the "selected" flag for any path that was previously
        saved in the persistent selection map.
    *************************************************************************/
    void RefreshAssetList();

    /*************************************************************************
      \brief
        Renders the filter / search bar and the scrollable asset table.
    *************************************************************************/
    void RenderAssetTable();

    /*************************************************************************
      \brief
        Renders one collapsible category section with its inner asset table.
      \param[in] category   Display name of the category (e.g. "Textures").
      \param[in] indices    Indices into assetEntries belonging to this category.
    *************************************************************************/
    void RenderCategorySection(const std::string& category,
        const std::vector<int>& indices,
        const std::string& filterLower);

    /*************************************************************************
      \brief
        Renders the bottom bar showing total selected size and action buttons.
    *************************************************************************/
    void RenderBottomBar();

    /*************************************************************************
      \brief
        Opens a native folder picker dialog (via NFD or a fallback ImGui
        input) and copies all selected assets to the chosen destination.
    *************************************************************************/
    void RunExport();

    /*************************************************************************
      \brief
        Converts a raw byte count to a human-readable string such as
        "1.23 MB" or "456 KB".
    *************************************************************************/
    static std::string FormatBytes(uintmax_t bytes);

    /*************************************************************************
      \brief
        Returns the category string for a file based on its extension.
    *************************************************************************/
    static std::string CategoryFromExtension(const std::string& ext);

    // -----------------------------------------------------------------------
    // Data
    // -----------------------------------------------------------------------

    // Project picker state
    std::vector<std::string> availableProjects; // Project folder names under GGProjects/
    int  selectedProjectIdx{ -1 };              // Index into availableProjects
    std::string activeProjectAssetsPath;        // Full path to Assets/ of chosen project
    std::string activeProjectName;              // Name of the chosen project

    // asset data
    std::vector<AssetEntry> assetEntries;   // All discovered assets

    // Maps category name to list of indices into assetEntries
    std::map<std::string, std::vector<int>> categoryMap;

    bool needsRefresh{true};              // Triggers RefreshAssetList on next Render

    uintmax_t totalSelectedBytes{ 0 };      // Sum of sizes for checked entries
    int       totalSelectedCount{ 0 };      // Number of checked entries

    char searchBuf[256]{ '\0' };            // Filter bar input buffer

    // Export-folder popup state
    char    exportFolderNameBuf[256]{ '\0' };
    bool    exportStatusIsError{ false };
    std::string exportStatusMessage;

    /*************************************************************************
      \brief
        Persistent selection map: relative path to selected flag.
      \details
        Declared static so selections survive the window being closed and
        reopened within the same editor session (satisfies requirement 4).
    *************************************************************************/
    static std::unordered_map<std::string, bool> s_persistedSelections;
};

/*************************************************************************
  \brief
    Returns the singleton instance of BuildSizeAnalyzer.
*************************************************************************/
inline auto& GetBuildSizeAnalyzer()
{
    static std::unique_ptr<BuildSizeAnalyzer> instance =
        std::make_unique<BuildSizeAnalyzer>();
    return instance;
}