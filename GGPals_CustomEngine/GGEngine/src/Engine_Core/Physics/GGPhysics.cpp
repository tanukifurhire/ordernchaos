/*************************************************************************
  \file       GGPhysics.cpp
  \author     Celeste Tong Jia Xuan
  \par        Email: celestejiaxuan.tong@digipen.edu
  \date       30 September 2025
  \version    1.1
  \brief
	Implements the GGPhysics class for Axis-Aligned Bounding Box (AABB)
	collision detection and response.

  \details
	Provides functionality for:
	- Static AABB collision (instantaneous overlap test).
	- Dynamic AABB collision (swept AABB detection over a timestep).
	- Screen bounds checking and bounce resolution.
	- Collision response against static and dynamic objects.
	- AABBIntersectionCollision helper (combined static + dynamic).
	- Click-to-move and WASD physics-based character control.

  \history
	- 1.0 (25 September 2025): Initial version with collision detection and
	  response.
	- 1.1 (30 September 2025): Added ClickToMove, Push when overlay,
	  changed clamping to AABBCheckScreenBounds

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include "GGPhysics.hpp"
#include "GGMath/Vector2.hpp"

using GGMath::Vector2;
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
void GGPhysics::ResetRubricLogFlags() {
	g_loggedDynVsStaticResponse = false;
	g_loggedDynVsDynResponse = false;
}

/*************************************************************************
\brief
	Default constructor for GGPhysics.
*************************************************************************/
GGPhysics::GGPhysics() {}

/*************************************************************************
\brief
	Default destructor for GGPhysics.
*************************************************************************/
GGPhysics::~GGPhysics() {}

/*************************************************************************
\brief
	Keeps an entity constrained inside screen bounds using AABB checks.
	If the entity goes beyond screen edges, snaps it back inside and
	bounces its velocity on the corresponding axis.

\input
  \param transform	Entity transform
  \param body		Rigidbody (velocity to update)
  \param aabb		Entity's AABB
  \param bounds     Window Bounds

\post
	Entity transform is clamped inside screen. Rigidbody velocity is
	reflected (bounced) against the edge.
*************************************************************************/
void GGPhysics::AABBCheckScreenBounds(LocalTransform& transform, Rigidbody2DFloat& body, AABBFloat& aabb, const ScreenBounds& bounds) {

	Vector2 postion(transform.position.x, transform.position.y);

	float halfBoxW = 0.5f * (aabb.right - aabb.left);
	float halfBoxH = 0.5f * (aabb.top - aabb.bottom);

	if (postion.x - halfBoxW < bounds.left) {
		postion.x = bounds.left + halfBoxW;
		GGPhysics::AABBResolveStatic(body, { -1,0 });
		if (!g_loggedDynVsStaticResponse) {
			GGLogger::Info("Rubric Dynamic vs. Static collision response resolved via overlap (entity pushed against static body).");
			g_loggedDynVsStaticResponse = true;
		}
	}
	if (postion.x + halfBoxW > bounds.right) {
		postion.x = bounds.right - halfBoxW;
		GGPhysics::AABBResolveStatic(body, { 1,0 });
		if (!g_loggedDynVsStaticResponse) {
			GGLogger::Info("Rubric Dynamic vs. Static collision response resolved via overlap (entity pushed against static body).");
			g_loggedDynVsStaticResponse = true;
		}
	}
	if (postion.y - halfBoxH < bounds.bottom) {
		postion.y = bounds.bottom + halfBoxH;
		GGPhysics::AABBResolveStatic(body, { 0,-1 });
		if (!g_loggedDynVsStaticResponse) {
			GGLogger::Info("Rubric Dynamic vs. Static collision response resolved via overlap (entity pushed against static body).");
			g_loggedDynVsStaticResponse = true;
		}
	}
	if (postion.y + halfBoxH > bounds.top) {
		postion.y = bounds.top - halfBoxH;
		GGPhysics::AABBResolveStatic(body, { 0,1 });
		if (!g_loggedDynVsStaticResponse) {
			GGLogger::Info("Rubric Dynamic vs. Static collision response resolved via overlap (entity pushed against static body).");
			g_loggedDynVsStaticResponse = true;
		}
	}

	// Write back updated values
	transform.position.x = postion.x;
	transform.position.y = postion.y;

	//// update AABB after clamp
	//aabb.left = postion.x - halfBoxW;
	//aabb.right = postion.x + halfBoxW;
	//aabb.top = postion.y + halfBoxH;
	//aabb.bottom = postion.y - halfBoxH;
}

/*************************************************************************
\brief
	Resolves a static collision (against immovable object) by bouncing
	the rigidbody's velocity along the provided normal.

\input
  \param body Rigidbody to update
  \param normal Collision normal (which axis to bounce)
*************************************************************************/
void GGPhysics::AABBResolveStatic(Rigidbody2DFloat& body, const GGMath::Vector2& normal)
{
	Vector2 velocity(body.vx, body.vy);

	if (normal.x != 0) velocity.x *= -1;
	if (normal.y != 0) velocity.y *= -1;

	body.vx = velocity.x;
	body.vy = velocity.y;
}
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
bool GGPhysics::AABBStaticCollision(const AABBFloat& aabb1, const AABBFloat& aabb2) {

	return !(aabb1.right < aabb2.left || aabb1.left > aabb2.right 
			|| aabb1.top < aabb2.bottom || aabb1.bottom > aabb2.top);

}

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
bool GGPhysics::AABBDynamicCollision(const AABBFloat& aabb1, const GGMath::Vector2& vel1, const AABBFloat& aabb2, const GGMath::Vector2& vel2, GGMath::float64 dt, float& firstTimeOfCollision) {

	// Vb: Vb - Va = VRel
	Vector2 VRel = { 0.0 , 0.0 };
	VRel = vel2 - vel1;

	// tFirst = 0  //tFirst variable is commonly used for both the x-axis and y-axis
	// tLast = dt  //tLast variable is commonly used for both the x-axis and y-axis
	float tFirst = 0.0f;
	float tLast = static_cast<float>(dt);


	// Step 3: Working with one dimension(x - axis).
	if (VRel.x < 0) {
		// Case 1
		if (aabb1.left > aabb2.right) {
			return false;		// no intersection and B is moving away
		}

		// Case 4: Revisited
			// tFirst = max((dfirst/Vrel), tfirst) = max((A.max - B.min)/VRel, tFirst)
		float entry = (aabb1.right - aabb2.left) / VRel.x;

		// tLast = min((dLast/Vrel), tlast) = min((A.min - B.max)/VRel, tLast)
		float exit = (aabb1.left - aabb2.right) / VRel.x;

		if (entry > tFirst) {	// Max
			tFirst = entry;
		}
		if (exit < tLast) {		// Min
			tLast = exit;
		}
	}

	else if (VRel.x > 0) {
		// Case 3
		if (aabb1.right < aabb2.left) {
			return false;
		}
		// Case 2: Revisited
			// tFirst = max((dfirst/Vrel), tfirst) = max((A.min - B.max)/VRel, tFirst)
		float entry = (aabb1.left - aabb2.right) / VRel.x;

		// tLast = min((dLast/Vrel), tlast) = min((A.max - B.min)/VRel, tLast)
		float exit = (aabb1.right - aabb2.left) / VRel.x;

		if (entry > tFirst) {	// Max
			tFirst = entry;
		}
		if (exit < tLast) {		//Min
			tLast = exit;
		}
	}

	else {
		// Case 5
		if (aabb1.right < aabb2.left) {
			return false;
		}
		else if (aabb1.left > aabb2.right) {
			return false;
		}
	}

	// Step 4: Repeat step 3 on the y - axis
	if (VRel.y < 0) {
		// Case 1
		if (aabb1.bottom > aabb2.top) {
			return false;		// no intersection and B is moving away
		}

		// Case 4: Revisited
			// tFirst = max((dfirst/Vrel), tfirst) = max((A.max - B.min)/VRel, tFirst)
		float entry = (aabb1.top - aabb2.bottom) / VRel.y;

		// tLast = min((dLast/Vrel), tlast) = min((A.min - B.max)/VRel, tLast)
		float exit = (aabb1.bottom - aabb2.top) / VRel.y;

		if (entry > tFirst) {		// Max
			tFirst = entry;
		}
		if (exit < tLast) {			// Min
			tLast = exit;
		}
	}

	else if (VRel.y > 0) {
		// Case 3
		if (aabb1.top < aabb2.bottom) {
			return false;
		}

		// Case 2: Revisited
			// tFirst = max((dfirst/Vrel), tfirst) = max((A.min - B.max)/VRel, tFirst)
		float entry = (aabb1.bottom - aabb2.top) / VRel.y;

		// tLast = min((dLast/Vrel), tlast) = min((A.max - B.min)/VRel, tLast)
		float exit = (aabb1.top - aabb2.bottom) / VRel.y;

		if (entry > tFirst) {		// Max
			tFirst = entry;
		}
		if (exit < tLast) {			// Min
			tLast = exit;
		}
	}

	else {
		// case 5
		if (aabb1.top < aabb2.bottom) {
			return false;
		}
		else if (aabb1.bottom > aabb2.top) {
			return false;
		}
	}

	// Case 6
	if (tFirst > tLast) {
		return false;
	}

	// Step 5 : Return true : the rectangles intersect
	firstTimeOfCollision = tFirst;
	return true;
}


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
void GGPhysics::AABBResolve(Rigidbody2DFloat& a, Rigidbody2DFloat& b, const GGMath::Vector2& normal) {
	bool aStatic = (a.mass <= 0.f); 
	bool bStatic = (b.mass <= 0.f); 

	Vector2 velocityA(a.vx, a.vy);
	Vector2 velocityB(b.vx, b.vy);

	if (aStatic && bStatic) return; 
		
	if (aStatic) { 
		if (normal.x != 0) velocityB.x *= -1;
		if (normal.y != 0) velocityB.y *= -1;
		return;
	}
	if (bStatic) { 
		if (normal.x != 0) velocityA.x *= -1;
		if (normal.y != 0) velocityA.y *= -1;
		return; 
	} // dynamic vs dynamic momentum exchange 
	float totalMass = a.mass + b.mass; 

	if (normal.x != 0) { 
		float newAx = (a.vx * (a.mass - b.mass) + 2 * b.mass * b.vx) / totalMass; 
		float newBx = (b.vx * (b.mass - a.mass) + 2 * a.mass * a.vx) / totalMass; 
		velocityA.x = newAx;
		velocityB.x = newBx;
	} 

	if (normal.y != 0) { 
		float newAy = (a.vy * (a.mass - b.mass) + 2 * b.mass * b.vy) / totalMass; 
		float newBy = (b.vy * (b.mass - a.mass) + 2 * a.mass * a.vy) / totalMass; 
		velocityA.y = newAy;
		velocityB.y = newBy;
	} 
	if (!g_loggedDynVsDynResponse) { 
		GGLogger::Info("Rubric Dynamic vs. Dynamic collision response velocities exchanged"); 
		g_loggedDynVsDynResponse = true; 
	}
}

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
void GGPhysics::AABBResolveOverlap(LocalTransform& t1, Rigidbody2DFloat& b1, LocalTransform& t2, Rigidbody2DFloat& b2, const AABBFloat& a1, const AABBFloat& a2) {

	// These constants control how overlap resolution behaves.
	const float cornerTolerance = 0.01f;
	const float pushScale = 1.0f;
	const float separationBuffer = 0.01f;
	const float impulseScale = 0.1f;
	const float damping = 0.95f;

	// Find how far the two AABBs overlap on each side (left/right, top/bottom).
	float overlapLeft = a2.right - a1.left;
	float overlapRight = a1.right - a2.left;
	float overlapBottom = a2.top - a1.bottom;
	float overlapTop = a1.top - a2.bottom;

	// Smallest horizontal and vertical overlaps
	float xOverlap = std::min(overlapLeft, overlapRight);
	float yOverlap = std::min(overlapTop, overlapBottom);

	// Check if bodies are static (immovable), If mass <= 0, treat as static
	bool aStatic = (b1.mass <= 0.f);
	bool bStatic = (b2.mass <= 0.f);

	// Case 1: If overlaps are nearly equal = diagonal push
	if (std::abs(xOverlap - yOverlap) < cornerTolerance) {	
		float pushX = (xOverlap * pushScale) + (separationBuffer * 0.5f);
		float pushY = (yOverlap * pushScale) + (separationBuffer * 0.5f);

		// Horizontal push
		if (!aStatic && !bStatic) { // both dynamic
			if (t1.position.x < t2.position.x) {		// only B moves
				t1.position.x -= pushX;
				t2.position.x += pushX;
			}
			else {					// only A moves
				t1.position.x += pushX;
				t2.position.x -= pushX;
			}
		}
		else if (aStatic) {
			if (!g_loggedDynVsStaticResponse) {
				g_loggedDynVsStaticResponse = true;
			}
			if (t1.position.x < t2.position.x) {
				t2.position.x += pushX;
			}
			else {
				t2.position.x -= pushX;
			}
		}
		else if (bStatic) {
			if (!g_loggedDynVsStaticResponse) {
				GGLogger::Info("Rubric Dynamic vs. Static collision response resolved via overlap (entity pushed against static body).");
				g_loggedDynVsStaticResponse = true;
			}
			if (t1.position.x < t2.position.x) {
				t1.position.x -= pushX;
			}
			else {
				t1.position.x += pushX;
			}
		}

		// Vertical push
		if (!aStatic && !bStatic) {
			if (t1.position.y < t2.position.y) {
				t1.position.y -= pushY;
				t2.position.y += pushY;
			}
			else { 
				t1.position.y += pushY;
				t2.position.y -= pushY;
			}
		}
		else if (aStatic) {
			if (t1.position.y < t2.position.y) {
				t2.position.y += pushY;
			}
			else {
				t2.position.y -= pushY;
			}
		}
		else if (bStatic) {
			if (t1.position.y < t2.position.y) {
				t1.position.y -= pushY;
			}
			else {
				t1.position.y += pushY;
			}
		}

		// Impulses (only applied to dynamic bodies)
		if (!aStatic) { 
			b1.vx -= pushX * impulseScale; 
			b1.vy -= pushY * impulseScale; 
		}
		if (!bStatic) { 
			b2.vx += pushX * impulseScale; 
			b2.vy += pushY * impulseScale; 
		}
	}

	// Case 2: Horizontal overlap smaller than vertical (push along X) 
	else if (xOverlap < yOverlap) {
		float push = (xOverlap * pushScale) + (separationBuffer * 0.5f);

		if (!aStatic && !bStatic) {
			if (t1.position.x < t2.position.x) {
				t1.position.x -= push;
				t2.position.x += push;
			}
			else { 
				t1.position.x += push;
				t2.position.x -= push;
			}
		}
		else if (aStatic) {
			if (!g_loggedDynVsStaticResponse) {
				GGLogger::Info("Rubric Dynamic vs. Static collision response resolved via overlap (entity pushed against static body).");
				g_loggedDynVsStaticResponse = true;
			}
			if (t1.position.x < t2.position.x) {
				t2.position.x += push;
			}
			else {
				t2.position.x -= push;
			}
		}
		else if (bStatic) {
			if (!g_loggedDynVsStaticResponse) {
				GGLogger::Info("Rubric Dynamic vs. Static collision response resolved via overlap (entity pushed against static body).");
				g_loggedDynVsStaticResponse = true;
			}
			if (t1.position.x < t2.position.x) {
				t1.position.x -= push;
			}
			else {
				t1.position.x += push;
			}
		}

		if (!aStatic) {
			b1.vx -= push * impulseScale;
		}
		if (!bStatic) {
			b2.vx += push * impulseScale;
		}
	}

	// Case 3: Vertical overlap smaller (push along Y) 
	else {
		float push = (yOverlap * pushScale) + (separationBuffer * 0.5f);

		if (!aStatic && !bStatic) {
			if (t1.position.y < t2.position.y) {
				t1.position.y -= push;
				t2.position.y += push;
			}
			else { 
				t1.position.y += push;
				t2.position.y -= push;
			}
		}
		else if (aStatic) {
			if (t1.position.y < t2.position.y) {
				t2.position.y += push;
			}
			else {
				t2.position.y -= push;
			}
		}
		else if (bStatic) {
			if (t1.position.y < t2.position.y) {
				t1.position.y -= push;
			}
			else {
				t1.position.y += push;
			}
		}

		if (!aStatic) {
			b1.vy -= push * impulseScale;
		}
		if (!bStatic) {
			b2.vy += push * impulseScale;
		}
	}

	if (xOverlap < yOverlap) {
		// Collision was horizontal: reverse x velocity
		b1.vx *= -1.f;
		b2.vx *= -1.f;
	}
	else if (yOverlap < xOverlap) {
		// Collision was vertical: reverse y velocity
		b1.vy *= -1.f;
		b2.vy *= -1.f;
	}

	// Apply damping to smooth jitter (only to dynamic ones)
	if (!aStatic) { 
		b1.vx *= damping; 
		b1.vy *= damping; 
	}
	if (!bStatic) { 
		b2.vx *= damping; 
		b2.vy *= damping; 
	}
}

