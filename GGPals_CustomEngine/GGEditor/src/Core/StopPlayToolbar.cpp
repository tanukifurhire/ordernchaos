/*************************************************************************
  \file       StopPlayToolbar.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       25 November 2025
  \version    1.0
  \brief
	Implements the StopPlayToolbar for GGEngine editor.
  \details
	Provides an ImGui-based toolbar for controlling the editor's simulation
	state (Play, Pause, Stop). Handles undo/redo keyboard shortcuts and
	manages simulation start/stop events.
*************************************************************************/
#include <StopPlayToolbar.hpp>
#include <EditorEventBus.hpp>
#include <EditorUndoStack.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <GGEngine.hpp>
#include <GGSceneManager.hpp>
#include <GGEngineEventBus.hpp>
#include <GGSystemManagers.hpp>
#include <EditorUtils.hpp>
#include <GGJSONConfig.hpp>
#include <AssetManagerNode.hpp>
#include <filesystem>
#include <vector>

/*************************************************************************
  \brief 
	Constructor for StopPlayToolbar.
  \details 
	Initializes the lambda callback handles for initialization and
	updating.
*************************************************************************/
StopPlayToolbar::StopPlayToolbar()
{
	initCallbackHandle = [this]() { this->Init(); };
	updateCallbackHandle = [this]() { this->Update(); };
}

/*************************************************************************
  \brief 
	Initializes the StopPlayToolbar.
  \details 
	Registers a callback to clear the undo stack whenever the
		   scene is switched.
*************************************************************************/
void StopPlayToolbar::Init()
{
	onSwitchSceneCallback =
		[](World&, World&)
		{
			GetUndoStack()->Clear();
		};

	onPlayModeCallback =
		[]()
		{
			GetUndoStack()->Clear();
		};

	onPauseCallback =
		[&]()
		{
			isPaused = true;
		};

	onResumeCallback =
		[&]()
		{
			isPaused = false;
		};

	onExternalStopCallback =
		[&]()
		{
			shouldStop = true;
		};

	GlobalEventBus::onSwitchScenes += onSwitchSceneCallback;
	GlobalEventBus::onPlayModeStart += onPlayModeCallback;
	GlobalEventBus::shutdownEvent += onExternalStopCallback;
}

/*************************************************************************
  \brief 
	Updates the toolbar each frame.
  \details
	- Handles Ctrl+Z for Undo, Ctrl+Y or Ctrl+Shift+Z for Redo.
	- Renders the toolbar and playback buttons (Play, Pause/Resume, Stop)
	  using ImGui.
*************************************************************************/
void StopPlayToolbar::Update()
{
	auto& inputManager = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();
	
	// Ctrl+Z = Undo
	if (inputManager.GGInputCheckCurr(GGKey_LEFT_CONTROL)
		&& inputManager.GGInputCheckTriggered(GGKey_Z)
		&& !inputManager.GGInputCheckCurr(GGKey_LEFT_SHIFT))  // Ensure Shift is NOT pressed
	{
		if (!isPlaying && GetUndoStack()->CanUndo())
		{
			GetUndoStack()->Undo(GetEngineInstance().GetSceneManager().GetActiveScene());
		}
	}
	// Ctrl+Y = Redo (primary)
	else if (inputManager.GGInputCheckCurr(GGKey_LEFT_CONTROL)
		&& inputManager.GGInputCheckTriggered(GGKey_Y))
	{
		if (!isPlaying && GetUndoStack()->CanRedo())
		{
			GetUndoStack()->Redo(GetEngineInstance().GetSceneManager().GetActiveScene());
		}
	}
	// Ctrl+Shift+Z = Redo (alternative)
	else if (inputManager.GGInputCheckCurr(GGKey_LEFT_CONTROL)
		&& inputManager.GGInputCheckCurr(GGKey_LEFT_SHIFT)
		&& inputManager.GGInputCheckTriggered(GGKey_Z))
	{
		if (!isPlaying && GetUndoStack()->CanRedo())
		{
			GetUndoStack()->Redo(GetEngineInstance().GetSceneManager().GetActiveScene());
		}
	}

	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, TOOLBAR_HEIGHT));
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

	ImGui::Begin("##Toolbar", nullptr, flags);

	RenderProjectControls();
	ImGui::SameLine();
	RenderBuildAnalyzerButton();
	ImGui::SameLine();
	RenderPlaybackControls();


	ImGui::End();

	ImGui::PopStyleVar(2);
}

/*************************************************************************
  \brief 
	Builds docking layout for toolbar.
  \details 
	Empty as the toolbar does not use docking.
*************************************************************************/
void StopPlayToolbar::DockBuild()
{
	// Empty on purpose
}

/*************************************************************************
  \brief
	Renders Open Project, New Project buttons.
  \details
	Buttons based on opening existing Projects or
	creating new project.
*************************************************************************/
void StopPlayToolbar::RenderProjectControls()
{
	static bool showNewProjectPopup = false;
	static bool showOpenProjectPopup = false;
	static bool showConfirmPopup = false;
	static bool pendingIsNewProject = false;
	static std::string pendingPath = "";
	static char newProjectNameBuf[128] = "NewProject";

	if (showConfirmPopup)
	{
		ImGui::OpenPopup("Confirm##switchproject");
		showConfirmPopup = false;
	}

	if (ImGui::Button("New Project", ImVec2(2*PLAY_BUTTON_WIDTH, PLAY_BUTTON_HEIGHT)))
	{
		showNewProjectPopup = true;
		ImGui::OpenPopup("New Project##popup"); //## suffix makes ID unique
	}

	ImGui::SetNextWindowSize(ImVec2(400, 160), ImGuiCond_Always);
	if (ImGui::BeginPopupModal("New Project##popup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::Text("Project name: ");
		ImGui::SetNextItemWidth(-1.f);
		ImGui::InputText("##newprojname", newProjectNameBuf, sizeof(newProjectNameBuf));

		if (ImGui::Button("Create", ImVec2(120, 0)))
		{
			std::string projectName = newProjectNameBuf;
			if (!projectName.empty())
			{
				std::filesystem::path ggProjectsRoot = std::filesystem::weakly_canonical(std::filesystem::current_path() / "../../../GGProjects");
				std::filesystem::path assetsRoot = ggProjectsRoot / projectName / "Assets";

				const std::vector<std::string> subFolders =
				{
					"Textures", "Shaders", "Scripts", "Scenes", "Prefabs", "Maps", "Fonts", "Audio", "Animations"
				};

				std::filesystem::create_directories(assetsRoot);
				for (const auto& sub : subFolders)
					std::filesystem::create_directories(assetsRoot / sub);

				pendingPath = assetsRoot.string();
				pendingIsNewProject = true;
				showNewProjectPopup = false;
				ImGui::CloseCurrentPopup();
				showConfirmPopup = true;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0))) { showNewProjectPopup = false; ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}
	ImGui::SameLine();

	if (ImGui::Button("Open Project", ImVec2(2*PLAY_BUTTON_WIDTH, PLAY_BUTTON_HEIGHT)))
	{
		showOpenProjectPopup = true;
		ImGui::OpenPopup("OpenProject##popup");
	}

	ImGui::SetNextWindowSize(ImVec2(420, 360), ImGuiCond_Always);
	if (ImGui::BeginPopupModal("OpenProject##popup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::Text("Select a project from GGProjects: "); ImGui::Separator();

		std::filesystem::path ggProjectsRoot = std::filesystem::weakly_canonical(std::filesystem::current_path() / "../../../GGProjects");

		static int selectedProject = -1;
		static std::vector<std::string> projectNames;

		if (ImGui::IsWindowAppearing())
		{
			selectedProject = -1;
			projectNames.clear();
			if (std::filesystem::exists(ggProjectsRoot) && std::filesystem::is_directory(ggProjectsRoot))
			{
				for (const auto& entry : std::filesystem::directory_iterator(ggProjectsRoot))
				{
					if (entry.is_directory() && std::filesystem::exists(entry.path() / "Assets"))
						projectNames.push_back(entry.path().filename().string());
				}
			}
		}

		ImGui::BeginChild("##projlist", ImVec2(0, 250), true);
		if (projectNames.empty()) 
		{ 
			ImGui::TextDisabled("No projects found in GGProjects/"); 
		}
		
		else
		{
			for (int i = 0; i < (int)projectNames.size(); ++i)
			{
				bool sel = (selectedProject == i);
				if (ImGui::Selectable(projectNames[i].c_str(), sel)) selectedProject = i;
			}
		}
		ImGui::EndChild();

		ImGui::Spacing(); 
		ImGui::BeginDisabled(selectedProject < 0);
		if (ImGui::Button("Open", ImVec2(120, 0)))
		{
			std::filesystem::path assetsPath = ggProjectsRoot / projectNames[selectedProject] / "Assets";
			pendingPath = assetsPath.string();
			pendingIsNewProject = false;
			showOpenProjectPopup = false;
			ImGui::CloseCurrentPopup();
			showConfirmPopup = true;
		}

		ImGui::EndDisabled(); 
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			showOpenProjectPopup = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	// Confirmation popup
	ImGui::SetNextWindowSize(ImVec2(380, 130), ImGuiCond_Always);
	if (ImGui::BeginPopupModal("Confirm##switchproject", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
	{
		ImGui::TextWrapped("Opening a new project will clear the current scene.");
		ImGui::TextWrapped("Do you want to save before continuing?");
		ImGui::Spacing(); 
		ImGui::Separator(); 
		ImGui::Spacing();

		if (ImGui::Button("Save & Open", ImVec2(120, 0)))
		{
			// Save current scene first
			auto& sceneManager = GGEngine::GetEngine().GetSceneManager();
			const std::string& sceneKey = sceneManager.GetActiveSceneKey();
			if (!sceneKey.empty() && sceneKey != "UnknownScene")
				SceneSerializer::SaveCurrentScene(sceneKey, sceneKey, sceneManager);

			GetAssetManagerNode()->LoadProject(pendingPath);
			auto& engine = GGEngine::GetEngine();
			auto& prefabManager = engine.GetPrefabManager();

			std::filesystem::path scenesDir = std::filesystem::path(pendingPath) / "Scenes";

			for (auto& entry : std::filesystem::directory_iterator(scenesDir))
			{
				if (entry.path().extension() == ".scene")
				{
					sceneManager.AddNewScene(entry.path().string(), prefabManager);
				}
			}
			showConfirmPopup = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Open", ImVec2(120, 0)))
		{
			GetAssetManagerNode()->LoadProject(pendingPath);
			auto& engine = GGEngine::GetEngine();
			auto& sceneManager = engine.GetSceneManager();
			auto& prefabManager = engine.GetPrefabManager();

			std::filesystem::path scenesDir = std::filesystem::path(pendingPath) / "Scenes";

			for (auto& entry : std::filesystem::directory_iterator(scenesDir))
			{
				if (entry.path().extension() == ".scene")
				{
					sceneManager.AddNewScene(entry.path().string(), prefabManager);
				}
			}
			showConfirmPopup = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			showConfirmPopup = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

/*************************************************************************
  \brief
	Renders the Build Size Analyzer toolbar button and delegates to
	the BuildSizeAnalyzer window each frame.
  \details
	- The button toggles BuildSizeAnalyzer::isOpen.
	- When a project is newly loaded the analyzer is marked dirty so it
	  rescans on next open.
	- BuildSizeAnalyzer::Render() is called unconditionally so the window
	  keeps updating even when the toolbar is re-entered.
*************************************************************************/
void StopPlayToolbar::RenderBuildAnalyzerButton()
{
	auto& analyzer = GetBuildSizeAnalyzer();

	if (ImGui::Button("Build Analyzer", ImVec2(2 * PLAY_BUTTON_WIDTH, PLAY_BUTTON_HEIGHT)))
	{
		analyzer->isOpen = !analyzer->isOpen;
		if (analyzer->isOpen)
			analyzer->MarkDirty(); // always refresh when the window is opened
	}

	// Render the window each frame (Render() is a no-op when isOpen == false)
	analyzer->Render();
}

/*************************************************************************
  \brief 
	Renders Play, Pause/Resume, and Stop buttons.
  \details 
	Handles enabling/disabling buttons based on simulation state,
	switching between editor and game view, and controlling
	simulation playback.
*************************************************************************/
void StopPlayToolbar::RenderPlaybackControls()
{
	float spacingFromProjectControls = 0.35f;
	float remainingWidth = ImGui::GetContentRegionAvail().x;
	float playbackWidth = (3 * PLAY_BUTTON_WIDTH) + (2 * ImGui::GetStyle().ItemSpacing.x);
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + spacingFromProjectControls * (remainingWidth - playbackWidth));

	// play button
	ImGui::BeginDisabled(isPlaying);
	if (ImGui::Button("Play", ImVec2(PLAY_BUTTON_WIDTH, PLAY_BUTTON_HEIGHT))) 
	{
		isPlaying = true;
		auto& renderMgr = Systems::GetSystemsRegistry()
			.GetSystem<Systems::RenderManager>();
		if (renderMgr.currentMode != Systems::RenderManager::EditorMode::gameView) {
			renderMgr.currentMode = Systems::RenderManager::EditorMode::gameView;
			
		}
		ImGui::SetWindowFocus("Game View");

		StartSimulation();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();

	// pause button
	ImGui::BeginDisabled(!isPlaying);

	if (ImGui::Button(isPaused ? "Resume" : "Pause", ImVec2(PLAY_BUTTON_WIDTH, PLAY_BUTTON_HEIGHT)))
	{
		if (isPaused)
		{
			GlobalEventBus::onResume.Emit();
			Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>().SetTimeScale(1.f); // pause animations
			isPaused = false;
		}
		else
		{
			GlobalEventBus::onPause.Emit();
			Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>().SetTimeScale(0.f); // pause animations
			isPaused = true;
		}
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	
	// stop button
	ImGui::BeginDisabled(!isPlaying);
	if (ImGui::Button("Stop", ImVec2(PLAY_BUTTON_WIDTH, PLAY_BUTTON_HEIGHT)))
	{
		isPlaying = false;
		auto& renderMgr = Systems::GetSystemsRegistry()
			.GetSystem<Systems::RenderManager>();

		if (renderMgr.currentMode == Systems::RenderManager::EditorMode::gameView) {
			renderMgr.currentMode = Systems::RenderManager::EditorMode::editorView;
			
		}
		ImGui::SetWindowFocus("Editor View");

		StopSimulation();
	}
	ImGui::EndDisabled();
	if (isPlaying == false) {
		isPlaying = false;
		auto& renderMgr = Systems::GetSystemsRegistry()
			.GetSystem<Systems::RenderManager>();

		if (renderMgr.currentMode != Systems::RenderManager::EditorMode::editorView) {
			renderMgr.currentMode = Systems::RenderManager::EditorMode::editorView;

		}
	}

	if (isPlaying && shouldStop)
	{
		isPlaying = false;
		shouldStop = false;
		auto& renderMgr = Systems::GetSystemsRegistry()
			.GetSystem<Systems::RenderManager>();
		if (renderMgr.currentMode == Systems::RenderManager::EditorMode::gameView) {
			renderMgr.currentMode = Systems::RenderManager::EditorMode::editorView;

		}
		ImGui::SetWindowFocus("Editor View");
		StopSimulation();
	}
}

/*************************************************************************
  \brief 
	Starts simulation mode.
  \details 
	Emits start events, backs up scenes, and triggers global
	play mode start events.
*************************************************************************/
void StopPlayToolbar::StartSimulation()
{
	auto& sceneManager = GGEngine::GetEngine().GetSceneManager();

	EditorEventBus::onStartSimulation.Emit();
	sceneManager.BackupAllScenes();

	GlobalEventBus::onPlayModeStart.Emit();
}

/*************************************************************************
  \brief 
	Stops simulation mode.
  \details 
	Emits stop events, restores scenes from backup, stops play mode,
	and triggers global stop play mode events.
*************************************************************************/
void StopPlayToolbar::StopSimulation()
{
	auto& prefabManager = GGEngine::GetEngine().GetPrefabManager();
	auto& sceneManager = GGEngine::GetEngine().GetSceneManager();

	EditorEventBus::onStopSimulation.Emit();
	GGEngine::GetEngine().GetActiveWorld().StopPlayMode();
	GlobalEventBus::onStopPlayMode.Emit();

	sceneManager.RestoreAllScenes(prefabManager);

	// reset audio config properties (only persist per session)
	using namespace GGEngineConfig::GGEngineAudio;

	SetMasterVolume(1.0);
	SetBGMVolume(1.0);
	SetSFXVolume(1.0);
	SaveAudioSettings();
}