/*************************************************************************
  \file       GGRender2d.cpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       03 October 2025
  \brief
    Immediate-mode 2D renderer for points, lines, triangles, squares,
    circles, and AABBs.

  \details
    Provides a static class `render2d` that allows drawing simple 2D
    primitives with OpenGL. Supports:
      - Single point, line, triangle, square, and circle rendering
      - AABB visualization for debugging
      - Projection matrix support
      - Dynamic buffer updates for immediate-mode drawing
      - Alpha blending
*************************************************************************/
#include "GGRender2d.hpp"
#include <GGEngine.hpp>
#include <GGEngineRender.hpp>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <GGSystemManagers.hpp>




/*************************************************************************
  \brief
    Initializes the 2D renderer, setting up shaders, VAOs, and VBOs for
    points, lines, triangles, squares, and circles.
  \param proj
    Projection matrix to use for rendering.
*************************************************************************/
void render2d::init(const glm::mat4& proj) {
    projection = proj;

    shader = GGShader("../../../EngineAssets/Shaders/render2d.vert", "../../../EngineAssets/Shaders/render2d.frag");
    uiShader= GGShader("../../../EngineAssets/Shaders/UITextShader.vert", "../../../EngineAssets/Shaders/UITextShader.frag");
   

    // --- Setup for points ---
    glGenVertexArrays(1, &vaoPoints);
    glGenBuffers(1, &vboPoints);
    glBindVertexArray(vaoPoints);
    glBindBuffer(GL_ARRAY_BUFFER, vboPoints);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GGPoint) * 1000, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GGPoint), (void*)offsetof(GGPoint, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GGPoint), (void*)offsetof(GGPoint, color));
    glEnableVertexAttribArray(1);
    // --- Setup for lines ---
    glGenVertexArrays(1, &vaoLines);
    glGenBuffers(1, &vboLines);
    glBindVertexArray(vaoLines);
    glBindBuffer(GL_ARRAY_BUFFER, vboLines);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GGPoint) * 2000, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GGPoint), (void*)offsetof(GGPoint, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GGPoint), (void*)offsetof(GGPoint, color));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // --- Setup for triangles ---
    glGenVertexArrays(1, &vaoTriangles);
    glGenBuffers(1, &vboTriangles);
    glBindVertexArray(vaoTriangles);
    glBindBuffer(GL_ARRAY_BUFFER, vboTriangles);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GGPoint) * 3000, nullptr, GL_DYNAMIC_DRAW); // enough space for many triangles

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GGPoint), (void*)offsetof(GGPoint, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GGPoint), (void*)offsetof(GGPoint, color));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);



    // Load textured shader
    //shaderTex = GGShader("../Shaders/render2dtex.vert", "../Shaders/render2dtex.frag");
    // --- Setup for squares ---
    glGenVertexArrays(1, &vaoSquares);
    glGenBuffers(1, &vboSquares);
    glBindVertexArray(vaoSquares);
    glBindBuffer(GL_ARRAY_BUFFER, vboSquares);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GGPoint4) * 6, nullptr, GL_DYNAMIC_DRAW); // only 6 vertices per square

    // position (2 floats)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GGPoint4), (void*)offsetof(GGPoint4, position));
    glEnableVertexAttribArray(0);

    // color (4 floats)
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GGPoint4), (void*)offsetof(GGPoint4, color));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // --- Setup for circles ---
    glGenVertexArrays(1, &vaoCircles);
    glGenBuffers(1, &vboCircles);
    glBindVertexArray(vaoCircles);
    glBindBuffer(GL_ARRAY_BUFFER, vboCircles);
    // allocate enough for a decently detailed circle
    glBufferData(GL_ARRAY_BUFFER, sizeof(GGPoint4) * 500, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GGPoint4), (void*)offsetof(GGPoint4, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GGPoint4), (void*)offsetof(GGPoint4, color));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);


    // ui setup
    glGenVertexArrays(1, &vaoUIBox);
    glGenBuffers(1, &vboUIBox);
    glBindVertexArray(vaoUIBox);
    glBindBuffer(GL_ARRAY_BUFFER, vboUIBox);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GGPoint4) * 100, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GGPoint4), (void*)offsetof(GGPoint4, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GGPoint4), (void*)offsetof(GGPoint4, color));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);


    // Enable blending for alpha transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


}

/*************************************************************************
  \brief
    Cleans up all allocated OpenGL resources, including VAOs and VBOs.
*************************************************************************/
void render2d::cleanup() {
    glDeleteVertexArrays(1, &vaoPoints);
    glDeleteBuffers(1, &vboPoints);
    glDeleteVertexArrays(1, &vaoLines);
    glDeleteBuffers(1, &vboLines);
    glDeleteVertexArrays(1, &vaoTriangles);
    glDeleteBuffers(1, &vboTriangles);

    glDeleteVertexArrays(1, &vaoSquares);
    glDeleteBuffers(1, &vboSquares);
    glDeleteVertexArrays(1, &vaoCircles);
    glDeleteBuffers(1, &vboCircles);

    //ui buffer
    glDeleteVertexArrays(1, &vaoUIBox);
    glDeleteBuffers(1, &vboUIBox);
}

/*************************************************************************
  \brief
    Draws a single point immediately to the screen.
  \param p
    The point to draw, including position and color.
  \param size
    Point size in pixels.
*************************************************************************/
void render2d::drawPoint(const GGPoint& p, float size) {
    shader.Use();
   

    shader.SetMat4("projection", Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetActiveProjection());
    shader.SetMat4("camera", Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetActiveView());
    glPointSize(size); // <-- Set point size
    glBindVertexArray(vaoPoints);
    glBindBuffer(GL_ARRAY_BUFFER, vboPoints);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GGPoint), &p);
    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);
}

/*************************************************************************
  \brief
    Draws a single line immediately to the screen.
  \param l
    The line to draw, including start and end points and color.
  \param width
    Line width in pixels.
*************************************************************************/
void render2d::drawLine(const GGLine& l, float width) {
    shader.Use();
   
    shader.SetMat4("projection", Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetActiveProjection());
    shader.SetMat4("camera", Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetActiveView());
    shader.SetBool("renderUI", false);
    GGPoint verts[2] = { l.start, l.end };

    glBindVertexArray(vaoLines);
    glBindBuffer(GL_ARRAY_BUFFER, vboLines);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glLineWidth(width);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
}

/*************************************************************************
  \brief
    Draws a single triangle immediately to the screen.
  \param c
    Triangle vertices and color data.
*************************************************************************/
void render2d::drawTriangle(const GGTriangle& c)
{
    shader.Use();
  
    shader.SetMat4("projection", Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetActiveProjection());
    shader.SetMat4("camera", Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetActiveView());
    GGPoint verts[3] = {
        { c.vertices[0], c.color },
        { c.vertices[1], c.color },
        { c.vertices[2], c.color }
    };

    glBindVertexArray(vaoTriangles);
    glBindBuffer(GL_ARRAY_BUFFER, vboTriangles);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

/*************************************************************************
  \brief
    Draws an axis-aligned bounding box (AABB) as lines for debugging.
  \param aabb
    The AABB to draw.
*************************************************************************/
void render2d::DrawAABB(const AABBFloat& aabb)
{
    glm::vec4 color(1.f, 1.f, 1.f, 1.f); // default white (you can change later)
    float width = 2.0f;             // default line thickness

    // Top edge
    drawLine(GGLine({ aabb.left,  aabb.top }, { aabb.right, aabb.top }, color), width);

    // Bottom edge
    drawLine(GGLine({ aabb.left,  aabb.bottom }, { aabb.right, aabb.bottom }, color), width);

    // Left edge
    drawLine(GGLine({ aabb.left,  aabb.bottom }, { aabb.left,  aabb.top }, color), width);

    // Right edge
    drawLine(GGLine({ aabb.right, aabb.bottom }, { aabb.right, aabb.top }, color), width);
}

/*************************************************************************
  \brief
    Draws a filled square immediately to the screen.
  \param sq
    Square vertices and color data.
*************************************************************************/
void render2d::drawSquare(const GGSquare& sq)
{
    shader.Use();
   

    shader.SetMat4("projection", Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetActiveProjection());
    shader.SetMat4("camera", Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetActiveView());
    shader.SetBool("renderUI", false);
    const auto& v = sq.vertices;

    GGPoint4 verts[6] = {
        {v[0], sq.color}, {v[1], sq.color}, {v[2], sq.color}, // first triangle
        {v[0], sq.color}, {v[2], sq.color}, {v[3], sq.color}  // second triangle
    };

    glBindVertexArray(vaoSquares);
    glBindBuffer(GL_ARRAY_BUFFER, vboSquares);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

/*************************************************************************
  \brief
    Draws a filled circle immediately to the screen.
  \param c
    Circle center, radius, color, and number of segments.
*************************************************************************/
void render2d::drawCircle(const GGCircle& c)
{
    shader.Use();
    

    shader.SetMat4("projection", Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetActiveProjection());
    shader.SetMat4("camera", Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetActiveView());
    std::vector<GGPoint4> verts;
    verts.reserve(c.segments + 2);
    verts.push_back({ c.center, c.color });

    for (int i = 0; i <= c.segments; i++)
    {
        float theta = (float)i / c.segments * 2.0f * glm::pi<float>();
        float x = c.center.x + c.radius * cosf(theta);
        float y = c.center.y + c.radius * sinf(theta);
        verts.push_back({ GGMath::Vector2(x, y), c.color });
    }

    glBindVertexArray(vaoCircles);
    glBindBuffer(GL_ARRAY_BUFFER, vboCircles);
    glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(GGPoint4), verts.data());
    glDrawArrays(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(verts.size()));
    glBindVertexArray(0);
}

/*************************************************************************
  \brief
    Enables or disables picking mode, used for entity selection.
  \param enabled
    True to enable picking mode, false to disable.
*************************************************************************/
void render2d::SetPickingMode(bool enabled) {
    pickingModeEnabled = enabled;

}

/*************************************************************************
  \brief
    Sets the current picking color to be used when rendering in picking mode.
  \param color
    RGBA color to assign to the object for picking.
*************************************************************************/
void render2d::SetCurrentPickingColor(const glm::vec4& color) {
    currentPickingColor = color;
}




/*************************************************************************
  \brief
    Draws a circle for picking purposes using the current picking color.
  \param circle
    Circle data including center, radius, and number of segments.
*************************************************************************/
void render2d::RenderCirclePicking(const GGCircle& circle) {
    if (!pickingModeEnabled) return;

    GGCircle c = circle;
    c.color = currentPickingColor;

    drawCircle(c);
}

void render2d::drawUIBox(const GGUIBox& box)
{
    uiShader.Use();

    uiShader.SetMat4("projection", Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetActiveCamera()->GetScreenProjectionMatrix());
    auto camera = Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetActiveCamera();

    glm::vec2 resScale = camera ? camera->GetResolutionScale() : glm::vec2(1.f);

    glm::mat4 zoomMatrix = glm::scale(glm::mat4(1.f), { resScale, 1.f });

    // Use identity matrix for UI objects
    uiShader.SetMat4("camera", zoomMatrix);
    uiShader.SetMat4(
        "worldscale",
        Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetDefaultWorldScale());
    //uiShader.SetBool("renderUI", true);

    // Calculate the four corners
    float left = box.position.x;
    float right = box.position.x + box.size.x;
    float top = box.position.y;
    float bottom = box.position.y + box.size.y;

    // Draw filled background
    GGPoint4 fillVerts[6] = {
        {GGMath::Vector2(left, top), box.fillColor},
        {GGMath::Vector2(right, top), box.fillColor},
        {GGMath::Vector2(right, bottom), box.fillColor},

        {GGMath::Vector2(left, top), box.fillColor},
        {GGMath::Vector2(right, bottom), box.fillColor},
        {GGMath::Vector2(left, bottom), box.fillColor}
    };

    glBindVertexArray(vaoUIBox);
    glBindBuffer(GL_ARRAY_BUFFER, vboUIBox);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(fillVerts), fillVerts);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Draw border
    if (box.borderWidth > 0.0f) {
       
            // Draw solid border
            drawUiLine(GGLine({ left, top }, { right, top }, box.borderColor), box.borderWidth);
            drawUiLine(GGLine({ right, top }, { right, bottom }, box.borderColor), box.borderWidth);
            drawUiLine(GGLine({ right, bottom }, { left, bottom }, box.borderColor), box.borderWidth);
            drawUiLine(GGLine({ left, bottom }, { left, top }, box.borderColor), box.borderWidth);
        
    }

    glBindVertexArray(0);
}

void render2d::drawUiLine(const GGLine& line,float width)
{
    uiShader.Use();

    uiShader.SetMat4("projection", Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetActiveCamera()->GetScreenProjectionMatrix());
    auto camera = Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetActiveCamera();

    glm::vec2 resScale = camera ? camera->GetResolutionScale() : glm::vec2(1.f);

    glm::mat4 zoomMatrix = glm::scale(glm::mat4(1.f), { resScale, 1.f });

    // Use identity matrix for UI objects
    uiShader.SetMat4("camera", zoomMatrix);
    uiShader.SetMat4(
        "worldscale",
        Systems::GetSystemsRegistry()
        .GetSystem<Systems::RenderManager>().GetDefaultWorldScale());
    //uiShader.SetBool("renderUI", true);

    GGPoint verts[2] = { line.start, line.end };

    glBindVertexArray(vaoLines);
    glBindBuffer(GL_ARRAY_BUFFER, vboLines);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glLineWidth(width);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
}


