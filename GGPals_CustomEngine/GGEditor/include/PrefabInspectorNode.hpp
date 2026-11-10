/*************************************************************************
  \file       PrefabInspectorNode.hpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       07 November 2025
  \version    1.0
  \brief
    Declares the PrefabInspectorNode class, an editor node that provides
    an ImGui-based interface for inspecting, editing, and saving prefab
    instances in the ECS world.

  \details
    PrefabInspectorNode inherits from EditorSceneBase and provides the
    following functionality:
    - Selecting and inspecting a prefab from the hierarchy panel.
    - Editing transform properties (position and scale) of prefab instances.
    - Saving modified prefab instances as new prefabs.
    - Docking the inspector window within the editor layout.

    The class maintains a reference to the currently selected entity and
    implements Init, Update, and DockBuild callback functions for
    editor integration.

  \dependencies
    <EditorSceneBase.hpp> <GGECSWorld.hpp> <GGObjectsFactory.hpp> <memory>
    <string> <GGECSInterface.hpp> <GGEngine.hpp>

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include <EditorSceneBase.hpp>
#include <GGECSWorld.hpp>
#include <GGObjectsFactory.hpp>
#include <memory>
#include <string>

#include <GGECSInterface.hpp> 
#include <GGEngine.hpp>         

class PrefabInspectorNode : public EditorSceneBase {
public:
    /*************************************************************************
        \brief Constructor for PrefabInspectorNode
        \details Initializes the callback handles for Init, Update, and DockBuild
    *************************************************************************/
	PrefabInspectorNode();

    /*************************************************************************
        \brief Initialize the PrefabInspectorNode
        \details Currently empty, reserved for future setup or initialization
    *************************************************************************/
	virtual void Init() override;

    /*************************************************************************
        \brief Dock the Prefab Inspector window in the editor layout
        \details Uses ImGui's DockBuilder to dock the window to the right side
                 as defined by the EditorManager's docking settings
    *************************************************************************/
	virtual void DockBuild() override;

    /*************************************************************************
        \brief Update and render the Prefab Inspector GUI
        \details Handles:
                 - Displaying the selected prefab from the hierarchy panel
                 - Editing transform components (position and scale)
                 - Prefab save functionality via PrefabManager
                 - Destroying all spawned objects in the current prefab world
                 - Rendering ImGui widgets and handling user input
    *************************************************************************/
	virtual void Update() override;

private:
	Entity selectedEntity = NULL_ENTITY;
};

/*************************************************************************
  \brief Provides singleton access to the PrefabInspectorNode instance.
  \return Reference to a unique_ptr holding the PrefabInspectorNode.
*************************************************************************/
inline auto& GetPrefabInspectorNode()
{
	static std::unique_ptr<PrefabInspectorNode> prefabInspector = std::make_unique<PrefabInspectorNode>();
	return prefabInspector;
}
