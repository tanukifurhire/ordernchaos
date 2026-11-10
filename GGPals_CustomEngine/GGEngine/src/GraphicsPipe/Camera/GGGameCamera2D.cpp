/*************************************************************************
  \file       GGGameCamera2D.cpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       7 November 2025
  \version    1.0
  \brief
    Implementation of the GGGameCamera2D class for GGEngine.
  \details
    Provides game-specific 2D camera functionality, including:
    - Smooth following of entities with optional offsets.
    - Dead zone camera logic to prevent jittering.
    - Screen shake effects with decay over time.
    - Integration with GGCamera2D base class for 2D orthographic projection.
    - Utility functions to retrieve entity positions from the ECS world.
*************************************************************************/
#include "GGGameCamera2D.hpp"
#include <GGECSComponents.hpp>
#include <GGEngine.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <random>


/*************************************************************************
  \brief Get the position of an entity in world space.
  \param entityID ID of the entity.
  \return Position of the entity or (0,0) if invalid or missing transform.
*************************************************************************/
// Helper function to get entity position from the world
glm::vec2 GGGameCamera2D::GetEntityPosition(EntityID entityID) const {
    auto& world = GGEngine::GetEngine().GetActiveWorld();
    if (!entityID.IsValid() || !world.IsEntityAlive(entityID))
        return glm::vec2(0.f);

    if (!world.HasComponent<LocalTransform>(entityID))
        return glm::vec2(0.f);

    if (auto* transform = world.GetComponent<LocalTransform>(entityID))
        return glm::vec2(transform->position.x, transform->position.y);

    return glm::vec2(0.f);
}

// Constructor
/*************************************************************************
  \brief Constructor for GGGameCamera2D.
  \details Initializes base camera position for shake and follow effects.
*************************************************************************/
GGGameCamera2D::GGGameCamera2D()
{
    m_basePosition = m_position;
    // Initialize with default game camera settings
}

// Main update function - call this every frame

/*************************************************************************
  \brief Update camera each frame.
  \param deltaTime Time elapsed since last frame.
*************************************************************************/
void GGGameCamera2D::Update(float deltaTime)
{
    // Update camera follow behavior first
    UpdateFollow(deltaTime);

    // Capture base position AFTER follow so shake is relative to current position
    m_basePosition = m_position;

    // Update screen shake effect
    UpdateShake(deltaTime);

    // Apply shake offset to camera position (non-destructive)
    if (m_isShaking) {
        glm::vec2 finalPos = m_basePosition + m_shakeOffset;
        // Temporarily set position for rendering
        m_position = finalPos;
       
    }
    MarkDirty();
}

// Follow an entity with the camera
/*************************************************************************
  \brief Follow a specific entity immediately.
  \param entityID Entity to follow.
  \param deltaTime Time elapsed since last frame.
*************************************************************************/
void GGGameCamera2D::FollowEntity(EntityID entityID, float deltaTime)
{
    if (entityID.IsValid()) {
        m_followTargetID = entityID;
        UpdateFollow(deltaTime);
    }
}

// Update camera following behavior
/*************************************************************************
  \brief Update following behavior.
  \param deltaTime Time elapsed since last frame.
*************************************************************************/
void GGGameCamera2D::UpdateFollow(float deltaTime)
{
    if (m_followTargetID == NULL_ENTITY || !m_followTargetID.IsValid())
        return;

    glm::vec2 targetPos = GetEntityPosition(m_followTargetID);

    // Apply follow offset
    targetPos += m_followOffset;

    // Check if target is outside dead zone
    glm::vec2 deadZoneCenter(m_deadZone.x + m_deadZone.z / 2.0f,
        m_deadZone.y + m_deadZone.w / 2.0f);
    glm::vec2 deadZoneHalfSize(m_deadZone.z / 2.0f, m_deadZone.w / 2.0f);

    // Calculate offset from camera center to target
    glm::vec2 offset = targetPos - m_position;

    // Only move camera if target is outside dead zone
    glm::vec2 movement(0.0f);

    if (std::abs(offset.x) > deadZoneHalfSize.x) {
        float sign = offset.x > 0 ? 1.0f : -1.0f;
        movement.x = sign * (std::abs(offset.x) - deadZoneHalfSize.x);
    }

    if (std::abs(offset.y) > deadZoneHalfSize.y) {
        float sign = offset.y > 0 ? 1.0f : -1.0f;
        movement.y = sign * (std::abs(offset.y) - deadZoneHalfSize.y);
    }

    // Smooth follow using exponential decay
    float t = 1.0f - std::exp(-m_followSpeed * deltaTime);
    t = std::clamp(t, 0.0f, 1.0f);

    m_position += movement * t;

    // Apply camera bounds
    ApplyBounds();
    MarkDirty();
}

/*************************************************************************
  \brief Start a screen shake effect.
  \param intensity Shake intensity.
  \param duration Shake duration in seconds.
*************************************************************************/
// Start a screen shake effect
void GGGameCamera2D::StartShake(float intensity, float duration)
{
    m_isShaking = true;
    m_shakeIntensity = intensity;
    m_shakeDuration = duration;
    m_shakeTimer = 0.0f;
}

/*************************************************************************
  \brief Update screen shake effect.
  \param deltaTime Time elapsed since last frame.
*************************************************************************/
// Update screen shake effect
void GGGameCamera2D::UpdateShake(float deltaTime)
{
    if (!m_isShaking)
        return;

    m_shakeTimer += deltaTime;

    if (m_shakeTimer >= m_shakeDuration) {
        m_isShaking = false;
        m_shakeOffset = glm::vec2(0.0f);
        m_position = m_basePosition; // restore original
        return;
    }

    // Calculate decay factor (shake weakens over time)
    float decay = 1.0f - (m_shakeTimer / m_shakeDuration);

    // Generate new shake offset
    m_shakeOffset = GenerateShakeOffset() * decay;
}


/*************************************************************************
  \brief Generate a random 2D offset for shake effect.
  \return Offset vector.
*************************************************************************/
// Generate random shake offset
glm::vec2 GGGameCamera2D::GenerateShakeOffset() const
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    // Random direction with consistent intensity
    float angle = dis(gen) * glm::pi<float>();
    float magnitude = m_shakeIntensity;

    return glm::vec2(
        std::cos(angle) * magnitude,
        std::sin(angle) * magnitude
    );
}