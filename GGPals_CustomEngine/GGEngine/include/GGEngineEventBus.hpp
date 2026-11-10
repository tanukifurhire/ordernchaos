/*************************************************************************
  \file       EditorEventBus.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.1
  \brief
	Global event bus providing engine-wide GGEvent channels for high-level
	editor and runtime state changes.
  \details
	This header defines the GlobalEventBus namespace, which exposes a set of
	inline GGEvent channels used for decoupled communication between core
	engine systems. These events notify subscribers about changes in play mode,
	scene transitions, script lifecycle actions, asset reload requests, and
	scene/prefab saving operations.

	Unlike EditorEventBus (UI-focused), the GlobalEventBus is engine-level
	and used by subsystems such as the ECS World, Scripting System, Animation
	System, SceneManager, and Save/Load pipelines. Each GGEvent<> provides a
	type-safe multicast mechanism for broadcasting state changes without
	direct dependencies between engine modules.
  \dependencies
	GGEvent.hpp       - Implements the GGEvent multicast event template.
	GGECSWorld.hpp    - Provides the World class for scene switching.
  \history
	- 1.0 (10 October 2025): Initial version.
	- 1.1 (30 November 2025): Updated brief to reflect engine-wide event usage.
  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <GGEvent.hpp>
#include <GGECSWorld.hpp>

/*************************************************************************
  \namespace GlobalEventBus
  \brief
	Contains all globally accessible engine events.
  \details
	These events allow systems to communicate with each other without direct
	coupling. Subscribers can register callbacks to respond to engine-wide
	actions such as play/stop mode, scene switching, script management,
	animation reloads, and saving.
*************************************************************************/
namespace GlobalEventBus
{
	/*************************************************************************
      \brief
        Triggered when the editor enters Play Mode.
      \details
        Fired once when the engine switches from Edit Mode to Play Mode.
        Systems can use this to initialize runtime data, spawn runtime-only
        entities, or reset simulation state before gameplay begins.
	*************************************************************************/
	inline GGEvent<> onPlayModeStart;
	inline GGEvent<> onPause;
	inline GGEvent<> onResume;

  /*************************************************************************
    \brief
      Triggered when the editor exits Play Mode.
    \details
      Fired when the engine returns to Edit Mode. Used by systems to
      restore editor state, clean up runtime components, or reinitialize
      editor-only data.
	*************************************************************************/
	inline GGEvent<> onStopPlayMode;

  /*************************************************************************
    \brief
      Broadcasts when the active scene should be switched.
    \details
      Passes a reference to the target World instance to be activated.
      Subscribers such as SceneManager and ECS subsystems handle unloading
      the current world and loading the new one.
    \param world Reference to the destination World.
	*************************************************************************/
	inline GGEvent<World&, World&> onSwitchScenes;

	/*************************************************************************
      \brief
        Requests a full script recompilation.
      \details
        Typically fired by the editor when scripts are modified or reloaded.
        The Scripting System listens for this event to rebuild and rebind all
        script components.
	*************************************************************************/
	inline GGEvent<> onRecompileScripts;

	/*************************************************************************
      \brief
        Requests instantiation of script components.
      \details
        Fired after scripts are compiled or reloaded. Used to rebind script
        instances to existing entities or initialize script state.
	*************************************************************************/
	inline GGEvent<> onInstantiateScripts;

	/*************************************************************************
      \brief
        Requests all reloadable assets to be refreshed.
      \details
        Used primarily by the Animation and Asset systems. Allows the editor
        to refresh animations, textures, audio, or other reloadable assets
        without requiring a full scene reload.
	*************************************************************************/
	inline GGEvent<> onReloadAssets;

	/*************************************************************************
      \brief
        Triggered when the editor initiates a save action.
      \details
        Typically tied to Ctrl+S or menu-driven save commands. The Save/Load
        pipeline listens to serialize scenes, prefabs, or project data.
	*************************************************************************/
	inline GGEvent<> onInputSave;

	/*************************************************************************
      \brief
        Broadcasts that the current scene has been modified.
      \details
        Fired whenever an operation marks the scene as "dirty." The editor UI
        or autosave systems can use this event to prompt saving or track
        modification states.
	*************************************************************************/
	inline GGEvent<> onSceneModified;

	inline GGEvent<> shutdownEvent;
}