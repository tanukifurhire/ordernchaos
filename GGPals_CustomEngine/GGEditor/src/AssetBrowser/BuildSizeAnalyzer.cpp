/*************************************************************************
  \file       BuildSizeAnalyzer.cpp
  \author     Nelson Tok
  \par        Email: t.duanjienelson@digipen.edu
  \date       26 March 2026
  \version    1.0
  \brief
    Implements the BuildSizeAnalyzer editor window for GGEngine.
  \details
    Pulls asset lists from AssetManagerNode's discovered paths, queries
    file sizes, renders an ImGui window with per-asset checkboxes, a
    running total, and an export button that copies selected files to a
    chosen destination folder.
*************************************************************************/
#include "BuildSizeAnalyzer.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <AssetManagerNode.hpp>
#include <GGAssetManager.hpp>
#include <GGECSAssetInterface.hpp>

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <unordered_set>

namespace fs = std::filesystem;

// Static member definition
std::unordered_map<std::string, bool> BuildSizeAnalyzer::s_persistedSelections;

// Constructor
BuildSizeAnalyzer::BuildSizeAnalyzer()
{
    needsRefresh = true;
}

/*************************************************************************
  \brief
    Top-level render function called every frame while isOpen == true.
  \details
    Renders three sections in order:
      1. Project picker row
      2. Collapsible asset list with search + select-all controls
      3. Bottom bar with totals, export-name field, and Export button
*************************************************************************/
void BuildSizeAnalyzer::Render()
{
    if (!isOpen) return;

    // Scan project list the first time the window opens each session
    if (availableProjects.empty())
        ScanForProjects();

    // Rebuild asset list when the active project changes
    if (needsRefresh && !activeProjectAssetsPath.empty())
    {
        RefreshAssetList();
        needsRefresh = false;
    }

    ImGui::SetNextWindowSize(ImVec2(900.f, 700.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(700.f, 500.f), ImVec2(FLT_MAX, FLT_MAX));

    if (ImGui::Begin("Build Size Analyzer", &isOpen))
    {
        RenderProjectPicker();
        ImGui::Separator();

        if (activeProjectAssetsPath.empty())
        {
            ImGui::Spacing();
            ImGui::TextDisabled("Select a project above to view its assets.");
        }
        else
        {
            RenderAssetTable();
            ImGui::Separator();
            RenderBottomBar();
        }
    }
    ImGui::End();

    // Flush selections to the persistent map every frame
    for (const auto& entry : assetEntries)
        s_persistedSelections[entry.relativePath] = entry.selected;
}

/*************************************************************************
  \brief
    Scans the GGProjects/ directory (relative to the engine working
    directory) and populates availableProjects with folder names that
    contain an Assets/ sub-directory.
  \details
    The path "../../../GGProjects" mirrors the same convention used by
    StopPlayToolbar::RenderProjectControls so the same set of projects
    is always visible regardless of which one the engine has loaded.
*************************************************************************/
void BuildSizeAnalyzer::ScanForProjects()
{
    availableProjects.clear();

    fs::path ggProjectsRoot = fs::weakly_canonical(
        fs::current_path() / "../../../GGProjects");

    if (!fs::exists(ggProjectsRoot) || !fs::is_directory(ggProjectsRoot))
        return;

    for (const auto& entry : fs::directory_iterator(ggProjectsRoot))
    {
        if (entry.is_directory() &&
            fs::exists(entry.path() / "Assets"))
        {
            availableProjects.push_back(entry.path().filename().string());
        }
    }

    std::sort(availableProjects.begin(), availableProjects.end());

    // Auto-select the currently loaded engine project if it is in the list
    GGAssetManager* mgr = GGEngine::GetEngine().GetAssetManager();
    if (mgr && !mgr->GetAssetBasePath().empty())
    {
        fs::path loaded = fs::path(mgr->GetAssetBasePath()).parent_path();
        std::string loadedName = loaded.filename().string();
        for (int i = 0; i < (int)availableProjects.size(); ++i)
        {
            if (availableProjects[i] == loadedName)
            {
                selectedProjectIdx = i;
                activeProjectName = loadedName;
                activeProjectAssetsPath =
                    (ggProjectsRoot / loadedName / "Assets").string();
                // Pre-fill export name to avoid an empty field
                if (exportFolderNameBuf[0] == '\0')
                {
                    std::string defName = loadedName + "_Export";
                    std::strncpy(exportFolderNameBuf, defName.c_str(),
                        sizeof(exportFolderNameBuf) - 1);
                }
                needsRefresh = true;
                break;
            }
        }
    }
}

/*************************************************************************
  \brief
    Renders the project-picker row at the top of the window.
  \details
    A combo lists every project found in GGProjects/. Clicking "Load"
    sets activeProjectAssetsPath and triggers a list refresh.
    This is independent of the engine's currently loaded project.
*************************************************************************/
void BuildSizeAnalyzer::RenderProjectPicker()
{
    ImGui::Text("Project:");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(300.f);
    const char* previewLabel = (selectedProjectIdx >= 0 &&
        selectedProjectIdx < (int)availableProjects.size())
        ? availableProjects[selectedProjectIdx].c_str()
        : "none";

    if (ImGui::BeginCombo("##bsa_projcombo", previewLabel))
    {
        for (int i = 0; i < (int)availableProjects.size(); ++i)
        {
            bool isSel = (selectedProjectIdx == i);
            if (ImGui::Selectable(availableProjects[i].c_str(), isSel))
                selectedProjectIdx = i;
            if (isSel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    bool canLoad = (selectedProjectIdx >= 0 &&
        selectedProjectIdx < (int)availableProjects.size());
    if (!canLoad) ImGui::BeginDisabled();

    if (ImGui::Button("Load##bsa_load"))
    {
        activeProjectName = availableProjects[selectedProjectIdx];

        fs::path ggProjectsRoot = fs::weakly_canonical(
            fs::current_path() / "../../../GGProjects");
        activeProjectAssetsPath =
            (ggProjectsRoot / activeProjectName / "Assets").string();

        // Prefill the export name from the project name
        std::string defName = activeProjectName + "_Export";
        std::strncpy(exportFolderNameBuf, defName.c_str(),
            sizeof(exportFolderNameBuf) - 1);

        exportStatusMessage.clear();
        needsRefresh = true;
    }

    if (!canLoad) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Refresh##bsa_refresh"))
    {
        availableProjects.clear();
        ScanForProjects();
    }

    // Show active path as a subtle hint
    if (!activeProjectAssetsPath.empty())
    {
        ImGui::SameLine(0.f, 16.f);
        ImGui::TextDisabled("%s", activeProjectAssetsPath.c_str());
    }
}

/*************************************************************************
  \brief
    Clears assetEntries and rebuilds it from the discovered paths held by
    AssetManagerNode.  File size is queried with std::filesystem.
  \details
    After populating, restores any previously saved selections from
    s_persistedSelections so the user's choices survive a window close.
*************************************************************************/
void BuildSizeAnalyzer::RefreshAssetList()
{
    assetEntries.clear();
    categoryMap.clear();
    totalSelectedBytes = 0;
    totalSelectedCount = 0;

    if (activeProjectAssetsPath.empty()) return;

    fs::path assetsRoot(activeProjectAssetsPath);
    if (!fs::exists(assetsRoot)) return;

    for (const auto& dirEntry : fs::recursive_directory_iterator(
        assetsRoot, fs::directory_options::skip_permission_denied))
    {
        if (!dirEntry.is_regular_file()) continue;

        // Skip .meta files — they are internal UUID bookkeeping
        if (dirEntry.path().extension() == ".meta") continue;

        // Derive relative path from Assets/ root
        std::string relPath = fs::relative(
            dirEntry.path(), assetsRoot).generic_string();

        AssetEntry e;
        e.relativePath = relPath;
        e.displayName = dirEntry.path().filename().string();

        std::string ext = dirEntry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        e.category = CategoryFromExtension(ext);

        std::error_code ec;
        e.sizeBytes = fs::file_size(dirEntry.path(), ec);
        if (ec) e.sizeBytes = 0;

        // Restore persisted selection
        auto it = s_persistedSelections.find(relPath);
        e.selected = (it != s_persistedSelections.end()) ? it->second : false;

        if (e.selected)
        {
            totalSelectedBytes += e.sizeBytes;
            ++totalSelectedCount;
        }

        assetEntries.push_back(std::move(e));
    }

    // Sort: category first, then display name
    std::sort(assetEntries.begin(), assetEntries.end(),
        [](const AssetEntry& a, const AssetEntry& b)
        {
            if (a.category != b.category) return a.category < b.category;
            return a.displayName < b.displayName;
        });

    // Build category -> index map for the collapsible renderer
    for (int i = 0; i < (int)assetEntries.size(); ++i)
        categoryMap[assetEntries[i].category].push_back(i);
}

/*************************************************************************
  \brief
    Renders the search bar, Select All / Deselect All, and iterates
    categoryMap to draw one collapsible section per category.
*************************************************************************/
void BuildSizeAnalyzer::RenderAssetTable()
{
    // --- Search bar ---
    ImGui::SetNextItemWidth(280.f);
    ImGui::InputText("##bsa_filter", searchBuf, sizeof(searchBuf));
    ImGui::SameLine();
    ImGui::TextDisabled("Filter");
    ImGui::SameLine(0.f, 20.f);

    // --- Select All / Deselect All ---
    if (ImGui::Button("Select All"))
    {
        totalSelectedBytes = 0;
        totalSelectedCount = 0;
        for (auto& e : assetEntries)
        {
            e.selected = true;
            totalSelectedBytes += e.sizeBytes;
            ++totalSelectedCount;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Deselect All"))
    {
        for (auto& e : assetEntries) e.selected = false;
        totalSelectedBytes = 0;
        totalSelectedCount = 0;
    }

    ImGui::Spacing();

    // Build lowercase filter once
    std::string filterLower(searchBuf);
    std::transform(filterLower.begin(), filterLower.end(),
        filterLower.begin(), ::tolower);

    // Scrollable region for all category sections
    const float scrollHeight = ImGui::GetContentRegionAvail().y - 58.f;
    ImGui::BeginChild("##bsa_scroll", ImVec2(0.f, scrollHeight), false);

    for (auto& [cat, indices] : categoryMap)
        RenderCategorySection(cat, indices, filterLower);

    ImGui::EndChild();
}

/*************************************************************************
  \brief
    Renders a single collapsible category block containing a compact
    asset table.
  \param[in] category     Display name (e.g. "Textures").
  \param[in] indices      Indices into assetEntries for this category.
  \param[in] filterLower  Lowercase search string (empty = show all).
  \details
    The header label shows the category name plus a count of how many
    assets in this category are currently selected and match the filter.
    A "Select Category" / "Deselect Category" pair of small buttons sits
    right-aligned in the header row before the arrow.
*************************************************************************/
void BuildSizeAnalyzer::RenderCategorySection(const std::string& category,
    const std::vector<int>& indices,
    const std::string& filterLower)
{
    // Count visible (filter-passing) entries for the header label
    int visibleCount = 0;
    int selectedInCat = 0;
    for (int idx : indices)
    {
        const auto& e = assetEntries[idx];
        if (!filterLower.empty())
        {
            std::string nl = e.displayName, cl = e.category;
            std::transform(nl.begin(), nl.end(), nl.begin(), ::tolower);
            if (nl.find(filterLower) == std::string::npos) continue;
        }
        ++visibleCount;
        if (e.selected) ++selectedInCat;
    }

    if (visibleCount == 0) return; // hide entire section if filter hides all

    // Build header label: "Textures  (3 / 12 selected)"
    std::string headerLabel = category +
        "  (" + std::to_string(selectedInCat) +
        " / " + std::to_string(visibleCount) + " selected)";

    // CollapsingHeader always owns 100% of the row width for hit-testing,
    // so no widget placed on the same line via SameLine() will ever receive
    // mouse clicks. The only reliable solution: draw the header normally,
    // then place the buttons on the very next line with a small indent.
    ImGui::PushID(category.c_str());

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen |
        ImGuiTreeNodeFlags_OpenOnArrow;

    bool open = ImGui::CollapsingHeader(headerLabel.c_str(), flags);

    // Buttons sit on the line immediately after the header, right-aligned.
    // They are always drawn (not just when open) so the user can quickly
    // select/deselect a whole category without expanding it first.
    {
        const float btnWidth = 70.f;
        const float btnGap = 4.f;

        float regionMaxX = ImGui::GetWindowContentRegionMax().x;
        //float cursorStartX = ImGui::GetCursorPosX();
        float buttonStartX = regionMaxX - (btnWidth * 2.f + btnGap);

        ImGui::SetCursorPosX(buttonStartX);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 1.f));

        if (ImGui::Button(("Sel All##" + category).c_str(), ImVec2(btnWidth, 0.f)))
        {
            for (int idx : indices)
            {
                auto& e = assetEntries[idx];
                if (!e.selected)
                {
                    e.selected = true;
                    totalSelectedBytes += e.sizeBytes;
                    ++totalSelectedCount;
                }
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Select all in %s", category.c_str());

        ImGui::SameLine(0.f, btnGap);

        if (ImGui::Button(("Desel All##" + category).c_str(), ImVec2(btnWidth, 0.f)))
        {
            for (int idx : indices)
            {
                auto& e = assetEntries[idx];
                if (e.selected)
                {
                    e.selected = false;
                    totalSelectedBytes = (e.sizeBytes <= totalSelectedBytes)
                        ? totalSelectedBytes - e.sizeBytes : 0;
                    if (totalSelectedCount > 0) --totalSelectedCount;
                }
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Deselect all in %s", category.c_str());

        ImGui::PopStyleVar();
    }

    if (open)
    {
        constexpr ImGuiTableFlags tFlags =
            ImGuiTableFlags_Borders |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_SizingStretchProp;

        if (ImGui::BeginTable(("##tbl_" + category).c_str(), 3, tFlags))
        {
            ImGui::TableSetupColumn("Export", ImGuiTableColumnFlags_WidthFixed, 50.f);
            ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 75.f);
            ImGui::TableHeadersRow();

            for (int idx : indices)
            {
                auto& entry = assetEntries[idx];

                // Apply filter
                if (!filterLower.empty())
                {
                    std::string nl = entry.displayName;
                    std::transform(nl.begin(), nl.end(), nl.begin(), ::tolower);
                    if (nl.find(filterLower) == std::string::npos) continue;
                }

                ImGui::TableNextRow();

                // Checkbox
                ImGui::TableSetColumnIndex(0);
                bool prev = entry.selected;
                ImGui::PushID(entry.relativePath.c_str());
                if (ImGui::Checkbox("##sel", &entry.selected))
                {
                    if (entry.selected && !prev)
                    {
                        totalSelectedBytes += entry.sizeBytes;
                        ++totalSelectedCount;
                    }
                    else if (!entry.selected && prev)
                    {
                        totalSelectedBytes = (entry.sizeBytes <= totalSelectedBytes)
                            ? totalSelectedBytes - entry.sizeBytes : 0;
                        if (totalSelectedCount > 0) --totalSelectedCount;
                    }
                }
                ImGui::PopID();

                // Asset name to tooltip shows relative path
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(entry.displayName.c_str());
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", entry.relativePath.c_str());

                // Size
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(FormatBytes(entry.sizeBytes).c_str());
            }

            ImGui::EndTable();
        }
        ImGui::Spacing();
    }

    ImGui::PopID();
}

/*************************************************************************
  \brief
    Renders the bottom bar: total size label, export-folder name input,
    and the Export button. The export destination is always
    GGProjects/<exportFolderNameBuf>/Assets so only the project name
    needs to be specified.
*************************************************************************/
void BuildSizeAnalyzer::RenderBottomBar()
{
    // Totals
    std::string totalLabel =
        std::to_string(totalSelectedCount) +
        " asset(s) selected  |  Total: " +
        FormatBytes(totalSelectedBytes);
    ImGui::TextUnformatted(totalLabel.c_str());

    ImGui::SameLine();
    // Status message on its own line so it never squashes the input field
    if (!exportStatusMessage.empty())
    {
        ImVec4 col = exportStatusIsError
            ? ImVec4(1.f, 0.3f, 0.3f, 1.f)
            : ImVec4(0.3f, 1.f, 0.3f, 1.f);
        ImGui::TextColored(col, "%s", exportStatusMessage.c_str());
    }

    ImGui::Spacing();

    // Export folder name input
    const float exportBtnWidth = 145.f;
    const float slashTextWidth = ImGui::CalcTextSize("/Assets").x + ImGui::GetStyle().ItemSpacing.x;
    const float labelWidth = ImGui::CalcTextSize("Export to GGProjects/").x + ImGui::GetStyle().ItemSpacing.x;
    const float totalAvail = ImGui::GetContentRegionAvail().x;
    float inputWidth = totalAvail - labelWidth - slashTextWidth - exportBtnWidth
        - ImGui::GetStyle().ItemSpacing.x * 3.f;
    if (inputWidth < 60.f) inputWidth = 60.f;

    ImGui::Text("Export to GGProjects/");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(inputWidth);
    ImGui::InputText("##bsa_exportname", exportFolderNameBuf,
        sizeof(exportFolderNameBuf));
    ImGui::SameLine();
    ImGui::TextDisabled("/Assets");
    ImGui::SameLine();

    bool canExport = (totalSelectedCount > 0) && (exportFolderNameBuf[0] != '\0');
    if (!canExport) ImGui::BeginDisabled();

    if (ImGui::Button("Export Selected", ImVec2(exportBtnWidth, 0.f)))
        RunExport();

    if (!canExport) ImGui::EndDisabled();

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && !canExport)
    {
        if (totalSelectedCount == 0)
            ImGui::SetTooltip("Select at least one asset first.");
        else
            ImGui::SetTooltip("Enter an export folder name first.");
    }
}

/*************************************************************************
  \brief
    Derives the .meta file path for a given asset, mirroring the exact
    logic used by GGAssetID::ComputeMetaPath so UUIDs are preserved.
  \details
    Shader files (.glsl, .vert, .frag) share a single .meta keyed on the
    stem name only (e.g. "Shaders/myShader.meta" for both
    "Shaders/myShader.vert" and "Shaders/myShader.frag").
    All other asset types append ".meta" directly to the full filename
    (e.g. "Textures/player.png.meta").
  \param[in] assetRelPath
    Relative path of the asset from the Assets/ root (forward-slash form).
  \return
    Relative path of the corresponding .meta file (same directory).
*************************************************************************/
static fs::path ComputeMetaRelPath(const std::string& assetRelPath)
{
    fs::path p(assetRelPath);
    std::string ext = p.extension().string();

    // Shader files share one meta keyed on stem (no extension)
    if (ext == ".glsl" || ext == ".vert" || ext == ".frag")
        return p.parent_path() / (p.stem().string() + ".meta");

    // Everything else: append .meta to the full filename
    return fs::path(assetRelPath + ".meta");
}

/*************************************************************************
  \brief
    Copies all selected assets from the active analyzer project's Assets/
    folder into GGProjects/<exportFolderNameBuf>/Assets/, preserving the
    full relative sub-folder structure.
  \details
    The destination is always kept inside the GGProjects/ root so it
    appears alongside other projects and can be opened by the engine.
    Existing files are overwritten. The Assets/ sub-folder is created
    automatically if absent.
*************************************************************************/
void BuildSizeAnalyzer::RunExport()
{
    exportStatusMessage.clear();

    if (activeProjectAssetsPath.empty())
    {
        exportStatusIsError = true;
        exportStatusMessage = "No project loaded.";
        return;
    }

    std::string outName(exportFolderNameBuf);
    if (outName.empty())
    {
        exportStatusIsError = true;
        exportStatusMessage = "Enter an export folder name.";
        return;
    }

    // Destination: GGProjects/<GameProjectName>/Assets/
    fs::path ggProjectsRoot = fs::weakly_canonical(
        fs::current_path() / "../../../GGProjects");
    fs::path destAssetsRoot = ggProjectsRoot / outName / "Assets";

    std::error_code ec;
    fs::create_directories(destAssetsRoot, ec);
    if (ec)
    {
        exportStatusIsError = true;
        exportStatusMessage = "Could not create folder: " + ec.message();
        return;
    }

    const std::vector<std::string> subFolders =
    {
        "Textures", 
        "Shaders", 
        "Scripts", 
        "Scenes", 
        "Prefabs", 
        "Maps", 
        "Fonts", 
        "Audio", 
        "Animations"
    };
    for (const auto& sub : subFolders)
        fs::create_directories(destAssetsRoot / sub, ec);

    fs::path srcAssetsRoot(activeProjectAssetsPath);
    int copiedCount = 0;
    int failedCount = 0;

    std::unordered_set<std::string> copiedMetaPaths;

    for (const auto& entry : assetEntries)
    {
        if (!entry.selected) continue;

        fs::path srcFull = srcAssetsRoot / entry.relativePath;
        fs::path destFull = destAssetsRoot / entry.relativePath;

        fs::create_directories(destFull.parent_path(), ec);
        fs::copy_file(srcFull, destFull,
            fs::copy_options::overwrite_existing, ec);

        if (ec) { ++failedCount; continue; }
        ++copiedCount;

        fs::path metaRelPath = ComputeMetaRelPath(entry.relativePath);
        std::string metaRelStr = metaRelPath.generic_string();

        if (copiedMetaPaths.count(metaRelStr)) continue;
        copiedMetaPaths.insert(metaRelStr);

        fs::path srcMeta = srcAssetsRoot / metaRelPath;
        fs::path destMeta = destAssetsRoot / metaRelPath;

        if (fs::exists(srcMeta))
        {
            fs::create_directories(destMeta.parent_path(), ec);
            fs::copy_file(srcMeta, destMeta,
                fs::copy_options::overwrite_existing, ec);
        }
    }



    if (failedCount == 0)
    {
        exportStatusIsError = false;
        exportStatusMessage = "Exported " + std::to_string(copiedCount) +
            " file(s) -> GGProjects/" + outName + "/Assets";
    }
    else
    {
        exportStatusIsError = true;
        exportStatusMessage = "| Exported " + std::to_string(copiedCount) +
            " file(s). " +
            std::to_string(failedCount) + " failed.";
    }
}

/*************************************************************************
  \brief
    Converts a raw byte count to a human-readable string.
  \param[in] bytes Raw byte count.
  \return  String such as "1.23 MB", "456 KB", or "789 B".
*************************************************************************/
std::string BuildSizeAnalyzer::FormatBytes(uintmax_t bytes)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);

    if (bytes >= 1024ULL * 1024ULL * 1024ULL)
        oss << (bytes / (1024.0 * 1024.0 * 1024.0)) << " GB";
    else if (bytes >= 1024ULL * 1024ULL)
        oss << (bytes / (1024.0 * 1024.0)) << " MB";
    else if (bytes >= 1024ULL)
        oss << (bytes / 1024.0) << " KB";
    else
        oss << bytes << " B";

    return oss.str();
}

/*************************************************************************
  \brief 
    Maps a lowercase file extension to a category string.
*************************************************************************/
std::string BuildSizeAnalyzer::CategoryFromExtension(const std::string& ext)
{
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".bmp" || ext == ".tga")                     return "Textures";

    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3")    return "Audio";

    if (ext == ".vert" || ext == ".frag" || ext == ".glsl") return "Shaders";

    if (ext == ".lua")                                      return "Scripts";

    if (ext == ".prefab")                                   return "Prefabs";

    if (ext == ".scene")                                    return "Scenes";

    if (ext == ".gganim")                                   return "Animations";

    if (ext == ".tilemap")                                  return "Tilemaps";

    if (ext == ".ttf" || ext == ".otf")                     return "Fonts";

    return "Other";
}