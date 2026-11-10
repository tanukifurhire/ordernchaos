/*************************************************************************
  \file       GGEditorCamera2D.cpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       7 November 2025
  \version    1.0
  \brief
    Implementation of the GGEditorCamera2D class for GGEngine.
  \details
    Extends GGCamera2D to provide editor-specific functionality, including
    smooth following of entities, panning, zooming, framing selection,
    framing all entities, and handling mouse/scroll input.
*************************************************************************/
#include "GGEditorCamera2D.hpp"
#include <GGECSComponents.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <GGEngine.hpp>
//helper functions

using EntityID = Entity;

/*************************************************************************
  \brief Get world position of an entity.
  \param entityID ID of the entity.
  \return 2D position in world coordinates.
*************************************************************************/
static glm::vec2 GetEntityPosition(EntityID entityID) {
    auto& world = GGEngine::GetEngine().GetActiveWorld();
    if (!entityID.IsValid() || !world.IsEntityAlive(entityID)) return { 0.f, 0.f };
    if (!world.HasComponent<LocalTransform>(entityID)) return { 0.f, 0.f };

    if (auto* transform = world.GetComponent<LocalTransform>(entityID))
        return { transform->position.x, transform->position.y };

   // return { 0.f, 0.f };
   return { 0.f, 0.f };
}

/*************************************************************************
  \brief Get world-space bounds of an entity.
  \param entityID ID of the entity.
  \return Bounding rectangle (x_min, y_min, width, height).
*************************************************************************/
static glm::vec4 GetEntityBounds(EntityID entityID) {
    auto& world = GGEngine::GetEngine().GetActiveWorld();
    if (!entityID.IsValid() || !world.IsEntityAlive(entityID))
        return glm::vec4(0.f);

    if (auto* transform = world.GetComponent<LocalTransform>(entityID)) {
        float halfW = 0.5f * transform->scale.x;
        float halfH = 0.5f * transform->scale.y;
        return glm::vec4(
            transform->position.x - halfW, transform->position.y - halfH,
            transform->position.x + halfW, transform->position.y + halfH
        );
    }
    return glm::vec4(0.f);
}

/*************************************************************************
  \brief Retrieve all living entities in the current world.
  \return Vector of all entity IDs.
*************************************************************************/
static std::vector<EntityID> GetAllEntities() {
    std::vector<EntityID> allEntities;
    auto& world = GGEngine::GetEngine().GetActiveWorld();

    for (const Entity& entity : world.GetLivingEntities()) {
        if (entity.IsValid())
            allEntities.push_back(entity);
    }
    return allEntities;
}

/*************************************************************************
  \brief Constructor for GGEditorCamera2D.
  \details
    Initializes camera position and zoom targets for smooth movement.
*************************************************************************/
// Constructor
GGEditorCamera2D::GGEditorCamera2D()
{
    m_targetPosition = m_position;
    m_targetZoom = m_zoom;
}

/*************************************************************************
  \brief Update camera every frame.
  \param deltaTime Time elapsed since last frame.
  \details
    Updates target position if following an entity and interpolates
    smoothly towards targets.
*************************************************************************/
// Call this every frame
void GGEditorCamera2D::Update(float deltaTime)
{
    // Follow target if set
    if (m_followTarget != NULL_ENTITY)
    {
        // TODO: Replace this with actual entity position retrieval
        glm::vec2 targetPos = GetEntityPosition(m_followTarget);
        m_targetPosition = targetPos;
       
    }

    UpdateSmoothMovement(deltaTime);
}

/*************************************************************************
  \brief Handle mouse input for panning or orbiting.
  \param xOffset Mouse movement along X.
  \param yOffset Mouse movement along Y.
  \param isPanning Whether panning is active.
  \param isOrbiting Whether orbiting is active (unused for 2D camera).
*************************************************************************/
// Handle mouse input for panning/orbiting
void GGEditorCamera2D::ProcessMouseInput(float xOffset, float yOffset, bool isPanning, bool /*isOrbiting*/)
{
    if (isPanning)
    {
        ClearTarget();
        // Move camera opposite to mouse drag (typical pan)
        glm::vec2 delta(-xOffset, yOffset); // flip x for natural pan
        delta *= (m_panSpeed/m_zoom);      // scale by speed and current zoom
        m_targetPosition += delta;

        if (!m_smoothMovement) {
            m_position = m_targetPosition;
        }


        MarkDirty();
    }
   // std::cout << "scorling\n";
   
}

/*************************************************************************
  \brief Handle scroll input for zooming.
  \param delta Scroll delta value.
*************************************************************************/
// Handle scroll input for zoom
void GGEditorCamera2D::ProcessScrollInput(float delta)
{
    m_targetZoom *= (1.0f + delta * m_zoomSpeed);
    m_targetZoom = std::clamp(m_targetZoom, m_minZoom, m_maxZoom);
    if (!m_smoothMovement) {
        m_zoom = m_targetZoom;
    }
    MarkDirty();
}

/*************************************************************************
  \brief Smoothly interpolate camera position and zoom towards targets.
  \param deltaTime Time elapsed since last frame.
*************************************************************************/
// Smoothly interpolate towards target position and zoom
void GGEditorCamera2D::UpdateSmoothMovement(float deltaTime)
{
    if (m_smoothMovement) {
        // Exponential smoothing for natural feel
        float t = 1.0f - std::exp(-m_smoothingSpeed * deltaTime);
        t = std::clamp(t, 0.0f, 1.0f);

        m_position = glm::mix(m_position, m_targetPosition, t);
        m_zoom = glm::mix(m_zoom, m_targetZoom, t);
    }
    else {
        // Instant movement
        m_position = m_targetPosition;
        m_zoom = m_targetZoom;
    }

    ApplyBounds();
    MarkDirty();
}

/*************************************************************************
  \brief Frame the camera to fit the current selection.
  \details
    Calculates bounding rectangle of all selected entities and
    sets camera position and zoom accordingly.
*************************************************************************/
// Focus camera to fit a selected entity
void GGEditorCamera2D::FrameSelection()
{
    std::vector<EntityID> selectedEntities = GetAllEntities();

    if (selectedEntities.empty()) {
        return; // Nothing to frame
    }

    // Stop following any target
    ClearTarget();

    // Calculate bounds of selection
    glm::vec4 bounds = CalculateSelectionBounds();

    // Use base class method to focus on bounds
    FocusOnBounds(bounds);

    // Update targets to match
    m_targetPosition = m_position;
    m_targetZoom = m_zoom;
}

/*************************************************************************
  \brief Frame the camera to include all entities in the world.
*************************************************************************/

// Focus camera to fit all entities
void GGEditorCamera2D::FrameAll()
{
    std::vector<EntityID> allEntities = GetAllEntities();

    if (allEntities.empty()) {
        // No entities, just reset to default view
        Reset();
        return;
    }

    // Stop following any target
    ClearTarget();

    // Calculate bounds of all entities
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();

    for (EntityID entity : allEntities) {
        glm::vec4 bounds = GetEntityBounds(entity);
        float left = bounds.x;
        float bottom = bounds.y;
        float right = bounds.x + bounds.z;
        float top = bounds.y + bounds.w;

        minX = std::min(minX, left);
        minY = std::min(minY, bottom);
        maxX = std::max(maxX, right);
        maxY = std::max(maxY, top);
    }

    // Add padding (10% of size)
    float width = maxX - minX;
    float height = maxY - minY;
    float padding = std::max(width, height) * 0.1f;

    glm::vec4 worldBounds(
        minX - padding,
        minY - padding,
        width + padding * 2.0f,
        height + padding * 2.0f
    );

    // Use base class method to focus on bounds
    FocusOnBounds(worldBounds);

    // Update targets to match
    m_targetPosition = m_position;
    m_targetZoom = m_zoom;
}


/*************************************************************************
  \brief Reset camera to default position and zoom.
*************************************************************************/
void GGEditorCamera2D::Reset()
{
    m_position = glm::vec2(0.0f);
    m_zoom = 64.0f;
    m_rotation = 0.0f;
    
    m_targetPosition = { 0.0f,0.0f };
    m_targetZoom = m_zoom;
    ClearTarget();
    MarkDirty();
}

/*************************************************************************
  \brief Calculate bounds of selected entities for framing.
  \return Bounding rectangle (x_min, y_min, width, height).
*************************************************************************/
// Calculate selection bounds for the camera to frame
glm::vec4 GGEditorCamera2D::CalculateSelectionBounds()
{
    std::vector<EntityID> selectedEntities = GetAllEntities();

    if (selectedEntities.empty()) {
        // Default bounds if nothing selected
        return glm::vec4(-100.0f, -100.0f, 200.0f, 200.0f);
    }

    // Find min/max extents of all selected entities
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();

    for (EntityID entity : selectedEntities) {
        glm::vec4 bounds = GetEntityBounds(entity);
        float left = bounds.x;
        float bottom = bounds.y;
        float right = bounds.x + bounds.z;
        float top = bounds.y + bounds.w;

        minX = std::min(minX, left);
        minY = std::min(minY, bottom);
        maxX = std::max(maxX, right);
        maxY = std::max(maxY, top);
    }

    // Add padding (10% of size)
    float width = maxX - minX;
    float height = maxY - minY;
    float padding = std::max(width, height) * 0.1f;

    return glm::vec4(
        minX - padding,
        minY - padding,
        width + padding * 2.0f,
        height + padding * 2.0f
    );

  
}

/*************************************************************************
  \brief Immediately set camera position without interpolation.
  \param pos Target position in world coordinates.
*************************************************************************/
void GGEditorCamera2D::SetPositionImmediate(const glm::vec2& pos)
{
    m_position = pos;
    m_targetPosition = pos;
    MarkDirty();
}


/*************************************************************************
  \brief Immediately set camera zoom without interpolation.
  \param zoom Target zoom factor.
*************************************************************************/
void GGEditorCamera2D::SetZoomImmediate(float zoom)
{
    m_zoom = glm::clamp(zoom, m_minZoom, m_maxZoom);
    m_targetZoom = m_zoom;
    MarkDirty();
}
