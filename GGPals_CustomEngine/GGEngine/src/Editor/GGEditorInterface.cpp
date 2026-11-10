/*************************************************************************
  \file       GGEditorInterface.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       3 October 2025
  \version    1.0
  \brief
  \dependencies
  \see
	GGEvent, SystemInterface
  \history
	- 1.0 (3 October 2025): Initial implementation
  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGEditorInterface.hpp>
#include <GGEngine.hpp>
#include <GGSystemManagers.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <GGEngineEventBus.hpp>
#include <GGInputManager.hpp>
#include <../../GGEditor/include/EditorUtils.hpp>
#include <../../GGEditor/include/StopPlayToolbar.hpp>

/*************************************************************************
  \brief
	Initializes the GGEditorInterface and sets up ImGui.
  \return
	True if initialization succeeds.
  \details
	- Creates the ImGui context and configures docking and viewport support.
	- Loads a custom font for the editor UI.
	- Sets up the ImGui style using SetupImGuiStyle().
	- Initializes ImGui backends for GLFW and OpenGL.
	- Registers callbacks for scene modification, input save, play/stop mode.
*************************************************************************/
bool GGEditorInterface::Init()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	customFont = io.Fonts->AddFontFromFileTTF("../../../EngineAssets/Fonts/SourceSans3-Regular.ttf", 20.f);

	SetupImGuiStyle();

	ImGui_ImplGlfw_InitForOpenGL(Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetMainWindow().GetGLFWwindow(), true);
	ImGui_ImplOpenGL3_Init("#version 450");

	onInitEventHandler.Emit();

	onSceneModifiedCallback =
		[&]()
		{
			if (!GetStopPlayToolbar()->IsPlaying()) {
				auto& sceneManager = GGEngine::GetEngine().GetSceneManager();
				const std::string& sceneKey = sceneManager.GetActiveSceneKey();
				EditorSceneUtils::MarkSceneDirty(sceneKey);
			}
		};

	GlobalEventBus::onSceneModified += onSceneModifiedCallback;

	checkSaveInputCallback =
		[&]()
		{
			CheckSaveInput();
		};

	GlobalEventBus::onInputSave += checkSaveInputCallback;

	onPlayCallback =
		[&]()
		{
			GlobalEventBus::onInputSave -= checkSaveInputCallback;
		};

	onStopCallback =
		[&]()
		{
			GlobalEventBus::onInputSave += checkSaveInputCallback;
		};

	GlobalEventBus::onPlayModeStart += onPlayCallback;
	GlobalEventBus::onStopPlayMode += onStopCallback;

	return true;
}

/*************************************************************************
  \brief
	Main update loop for the editor UI.
  \details
	- Starts a new ImGui frame for GLFW/OpenGL.
	- Pushes the custom font and creates the main docking space.
	- Sets up docking layout (left, right, bottom) on first frame.
	- Emits the onUpdateEventHandler for custom editor updates.
	- Handles Ctrl+S input to trigger save events.
	- Renders toast notifications and ImGui draw data.
	- Updates platform windows when multiple viewports are enabled.
*************************************************************************/
void GGEditorInterface::Update()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();

	ImGui::PushFont(customFont);

	static bool firstUpdate = true;
	static ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	
	// 50.f hardcoded for toolbar for now.
	ImVec2 dockPos = ImVec2(viewport->Pos.x, viewport->Pos.y + 50.f);
	ImVec2 dockSize = ImVec2(viewport->Size.x, viewport->Size.y - 50.f);

	ImGui::SetNextWindowPos(dockPos);
	ImGui::SetNextWindowSize(dockSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
	windowFlags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	if (dockspaceFlags & ImGuiDockNodeFlags_PassthruCentralNode)
		windowFlags |= ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("GGEditor_DockSpace", nullptr, windowFlags);
	ImGui::PopStyleVar(3);

	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		dockMain = ImGui::GetID("GGEditorDockspace");
		ImGui::DockSpace(dockMain, ImVec2(0.0f, 0.0f), dockspaceFlags);

		// Docking setup (only runs on first update)
		if (firstUpdate)
		{
			// Remove any existing node and add a new dockspace node
			ImGui::DockBuilderRemoveNode(dockMain);
			ImGui::DockBuilderAddNode(dockMain, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockMain, ImGui::GetMainViewport()->WorkSize);

			// Split dockspace into left, right, and bottom regions
			dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.2f, nullptr, &dockMain);
			dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.25f, nullptr, &dockMain);
			dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.25f, nullptr, &dockMain);

			onDockBuildEventHandler.Emit(); // Signal that dock building is finished

			ImGui::DockBuilderFinish(dockMain);

			firstUpdate = false;
		}
	}

	onUpdateEventHandler.Emit();

	auto& inputManager = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();

	// Input handling: Ctrl+S triggers a save event
	if (inputManager.GGInputCheckCurr(GGKey_LEFT_CONTROL)
		&& inputManager.GGInputCheckTriggered(GGKey_S))
	{
		GlobalEventBus::onInputSave.Emit();
	}

	ImGui::PopFont();

	ImGui::End();

	// draw toast notifications
	EditorGeneralUtils::RenderToasts();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		GLFWwindow* backupCurrentContext = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backupCurrentContext);
	}
}

/*************************************************************************
  \brief
	Checks whether the current scene or prefab should be saved.
  \details
	- Handles saving for "PrefabEditorWorld" by saving the currently edited prefab.
	- Handles saving for "UnknownScene" by triggering a "Save Scene As" popup.
	- For other scenes, saves the scene immediately and shows a toast notification.
*************************************************************************/
void GGEditorInterface::CheckSaveInput()
{
	auto& sceneManager = GGEngine::GetEngine().GetSceneManager();
	auto& prefabManager = GGEngine::GetEngine().GetPrefabManager();

	if (sceneManager.GetActiveSceneKey() == "PrefabEditorWorld")
	{
		if (sceneManager.GetActiveScene().GetLivingEntitiesCount() > 0)
		{
			auto& world = sceneManager.GetActiveScene();
			bool success = prefabManager.SavePrefab(
				*sceneManager.GetActiveScene().GetLivingEntities().begin(),
				prefabManager.GetCurrentlyEditedPrefabName(),
				world);
			if (success) {
				sceneManager.SyncPrefabInstances(prefabManager.GetCurrentlyEditedPrefabName(), prefabManager);
			}
		}
	}
	if (sceneManager.GetActiveSceneKey() == "UnknownScene")
	{
		// trigger the "Save Scene" popup using the shared state
		const std::string& sceneName = sceneManager.GetActiveSceneKey();
		auto& state = EditorSceneUtils::GetSceneSaveState(sceneName);

		// suggest a free name like "MyScene", "MyScene2", ...
		std::string suggestion = EditorSceneUtils::GetNextAvailableSceneName("MyScene");

		std::strncpy(state.newSceneNameBuf, suggestion.c_str(),
			sizeof(state.newSceneNameBuf) - 1);
		state.newSceneNameBuf[sizeof(state.newSceneNameBuf) - 1] = '\0';

		state.openSaveAsPopup = true;   // next frame, DrawSceneSavePopup will open the modal
	}
	else
	{
		bool success = SceneSerializer::SaveCurrentScene(sceneManager.GetActiveSceneKey(), sceneManager.GetActiveSceneKey(), sceneManager);
		if (success) {
			const std::string& sceneName = sceneManager.GetActiveSceneKey();
			EditorSceneUtils::GetSceneSaveState(sceneName).isDirty = false;

			// show toast notification
			EditorGeneralUtils::ShowToast("Scene \"" + sceneName + "\" saved!");
		}
	}
}

/*************************************************************************
  \brief
	Sets up a custom ImGui style for the editor.
  \details
	- Forked from Moonlight style in ImThemes.
	- Configures colors, paddings, rounding, and sizes for windows, frames, tabs, buttons, etc.
*************************************************************************/
void GGEditorInterface::SetupImGuiStyle()
{
	// Fork of Moonlight style from ImThemes
	ImGuiStyle& style = ImGui::GetStyle();

	style.Alpha = 1.0f;
	style.DisabledAlpha = 1.0f;
	style.WindowPadding = ImVec2(12.0f, 12.0f);
	style.WindowRounding = 0.0f;
	style.WindowBorderSize = 0.0f;
	style.WindowMinSize = ImVec2(20.0f, 20.0f);
	style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
	style.WindowMenuButtonPosition = ImGuiDir_Right;
	style.ChildRounding = 0.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupRounding = 3.1f;
	style.PopupBorderSize = 1.0f;
	style.FramePadding = ImVec2(4.f, 3.4f);
	style.FrameRounding = 3.1f;
	style.FrameBorderSize = 0.0f;
	style.ItemSpacing = ImVec2(6.3f, 5.9f);
	style.ItemInnerSpacing = ImVec2(7.1f, 1.8f);
	style.CellPadding = ImVec2(12.5f, 9.1f);
	style.IndentSpacing = 3.3f;
	style.ColumnsMinSpacing = 4.9f;
	style.ScrollbarSize = 12.4f;
	style.ScrollbarRounding = 15.9f;
	style.GrabMinSize = 11.0f;
	style.GrabRounding = 2.7f;
	style.TabRounding = 2.4f;
	style.TabBorderSize = 0.0f;
	style.ColorButtonPosition = ImGuiDir_Right;
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

	style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.27450982f, 0.31764707f, 0.4509804f, 1.0f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.09356401f, 0.11312355f, 0.15294118f, 1.0f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.06813535f, 0.08405019f, 0.11587983f, 1.0f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
	style.Colors[ImGuiCol_Border] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
	style.Colors[ImGuiCol_FrameBg] = ImVec4(0.18257841f, 0.19992094f, 0.24034333f, 1.0f);
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.14710161f, 0.19250639f, 0.28326178f, 1.0f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.17987068f, 0.27020168f, 0.45064378f, 1.0f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.0648366f, 0.08352479f, 0.12156863f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.15686275f, 0.16862746f, 0.19215687f, 1.0f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
	style.Colors[ImGuiCol_CheckMark] = ImVec4(0.49803925f, 0.7220904f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.49803925f, 0.73501647f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.6993682f, 0.9037694f, 0.96995705f, 1.0f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.16692042f, 0.18907876f, 0.21176471f, 1.0f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.2746636f, 0.2862139f, 0.29803923f, 1.0f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.15294118f, 0.15294118f, 0.15294118f, 1.0f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.16733563f, 0.2702137f, 0.45882353f, 1.0f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.120276816f, 0.19608763f, 0.30980393f, 1.0f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
	style.Colors[ImGuiCol_Separator] = ImVec4(0.12941177f, 0.14901961f, 0.19215687f, 1.0f);
	style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.15686275f, 0.18431373f, 0.2509804f, 1.0f);
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.15686275f, 0.18431373f, 0.2509804f, 1.0f);
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.14509659f, 0.14509758f, 0.14509805f, 1.0f);
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.49803925f, 0.69623834f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.0f, 0.99999f, 0.99999f, 1.0f);
	style.Colors[ImGuiCol_Tab] = ImVec4(0.14345251f, 0.22885533f, 0.34509805f, 1.0f);
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.24135332f, 0.35343942f, 0.52156866f, 1.0f);
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.17516339f, 0.37176463f, 0.6666667f, 1.0f);
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.078431375f, 0.08627451f, 0.101960786f, 1.0f);
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.1254902f, 0.27450982f, 0.57254905f, 1.0f);
	style.Colors[ImGuiCol_PlotLines] = ImVec4(0.52156866f, 0.6f, 0.7019608f, 1.0f);
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.039215688f, 0.98039216f, 0.98039216f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.2366782f, 0.33258703f, 0.37254903f, 1.0f);
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.95686275f, 0.95686275f, 0.95686275f, 1.0f);
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.047058824f, 0.05490196f, 0.07058824f, 1.0f);
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.11764706f, 0.13333334f, 0.14901961f, 1.0f);
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.09803922f, 0.105882354f, 0.12156863f, 1.0f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.9372549f, 0.9372549f, 0.9372549f, 1.0f);
	style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.49803925f, 0.99353695f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.26666665f, 0.93390566f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.49803925f, 0.9030549f, 1.0f, 1.0f);
	style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.17647058f, 0.32202303f, 0.54509807f, 0.5019608f);
	style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.17647058f, 0.3315157f, 0.54509807f, 0.5019608f);
}