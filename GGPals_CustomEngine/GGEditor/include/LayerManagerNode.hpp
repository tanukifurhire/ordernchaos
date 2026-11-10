/***********************************************************************
\file       LayerManagerNode.cpp
\author     Abhishek Akilandeswari Patchamuthu
\par        Email : a.patchamuthu@digipen.edu
\date       February 02, 2026
\version    1.0
  \brief
	Editor panel class for layer visualization and management.

  \details
	Declares the LayerManagerNode class, which provides an ImGui-based
	interface for viewing, enabling/disabling, and managing rendering layers
	in the ECS world. Supports:
	  - Separate visualization of Object and UI layers.
	  - Checkbox controls to enable/disable individual layers.
	  - Display of entity count per layer.
	  - Display of layer names.
	  - Real-time synchronization with the GGLayerManager.

  \dependencies
	EditorSceneBase.hpp, GGLayerManager.hpp, GGECSInterface.hpp

  \see
	LayerManagerNode.cpp, GGLayerManager.hpp

  \history
	- 1.0 (February 02, 2026): Initial version.df

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <EditorSceneBase.hpp>
#include <GGLayerManager.hpp>
#include <GGECSInterface.hpp>
#include <GGEngine.hpp>
#include <memory>

class LayerManagerNode : public EditorSceneBase
{
public:
	/*************************************************************************
	  \brief
		Constructor for LayerManagerNode.
	  \details
		Sets up lambda callback handles for initialization, updating,
		and docking.
	*************************************************************************/
	LayerManagerNode();

	/*************************************************************************
	  \brief
		Initializes the LayerManagerNode panel.
	  \details
		Called once at editor startup. Ensures the layer manager is ready
		for the active world.
	*************************************************************************/
	virtual void Init() override;

	/*************************************************************************
	  \brief
		Builds the dockable ImGui window for the Layer Manager panel.
	  \details
		Registers the Layer Manager window with the editor's DockBuilder,
		positioning it according to the default dock layout managed by
		EditorManager (typically docked to the right side like Inspector).
	*************************************************************************/
	virtual void DockBuild() override;

	/*************************************************************************
	  \brief
		Updates and renders the Layer Manager panel each frame.
	  \details
		Checks for layer changes in the world, displays all Object layers
		followed by UI layers in sorted order, and provides checkboxes to
		enable/disable each layer. Shows entity count and layer name for
		each layer.
	*************************************************************************/
	virtual void Update() override;

private:
	/*************************************************************************
	  \brief
		Renders the Object layers section.
	  \details
		Displays all Object layers (from MeshRenderer components) in sorted
		order by layer number. Each layer shows a checkbox for enable/disable,
		layer name, and entity count.
	*************************************************************************/
	void RenderObjectLayers();

	/*************************************************************************
	  \brief
		Renders the UI layers section.
	  \details
		Displays all UI layers (from UIComponent and TextComponent) in sorted
		order by layer number. Each layer shows a checkbox for enable/disable,
		layer name, and entity count.
	*************************************************************************/
	void RenderUILayers();

	void RenderTilemapLayer();

	/*************************************************************************
	  \brief
		Renders a single layer entry with checkbox and information.
	  \param layer
		Pointer to the GGLayer to render.
	  \details
		Creates an ImGui checkbox for enabling/disabling the layer, displays
		the layer number, name, and entity count. Handles user interaction
		to toggle layer state.
	*************************************************************************/
	void RenderLayerEntry(GGLayer* layer);

	/*************************************************************************
	  \brief
		Renders toolbar buttons for batch layer operations.
	  \details
		Provides buttons to enable/disable all Object layers, all UI layers,
		or all layers at once.
	*************************************************************************/
	void RenderToolbar();
};

/*************************************************************************
  \brief
	Provides a singleton access to the LayerManagerNode instance.
  \return
	Reference to a unique_ptr holding the LayerManagerNode.
*************************************************************************/
inline auto& GetLayerManagerNode()
{
	static std::unique_ptr<LayerManagerNode> layerManager = std::make_unique<LayerManagerNode>();
	return layerManager;
}