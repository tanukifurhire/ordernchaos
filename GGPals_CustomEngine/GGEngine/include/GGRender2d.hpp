/*************************************************************************
  \file       GGRender2d.hpp 
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
#pragma once

#include <GGEngine.hpp>
#include <GGEngineRender.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GGMath/Vector2.hpp>
#include <array>
#include <GGECSComponents.hpp>

/*************************************************************************
  \struct GGPoint
  \brief Represents a colored 2D point.
*************************************************************************/
struct GGPoint {
	GGMath::Vector2 position;
	glm::vec4 color;

	GGPoint() : position(0.0f, 0.0f), color(1.0f, 1.0f, 1.0f, 1.0f) {};

	GGPoint(float x, float y, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f)
		: position(x, y), color(r, g, b, a) {
	}

	GGPoint(GGMath::Vector2 pos, glm::vec4 clr) :position(pos), color(clr) {};
	
};

/*************************************************************************
  \struct GGLine
  \brief Represents a colored 2D line segment.
*************************************************************************/
struct GGLine {

	GGPoint start;
	GGPoint end;

	GGLine() : start(), end() {}
	GGLine(const GGPoint& s, const GGPoint& e) : start(s), end(e) {}
	GGLine(GGMath::Vector2 s, GGMath::Vector2 e, glm::vec4 color = { 1.f,1.f,1.f,1.f })
		: start(s, color), end(e, color) {
	}
	
};

/*************************************************************************
  \struct GGTriangle
  \brief Represents a colored 2D triangle.
*************************************************************************/
struct GGTriangle {
	std::array<GGMath::Vector2, 3> vertices;
	glm::vec4 color;


	// Constructor
	GGTriangle(const GGMath::Vector2& v0,
		const GGMath::Vector2& v1,
		const GGMath::Vector2& v2,
		const glm::vec4& col = { 1.0f, 1.0f, 1.0f ,1.0f })
		: vertices{ v0, v1, v2 }, color(col) {
	}


};


/*************************************************************************
  \struct GGSquare
  \brief Represents a colored 2D square.
*************************************************************************/
// Solid color square defined by 4 vertices (RGBA)
struct GGSquare {
	std::array<GGMath::Vector2, 4> vertices; // order them in CCW or CW
	glm::vec4 color;                        // RGBA

	GGSquare(const GGMath::Vector2& v0,
		const GGMath::Vector2& v1,
		const GGMath::Vector2& v2,
		const GGMath::Vector2& v3,
		const glm::vec4& col = { 1.f,1.f,1.f,1.f })
		: vertices{ v0,v1,v2,v3 }, color(col) {
	}

	// or array-based constructor:
	GGSquare(const std::array<GGMath::Vector2, 4>& verts,
		const glm::vec4& col = { 1.f,1.f,1.f,1.f })
		: vertices(verts), color(col) {
	}
};
/*************************************************************************
  \struct GGCircle
  \brief Represents a colored 2D circle.
*************************************************************************/
struct GGCircle {
	GGMath::Vector2 center;
	float radius;
	glm::vec4 color;
	int segments;

	GGCircle(const GGMath::Vector2& c, float r,
		const glm::vec4& col = { 1.f,1.f,1.f,1.f },
		int seg = 32)
		: center(c), radius(r), color(col), segments(seg) {
	}
	
};
/*************************************************************************
  \struct GGPoint4
  \brief Vertex structure for 2D square/circle rendering.
*************************************************************************/
// Vertex for square/circle
struct GGPoint4 {
	GGMath::Vector2 position;
	glm::vec4 color;

	GGPoint4() : position(0.f, 0.f), color(1.f, 1.f, 1.f, 1.f) {}
	GGPoint4(GGMath::Vector2 pos, glm::vec4 col) : position(pos), color(col) {}
};


struct GGUIBox {
	GGMath::Vector2 position;      // Top-left corner
	GGMath::Vector2 size;          // Width and height
	glm::vec4 fillColor;           // Background color
	glm::vec4 borderColor;         // Border color
	float borderWidth;             // Border thickness in pixels
	
};




/*************************************************************************
  \class render2d
  \brief Static class for immediate-mode 2D rendering.
  \details
	Provides static methods to draw points, lines, triangles, squares,
	circles, and AABBs. Uses VAOs/VBOs and dynamic buffer updates.
*************************************************************************/
class render2d {

public:
	/*************************************************************************
	  \brief
		Initializes the 2D renderer, setting up shaders, VAOs, and VBOs for
		points, lines, triangles, squares, and circles.
	  \param proj
		Projection matrix to use for rendering.
	*************************************************************************/
	static void init(const glm::mat4& proj);

	/*************************************************************************
	  \brief
		Cleans up all allocated OpenGL resources, including VAOs and VBOs.
	*************************************************************************/
	static void cleanup();


	/*************************************************************************
	  \brief
		Draws a single point immediately to the screen.
	  \param p
		The point to draw, including position and color.
	  \param size
		Point size in pixels.
	*************************************************************************/
	static void drawPoint(const GGPoint& p, float size = 5.0f);

	/*************************************************************************
	  \brief
		Draws a single line immediately to the screen.
	  \param l
		The line to draw, including start and end points and color.
	  \param width
		Line width in pixels.
	*************************************************************************/
	static void drawLine(const GGLine& l, float width = 3.0f);

	/*************************************************************************
	  \brief
		Draws a single triangle immediately to the screen.
	  \param c
		Triangle vertices and color data.
	*************************************************************************/
	static void drawTriangle(const GGTriangle& c);

	/*************************************************************************
	  \brief
		Draws an axis-aligned bounding box (AABB) as lines for debugging.
	  \param aabb
		The AABB to draw.
	*************************************************************************/
	static void DrawAABB(const AABBFloat& aabb);

	/*************************************************************************
	  \brief
		Draws a filled square immediately to the screen.
	  \param sq
		Square vertices and color data.
	*************************************************************************/
	static void drawSquare(const GGSquare& sq);

	/*************************************************************************
	  \brief
		Draws a filled circle immediately to the screen.
	  \param c
		Circle center, radius, color, and number of segments.
	*************************************************************************/
	static void drawCircle(const GGCircle& c);

	static void SetProjection(const glm::mat4& proj) { projection = proj; }

	/*************************************************************************
	  \brief
		Enables or disables picking mode, used for entity selection.
	  \param enabled
		True to enable picking mode, false to disable.
	*************************************************************************/
	static void SetPickingMode(bool enabled);

	/*************************************************************************
	  \brief
		Sets the current picking color to be used when rendering in picking mode.
	  \param color
		RGBA color to assign to the object for picking.
	*************************************************************************/
	static void SetCurrentPickingColor(const glm::vec4& color);

	/*************************************************************************
	  \brief
		Draws a circle for picking purposes using the current picking color.
	  \param circle
		Circle data including center, radius, and number of segments.
	*************************************************************************/
	static void RenderCirclePicking(const GGCircle& circle);

	static void drawUIBox(const GGUIBox& box);


	static void drawUiLine(const GGLine& line,float width=5.f);


	static inline bool pickingModeEnabled = false;
	static inline glm::vec4 currentPickingColor = { 0, 0, 0, 0 };

	
	
private:
	 


	static inline GLuint vaoPoints = 0, vboPoints = 0;
	static inline GLuint vaoLines = 0, vboLines = 0;
	static inline GLuint vaoTriangles = 0, vboTriangles = 0;
	static inline GLuint vaoTrianglesTex = 0, vboTrianglesTex = 0;
	static inline GGShader shader;

	static inline GGShader uiShader;
	
	static inline glm::mat4 projection;
	static inline GLuint vaoSquares = 0, vboSquares = 0;
	static inline GLuint vaoCircles = 0, vboCircles = 0;


	static inline GGShader shaderTransform;  // Shader matching instancing approach
	static inline glm::mat4 worldScale;

	static inline GLuint vaoUIBox=0;
	static inline GLuint vboUIBox=0;

};






