/*************************************************************************
  \file       GameViewNode.cpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email:  a.patchamuthu@digipen.edu
  \date       7 November 2025
  \version    2.0
  \brief
    Implementation of the GameViewNode class for GGEngine editor.
  \details
    Handles rendering of the game scene into an ImGui viewport,
    camera management
*************************************************************************/
#include <GameViewNode.hpp>
#include <GGSystemManagers.hpp>
#include <imgui_internal.h>


/*************************************************************************
  \brief Constructor for GameViewNode.
  \details
    Sets up callback handles for initialization, update, and docking.
*************************************************************************/
GameViewNode::GameViewNode()
{
	initCallbackHandle = [this]() -> void { this->Init(); };
	updateCallbackHandle = [this]() -> void { this->Update(); };
	dockBuildCallbackHandle = [this]() -> void { this->DockBuild(); };
}
/*************************************************************************
  \brief Get camera from RenderManager
  \return Pointer to game camera (owned by RenderManager)
*************************************************************************/
GGGameCamera2D* GameViewNode::GetCamera()
{
    auto& renderMgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
    return renderMgr.GetGameCamera();
}

/*************************************************************************
  \brief Get camera from RenderManager (const)
  \return Const pointer to game camera
*************************************************************************/
const GGGameCamera2D* GameViewNode::GetCamera() const
{
    auto& renderMgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
    return renderMgr.GetGameCamera();
}
/*************************************************************************
  \brief Initializes the GameViewNode.
  \details
    Allocates the internal GGGameCamera2D, sets viewport size,
    and registers the camera with the editor system.
*************************************************************************/
void GameViewNode::Init()
{
    
    auto& editorInterface = Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>();
    editorInterface.RegisterGameCamera(GetCamera());


}

/*************************************************************************
  \brief Builds the docking layout for the game view window.
  \details
    Uses ImGui DockBuilder to attach "Game View" window to the editor's main dock.
*************************************************************************/
void GameViewNode::DockBuild()
{
	ImGui::DockBuilderDockWindow("Game View", Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().dockMain);
}

/*************************************************************************
  \brief Updates the game view node each frame.
  \details
    Handles:
    - Delta time calculation
    - Viewport size update
    - Camera updates and following entities
    - Input handling for camera and debug keys
    - Rendering game view and dead zone overlay
    - Syncing renderer camera
*************************************************************************/
void GameViewNode::Update()
{


    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (ImGui::Begin("Game View", nullptr, ImGuiWindowFlags_NoResize))
    {

        auto gameCamera = GetCamera();
        if (!gameCamera) {
            ImGui::Text("Error: Game camera not initialized");
            ImGui::End();
            ImGui::PopStyleVar();
            return;
        }
        // Calculate delta time
        float currentTime = static_cast<float>(glfwGetTime());
        auto& timeMgr = Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>();
        float deltaTime = static_cast<float>(timeMgr.GetDeltaTime());
        m_lastFrameTime = currentTime;
        auto& renderMgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
        if (ImGui::IsWindowFocused())
        {
            // The current window is focused
            renderMgr.currentMode = Systems::RenderManager::EditorMode::gameView;

            if (GGMeshRendererSystem::showMoveGizmo) {
                GGMeshRendererSystem::showMoveGizmo = !GGMeshRendererSystem::showMoveGizmo;
            }
        }
        // Update viewport size
        UpdateViewportSize();
        
        // Update camera
        gameCamera->Update(deltaTime);

     
       
        // Render the game view
        RenderGameView();

       
      

        // Update renderer camera
     
        //renderMgr.SetGameCamera(gameCamera);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}



/*************************************************************************
  \brief Updates the viewport size and maintains target aspect ratio.
  \details
    Adjusts camera and input manager for new viewport dimensions.
*************************************************************************/
void GameViewNode::UpdateViewportSize()
{
    auto& renderMgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
    float targetAspect = aspectRatio;//static_cast<float>(renderMgr.viewBuffer.getWidth()) / static_cast<float>(renderMgr.viewBuffer.getHeight());
    ImVec2 available = ImGui::GetContentRegionAvail();

    ImVec2 viewportSize = available;
    float currentAspect = available.x / available.y;

    if (currentAspect > targetAspect) {
        viewportSize.x = available.y * targetAspect;
    }
    else {
        viewportSize.y = available.x / targetAspect;
    }

    m_viewportSize = glm::vec2(viewportSize.x, viewportSize.y);
    auto gameCamera = GetCamera();
    // Update input manager
    auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();
    gameCamera->SetViewportSize(glm::vec2(renderMgr.viewBuffer.getWidth(), renderMgr.viewBuffer.getHeight()));
    inputMgr.UpdateViewportSize(m_viewportSize);
}

/*************************************************************************
  \brief Renders the game scene into the ImGui viewport.
  \details
    Converts framebuffer texture to ImGui image, updates input camera matrix,
    and stores rendered image position for mouse picking.
*************************************************************************/
void GameViewNode::RenderGameView()
{
    auto& renderMgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
    auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();
    auto gameCamera = GetCamera();
    // Calculate centered position for the viewport
    ImVec2 available = ImGui::GetContentRegionAvail();
    ImVec2 viewportSize(m_viewportSize.x, m_viewportSize.y);

    ImVec2 cursorPos = ImGui::GetCursorPos();
    ImVec2 offset = {
        (available.x - viewportSize.x) * 0.5f,
        (available.y - viewportSize.y) * 0.5f
    };
    ImGui::SetCursorPos({ cursorPos.x + offset.x, cursorPos.y + offset.y });

    // Update camera matrix for input manager
    glm::mat4 cameraViewProj = gameCamera->GetProjectionMatrix() *
        gameCamera->GetViewMatrix4D();
    inputMgr.UpdateCameraMatrix(cameraViewProj);

    // Render the framebuffer texture
    GLuint textureID = renderMgr.viewBuffer.GetFrameTexture();
    ImGui::Image(
        reinterpret_cast<void*>(static_cast<intptr_t>(textureID)),
        viewportSize,
        ImVec2(0, 1),   // UV flipped vertically (OpenGL convention)
        ImVec2(1, 0)
    );

   /* GLuint pickingTexture = renderMgr.pickingFramebuffer.GetTexture();
    ImGui::Image((void*)(intptr_t)pickingTexture,
        viewportSize, ImVec2(0, 1), ImVec2(1, 0));*/

    // Store image position for potential clicking/picking
    m_imagePos = ImGui::GetItemRectMin();
    
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();

    // Get window size
    int windowWidth, windowHeight;
    glfwGetWindowPos(Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetMainWindow().GetGLFWwindow(), &windowWidth, &windowHeight);

    inputMgr.SetFramebufferOffset({ min.x - static_cast<float>(windowWidth), min.y - static_cast<float>(windowHeight) });
    inputMgr.SetFramebufferScale({ max.x - min.x, max.y - min.y });
}



/*************************************************************************
  \brief Renders debug overlay in the Game View.
  \details Shows camera info, game state, and debug indicators.
*************************************************************************/
// to be implement better  in the future
void GameViewNode::RenderDebugOverlay()
{
    ImGui::SetCursorPos({ 10, 10 });
    ImGui::BeginGroup();
    auto gameCamera = GetCamera();
    ImGui::Text("Game View Debug");
    ImGui::Separator();
    ImGui::Text("Playing: %s", m_isPlaying ? "Yes" : "No");
    ImGui::Text("Paused: %s", m_isPaused ? "Yes" : "No");
    ImGui::Text("Camera Pos: (%.1f, %.1f)",
        gameCamera->GetPosition().x,
        gameCamera->GetPosition().y);
    ImGui::Text("Camera Zoom: %.2f", gameCamera->GetZoom());
    ImGui::Text("Follow Target: %u", m_followTargetID);

    ImGui::EndGroup();
}

/*************************************************************************
  \brief Resets the game state to initial conditions.
  \details
    Resets camera position, zoom, follow target, selected object, and
    internal state flags.
*************************************************************************/
void GameViewNode::ResetGameState()
{auto gameCamera = GetCamera();

    gameCamera->SetPosition(glm::vec2(0.0f));
    gameCamera->SetZoom(64.0f);
    gameCamera->ClearFollowTarget();
    m_followTargetID = NULL_ENTITY;
    m_selectedObjectID = 0;

    std::cout << "[Game] Game state reset\n";
}

/*************************************************************************
  \brief Converts screen coordinates to world coordinates.
  \param[in] screenPos  Position in screen space
  \return glm::vec2 representing world coordinates
*************************************************************************/
glm::vec2 GameViewNode::ScreenToWorld(glm::vec2 screenPos)
{
    auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();
    return inputMgr.ScreenToWorld2D(screenPos);
}

