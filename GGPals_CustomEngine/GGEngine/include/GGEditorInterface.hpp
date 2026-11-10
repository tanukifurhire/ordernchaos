/*************************************************************************
  \file       GGEditorInterface.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       3 October 2025
  \version    1.0
  \brief
    Provides a simple container using callbacks to register Editor Systems
  \dependencies
    GGSystemInterface.hpp, GGEvent.hpp
  \see
    GGEvent, SystemInterface
  \history
    - 1.0 (3 October 2025): Initial implementation
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <GGEvent.hpp>
#include <GGSystemInterface.hpp>

struct ImFont;

class GGEditorCamera2D;
class GGGameCamera2D;

/*************************************************************************
  \class GGEditorInterface
  \brief
    Manages editor UI, camera registration, and system callbacks.
  \details
    - Provides docking support via ImGui.
    - Exposes events for editor initialization, dock building, and update per frame.
    - Registers active editor/game cameras for other systems to access.
    - Handles input callbacks for saving scenes and prefabs.
*************************************************************************/
class GGEditorInterface : public SystemManagerInterface
{
public:
    /*************************************************************************
      \brief
        Initializes the editor interface and ImGui context.
      \return
        True if initialization succeeds.
      \details
        Sets up docking, custom fonts, ImGui style, and global callbacks.
    *************************************************************************/
    bool Init() override;

    /*************************************************************************
      \brief
        Updates the editor UI each frame.
      \details
        Handles docking, viewport management, and Ctrl+S input for saving.
        Emits the onUpdateEventHandler callback for custom editor logic.
    *************************************************************************/
    void Update() override;

    // Callback events
    GGEvent<> onInitEventHandler; // Emitted after editor initialization
    GGEvent<> onDockBuildEventHandler; // Emitted after the docking layout is built
    GGEvent<> onUpdateEventHandler; // Emitted every frame during Update()

    uint32_t dockMain{}; // Main docking space ID
    uint32_t dockLeft{}; // Left dock space ID
    uint32_t dockRight{}; // Right dock space ID
    uint32_t dockBottom{}; // Bottom dock space ID


private:
    // Camera management
    GGEditorCamera2D* m_activeEditorCamera = nullptr; // Currently active editor camera
    GGGameCamera2D* m_activeGameCamera = nullptr; // Currently active game camera

    bool checkSaveInput{}; // Flag to track save input state

    /*************************************************************************
      \brief
        Checks if the current scene or prefab should be saved.
      \details
        Called on Ctrl+S or global save events.
    *************************************************************************/
    void CheckSaveInput();

    /*************************************************************************
      \brief
        Sets up ImGui style for the editor interface.
      \details
        Configures colors, padding, rounding, and other ImGui style settings.
    *************************************************************************/
    void SetupImGuiStyle();

    // Internal callback handles
    Callback<> onSceneModifiedCallback; // Invoked when a scene is modified
    Callback<> checkSaveInputCallback;  // Invoked on save input (Ctrl+S)
    Callback<> onPlayCallback;          // Invoked on play mode start
    Callback<> onStopCallback;          // Invoked on stop play mode

    ImFont* customFont = nullptr;       // Custom font used for editor UI

public:
    /*************************************************************************
      \brief
        Registers the editor camera.
      \param[in] camera
        Pointer to the GGEditorCamera2D to set as active.
    *************************************************************************/
    // Camera registration (called by nodes during initialization)
    void RegisterEditorCamera(GGEditorCamera2D* camera) { m_activeEditorCamera = camera; }

    /*************************************************************************
      \brief
        Registers the game camera.
      \param[in] camera
        Pointer to the GGGameCamera2D to set as active.
    *************************************************************************/
    void RegisterGameCamera(GGGameCamera2D* camera) { m_activeGameCamera = camera; }
    
    /*************************************************************************
      \brief
        Gets the currently active editor camera.
      \return
        Pointer to the active GGEditorCamera2D.
    *************************************************************************/
    // Camera access for other systems
    GGEditorCamera2D* GetActiveEditorCamera() const { return m_activeEditorCamera; }

    /*************************************************************************
      \brief
        Gets the currently active game camera.
      \return
        Pointer to the active GGGameCamera2D.
    *************************************************************************/
    GGGameCamera2D* GetActiveGameCamera() const { return m_activeGameCamera; }
    
    // Update camera matrices in input system
   // void UpdateInputSystemCameras();
};