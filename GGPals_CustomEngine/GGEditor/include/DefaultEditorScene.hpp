/*************************************************************************
  \file       DefaultEditorScene.hpp
  \author     Tai Xiao Xuan, Zafir Haqqym
  \par        Email: xiaoxuan.tai@digipen.edu, zafir.h@digipen.edu
  \date       1 October 2025
  \version    1.3
  \contribute Zafir(50%), Xiao Xuan(50%)
  \brief
    Defines the default editor scene implementation for GGEngine.
  \details
    The DefaultEditorScene class provides a concrete implementation of
    EditorSceneBase, serving as the primary editor interface for the
    engine. It integrates Dear ImGui for immediate-mode GUI rendering
    and provides development tools including physics simulation controls,
    performance monitoring, and visual debugging utilities. The scene
    follows the Singleton pattern through the GetDefaultEditorScene()
    accessor function.
  \dependencies
    EditorSceneBase.hpp, <memory> (for std::unique_ptr), <vector>, <string>,
    GGLogger.hpp, <glm/glm.hpp>
  \see
    EditorSceneBase, Systems::EditorManager, Systems::SimulationManager,
    GGAssetManager, GGLogger
  \history
    - 1.0 (26 September 2025): Initial implementation with ImGui integration.
    - 1.1 (27 September 2025): Added Dev Tools window with tabbed interface,
      functional console integration using GGLogger, and physics controls
      with manual stepping capabilities.
    - 1.2 (30 September 2025): Added Frame Rate tab with configurable frame
      cap slider. Frame limiting always active for consistent network timing.
    - 1.3 (1 October 2025): Added Font Test tab with FontAtlas testing
      capabilities including static text, dynamic FPS counter, and interactive
      custom text rendering with real-time position and color controls.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <EditorSceneBase.hpp>
#include <memory>
#include <vector>
#include <string>
#include <GGLogger.hpp>
#include <glm/glm.hpp>
#include <GGPrefabManager.hpp>
#include <GGObjectsFactory.hpp>
#include <GGSceneManager.hpp>
#include <GGECSInterface.hpp>
#include <AssetManagerNode.hpp>

/*************************************************************************
  \brief
    Default editor scene implementation providing development tools interface
  \details
    Concrete implementation of EditorSceneBase that serves as the primary
    development environment for GGEngine. Provides immediate-mode GUI
    interface using Dear ImGui, including physics simulation controls,
    performance metrics, and debugging utilities. The scene automatically
    configures its callback handles during construction for seamless
    integration with the EditorManager system.
  \note
    This scene is designed to remain active throughout the editor session
    and provides persistent development tools and debugging interfaces.
  \par Performance
    UI rendering occurs every frame. Consider frame rate impact when
    adding complex debugging visualizations.
*************************************************************************/
class DefaultEditorScene : public EditorSceneBase 
{
public:
    /*************************************************************************
      \brief
        Constructor initializing callback system integration
      \details
        Configures the initCallbackHandle and updateCallbackHandle to
        properly dispatch to this scene's Init() and Update() methods.
        Enables automatic registration with the EditorManager event system.
    *************************************************************************/
	DefaultEditorScene();


    /*************************************************************************
      \brief
        Scene initialization implementation
      \details
        Performs one-time setup for the default editor scene. Currently
        minimal but provides extension point for future initialization
        requirements such as resource loading or system configuration.
      \note
        Called automatically through the callback system during engine
        initialization phase.
    *************************************************************************/
	virtual void Init() override;

    /*************************************************************************
      \brief
        Dock build implementation
      \details
        Performs one-time setup for the dock building for the dockable interface
        of ImGui.
      \note
        Called automatically through the callback system during the first update
        frame.
    *************************************************************************/
    virtual void DockBuild() override;

    /*************************************************************************
      \brief
        Per-frame update and UI rendering implementation
      \details
        Renders the Dear ImGui interface including development tools such as:
        - Physics simulation step controls
        - Performance metrics display
        - Color picker for clear color configuration
        - Frame rate monitoring

        Handles the complete ImGui frame lifecycle from NewFrame() to
        final render data submission.
      \note
        Called every frame through the callback system during the engine's
        update cycle.
    *************************************************************************/
	virtual void Update() override;

    struct TextRenderData
    {
        std::string text;
        float x, y, scale;
        glm::vec3 color;
    };

    bool ShouldRenderTestText() const { return renderTestText; }
    bool ShouldRenderFPS() const { return showFPSCounter; }
    TextRenderData GetTestTextData() const
    {
        return { testText, testTextX, testTextY, testTextScale, testTextColor };
    }

private:
    // UI state variables - persist between frames
    bool showResourceBrowser = true;
    bool showAudioControls = true;
    bool showConsole = true;
    bool showPhysicsControls = true;

    // Event listener for audio asset selection
    Callback<uint32_t, std::string> audioAssetSelectedCallback;

    // Resource browser state
    int selectedTextureIndex = -1;
    int selectedAudioIndex = -1;
    int selectedShaderIndex = -1;
    std::vector<std::string> cachedTexturePaths;
    std::vector<std::string> cachedAudioPaths;
    std::vector<std::string> cachedShaderPaths;
    bool needsAssetRefresh = true;

    // Audio control state
    std::string selectedAudioPath = "";
    float masterVolume = 1.0f;

    // Console state
    bool autoScrollConsole = true;

    // Asset discovery
    std::vector<std::string> discoveredAudioPaths;

    // Performance profiler
    bool sortByTime = true;
    void RenderPerformanceProfiler();

    // Entity spawning
    int selectedSpawnPrefabIndex = -1;
    int selectedDestroyPrefabIndex = -1;
    bool prefabsLoaded = false;

    // Scene Management
    std::string selectedSceneKey = "";
    bool scenesLoaded = false;

    // Font test state
    std::string testText = "Hello World!";
    float testTextScale = 1.0f;
    glm::vec3 testTextColor = glm::vec3(1.0f, 1.0f, 1.0f);
    float testTextX = 0.0f;
    float testTextY = 0.0f;
    bool renderTestText = false;
    bool showFPSCounter = false;

    // Helper methods
    void RefreshAssetLists();
    void RenderResourceBrowser();
    void RenderAudioControls();
    void RenderConsole();
    void RenderPhysicsControls();  
    void RenderFrameRateSettings();
    void RenderEntitySpawningControls();
    void RenderPrefabEditingControls();
    void RenderSceneManagementControls();
    void RenderInstancingControls();
    void RenderFontTestControls();
    void RenderFontTestOverlays();
    void RenderNetworkingTestControls();
    void RenderCrashLogging();
    void BackgroundSpriteScene();

    // temperory for testing picking
    void PickingFrameBuffer();
    void CameraSettings();
    void GizmoSettings();
};

/*************************************************************************
  \brief
    Singleton accessor for the default editor scene instance
  \return
    Reference to std::unique_ptr containing the singleton DefaultEditorScene
  \details
    Provides global access to the default editor scene using the Singleton
    pattern. The instance is created on first access and persists for the
    application lifetime. Returns a reference to the unique_ptr to enable
    both pointer semantics and automatic lifetime management.
  \note
    Thread-safe in C++11 and later due to static local initialization
    guarantees. The unique_ptr ensures proper cleanup during application
    shutdown.
  \par Usage
  \code
    // Access the default editor scene
    auto& scene = GetDefaultEditorScene();

    // Register with EditorManager
    editorManager.onInitEventHandler += scene->initCallbackHandle;
  \endcode
*************************************************************************/
inline auto& GetDefaultEditorScene() 
{
	static std::unique_ptr<DefaultEditorScene> defaultEditor = std::make_unique<DefaultEditorScene>();
	return defaultEditor;
}