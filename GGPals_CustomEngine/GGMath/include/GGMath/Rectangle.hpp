/*************************************************************************
  \file       Rectangle.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       7 September 2025
  \version    1.0
  \brief
    Defines the Rectangle class in the GGMath namespace.
  \details
    The Rectangle class provides axis-aligned bounding box functionality
    for collision detection, UI bounds checking, and spatial queries.
    It uses floating point coordinates for precise positioning and
    supports common rectangle operations like intersection and containment.
    All collision detection is AABB (Axis-Aligned Bounding Box) static-to-static.
  \dependencies
    <algorithm>, <iostream>, Vector2.hpp, Types.hpp
  \see
    Vector2, Circle, Line2D
  \history
    - 1.0 (7 September 2025): Initial version.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <algorithm>
#include <iostream>
#include "Vector2.hpp"
#include "Types.hpp"

namespace GGMath
{
    /*************************************************************************
      \brief
        Axis-aligned rectangle for collision detection and UI bounds
      \par Usage
      \code
        Rectangle collisionBounds(0, 0, 100, 50);
        Rectangle playerHitbox = Rectangle::createFromCenterPoint(position, dimensions);
        if (buttonBounds.containsPoint(mousePosition))
      \endcode
    *************************************************************************/
    class Rectangle
    {
    public:
        float32 leftEdge;     // X coordinate of left edge
        float32 bottomEdge;   // Y coordinate of bottom edge  
        float32 width;        // Horizontal dimension (must be non-negative)
        float32 height;       // Vertical dimension (must be non-negative)

        /*************************************************************************
          \brief
            Default constructor - creates zero-area rectangle at origin
          \par Usage
          \code
            Rectangle boundingBox;          // Creates (0, 0, 0, 0)
            Rectangle collisionArea;        // Creates empty rectangle
          \endcode
          \par Output
            Creates rectangle with zero position and dimensions
          \par Purpose
            Initialize empty rectangle for later configuration via setter methods
        *************************************************************************/
        Rectangle() : leftEdge(0.0f), bottomEdge(0.0f), width(0.0f), height(0.0f)
        {
        }

        /*************************************************************************
          \brief
            Parameterized constructor - creates rectangle with specified bounds
          \param[in] leftPosition
            X coordinate of the left edge
          \param[in] bottomPosition
            Y coordinate of the bottom edge
          \param[in] rectangleWidth
            Horizontal dimension (must be non-negative)
          \param[in] rectangleHeight
            Vertical dimension (must be non-negative)
          \par Usage
          \code
            Rectangle collisionBounds(100, 50, 200, 100);    // 200x100 rect at (100,50)
            Rectangle screenArea(0, 0, screenWidth, screenHeight);
          \endcode
          \par Output
            Creates rectangle with specified position and dimensions
          \par Purpose
            Direct initialization of rectangle bounds for collision detection
        *************************************************************************/
        Rectangle(float32 leftPosition, float32 bottomPosition, float32 rectangleWidth, float32 rectangleHeight)
            : leftEdge(leftPosition), bottomEdge(bottomPosition), width(rectangleWidth), height(rectangleHeight)
        {
        }

        /*************************************************************************
          \brief
            Constructor from position and size vectors
          \param[in] bottomLeftPosition
            Bottom-left corner position as Vector2
          \param[in] dimensions
            Width and height as Vector2 (x=width, y=height)
          \par Usage
          \code
            Vector2 position(100, 50);
            Vector2 size(200, 100);
            Rectangle entityBounds(position, size);
          \endcode
          \par Output
            Creates rectangle using vector parameters
          \par Purpose
            Convenient construction from existing position and size vectors
        *************************************************************************/
        Rectangle(const Vector2& bottomLeftPosition, const Vector2& dimensions)
            : leftEdge(bottomLeftPosition.x), bottomEdge(bottomLeftPosition.y), width(dimensions.x), height(dimensions.y)
        {
        }

        /*************************************************************************
          \brief
            Test if point lies within rectangle boundaries (inclusive)
          \param[in] testPoint
            Point coordinates to test for containment
          \return
            True if point is inside rectangle or on its boundary
          \par Usage
          \code
            if (buttonBounds.containsPoint(mousePosition))      // Mouse over UI element
            if (levelBounds.containsPoint(playerPosition))      // Player within level
          \endcode
          \par Output
            Returns true if point coordinates satisfy: leftEdge <= x <= rightEdge and bottomEdge <= y <= topEdge
          \par Purpose
            UI interaction testing and spatial boundary validation
        *************************************************************************/
        bool containsPoint(const Vector2& testPoint) const
        {
            return testPoint.x >= leftEdge && testPoint.x <= leftEdge + width &&
                testPoint.y >= bottomEdge && testPoint.y <= bottomEdge + height;
        }

        /*************************************************************************
          \brief
            Test if this rectangle intersects with another rectangle (AABB collision)
          \param[in] otherRectangle
            Rectangle to test intersection against
          \return
            True if rectangles overlap in any area
          \par Usage
          \code
            if (playerHitbox.intersectsRectangle(enemyHitbox))   // Entity collision
            if (projectileBounds.intersectsRectangle(wallBounds)) // Projectile hit wall
          \endcode
          \par Output
            Returns true if rectangles share any overlapping area using AABB algorithm
          \par Purpose
            Static-to-static collision detection between rectangular game objects
        *************************************************************************/
        bool intersectsRectangle(const Rectangle& otherRectangle) const
        {
            return leftEdge < otherRectangle.leftEdge + otherRectangle.width &&
                leftEdge + width > otherRectangle.leftEdge &&
                bottomEdge < otherRectangle.bottomEdge + otherRectangle.height &&
                bottomEdge + height > otherRectangle.bottomEdge;
        }

        /*************************************************************************
          \brief
            Calculate the geometric center point of rectangle
          \return
            Vector2 representing the center coordinates
          \par Usage
          \code
            Vector2 centerPoint = entityBounds.computeCenterPoint();
            Vector2 rotationOrigin = spriteBounds.computeCenterPoint();
          \endcode
          \par Output
            Returns center position calculated as (leftEdge + width/2, bottomEdge + height/2)
          \par Purpose
            Determine center coordinates for positioning, rotation, and rendering operations
        *************************************************************************/
        Vector2 computeCenterPoint() const
        {
            return Vector2(leftEdge + width * 0.5f, bottomEdge + height * 0.5f);
        }

        /*************************************************************************
          \brief
            Calculate the total area of rectangle
          \return
            Area as width multiplied by height
          \par Usage
          \code
            float32 totalArea = levelBounds.computeArea();
            if (selectionBox.computeArea() > minimumSelectionArea)
          \endcode
          \par Output
            Returns area as positive float32 (width * height)
          \par Purpose
            Size validation and spatial calculations for game logic
        *************************************************************************/
        float32 computeArea() const
        {
            return width * height;
        }

        /*************************************************************************
          \brief
            Create rectangle positioned around center point with specified dimensions
          \param[in] centerPosition
            Center point where rectangle should be positioned
          \param[in] rectangleDimensions
            Width and height dimensions as Vector2
          \return
            Rectangle positioned with center at specified point
          \par Usage
          \code
            Rectangle playerHitbox = Rectangle::createFromCenterPoint(playerPosition, playerSize);
            Rectangle menuDialog = Rectangle::createFromCenterPoint(screenCenter, dialogSize);
          \endcode
          \par Output
            Returns rectangle with bottom-left calculated to center the rectangle at centerPosition
          \par Purpose
            Convenient creation for objects that store center position rather than corner position
        *************************************************************************/
        static Rectangle createFromCenterPoint(const Vector2& centerPosition, const Vector2& rectangleDimensions)
        {
            Vector2 halfDimensions = rectangleDimensions * 0.5f;
            return Rectangle(centerPosition.x - halfDimensions.x, centerPosition.y - halfDimensions.y,
                rectangleDimensions.x, rectangleDimensions.y);
        }

        /*************************************************************************
          \brief
            Create rectangle spanning between two corner points with automatic orientation correction
          \param[in] firstCorner
            First corner point (any corner of desired rectangle)
          \param[in] secondCorner
            Opposite corner point (diagonally opposite corner)
          \return
            Rectangle with correct orientation regardless of corner point order
          \par Usage
          \code
            Rectangle selectionArea = Rectangle::createFromCornerPoints(dragStartPosition, dragEndPosition);
            Rectangle levelBounds = Rectangle::createFromCornerPoints(minimumBounds, maximumBounds);
          \endcode
          \par Output
            Returns rectangle with leftEdge, bottomEdge representing minimum coordinates
          \par Purpose
            Create rectangles from user selection or bounding box calculations where corner order is unknown
        *************************************************************************/
        static Rectangle createFromCornerPoints(const Vector2& firstCorner, const Vector2& secondCorner)
        {
            float32 minimumX = std::min(firstCorner.x, secondCorner.x);
            float32 minimumY = std::min(firstCorner.y, secondCorner.y);
            float32 maximumX = std::max(firstCorner.x, secondCorner.x);
            float32 maximumY = std::max(firstCorner.y, secondCorner.y);
            return Rectangle(minimumX, minimumY, maximumX - minimumX, maximumY - minimumY);
        }
    };

    /*************************************************************************
      \brief
        Stream output operator for debugging and logging
      \param[in] outputStream
        Output stream to write rectangle data to
      \param[in] rectangle
        Rectangle object to output
      \return
        Reference to output stream for method chaining
      \par Usage
      \code
        std::cout << collisionBounds;           // Print rectangle coordinates
        debugLog << "Entity bounds: " << entityRect;
      \endcode
      \par Output
        Outputs rectangle in format "Rectangle(left, bottom, width, height)"
      \par Purpose
        Debug output and logging support for rectangle objects
    *************************************************************************/
    inline std::ostream& operator<<(std::ostream& outputStream, const Rectangle& rectangle)
    {
        outputStream << "Rectangle(" << rectangle.leftEdge << ", " << rectangle.bottomEdge
            << ", " << rectangle.width << ", " << rectangle.height << ")";
        return outputStream;
    }
}