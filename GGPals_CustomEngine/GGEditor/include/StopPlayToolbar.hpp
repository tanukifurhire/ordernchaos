/*************************************************************************
  \file       StopPlayToolbar.hpp
  \author	  Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       25 November 2025
  \version    1.0
  \brief
	Declares the StopPlayToolbar for GGEngine editor.
  \details
	Provides an ImGui-based toolbar to control the editor's simulation
    state (Play, Pause, Stop). Handles undo/redo shortcuts and integrates
    with editor events for starting and stopping simulation mode.
*************************************************************************/
#pragma once

#include "EditorSceneBase.hpp" // Note from Trish: getting build errors for some reason when we use <>, changed to "" instead
#include "BuildSizeAnalyzer.hpp"
#include <GGECSWorld.hpp>
#include <memory>

/*************************************************************************
  \class StopPlayToolbar
  \brief 
	Toolbar editor node for controlling simulation playback.
  \details 
	Inherits from EditorSceneBase and provides:
	- Play, Pause/Resume, Stop buttons
	- Undo/Redo keyboard shortcuts (Ctrl+Z, Ctrl+Y, Ctrl+Shift+Z)
	- Integration with editor and global event buses
*************************************************************************/
class StopPlayToolbar : public EditorSceneBase
{
public:
	/*************************************************************************
	  \brief
		Constructor for StopPlayToolbar.
	  \details
		Initializes the lambda callback handles for initialization and
		updating.
	*************************************************************************/
	StopPlayToolbar();

	/*************************************************************************
	  \brief
		Destructor for StopPlayToolbar.
	  \details
		Default destructor.
	*************************************************************************/
	~StopPlayToolbar() override = default;

	/*************************************************************************
	  \brief
		Initializes the StopPlayToolbar.
	  \details
		Registers a callback to clear the undo stack whenever the
		scene is switched.
	*************************************************************************/
	void Init() override;

	/*************************************************************************
	  \brief 
		Updates the toolbar each frame.
	  \details 
		Handles keyboard shortcuts and renders the playback controls.
	*************************************************************************/
	void Update() override;

	/*************************************************************************
	  \brief 
		Builds the docking layout for the toolbar.
	  \details 
		Empty because the toolbar does not use docking.
	*************************************************************************/
	void DockBuild() override;

	/*************************************************************************
	  \brief
		Renders Open Project, New Project buttons.
	  \details
		Buttons based on opening existing Projects or
		creating new project.
	*************************************************************************/
	void RenderProjectControls();

	/*************************************************************************
	  \brief
		Renders the Build Size Analyzer button and its window.
	  \details
		Button sits to the right of Open Project. When clicked it opens
		the BuildSizeAnalyzer window. The window itself is rendered here
		each frame so it persists as long as the toolbar updates.
	*************************************************************************/
	void RenderBuildAnalyzerButton();

	/*************************************************************************
	  \brief 
		Renders Play, Pause/Resume, and Stop buttons.
	  \details 
		Enables/disables buttons based on simulation state and
		switches between editor and game view.
	*************************************************************************/
	void RenderPlaybackControls();

	/*************************************************************************
	  \brief 
		Starts simulation mode.
	  \details 
		Emits start events and backs up all scenes.
	*************************************************************************/
	void StartSimulation();

	/*************************************************************************
	  \brief 
		Stops simulation mode.
	  \details 
		Emits stop events, restores scenes from backup, and stops
		play mode.
	*************************************************************************/
	void StopSimulation();

	/*************************************************************************
	  \brief 
		Returns whether the simulation is currently playing.
	*************************************************************************/
	bool IsPlaying() const { return isPlaying; }

	/*************************************************************************
	  \brief 
		Returns whether the simulation is currently paused.
	*************************************************************************/
	bool IsPaused() const { return isPaused; }

private:
	bool isPlaying{ false };
	bool isPaused{ false };
	bool shouldStop{ false };

	float const TOOLBAR_HEIGHT{ 50.f };
	
	float const PLAY_BUTTON_WIDTH{ 60.f };
	float const PLAY_BUTTON_HEIGHT{ 40.f };

	Callback<World&, World&> onSwitchSceneCallback;
	Callback<> onPlayModeCallback;

	Callback<> onPauseCallback;
	Callback<> onResumeCallback;
	Callback<> onExternalStopCallback;
};
/*************************************************************************
  \brief 
	Returns a singleton instance of the StopPlayToolbar.
*************************************************************************/
inline auto& GetStopPlayToolbar()
{
	static std::unique_ptr<StopPlayToolbar> toolbar = std::make_unique<StopPlayToolbar>();
	return toolbar;
}