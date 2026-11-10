/*************************************************************************
  \file       GGECSPhysics.hpp
  \author     Celeste Tong Jia Xuan
  \par        Email: celestejiaxuan.tong@digipen.edu
  \date       02 October 2025
  \version    1.0
  \brief
    Physics system for handling entity collisions and interactions within
    the ECS framework.
  \details
    Provides physics-related processing for entities including:
    - Updating Axis-Aligned Bounding Box (AABB) bounds.
    - Checking intersections between entities.
    - Managing input-driven movement and rigidbody updates.
    - Detecting collisions against screen boundaries.

    Uses function objects to decouple physics logic for AABB, rigidbody,
    and input handling in a modular ECS-based architecture.
  \dependencies
    <cstdint>, <functional>, GGSystemInterface.hpp, GGECSInterface.hpp,
    GGECSComponents.hpp, GGMath/Types.hpp
  \see
    GGECSPhysics.cpp, GGECSComponents.hpp
  \history

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <cstdint>
#include <GGSystemInterface.hpp>
#include <GGECSInterface.hpp>
#include <GGECSComponents.hpp>
#include <GGMath/Types.hpp>


class GGPhysicsSystem : public SystemInterface {
public:
    bool Init() override;

    void Update(World& world) override;

private:

    /*************************************************************************
    \brief
        Updates Axis-Aligned Bounding Box (AABB) bounds for an entity.

    \details
        - Computes left, right, top, and bottom edges from the entity’s
          transform position and scale.
        - AABB dimensions assume a tile size of 64 units.
        - Ensures collision checks use up-to-date bounding volumes.

    \input
        \param transform Transform2DFloat containing position and scale.
        \param aabb      AABBFloat reference to update with calculated bounds.

    \post
        The entity’s AABB is updated to reflect its current position and scale.
    *************************************************************************/
    std::function
        <void(Entity,
        WorldTransform&,
        AABBFloat&)> updateAABBBounds;

    /*************************************************************************
    \brief
        Handles entity–entity collision detection and response.

    \details
        - Iterates through all pairs of entities with transforms, rigidbodies,
          and AABBs.
        - Checks both static overlap and dynamic swept collision using
          AABBIntersectionCollision.
        - On collision:
            * Resolves overlap.
            * Applies bounce response with GGPhysics::AABBResolve.
            * Advances positions to time of impact.
        - On no collision:
            * Updates positions normally using velocity * dt.

    \input
        \param world ECS World containing entities.
        \param e1    First entity ID.
        \param t1    Transform of first entity.
        \param b1    Rigidbody of first entity.
        \param aabb1 AABB of first entity.

    \post
        Entities may have adjusted positions and velocities depending on
        collision outcome.
    *************************************************************************/
    std::function
        <void(World&,
        Entity,
        LocalTransform&,
        Rigidbody2DFloat&,
        AABBFloat&)> checkEntityIntersections;

    /*************************************************************************
    \brief
        Handles input-driven entity movement.

    \details
        - Resets velocity to zero each frame.
        - Checks WASD (or mapped keys) from the InputManager system.
        - Updates velocity vector based on keypresses.
        - Applies movement scaled by fixed delta time.

    \input
        \param i MovementInput component (key bindings and speed).
        \param b Rigidbody2DFloat (velocity, updated here).
        \param t Transform2DFloat (position, updated here).

    \post
        Entity position updated according to input and fixed timestep.
*************************************************************************/
    //std::function<void(Entity,
    //    MovementInput&,
    //    Rigidbody2DFloat&, LocalTransform&)> inputManager;

    /*************************************************************************
    \brief
        Checks and resolves collisions against screen bounds. (Used for prefabs)

    \details
        - Uses GGPhysics::AABBCheckScreenBounds to test if an entity is
          outside the screen rectangle.
        - If outside, entity is clamped back inside and velocity reflected.

    \input
        \param t      Entity transform (position).
        \param b      Rigidbody component (velocity).
        \param aabb   Axis-Aligned Bounding Box of the entity.
        \param bounds Precomputed screen bounds.

    \post
        Entity position may be clamped inside screen. Rigidbody velocity
        may be inverted to simulate bounce.
    *************************************************************************/
    std::function<void(Entity,
        LocalTransform&,
        Rigidbody2DFloat&,
        AABBFloat&,
        ScreenBounds&)> checkCollisionScreen;
};