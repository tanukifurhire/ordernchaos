/*************************************************************************
  \file       GGSystemManagers.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       3 October 2025
  \version    1.0
  \brief
    Defines centralized system registry with compile-time priority ordering.
  \details
    This file establishes the global system and ECS system registries for
    GGEngine via type-safe, priority-ordered registration templates.
    Uses template metaprogramming to construct statically-typed system
    containers with compile-time priority validation and automatic
    initialization ordering. The registries serve as the single source of
    truth for system lifecycle management, enabling deterministic execution
    order and dependency resolution without runtime overhead.
    
    Design Philosophy:
    - Compile-time system registration eliminates runtime registry overhead
    - Type aliases provide semantic names for priority-wrapped systems
    - Static inline variables ensure single definition across translation units
    - Accessor functions abstract registry implementation details
  \dependencies
    GGTime.hpp, GGSystemDebug.hpp, GGEngineRender.hpp, GGInputManager.hpp,
    GGEditorInterface.hpp, GGNetworkUtils.hpp, GGECSPhysics.hpp,
    GGECSMeshRenderer.hpp, GGECSSpriteAnimator.hpp, GGECSTransform.hpp
  \see
    SystemPriorityRegister, ECSSystemPriorityRegister, SystemContainer
  \history
    - 1.0 (3 October 2025): Initial static registry implementation.
  \copyright
    Copyright (C) 2025 GGPals. DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include <GGTime.hpp>
#include <GGSystemDebug.hpp>
#include <GGEngineRender.hpp>
#include <GGInputManager.hpp>
#include <GGEditorInterface.hpp>

#include <GGECSPhysics.hpp>
#include <GGECSMeshRenderer.hpp>
#include <GGECSSpriteAnimator.hpp>
#include <GGECSTransform.hpp>
#include <GGHierarchyTransformSystem.hpp>
#include <GGECSScriptSystem.hpp>
#include <GGECSGridSystem.hpp>
#include <GGECSAudioSystem.hpp>

/*************************************************************************
  \namespace Systems
  \brief
    Encapsulates system registry and type alias definitions.
  \details
    Provides centralized namespace for all engine system registrations.
    Contains type aliases mapping semantic system names to priority-wrapped
    template instantiations, along with global registry instances accessed
    via inline accessor functions.
*************************************************************************/
namespace Systems {
	using RenderManager = Systems::SystemPriorityRegister<GGEngineRender, Systems::SystemPriority::RENDERING>;
	using TimeManager = Systems::SystemPriorityRegister<GGTime::SystemTimeManager, Systems::SystemPriority::TIME>;
	using SimulationManager = Systems::SystemPriorityRegister<GGTime::SimulationManager, Systems::SystemPriority::PHYSICS>;
	using TimerManager = Systems::SystemPriorityRegister<GGTime::TimerManager, Systems::SystemPriority::TIMER>;
	using InputManager = Systems::SystemPriorityRegister<GGInputManager, Systems::SystemPriority::INPUT>;
	using EditorManager = Systems::SystemPriorityRegister<GGEditorInterface, Systems::SystemPriority::DEBUG>;

	using ECSPhysics = Systems::ECSSystemPriorityRegister<GGPhysicsSystem, Systems::SystemPriority::PHYSICS>;
	using ECSRender = Systems::ECSSystemPriorityRegister<GGMeshRendererSystem, Systems::SystemPriority::RENDERING>;
	using ECSSpriteAnimator = Systems::ECSSystemPriorityRegister<GGSpriteAnimationSystem, Systems::SystemPriority::RENDERING>;
	using ECSTransform = Systems::ECSSystemPriorityRegister<GGTransformSystem, Systems::SystemPriority::GAMEPLAY>;
    using ECSHierarchy = Systems::ECSSystemPriorityRegister<GGHierarchyTransformSystem, Systems::SystemPriority::GAMEPLAY>;
    using ECSScript = Systems::ECSSystemPriorityRegister<GGECSScriptSystem, Systems::SystemPriority::GAMEPLAY>;
    using ECSGrid = Systems::ECSSystemPriorityRegister<GGECSGridSystem, Systems::SystemPriority::GAMEPLAY>;
    using ECSAudio = Systems::ECSSystemPriorityRegister<GGECSAudioSystem, Systems::SystemPriority::GAMEPLAY>;
 /*************************************************************************
  \var registry
  \brief
    Global static registry for non-ECS systems with compile-time priority ordering.
  \details
    SystemContainer instantiation holding all SystemManagerInterface-derived
    systems. Inline variable ensures single definition across translation units
    (C++17 feature). Initialized via MakeStaticRegistry factory function with
    template parameter pack expansion for type-safe registration.
    
    Registered systems (in priority order):
    - TimeManager (TIME)
    - InputManager (INPUT)
    - SimulationManager, ECSPhysics (PHYSICS)
    - TimerManager (TIMER)
    - RenderManager, ECSRender, ECSSpriteAnimator (RENDERING)
    - NetworkServerManager, NetworkClientManager (NETWORK)
    - EditorManager (DEBUG)
  \note
    Access via GetSystemsRegistry() for encapsulation. Direct access discouraged.
  \see
    MakeStaticRegistry, SystemContainer, GetSystemsRegistry
*************************************************************************/
	inline Systems::SystemContainer
			<RenderManager,
			TimeManager,
			SimulationManager,
			TimerManager,
			InputManager,
			EditorManager> registry = 
			Systems::MakeStaticRegistry
			<RenderManager,
			TimeManager,
			SimulationManager,
			TimerManager,
			InputManager,
			EditorManager>();

/*************************************************************************
  \var ecsRegistry
  \brief
    Global static registry for ECS systems with compile-time priority ordering.
  \details
    ECSSystemContainer instantiation holding all SystemInterface-derived ECS systems.
    Separate from non-ECS registry to enable distinct iteration patterns (component
    array traversal vs global update). Inline variable ensures single definition.
    
    Registered ECS systems (in priority order):
    - ECSPhysics (PHYSICS)
    - ECSTransform (GAMEPLAY)
    - ECSSpriteAnimator, ECSRender (RENDERING)
  \note
    Access via GetECSSystemsRegistry() for encapsulation.
  \see
    MakeStaticECSRegistry, ECSSystemContainer, GetECSSystemsRegistry
*************************************************************************/
	inline Systems::ECSSystemContainer
			<ECSPhysics,
			ECSSpriteAnimator,
			ECSRender,
			ECSTransform,
            ECSHierarchy,
            ECSScript,
            ECSGrid,
            ECSAudio> ecsRegistry =
			Systems::MakeStaticECSRegistry
			<ECSPhysics,
			ECSSpriteAnimator,
			ECSRender,
			ECSTransform,
            ECSHierarchy,
            ECSScript,
            ECSGrid,
            ECSAudio>();

/*************************************************************************
  \brief
    Retrieves reference to non-ECS system registry.
  \return
    Reference to global SystemContainer instance.
  \details
    Accessor function abstracting registry implementation details. Enables
    future registry implementation changes without affecting call sites.
    Inline function has zero runtime overhead (inlined at call site).
  \par Usage
  \code
    auto& timeManager = Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>();
    timeManager.Init();
  \endcode
  \see
    registry, SystemContainer
*************************************************************************/
	inline auto& GetSystemsRegistry()
	{
		return registry;
	}

/*************************************************************************
  \brief
    Retrieves reference to ECS system registry.
  \return
    Reference to global ECSSystemContainer instance.
  \details
    Accessor function for ECS system registry. Separates ECS and non-ECS
    system access patterns for cleaner architecture and potential optimization
    (e.g., different iteration strategies).
  \par Usage
  \code
    auto& physicsSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSPhysics>();
    physicsSystem.Update(world);
  \endcode
  \see
    ecsRegistry, ECSSystemContainer
*************************************************************************/
	inline auto& GetECSSystemsRegistry()
	{
		return ecsRegistry;
	}
}