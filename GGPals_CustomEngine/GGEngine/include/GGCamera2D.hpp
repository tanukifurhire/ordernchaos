#pragma once
/*************************************************************************
  \file       GGCamera2D.hpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       7 November 2025
  \version    1.0
  \brief
    2D camera system for GGEngine with orthographic projection.
  \details
    Integrates with existing GGEngineRender projection matrix system
    and provides 2D camera functionality for editor and game views.
*************************************************************************/

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdint>
#include <glm/glm.hpp>

/*************************************************************************
  \class GGCamera2D
  \brief 2D orthographic camera class for GGEngine.
  \details
    Provides camera position, zoom, rotation, and viewport handling.
    Supports panning, zooming, focusing on points or bounds, and matrix
    caching for efficient view/projection updates.
*************************************************************************/
class GGCamera2D {
public:
    static constexpr float DEFAULT_UI_XSCALE = 1.f / 1600.f;
    static constexpr float DEFAULT_UI_YSCALE = 1.f / 900.f;

    GGCamera2D(glm::vec2 position = glm::vec2(0.0f), float zoom = 64.0f);
    virtual ~GGCamera2D() = default;

    // Core matrix functions integrating with GGEngineRender
    glm::mat4 GetProjectionMatrix();
    glm::mat4 GetScreenProjectionMatrix() const;

    glm::mat3 GetViewMatrix2D();
    glm::mat4 GetViewMatrix4D(); // For integration with existing 4x4 system

    // Camera properties
    void SetPosition(glm::vec2 position) { m_position = position; MarkDirty(); }
    void SetZoom(float zoom) { m_zoom = glm::clamp(zoom, m_minZoom, m_maxZoom); MarkDirty(); }
    void SetRotation(float rotation) { m_rotation = rotation; MarkDirty(); }
    void SetViewportSize(glm::vec2 size) { m_viewportSize = size; MarkDirty(); }

    glm::vec2 GetPosition() const { return m_position; }
    float GetZoom() const { return m_zoom; }
    float GetRotation() const { return m_rotation; }
    glm::vec2 GetViewportSize() const { return m_viewportSize; }

    glm::vec2 GetResolutionScale() const 
    {
        return m_viewportSize * glm::vec2(DEFAULT_UI_XSCALE, DEFAULT_UI_YSCALE);
    }

    // Camera controls
    void Pan(glm::vec2 delta);
    void ZoomIn(float factor = 1.1f);
    void ZoomOut(float factor = 0.9f);
    void FocusOnPoint(glm::vec2 target);
    void FocusOnBounds(glm::vec4 bounds); // x, y, width, height

    // Bounds and constraints
    void SetBounds(glm::vec4 bounds) { m_bounds = bounds; m_hasBounds = true; }
    void RemoveBounds() { m_hasBounds = false; }
    void SetZoomLimits(float minZoom, float maxZoom) {
        m_minZoom = minZoom; m_maxZoom = maxZoom;
    }
    void ApplyBounds();

protected:
    glm::vec2 m_position{ 0.0f };
    float m_zoom = 64.f;
    float m_rotation = 0.0f;
    glm::vec2 m_viewportSize{ 1600.0f, 900.0f };

    // Constraints
    float m_minZoom = 1.f;
    float m_maxZoom = 640.0f;
    bool m_hasBounds = false;
    glm::vec4 m_bounds{ -1000, -1000, 2000, 2000 }; // x, y, width, height

    // Matrix caching
    mutable bool m_matrixDirty = true;
    mutable glm::mat3 m_cachedViewMatrix2D{ 1.0f };
    mutable glm::mat4 m_cachedViewMatrix4D{ 1.0f };
    mutable glm::mat4 m_cachedProjectionMatrix{ 1.0f };

    void MarkDirty() { m_matrixDirty = true; }
    void RecalculateMatrices();
};