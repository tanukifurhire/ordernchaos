/*************************************************************************
  \file       Vector2Int.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       7 September 2025
  \version    1.0
  \brief
    Defines the Vector2Int class in the GGMath namespace.
  \details
    The Vector2Int class provides a 2D integer vector type optimised for
    grid coordinates, pixel positions, and tile-based operations. It
    offers exact precision for discrete positioning systems and efficient
    integer arithmetic.
  \dependencies
    <iostream>, Types.hpp, Vector2.hpp
  \see
    Vector2, Matrix3x3
  \history
    - 1.0 (7 September 2025): Initial version.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <iostream>
#include "Types.hpp"
#include "Vector2.hpp"

namespace GGMath
{
    /*************************************************************************
      \brief
        2D integer vector class for grid coordinates and pixel positions
      \par Usage
      \code
        Vector2Int tilePos(5, 3);
        Vector2Int screenCoord(1920, 1080);
        Vector2Int gridSize = Vector2Int::zero();
      \endcode
    *************************************************************************/
    class Vector2Int
    {
    public:
        int32 x, y;

        /*************************************************************************
          \brief
            Default constructor - creates zero vector
          \par Usage
          \code
            Vector2Int gridPos;               // Creates (0, 0)
            Vector2Int tileCoord{};          // Creates (0, 0)
          \endcode
          \par Output
          Creates vector with components (0, 0)
          \par Purpose
          Initialises vector to origin for default grid position
        *************************************************************************/
        Vector2Int() : x(0), y(0) {}

        /*************************************************************************
          \brief
            Parameterised constructor - creates vector with specified components
          \param[in] xVal
            X component (column/horizontal position)
          \param[in] yVal
            Y component (row/vertical position)
          \par Usage
          \code
            Vector2Int tilePos(5, 3);         // Tile at column 5, row 3
            Vector2Int pixelCoord(640, 480);  // Pixel position
          \endcode
          \par Output
          Creates vector with specified integer x and y components
          \par Purpose
          Direct initialisation for grid and pixel coordinates
        *************************************************************************/
        Vector2Int(int32 xVal, int32 yVal) : x(xVal), y(yVal) {}

        /*************************************************************************
          \brief
            Conversion constructor from Vector2 (truncates to integers)
          \param[in] vec
            Floating point vector to convert
          \par Usage
          \code
            Vector2 worldPos(100.7f, 50.3f);
            Vector2Int pixelPos(worldPos);    // Result: (100, 50)
          \endcode
          \par Output
          Creates integer vector by truncating float components
          \par Purpose
          Convert continuous positions to discrete grid coordinates
        *************************************************************************/
        explicit Vector2Int(const Vector2& vec) : x(static_cast<int32>(vec.x)), y(static_cast<int32>(vec.y)) {}

        /*************************************************************************
          \brief
            Compound addition - adds another vector to this one
          \param[in] rhs
            Right-hand side vector to add
          \return
            Reference to this vector after addition
          \par Usage
          \code
            Vector2Int position(5, 3);
            Vector2Int offset(1, 0);
            position += offset;             // position = (6, 3)
          \endcode
          \par Output
          Modifies this vector by adding rhs components
          \par Purpose
          Move position by offset in grid coordinates
        *************************************************************************/
        Vector2Int& operator+=(const Vector2Int& rhs)
        {
            x += rhs.x;
            y += rhs.y;
            return *this;
        }

        /*************************************************************************
          \brief
            Compound subtraction - subtracts another vector from this one
          \param[in] rhs
            Right-hand side vector to subtract
          \return
            Reference to this vector after subtraction
          \par Usage
          \code
            Vector2Int position(10, 5);
            Vector2Int offset(2, 1);
            position -= offset;             // position = (8, 4)
          \endcode
          \par Output
          Modifies this vector by subtracting rhs components
          \par Purpose
          Efficient in-place vector subtraction for grid movement
        *************************************************************************/
        Vector2Int& operator-=(const Vector2Int& rhs)
        {
            x -= rhs.x;
            y -= rhs.y;
            return *this;
        }

        /*************************************************************************
          \brief
            Vector addition - creates new vector from sum
          \param[in] rhs
            Right-hand side vector to add
          \return
            New vector representing the sum
          \par Usage
          \code
            Vector2Int newPos = tilePos + direction;
            Vector2Int total = offset1 + offset2;
          \endcode
          \par Output
          Returns new vector with summed integer components
          \par Purpose
          Calculate new grid positions from movement
        *************************************************************************/
        Vector2Int operator+(const Vector2Int& rhs) const
        {
            return Vector2Int(x + rhs.x, y + rhs.y);
        }

        /*************************************************************************
          \brief
            Vector subtraction - creates direction vector
          \param[in] rhs
            Right-hand side vector to subtract
          \return
            New vector representing the difference
          \par Usage
          \code
            Vector2Int direction = targetTile - playerTile;
            Vector2Int offset = newPos - oldPos;
          \endcode
          \par Output
          Returns new vector representing grid direction
          \par Purpose
          Calculate movement direction in tile coordinates
        *************************************************************************/
        Vector2Int operator-(const Vector2Int& rhs) const
        {
            return Vector2Int(x - rhs.x, y - rhs.y);
        }

        /*************************************************************************
          \brief
            Scalar multiplication - scales integer vector by integer factor
          \param[in] scalar
            Integer scaling factor to multiply both components
          \return
            New vector with components multiplied by scalar
          \par Usage
          \code
            Vector2Int tileSize(32, 32);
            Vector2Int doubleSize = tileSize * 2;           // Result: (64, 64)
            Vector2Int gridOffset = Vector2Int(1, 1) * 5;   // Result: (5, 5)
          \endcode
          \par Output
            Returns new Vector2Int with components (x * scalar, y * scalar)
          \par Purpose
            Scale grid coordinates for different zoom levels, tile sizes, or
            batch operations in tile-based games
        *************************************************************************/
        Vector2Int operator*(int32 scalar) const
        {
            return Vector2Int(x * scalar, y * scalar);
        }

        /*************************************************************************
          \brief
            Compound scalar multiplication - scales this vector by integer factor
          \param[in] scalar
            Integer scaling factor to multiply both components
          \return
            Reference to this vector after scaling
          \par Usage
          \code
            Vector2Int gridPos(2, 3);
            gridPos *= 4;                   // gridPos becomes (8, 12)
          \endcode
          \par Output
            Modifies this vector by multiplying both components by scalar
          \par Purpose
            Efficient in-place scaling for grid coordinate transformations
        *************************************************************************/
        Vector2Int& operator*=(int32 scalar)
        {
            x *= scalar;
            y *= scalar;
            return *this;
        }

        /*************************************************************************
          \brief
            Unary negation - returns vector with negated components
          \return
            New vector with negated x and y components
          \par Usage
          \code
            Vector2Int direction(1, -1);
            Vector2Int opposite = -direction;       // Result: (-1, 1)
            Vector2Int reversed = -Vector2Int::up(); // Result: (0, -1) which is down
          \endcode
          \par Output
            Returns new Vector2Int with components (-x, -y)
          \par Purpose
            Reverse direction vectors for opposite movement, undo operations,
            or mirror transformations in tile-based games
        *************************************************************************/
        Vector2Int operator-() const
        {
            return Vector2Int(-x, -y);
        }

        /*************************************************************************
          \brief
            Equality comparison for exact integer matching
          \param[in] rhs
            Right-hand side vector to compare
          \return
            True if both components are exactly equal
          \par Usage
          \code
            if (playerTile == targetTile)
            if (gridPos == Vector2Int::zero())
          \endcode
          \par Output
            Returns true if integer components match exactly
          \par Purpose
            Precise tile and grid position comparison
        *************************************************************************/
        bool operator==(const Vector2Int& rhs) const
        {
            return x == rhs.x && y == rhs.y;
        }

        /*************************************************************************
          \brief
            Inequality comparison for integer vectors
          \param[in] rhs
            Right-hand side vector to compare
          \return
            True if vectors are not equal
          \par Usage
          \code
            if (playerTile != lastTile)     // Player moved
            if (gridPos != Vector2Int::zero())    // Not at origin
          \endcode
          \par Output
            Returns true if any component differs
          \par Purpose
            Check for differences in grid positions
        *************************************************************************/
        bool operator!=(const Vector2Int& rhs) const
        {
            return !(*this == rhs);
        }

        /*************************************************************************
          \brief
            Calculate dot product with another integer vector
          \param[in] vector
            Vector to calculate dot product with
          \return
            Dot product value as integer
          \par Usage
          \code
            int32 alignment = gridDir1.dot(gridDir2);
            int32 projection = tileOffset.dot(gridNormal);
          \endcode
          \par Output
            Returns integer dot product (x1*x2 + y1*y2)
          \par Purpose
            Integer-based vector alignment and projection calculations
        *************************************************************************/
        int32 dot(const Vector2Int& vector) const
        {
            return x * vector.x + y * vector.y;
        }

        /*************************************************************************
          \brief
            2D cross product for integer vectors (returns Z component)
          \param[in] vector
            Vector to calculate cross product with
          \return
            Scalar Z component of cross product as integer
          \par Usage
          \code
            int32 orientation = gridDir1.cross(gridDir2);  // > 0 left, < 0 right
            int32 turn = tileMovement.cross(facingDir);
          \endcode
          \par Output
            Returns positive for counterclockwise, negative for clockwise
          \par Purpose
            Determine relative orientation in grid coordinates
        *************************************************************************/
        int32 cross(const Vector2Int& vector) const
        {
            return x * vector.y - y * vector.x;
        }

        /*************************************************************************
          \brief
            Calculate Manhattan distance between two integer vector positions
          \details
            Manhattan distance (also called city block distance or taxicab distance)
            measures the distance between two points by summing the absolute differences
            of their coordinates. This represents the shortest path distance when you
            can only move horizontally or vertically, like navigating city blocks.
          \param[in] target
            The destination position vector to measure distance to
          \return
            Manhattan distance as positive integer representing grid steps
          \par Usage
          \code
            Vector2Int playerPos(3, 5);
            Vector2Int treasurePos(7, 2);
            int32 stepsToTreasure = playerPos.manhattanDistance(treasurePos);  // Result: 7

            // Useful for pathfinding cost estimation
            if (currentTile.manhattanDistance(targetTile) <= moveRange)
            {
                // Target is reachable in one turn
            }
          \endcode
          \par Output
            Returns the sum of absolute differences: |x1-x2| + |y1-y2|
          \par Purpose
            Essential for tile-based game mechanics including:
            - Turn-based movement range calculation
            - Pathfinding heuristics (A* algorithm)
            - Attack range and area-of-effect validation
            - Proximity detection between game entities
            - Resource gathering distance checks
            - Building placement validation in strategy games
        *************************************************************************/
        int32 manhattanDistance(const Vector2Int& target) const
        {
            // Calculate the absolute difference in X coordinates
            // This represents horizontal distance in grid units
            int32 horizontalDistance = std::abs(x - target.x);

            // Calculate the absolute difference in Y coordinates  
            // This represents vertical distance in grid units
            int32 verticalDistance = std::abs(y - target.y);

            // Manhattan distance is the sum of horizontal and vertical distances
            // This gives us the minimum number of grid steps needed to reach the target
            // when only orthogonal (up/down/left/right) movement is allowed
            return horizontalDistance + verticalDistance;
        }

        /*************************************************************************
          \brief
            Convert to floating point Vector2
          \return
            Vector2 with components cast to float32
          \par Usage
          \code
            Vector2Int tilePos(5, 3);
            Vector2 worldPos = tilePos.toFloat32() * tileSize;
          \endcode
          \par Output
          Returns Vector2 with float32 components
          \par Purpose
          Convert grid coordinates to world space calculations
        *************************************************************************/
        Vector2 toFloat32() const
        {
            return Vector2(static_cast<float32>(x), static_cast<float32>(y));
        }

        /*************************************************************************
          \brief
            Get zero vector (0, 0)
          \return
            Vector with zero components
          \par Usage
          \code
            Vector2Int gridPos = Vector2Int::zero();
                if (offset != Vector2Int::zero())
          \endcode
          \par Output
            Returns vector(0, 0)
          \par Purpose
            Standard null position for grid coordinates
        *************************************************************************/
        static Vector2Int zero()
        {
            return Vector2Int(0, 0);
        }

        /*************************************************************************
          \brief
            Get right direction vector (1, 0)
          \return
            Unit vector pointing right
          \par Usage
          \code
            Vector2Int nextTile = currentTile + Vector2Int::right();
            Vector2Int moveRight = Vector2Int::right();
          \endcode
          \par Output
            Returns vector (1, 0)
          \par Purpose
            Standard direction for rightward grid movement
        *************************************************************************/
        static Vector2Int right()
        {
            return Vector2Int(1, 0);
        }

        /*************************************************************************
          \brief
            Get up direction vector (0, 1)
          \return
            Unit vector pointing up
          \par Usage
          \code
            Vector2Int above = tilePos + Vector2Int::up();
            Vector2Int upDir = Vector2Int::up();
          \endcode
          \par Output
            Returns vector (0, 1)
          \par Purpose
            Standard direction for upward grid movement
        *************************************************************************/
        static Vector2Int up()
        {
            return Vector2Int(0, 1);
        }

        /*************************************************************************
          \brief
            Get left direction vector (-1, 0)
          \return
            Unit vector pointing left
          \par Usage
          \code
            Vector2Int leftTile = currentTile + Vector2Int::left();
            Vector2Int moveLeft = Vector2Int::left();
          \endcode
          \par Output
            Returns vector (-1, 0)
          \par Purpose
            Standard direction for leftward grid movement
        *************************************************************************/
        static Vector2Int left()
        {
            return Vector2Int(-1, 0);
        }

        /*************************************************************************
          \brief
            Get down direction vector (0, -1)
          \return
            Unit vector pointing down
          \par Usage
          \code
            Vector2Int below = tilePos + Vector2Int::down();
            Vector2Int downDir = Vector2Int::down();
          \endcode
          \par Output
            Returns vector (0, -1)
          \par Purpose
            Standard direction for downward grid movement
        *************************************************************************/
        static Vector2Int down()
        {
            return Vector2Int(0, -1);
        }
    };

    /*************************************************************************
      \brief
        Scalar multiplication (integer * vector)
      \param[in] scalar
        Integer scaling factor
      \param[in] vector
        Vector to scale
      \return
        Scaled vector
      \par Usage
      \code
        Vector2Int tilePos = 3 * Vector2Int(2, 4);      // Result: (6, 12)
        Vector2Int scaled = multiplier * baseOffset;
      \endcode
      \par Output
        Returns vector scaled by scalar
      \par Purpose
        Allows natural mathematical notation for integer vector scaling
    *************************************************************************/
    inline Vector2Int operator*(int32 scalar, const Vector2Int& vector)
    {
        return vector * scalar;
    }

    /*************************************************************************
      \brief
        Stream output for debugging
      \param[in] os
        Output stream
      \param[in] vec
        Vector to output
      \return
        Reference to output stream
      \par Usage
      \code
        std::cout << tilePos;           // Prints "Vector2Int(x, y)"
        debugLog << "Grid: " << gridCoord;
      \endcode
      \par Output
        Outputs vector in format "Vector2Int(x, y)"
      \par Purpose
        Debug output and logging for integer vectors
    *************************************************************************/
    inline std::ostream& operator<<(std::ostream& os, const Vector2Int& vec)
    {
        os << "Vector2Int(" << vec.x << ", " << vec.y << ")";
        return os;
    }
}