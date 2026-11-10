/*************************************************************************
  \file       GGECSMeshRenderer.cpp
  \author     Abhishek Akilandeswari Patchamuthu, Tai Xiao Xuan
  \par        Email: a.patchamuthu@digipen.edu,xiaoxuan.tai@digipen.edu
  \date       03 October 2025
  \contribute Abhishek(60%), Xiaoxuan(40%)
  \brief
	Implementation of GGMeshRendererSystem: collecting instance data
	and issuing instanced draws each frame, plus debug visuals and
	dynamic transform effects.

  \details
	This file contains the logic for:
	- Iterating over all entities with Transform2DFloat and MeshRenderer,
	  optionally applying dynamic scaling/rotation.
	- Creating InstanceData entries (position, scale, rotation, UVs,
	  texture handles) and uploading them to GPU.
	- Binding shaders, setting uniforms, and calling glDrawElementsInstanced.
	- Drawing debug primitives (AABBs, velocity lines, squares, circles, points)
	  for entities with relevant components.

	All per-frame instance data is cleared after rendering.
*************************************************************************/

#include <GGECSComponents.hpp>
#include <GGECSMeshRenderer.hpp>
#include <span>
#include <GGECSTransform.hpp>
#include <GGEngine.hpp>
#include <GGEngineRender.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GGECSAssetInterface.hpp>
#include <GGSystemManagers.hpp>
#include <GGRender2d.hpp>
#include <GGJSONConfig.hpp>
#include <GGColor2DIndexing.hpp>
#include <GGEngineEventBus.hpp>
#include <GGLayerManager.hpp>

static void DrawTextBoundingBox(float x, float y, float width, float height, const glm::vec4& color) {
	std::array<GGMath::Vector2, 4> corners = {
		GGMath::Vector2(x, y),
		GGMath::Vector2(x + width, y),
		GGMath::Vector2(x + width, y + height),
		GGMath::Vector2(x, y + height)
	};

	// Draw four sides
	for (int i = 0; i < 4; ++i) {
		GGLine line(
			GGPoint(corners[i], color),
			GGPoint(corners[(i + 1) % 4], color)
		);
		render2d::drawUiLine(line, 1.5f);
	}
}


/*************************************************************************
  \brief Draws a circular arc representing rotation.
  \param center The center of the arc.
  \param radius The radius of the arc.
  \param startAngle The starting angle (in radians) of the arc.
  \param endAngle The ending angle (in radians) of the arc.
  \details
	Used for visualizing rotation in the editor gizmo when dragging
	rotation handles.
*************************************************************************/
static void DrawRotationArc(GGMath::Vector2 center, float radius,
	float startAngle, float endAngle) {
	const int segments = 32;
	float angleDelta = endAngle - startAngle;
	int arcSegments = static_cast<int>(std::abs(angleDelta) / (2.0f * 3.14159f) * segments);
	arcSegments = std::max(2, std::min(arcSegments, segments));

	for (int i = 0; i < arcSegments; ++i) {
		float angle1 = startAngle + (angleDelta * i) / arcSegments;
		float angle2 = startAngle + (angleDelta * (i + 1)) / arcSegments;

		GGMath::Vector2 p1(center.x + radius * cosf(angle1),
			center.y + radius * sinf(angle1));
		GGMath::Vector2 p2(center.x + radius * cosf(angle2),
			center.y + radius * sinf(angle2));

		GGLine arcLine(GGPoint(p1, { 1.0f, 0.5f, 0.0f, 1.0f }),
			GGPoint(p2, { 1.0f, 0.5f, 0.0f, 1.0f }));
		render2d::drawLine(arcLine, 5.f);

	}

}
// Add these with your other static initializations
bool GGMeshRendererSystem::enableDynamicScale = false;
float GGMeshRendererSystem::dynamicScaleAmplitude = 0.5f;
float GGMeshRendererSystem::dynamicScaleFrequency = 2.0f;
bool GGMeshRendererSystem::enableDynamicRotation = false;
float GGMeshRendererSystem::dynamicRotationSpeed = 50.0f;


// Define and initialize the static members
bool GGMeshRendererSystem::showDebugPoints = false;
bool GGMeshRendererSystem::showDebugLines = false;
bool GGMeshRendererSystem::showDebugAABBs = false;
bool GGMeshRendererSystem::showDebugCircles = false;
bool GGMeshRendererSystem::showDebugSquares = false;

// Store world positions for each entity rendered in picking
static std::unordered_map<Entity, glm::vec2> g_entityWorldPositions;

// Initialize in cpp
bool GGMeshRendererSystem::showMoveGizmo = false;
uint32_t GGMeshRendererSystem::selectedEntity = {}; // default invalid
float GGMeshRendererSystem::gizmoHandleRadius = .10f;
float GGMeshRendererSystem::gizmoLineLength = 1.0f;

bool GGMeshRendererSystem::draggingAxis;
//static enum class Axis { NONE, X, Y };
glm::vec2 GGMeshRendererSystem::dragStartMouse;
glm::vec2 GGMeshRendererSystem::dragStartEntityPos;
GGMeshRendererSystem::Axis GGMeshRendererSystem::draggedAxis = GGMeshRendererSystem::Axis::NONE;

//gizmo mode system
GGMeshRendererSystem::GizmoMode GGMeshRendererSystem::currentGizmoMode = GGMeshRendererSystem::GizmoMode::TRANSLATE;
// Rotation gizmo state
float GGMeshRendererSystem::rotationGizmoRadius = 0.5f;
float GGMeshRendererSystem::rotationStartAngle = 0.0f;
float GGMeshRendererSystem::entityStartRotation = 0.0f;
glm::vec2 GGMeshRendererSystem::rotationDragStartPos{};
float GGMeshRendererSystem::rotationCurrentAngle = 0.0f;

// Scale gizmo state
float GGMeshRendererSystem::scaleHandleSize = 0.15f;
glm::vec2 GGMeshRendererSystem::scaleStartEntityScale{};
glm::vec2 GGMeshRendererSystem::scaleStartMouseWorld{};
GGMeshRendererSystem::ScaleHandle GGMeshRendererSystem::draggedScaleHandle = GGMeshRendererSystem::ScaleHandle::NONE;


/*************************************************************************
  \brief Creates a square mesh with the specified transform and color.
  \param transform The 2D transform containing position, scale, and rotation.
  \param color The color of the square; defaults to white if not specified.
  \return A GGSquare object representing the transformed square.
*************************************************************************/
static GGSquare MakeSquare(const WorldTransform& transform, const glm::vec4& color = { 1.f,1.f,1.f,1.f })
{
	GGMath::Vector2 pos = transform.matrix.getTranslation(); // world-space position
	float rot = atan2f(transform.matrix(1, 0), transform.matrix(0, 0)); // rotation in radians
	float scaleX = sqrtf(transform.matrix(0, 0) * transform.matrix(0, 0) + transform.matrix(1, 0) * transform.matrix(1, 0));
	float scaleY = sqrtf(transform.matrix(0, 1) * transform.matrix(0, 1) + transform.matrix(1, 1) * transform.matrix(1, 1));

	float hw = scaleX * 0.5f * GGEngineRender::WORLD_TO_SCREEN_SCALE; // scale up
	float hh = scaleY * 0.5f * GGEngineRender::WORLD_TO_SCREEN_SCALE;

	float cosR = cosf(rot);
	float sinR = sinf(rot);

	std::array<GGMath::Vector2, 4> verts = {
		GGMath::Vector2{-hw, -hh},
		GGMath::Vector2{ hw, -hh},
		GGMath::Vector2{ hw,  hh},
		GGMath::Vector2{-hw,  hh}
	};

	for (auto& v : verts) {
		float x = v.x * cosR - v.y * sinR;
		float y = v.x * sinR + v.y * cosR;
		v.x = x + pos.x; // also scale position
		v.y = y + pos.y;
	}

	return GGSquare(verts, color);
}

/*************************************************************************
  \brief Initializes the GGMeshRendererSystem.
  \details
	This function sets up the GGMeshRendererSystem by reserving memory for
	instances and configuring debug settings based on engine configuration.
  \return True if initialization is successful, false otherwise.
*************************************************************************/
bool GGMeshRendererSystem::Init()
{
	GlobalEventBus::onPlayModeStart += onStartSimulate;
	GlobalEventBus::onStopPlayMode += onStopSimulate;

	// Reserve 2x of World::MAX_ENTITIES (more headroom for tiles)
	instances.reserve(World::MAX_ENTITIES * 2);

	GGMeshRendererSystem::showDebugAABBs = GGEngineConfig::GGEngineDebug::DrawColliders();

	return true;
}

/*************************************************************************
  \brief Updates the rendering system each frame.
  \details
	This function iterates over all entities with Transform2DFloat and
	MeshRenderer components, applies dynamic transformations if enabled,
	and uploads instance data to the GPU. It also draws debug visuals for
	entities with relevant components.
  \param world The World object containing all entities and components.
  \return Void.
*************************************************************************/
void GGMeshRendererSystem::Update(World& world) {
	world.ForEach<AABBFloat>(std::function<void(Entity, AABBFloat&)>
		([&](Entity, AABBFloat& aabb)
			{
				if (showDebugAABBs)render2d::DrawAABB(aabb);
			})
	);

	// Add dynamic transform updates
	static float lastTime = 0.0f;
	static std::unordered_map<Entity, glm::vec2> baseScales; // Store original scales

	float currentTime = static_cast<float>(glfwGetTime());
	[[maybe_unused]] float deltaTime = currentTime - lastTime;
	lastTime = currentTime;
#ifdef GG_EDITOR_BUILD
	GGLayerManager::Get().CheckForChanges(world);
#endif
	RenderObjects(world);
	RenderUI(world);

	world.ForEach<AABBFloat, Rigidbody2DFloat, WorldTransform>(std::function<void(Entity, AABBFloat&, Rigidbody2DFloat&, WorldTransform&)>
		([&]([[maybe_unused]] Entity entity, [[maybe_unused]] AABBFloat& aabb, Rigidbody2DFloat& body, WorldTransform& transform)
			{
				if (!transform.isValid)
					return;

				// Extract world-space position from matrix
				GGMath::Vector2 worldPos = transform.matrix.getTranslation();
				float scale = 15.f;
				GGSquare debugSquare = MakeSquare(transform, { 1.0f, 0.f, 0.f, 0.2f });
				if (ShowDebugSquares())render2d::drawSquare(debugSquare);
				GGPoint center({ worldPos.x,worldPos.y }, { 1.0f,1.0f,1.0f,1.f });// default white color
				GGPoint velocityTip({ (body.vx * scale + worldPos.x),(body.vy * scale + worldPos.y) }, { 1.f,1.f,1.f,1.f });

				GGLine  debugVisual(center, velocityTip);
				if (ShowDebugLines())render2d::drawLine(debugVisual);

				GGCircle circle({ worldPos.x,worldPos.y }, 10, { 0.f,0.f,1.f,0.8f }, 32);
				if (ShowDebugCircles())render2d::drawCircle(circle);
				if (ShowDebugPoints())render2d::drawPoint(center, 5.f);
			})
	);
	// draw gizmo
	world.ForEach<WorldTransform>(std::function<void(Entity, WorldTransform&)>
		([&](Entity entity, WorldTransform& transform)
			{

				if (showMoveGizmo && selectedEntity == entity.id) {


					GGMath::Vector2 pos = transform.matrix.getTranslation();

					// Extract entity scale from transform
					float entityScaleX = std::sqrt(
						transform.matrix.m[0] * transform.matrix.m[0] +
						transform.matrix.m[1] * transform.matrix.m[1]
					);

					float entityScaleY = std::sqrt(
						transform.matrix.m[3] * transform.matrix.m[3] +
						transform.matrix.m[4] * transform.matrix.m[4]
					);

					float avgEntityScale = (entityScaleX + entityScaleY) * 0.5f;

					float gizmoScaleFactor = avgEntityScale * 0.5f;
					// Clamp to prevent too small or too large gizmos
					gizmoScaleFactor = std::max(0.2f, std::min(gizmoScaleFactor, 5.0f));

					// TRANSLATE MODE 
					if (currentGizmoMode == GizmoMode::TRANSLATE) {
						/*float lineLength = gizmoLineLength;
						float handleRadius = gizmoHandleRadius;*/
						float lineLength = gizmoLineLength;// *gizmoScaleFactor;
						float handleRadius = gizmoHandleRadius;// *gizmoScaleFactor;

						// X axis
						GGLine xLine(
							GGPoint(pos, { 1.f, 0.f, 0.f, 1.f }),
							GGPoint(pos + GGMath::Vector2(lineLength, 0.f), { 1.f, 0.f, 0.f, 1.f })
						);
						render2d::drawLine(xLine);

						WorldTransform handleTransformX;
						handleTransformX.matrix = GGMath::Matrix3x3::TRS(
							GGMath::Vector2(pos.x + lineLength, pos.y),
							0.0f,
							GGMath::Vector2(handleRadius, handleRadius)
						);
						handleTransformX.isValid = true;
						GGSquare xHandle = MakeSquare(handleTransformX, { 1, 0, 0, 1 });
						render2d::drawSquare(xHandle);

						// Y axis
						GGLine yLine(
							GGPoint(pos, { 0.f, 1.f, 0.f, 1.f }),
							GGPoint(pos + GGMath::Vector2(0.f, lineLength), { 0.f, 1.f, 0.f, 1.f })
						);
						render2d::drawLine(yLine);

						WorldTransform handleTransformY;
						handleTransformY.matrix = GGMath::Matrix3x3::TRS(
							GGMath::Vector2(pos.x, pos.y + lineLength),
							0.0f,
							GGMath::Vector2(handleRadius, handleRadius)
						);
						handleTransformY.isValid = true;
						GGSquare yHandle = MakeSquare(handleTransformY, { 0.f, 1.f, 0.f, 1.f });
						render2d::drawSquare(yHandle);
					}
					// ROTATE MODE
					else if (currentGizmoMode == GizmoMode::ROTATE) {
						//float radius = rotationGizmoRadius;
						float radius = rotationGizmoRadius * gizmoScaleFactor;

						// Draw rotation circle
						GGCircle rotationCircle(pos, radius, { 0.8f, 0.8f, 0.0f, 0.7f }, 64);
						render2d::drawCircle(rotationCircle);

						// If actively dragging, draw arc indicator
						if (draggingAxis && draggedAxis == Axis::ROTATE) {
							// Calculate current angle for arc
							glm::vec2 entityPosGlm(pos.x, pos.y);

							DrawRotationArc(pos, radius, rotationStartAngle, rotationCurrentAngle);
						}
					}
					// SCALE MODE
					else if (currentGizmoMode == GizmoMode::SCALE) {
						// Extract transform components
						float rot = atan2f(transform.matrix(1, 0), transform.matrix(0, 0));
						float scaleX = sqrtf(transform.matrix(0, 0) * transform.matrix(0, 0) +
							transform.matrix(1, 0) * transform.matrix(1, 0));
						float scaleY = sqrtf(transform.matrix(0, 1) * transform.matrix(0, 1) +
							transform.matrix(1, 1) * transform.matrix(1, 1));



						float handleSize = scaleHandleSize * gizmoScaleFactor;


						float halfWidth = scaleX * 0.5f;
						float halfHeight = scaleY * 0.5f;

						// You can adjust these values to control how far inside the handles are
						float insetDistance = handleSize * 0.6f;

						// Calculate inset in local space (before rotation)
						float insetX = insetDistance;
						float insetY = insetDistance;

						// to make that the inset does not exced half the object size
						insetX = std::min(insetX, halfWidth * 0.3f);
						insetY = std::min(insetY, halfHeight * 0.3f);


						std::array<GGMath::Vector2, 4> localCorners = {
							GGMath::Vector2(-halfWidth + insetX, -halfHeight + insetY),  // Bottom-left (inset)
							GGMath::Vector2(halfWidth - insetX, -halfHeight + insetY),  // Bottom-right (inset)
							GGMath::Vector2(halfWidth - insetX,  halfHeight - insetY),  // Top-right (inset)
							GGMath::Vector2(-halfWidth + insetX,  halfHeight - insetY)   // Top-left (inset)
						};

						std::array<GGMath::Vector2, 4> corners;
						float cosR = cosf(rot);
						float sinR = sinf(rot);

						for (size_t i = 0; i < 4; ++i) {
							float localX = localCorners[i].x;
							float localY = localCorners[i].y;

							// Rotate around origin
							float rotatedX = localX * cosR - localY * sinR;
							float rotatedY = localX * sinR + localY * cosR;

							// Translate to world position
							corners[i] = GGMath::Vector2(
								pos.x + rotatedX,
								pos.y + rotatedY
							);
						}

						// outline for scale gizmo
						std::array<GGMath::Vector2, 4> boxCorners;
						std::array<GGMath::Vector2, 4> actualLocalCorners = {
							GGMath::Vector2(-halfWidth, -halfHeight),
							GGMath::Vector2(halfWidth, -halfHeight),
							GGMath::Vector2(halfWidth,  halfHeight),
							GGMath::Vector2(-halfWidth,  halfHeight)
						};

						for (size_t i = 0; i < 4; ++i) {
							float localX = actualLocalCorners[i].x;
							float localY = actualLocalCorners[i].y;
							float rotatedX = localX * cosR - localY * sinR;
							float rotatedY = localX * sinR + localY * cosR;
							boxCorners[i] = GGMath::Vector2(pos.x + rotatedX, pos.y + rotatedY);
						}

						for (int i = 0; i < 4; ++i) {
							int next = (i + 1) % 4;
							GGLine boxLine(
								GGPoint(boxCorners[i], { 0.6f, 0.6f, 0.6f, 1.0f }),
								GGPoint(boxCorners[next], { 0.6f, 0.6f, 0.6f, 1.0f })
							);
							render2d::drawLine(boxLine, 2.0f);
						}


						auto getCornerIndex = [](ScaleHandle handle) -> int {
							switch (handle) {
							case ScaleHandle::BOTTOM_LEFT:  return 0;
							case ScaleHandle::BOTTOM_RIGHT: return 1;
							case ScaleHandle::TOP_RIGHT:    return 2;
							case ScaleHandle::TOP_LEFT:     return 3;
							default:                        return -1;
							}
							};


						for (size_t i = 0; i < 4; ++i) {
							WorldTransform handleTransform;
							handleTransform.matrix = GGMath::Matrix3x3::TRS(
								corners[i],
								rot,
								GGMath::Vector2(handleSize, handleSize)
							);
							handleTransform.isValid = true;

							// Default color
							glm::vec4 handleColor = { 0.0f, 0.5f, 1.0f, 1.0f }; // Blue

							// Check if this handle is being dragged
							if (draggingAxis && draggedScaleHandle != ScaleHandle::NONE &&
								draggedScaleHandle != ScaleHandle::CENTER) {
								int draggedCornerIndex = getCornerIndex(draggedScaleHandle);
								if (draggedCornerIndex == static_cast<int>(i)) {
									handleColor = { 0.0f, 0.8f, 1.0f, 1.0f }; // Brighter blue when dragging
								}
							}

							GGSquare cornerHandle = MakeSquare(handleTransform, handleColor);
							render2d::drawSquare(cornerHandle);
						}

						// Center handle for uniform scaling 
						float centerHandleSize = handleSize * 1.0f;


						WorldTransform centerHandleTransform;
						centerHandleTransform.matrix = GGMath::Matrix3x3::TRS(
							pos,
							rot,
							GGMath::Vector2(centerHandleSize, centerHandleSize)
						);
						centerHandleTransform.isValid = true;

						glm::vec4 centerColor = { 1.0f, 1.0f, 1.0f, 1.0f };
						if (draggingAxis && draggedScaleHandle == ScaleHandle::CENTER) {
							centerColor = { 1.0f, 1.0f, 0.5f, 1.0f }; // Yellowish when dragging
						}

						GGSquare centerHandle = MakeSquare(centerHandleTransform, centerColor);
						render2d::drawSquare(centerHandle);
					}
				}
			})
	);

	auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();
	auto& renderMgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();

	if (inputMgr.GGInputCheckTriggered(GGKey_F8))
	{
		renderMgr.SetOverlayShowFPS(!renderMgr.GetOverlayShowFPS());
		showDebugAABBs = !showDebugAABBs;
		showDebugSquares = !showDebugSquares;
	}


}

/*************************************************************************
  \brief Adds an instance to the current frame's instance buffer.
  \param instance The instance data to push.
  \details
	Used internally by RenderObjects/RenderUI to accumulate per-frame
	instance data for instanced drawing.
*************************************************************************/
void GGMeshRendererSystem::PushInstance(InstanceData&& instance)
{
	instances.emplace_back(std::move(instance));
}

/*************************************************************************
  \brief Renders entities for GPU picking.
  \param world Reference to the ECS world.
  \details
	- Collects entities' transform data and assigns unique picking colors.
	- Sorts instances according to layer and Y-sort rules.
	- Uploads data to GPU and draws instanced geometry with the picking shader.
	- Stores world positions for gizmo interaction.
*************************************************************************/
void GGMeshRendererSystem::RenderPicking(World& world) {

	std::vector<PickingInstanceData> pickingInstances;
	pickingInstances.reserve(World::MAX_ENTITIES * 2);

	// Render picking
	world.ForEach<WorldTransform, MeshRenderer>(
		std::function<void(Entity, WorldTransform&, MeshRenderer&)>(
			[&](Entity entity, WorldTransform& transform, MeshRenderer& renderer) {

				if (!transform.isValid) return;

				PickingInstanceData data{};
				// Get layer and ySort from MeshRenderer if it exists, otherwise use defaults
				//if (world.HasComponent<MeshRenderer>(entity)) {
					//const MeshRenderer* renderer = world.GetComponent<MeshRenderer>(entity);
#ifdef GG_EDITOR_BUILD

				if (!GGLayerManager::Get().IsEnabled({ renderer.layer, LayerType::OBJECT }))
					return;
#endif

				data.translation.z = renderer.perspectiveFactor;
				data.layer = renderer.layer;
				data.ySort = renderer.ySort ? 1 : 0;
				data.pixelPerfectPicking = renderer.pixelPerfectPicking ? 1 : 0;
				//}
				//else {
				//	// Default values for entities without MeshRenderer
				//	data.layer = 0;
				//	data.ySort = 0;
				//}

				 // Extract transform data
				GGMath::Vector2 worldPos = transform.matrix.getTranslation();
				float rot = atan2f(transform.matrix(1, 0), transform.matrix(0, 0));
				float scaleX = sqrtf(transform.matrix(0, 0) * transform.matrix(0, 0) +
					transform.matrix(1, 0) * transform.matrix(1, 0));
				float scaleY = sqrtf(transform.matrix(0, 1) * transform.matrix(0, 1) +
					transform.matrix(1, 1) * transform.matrix(1, 1));


				data.translation = glm::vec3(worldPos.x, worldPos.y, 0.f);
				data.rotation = rot;
				data.scale = glm::vec3(scaleX, scaleY, 1.0f);

				data.pickableObject = renderer.pickableObject ? 1 : 0;
				if (world.HasComponent<Sprite>(entity)) {
					const Sprite* sprite = world.GetComponent<Sprite>(entity);

					data.textureID = sprite->GetTextureID();
					data.uvBounds = sprite->textureUVBounds;
					data.useTexture = (data.textureID != 0) ? 1 : 0;
				}
				else {
					data.useTexture = 0;
					data.textureID = 0;
					data.uvBounds = glm::vec4(0.f, 0.f, 1.f, 1.f);
				}





				// Set picking color for this entity
				data.pickingColor = Color2DIndexing::IDToColor(entity.id);

				pickingInstances.emplace_back(data);

				// Store world position for gizmo interaction
				g_entityWorldPositions[entity] = glm::vec2(worldPos.x, worldPos.y);
			}
		)
	);

	if (pickingInstances.empty()) return;

	// 2. Sort IDENTICALLY to main render
	std::sort(pickingInstances.begin(), pickingInstances.end(),
		[](const PickingInstanceData& a, const PickingInstanceData& b)
		{
			if (a.layer != b.layer)
				return a.layer < b.layer;

			if (a.ySort != b.ySort)
				return a.ySort < b.ySort;

			if (a.ySort == 1)
				return a.translation.y > b.translation.y;

			return false;
		});

	// 3. Upload to GPU and draw
	GGMesh mesh = Systems::GetSystemsRegistry()
		.GetSystem<Systems::RenderManager>()
		.GetDefaultMesh();

	// Use picking shader
	GGShader const& pickingShader = Systems::GetSystemsRegistry()
		.GetSystem<Systems::RenderManager>()
		.GetDefaultPickingShader();

	pickingShader.Use();

	// Same matrices as main render
	pickingShader.SetMat4("projection",
		Systems::GetSystemsRegistry()
		.GetSystem<Systems::RenderManager>()
		.GetActiveProjection());

	pickingShader.SetMat4("cameraView",
		Systems::GetSystemsRegistry()
		.GetSystem<Systems::RenderManager>()
		.GetActiveView());

	pickingShader.SetMat4("worldscale",
		Systems::GetSystemsRegistry()
		.GetSystem<Systems::RenderManager>()
		.GetDefaultWorldScale());

	pickingShader.SetBool("uUseInstancing", true);
	pickingShader.SetBool("uUseUVTransform", true);
	pickingShader.SetFloat("alphaThreshold", 0.1f);

	// Batch draw by texture
	size_t i = 0;
	while (i < pickingInstances.size()) {
		uint32_t currentTex = pickingInstances[i].textureID;
		size_t batchStart = i;
		while (i < pickingInstances.size() && pickingInstances[i].textureID == currentTex) {
			++i;
		}
		size_t batchSize = i - batchStart;

		if (currentTex != 0) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, currentTex);
			pickingShader.SetInt("uTexture", 0);
		}

		std::span<const PickingInstanceData> batchSpan(pickingInstances.data() + batchStart, batchSize);
		mesh.UpdatePickingInstanceData(batchSpan);

		glBindVertexArray(mesh.vaoid);
		glDrawElementsInstanced(
			mesh.primitiveType,
			mesh.drawCount,
			GL_UNSIGNED_SHORT,
			nullptr,
			static_cast<GLsizei>(batchSize)
		);
	}
	glBindVertexArray(0);
}

/*************************************************************************
  \brief Renders UI entities for GPU picking.
  \param world Reference to the ECS world.
  \details
	Similar to RenderPicking but adapted for UI elements:
	- Uses zoom-adjusted camera view instead of world camera.
	- Collects UI-specific components (UIComponent) and layer/ySort info.
*************************************************************************/
void GGMeshRendererSystem::RenderUIPicking(World& world) {
	auto& render = Systems::GetSystemsRegistry()
		.GetSystem<Systems::RenderManager>();

	if (!render.GetActiveCamera()) return;

	std::vector<PickingInstanceData> pickingInstances;
	pickingInstances.reserve(World::MAX_ENTITIES * 2);

	// Render picking for ui
	world.ForEach<WorldTransform>(
		std::function<void(Entity, WorldTransform&)>(
			[&](Entity entity, WorldTransform& transform) {

				if (!transform.isValid) return;

				PickingInstanceData data{};
				// Get layer and ySort from MeshRenderer if it exists, otherwise use defaults
				if (world.HasComponent<UIComponent>(entity)) {
					const UIComponent* renderer = world.GetComponent<UIComponent>(entity);

#ifdef GG_EDITOR_BUILD
					if (!GGLayerManager::Get().IsEnabled({ renderer->layer, LayerType::UI }))
						return;

#endif

					data.layer = renderer->layer;
					data.ySort = renderer->ySort ? 1 : 0;
					data.pixelPerfectPicking = renderer->pixelPerfectPicking ? 1 : 0;
					data.pickableObject = renderer->pickableObject ? 1 : 0;
					if (world.HasComponent<Sprite>(entity)) {
						const Sprite* sprite = world.GetComponent<Sprite>(entity);
						data.textureID = sprite->GetTextureID();
						data.uvBounds = sprite->textureUVBounds;
						data.useTexture = (data.textureID != 0) ? 1 : 0;
					}
					else {
						data.useTexture = 0;
						data.textureID = 0;
						data.uvBounds = glm::vec4(0.f, 0.f, 1.f, 1.f);
					}
				}
				else {
					// Default values for entities without MeshRenderer
					data.layer = 0;
					data.ySort = 0;
				}
				if (world.HasComponent<TextComponent>(entity)) {
					const TextComponent* textComp = world.GetComponent<TextComponent>(entity);

#ifdef GG_EDITOR_BUILD

					if (!GGLayerManager::Get().IsEnabled({ textComp->layer, LayerType::UI }))
						return;

#endif

					data.layer = textComp->layer;
					data.ySort = textComp->ySort ? 1 : 0;
				}

				// Extract transform data
				GGMath::Vector2 worldPos = transform.matrix.getTranslation();
				float rot = atan2f(transform.matrix(1, 0), transform.matrix(0, 0));
				float scaleX = sqrtf(transform.matrix(0, 0) * transform.matrix(0, 0) +
					transform.matrix(1, 0) * transform.matrix(1, 0));
				float scaleY = sqrtf(transform.matrix(0, 1) * transform.matrix(0, 1) +
					transform.matrix(1, 1) * transform.matrix(1, 1));


				data.translation = glm::vec3(worldPos.x, worldPos.y, 0.0f);
				data.rotation = rot;
				data.scale = glm::vec3(scaleX, scaleY, 1.0f);




				// Set picking color for this entity
				data.pickingColor = Color2DIndexing::IDToColor(entity.id);

				pickingInstances.emplace_back(data);

				// Store world position for gizmo interaction
				g_entityWorldPositions[entity] = glm::vec2(worldPos.x, worldPos.y);
			}
		)
	);

	if (pickingInstances.empty()) return;

	// 2. Sort IDENTICALLY to main render
	std::sort(pickingInstances.begin(), pickingInstances.end(),
		[](const PickingInstanceData& a, const PickingInstanceData& b)
		{
			if (a.layer != b.layer)
				return a.layer < b.layer;

			if (a.ySort != b.ySort)
				return a.ySort < b.ySort;

			if (a.ySort == 1)
				return a.translation.y > b.translation.y;

			return false;
		});

	// 3. Upload to GPU and draw
	GGMesh mesh = render.GetDefaultMesh();

	// Use picking shader
	GGShader const& pickingShader = Systems::GetSystemsRegistry()
		.GetSystem<Systems::RenderManager>()
		.GetDefaultPickingShader();

	pickingShader.Use();

	// Same matrices as main render
	pickingShader.SetMat4("projection", render.GetActiveCamera()->GetScreenProjectionMatrix());

	auto camera = Systems::GetSystemsRegistry()
		.GetSystem<Systems::RenderManager>().GetActiveCamera();

	glm::vec2 resScale = camera ? camera->GetResolutionScale() : glm::vec2(1.f);

	glm::mat4 zoomMatrix = glm::scale(glm::mat4(1.f), { resScale, 1.f });

	// Use identity matrix for UI objects
	pickingShader.SetMat4("cameraView", zoomMatrix);

	pickingShader.SetMat4("worldscale",
		Systems::GetSystemsRegistry()
		.GetSystem<Systems::RenderManager>()
		.GetDefaultWorldScale());

	pickingShader.SetBool("uUseInstancing", true);
	pickingShader.SetBool("uUseUVTransform", true);
	pickingShader.SetFloat("alphaThreshold", 0.1f);

	// Batch draw by texture
	size_t i = 0;
	while (i < pickingInstances.size()) {
		uint32_t currentTex = pickingInstances[i].textureID;
		size_t batchStart = i;
		while (i < pickingInstances.size() && pickingInstances[i].textureID == currentTex) {
			++i;
		}
		size_t batchSize = i - batchStart;

		if (currentTex != 0) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, currentTex);
			pickingShader.SetInt("uTexture", 0);
		}

		std::span<const PickingInstanceData> batchSpan(pickingInstances.data() + batchStart, batchSize);
		mesh.UpdatePickingInstanceData(batchSpan);

		glBindVertexArray(mesh.vaoid);
		glDrawElementsInstanced(
			mesh.primitiveType,
			mesh.drawCount,
			GL_UNSIGNED_SHORT,
			nullptr,
			static_cast<GLsizei>(batchSize)
		);
	}
	glBindVertexArray(0);
}

/*************************************************************************
  \brief Renders all UI elements using instancing.
  \param world Reference to the ECS world.
  \details
	- Collects WorldTransform + UIComponent + optional Sprite data.
	- Sorts instances based on layer and Y-sort rules.
	- Uploads instance data to GPU and draws using the default shader.
*************************************************************************/
void GGMeshRendererSystem::RenderUI(World& world)
{
	auto& render = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();

	if (!render.GetActiveCamera()) return;

	// Gather instance data
	world.ForEach<WorldTransform, UIComponent>(
		std::function<void(Entity, WorldTransform&, UIComponent&)>(
			[&](Entity entity, WorldTransform& transform, UIComponent& renderer) {

				if (!transform.isValid)
					return; // skip entities not updated yet
#ifdef GG_EDITOR_BUILD
				if (!GGLayerManager::Get().IsEnabled({ renderer.layer, LayerType::UI }))
					return;
#endif
				// Extract world position, rotation, and scale
				GGMath::Vector2 worldPos = transform.matrix.getTranslation();

				float rot = atan2f(transform.matrix(1, 0), transform.matrix(0, 0));
				float scaleX = sqrtf(transform.matrix(0, 0) * transform.matrix(0, 0) + transform.matrix(1, 0) * transform.matrix(1, 0));
				float scaleY = sqrtf(transform.matrix(0, 1) * transform.matrix(0, 1) + transform.matrix(1, 1) * transform.matrix(1, 1));

				// fill instance data
				InstanceData data{};
				data.translation = glm::vec3(worldPos.x, worldPos.y, 0.0f);
				data.rotation = rot;
				data.scale = glm::vec3(scaleX, scaleY, 1.0f);
				data.colorAdd = renderer.colorAdd;
				data.colorMultiply = renderer.colorMultiply;

				// Higher layers render first
				data.layer = renderer.layer;

				// Enable/disable ySort
				data.ySort = renderer.ySort ? 1 : 0;

				if (world.HasComponent<ObjectEffectComponent>(entity)) {
					const ObjectEffectComponent* fx = world.GetComponent<ObjectEffectComponent>(entity);
					data.effectType = fx->effectType;
					data.intensity = fx->intensity;

					data.effectColor = fx->color;
					data.effectDuration = fx->effectDuration;
					data.loop = fx->loop ? 1 : 0;
					data.useEffect = 1;
					if (fx->effectType != ObjectEffectComponent::EffectType::NONE) {
						// Re-stamp if this is a fresh activation (startTime == 0)
						if (fx->startTime == 0.f) {
							const_cast<ObjectEffectComponent*>(fx)->startTime = (float)glfwGetTime();
						}
					}
					else {
						// Effect is NONE � reset so next activation gets a fresh stamp
						if (fx->startTime != 0.f) {
							const_cast<ObjectEffectComponent*>(fx)->startTime = 0.f;
						}
					}
					data.effectStartTime = fx->startTime;
				}
				else {
					data.effectType = 0;
					data.intensity = 1.f;

					data.effectColor = glm::vec4(1.f);
					data.effectDuration = 1.f;
					data.loop = 0;
					data.useEffect = 0;

				}
				if (world.HasComponent<Sprite>(entity)) {
					const Sprite* sprite = world.GetComponent<Sprite>(entity);

					data.textureID = sprite->GetTextureID();
					data.uvBounds = sprite->textureUVBounds;
					data.useTexture = (data.textureID != 0) ? 1 : 0;

					instances.emplace_back(data);
				}
				else {
					data.useTexture = 0;

					instances.emplace_back(data);
				}
			}
		)
	);

	world.ForEach<WorldTransform, TextComponent>(
		std::function<void(Entity, WorldTransform&, TextComponent&)>(
			[&](Entity entity, WorldTransform& transform, TextComponent& textComp) {

				(void)entity;
				if (!transform.isValid || !textComp.visible) return;

#ifdef GG_EDITOR_BUILD
				if (!GGLayerManager::Get().IsEnabled({ textComp.layer, LayerType::UI }))
					return;
#endif
				GGMath::Vector2 pos = transform.matrix.getTranslation();


				int   effectType = 0;
				float intensity = 1.f;
				glm::vec4 effectColor = glm::vec4(1.f);
				float effectDuration = 1.f;
				int   loop = 0;
				int   useEffect = 0;
				float effectStartTime = 0.f;

				if (world.HasComponent<ObjectEffectComponent>(entity)) {
					const ObjectEffectComponent* fx = world.GetComponent<ObjectEffectComponent>(entity);
					effectType = fx->effectType;
					intensity = fx->intensity;
					effectColor = fx->color;
					effectDuration = fx->effectDuration;
					loop = fx->loop ? 1 : 0;
					useEffect = 1;
					if (fx->effectType != ObjectEffectComponent::EffectType::NONE) {
						if (fx->startTime == 0.f)
							const_cast<ObjectEffectComponent*>(fx)->startTime = (float)glfwGetTime();
					}
					else {
						if (fx->startTime != 0.f)
							const_cast<ObjectEffectComponent*>(fx)->startTime = 0.f;
					}
					effectStartTime = fx->startTime;
				}


				try {

					std::string fontPath = "";

					fontPath = textComp.getFontPath();



					if (fontPath == "") {
						fontPath = "Fonts/SourceSans3-Bold.ttf";
					}



					auto& atlas = GGFontAtlasManager::GetFontAtlas(fontPath, textComp.fontSize);


					float scaleX = sqrtf(transform.matrix(0, 0) * transform.matrix(0, 0) +
						transform.matrix(1, 0) * transform.matrix(1, 0));

					// Create temporary vector for text instances
					std::vector<TextInstanceData> textInstances;

					// Store layout info for debug box
					TextLayoutInfo layoutInfo;

					// Convert component alignment enums to font atlas enums
					TextAlignment hAlign = static_cast<TextAlignment>(textComp.alignment);
					VerticalAlignment vAlign = static_cast<VerticalAlignment>(textComp.verticalAlignment);

					// Generate text instances with alignment
					atlas.GenerateTextInstancesAligned(
						textComp.text,
						pos.x,
						pos.y,
						scaleX,
						textComp.color,
						textInstances,
						textComp.layer,
						0.0f, // rotation
						textComp.ySort,
						hAlign,
						vAlign,
						textComp.maxWidth,
						&layoutInfo,
						textComp.lineSpacing
					);

					// Convert TextInstanceData to InstanceData and add to main instances
					for (const auto& textInst : textInstances) {
						InstanceData inst{};
						inst.translation = textInst.translation;
						inst.rotation = textInst.rotation;
						inst.scale = textInst.scale;
						inst.colorAdd = textInst.colorAdd;
						inst.colorMultiply = textInst.colorMultiply;
						inst.uvBounds = textInst.uvBounds;
						inst.textureID = textInst.textureID;
						inst.useTexture = textInst.useTexture;
						inst.layer = textInst.layer;
						inst.ySort = textInst.ySort;
						inst.isFontTexture = 1;

						// Apply effect
						inst.effectType = effectType;
						inst.intensity = intensity;
						inst.effectColor = effectColor;
						inst.effectDuration = effectDuration;
						inst.loop = loop;
						inst.useEffect = useEffect;
						inst.effectStartTime = effectStartTime;


						instances.emplace_back(inst);
					}

					// Draw text bounding box for debugging (dotted line outline)
					if (showDebugAABBs || showDebugSquares) {
						// Extract bounding box info
						float boxX = layoutInfo.boundingBox.x;
						float boxY = layoutInfo.boundingBox.y;
						float boxWidth = layoutInfo.boundingBox.z;
						float boxHeight = layoutInfo.boundingBox.w;

						DrawTextBoundingBox(
							boxX,
							boxY,
							boxWidth,
							boxHeight,
							{ 1.0f, 0.0f, 1.0f, 1.0f } // magenta debug box
						);
					}

				}
				catch (const std::exception& e) {
					// Error handling 
					std::cout << "[error meshrender] enity " << ": " << e.what() << std::endl;
				}
			}
		)
	);

	if (instances.empty()) return;

	// Sort instance data
	std::sort(instances.begin(), instances.end(),
		[](const InstanceData& a, const InstanceData& b)
		{
			// Sort by layer (descending)
			if (a.layer != b.layer)
			{
				return a.layer < b.layer;
			}

			// Within same layer: ySort == 0 before ySort == 1
			if (a.ySort != b.ySort)
			{
				return a.ySort < b.ySort;
			}

			// Within same layer an ySort value:
			// If ySort == 1, sort by translation.y (decreasing)
			if (a.ySort == 1)
			{
				return a.translation.y > b.translation.y;
			}

			// For ySort == 0, order doesn't matter (stable sort preserves original order)
			return false;
		});

	GGMesh mesh = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetDefaultMesh();

	GGShader const& shader = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetDefaultShader();
	shader.Use();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	shader.SetMat4("projection", render.GetActiveCamera()->GetScreenProjectionMatrix());

	auto camera = Systems::GetSystemsRegistry()
		.GetSystem<Systems::RenderManager>().GetActiveCamera();

	glm::vec2 resScale = camera ? camera->GetResolutionScale() : glm::vec2(1.f);

	glm::mat4 zoomMatrix = glm::scale(glm::mat4(1.f), { resScale, 1.f });

	// Use identity matrix for UI objects
	shader.SetMat4("cameraView", zoomMatrix);

	shader.SetBool("uUseInstancing", true);
	shader.SetMat4(
		"worldscale",
		Systems::GetSystemsRegistry()
		.GetSystem<Systems::RenderManager>().GetDefaultWorldScale());

	shader.SetVec4("colorAdd", 0.f, 0.f, 0.f, 0.f);
	shader.SetVec4("colorMultiply", 1.f, 1.f, 1.f, 1.f);

	shader.SetFloat("uTime", (float)glfwGetTime());

	// Batch draw by texture — walk sorted instances, split at texture boundaries
	size_t i = 0;
	while (i < instances.size()) {
		uint32_t currentTex = instances[i].textureID;
		size_t batchStart = i;
		while (i < instances.size() && instances[i].textureID == currentTex) {
			++i;
		}
		size_t batchSize = i - batchStart;

		// Bind texture for this batch
		if (currentTex != 0) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, currentTex);
			shader.SetInt("uTexture", 0);
		}

		// Upload this batch's instances to the SSBO
		std::span<const InstanceData> batchSpan(instances.data() + batchStart, batchSize);
		mesh.UpdateInstanceData(batchSpan);

		glBindVertexArray(mesh.vaoid);
		glDrawElementsInstanced(
			mesh.primitiveType,
			mesh.drawCount,
			GL_UNSIGNED_SHORT,
			nullptr,
			static_cast<GLsizei>(batchSize)
		);
	}
	glBindVertexArray(0);
	shader.UnUse();
	// Clear instances after using
	instances.clear();
}

/*************************************************************************
  \brief Renders all MeshRenderer entities using instancing.
  \param world Reference to the ECS world.
  \details
	- Collects WorldTransform + MeshRenderer + optional Sprite data.
	- Sorts instances based on layer and Y-sort rules.
	- Uploads instance data to GPU and draws using the default shader.
*************************************************************************/
void GGMeshRendererSystem::RenderObjects(World& world)
{

	world.ForEach<WorldTransform, WorldTextComponent>(
		std::function<void(Entity, WorldTransform&, WorldTextComponent&)>(
			[&](Entity entity, WorldTransform& transform, WorldTextComponent& textComp) {

				(void)entity;
				if (!transform.isValid || !textComp.visible) return;

#ifdef GG_EDITOR_BUILD
				if (!GGLayerManager::Get().IsEnabled({ textComp.layer, LayerType::OBJECT }))
					return;
#endif

				GGMath::Vector2 pos = transform.matrix.getTranslation();

				float scaleX = sqrtf(transform.matrix(0, 0) * transform.matrix(0, 0) +
					transform.matrix(1, 0) * transform.matrix(1, 0));

				int   effectType = 0;
				float intensity = 1.f;
				glm::vec4 effectColor = glm::vec4(1.f);
				float effectDuration = 1.f;
				int   loop = 0;
				int   useEffect = 0;
				float effectStartTime = 0.f;

				if (world.HasComponent<ObjectEffectComponent>(entity)) {
					const ObjectEffectComponent* fx = world.GetComponent<ObjectEffectComponent>(entity);
					effectType = fx->effectType;
					intensity = fx->intensity;
					effectColor = fx->color;
					effectDuration = fx->effectDuration;
					loop = fx->loop ? 1 : 0;
					useEffect = 1;
					if (fx->effectType != ObjectEffectComponent::EffectType::NONE) {
						if (fx->startTime == 0.f)
							const_cast<ObjectEffectComponent*>(fx)->startTime = (float)glfwGetTime();
					}
					else {
						if (fx->startTime != 0.f)
							const_cast<ObjectEffectComponent*>(fx)->startTime = 0.f;
					}
					effectStartTime = fx->startTime;
				}


				try {
					std::string fontPath = textComp.getFontPath();
					if (fontPath.empty()) {
						fontPath = "Fonts/SourceSans3-Bold.ttf";
					}

					auto& atlas = GGFontAtlasManager::GetFontAtlas(fontPath, textComp.fontSize);

					std::vector<TextInstanceData> textInstances;
					TextLayoutInfo layoutInfo;

					// Use same GenerateTextInstancesAligned as TextComponent
					atlas.GenerateTextInstancesAligned(
						textComp.text,
						pos.x,
						pos.y,
						scaleX,
						textComp.color,
						textInstances,
						textComp.layer,
						0.0f,           // rotation
						textComp.ySort,
						TextAlignment::CENTER,          // center aligned on the tile
						VerticalAlignment::BASELINE,
						0.0f,           // no max width
						&layoutInfo,
						textComp.lineSpacing
					);

					// Push into main instances buffer - will be drawn with world projection
					for (const auto& textInst : textInstances) {
						InstanceData inst{};
						inst.translation = textInst.translation;
						inst.rotation = textInst.rotation;
						inst.scale = textInst.scale;
						inst.colorAdd = textInst.colorAdd;
						inst.colorMultiply = textInst.colorMultiply;
						inst.uvBounds = textInst.uvBounds;
						inst.textureID = textInst.textureID;
						inst.useTexture = textInst.useTexture;
						inst.layer = textComp.layer;
						inst.ySort = textComp.ySort ? 1 : 0;
						inst.isFontTexture = 1;

						// Apply effect
						inst.effectType = effectType;
						inst.intensity = intensity;
						inst.effectColor = effectColor;
						inst.effectDuration = effectDuration;
						inst.loop = loop;
						inst.useEffect = useEffect;
						inst.effectStartTime = effectStartTime;


						instances.emplace_back(inst);
					}
				}
				catch (const std::exception& e) {
					std::cout << "[WorldTextComponent] render error: " << e.what() << std::endl;
				}
			}
		)
	);
	// Gather instance data
	world.ForEach<WorldTransform, MeshRenderer>(
		std::function<void(Entity, WorldTransform&, MeshRenderer&)>(
			[&](Entity entity, WorldTransform& transform, MeshRenderer& renderer) {

				if (!transform.isValid)
					return; // skip entities not updated yet

#ifdef GG_EDITOR_BUILD
				if (!GGLayerManager::Get().IsEnabled({ renderer.layer, LayerType::OBJECT }))
					return;
#endif

				// Extract world position, rotation, and scale
				GGMath::Vector2 worldPos = transform.matrix.getTranslation();

				/*float rot = atan2f(transform.matrix(1, 0), transform.matrix(0, 0));
				float scaleX = sqrtf(transform.matrix(0, 0) * transform.matrix(0, 0) + transform.matrix(1, 0) * transform.matrix(1, 0));
				float scaleY = sqrtf(transform.matrix(0, 1) * transform.matrix(0, 1) + transform.matrix(1, 1) * transform.matrix(1, 1));*/

				float scaleX = sqrtf(transform.matrix(0, 0) * transform.matrix(0, 0) +
					transform.matrix(1, 0) * transform.matrix(1, 0));
				float scaleY = sqrtf(transform.matrix(0, 1) * transform.matrix(0, 1) +
					transform.matrix(1, 1) * transform.matrix(1, 1));

				// Check for reflection (negative determinant)
				float det = transform.matrix(0, 0) * transform.matrix(1, 1) -
					transform.matrix(0, 1) * transform.matrix(1, 0);
				if (det < 0.0f) {
					scaleX = -scaleX; // Preserve the sign
				}

				float rot = atan2f(transform.matrix(1, 0), transform.matrix(0, 0));

				// fill instance data
				InstanceData data{};
				data.translation = glm::vec3(worldPos.x, worldPos.y, renderer.perspectiveFactor);
				data.rotation = rot;
				data.scale = glm::vec3(scaleX, scaleY, 1.0f);
				data.colorAdd = renderer.colorAdd;
				data.colorMultiply = renderer.colorMultiply;

				// Higher layers render first
				data.layer = renderer.layer;

				// Enable/disable ySort
				data.ySort = renderer.ySort ? 1 : 0;
				if (world.HasComponent<ObjectEffectComponent>(entity)) {
					const ObjectEffectComponent* fx = world.GetComponent<ObjectEffectComponent>(entity);
					data.effectType = fx->effectType;
					data.intensity = fx->intensity;

					data.effectColor = fx->color;
					data.effectDuration = fx->effectDuration;
					data.loop = fx->loop ? 1 : 0;
					data.useEffect = 1;
					if (fx->effectType != ObjectEffectComponent::EffectType::NONE) {
						// Re-stamp if this is a fresh activation (startTime == 0)
						if (fx->startTime == 0.f) {
							const_cast<ObjectEffectComponent*>(fx)->startTime = (float)glfwGetTime();
						}
					}
					else {
						// Effect is NONE � reset so next activation gets a fresh stamp
						if (fx->startTime != 0.f) {
							const_cast<ObjectEffectComponent*>(fx)->startTime = 0.f;
						}
					}
					data.effectStartTime = fx->startTime;
				}
				else {
					data.effectType = 0;
					data.intensity = 1.f;

					data.effectColor = glm::vec4(1.f);
					data.effectDuration = 1.f;
					data.loop = 0;
					data.useEffect = 0;
				}
				if (world.HasComponent<Sprite>(entity)) {
					const Sprite* sprite = world.GetComponent<Sprite>(entity);

					data.textureID = sprite->GetTextureID();
					data.uvBounds = sprite->textureUVBounds;
					data.useTexture = (data.textureID != 0) ? 1 : 0;

					instances.emplace_back(data);
				}
				else {
					data.useTexture = 0;

					instances.emplace_back(data);
				}


			}
		)
	);

	if (instances.empty()) return;

	// Sort instance data
	std::sort(instances.begin(), instances.end(),
		[](const InstanceData& a, const InstanceData& b)
		{
			// Sort by layer (descending)
			if (a.layer != b.layer)
			{
				return a.layer < b.layer;
			}

			// Within same layer: ySort == 0 before ySort == 1
			if (a.ySort != b.ySort)
			{
				return a.ySort < b.ySort;
			}

			// Within same layer an ySort value:
			// If ySort == 1, sort by translation.y (decreasing)
			if (a.ySort == 1)
			{
				return a.translation.y > b.translation.y;
			}

			// For ySort == 0, order doesn't matter (stable sort preserves original order)
			return false;
		});

	GGMesh mesh = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetDefaultMesh();

	// 3. Bind shader once
	GGShader const& shader = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetDefaultShader();

	shader.Use();

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	shader.SetMat4("projection", Systems::GetSystemsRegistry()
		.GetSystem<Systems::RenderManager>()
		.GetActiveProjection());

	shader.SetMat4("cameraView", Systems::GetSystemsRegistry()
		.GetSystem<Systems::RenderManager>().GetActiveView());

	shader.SetBool("uUseInstancing", true);
	shader.SetMat4(
		"worldscale",
		Systems::GetSystemsRegistry()
		.GetSystem<Systems::RenderManager>().GetDefaultWorldScale());

	shader.SetVec4("colorAdd", 0.f, 0.f, 0.f, 0.f);
	shader.SetVec4("colorMultiply", 1.f, 1.f, 1.f, 1.f);
	shader.SetFloat("uTime", (float)glfwGetTime());

	// Batch draw by texture — walk sorted instances, split at texture boundaries
	size_t i = 0;
	while (i < instances.size()) {
		uint32_t currentTex = instances[i].textureID;
		size_t batchStart = i;
		while (i < instances.size() && instances[i].textureID == currentTex) {
			++i;
		}
		size_t batchSize = i - batchStart;

		// Bind texture for this batch
		if (currentTex != 0) {
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, currentTex);
			shader.SetInt("uTexture", 0);
		}

		// Upload this batch's instances to the SSBO
		std::span<const InstanceData> batchSpan(instances.data() + batchStart, batchSize);
		mesh.UpdateInstanceData(batchSpan);

		glBindVertexArray(mesh.vaoid);
		glDrawElementsInstanced(
			mesh.primitiveType,
			mesh.drawCount,
			GL_UNSIGNED_SHORT,
			nullptr,
			static_cast<GLsizei>(batchSize)
		);
	}
	glBindVertexArray(0);
	shader.UnUse();
	// Clear instances after using
	instances.clear();
}

void  GGMeshRendererSystem::SampleTextTest(World& world) {



	const int TEXT_COUNT = 100;
	const float START_X = -5.0f;
	const float START_Y = 0.0f;
	const float X_SPACING = 0.4f;

	const int FONT_SIZES[] = { 10,12,34,65,70,46,11, 24, 32,51,14,45,76,22,33,36, 48, 60, 72 };
	const int FONT_SIZE_COUNT = sizeof(FONT_SIZES) / sizeof(int);

	for (int i = 0; i < TEXT_COUNT; ++i)
	{
		Entity worldText = world.CreateEntity();

		WorldTransform worldXform{};
		float x = START_X + i * X_SPACING;
		float y = START_Y;

		worldXform.matrix =
			GGMath::Matrix3x3::translation(x, y) *
			GGMath::Matrix3x3::scale(1.0f, 1.0f);
		worldXform.isValid = true;

		world.AddComponent(worldText, worldXform);

		TextComponent text{};
		snprintf(text.text, sizeof(text.text),
			"HELLO font=%d %02d akjf kajdkf jakdsjfkajf ;lasdjf lkajdfasdf",
			FONT_SIZES[i % FONT_SIZE_COUNT], i + 1);

		text.fontSize = FONT_SIZES[i % FONT_SIZE_COUNT];
		text.color = { 0.0f, 0.0f, 0.0f ,1.f };
		text.layer = 5;
		text.visible = true;

		world.AddComponent(worldText, text);
	}

	std::cout << "Created " << TEXT_COUNT << " text entities\n";

}
