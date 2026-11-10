#pragma once
/*************************************************************************
  \file       GGGameCamera2D.hpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       7 November 2025
  \version    1.0
  \brief
    Game-specific 2D camera for GGEngine.
  \details
    Extends GGCamera2D to provide game functionality including
    entity following, dead zone camera movement, screen shake effects,
    and adjustable follow parameters. Designed for real-time gameplay
    camera control in 2D games.
*************************************************************************/
#include <glm/glm.hpp>
#include <GGCamera2D.hpp>
#include <GGECSWorld.hpp>

using EntityID = Entity;

/*************************************************************************
  \class GGGameCamera2D
  \brief 2D camera specialized for gameplay.
  \details
    Provides features for gameplay camera control such as following
    an entity, smoothing, dead zone handling, and screen shake effects.
*************************************************************************/
class GGGameCamera2D : public GGCamera2D{
public:
    GGGameCamera2D();

    // Game-specific functionality
    void Update(float deltaTime);
    void FollowEntity(EntityID entityID, float deltaTime);
    void SetFollowTarget(EntityID entityID) { m_followTargetID = entityID; }
    void ClearFollowTarget() { m_followTargetID = NULL_ENTITY; }

    // Camera effects
    void StartShake(float intensity, float duration);
    void SetDeadZone(glm::vec4 deadZone) { m_deadZone = deadZone; }
    void Reset() {
        m_position = glm::vec2(0.0f, 0.0f);
        m_zoom = 64.0f;
        ClearFollowTarget();
        MarkDirty();
    }
    glm::vec4 GetDeadZone() const { return m_deadZone; }
private:
    // Follow system
    EntityID m_followTargetID = NULL_ENTITY;
    glm::vec2 m_followOffset{0.0f};
    float m_followSpeed = 5.0f;
    glm::vec4 m_deadZone{0, 0, 100, 100}; // Dead zone for follow camera

    // Screen shake
    bool m_isShaking = false;
    float m_shakeIntensity = 0.0f;
    float m_shakeDuration = 0.0f;
    float m_shakeTimer = 0.0f;
    glm::vec2 m_shakeOffset{0.0f};

    // In your camera class
    glm::vec2 m_basePosition;

    void UpdateShake(float deltaTime);
    void UpdateFollow(float deltaTime);
    glm::vec2 GenerateShakeOffset() const;
    glm::vec2 GetEntityPosition(EntityID entityID) const;
};