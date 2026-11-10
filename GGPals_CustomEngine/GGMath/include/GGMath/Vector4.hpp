/*************************************************************************
  \file       Vector4.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       13 September 2025
  \version    1.0
  \brief
    Defines the Vector4 class in the GGMath namespace.
  \details
    The Vector4 class provides a 4D floating point vector type with
    constructors, arithmetic operators, geometric utilities, and
    RGBA color representation. It supports homogeneous coordinates
    for 3D transformations and provides comprehensive color manipulation.
  \dependencies
    <cmath>, <iostream>, Constants.hpp, Types.hpp, Vector3.hpp
  \see
    Vector2, Vector3, Matrix3x3
  \history
    - 1.0 (13 September 2025): Initial version with RGBA colour support.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <cmath>
#include <iostream>
#include "Constants.hpp"
#include "Types.hpp"
#include "Vector3.hpp"

namespace GGMath
{
    /*************************************************************************
      \brief
        4D floating point vector class for homogeneous coordinates and RGBA colors
      \par Usage
      \code
        Vector4 color(1.0f, 0.5f, 0.0f, 1.0f);  // Orange with full alpha
        Vector4 position(10.0f, 20.0f, 5.0f, 1.0f);  // Homogeneous coordinate
        Vector4 transparent = Vector4::red() * 0.5f;  // Semi-transparent red
      \endcode
    *************************************************************************/
    class Vector4
    {
    public:
        float32 x, y, z, w;

        /*************************************************************************
          \brief
            Default constructor - creates zero vector
          \par Usage
          \code
            Vector4 color;              // Creates (0, 0, 0, 0) - transparent black
            Vector4 position;           // Creates (0, 0, 0, 0)
          \endcode
          \par Output
            Creates vector with components (0, 0, 0, 0)
          \par Purpose
            Initialises vector to origin for default state
        *************************************************************************/
        Vector4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}

        /*************************************************************************
          \brief
            Parameterised constructor - creates vector with specified components
          \param[in] xVal
            X component (Red for colors)
          \param[in] yVal
            Y component (Green for colors)
          \param[in] zVal
            Z component (Blue for colors)
          \param[in] wVal
            W component (Alpha for colors)
          \par Usage
          \code
            Vector4 color(1.0f, 0.5f, 0.0f, 1.0f);    // Orange color
            Vector4 position(100.0f, 200.0f, 50.0f, 1.0f);  // 3D position
          \endcode
          \par Output
            Creates vector with specified x, y, z, and w components
          \par Purpose
            Direct initialisation of vector components
        *************************************************************************/
        Vector4(float32 xVal, float32 yVal, float32 zVal, float32 wVal)
            : x(xVal), y(yVal), z(zVal), w(wVal) {
        }

        /*************************************************************************
          \brief
            Constructor from Vector3 and W component
          \param[in] xyz
            Vector3 for X, Y, Z components
          \param[in] wVal
            W component value
          \par Usage
          \code
            Vector3 rgb(1.0f, 0.5f, 0.0f);
            Vector4 color(rgb, 1.0f);           // Orange with full alpha

            Vector3 position3D(100, 200, 50);
            Vector4 homogeneous(position3D, 1.0f);  // Homogeneous coordinate
          \endcode
          \par Output
            Creates Vector4 using Vector3 for first three components
          \par Purpose
            Convert Vector3 to Vector4 with specified W component
        *************************************************************************/
        Vector4(const Vector3& xyz, float32 wVal) : x(xyz.x), y(xyz.y), z(xyz.z), w(wVal) {}

        /*************************************************************************
          \brief
            Uniform value constructor - creates vector with same value for all components
          \param[in] value
            Value for all four components
          \par Usage
          \code
            Vector4 grey(0.5f);         // Creates (0.5, 0.5, 0.5, 0.5) - grey with alpha
            Vector4 ones(1.0f);         // Creates (1.0, 1.0, 1.0, 1.0) - opaque white
          \endcode
          \par Output
            Creates vector with all components set to same value
          \par Purpose
            Convenient creation of uniform vectors and grey colors
        *************************************************************************/
        explicit Vector4(float32 value) : x(value), y(value), z(value), w(value) {}

        /*************************************************************************
          \brief
            Compound addition - adds another vector to this one
          \param[in] rhs
            Right-hand side vector to add
          \return
            Reference to this vector after addition
          \par Usage
          \code
            Vector4 color1(1.0f, 0.0f, 0.0f, 1.0f);
            Vector4 color2(0.0f, 0.5f, 0.0f, 0.0f);
            color1 += color2;       // color1 = (1.0, 0.5, 0.0, 1.0)
          \endcode
          \par Output
            Modifies this vector by adding rhs components
          \par Purpose
            Efficient in-place vector addition for color blending
        *************************************************************************/
        Vector4& operator+=(const Vector4& rhs)
        {
            x += rhs.x;
            y += rhs.y;
            z += rhs.z;
            w += rhs.w;
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
            Vector4 color1(1.0f, 1.0f, 1.0f, 1.0f);
            Vector4 color2(0.2f, 0.1f, 0.0f, 0.0f);
            color1 -= color2;       // color1 = (0.8, 0.9, 1.0, 1.0)
          \endcode
          \par Output
            Modifies this vector by subtracting rhs components
          \par Purpose
            Efficient in-place vector subtraction for color operations
        *************************************************************************/
        Vector4& operator-=(const Vector4& rhs)
        {
            x -= rhs.x;
            y -= rhs.y;
            z -= rhs.z;
            w -= rhs.w;
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
            Vector4 color(1.0f, 0.5f, 0.0f, 1.0f);
            color *= 0.8f;          // Darken color to (0.8, 0.4, 0.0, 0.8)
          \endcode
          \par Output
            Modifies this vector by scaling all components
          \par Purpose
            Efficient in-place vector scaling for brightness and alpha adjustments
        *************************************************************************/
        Vector4& operator*=(float32 scalar)
        {
            x *= scalar;
            y *= scalar;
            z *= scalar;
            w *= scalar;
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
            Vector4 result = baseColor + tintColor;
            Vector4 blended = color1 + color2;
          \endcode
          \par Output
            Returns new vector with summed components
          \par Purpose
            Combines vectors for color blending and additive effects
        *************************************************************************/
        Vector4 operator+(const Vector4& rhs) const
        {
            return Vector4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
        }

        /*************************************************************************
          \brief
            Vector subtraction - creates difference vector
          \param[in] rhs
            Right-hand side vector to subtract
          \return
            New vector representing the difference
          \par Usage
          \code
            Vector4 difference = targetColor - currentColor;
            Vector4 delta = newPos - oldPos;
          \endcode
          \par Output
            Returns new vector representing difference from rhs to this
          \par Purpose
            Calculate color differences and relative positions
        *************************************************************************/
        Vector4 operator-(const Vector4& rhs) const
        {
            return Vector4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
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
            Vector4 dimColor = baseColor * 0.5f;
            Vector4 brightColor = originalColor * 1.2f;
          \endcode
          \par Output
            Returns new vector scaled by the factor
          \par Purpose
            Apply brightness multipliers and alpha adjustments
        *************************************************************************/
        Vector4 operator*(float32 scalar) const
        {
            return Vector4(x * scalar, y * scalar, z * scalar, w * scalar);
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
            Vector4 normalized = homogeneous / homogeneous.w;
            Vector4 halfIntensity = color / 2.0f;
          \endcode
          \par Output
            Returns new vector with each component divided by scalar
          \par Purpose
            Homogeneous coordinate normalization and color scaling
        *************************************************************************/
        Vector4 operator/(float32 scalar) const
        {
            return Vector4(x / scalar, y / scalar, z / scalar, w / scalar);
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
            Vector4 color(2.0f, 1.0f, 0.8f, 1.0f);
            color /= 2.0f;              // color = (1.0, 0.5, 0.4, 0.5)
          \endcode
          \par Output
            Modifies this vector by dividing all components by scalar
          \par Purpose
            Efficient in-place vector scaling down
        *************************************************************************/
        Vector4& operator/=(float32 scalar)
        {
            x /= scalar;
            y /= scalar;
            z /= scalar;
            w /= scalar;
            return *this;
        }

        /*************************************************************************
          \brief
            Unary negation - returns vector with negated components
          \return
            New vector with negated x, y, z, and w components
          \par Usage
          \code
            Vector4 inverted = -color;
            Vector4 opposite = -direction;
          \endcode
          \par Output
            Returns new vector (-x, -y, -z, -w)
          \par Purpose
            Invert colors or directions
        *************************************************************************/
        Vector4 operator-() const
        {
            return Vector4(-x, -y, -z, -w);
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
            if (color == Vector4::white())    // is white color
            if (position == target)           // arrived at target
          \endcode
          \par Output
            Returns true if components differ by less than EPSILON
          \par Purpose
            Safe floating point vector comparison
        *************************************************************************/
        bool operator==(const Vector4& rhs) const
        {
            return std::abs(x - rhs.x) < FLOAT_EPSILON &&
                std::abs(y - rhs.y) < FLOAT_EPSILON &&
                std::abs(z - rhs.z) < FLOAT_EPSILON &&
                std::abs(w - rhs.w) < FLOAT_EPSILON;
        }

        /*************************************************************************
          \brief
            Get squared length of vector (avoids expensive sqrt)
          \return
            Squared magnitude of the vector
          \par Usage
          \code
            float32 distSq = (target - current).lengthSquared();
            if (color.lengthSquared() > maxBrightnessSq)  // Too bright
          \endcode
          \par Output
            Returns squared distance from origin
          \par Purpose
            Performance-optimized distance comparisons without sqrt
        *************************************************************************/
        float32 lengthSquared() const
        {
            return x * x + y * y + z * z + w * w;
        }

        /*************************************************************************
          \brief
            Calculate magnitude (length) of vector
          \return
            Length of the vector
          \par Usage
          \code
            float32 brightness = color.length();
            float32 distance = displacement.length();
          \endcode
          \par Output
            Returns positive distance from origin
          \par Purpose
            Measure vector magnitude for brightness and distance calculations
        *************************************************************************/
        float32 length() const
        {
            return std::sqrt(x * x + y * y + z * z + w * w);
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
            float32 similarity = color1.dot(color2);
            float32 alignment = direction1.dot(direction2);
          \endcode
          \par Output
            Returns scalar representing vector alignment
          \par Purpose
            Measure vector similarity and alignment in 4D space
        *************************************************************************/
        float32 dot(const Vector4& vector) const
        {
            return x * vector.x + y * vector.y + z * vector.z + w * vector.w;
        }

        /*************************************************************************
          \brief
            Get normalized copy of vector (unit length)
          \return
            Unit vector in same direction, or zero if length too small
          \par Usage
          \code
            Vector4 direction = toTarget.normalized();
            Vector4 unitColor = color.normalized();
          \endcode
          \par Output
            Returns vector with length 1.0 in same direction
          \par Purpose
            Extract pure direction without magnitude
        *************************************************************************/
        Vector4 normalized() const
        {
            float32 len = length();
            if (len > FLOAT_EPSILON)
            {
                return Vector4(x / len, y / len, z / len, w / len);
            }
            return Vector4::zero();
        }

        /*************************************************************************
          \brief
            Get XYZ components as Vector3 (ignoring W)
          \return
            Vector3 containing only the X, Y, Z components
          \par Usage
          \code
            Vector4 homogeneous(10, 20, 30, 1);
            Vector3 position = homogeneous.xyz();    // (10, 20, 30)

            Vector4 color(1, 0.5, 0, 0.8);
            Vector3 rgb = color.xyz();               // (1, 0.5, 0)
          \endcode
          \par Output
            Returns Vector3 with first three components
          \par Purpose
            Convert 4D homogeneous coordinates to 3D or extract RGB from RGBA
        *************************************************************************/
        Vector3 xyz() const
        {
            return Vector3(x, y, z);
        }

        /*************************************************************************
          \brief
            Clamp all components to [0, 1] range for valid color representation
          \return
            Vector4 with all components clamped to [0, 1]
          \par Usage
          \code
            Vector4 validColor = rawColor.clampColor();
            Vector4 normalizedRGBA = computation.clampColor();
          \endcode
          \par Output
            Returns vector with components constrained to valid color range
          \par Purpose
            Ensure color values are in valid range for rendering
        *************************************************************************/
        Vector4 clampColor() const
        {
            return Vector4(
                std::max(0.0f, std::min(1.0f, x)),
                std::max(0.0f, std::min(1.0f, y)),
                std::max(0.0f, std::min(1.0f, z)),
                std::max(0.0f, std::min(1.0f, w))
            );
        }

        /*************************************************************************
          \brief
            Get zero vector (0, 0, 0, 0)
          \return
            Vector with zero components
          \par Usage
          \code
            Vector4 transparent = Vector4::zero();
            Vector4 origin = Vector4::zero();
          \endcode
          \par Output
            Returns vector (0, 0, 0, 0)
          \par Purpose
            Standard null vector and transparent color
        *************************************************************************/
        static Vector4 zero()
        {
            return Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        }

        /*************************************************************************
          \brief
            Get red color with full alpha (1, 0, 0, 1)
          \return
            Pure red RGBA color
          \par Usage
          \code
            Vector4 errorColor = Vector4::red();
            material.color = Vector4::red();
          \endcode
          \par Output
            Returns RGBA color (1, 0, 0, 1)
          \par Purpose
            Standard red color for UI and materials
        *************************************************************************/
        static Vector4 red()
        {
            return Vector4(1.0f, 0.0f, 0.0f, 1.0f);
        }

        /*************************************************************************
          \brief
            Get green color with full alpha (0, 1, 0, 1)
          \return
            Pure green RGBA color
          \par Usage
          \code
            Vector4 successColor = Vector4::green();
            material.color = Vector4::green();
          \endcode
          \par Output
            Returns RGBA color (0, 1, 0, 1)
          \par Purpose
            Standard green color for UI and materials
        *************************************************************************/
        static Vector4 green()
        {
            return Vector4(0.0f, 1.0f, 0.0f, 1.0f);
        }

        /*************************************************************************
          \brief
            Get blue color with full alpha (0, 0, 1, 1)
          \return
            Pure blue RGBA color
          \par Usage
          \code
            Vector4 infoColor = Vector4::blue();
            material.color = Vector4::blue();
          \endcode
          \par Output
            Returns RGBA color (0, 0, 1, 1)
          \par Purpose
            Standard blue color for UI and materials
        *************************************************************************/
        static Vector4 blue()
        {
            return Vector4(0.0f, 0.0f, 1.0f, 1.0f);
        }

        /*************************************************************************
          \brief
            Get white color with full alpha (1, 1, 1, 1)
          \return
            White RGBA color
          \par Usage
          \code
            Vector4 defaultColor = Vector4::white();
            material.color = Vector4::white();
          \endcode
          \par Output
            Returns RGBA color (1, 1, 1, 1)
          \par Purpose
            Standard white color for default materials and lighting
        *************************************************************************/
        static Vector4 white()
        {
            return Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        /*************************************************************************
          \brief
            Get black color with full alpha (0, 0, 0, 1)
          \return
            Black RGBA color
          \par Usage
          \code
            Vector4 shadowColor = Vector4::black();
            material.color = Vector4::black();
          \endcode
          \par Output
            Returns RGBA color (0, 0, 0, 1)
          \par Purpose
            Standard black color for shadows and dark materials
        *************************************************************************/
        static Vector4 black()
        {
            return Vector4(0.0f, 0.0f, 0.0f, 1.0f);
        }

        /*************************************************************************
          \brief
            Get transparent black color (0, 0, 0, 0)
          \return
            Fully transparent RGBA color
          \par Usage
          \code
            Vector4 invisible = Vector4::transparent();
            fadeColor = Vector4::transparent();
          \endcode
          \par Output
            Returns RGBA color (0, 0, 0, 0)
          \par Purpose
            Fully transparent color for fading effects
        *************************************************************************/
        static Vector4 transparent()
        {
            return Vector4(0.0f, 0.0f, 0.0f, 0.0f);
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
            Vector4 fadeColor = Vector4::lerp(visible, transparent, fadeAmount);
            Vector4 blended = Vector4::lerp(color1, color2, blendFactor);
          \endcode
          \par Output
            Returns vector blended between start and end
          \par Purpose
            Smooth color transitions and animations
        *************************************************************************/
        static Vector4 lerp(const Vector4& start, const Vector4& end, float32 t)
        {
            return start + (end - start) * t;
        }

        /*************************************************************************
          \brief
            Calculate distance between two points in 4D space
          \param[in] pointA
            First point
          \param[in] pointB
            Second point
          \return
            Distance between the points
          \par Usage
          \code
            float32 colorDiff = Vector4::distance(color1, color2);
            float32 dist = Vector4::distance(pos1, pos2);
          \endcode
          \par Output
            Returns distance as positive float32
          \par Purpose
            Color similarity measurement and 4D distance calculations
        *************************************************************************/
        static float32 distance(const Vector4& pointA, const Vector4& pointB)
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
        Vector4 scaled = 0.5f * color;
        Vector4 bright = 2.0f * baseColor;
      \endcode
      \par Output
        Returns vector scaled by scalar
      \par Purpose
        Allows natural mathematical notation
    *************************************************************************/
    inline Vector4 operator*(float32 scalar, const Vector4& vector)
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
        std::cout << color;  // Prints "Vector4(r, g, b, a)"
        debugLog << "Color: " << backgroundColor;
      \endcode
      \par Output
        Outputs vector in format "Vector4(x, y, z, w)"
      \par Purpose
        Debug output and logging support
    *************************************************************************/
    inline std::ostream& operator<<(std::ostream& os, const Vector4& vector)
    {
        os << "Vector4(" << vector.x << ", " << vector.y << ", " << vector.z << ", " << vector.w << ")";
        return os;
    }
}