/*************************************************************************
  \file       GGCamera2D.cpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       7 November 2025
  \version    1.0
  \brief
    Implementation of 2D orthographic camera for GGEngine.
  \details
    Provides methods for panning, zooming, focusing, rotation, and
    world-space bounds. Handles matrix caching for efficient rendering
    and supports both 3x3 and 4x4 view matrices for integration
    with editor and game renderers.
*************************************************************************/
#define GLM_ENABLE_EXPERIMENTAL

#include <GGCamera2D.hpp>
#include <glm/glm.hpp>                      
#include <glm/gtc/matrix_transform.hpp>     
#include <glm/gtx/transform.hpp>           
#include <glm/gtc/type_ptr.hpp>             
#include <glm/gtx/matrix_transform_2d.hpp>  
#include <algorithm>
#include <iostream>


/*************************************************************************
  \brief Constructor for GGCamera2D.
  \param position Initial camera position in world coordinates.
  \param zoom Initial zoom factor.
  \details
    Initializes the camera position, zoom level, and recalculates
    matrices.
*************************************************************************/
GGCamera2D::GGCamera2D(glm::vec2 position, float zoom)
    : m_position(position), m_zoom(zoom)
{
    RecalculateMatrices();
}

/*************************************************************************
  \brief Returns the camera's projection matrix.
  \return 4x4 orthographic projection matrix.
  \details
    Recalculates matrices if marked dirty.
*************************************************************************/
glm::mat4 GGCamera2D::GetProjectionMatrix()
{
    if (m_matrixDirty)
        RecalculateMatrices();
    return m_cachedProjectionMatrix;
}

glm::mat4 GGCamera2D::GetScreenProjectionMatrix() const
{
    float halfW = m_viewportSize.x * 0.5f;
    float halfH = m_viewportSize.y * 0.5f;
    return glm::ortho(-halfW, halfW, -halfH, halfH, -1.0f, 1.0f);
}

/*************************************************************************
  \brief Returns the 2D view matrix (3x3) for world-to-camera transformation.
  \return 3x3 view matrix.
*************************************************************************/
glm::mat3 GGCamera2D::GetViewMatrix2D()
{
    if (m_matrixDirty)
        RecalculateMatrices();
    return m_cachedViewMatrix2D;
}

/*************************************************************************
  \brief Returns the 4x4 view matrix for renderer integration.
  \return 4x4 view matrix.
*************************************************************************/
glm::mat4 GGCamera2D::GetViewMatrix4D()
{
    if (m_matrixDirty)
        RecalculateMatrices();
    return m_cachedViewMatrix4D;
}

/*************************************************************************
  \brief Pan the camera by a delta in world space.
  \param delta Movement vector in world units.
*************************************************************************/
void GGCamera2D::Pan(glm::vec2 delta)
{
    m_position += delta;
    ApplyBounds();
    MarkDirty();
}

/*************************************************************************
  \brief Zoom in the camera by a factor.
  \param factor Multiplicative zoom factor (default 1.1f).
*************************************************************************/
void GGCamera2D::ZoomIn(float factor)
{
    m_zoom *= factor;
    m_zoom = std::clamp(m_zoom, m_minZoom, m_maxZoom);
    MarkDirty();
}

/*************************************************************************
  \brief Zoom out the camera by a factor.
  \param factor Multiplicative zoom factor (default 0.9f).
*************************************************************************/
void GGCamera2D::ZoomOut(float factor)
{
    m_zoom *= factor;
    m_zoom = std::clamp(m_zoom, m_minZoom, m_maxZoom);
    MarkDirty();
}

/*************************************************************************
\brief Focus the camera on a specific world - space point.
\param target Position to center camera on.
* ************************************************************************/
void GGCamera2D::FocusOnPoint(glm::vec2 target)
{
    m_position = target;
    ApplyBounds();
    MarkDirty();
}

/*************************************************************************
  \brief Focus the camera on a bounding rectangle and adjust zoom.
  \param bounds Rectangle in world space (x, y, width, height).
  \details
    Computes the center of the bounds and sets the camera to that position.
    Adjusts zoom to fit the rectangle inside the current viewport.
*************************************************************************/
void GGCamera2D::FocusOnBounds(glm::vec4 bounds)
{
    SetBounds(bounds);
    glm::vec2 center(bounds.x + bounds.z / 2.0f, bounds.y + bounds.w / 2.0f);
    m_position = center;

    // Adjust zoom to fit the bounding box into the current viewport
    float zoomX = m_viewportSize.x / bounds.z;
    float zoomY = m_viewportSize.y / bounds.w;
    m_zoom = std::min(zoomX, zoomY);
    m_zoom = std::clamp(m_zoom, m_minZoom, m_maxZoom);
    
    MarkDirty();
}

/*************************************************************************
  \brief Recalculate view and projection matrices.
  \details
    Computes 3x3 and 4x4 view matrices and orthographic projection
    matrix based on camera position, zoom, rotation, and viewport.
*************************************************************************/
void GGCamera2D::RecalculateMatrices()
{
    // View matrix (camera transform inverse)
    glm::mat3 translation = glm::translate(glm::mat3(1.0f), -m_position);
    glm::mat3 rotation = glm::rotate(glm::mat3(1.0f), -m_rotation);
    glm::mat3 scale = glm::scale(glm::mat3(1.0f), glm::vec2(m_zoom));

    m_cachedViewMatrix2D = scale * rotation * translation;

    // Convert to 4x4 for renderer compatibility
    glm::mat4 view4(1.0f);
    view4 = glm::translate(view4, glm::vec3(-m_position, 0.0f));
    view4 = glm::rotate(view4, -m_rotation, glm::vec3(0, 0, 1));
    //view4 = glm::scale(view4, glm::vec3(m_zoom, m_zoom, 1.0f));
    m_cachedViewMatrix4D = view4;

    // Orthographic projection matrix (viewport in world space)
    float halfW = m_viewportSize.x * 0.5f / m_zoom;
    float halfH = m_viewportSize.y * 0.5f / m_zoom;
    m_cachedProjectionMatrix = glm::ortho(-halfW, halfW, -halfH, halfH, -1.0f, 1.0f);

    m_matrixDirty = false;
}

/*************************************************************************
  \brief Apply world-space camera bounds if enabled.
  \details
    Clamps camera position to ensure viewport remains inside specified
    world boundaries.
*************************************************************************/
void GGCamera2D::ApplyBounds()
{
    if (!m_hasBounds)
        return;

    FocusOnBounds(m_bounds);

    // Half of the visible world size (in world units)
    float halfW = (m_viewportSize.x * 0.5f) / m_zoom;
    float halfH = (m_viewportSize.y * 0.5f) / m_zoom;

    // World boundaries
    float minX = m_bounds.x + halfW;
    float maxX = m_bounds.x + m_bounds.z - halfW;
    float minY = m_bounds.y + halfH;
    float maxY = m_bounds.y + m_bounds.w - halfH;

    // Clamp camera position
    float actualMinX = std::min(minX, maxX);
    float actualMaxX = std::max(minX, maxX);
    float actualMinY = std::min(minY, maxY);
    float actualMaxY = std::max(minY, maxY);
    m_position.x = std::clamp(m_position.x, actualMinX, actualMaxX);
    m_position.y = std::clamp(m_position.y, actualMinY, actualMaxY);
}
