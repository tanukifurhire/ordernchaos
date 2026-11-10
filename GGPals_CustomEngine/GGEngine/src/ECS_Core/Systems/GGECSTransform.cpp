/*************************************************************************
  \file       GGTransformSystem.cpp
  \author     Celeste Tong Jia Xuan
  \par        Email: celestejiaxuan.tong@digipen.edu
  \date       02 October 2025
  \version    1.0
  \brief
    Implements the GGTransformSystem class for ECS-based transform updates.

  \details
    Provides transform update logic that integrates input, click-to-move,
    and animation systems within the ECS world, including:

    - Handling click-to-move input and setting movement targets.
    - Applying smooth movement with damping near targets.
    - Preventing overshooting of target positions.
    - Synchronizing target movement via network messages.
    - Switching animation states between idle and moving.

  \dependencies
    GGECSTransform.hpp, GGSystemManagers.hpp,
    GGEngineConfig.hpp

  \see
    GGTransformSystem.hpp, GGECSComponents.hpp

  \history
    - 1.0 (02 October 2025): Initial version with click-to-move, damping,
      overshoot prevention, and animation switching.
    - 1.1 (1 November 2025): movement only and updated to Local Transform

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGECSTransform.hpp>
#include <GGMath/Vector2.hpp>
#include <GGSystemManagers.hpp>
#include <GGLogger.hpp>
#include <GGLayerManager.hpp>

using GGMath::Vector2;

bool GGTransformSystem::Init()
{
    GlobalEventBus::onPlayModeStart += onStartSimulate;
    GlobalEventBus::onStopPlayMode += onStopSimulate;

    /*************************************************************************
    \brief
        Applies velocity-based movement to an entity.

    \details
        - Updates the entity’s position each frame using its current velocity
          and the engine’s fixed delta time.
        - Marks the entity’s LocalTransform as dirty so its world transform
          will be recomputed in the next hierarchy update.

        This function represents the basic physics integration step
        (position = velocity × deltaTime) and is used for all dynamic entities
        with Rigidbody2DFloat components.

    \input
        \param transform	Entity’s LocalTransform component (position and state flag).
        \param body			Entity’s Rigidbody2DFloat component (velocity data).

    \post
        - The entity’s position is updated based on its velocity and timestep.
        - transform.isDirty is set to true, triggering a transform update in the next frame.
    *************************************************************************/
    updateMovement =
    []([[maybe_unused]] Entity e, LocalTransform& transform, Rigidbody2DFloat& body)
        {

#ifdef GG_EDITOR_BUILD
            auto& layerManager = GGLayerManager::Get();
            if (layerManager.IsEntityDisabled(e)) {
                return;  // Skip disabled entity
            }
#endif
            float dt = static_cast<float>(Systems::GetSystemsRegistry().GetSystem<Systems::SimulationManager>().GetFixedDeltaTime());

            transform.position.x += body.vx * dt;
            transform.position.y += body.vy * dt;
            
            transform.isDirty = true;
        };
    return true;
}

void GGTransformSystem::Update(World& world)
{
    if (shouldSimulate)
    {
        world.ForEach<LocalTransform, Rigidbody2DFloat>(updateMovement);
    }
}