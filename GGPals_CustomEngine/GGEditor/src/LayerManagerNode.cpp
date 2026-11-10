/*************************************************************************
  \file       LayerManagerNode.cpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu.email@digipen.edu
  \date       February 02, 2026
  \version    1.0
  \brief
	Implements the LayerManagerNode for GGEngine editor.

  \details
	Handles displaying layer information in the editor, enabling/disabling
	layers, and providing controls for batch layer operations.

  \dependencies
	LayerManagerNode.hpp, GGSystemManagers.hpp, imgui.h

  \see
	LayerManagerNode.hpp, GGLayerManager.hpp

  \history
	- 1.0 (February 02, 2026): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <LayerManagerNode.hpp>
#include <GGSystemManagers.hpp>
#include <imgui.h>

/*************************************************************************
  \brief
	Constructor for LayerManagerNode.
  \details
	Sets up lambda callback handles for initialization, updating,
	and docking.
*************************************************************************/
LayerManagerNode::LayerManagerNode()
{
	initCallbackHandle = [this]() -> void { this->Init(); };
	updateCallbackHandle = [this]() -> void { this->Update(); };
	dockBuildCallbackHandle = [this]() -> void { this->DockBuild(); };
}

/*************************************************************************
  \brief
	Initializes the LayerManagerNode.
  \details
	Performs initial setup for the layer manager panel.
	Ensures the GGLayerManager is ready to scan the active world.
*************************************************************************/
void LayerManagerNode::Init()
{
	// Get the active world and perform initial scan
	World& activeWorld = GGEngine::GetEngine().GetActiveWorld();
	GGLayerManager::Get().InitialScan(activeWorld);
}

/*************************************************************************
  \brief
	Builds the docking layout for the Layer Manager window.
  \details
	Docks the Layer Manager window to the right side, similar to
	the Inspector panel.
*************************************************************************/
void LayerManagerNode::DockBuild()
{
	ImGui::DockBuilderDockWindow("Layer Manager", Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().dockRight);
}

/*************************************************************************
  \brief
	Updates and renders the Layer Manager panel each frame.
  \details
	Creates the ImGui window, checks for layer changes in the world,
	renders the toolbar, and displays Object and UI layers sections.
*************************************************************************/
void LayerManagerNode::Update()
{
	ImGui::SetNextWindowSize(ImVec2(800, 700), ImGuiCond_Always);

	// Begin the Layer Manager window
	if (!ImGui::Begin("Layer Manager", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
	{
		ImGui::End();
		return;
	}
	// Get the active world and check for changes
	World& activeWorld = GGEngine::GetEngine().GetActiveWorld();
	GGLayerManager::Get().CheckForChanges(activeWorld);

	// Render toolbar with batch operation buttons
	RenderToolbar();

	ImGui::Separator();

	// Render Object Layers section
	RenderObjectLayers();

	ImGui::Separator();

	// Render UI Layers section
	RenderUILayers();

	// Render Tile map Layer section
	RenderTilemapLayer();

	ImGui::End();
}

/*************************************************************************
  \brief
	Renders toolbar buttons for batch layer operations.
  \details
	Provides buttons to enable/disable all Object layers, all UI layers,
	or all layers at once.
*************************************************************************/
void LayerManagerNode::RenderToolbar()
{
	ImGui::Text("Batch Operations:");

	// Enable/Disable all Object layers
	if (ImGui::Button("Enable All Objects"))
	{
		GGLayerManager::Get().EnableAllOfType(LayerType::OBJECT);
	}
	ImGui::SameLine();
	if (ImGui::Button("Disable All Objects"))
	{
		GGLayerManager::Get().DisableAllOfType(LayerType::OBJECT);
	}

	// Enable/Disable all UI layers
	if (ImGui::Button("Enable All UI"))
	{
		GGLayerManager::Get().EnableAllOfType(LayerType::UI);
	}
	ImGui::SameLine();
	if (ImGui::Button("Disable All UI"))
	{
		GGLayerManager::Get().DisableAllOfType(LayerType::UI);
	}
	// Enable/Disable all UI layers
	if (ImGui::Button("Enable All TILEMAP"))
	{
		GGLayerManager::Get().EnableAllOfType(LayerType::TILEMAP);
	}
	ImGui::SameLine();
	if (ImGui::Button("Disable All TILEMAP"))
	{
		GGLayerManager::Get().DisableAllOfType(LayerType::TILEMAP);
	}

	// Enable/Disable all layers
	if (ImGui::Button("Enable All Layers"))
	{
		GGLayerManager::Get().EnableAll();
	}
	ImGui::SameLine();
	if (ImGui::Button("Disable All Layers"))
	{
		GGLayerManager::Get().DisableAll();
	}
}

/*************************************************************************
  \brief
	Renders the Object layers section.
  \details
	Displays all Object layers (from MeshRenderer components) in sorted
	order by layer number. Each layer shows a checkbox for enable/disable,
	layer name, and entity count.
*************************************************************************/
void LayerManagerNode::RenderObjectLayers()
{
	// Get all object layers sorted by layer number
	std::vector<GGLayer*> objectLayers = GGLayerManager::Get().GetObjectLayers();

	// Header for Object Layers section
	if (ImGui::CollapsingHeader("Object Layers", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent();

		if (objectLayers.empty())
		{
			ImGui::TextDisabled("No object layers found");
		}
		else
		{
			// Display each object layer
			for (GGLayer* layer : objectLayers)
			{
				RenderLayerEntry(layer);
			}
		}

		ImGui::Unindent();
	}
}

/*************************************************************************
  \brief
	Renders the UI layers section.
  \details
	Displays all UI layers (from UIComponent and TextComponent) in sorted
	order by layer number. Each layer shows a checkbox for enable/disable,
	layer name, and entity count.
*************************************************************************/
void LayerManagerNode::RenderUILayers()
{
	// Get all UI layers sorted by layer number
	std::vector<GGLayer*> uiLayers = GGLayerManager::Get().GetUILayers();

	// Header for UI Layers section
	if (ImGui::CollapsingHeader("UI Layers", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent();

		if (uiLayers.empty())
		{
			ImGui::TextDisabled("No UI layers found");
		}
		else
		{
			// Display each UI layer
			for (GGLayer* layer : uiLayers)
			{
				RenderLayerEntry(layer);
			}
		}

		ImGui::Unindent();
	}
}
void LayerManagerNode::RenderTilemapLayer()
{
	// Get all UI layers sorted by layer number
	std::vector<GGLayer*> tiLayers = GGLayerManager::Get().GetTilemapLayers();

	// Header for UI Layers section
	if (ImGui::CollapsingHeader("tilemap Layers", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent();

		if (tiLayers.empty())
		{
			ImGui::TextDisabled("No tilemap layers found");
		}
		else
		{
			// Display each UI layer
			for (GGLayer* layer : tiLayers)
			{
				RenderLayerEntry(layer);
			}
		}

		ImGui::Unindent();
	}
}

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
void LayerManagerNode::RenderLayerEntry(GGLayer* layer)
{
	if (!layer) return;

	// Get layer information
	int layerNumber = layer->GetLayer();
	const std::string& layerName = layer->GetName();
	size_t entityCount = layer->GetEntities().size();
	bool isActive = layer->IsActive();

	// Create unique ID for this layer
	ImGui::PushID(layer);

	// Checkbox for enabling/disabling the layer
	bool enabled = isActive;
	if (ImGui::Checkbox("##LayerCheckbox", &enabled))
	{
		// Toggle the layer state
		if (enabled)
		{
			GGLayerManager::Get().EnableLayer(layer->GetKey());
		}
		else
		{
			GGLayerManager::Get().DisableLayer(layer->GetKey());
		}
	}

	ImGui::SameLine();

	// Display layer information
	ImGui::Text("[%d] %s - %zu %s",
		layerNumber,
		layerName.c_str(),
		entityCount,
		entityCount == 1 ? "entity" : "entities");

	ImGui::PopID();
}