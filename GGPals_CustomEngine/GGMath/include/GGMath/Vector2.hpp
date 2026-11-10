/*************************************************************************
  \file       Vector2.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       6 September 2025
  \version    1.0
  \brief
    Defines the Vector2 class in the GGMath namespace.
  \details
    The Vector2 class provides a 2D floating point vector type with
    constructors, arithmetic operators, geometric utilities, and
    transformation functions. It serves as a foundational type for
    2D math operations in the GGMath library.
  \dependencies
    <cmath>, <iostream>, Constants.hpp, Types.hpp
  \see
    Vector2Int, Matrix3x3
  \history
    - 1.0 (6 September 2025): Initial version.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <cmath>
#include <iostream>
#include "Constants.hpp"
#include "Types.hpp"

namespace GGMath
{
    /*************************************************************************
      \brief
        2D floating point vector class for positions, directions, and velocities
      \par Usage
      \code
        Vector2 position(100.0f, 200.0f);
        Vector2 velocity = Vector2::right() * speed;
        Vector2 direction = (target - player).normalized();
      \endcode
    *************************************************************************/
    class Vector2
    {
    public:
        float32 x, y;

        /*************************************************************************
          \brief
            Default constructor - creates zero vector
          \par Usage
          \code
            Vector2 position;           // Creates (0, 0)
            Vector2 velocity;           // Creates (0, 0)
          \endcode
          \par Output
            Creates vector with components (0, 0)
          \par Purpose
            Initialises vector to origin for default state
        *************************************************************************/
        Vector2() : x(0.0f), y(0.0f) {}

        /*************************************************************************
          \brief
            Parameterised constructor - creates vector with specified components
          \param[in] xVal
            X component
          \param[in] yVal
            Y component
          \par Usage
          \code
            Vector2 position(100.0f, 200.0f);
            Vector2 velocity(5.0f, -3.0f);
          \endcode
          \par Output
            Creates vector with specified x and y components
          \par Purpose
            Direct initialisation of vector components
        *************************************************************************/
        Vector2(float32 xVal, float32 yVal) : x(xVal), y(yVal) {}

        /*************************************************************************
          \brief
            Compound addition - adds another vector to this one
          \param[in] rhs
            Right-hand side vector to add
          \return
            Reference to this vector after addition
          \par Usage
          \code
            Vector2 position(100, 100);
            Vector2 velocity(5, 0);
            position += velocity;       // position = (105, 100)
          \endcode
          \par Output
            Modifies this vector by adding rhs components
          \par Purpose
            Efficient in-place vector addition, commonly used for updating
            positions by velocity or accumulating forces.
        *************************************************************************/
        Vector2& operator+=(const Vector2& rhs)
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
            Vector2 position(100, 100);
            Vector2 offset(10, 5);
            position -= offset;             // position = (90, 95)
          \endcode
          \par Output
            Modifies this vector by subtracting rhs components
          \par Purpose
            Efficient in-place vector subtraction for movement updates
        *************************************************************************/
        Vector2& operator-=(const Vector2& rhs)
        {
            x -= rhs.x;
            y -= rhs.y;
            return *this;
        }

        /*************************************************************************
          \brief
            Compound scalar multiplication - scales this vector by factor
          \param[in] scalar
            Scaling factor
          \return
            Reference to this vector after scaling
          \par Usage
          \code
            Vector2 velocity(10, 5);
            velocity *= 0.5f;               // velocity = (5, 2.5)
          \endcode
          \par Output
            Modifies this vector by scaling both components
          \par Purpose
            Efficient in-place vector scaling for damping and acceleration
        *************************************************************************/
        Vector2& operator*=(float32 scalar)
        {
            x *= scalar;
            y *= scalar;
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
            Vector2 result = position + velocity;
            Vector2 total = force1 + force2;
          \endcode
          \par Output
            Returns new vector with summed components
          \par Purpose
            Combines vectors for displacement calculations
        *************************************************************************/
        Vector2 operator+(const Vector2& rhs) const
        {
            return Vector2(x + rhs.x, y + rhs.y);
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
            Vector2 direction = target - player;
            Vector2 relative = worldPos - cameraPos;
          \endcode
          \par Output
            Returns new vector representing direction from rhs to this
          \par Purpose
            Calculate direction vectors and relative positions
        *************************************************************************/
        Vector2 operator-(const Vector2& rhs) const
        {
            return Vector2(x - rhs.x, y - rhs.y);
        }

        /*************************************************************************
          \brief
            Scalar multiplication - scales vector by factor
          \param[in] scalar
            Scaling factor
          \return
            New scaled vector
          \par Usage
          \code
            Vector2 movement = velocity * deltaTime;
            Vector2 scaled = direction * speed;
          \endcode
          \par Output
            Returns new vector scaled by the factor
          \par Purpose
            Apply magnitude to unit vectors or scale existing vectors
        *************************************************************************/
        Vector2 operator*(float32 scalar) const
        {
            return Vector2(x * scalar, y * scalar);
        }

        /*************************************************************************
          \brief
            Division by scalar - divides vector components by scalar value
          \param[in] scalar
            Division factor (must not be zero)
          \return
            New vector with components divided by scalar
          \par Usage
          \code
            Vector2 normalized = direction / length;
            Vector2 halfSpeed = velocity / 2.0f;
          \endcode
          \par Output
            Returns new vector with each component divided by scalar
          \par Purpose
            Scale vector down by division, useful for normalization
        *************************************************************************/
        Vector2 operator/(float32 scalar) const
        {
            return Vector2(x / scalar, y / scalar);
        }

        /*************************************************************************
          \brief
            Compound division - divides this vector by scalar
          \param[in] scalar
            Division factor (must not be zero)
          \return
            Reference to this vector after division
          \par Usage
          \code
            Vector2 velocity(10, 20);
            velocity /= 2.0f;               // velocity = (5, 10)
          \endcode
          \par Output
            Modifies this vector by dividing both components by scalar
          \par Purpose
            Efficient in-place vector scaling down
        *************************************************************************/
        Vector2& operator/=(float32 scalar)
        {
            x /= scalar;
            y /= scalar;
            return *this;
        }

        /*************************************************************************
          \brief
            Unary negation - returns vector with negated components
          \return
            New vector with negated x and y components
          \par Usage
          \code
            Vector2 opposite = -velocity;
            Vector2 reversed = -direction;
          \endcode
          \par Output
            Returns new vector (-x, -y)
          \par Purpose
            Reverse vector direction for opposite movement
        *************************************************************************/
        Vector2 operator-() const
        {
            return Vector2(-x, -y);
        }

        /*************************************************************************
          \brief
            Equality comparison with epsilon tolerance
          \param[in] rhs
            Right-hand side vector to compare
          \return
            True if vectors are approximately equal
          \par Usage
          \code
            if (position == target)     // arrived
            if (velocity == Vector2::zero())    // stationary
          \endcode
          \par Output
            Returns true if components differ by less than EPSILON
          \par Purpose
            Safe floating point vector comparison
        *************************************************************************/
        bool operator==(const Vector2& rhs) const
        {
            return std::abs(x - rhs.x) < FLOAT_EPSILON && std::abs(y - rhs.y) < FLOAT_EPSILON;
        }

        /*************************************************************************
          \brief
            Get angle of vector in radians
          \return
            Angle from positive X-axis in radians [-PI, PI]
          \par Usage
          \code
            float32 direction = velocity.angle();
            float32 targetAngle = (target - player).angle();
          \endcode
          \par Output
            Returns angle in radians using atan2
          \par Purpose
            Extract rotation angle from direction vectors
        *************************************************************************/
        float32 angle() const
        {
            return std::atan2(y, x);
        }

        /*************************************************************************
          \brief
            Get squared length of vector (avoids expensive sqrt)
          \return
            Squared magnitude of the vector
          \par Usage
          \code
            float32 distSq = (target - player).lengthSquared();
            if (velocity.lengthSquared() > maxSpeedSq)  // Fast speed check
          \endcode
          \par Output
            Returns squared distance from origin
          \par Purpose
            Performance-optimized distance comparisons without sqrt
        *************************************************************************/
        float32 lengthSquared() const
        {
            return x * x + y * y;
        }

        /*************************************************************************
          \brief
            Calculate dot product with another vector
          \param[in] vector
            Vector to calculate dot product with
          \return
            Dot product value
          \par Usage
          \code
            float32 alignment = forward.dot(toTarget);    // > 0 same direction
            float32 projection = velocity.dot(normal);
          \endcode
          \par Output
            Returns scalar representing vector alignment
          \par Purpose
            Measure vector similarity and project vectors
        *************************************************************************/
        float32 dot(const Vector2& vector) const
        {
            return x * vector.x + y * vector.y;
        }

        /*************************************************************************
          \brief
            2D cross product (returns Z component of 3D cross)
          \param[in] vector
            Vector to calculate cross product with
          \return
            Scalar Z component of cross product
          \par Usage
          \code
            float32 orientation = forward.cross(toTarget);  // > 0 left, < 0 right
            float32 turn = velocity.cross(steering);
          \endcode
          \par Output
            Returns positive for counterclockwise, negative for clockwise
          \par Purpose
            Determine relative orientation and rotation direction
        *************************************************************************/
        float32 cross(const Vector2& vector) const
        {
            return x * vector.y - y * vector.x;
        }

        /*************************************************************************
          \brief
            Calculate magnitude (length) of vector
          \return
            Length of the vector
          \par Usage
          \code
            float32 speed = velocity.length();
            float32 distance = displacement.length();
          \endcode
          \par Output
            Returns positive distance from origin
          \par Purpose
            Measure vector magnitude for distance and speed calculations
        *************************************************************************/
        float32 length() const
        {
            return std::sqrt(x * x + y * y);
        }

        /*************************************************************************
          \brief
            Get normalized copy of vector (unit length)
          \return
            Unit vector in same direction, or zero if length too small
          \par Usage
          \code
            Vector2 direction = toTarget.normalized();
            Vector2 forward = velocity.normalized();
          \endcode
          \par Output
            Returns vector with length 1.0 in same direction
          \par Purpose
            Extract pure direction without magnitude
        *************************************************************************/
        Vector2 normalized() const
        {
            float32 len = length();
            if (len > FLOAT_EPSILON)
            {
                return Vector2(x / len, y / len);
            }
            return Vector2::zero();
        }

        /*************************************************************************
          \brief
            Reflect vector across a surface normal
          \param[in] normal
            Surface normal vector (should be normalized)
          \return
            Reflected vector
          \par Usage
          \code
            Vector2 bounced = velocity.reflect(wallNormal);
            Vector2 mirror = lightRay.reflect(surfaceNormal);
          \endcode
          \par Output
            Returns vector reflected across the normal
          \par Purpose
            Calculate collision bounces and light reflections
        *************************************************************************/
        Vector2 reflect(const Vector2& normal) const
        {
            float32 dotProduct = dot(normal);
            float32 scaleFactor = static_cast<float32>(2.0) * dotProduct;
            return *this - normal * scaleFactor;
        }

        /*************************************************************************
          \brief
            Rotate vector by specified angle
          \param[in] radians
            Rotation angle in radians
          \return
            New rotated vector
          \par Usage
          \code
            Vector2 forward = Vector2::right().rotate(playerAngle);
            Vector2 rotated = velocity.rotate(QUARTER_CIRCLE);
          \endcode
          \par Output
            Returns vector rotated counterclockwise by angle
          \par Purpose
            Apply rotation to direction vectors and movement
        *************************************************************************/
        Vector2 rotate(float32 radians) const
        {
            float32 c = std::cos(radians);
            float32 s = std::sin(radians);
            return Vector2(x * c - y * s, x * s + y * c);
        }

        /*************************************************************************
          \brief
            Clamp vector magnitude to maximum length
          \param[in] maxLength
            Maximum allowed magnitude
          \return
            Vector with magnitude clamped to maxLength
          \par Usage
          \code
            velocity = velocity.clamp(maxSpeed);
            Vector2 limited = input.clamp(1.0f);
          \endcode
          \par Output
            Returns vector with same direction but limited magnitude
          \par Purpose
            Limit movement speed and input magnitudes
        *************************************************************************/
        Vector2 clamp(float32 maxLength) const
        {
            float32 lenSq = lengthSquared();
            if (lenSq > maxLength * maxLength)
            {
                float32 len = std::sqrt(lenSq);
                return Vector2(x * maxLength / len, y * maxLength / len);
            }
            return *this;
        }

        /*************************************************************************
          \brief
            Get zero vector (0, 0)
          \return
            Vector with zero components
          \par Usage
          \code
            Vector2 velocity = Vector2::zero();
            position = Vector2::zero();
          \endcode
          \par Output
            Returns vector (0, 0)
          \par Purpose
            Standard null vector for initialisation and comparisons
        *************************************************************************/
        static Vector2 zero()
        {
            return Vector2(0.0f, 0.0f);
        }

        /*************************************************************************
          \brief
            Get right vector (1, 0)
          \return
            Unit vector pointing right
          \par Usage
          \code
            Vector2 velocity = Vector2::right() * speed;
            Vector2 rightDir = Vector2::right();
          \endcode
          \par Output
            Returns vector (1, 0)
          \par Purpose
            Standard direction vector for rightward movement
        *************************************************************************/
        static Vector2 right()
        {
            return Vector2(1.0f, 0.0f);
        }

        /*************************************************************************
          \brief
            Get left vector (-1, 0)
          \return
            Unit vector pointing left
          \par Usage
          \code
            velocity = Vector2::left() * moveSpeed;
            Vector2 leftDir = Vector2::left();
          \endcode
          \par Output
            Returns vector (-1, 0)
          \par Purpose
            Standard direction for leftward movement
        *************************************************************************/
        static Vector2 left()
        {
            return Vector2(-1.0f, 0.0f);
        }

        /*************************************************************************
          \brief
            Get down vector (0, -1)
          \return
            Unit vector pointing down
          \par Usage
          \code
            velocity += Vector2::down() * gravity * deltaTime;
            Vector2 downDir = Vector2::down();
          \endcode
          \par Output
            Returns vector (0, -1)
          \par Purpose
            Standard direction for downward movement and gravity
        *************************************************************************/
        static Vector2 down()
        {
            return Vector2(0.0f, -1.0f);
        }

        /*************************************************************************
          \brief
            Linear interpolation between two vectors
          \param[in] start
            Start vector (t = 0)
          \param[in] end
            End vector (t = 1)
          \param[in] t
            Interpolation factor [0, 1]
          \return
            Interpolated vector between start and end
          \par Usage
          \code
            Vector2 smooth = Vector2::lerp(startPos, endPos, 0.5f);
            Vector2 camera = Vector2::lerp(camera.pos, target, smoothing);
          \endcode
          \par Output
            Returns vector blended between start and end
          \par Purpose
            Smooth animations and camera following
        *************************************************************************/
        static Vector2 lerp(const Vector2& start, const Vector2& end, float32 t)
        {
            return start + (end - start) * t;
        }

        /*************************************************************************
          \brief
            Calculate distance between two points
          \param[in] pointA
            First point
          \param[in] pointB
            Second point
          \return
            Distance between the points
          \par Usage
          \code
            float32 dist = Vector2::distance(player, enemy);
            if (Vector2::distance(pos, target) < 5.0f)  // close
          \endcode
          \par Output
            Returns distance as positive float32
          \par Purpose
            Range checking and collision detection
        *************************************************************************/
        static float32 distance(const Vector2& pointA, const Vector2& pointB)
        {
            return (pointB - pointA).length();
        }

        /*************************************************************************
          \brief
            Get up vector (0, 1)
          \return
            Unit vector pointing up
          \par Usage
          \code
            Vector2 upDir = Vector2::up();
          \endcode
          \par Output
            Returns vector (0, 1)
          \par Purpose
            Standard direction for upward movement
        *************************************************************************/
        static Vector2 up()
        {
            return Vector2(0.0f, 1.0f);
        }
    };

    /*************************************************************************
      \brief
        Scalar multiplication (scalar * vector)
      \param[in] scalar
        Scaling factor
      \param[in] vector
        Vector to scale
      \return
        Scaled vector
      \par Usage
      \code
        Vector2 scaled = 5.0f * direction;
        Vector2 force = mass * acceleration;
      \endcode
      \par Output
        Returns vector scaled by scalar
      \par Purpose
        Allows natural mathematical notation
    *************************************************************************/
    inline Vector2 operator*(float32 scalar, const Vector2& vector)
    {
        return vector * scalar;
    }

    /*************************************************************************
      \brief
        Division operator (scalar / vector) - component-wise division
      \param[in] scalar
        Numerator scalar value
      \param[in] vector
        Vector denominator
      \return
        New vector with scalar divided by each component
      \par Usage
      \code
        Vector2 reciprocal = 1.0f / someVector;
      \endcode
      \par Output
        Returns vector (scalar/x, scalar/y)
      \par Purpose
        Component-wise scalar division (rarely used, but for completeness)
    *************************************************************************/
    inline Vector2 operator/(float32 scalar, const Vector2& vector)
    {
        return Vector2(scalar / vector.x, scalar / vector.y);
    }

    /*************************************************************************
      \brief
        Stream output for debugging
      \param[in] os
        Output stream
      \param[in] vector
        Vector to output
      \return
        Reference to output stream
      \par Usage
      \code
        std::cout << position;  // Prints "Vector2(x, y)"
        debugLog << "Velocity: " << velocity;
      \endcode
      \par Output
        Outputs vector in format "Vector2(x, y)"
      \par Purpose
        Debug output and logging support
    *************************************************************************/
    inline std::ostream& operator<<(std::ostream& os, const Vector2& vector)
    {
        os << "Vector2(" << vector.x << ", " << vector.y << ")";
        return os;
    }
}