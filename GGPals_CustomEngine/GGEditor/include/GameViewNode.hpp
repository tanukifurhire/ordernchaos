/*************************************************************************
  \file       GameViewNode.hpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email:  a.patchamuthu@digipen.edu
  \date       7 November 2025
  \version    1.0
  \brief
	Editor game view node for GGEngine.
  \details
	Handles rendering the game scene into an ImGui viewport inside the
	editor, including camera management, dead zone visualization,
	input handling, and debug overlays.
*************************************************************************/
#pragma once

#include <EditorSceneBase.hpp>
#include <memory>
#include <GGECSInterface.hpp>
#include <GGGameCamera2D.hpp>
#include <EditorViewNode.hpp>

/*************************************************************************
  \class      GameViewNode
  \brief      Editor scene node representing the in-editor game view.
  \details
	Provides a renderable viewport for the game scene, manages a
	GGGameCamera2D, handles camera controls, game state (play/pause),
	dead zone visualization, and debug overlays.
*************************************************************************/

class GameViewNode : public EditorSceneBase
{
public:
	/*************************************************************************
	  \brief Constructor for GameViewNode.
	  \details
		Sets up callback handles for initialization, update, and docking.
	*************************************************************************/
	GameViewNode();
	virtual ~GameViewNode() = default;

	/*************************************************************************
	  \brief Initializes the GameViewNode.
	  \details
		Allocates the internal GGGameCamera2D, sets viewport size,
		and registers the camera with the editor system.
	*************************************************************************/
	virtual void Init() override;

	/*************************************************************************
	  \brief Builds the docking layout for the game view window.
	  \details
		Uses ImGui DockBuilder to attach "Game View" window to the editor's main dock.
	*************************************************************************/
	virtual void DockBuild() override;

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
	virtual void Update() override;

	/*************************************************************************
	  \brief Get camera from RenderManager
	  \return Pointer to game camera (owned by RenderManager)
	*************************************************************************/
	GGGameCamera2D* GetCamera();

	/*************************************************************************
	  \brief Get camera from RenderManager (const)
	  \return Const pointer to game camera
	*************************************************************************/
	const GGGameCamera2D* GetCamera() const;
	

	// Game view controls
	void StartGame() { m_isPlaying = true; }
	void StopGame() {
		m_isPlaying = false;
		ResetGameState();
	}
	void PauseGame() { m_isPaused = true; }
	void ResumeGame() { m_isPaused = false; }
	void TogglePause() { m_isPaused = !m_isPaused; }

	// State queries
	bool IsPlaying() const { return m_isPlaying; }
	bool IsPaused() const { return m_isPaused; }

	
	glm::vec2 GetViewportSize() const { return m_viewportSize; }

	// Object selection (for debug/testing purposes)
	uint32_t GetSelectedObjectID() const { return m_selectedObjectID; }
private:
	/*************************************************************************
	  \brief Renders the game scene into the ImGui viewport.
	  \details
		Converts framebuffer texture to ImGui image, updates input camera matrix,
		and stores rendered image position for mouse picking.
	*************************************************************************/
	void RenderGameView();
	/*************************************************************************
	  \brief Renders debug overlay in the Game View.
	  \details Shows camera info, game state, and debug indicators.
	*************************************************************************/
	void RenderDebugOverlay();

	// Input handling
	void HandleObjectPicking(glm::vec2 mousePos);


	/*************************************************************************
	  \brief Resets the game state to initial conditions.
	  \details
		Resets camera position, zoom, follow target, selected object, and
		internal state flags.
	*************************************************************************/
	void ResetGameState();

	/*************************************************************************
	  \brief Updates the viewport size and maintains target aspect ratio.
	  \details
		Adjusts camera and input manager for new viewport dimensions.
	*************************************************************************/
	void UpdateViewportSize();

	//void RenderDeadZone();

	/*************************************************************************
	  \brief Converts screen coordinates to world coordinates.
	  \param[in] screenPos  Position in screen space
	  \return glm::vec2 representing world coordinates
	*************************************************************************/
	glm::vec2 ScreenToWorld(glm::vec2 screenPos);



	EntityID m_followTargetID = NULL_ENTITY;
//default viewport
	glm::vec2 m_viewportSize{ 1600.0f, 900.0f };
	ImVec2 m_imagePos{ 0, 0 }; // Position of the rendered image in ImGui space
	float aspectRatio = 16.0f / 9.0f;
	// Game state
	bool m_isPlaying = false;
	bool m_isPaused = false;
	uint32_t m_selectedObjectID = 0;

	float m_lastFrameTime = 0.0f;
	bool m_showDeadZone = false; // Toggle for dead zone visualization

};

/*************************************************************************
\brief Returns a singleton instance of GameViewNode.
\return Reference to a unique_ptr<GameViewNode>.
\details
Ensures that only one instance of GameViewNode exists in the editor.
* ************************************************************************/
inline auto& GetGameViewNode()
{
	
	

	static std::unique_ptr<GameViewNode> gameView = std::make_unique<GameViewNode>();
	return gameView;
}