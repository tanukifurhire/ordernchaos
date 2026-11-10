/*************************************************************************
  \file       Vector3.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       13 September 2025
  \version    1.0
  \brief
    Defines the Vector3 class in the GGMath namespace.
  \details
    The Vector3 class provides a 3D floating point vector type with
    constructors, arithmetic operators, geometric utilities, and
    transformation functions. It serves as a foundational type for
    3D math operations, RGB color representation, and lighting calculations.
  \dependencies
    <cmath>, <iostream>, Constants.hpp, Types.hpp
  \see
    Vector2, Vector4, Matrix3x3
  \history
    - 1.0 (13 September 2025): Initial version with RGB colour support.
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
        3D floating point vector class for positions, directions, velocities, and RGB colors
      \par Usage
      \code
        Vector3 position(10.0f, 20.0f, 5.0f);
        Vector3 color = Vector3::red();
        Vector3 direction = (target - origin).normalized();
      \endcode
    *************************************************************************/
    class Vector3
    {
    public:
        float32 x, y, z;

        /*************************************************************************
          \brief
            Default constructor - creates zero vector
          \par Usage
          \code
            Vector3 position;           // Creates (0, 0, 0)
            Vector3 velocity;           // Creates (0, 0, 0)
          \endcode
          \par Output
            Creates vector with components (0, 0, 0)
          \par Purpose
            Initialises vector to origin for default state
        *************************************************************************/
        Vector3() : x(0.0f), y(0.0f), z(0.0f) {}

        /*************************************************************************
          \brief
            Parameterised constructor - creates vector with specified components
          \param[in] xVal
            X component
          \param[in] yVal
            Y component
          \param[in] zVal
            Z component
          \par Usage
          \code
            Vector3 position(100.0f, 200.0f, 50.0f);
            Vector3 color(1.0f, 0.5f, 0.0f);  // Orange color
          \endcode
          \par Output
            Creates vector with specified x, y, and z components
          \par Purpose
            Direct initialisation of vector components
        *************************************************************************/
        Vector3(float32 xVal, float32 yVal, float32 zVal) : x(xVal), y(yVal), z(zVal) {}

        /*************************************************************************
          \brief
            Uniform value constructor - creates vector with same value for all components
          \param[in] value
            Value for all three components
          \par Usage
          \code
            Vector3 grey(0.5f);         // Creates (0.5, 0.5, 0.5) - grey color
            Vector3 ones(1.0f);         // Creates (1.0, 1.0, 1.0) - white color
          \endcode
          \par Output
            Creates vector with all components set to same value
          \par Purpose
            Convenient creation of uniform vectors and grey colors
        *************************************************************************/
        explicit Vector3(float32 value) : x(value), y(value), z(value) {}

        /*************************************************************************
          \brief
            Compound addition - adds another vector to this one
          \param[in] rhs
            Right-hand side vector to add
          \return
            Reference to this vector after addition
          \par Usage
          \code
            Vector3 position(100, 100, 100);
            Vector3 velocity(5, 0, -2);
            position += velocity;       // position = (105, 100, 98)
          \endcode
          \par Output
            Modifies this vector by adding rhs components
          \par Purpose
            Efficient in-place vector addition for movement updates
        *************************************************************************/
        Vector3& operator+=(const Vector3& rhs)
        {
            x += rhs.x;
            y += rhs.y;
            z += rhs.z;
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
            Vector3 position(100, 100, 100);
            Vector3 offset(10, 5, 2);
            position -= offset;             // position = (90, 95, 98)
          \endcode
          \par Output
            Modifies this vector by subtracting rhs components
          \par Purpose
            Efficient in-place vector subtraction for movement updates
        *************************************************************************/
        Vector3& operator-=(const Vector3& rhs)
        {
            x -= rhs.x;
            y -= rhs.y;
            z -= rhs.z;
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
            Vector3 velocity(10, 5, -2);
            velocity *= 0.5f;               // velocity = (5, 2.5, -1)
          \endcode
          \par Output
            Modifies this vector by scaling all components
          \par Purpose
            Efficient in-place vector scaling for damping and acceleration
        *************************************************************************/
        Vector3& operator*=(float32 scalar)
        {
            x *= scalar;
            y *= scalar;
            z *= scalar;
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
            Vector3 result = position + velocity;
            Vector3 total = force1 + force2;
          \endcode
          \par Output
            Returns new vector with summed components
          \par Purpose
            Combines vectors for displacement calculations
        *************************************************************************/
        Vector3 operator+(const Vector3& rhs) const
        {
            return Vector3(x + rhs.x, y + rhs.y, z + rhs.z);
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
            Vector3 direction = target - player;
            Vector3 relative = worldPos - cameraPos;
          \endcode
          \par Output
            Returns new vector representing direction from rhs to this
          \par Purpose
            Calculate direction vectors and relative positions
        *************************************************************************/
        Vector3 operator-(const Vector3& rhs) const
        {
            return Vector3(x - rhs.x, y - rhs.y, z - rhs.z);
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
            Vector3 movement = velocity * deltaTime;
            Vector3 brightColor = baseColor * 1.5f;
          \endcode
          \par Output
            Returns new vector scaled by the factor
          \par Purpose
            Apply magnitude to unit vectors or scale existing vectors
        *************************************************************************/
        Vector3 operator*(float32 scalar) const
        {
            return Vector3(x * scalar, y * scalar, z * scalar);
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
            Vector3 normalized = direction / length;
            Vector3 halfIntensity = lightColor / 2.0f;
          \endcode
          \par Output
            Returns new vector with each component divided by scalar
          \par Purpose
            Scale vector down by division, useful for normalization
        *************************************************************************/
        Vector3 operator/(float32 scalar) const
        {
            return Vector3(x / scalar, y / scalar, z / scalar);
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
            Vector3 velocity(10, 20, 5);
            velocity /= 2.0f;               // velocity = (5, 10, 2.5)
          \endcode
          \par Output
            Modifies this vector by dividing all components by scalar
          \par Purpose
            Efficient in-place vector scaling down
        *************************************************************************/
        Vector3& operator/=(float32 scalar)
        {
            x /= scalar;
            y /= scalar;
            z /= scalar;
            return *this;
        }

        /*************************************************************************
          \brief
            Unary negation - returns vector with negated components
          \return
            New vector with negated x, y, and z components
          \par Usage
          \code
            Vector3 opposite = -velocity;
            Vector3 inverted = -lightDirection;
          \endcode
          \par Output
            Returns new vector (-x, -y, -z)
          \par Purpose
            Reverse vector direction for opposite movement or lighting
        *************************************************************************/
        Vector3 operator-() const
        {
            return Vector3(-x, -y, -z);
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
            if (color == Vector3::white())    // is white color
          \endcode
          \par Output
            Returns true if components differ by less than EPSILON
          \par Purpose
            Safe floating point vector comparison
        *************************************************************************/
        bool operator==(const Vector3& rhs) const
        {
            return std::abs(x - rhs.x) < FLOAT_EPSILON &&
                std::abs(y - rhs.y) < FLOAT_EPSILON &&
                std::abs(z - rhs.z) < FLOAT_EPSILON;
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
            return x * x + y * y + z * z;
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
            return std::sqrt(x * x + y * y + z * z);
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
            float32 brightness = lightDir.dot(normal);
          \endcode
          \par Output
            Returns scalar representing vector alignment
          \par Purpose
            Measure vector similarity and lighting calculations
        *************************************************************************/
        float32 dot(const Vector3& vector) const
        {
            return x * vector.x + y * vector.y + z * vector.z;
        }

        /*************************************************************************
          \brief
            Calculate cross product with another vector
          \param[in] vector
            Vector to calculate cross product with
          \return
            New vector perpendicular to both input vectors
          \par Usage
          \code
            Vector3 normal = edge1.cross(edge2);
            Vector3 rightVector = forward.cross(up);
          \endcode
          \par Output
            Returns vector perpendicular to both inputs (right-hand rule)
          \par Purpose
            Calculate normal vectors and perpendicular directions
        *************************************************************************/
        Vector3 cross(const Vector3& vector) const
        {
            return Vector3(
                y * vector.z - z * vector.y,
                z * vector.x - x * vector.z,
                x * vector.y - y * vector.x
            );
        }

        /*************************************************************************
          \brief
            Get normalized copy of vector (unit length)
          \return
            Unit vector in same direction, or zero if length too small
          \par Usage
          \code
            Vector3 direction = toTarget.normalized();
            Vector3 forward = velocity.normalized();
          \endcode
          \par Output
            Returns vector with length 1.0 in same direction
          \par Purpose
            Extract pure direction without magnitude
        *************************************************************************/
        Vector3 normalized() const
        {
            float32 len = length();
            if (len > FLOAT_EPSILON)
            {
                return Vector3(x / len, y / len, z / len);
            }
            return Vector3::zero();
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
            Vector3 limited = input.clamp(1.0f);
          \endcode
          \par Output
            Returns vector with same direction but limited magnitude
          \par Purpose
            Limit movement speed and input magnitudes
        *************************************************************************/
        Vector3 clamp(float32 maxLength) const
        {
            float32 lenSq = lengthSquared();
            if (lenSq > maxLength * maxLength)
            {
                float32 len = std::sqrt(lenSq);
                return Vector3(x * maxLength / len, y * maxLength / len, z * maxLength / len);
            }
            return *this;
        }

        /*************************************************************************
          \brief
            Get zero vector (0, 0, 0)
          \return
            Vector with zero components
          \par Usage
          \code
            Vector3 velocity = Vector3::zero();
            position = Vector3::zero();
          \endcode
          \par Output
            Returns vector (0, 0, 0)
          \par Purpose
            Standard null vector for initialisation and comparisons
        *************************************************************************/
        static Vector3 zero()
        {
            return Vector3(0.0f, 0.0f, 0.0f);
        }

        /*************************************************************************
          \brief
            Get unit vector pointing right (1, 0, 0)
          \return
            Unit vector pointing in positive X direction
          \par Usage
          \code
            Vector3 rightDir = Vector3::right();
            velocity = Vector3::right() * speed;
          \endcode
          \par Output
            Returns vector (1, 0, 0)
          \par Purpose
            Standard direction vector for rightward movement
        *************************************************************************/
        static Vector3 right()
        {
            return Vector3(1.0f, 0.0f, 0.0f);
        }

        /*************************************************************************
          \brief
            Get unit vector pointing up (0, 1, 0)
          \return
            Unit vector pointing in positive Y direction
          \par Usage
          \code
            Vector3 upDir = Vector3::up();
            jumpVelocity = Vector3::up() * jumpForce;
          \endcode
          \par Output
            Returns vector (0, 1, 0)
          \par Purpose
            Standard direction for upward movement
        *************************************************************************/
        static Vector3 up()
        {
            return Vector3(0.0f, 1.0f, 0.0f);
        }

        /*************************************************************************
          \brief
            Get unit vector pointing forward (0, 0, 1)
          \return
            Unit vector pointing in positive Z direction
          \par Usage
          \code
            Vector3 forwardDir = Vector3::forward();
            movement = Vector3::forward() * speed;
          \endcode
          \par Output
            Returns vector (0, 0, 1)
          \par Purpose
            Standard direction for forward movement in 3D space
        *************************************************************************/
        static Vector3 forward()
        {
            return Vector3(0.0f, 0.0f, 1.0f);
        }

        /*************************************************************************
          \brief
            Get red color vector (1, 0, 0)
          \return
            Pure red color as RGB vector
          \par Usage
          \code
            Vector3 errorColor = Vector3::red();
            material.color = Vector3::red();
          \endcode
          \par Output
            Returns RGB color (1, 0, 0)
          \par Purpose
            Standard red color for UI and materials
        *************************************************************************/
        static Vector3 red()
        {
            return Vector3(1.0f, 0.0f, 0.0f);
        }

        /*************************************************************************
          \brief
            Get green color vector (0, 1, 0)
          \return
            Pure green color as RGB vector
          \par Usage
          \code
            Vector3 successColor = Vector3::green();
            material.color = Vector3::green();
          \endcode
          \par Output
            Returns RGB color (0, 1, 0)
          \par Purpose
            Standard green color for UI and materials
        *************************************************************************/
        static Vector3 green()
        {
            return Vector3(0.0f, 1.0f, 0.0f);
        }

        /*************************************************************************
          \brief
            Get blue color vector (0, 0, 1)
          \return
            Pure blue color as RGB vector
          \par Usage
          \code
            Vector3 infoColor = Vector3::blue();
            material.color = Vector3::blue();
          \endcode
          \par Output
            Returns RGB color (0, 0, 1)
          \par Purpose
            Standard blue color for UI and materials
        *************************************************************************/
        static Vector3 blue()
        {
            return Vector3(0.0f, 0.0f, 1.0f);
        }

        /*************************************************************************
          \brief
            Get white color vector (1, 1, 1)
          \return
            White color as RGB vector
          \par Usage
          \code
            Vector3 defaultColor = Vector3::white();
            material.color = Vector3::white();
          \endcode
          \par Output
            Returns RGB color (1, 1, 1)
          \par Purpose
            Standard white color for default materials and lighting
        *************************************************************************/
        static Vector3 white()
        {
            return Vector3(1.0f, 1.0f, 1.0f);
        }

        /*************************************************************************
          \brief
            Get black color vector (0, 0, 0)
          \return
            Black color as RGB vector
          \par Usage
          \code
            Vector3 shadowColor = Vector3::black();
            material.color = Vector3::black();
          \endcode
          \par Output
            Returns RGB color (0, 0, 0)
          \par Purpose
            Standard black color for shadows and dark materials
        *************************************************************************/
        static Vector3 black()
        {
            return Vector3(0.0f, 0.0f, 0.0f);
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
            Vector3 smooth = Vector3::lerp(startPos, endPos, 0.5f);
            Vector3 blendedColor = Vector3::lerp(color1, color2, blendFactor);
          \endcode
          \par Output
            Returns vector blended between start and end
          \par Purpose
            Smooth animations, color blending, and camera following
        *************************************************************************/
        static Vector3 lerp(const Vector3& start, const Vector3& end, float32 t)
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
            float32 dist = Vector3::distance(player, enemy);
            if (Vector3::distance(pos, target) < 5.0f)  // close
          \endcode
          \par Output
            Returns distance as positive float32
          \par Purpose
            Range checking and collision detection in 3D space
        *************************************************************************/
        static float32 distance(const Vector3& pointA, const Vector3& pointB)
        {
            return (pointB - pointA).length();
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
        Vector3 scaled = 5.0f * direction;
        Vector3 brightColor = 2.0f * baseColor;
      \endcode
      \par Output
        Returns vector scaled by scalar
      \par Purpose
        Allows natural mathematical notation
    *************************************************************************/
    inline Vector3 operator*(float32 scalar, const Vector3& vector)
    {
        return vector * scalar;
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
        std::cout << position;  // Prints "Vector3(x, y, z)"
        debugLog << "Color: " << color;
      \endcode
      \par Output
        Outputs vector in format "Vector3(x, y, z)"
      \par Purpose
        Debug output and logging support
    *************************************************************************/
    inline std::ostream& operator<<(std::ostream& os, const Vector3& vector)
    {
        os << "Vector3(" << vector.x << ", " << vector.y << ", " << vector.z << ")";
        return os;
    }
}