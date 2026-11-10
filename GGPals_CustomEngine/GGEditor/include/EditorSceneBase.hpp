/*************************************************************************
  \file       EditorSceneBase.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       26 September 2025
  \version    1.0
  \brief
    Defines the abstract base class for all editor scenes in GGEngine.
  \details
    The EditorSceneBase class establishes the fundamental interface contract
    for all editor scenes within the GGEngine architecture. It provides
    callback-based integration with the engine's EditorManager system,
    enabling scenes to receive initialization and update events through
    the event dispatch system. The class follows RAII principles and
    supports polymorphic destruction for proper resource cleanup.
  \dependencies
    GGEvent.hpp (for Callback<> template)
  \see
    DefaultEditorScene, Systems::EditorManager
  \history
    - 1.0 (26 September 2025): Initial interface definition with callback system.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <GGEvent.hpp>


/*************************************************************************
  \brief
    Abstract base class defining the editor scene interface contract
  \details
    Provides the foundational interface for all editor scenes, establishing
    a callback-based event system for integration with the EditorManager.
    Derived classes must implement Init() and Update() methods to define
    scene-specific behavior. The class maintains callback handles that
    are automatically registered with the EditorManager during scene
    initialization.
  \note
    Scenes are expected to be long-lived objects. The callback handles
    should be registered once during construction and remain valid for
    the scene's lifetime.
  \par Threading
    Not thread-safe. All scene operations must occur on the main thread.
*************************************************************************/
class EditorSceneBase 
{
public:
    /*************************************************************************
      \brief
        Type alias for initialization callback function
      \details
        Callback signature for scene initialization events. Uses the engine's
        event system for decoupled communication with the EditorManager.
    *************************************************************************/
	using InitCallback = Callback<>;

    /*************************************************************************
      \brief
        Type alias for update callback function
      \details
        Callback signature for per-frame update events. Invoked by the
        EditorManager during the engine's update cycle.
    *************************************************************************/
	using UpdateCallback = Callback<>;

    /*************************************************************************
      \brief
        Type alias for dockbuild callback function
      \details
        Callback signature for dock build events. Invoked by the
        EditorManager during the first frame of the engine's update.
    *************************************************************************/
    using DockBuildCallback = Callback<>;


    /*************************************************************************
      \brief
        Default constructor
      \details
        Initializes the scene with default state. Derived classes should
        configure their callback handles in their constructors.
    *************************************************************************/
	EditorSceneBase() = default;

    /*************************************************************************
      \brief
        Virtual destructor for proper polymorphic destruction
      \details
        Ensures proper cleanup of derived scene objects when destroyed
        through base class pointers. Critical for RAII compliance.
    *************************************************************************/
	virtual ~EditorSceneBase() = default;

    /*************************************************************************
      \brief
        Pure virtual initialization method
      \details
        Called once during scene setup. Derived classes should implement
        resource loading, system initialization, and any one-time setup
        operations here.
      \note
        This method is invoked through the callback system when the
        EditorManager triggers initialization events.
    *************************************************************************/
	virtual void Init() = 0;

    /*************************************************************************
      \brief
        Pure virtual dock build method
      \details
        Called once during the first update loop. Derived classes should implement
        dock building setup here.
      \note
        This method is invoked through the callback system when the
        EditorManager triggers dock build events.
    *************************************************************************/
    virtual void DockBuild() = 0;

    /*************************************************************************
      \brief
        Pure virtual per-frame update method
      \details
        Called every frame during the engine's update cycle. Derived classes
        should implement their frame-by-frame logic, UI rendering, and
        state management here.
      \note
        This method is invoked through the callback system during each
        EditorManager update cycle.
    *************************************************************************/
	virtual void Update() = 0;


    /*************************************************************************
      \brief
        Callback handle for initialization events
      \details
        Function object that wraps the Init() method for registration with
        the EditorManager's event system. Should be configured during
        construction to enable proper event dispatch.
    *************************************************************************/
	InitCallback initCallbackHandle;

    /*************************************************************************
      \brief
        Callback handle for update events
      \details
        Function object that wraps the Update() method for registration with
        the EditorManager's event system. Should be configured during
        construction to enable proper event dispatch.
    *************************************************************************/
	UpdateCallback updateCallbackHandle;

    /*************************************************************************
      \brief
        Callback handle for dock build events
      \details
        Function object that wraps the DockBuild() method for registration with
        the EditorManager's event system. Should be configured during
        construction to enable proper event dispatch.
    *************************************************************************/
    DockBuildCallback dockBuildCallbackHandle;
};