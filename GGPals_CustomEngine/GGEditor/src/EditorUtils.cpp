/*************************************************************************
  \file       EditorUtils.cpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       27 November 2025
  \version    1.0
  \brief
	Implements editor utility functions for scene-saving workflow and
	UI toast notifications.

  \details
	This file provides two primary sets of editor utilities:

	**EditorGeneralUtils**
	- Displays temporary toast notifications using ImGui.
	- Manages timed fading, stacking, and removal of active toasts.

	**EditorSceneUtils**
	- Tracks per-scene save state (dirty flag, Save As popup info).
	- Generates unique scene filenames.
	- Draws scene header UI for save status and user prompts.
	- Implements the "Save Scene" popup logic, including name validation
	  and scene serialization through SceneSerializer.

	Together, these utilities support the editor's user interface for
	scene file management and general UX feedback.

  \dependencies
	EditorUtils.hpp,
	GGLogger.hpp,
	<imgui.h>, <filesystem>, <string>, <unordered_map>

  \history
	- 1.0 (27 November 2025): Initial implementation of toast UI system and
	  scene save utilities for the editor.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <EditorUtils.hpp>
#include <GGLogger.hpp>
#include <GGEngine.hpp>

namespace EditorGeneralUtils
{
	/*************************************************************************
	  \brief
		Add a new toast notification to the editor.
	  \param msg
		Message text displayed in the toast.
	  \param color
		Text color used for the notification.
	  \param duration
		How long the toast remains visible (in seconds).
	  \details
		Pushes a new EditorToast entry into the toast list. Rendering is handled
		externally by RenderToasts().
	*************************************************************************/
	void ShowToast(const std::string& msg, ImVec4 color, float duration) 
	{
		EditorToast t;
		t.message = msg;
		t.duration = duration;
		t.color = color;
		t.timer = 0.f;
		m_toasts.push_back(t);
	}

	/*************************************************************************
	  \brief
		Render all active toast notifications in the editor UI.
	  \details
		- Updates toast timers and calculates fade-out transparency.
		- Draws each toast as an ImGui window.
		- Automatically removes expired toasts.
		- Newer toasts stack above older ones.
	*************************************************************************/
	void RenderToasts()
	{
		ImGuiIO& io = ImGui::GetIO();

		float yOffset = 20.f;

		for (int i = static_cast<int>(m_toasts.size()) - 1; i >= 0; --i)
		{
			EditorToast& t = m_toasts[i];
			t.timer += io.DeltaTime;

			float alpha = 1.f - (t.timer / t.duration);
			alpha = std::clamp(alpha, 0.f, 1.f);

			ImGui::SetNextWindowBgAlpha(alpha);

			ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 260.f, io.DisplaySize.y - yOffset));

			ImGuiWindowFlags flags =
				ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoInputs |
				ImGuiWindowFlags_NoNav |
				ImGuiWindowFlags_NoMove;

			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.20f, 0.65f, 0.20f, alpha * 0.90f));

			ImGui::Begin("##toast_window", nullptr, flags);
			ImGui::TextColored(ImVec4(t.color.x, t.color.y, t.color.z, alpha), "%s", t.message.c_str());
			ImGui::End();

			ImGui::PopStyleColor();

			yOffset += 40.f;

			if (t.timer >= t.duration) {
				m_toasts.erase(m_toasts.begin() + i);
			}	
		}
	}

} // end of namespace EditorGeneralUtils

namespace EditorSceneUtils
{
	/*************************************************************************
	  \brief
		Retrieve the SceneSaveState associated with a scene key.
	  \param sceneKey
		Unique identifier for the scene.
	  \return
		Reference to the SceneSaveState entry for the scene.
	  \details
		Internally creates a new entry if one does not already exist.
	*************************************************************************/
	SceneSaveState& GetSceneSaveState(const std::string& sceneKey)
	{	
		static std::unordered_map<std::string, SceneSaveState> sceneStates;
		return sceneStates[sceneKey];
	}

	/*************************************************************************
	  \brief
		Mark a scene as modified (unsaved).
	  \param sceneKey
		Scene to flag as dirty.
	  \details
		Causes the UI to append an asterisk (*) to the scene name.
	*************************************************************************/
	void MarkSceneDirty(const std::string& sceneKey) {
		GetSceneSaveState(sceneKey).isDirty = true;
	}

	/*************************************************************************
	  \brief
		Generate a unique scene filename based on a given base name.
	  \param base
		Base scene name (e.g., "MyScene").
	  \return
		A valid, non-conflicting scene name.
	  \details
		- Ensures the Assets/Scenes directory exists
		- Produces "MyScene", "MyScene2", "MyScene3", ... depending on availability
	*************************************************************************/
	std::string GetNextAvailableSceneName(const std::string& base)
	{
		const std::string sceneFolderPath = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath() + "Scenes";
		std::filesystem::create_directories(sceneFolderPath);

		std::string candidate;

		for (int i{ 0 }; ; ++i)
		{
			if (i == 0) {
				candidate = base;
			}
			else 
			{
				candidate = base + std::to_string(i + 1); // MyScene2, MyScene3, ...
			}

			std::filesystem::path path =
				std::filesystem::path(sceneFolderPath) / (candidate + ".scene");

			if (!std::filesystem::exists(path))
			{
				return candidate;
			}
		}
	}

	/*************************************************************************
	  \brief
		Draw the scene save header section in the editor UI.
	  \param sceneManager
		Reference to the active SceneManager.
	  \details
		- Displays the current scene name (with * if unsaved)
		- Provides a "Save Changes" button when a scene is dirty
		- Triggers Save As popup for unnamed scenes
	*************************************************************************/
	void DrawSceneSaveHeader(SceneManager& sceneManager)
	{
		const std::string& sceneName = sceneManager.GetActiveSceneKey();
		auto& state = GetSceneSaveState(sceneName);

		bool dirty = state.isDirty;
		std::string displayName = dirty ? (sceneName + "*") : sceneName; // add asterisk (*) to unsaved scenes

		ImGui::BeginGroup();

		ImGui::AlignTextToFramePadding();
		ImGui::Text("Scene:");
		ImGui::SameLine();

		ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", displayName.c_str());
		ImGui::SameLine();

		// only prompt user to save changes is the scene is dirty
		if (dirty) {
			ImGui::Dummy(ImVec2(10.0f, 0.0f));
			ImGui::SameLine();

			if (ImGui::SmallButton("Save Changes"))
			{
				if (sceneName == "UnknownScene")
				{
					std::string suggestion = GetNextAvailableSceneName("MyScene");
					std::strncpy(state.newSceneNameBuf, suggestion.c_str(),
						IM_ARRAYSIZE(state.newSceneNameBuf) - 1);
					state.newSceneNameBuf[IM_ARRAYSIZE(state.newSceneNameBuf) - 1] = '\0';
					state.openSaveAsPopup = true;
				}
				else
				{
					bool success = SceneSerializer::SaveCurrentScene(sceneName, sceneName, sceneManager);
					if (success) {
						state.isDirty = false; // remove asterisk (*)
					}
				}
			}
		}

		ImGui::EndGroup();
	}

	/*************************************************************************
	  \brief
		Draw and handle the Save As popup window for saving scenes.
	  \param sceneManager
		Reference to the active SceneManager.
	  \details
		- Prompts user for a scene name if the scene is unnamed
		- Validates filename and detects name conflicts
		- Performs scene serialization via SceneSerializer
		- Resets dirty state after a successful save
	*************************************************************************/
	void DrawSceneSavePopup(SceneManager& sceneManager)
	{
		const std::string& sceneName = sceneManager.GetActiveSceneKey();
		auto& state = GetSceneSaveState(sceneName);

		if (state.openSaveAsPopup)
		{
			ImGui::OpenPopup("Save Scene");
			state.openSaveAsPopup = false;
		}

		if (ImGui::BeginPopupModal("Save Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("This scene does not have a name yet.");
			ImGui::Text("Please enter a scene name:");
			ImGui::InputText("##NewSceneName", state.newSceneNameBuf, IM_ARRAYSIZE(state.newSceneNameBuf));

			ImGui::Separator();

			const std::string sceneFolderPath = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath() + "Scenes";

			std::string baseName(state.newSceneNameBuf);

			const std::string ext = ".scene";
			if (baseName.size() >= ext.size() &&
				baseName.compare(baseName.size() - ext.size(), ext.size(), ext) == 0)
			{
				baseName = baseName.substr(0, baseName.size() - ext.size());
			}

			std::filesystem::path fullPath =
				std::filesystem::path(sceneFolderPath) / (baseName + ".scene");

			bool nameClashes = std::filesystem::exists(fullPath);
			if (nameClashes)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
					"A scene named \"%s\" already exists.", baseName.c_str());
				ImGui::Text("Are you sure you want to overwrite the scene?");
				ImGui::Spacing();
			}

			bool nameEmpty = baseName.empty();

			// --- centered full-width buttons ---
			float region = ImGui::GetContentRegionAvail().x;
			float spacing = ImGui::GetStyle().ItemSpacing.x;

			float buttonWidth = (region - spacing) * 0.5f;

			// save button
			ImGui::BeginDisabled(nameEmpty);
			if (ImGui::Button("Save", ImVec2(buttonWidth, 0)))
			{
				const std::string& currentKey = sceneManager.GetActiveSceneKey();
				bool success = SceneSerializer::SaveCurrentScene(currentKey, baseName, sceneManager);
				if (success) {
					state.isDirty = false; //  remove asterisk (*)
					GGLogger::Info("Scene saved as: " + fullPath.string());
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndDisabled();

			ImGui::SameLine();

			// cancel button
			if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0)))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

} // end of namespace EditorSceneUtils