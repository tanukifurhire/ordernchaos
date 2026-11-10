/*************************************************************************
  \file       Line2D.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       7 September 2025
  \version    1.0
  \brief
    Defines the Line2D class in the GGMath namespace.
  \details
    The Line2D class provides line segment operations for 2D games including
    intersection testing, distance calculations, and point projections.
    It represents finite line segments defined by two endpoints and supports
    common geometric queries used in collision detection, pathfinding, and
    line-of-sight calculations. All operations are static geometric calculations.
  \dependencies
    <cmath>, <iostream>, Constants.hpp, Vector2.hpp, Types.hpp
  \see
    Vector2, Rectangle, Circle
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
#include "Types.hpp"

namespace GGMath
{
    /*************************************************************************
      \brief
        Line segment class for intersection testing and geometric queries
      \par Usage
      \code
        Line2D wallSegment(Vector2(0, 0), Vector2(100, 0));
        Line2D sightRay(playerPosition, targetPosition);
        if (sightRay.intersectsLineSegment(wallSegment)) // line of sight blocked
      \endcode
    *************************************************************************/
    class Line2D
    {
    public:
        Vector2 startPoint;     // Starting endpoint coordinates of line segment
        Vector2 endPoint;       // Ending endpoint coordinates of line segment

        /*************************************************************************
          \brief
            Default constructor - creates unit line segment along positive X-axis
          \par Usage
          \code
            Line2D defaultLine;                 // Creates line from (0,0) to (1,0)
            Line2D temporarySegment;            // Default line for later configuration
          \endcode
          \par Output
            Creates line segment from origin (0,0) to point (1,0)
          \par Purpose
            Initialize default line segment state for later configuration via setter methods
        *************************************************************************/
        Line2D() : startPoint(Vector2::zero()), endPoint(Vector2::right())
        {
        }

        /*************************************************************************
          \brief
            Parameterized constructor - creates line segment between two specified points
          \param[in] segmentStart
            Starting endpoint coordinates as Vector2
          \param[in] segmentEnd
            Ending endpoint coordinates as Vector2
          \par Usage
          \code
            Line2D wallBoundary(Vector2(0, 0), Vector2(100, 0));
            Line2D visionRay(playerPosition, targetPosition);
          \endcode
          \par Output
            Creates finite line segment connecting segmentStart to segmentEnd
          \par Purpose
            Direct initialization of line segment endpoints for geometric calculations
        *************************************************************************/
        Line2D(const Vector2& segmentStart, const Vector2& segmentEnd) : startPoint(segmentStart), endPoint(segmentEnd)
        {
        }

        /*************************************************************************
          \brief
            Calculate direction vector from start to end point (maintains original length)
          \return
            Vector representing direction and magnitude from startPoint to endPoint
          \par Usage
          \code
            Vector2 wallDirection = wallSegment.computeDirectionVector();
            Vector2 normalizedMovement = raySegment.computeDirectionVector().normalized();
          \endcode
          \par Output
            Returns vector calculated as (endPoint - startPoint)
          \par Purpose
            Determine line orientation, length, and direction for movement calculations
        *************************************************************************/
        Vector2 computeDirectionVector() const
        {
            return endPoint - startPoint;
        }

        /*************************************************************************
          \brief
            Calculate euclidean distance between start and end points
          \return
            Length of line segment as positive floating point value
          \par Usage
          \code
            float32 wallLength = wallSegment.computeLength();
            float32 pathDistance = movementPath.computeLength();
          \endcode
          \par Output
            Returns length calculated using euclidean distance formula
          \par Purpose
            Measure distances for pathfinding, collision detection, and spatial validation
        *************************************************************************/
        float32 computeLength() const
        {
            return Vector2::distance(startPoint, endPoint);
        }

        /*************************************************************************
          \brief
            Calculate squared distance between endpoints (avoids expensive square root)
          \return
            Squared length for performance-critical distance comparisons
          \par Usage
          \code
            float32 lengthSquared = lineSegment.computeLengthSquared();
            if (pathSegment.computeLengthSquared() > minimumPathLengthSquared) // valid path
          \endcode
          \par Output
            Returns squared length for efficient distance comparisons without sqrt operation
          \par Purpose
            Performance-optimized length comparisons and validation in tight loops
        *************************************************************************/
        float32 computeLengthSquared() const
        {
            return (endPoint - startPoint).lengthSquared();
        }

        /*************************************************************************
          \brief
            Calculate point at specified parameter along line segment using linear interpolation
          \param[in] parameter
            Interpolation parameter where 0.0 = startPoint, 1.0 = endPoint
          \return
            Point coordinates at parameter position along line segment
          \par Usage
          \code
            Vector2 midpoint = lineSegment.computePointAtParameter(0.5f);
            Vector2 quarterPoint = pathSegment.computePointAtParameter(0.25f);
          \endcode
          \par Output
            Returns interpolated point clamped to line segment boundaries
          \par Purpose
            Sample points along line for rendering, collision detection, and path calculations
        *************************************************************************/
        Vector2 computePointAtParameter(float32 parameter) const
        {
            return Vector2::lerp(startPoint, endPoint, parameter);
        }

        /*************************************************************************
          \brief
            Find closest point on line segment to specified external point
          \param[in] externalPoint
            Point coordinates to find closest position relative to
          \return
            Closest point on line segment boundary to externalPoint
          \par Usage
          \code
            Vector2 closestWallPoint = wallSegment.computeClosestPointTo(playerPosition);
            Vector2 snapPosition = guideLineSegment.computeClosestPointTo(mousePosition);
          \endcode
          \par Output
            Returns point on line segment that minimizes distance to externalPoint
          \par Purpose
            Distance calculations, point snapping, and proximity-based game mechanics
        *************************************************************************/
        Vector2 computeClosestPointTo(const Vector2& externalPoint) const
        {
            Vector2 directionVector = computeDirectionVector();
            float32 segmentLengthSquared = directionVector.lengthSquared();

            // Handle degenerate case where start and end points are identical
            if (segmentLengthSquared < FLOAT_EPSILON)
            {
                return startPoint;
            }

            // Project external point onto line and clamp to segment boundaries
            float32 projectionParameter = (externalPoint - startPoint).dot(directionVector) / segmentLengthSquared;
            projectionParameter = std::max(0.0f, std::min(1.0f, projectionParameter));

            return startPoint + directionVector * projectionParameter;
        }

        /*************************************************************************
          \brief
            Calculate shortest euclidean distance from external point to line segment
          \param[in] externalPoint
            Point coordinates to measure distance from
          \return
            Shortest distance from externalPoint to any point on line segment
          \par Usage
          \code
            float32 wallDistance = wallSegment.computeDistanceToPoint(playerPosition);
            if (lineSegment.computeDistanceToPoint(mousePosition) < clickThreshold) // clicked
          \endcode
          \par Output
            Returns positive distance value representing shortest path to line segment
          \par Purpose
            Collision detection, proximity testing, and user interaction validation
        *************************************************************************/
        float32 computeDistanceToPoint(const Vector2& externalPoint) const
        {
            Vector2 closestPoint = computeClosestPointTo(externalPoint);
            return Vector2::distance(externalPoint, closestPoint);
        }

        /*************************************************************************
          \brief
            Test if this line segment intersects with another line segment
          \param[in] otherSegment
            Line segment to test intersection against
          \return
            True if line segments cross, touch, or overlap
          \par Usage
          \code
            if (playerMovementPath.intersectsLineSegment(wallBoundary))     // Path blocked by wall
            if (projectileTrajectory.intersectsLineSegment(obstacleEdge))   // Hit detection
          \endcode
          \par Output
            Returns true if segments intersect using parametric line intersection algorithm
          \par Purpose
            Line-to-line collision detection, path validation, and visibility testing
        *************************************************************************/
        bool intersectsLineSegment(const Line2D& otherSegment) const
        {
            Vector2 thisDirection = computeDirectionVector();
            Vector2 otherDirection = otherSegment.computeDirectionVector();
            Vector2 startPointDifference = otherSegment.startPoint - startPoint;

            float32 directionCrossProduct = thisDirection.cross(otherDirection);

            // Check if lines are parallel (cross product approaches zero)
            if (std::abs(directionCrossProduct) < FLOAT_EPSILON)
            {
                return false; // Parallel lines do not intersect
            }

            // Calculate intersection parameters for both line segments
            float32 thisSegmentParameter = startPointDifference.cross(otherDirection) / directionCrossProduct;
            float32 otherSegmentParameter = startPointDifference.cross(thisDirection) / directionCrossProduct;

            // Intersection occurs if both parameters are within [0,1] range
            return thisSegmentParameter >= 0.0f && thisSegmentParameter <= 1.0f &&
                otherSegmentParameter >= 0.0f && otherSegmentParameter <= 1.0f;
        }
    };

    /*************************************************************************
      \brief
        Stream output operator for debugging and logging
      \param[in] outputStream
        Output stream to write line segment data to
      \param[in] lineSegment
        Line2D object to output
      \return
        Reference to output stream for method chaining
      \par Usage
      \code
        std::cout << wallBoundary;              // Print line segment coordinates
        debugLog << "Vision ray: " << sightLine;
      \endcode
      \par Output
        Outputs line in format "Line2D(start: Vector2(x, y), end: Vector2(x, y))"
      \par Purpose
        Debug output and logging support for line segment objects
    *************************************************************************/
    inline std::ostream& operator<<(std::ostream& outputStream, const Line2D& lineSegment)
    {
        outputStream << "Line2D(start: " << lineSegment.startPoint << ", end: " << lineSegment.endPoint << ")";
        return outputStream;
    }
}