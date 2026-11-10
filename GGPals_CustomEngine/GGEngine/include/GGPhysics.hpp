/*************************************************************************
  \file       GGPhysics.hpp
  \author     Celeste Tong Jia Xuan
  \par        Email: celestejiaxuan.tong@digipen.edu
  \date       02 October 2025
  \version    1.2
  \brief
    Declares the GGPhysics class for Axis-Aligned Bounding Box (AABB)
    collision detection and response.

  \details
    Provides functionality for:
    - Static AABB collision (instantaneous overlap test).
    - Dynamic AABB collision (swept AABB detection over a timestep).
    - Screen bounds checking and bounce resolution.
    - Collision response against static and dynamic objects.
    - AABBIntersectionCollision helper (combined static + dynamic).
    - Click-to-move and WASD physics-based character control.
    - Rubric logging support for one-time collision responses.

  \dependencies
    <iostream>, <vector>, GGSystemManagers.hpp, GGECSComponents.hpp,
    GGMath/Vector2.hpp, GGLogger.hpp
  \see
    GGPhysics.cpp, GGECSPhysics.cpp
  \history
    - 1.0 (25 September 2025): Initial version with collision detection and
      response.
    - 1.1 (30 September 2025): Added ClickToMove, Push when overlay,
      changed clamping to AABBCheckScreenBounds.
    - 1.2 (02 October 2025): Added ResetRubricLogFlags method for one-time
      rubric collision logging control.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <iostream>
#include <vector>
#include <GGSystemManagers.hpp>
#include <GGECSComponents.hpp>
#include <GGMath/Vector2.hpp>


#include <GGLogger.hpp>

// --- Rubric one-time logging flags ---
static bool g_loggedDynVsStaticResponse = false; // “Basic dynamic vs static response”
static bool g_loggedDynVsDynResponse = false; // “Basic dynamic vs dynamic response”


class GGPhysics
{
public:

    /*************************************************************************
    \brief
        Resets one-time rubric logging flags for collision events.

    \details
        - g_loggedDynVsStaticResponse	= tracks if dynamic vs. static response
          was logged once.
        - g_loggedDynVsDynResponse		= tracks if dynamic vs. dynamic response
          was logged once.

        This prevents spamming the log with the same rubric messages.
        Call this before a test/demo run to ensure new collisions are logged.

    \howto
        Example:
            GGPhysics::ResetRubricLogFlags();
            // Next collision will log rubric messages again.

    *************************************************************************/
    static void ResetRubricLogFlags();

    /*************************************************************************
    \brief
        Default constructor for GGPhysics.
    *************************************************************************/
    GGPhysics();

    /*************************************************************************
    \brief
        Default destructor for GGPhysics.
    *************************************************************************/
    ~GGPhysics();

    /*************************************************************************
    \brief
        Keeps an entity constrained inside screen bounds using AABB checks.
        If the entity goes beyond screen edges, snaps it back inside and
        bounces its velocity on the corresponding axis.

    \input
      \param transform  Entity transform
      \param body       Rigidbody (velocity to update)
      \param aabb       Entity's AABB
      \param bounds     Window Bounds

    \post
        Entity transform is clamped inside screen. Rigidbody velocity is
        reflected (bounced) against the edge.
    *************************************************************************/
    static void AABBCheckScreenBounds(LocalTransform& transform, Rigidbody2DFloat& body, AABBFloat& aabb, const ScreenBounds& bounds);

    /*************************************************************************
    \brief
        Resolves a static collision (against immovable object) by bouncing
        the rigidbody's velocity along the provided normal.

    \input
      \param body Rigidbody to update
      \param normal Collision normal (which axis to bounce)
    *************************************************************************/
    static void AABBResolveStatic(Rigidbody2DFloat& body, const GGMath::Vector2& normal);

    /*************************************************************************
    \brief
        Checks for static overlap between two AABBs.

    \details
        - Tests whether two AABBs are overlapping *right now* (no velocity).
        - Checks all four separating axis conditions (X and Y).
        - Returns true if overlap exists, false otherwise.

    \howto
        Example:
            if (GGPhysics::AABBStaticCollision(box1, box2)) {
                // They are colliding right now (static overlap)
            }

    \input
      \param aabb1 First bounding box
      \param aabb2 Second bounding box

    \return
        True if the two AABBs are currently overlapping, false otherwise.

    \note
        This is an instantaneous overlap test (no velocity).
    *************************************************************************/
    static bool AABBStaticCollision(const AABBFloat& aabb1, const AABBFloat& aabb2);
    
    /*************************************************************************
    \brief
        AABB collision between two rectangles, that we already know they are
        not overlapping. Dynamic collision check between two axis-aligned
        bounding boxes (AABB). Uses the swept AABB method to determine if
        and when the boxes will collide within the given timestep.

    \details
        - Computes relative velocity between AABB1 and AABB2.
        - Solves for entry and exit times along X and Y axes.
        - Returns true if they will collide within the given dt.
        - Also reports the first contact time (TOI, "time of impact").

    \howto
        Example:
            float toi = 0.f;
            if (GGPhysics::AABBDynamicCollision(a1, v1, a2, v2, deltaTime, toi)) {
                // Collision will occur within deltaTime at fraction = toi
            }

    \input
      \param aabb1
        First bounding box to test
      \param vel1
        Velocity of the first box (units per second)
      \param aabb2
        Second bounding box to test
      \param vel2
        Velocity of the second box (units per second)
      \param dt
        Duration of the current frame/timestep to test over

    \output
      \param[out] firstTimeOfCollision
        Fraction of dt (0.0f–dt) when the first contact occurs.
        0.0f means already overlapping at frame start,
        values closer to dt mean contact later in the frame.

      \return
        True if the boxes will collide within [0, dt], false otherwise.

      \note
        This function only reports the first time of collision (TOI).
        It does not modify positions or velocities; collision response must be
        handled separately.
    *************************************************************************/
    bool AABBDynamicCollision(const AABBFloat& aabb1, const GGMath::Vector2& vel1, const AABBFloat& aabb2, const GGMath::Vector2& vel2, GGMath::float64 dt, float& firstTimeOfCollision);


    /*************************************************************************
    \brief
        Resolves a collision response between two rigidbodies along a normal.

    \details
        - Handles three cases:
          1. Both static = do nothing.
          2. One static = invert velocity of the moving body along the collision axis.
          3. Both dynamic = exchange velocities along the collision axis using momentum equations.

        - Supports separate normal.x and normal.y resolution.
        - Includes rubric logging for dynamic vs. dynamic.

    \howto
        Example:
            GGPhysics::AABBResolveDynamic(rigidA, rigidB, {1,0}); // X-axis bounce

    \input
        \param a First rigidbody (vx, vy, mass).
        \param b Second rigidbody.
        \param normal Collision normal (1,0) = X axis, (0,1) = Y axis.

    \post
        Velocities updated according to collision type (static or dynamic).

    *************************************************************************/
    static void AABBResolve(Rigidbody2DFloat& a, Rigidbody2DFloat& b, const GGMath::Vector2& normal);

    /*************************************************************************
    \brief
        Resolves overlap between two AABBs with configurable parameters for
        push strength, separation buffer, impulses, and damping. Handles
        normal collisions along a primary axis as well as corner cases where
        overlaps on X and Y are nearly equal.

    \details
        - Entities are pushed apart along X or Y depending on which overlap
          is smaller. If overlaps are within cornerTolerance, they are pushed
          diagonally.
        - Push distance is scaled by pushScale and separationBuffer so objects
          don’t remain flush edge-to-edge.
        - Opposite impulses are applied to each entity’s velocity based on
          overlap depth and impulseScale.
        - Damping is applied to reduce sliding or jitter after separation.

    \input
        \param t1  Transform of the first entity
        \param b1  Rigidbody of the first entity
        \param t2  Transform of the second entity
        \param b2  Rigidbody of the second entity
        \param a1  AABB of the first entity
        \param a2  AABB of the second entity

    \post
        - Entities are separated so their AABBs no longer overlap.
        - Their transforms are updated with the push.
        - Their velocities are updated with impulses and damping.

    \todo
        - cornerTolerance   : Threshold to decide if X and Y overlaps
                              are close enough to be treated as a corner collision.

        - pushScale         : Multiplier (0.5 = half overlap, 1.0 = full overlap)
                              that controls how strongly objects are pushed apart.

        - separationBuffer  : Extra distance to keep between entities
                              after resolving, ensuring they don’t remain flush.

        - impulseScale      : Factor controlling how much velocity is imparted to
                            each body when pushing them apart (higher = more bounce).

        - damping           : Factor applied to velocities
                              after resolution to reduce jitter/small movements
                              and infinite sliding.
    *************************************************************************/
    static void AABBResolveOverlap(LocalTransform& t1, Rigidbody2DFloat& b1, LocalTransform& t2, Rigidbody2DFloat& b2, const AABBFloat& a1, const AABBFloat& a2);
};

/*************************************************************************
\brief
    Combined static + dynamic AABB collision test. Determines if two
    axis-aligned bounding boxes either overlap immediately or will
    collide within the current frame using swept AABB.

\input
  \param aabb1 First bounding box
  \param vel1  Velocity of the first box (units per second)
  \param aabb2 Second bounding box
  \param vel2  Velocity of the second box (units per second)
  \param dt    Duration of the timestep to test (in seconds)

\output
  \param[out] firstTimeOfCollision
    Fraction of dt when the first collision occurs.
    - 0.0f means they were already overlapping at frame start.
    - Values closer to dt mean contact later in the frame.

\return
    True if the boxes are overlapping now OR will collide within [0,dt].
    False otherwise.

\note
    This is a convenience wrapper:
    - Calls AABBStaticCollision first to check immediate overlap.
    - If not overlapping, calls AABBDynamicCollision to check for
      swept collision during the timestep.
*************************************************************************/
inline bool AABBIntersectionCollision(const AABBFloat& aabb1, const GGMath::Vector2& vel1, const AABBFloat& aabb2, const GGMath::Vector2& vel2, GGMath::float64 dt, float& firstTimeOfCollision)	
{

    GGPhysics physics;
    bool staticCollision = false;
    staticCollision = physics.AABBStaticCollision(aabb1, aabb2);
    if (staticCollision)
    {
        return true;
    }

    return physics.AABBDynamicCollision(aabb1, vel1, aabb2, vel2, dt, firstTimeOfCollision);
}

