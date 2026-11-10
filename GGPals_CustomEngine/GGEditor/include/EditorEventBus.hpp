/*************************************************************************
  \file       EditorEventBus.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
    Centralized event bus for all editor-related interactions in GGEngine.
  \details
    The EditorEventBus provides a global collection of strongly-typed GGEvent
    channels used by the GGEngine Editor to communicate UI actions, selections,
    and system-level editor state changes.

    This class acts as a lightweight static dispatcher, enabling decoupled
    communication between Editor panels (Hierarchy, Inspector, Tilemap, Asset
    Browser, Audio Browser, etc.) without requiring direct references between
    systems. All events use the GGEvent<> template for efficient multicast
    callback handling.

  \dependencies
    GGEvent.hpp        - Provides the GGEvent multicast event system.
    GGECSInterface.hpp - Entity ID definitions used for selection events.
    <string>           - Used for audio asset path parameters.
  \history
    - 1.0 (10 October 2025): Initial version.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include <GGEvent.hpp>
#include <GGECSInterface.hpp>
#include <string>

/*************************************************************************
  \brief
	Static event dispatch channels used by the GGEngine Editor.
*************************************************************************/
class EditorEventBus
{
public:

	/*********************************************************************
	  \brief
		Fired when an entity is selected in panels such as Hierarchy,
		Scene View, or Inspector.
	  \param Entity
		The selected ECS entity ID.
	*********************************************************************/
	static inline GGEvent<Entity> onEntitySelected;

	/*********************************************************************
	  \brief
		Fired when the user begins dragging an entity (Hierarchy -> Scene,
		re-parenting, prefab operations, etc.).
	  \param Entity
		The dragged ECS entity ID.
	*********************************************************************/
	static inline GGEvent<Entity> onEntityDrag;

	/*********************************************************************
	  \brief
		Fired when an entity drag operation is released/dropped.
	  \param Entity
		The entity that was dropped.
	*********************************************************************/
	static inline GGEvent<Entity> onEntityDrop;

	/*********************************************************************
	  \brief
		Fired when a generic resource (texture, mesh, material, prefab)
		is selected in the Asset Browser.
	  \param uint64_t
		Unique resource identifier or hashed asset GUID.
	*********************************************************************/
	static inline GGEvent<uint64_t> onResourceSelected;

	/*********************************************************************
	  \brief
		Fired when a resource begins a drag operation from the Asset Browser.
	  \param uint64_t
		ID of the resource being dragged.
	*********************************************************************/
	static inline GGEvent<uint64_t> onResourceDrag;

	/*********************************************************************
	  \brief
		Fired when a resource drag operation ends.
	  \param uint64_t
		ID of the resource dropped.
	*********************************************************************/
	static inline GGEvent<uint64_t> onResourceDrop;

	/*********************************************************************
	  \brief
		Fired when the editor enters Play Mode (simulation start).
		EditorManager listens for this event to transition to runtime.
	*********************************************************************/
	static inline GGEvent<> onStartSimulation;

	/*********************************************************************
	  \brief
		Fired when the editor stops simulation and returns to Edit Mode.
	*********************************************************************/
	static inline GGEvent<> onStopSimulation;

	/*********************************************************************
	  \brief
		Fired when a basic asset (e.g., texture, shader, material) is
		selected in the Asset Browser.
	  \param uint32_t
		Asset ID used by the Asset System.
	*********************************************************************/
	static inline GGEvent<uint32_t> onAssetSelected;

	/*********************************************************************
	  \brief
		Fired when an audio asset is selected from the Audio Browser.
	  \param uint32_t
		Audio asset ID.
	  \param std::string
		File path of the audio resource.
	*********************************************************************/
	static inline GGEvent<uint32_t, std::string> onAudioAssetSelected;

	/*********************************************************************
	  \brief
		Fired when a tilemap asset is opened in the Tilemap Editor.
	  \param uint32_t
		Tilemap asset ID.
	*********************************************************************/
	static inline GGEvent<uint32_t> onTilemapOpen;

	/*********************************************************************
	  \brief
		Fired when the asset system requires a refresh—typically after
		imports, file changes, or engine-side rebuild of asset metadata.
	*********************************************************************/
	static inline GGEvent<> onAssetSystemRefresh;
};