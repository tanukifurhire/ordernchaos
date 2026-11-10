/*************************************************************************
  \file		  InspectorNode.cpp
  \author	  Tai Xiao Xuan, Tok Duan Jie Nelson
  \par        Email: xiaoxuan.tai@digipen.edu, t.duanjienelson@digipen.edu
  \date       22 November 2025
  \version    1.1
  \contribute Tai Xiao Xuan(80%), Nelson(20% Undo/ redo to components in RenderEntityInspector()),
  \brief
	Implements the InspectorNode for GGEngine editor.
  \details
	Handles displaying entity components in the inspector, adding/removing
	components, tracking modifications, and integrating undo/redo commands.
  \dependencies
	EditorSceneBase.hpp, GGECSWorld.hpp, GGObjectsFactory.hpp, EditorViewNode.hpp, GGEngine.hpp
  \history
	- 1.0 (02 November 2025): Initial version.
	- 1.1 (22 November 2025): Added Undo/ Redo logic to components in RenderEntityInspector()
  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <EditorSceneBase.hpp>
#include <GGECSWorld.hpp>
#include <GGObjectsFactory.hpp>
#include <EditorViewNode.hpp> // to get mouse position in editor view
#include <memory>
#include <string>

#include <GGECSInterface.hpp> 
#include <GGEngine.hpp>     

class InspectorNode : public EditorSceneBase
{
public:
	/*************************************************************************
	  \brief
		Constructor for InspectorNode.
	  \details
		Sets up lambda callback handles for initialization, updating,
		and docking.
	*************************************************************************/
	InspectorNode();

	/*************************************************************************
	  \brief
		Initializes the InspectorNode.
	  \details
		Registers an entity selected callback with the EditorEventBus.
	*************************************************************************/
	virtual void Init() override;

	/*************************************************************************
	  \brief
		Builds the docking layout for the Inspector window.
	*************************************************************************/
	virtual void DockBuild() override;

	/*************************************************************************
	  \brief
		Displays the overrides panel for a selected entity.
	  \param
		entityName Name of the currently selected entity.
	  \param
		sceneName Name of the scene containing the entity.
	*************************************************************************/
	virtual void DisplayOverrideOptions(const char* entityName, std::string& sceneName);

	/*************************************************************************
	  \brief
		Displays transform editing options for the selected entity.
	  \param
		world Reference to the current World.
	  \param
		objectsFactory Reference to the object factory.
	  \param
		posX Reference to the entity's X position.
	  \param
		posY Reference to the entity's Y position.
	  \param
		rot Reference to the entity's rotation.
	*************************************************************************/
	virtual void DisplayTransformOptions(World& world [[maybe_unused]], ObjectsFactory& objectsFactory [[maybe_unused]], float& posX, float& posY, float& rot);

	/*************************************************************************
	  \brief
		Updates the Inspector window each frame.
	  \details
		Handles selection, input detection, and renders components
		inspector or add-component popup.
	*************************************************************************/
	virtual void Update() override;

	/*************************************************************************
	  \brief
		Renders all components of the currently selected entity.
	  \details
		Handles modification tracking, undo/redo, and removal of
		components.
	*************************************************************************/
	void RenderEntityInspector();

	/*************************************************************************
	  \brief
		Renders a placeholder UI when no entity is selected.
	*************************************************************************/
	void RenderNoEntitySelected();

	/*************************************************************************
	  \brief
		Renders the Add Component popup.
	  \details
		Allows the user to add components to the selected entity.
	*************************************************************************/
	void RenderAddComponent();

private: 
	Entity selectedEntity = NULL_ENTITY;
	Callback<Entity> entitySelectedCallback;

	/*************************************************************************
	  \brief
		Getter for the currently selected entity.
	  \return
		The selected Entity ID.
	*************************************************************************/
	Entity GetSelectedEntity() const { return selectedEntity; }

	/*************************************************************************
	  \brief
		Setter for the currently selected entity.
	  \param entity
		Entity to set as selected.
	*************************************************************************/
	void SetSelectedEntity(Entity entity) { selectedEntity = entity; }
};

/*************************************************************************
  \brief
	Provides a singleton access to the InspectorNode instance.
  \return
	Reference to a unique_ptr holding the InspectorNode.
*************************************************************************/
inline auto& GetInspectorNode()
{
	static std::unique_ptr<InspectorNode> inspector = std::make_unique<InspectorNode>();
	return inspector;
}
