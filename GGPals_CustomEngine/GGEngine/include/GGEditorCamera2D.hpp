/*************************************************************************
  \file       GGEditorCamera2D.hpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       7 November 2025
  \version    1.0
  \brief
    Editor-specific 2D camera system for GGEngine.
  \details
    Extends GGCamera2D to provide editor functionality such as
    panning, zooming, smooth following of entities, framing selection,
    framing all entities, and handling mouse and scroll input.
    Supports adjustable pan/zoom speeds, smooth movement, and bounds
    constraints for editor viewport control.
*************************************************************************/
#pragma once

#include <glm/glm.hpp>
#include <GGCamera2D.hpp>
#include <GGECSWorld.hpp>
#include <GGEngine.hpp>

using EntityID = Entity;

/*************************************************************************
  \class GGEditorCamera2D
  \brief Editor-specific 2D camera extending GGCamera2D.
  \details
    Provides additional features for editor use, including smooth
    entity following, framing, mouse and scroll input handling,
    and adjustable camera settings for pan, zoom, and smooth movement.
*************************************************************************/

class GGEditorCamera2D : public GGCamera2D {
public:
    GGEditorCamera2D();

    /*************************************************************************
      \brief Update camera every frame.
      \param deltaTime Time elapsed since last frame.
      \details
        Updates target position if following an entity and interpolates
        smoothly towards targets.
    *************************************************************************/
    void Update(float deltaTime);

    /*************************************************************************
      \brief Handle mouse input for panning or orbiting.
      \param xOffset Mouse movement along X.
      \param yOffset Mouse movement along Y.
      \param isPanning Whether panning is active.
      \param isOrbiting Whether orbiting is active (unused for 2D camera).
    *************************************************************************/
    void ProcessMouseInput(float xOffset, float yOffset, bool isPanning, bool isOrbiting);

    /*************************************************************************
      \brief Handle scroll input for zooming.
      \param delta Scroll delta value.
    *************************************************************************/
    void ProcessScrollInput(float delta);

    // Editor features
    void SetTarget(EntityID entityID) { m_followTarget = entityID; }
    void ClearTarget() {
        m_followTarget = Entity{ 0,0 }; 
    }
    EntityID GetTarget() const { return m_followTarget; }

    /*************************************************************************
      \brief Frame the camera to fit the current selection.
      \details
        Calculates bounding rectangle of all selected entities and
        sets camera position and zoom accordingly.
    *************************************************************************/
    void FrameSelection();

    /*************************************************************************
      \brief Frame the camera to include all entities in the world.
    *************************************************************************/
    void FrameAll();
    // Camera settings
    void SetPanSpeed(float speed) { m_panSpeed = speed; }
    void SetZoomSpeed(float speed) { m_zoomSpeed = speed; }
    void SetSmoothMovement(bool enabled) { m_smoothMovement = enabled; }
    void SetSmoothingSpeed(float speed) { m_smoothingSpeed = speed; }

    float GetPanSpeed() const { return m_panSpeed; }
    float GetZoomSpeed() const { return m_zoomSpeed; }
    bool IsSmoothMovementEnabled() const { return m_smoothMovement; }

    /*************************************************************************
      \brief Reset camera to default position and zoom.
    *************************************************************************/
    void Reset();

    /*************************************************************************
      \brief Immediately set camera position without interpolation.
      \param pos Target position in world coordinates.
    *************************************************************************/
    void SetPositionImmediate(const glm::vec2& pos);

    /*************************************************************************
      \brief Immediately set camera zoom without interpolation.
      \param zoom Target zoom factor.
    *************************************************************************/
    void SetZoomImmediate(float zoom);
private:
    EntityID m_followTarget = NULL_ENTITY;


    float m_panSpeed = 3.0f;        // Units per second at zoom 1.0
    float m_zoomSpeed = 0.1f;         // Zoom factor per scroll tick
    bool m_smoothMovement = true;
    float m_smoothingSpeed = 8.0f;

    // Smooth movement targets
    glm::vec2 m_targetPosition{ 0.0f };
    float m_targetZoom = 64.0f;

    /*************************************************************************
      \brief Smoothly interpolate camera position and zoom towards targets.
      \param deltaTime Time elapsed since last frame.
    *************************************************************************/
    void UpdateSmoothMovement(float deltaTime);

    /*************************************************************************
      \brief Calculate bounds of selected entities for framing.
      \return Bounding rectangle (x_min, y_min, width, height).
    *************************************************************************/
    glm::vec4 CalculateSelectionBounds();
};
