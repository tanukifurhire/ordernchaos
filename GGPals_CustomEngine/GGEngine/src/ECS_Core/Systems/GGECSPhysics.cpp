/*************************************************************************
  \file       GGECSPhysics.cpp
  \author     Celeste Tong Jia Xuan
  \par        Email: celestejiaxuan.tong@digipen.edu
  \date       02 October 2025
  \version    1.1
  \brief
	Implements the GGPhysicsSystem class for ECS-based physics updates.

  \details
	Provides the runtime physics system logic executed each frame within
	the ECS world, including:

	- Updating Axis-Aligned Bounding Box (AABB) bounds from transforms.
	- Checking and resolving collisions against screen boundaries.
	- Detecting and resolving collisions between entities using AABB
	  static and dynamic intersection tests.
	- Applying simple bounce responses and overlap resolution.
	- Managing input-driven movement (WASD + click-to-move).
	- Tinting colliding entities red via MeshRenderer colorMultiply.

  \dependencies
	GGECSComponents.hpp, GGECSPhysics.hpp, GGECSTransform.hpp,
	GGECSWorld.hpp, GGEngine.hpp, GGJSONConfig.hpp, GGPhysics.hpp,
	GGEngineRender.hpp, GGSystemManagers.hpp
  \see
	GGECSPhysics.hpp, GGPhysics.hpp
  \history
	- 1.0 (09 September 2025): Initial version with AABB bounds update,
	  screen clamp, and entity–entity bounce response.
	- 1.1 (30 September 2025): moved click-to-move into GGPhysics.cpp, 
	  added AABB screen bounds.
	- 1.2 (1 November 2025): Added rotation and scaling to AABB
  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGECSComponents.hpp>
#include <GGECSPhysics.hpp>
#include <GGECSTransform.hpp>
#include <GGECSWorld.hpp>
#include <GGEngine.hpp>
#include <GGJSONConfig.hpp>
#include <GGPhysics.hpp>
#include <GGEngineRender.hpp>
#include <GGSystemManagers.hpp>
#include <GGLayerManager.hpp>

bool GGPhysicsSystem::Init()
{
	GlobalEventBus::onPlayModeStart += onStartSimulate;
	GlobalEventBus::onPause += onStopSimulate;
	GlobalEventBus::onResume += onStartSimulate;
	GlobalEventBus::onStopPlayMode += onStopSimulate;

	/*************************************************************************
	\brief
		Updates Axis-Aligned Bounding Box (AABB) bounds for an entity.

	\details
		- Recalculates an entity's Axis-Aligned Bounding Box (AABB)
		  based on its world transform (position, rotation, and scale).
		  This ensures the bounding box always fits tightly around the
		  rotated and scaled object in world space.

	\input
		\param transform Transform2DFloat containing position and scale.
		\param aabb      AABBFloat reference to update with calculated bounds.

	\post
		The entity’s AABB is updated to reflect its current position and scale.
	*************************************************************************/
	updateAABBBounds =
		[]([[maybe_unused]] Entity e, WorldTransform& transform, AABBFloat& aabb)
		{
#ifdef GG_EDITOR_BUILD
			auto& layerManager = GGLayerManager::Get();
			if (layerManager.IsEntityDisabled(e)) {
				return;  // Skip disabled entity
			}
#endif
			// ------------------------------------------------------------
			//
			//     | m00  m01  m02 |  m00 = cosθ * sx ,  m01 = -sinθ * sy ,  m02 = tx
			// M = | m10  m11  m12 |  m10 = sinθ * sx ,  m11 =  cosθ * sy ,  m12 = ty
			//     |  0    0    1  |
			//
			//  Each column represents one local axis:
			//   - Column 0 (m00, m10): local X-axis (includes rotation + X scale)
			//   - Column 1 (m01, m11): local Y-axis (includes rotation + Y scale)
			//   - Column 2 (m02, m12): translation (world position)
			// ------------------------------------------------------------
			
			// Extract position (tx, ty) the object’s center in world space
			GGMath::Vector2 pos = transform.matrix.getTranslation();

			// ------------------------------------------------------------
			// Extract rotation (θ) in radians.
			// 
			// atan2f(y, x) finds the angle of the vector (x, y) relative to the X-axis.
			// In our matrix, the first column (m00, m10) is the rotated X-axis:
			//   m00 = cosθ * sx: how much the X-axis points right
			//   m10 = sinθ * sx: how much the X-axis points upward
			// So, atan2f(m10, m00) = atan2f(sinθ, cosθ) = θ
			// ------------------------------------------------------------
			float rotation = atan2f(transform.matrix(1, 0), transform.matrix(0, 0));

			// ------------------------------------------------------------
			// Extract scale from the length of each local axis vector.
			// Each axis can be scaled independently, so we measure the
			// vector length (hypotenuse) of both columns:
			// a^2 + b^2 = c^2
			// sx = √(m00² + m10²)
			// sy = √(m01² + m11²)
			// ------------------------------------------------------------
			float sx = sqrtf(transform.matrix(0, 0) * transform.matrix(0, 0) + transform.matrix(1, 0) * transform.matrix(1, 0));
			float sy = sqrtf(transform.matrix(0, 1) * transform.matrix(0, 1) + transform.matrix(1, 1) * transform.matrix(1, 1));

			// Half extents (local space)
			float halfW = (sx) * 0.5f;
			float halfH = (sy) * 0.5f;

			// compute cosine & sine of θ for rotation math
			float cosR = cosf(rotation);
			float sinR = sinf(rotation);

			// ------------------------------------------------------------
			// Define the four local-space corners of the unrotated box.
			// These corners are relative to the object’s center (0,0)
			//
			//    (-halfW, -halfH)   (halfW, -halfH)
			//           +-------------+
			//           |             |
			//           |      0,0    |
			//           |             |
			//           +-------------+
			//    (-halfW, halfH)     (halfW, halfH)
			// ------------------------------------------------------------
			GGMath::Vector2 corners[4] = {
				{ -halfW, -halfH },
				{  halfW, -halfH },
				{  halfW,  halfH },
				{ -halfW,  halfH }
			};

			// ------------------------------------------------------------
			// Rotate and translate each corner into world space
			// 
			// Rotation formula:
			//   x' = x*cosθ - y*sinθ
			//   y' = x*sinθ + y*cosθ
			// Translation adds world position:
			//   x_world = x' + pos.x
			//   y_world = y' + pos.y
			// ------------------------------------------------------------
			for (auto& c : corners) {
				float x = c.x * cosR - c.y * sinR + pos.x;
				float y = c.x * sinR + c.y * cosR + pos.y;
				c = { x, y };
			}

			// Initialize min / max bounds using the first corner
			float minX = corners[0].x;
			float maxX = corners[0].x;
			float minY = corners[0].y;
			float maxY = corners[0].y;


			// ------------------------------------------------------------
			//  Loop through all 4 corners to find the min/max X and Y.
			//  These define the smallest axis-aligned box that contains
			//  the rotated shape (tight-fitting AABB) so that even if rotated
			//	by 45 degrees entity still fits in the box 
			// ------------------------------------------------------------
			for (int i = 1; i < 4; ++i) {
				minX = std::min(minX, corners[i].x);
				maxX = std::max(maxX, corners[i].x);
				minY = std::min(minY, corners[i].y);
				maxY = std::max(maxY, corners[i].y);
			}

			// Update AABB with final world-space bounds
			aabb.left = minX;
			aabb.right = maxX;
			aabb.bottom = minY;
			aabb.top = maxY;
		};

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
	checkCollisionScreen =
		[]([[maybe_unused]] Entity e, LocalTransform& t, Rigidbody2DFloat& b, AABBFloat& aabb, ScreenBounds& bounds)
		{
#ifdef GG_EDITOR_BUILD
			auto& layerManager = GGLayerManager::Get();
			if (layerManager.IsEntityDisabled(e)) {
				return;  // Skip disabled entity
			}
#endif
			int screenW = GGEngineConfig::GGEngineGraphics::GGEngineWindow::WindowWidth();
			int screenH = GGEngineConfig::GGEngineGraphics::GGEngineWindow::WindowHeight();

			float halfW = 0.5f * static_cast<float>(screenW);
			float halfH = 0.5f * static_cast<float>(screenH);

			bounds.left = -halfW;
			bounds.right = halfW;
			bounds.top = halfH;
			bounds.bottom = -halfH;

			GGPhysics::AABBCheckScreenBounds(t, b, aabb, bounds);
		};


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
	/*inputManager =
		[](Entity, MovementInput i, Rigidbody2DFloat b, Transform2DFloat& t) {

		b.vx = 0.f;
		b.vy = 0.f;

			if (Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>()
				.GGInputCheckCurr(i.keyUp))
				b.vy = i.speed;

			if (Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>()
				.GGInputCheckCurr(i.keyDown))
				b.vy = -i.speed;

			if (Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>()
				.GGInputCheckCurr(i.keyLeft))
				b.vx = -i.speed;

			if (Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>()
				.GGInputCheckCurr(i.keyRight))
				b.vx = i.speed;

			float dt = static_cast<float>(Systems::GetSystemsRegistry().GetSystem<Systems::SimulationManager>().GetFixedDeltaTime());

			t.x += b.vx * dt;
			t.y += b.vy * dt;
			t.x += b.vx * dt;
			t.y += b.vy * dt;
		};*/

	/*************************************************************************
	\brief
		Handles entity-entity collision detection and response.

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
	checkEntityIntersections =
		[&](World& world, Entity e1, LocalTransform& t1, Rigidbody2DFloat& b1, AABBFloat& aabb1)
		{
			std::function<void(Entity, LocalTransform&, Rigidbody2DFloat&, AABBFloat&)> innerLoop =
				[&](Entity e2, LocalTransform& t2, Rigidbody2DFloat& b2, AABBFloat& aabb2)
				{
#ifdef GG_EDITOR_BUILD
					auto& layerManager = GGLayerManager::Get();
					if (layerManager.IsEntityDisabled(e2)) {
						return;  // Skip disabled entity
					}
#endif
					if (e1.id >= e2.id) return;


					GGMath::Vector2 vel1(b1.vx, b1.vy);
					GGMath::Vector2 vel2(b2.vx, b2.vy);

					float firstTime = 0.0f;
					bool hit = AABBIntersectionCollision(aabb1, vel1, aabb2, vel2, Systems::GetSystemsRegistry().GetSystem<Systems::SimulationManager>().GetFixedDeltaTime(), firstTime);

					if (hit) {

						if (GGPhysics::AABBStaticCollision(aabb1, aabb2)) {
							// Step 1: Push them apart
							GGPhysics::AABBResolveOverlap(t1, b1, t2, b2, aabb1, aabb2);

							// Step 2: Handle velocity bounce or momentum
							GGMath::Vector2 normal;

							// Determine dominant axis of collision (horizontal vs vertical)
							if (fabs(aabb1.right - aabb2.left) < fabs(aabb1.top - aabb2.bottom)) {
								normal = { 1, 0 }; // horizontal
							}
							else {
								normal = { 0, 1 }; // vertical
							}

							// Apply velocity response
							GGPhysics::AABBResolve(b1, b2, normal);
						}

						float dt = static_cast<float>(Systems::GetSystemsRegistry().GetSystem<Systems::SimulationManager>().GetFixedDeltaTime());

						// Pick a simple collision normal (e.g. horizontal if overlap is wider on X)
						GGMath::Vector2 normal;

						normal.x = (t1.position.x < t2.position.x) ? -1.f : 1.f;
						normal.y = 1.f;


						t1.position.x += b1.vx * dt;
						t1.position.y += b1.vy * dt;
						t2.position.x += b2.vx * dt;
						t2.position.y += b2.vy * dt;


						GGPhysics::AABBResolve(b1, b2, normal);

						t1.position.x += b1.vx * firstTime;
						t1.position.y += b1.vy * firstTime;
						t2.position.x += b2.vx * firstTime;
						t2.position.y += b2.vy * firstTime;

					}
					else {

						if (GGPhysics::AABBStaticCollision(aabb1, aabb2)) {
							// Step 1: Push them apart
							GGPhysics::AABBResolveOverlap(t1, b1, t2, b2, aabb1, aabb2);

							// Step 2: Handle velocity bounce or momentum
							GGMath::Vector2 normal;

							// Determine dominant axis of collision (horizontal vs vertical)
							if (fabs(aabb1.right - aabb2.left) < fabs(aabb1.top - aabb2.bottom)) {
								normal = { 1, 0 }; // horizontal
							}
							else {
								normal = { 0, 1 }; // vertical
							}

							// Apply velocity response
							GGPhysics::AABBResolve(b1, b2, normal);
						}

						else {
							float dt = static_cast<float>(Systems::GetSystemsRegistry().GetSystem<Systems::SimulationManager>().GetFixedDeltaTime());

							t1.position.x += b1.vx * dt;
							t1.position.y += b1.vy * dt;
							t2.position.x += b2.vx * dt;
							t2.position.y += b2.vy * dt;

						}
					}

					t1.isDirty = true;
					t2.isDirty = true;
				};

			world.ForEach<LocalTransform, Rigidbody2DFloat, AABBFloat>(innerLoop);
		};

	return true;
}

/*************************************************************************
\brief
	Executes the physics system update loop.

\details
	- Updates all AABB bounds.
	- Processes movement input.
	- Resolves collisions against screen boundaries.
	- Detects and resolves collisions between entities.

\input
	\param world ECS World containing entities to update.

\post
	ECS entities’ transforms and rigidbodies are updated with physics
	results for the current frame.
*************************************************************************/
void GGPhysicsSystem::Update(World& world) {

	world.ForEach<WorldTransform, AABBFloat>(updateAABBBounds);

	if (shouldSimulate)
	{
		world.ForEach<LocalTransform, Rigidbody2DFloat, AABBFloat, ScreenBounds>(checkCollisionScreen);
		world.ForEachNested<LocalTransform, Rigidbody2DFloat, AABBFloat>(checkEntityIntersections);
	}
}