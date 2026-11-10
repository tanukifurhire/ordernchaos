/*************************************************************************
  \file       Circle.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       7 September 2025
  \version    1.0
  \brief
    Defines the Circle class in the GGMath namespace.
  \details
    The Circle class provides circular collision detection and geometric
    operations for 2D games. It supports point containment, circle-circle
    intersection, and distance calculations commonly used in collision
    systems and spatial queries. All collision detection is static-to-static.
  \dependencies
    <cmath>, <iostream>, Constants.hpp, Vector2.hpp, Rectangle.hpp, Types.hpp
  \see
    Vector2, Rectangle, Line2D
  \history
    - 1.0 (7 September 2025): Initial version.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <cmath>
#include <iostream>
#include "Constants.hpp"
#include "Vector2.hpp"
#include "Rectangle.hpp"
#include "Types.hpp"

namespace GGMath
{
    /*************************************************************************
      \brief
        Circle class for collision detection and geometric operations
      \par Usage
      \code
        Circle playerHitbox(playerPosition, characterRadius);
        Circle explosionArea(bombPosition, blastRadius);
        if (projectile.intersectsCircle(targetHitbox))
      \endcode
    *************************************************************************/
    class Circle
    {
    public:
        Vector2 centerPosition;     // Center point coordinates of the circle
        float32 radius;             // Distance from center to edge (must be non-negative)

        /*************************************************************************
          \brief
            Default constructor - creates unit circle at coordinate origin
          \par Usage
          \code
            Circle defaultBounds;               // Creates circle at (0,0) with radius 1.0
            Circle temporaryArea;               // Default circle for later configuration
          \endcode
          \par Output
            Creates circle with center at (0,0) and radius of 1.0 units
          \par Purpose
            Initialize default circle state for later configuration via setter methods
        *************************************************************************/
        Circle() : centerPosition(Vector2::zero()), radius(1.0f)
        {
        }

        /*************************************************************************
          \brief
            Parameterized constructor - creates circle with specified center and radius
          \param[in] circleCenter
            Center position coordinates as Vector2
          \param[in] circleRadius
            Distance from center to edge (must be non-negative)
          \par Usage
          \code
            Circle playerHitbox(playerPosition, characterRadius);
            Circle explosionArea(Vector2(100, 200), explosionRadius);
          \endcode
          \par Output
            Creates circle with specified center coordinates and radius
          \par Purpose
            Direct initialization of circle properties for collision detection
        *************************************************************************/
        Circle(const Vector2& circleCenter, float32 circleRadius) : centerPosition(circleCenter), radius(circleRadius)
        {
        }

        /*************************************************************************
          \brief
            Constructor with separate center coordinate parameters
          \param[in] centerX
            X coordinate of circle center
          \param[in] centerY
            Y coordinate of circle center
          \param[in] circleRadius
            Distance from center to edge (must be non-negative)
          \par Usage
          \code
            Circle entityBounds(entityX, entityY, entityRadius);
            Circle mouseArea(mouseX, mouseY, cursorRadius);
          \endcode
          \par Output
            Creates circle with center at (centerX, centerY) and specified radius
          \par Purpose
            Convenient construction when center coordinates are available separately
        *************************************************************************/
        Circle(float32 centerX, float32 centerY, float32 circleRadius) : centerPosition(centerX, centerY), radius(circleRadius)
        {
        }

        /*************************************************************************
          \brief
            Test if point lies within circle boundary (inclusive)
          \param[in] testPoint
            Point coordinates to test for containment
          \return
            True if point is inside circle or on its circumference
          \par Usage
          \code
            if (explosionArea.containsPoint(playerPosition))    // Player caught in blast
            if (interactionZone.containsPoint(mousePosition))   // Mouse over interaction area
          \endcode
          \par Output
            Returns true if distance from testPoint to center is less than or equal to radius
          \par Purpose
            Point-in-circle collision detection and spatial boundary testing
        *************************************************************************/
        bool containsPoint(const Vector2& testPoint) const
        {
            return Vector2::distance(centerPosition, testPoint) <= radius;
        }

        /*************************************************************************
          \brief
            Test if this circle intersects with another circle (static collision)
          \param[in] otherCircle
            Circle to test intersection against
          \return
            True if circles overlap or touch
          \par Usage
          \code
            if (playerHitbox.intersectsCircle(enemyHitbox))     // Character collision
            if (projectileArea.intersectsCircle(targetArea))    // Projectile hit detection
          \endcode
          \par Output
            Returns true if distance between centers is less than or equal to sum of radii
          \par Purpose
            Static-to-static circular collision detection between game entities
        *************************************************************************/
        bool intersectsCircle(const Circle& otherCircle) const
        {
            float32 centerDistance = Vector2::distance(centerPosition, otherCircle.centerPosition);
            return centerDistance <= (radius + otherCircle.radius);
        }

        /*************************************************************************
          \brief
            Calculate the total area enclosed by circle
          \return
            Area as PI multiplied by radius squared
          \par Usage
          \code
            float32 blastArea = explosionCircle.computeArea();
            if (selectionCircle.computeArea() > minimumSelectionArea)
          \endcode
          \par Output
            Returns area calculated as PI * radius^2
          \par Purpose
            Area calculations for game mechanics and spatial validation
        *************************************************************************/
        float32 computeArea() const
        {
            return PI * radius * radius;
        }

        /*************************************************************************
          \brief
            Calculate the perimeter length around circle edge
          \return
            Circumference as 2 * PI * radius
          \par Usage
          \code
            float32 trackLength = racingTrack.computeCircumference();
            float32 orbitPerimeter = planetOrbit.computeCircumference();
          \endcode
          \par Output
            Returns circumference calculated as 2 * PI * radius
          \par Purpose
            Perimeter calculations for movement paths and rendering operations
        *************************************************************************/
        float32 computeCircumference() const
        {
            return 2.0f * PI * radius;
        }

        /*************************************************************************
          \brief
            Test if circle intersects with axis-aligned rectangle
          \param[in] rectangle
            Rectangle to test intersection against
          \return
            True if circle and rectangle overlap or touch
          \par Usage
          \code
            if (playerCircle.intersectsRectangle(wallBounds))   // Circle-box collision
            if (explosionArea.intersectsRectangle(buildingBounds)) // Area damage calculation
          \endcode
          \par Output
            Returns true if circle intersects rectangle using closest point algorithm
          \par Purpose
            Mixed-shape collision detection between circular and rectangular objects
        *************************************************************************/
        bool intersectsRectangle(const Rectangle& rectangle) const
        {
            // Find closest point on rectangle boundary to circle center
            float32 closestX = std::max(rectangle.leftEdge,
                std::min(centerPosition.x, rectangle.leftEdge + rectangle.width));
            float32 closestY = std::max(rectangle.bottomEdge,
                std::min(centerPosition.y, rectangle.bottomEdge + rectangle.height));

            Vector2 closestPoint(closestX, closestY);
            return Vector2::distance(centerPosition, closestPoint) <= radius;
        }
    };

    /*************************************************************************
      \brief
        Stream output operator for debugging and logging
      \param[in] outputStream
        Output stream to write circle data to
      \param[in] circle
        Circle object to output
      \return
        Reference to output stream for method chaining
      \par Usage
      \code
        std::cout << playerHitbox;              // Print circle properties
        debugLog << "Explosion area: " << blastCircle;
      \endcode
      \par Output
        Outputs circle in format "Circle(center: Vector2(x, y), radius: r)"
      \par Purpose
        Debug output and logging support for circle objects
    *************************************************************************/
    inline std::ostream& operator<<(std::ostream& outputStream, const Circle& circle)
    {
        outputStream << "Circle(center: " << circle.centerPosition << ", radius: " << circle.radius << ")";
        return outputStream;
    }
}