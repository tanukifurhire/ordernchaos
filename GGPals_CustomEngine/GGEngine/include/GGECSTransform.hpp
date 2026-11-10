/*************************************************************************
  \file       GGTransformSystem.hpp
  \author     Celeste Tong Jia Xuan
  \par        Email: celestejiaxuan.tong@digipen.edu
  \date       02 October 2025
  \version    1.0
  \brief
    ECS Transform system for updating entity positions and animations.

  \details
    Provides logic to update entity transforms each frame based on:
    - Rigidbody movement (velocity integration).
    - Input-driven movement (click-to-move).
    - Sprite animation updates tied to transform changes.

    The system runs as part of the ECS update loop and ensures entities
    respond correctly to player input and physics updates.
  \dependencies
    <cstdint>, GGECSWorld.hpp, GGSystemInterface.hpp,
    GGECSInterface.hpp, GGECSComponents.hpp, GGMath/Types.hpp
  \see
    GGTransformSystem.cpp, GGECSComponents.hpp
  \history
    - 1.0 (02 October 2025): Initial version with transform, movement,
      and animation update support.
    - 1.1 (1 November 2025): movement only and updated to Local 
      Transform
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include <cstdint>
#include <GGECSWorld.hpp>
#include <GGSystemInterface.hpp>
#include <GGECSInterface.hpp>
#include <GGECSComponents.hpp>
#include <GGMath/Types.hpp>

class GGTransformSystem : public SystemInterface
{
public:
	bool Init() override;

	void Update(World& world) override;

private:
    /*std::function<void(Entity,
        LocalTransform&,
        Rigidbody2DFloat& b,
        MovementInput&,
        ClickToMove&,
        SpriteAnimator&)> updateMovement;*/

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
    std::function<void(Entity,
        LocalTransform&,
        Rigidbody2DFloat&)> updateMovement;
};