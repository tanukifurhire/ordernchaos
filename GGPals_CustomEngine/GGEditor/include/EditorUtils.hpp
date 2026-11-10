/*************************************************************************
  \file       EditorUtils.hpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       27 November 2025
  \version    1.0
  \brief
	Declares utility functions and structures for editor UI features,
	including toast notifications and scene save state management.

  \details
	Provides editor-side helpers for:
	- Displaying temporary UI toast messages
	- Rendering toast animations and handling their lifetime
	- Tracking scene save states (dirty flags, Save As popup state)
	- Generating unique scene names for saving
	- Drawing scene save UI elements in ImGui

  \dependencies
	<string>, <vector>, <filesystem>, <imgui.h>,
	GGSceneManager.hpp

  \history
	- 1.0 (27 November 2025): Initial version containing editor utility
	  helpers for toast notifications and scene save UI integration.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <string>
#include <filesystem>
#include <GGSceneManager.hpp>
#include <imgui.h>

/*************************************************************************
  \namespace EditorGeneralUtils
  \brief
	Utility functions for editor-wide UI helpers such as toast notifications.
*************************************************************************/
namespace EditorGeneralUtils
{
	/*************************************************************************
	  \struct EditorToast
	  \brief
		Represents a temporary toast notification displayed in the editor UI.
	  \details
		- Stores the message, duration, remaining time, and display color.
		- Used by ShowToast() and RenderToasts().
	*************************************************************************/
	struct EditorToast
	{
		std::string message;
		float duration = 2.f; // seconds
		float timer = 0.f;
		ImVec4 color;
	};

	static std::vector<EditorToast> m_toasts;

	// default text color (white)
	#define DEFAULT_TEXT_COLOR ImVec4(1.f, 1.f, 1.f, 1.f)

	/*************************************************************************
	  \brief
		Show a toast notification at the top of the editor UI.
	  \param msg
		Message to display.
	  \param color
		Toast text color (default: white).
	  \param duration
		Duration in seconds before the toast disappears.
	*************************************************************************/
	void ShowToast(const std::string& msg, ImVec4 color = DEFAULT_TEXT_COLOR, float duration = 2.f);

	/*************************************************************************
	  \brief
		Render and update all active toast notifications.
	  \details
		Handles timing, fade-out, and removal of expired toasts.
	*************************************************************************/
	void RenderToasts();

}

namespace EditorSceneUtils
{
	/*************************************************************************
	  \struct SceneSaveState
	  \brief
		Tracks save-related UI state for a scene.
	  \details
		- Determines whether the scene is marked dirty (unsaved changes)
		- Controls Save As popup state and its input buffer
	*************************************************************************/
	struct SceneSaveState
	{
		bool isDirty = false; // add an asterisk (*) to unsaved scenes

		bool openSaveAsPopup = false;
		char newSceneNameBuf[128] = "MyScene";
	};

	/*************************************************************************
	  \brief
		Get the SceneSaveState object associated with a specific scene key.
	  \param sceneKey
		Name/key of the scene.
	  \return
		Reference to the save state object.
	*************************************************************************/
	SceneSaveState& GetSceneSaveState(const std::string& sceneKey);

	/*************************************************************************
	  \brief
		Generate a valid, unused scene name by appending a number to a base name.
	  \param base
		Base name (e.g., "NewScene").
	  \return
		Next available unique scene name.
	*************************************************************************/
	std::string GetNextAvailableSceneName(const std::string& base);

	/*************************************************************************
	 \brief
	   Draw the scene save bar/header in the editor UI.
	 \param sceneManager
	   Reference to SceneManager used to check active scene and save status.
   *************************************************************************/
	void DrawSceneSaveHeader(SceneManager& sceneManager);

	/*************************************************************************
	  \brief
		Draw and handle the Save As popup for naming/creating a new scene file.
	  \param sceneManager
		Reference to SceneManager used to save the scene.
	*************************************************************************/
	void DrawSceneSavePopup(SceneManager& sceneManager);

	/*************************************************************************
	  \brief
		Mark a scene as dirty, indicating unsaved changes.
	  \param sceneKey
		Scene to flag as dirty.
	*************************************************************************/
	void MarkSceneDirty(const std::string& sceneKey);

}