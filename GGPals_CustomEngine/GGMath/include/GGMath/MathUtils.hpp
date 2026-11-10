/*************************************************************************
  \file       MathUtils.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       6 September 2025
  \version    1.0
  \brief
    Defines utility functions in the GGMath namespace.
  \details
    This file provides mathematical utility functions for interpolation,
    clamping, angle conversion, and common game mathematics operations.
    All functions are designed for performance and ease of use in 2D
    game development.
  \dependencies
    <cmath>, <algorithm>, Constants.hpp, Types.hpp
  \see
    Vector2, Vector3, Constants
  \history
    - 1.0 (6 September 2025): Initial version.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <cmath>       // std::lerp, std::abs
#include <algorithm>   // std::clamp
#include "Constants.hpp"
#include "Types.hpp"

namespace GGMath
{
    /*************************************************************************
      \brief
        Converts degrees to radians
      \param[in] degrees
        Angle in degrees
      \return
        Angle in radians
      \par Usage
      \code
        float32 radians = toRadians(90.0f);    // Result: PI/2
        float32 angle = toRadians(180.0f);     // Result: PI
      \endcode
      \par Output
        Returns the angle converted to radians
      \par Purpose
        Standard conversion for trigonometric functions which expect radians
    *************************************************************************/
    inline constexpr float32 toRadians(float32 degrees) noexcept
    {
        return degrees * GGMath::DEGREES_TO_RADIANS;
    }

    /*************************************************************************
      \brief
        Converts radians to degrees
      \param[in] radians
        Angle in radians
      \return
        Angle in degrees
      \par Usage
      \code
        float32 degrees = toDegrees(PI);       // Result: 180.0f
        float32 angle = toDegrees(HALF_PI);    // Result: 90.0f
      \endcode
      \par Output
        Returns the angle converted to degrees
      \par Purpose
        Conversion for display or human-readable angle values
    *************************************************************************/
    inline constexpr float32 toDegrees(float32 radians) noexcept
    {
        return radians * GGMath::RADIANS_TO_DEGREES;
    }

    /*************************************************************************
      \brief
        Clamps a value between minimum and maximum bounds
      \param[in] value
        Value to clamp
      \param[in] minVal
        Minimum allowed value
      \param[in] maxVal
        Maximum allowed value
      \return
        Clamped value
      \par Usage
      \code
        float32 health = clamp(damage, 0.0f, 100.0f);
        int32 index = clamp(mouseX, 0, screenWidth);
      \endcode
      \par Output
        Returns value constrained to [minVal, maxVal] range
      \par Purpose
        Ensures values stay within valid bounds
    *************************************************************************/
    template<typename T>
    inline constexpr T clamp(const T& value, const T& minVal, const T& maxVal) noexcept
    {
        // Precondition: minVal <= maxVal
        return std::clamp(value, minVal, maxVal);
    }

    /*************************************************************************
      \brief
        Linear interpolation between two values
      \param[in] start
        Starting value (t = 0)
      \param[in] end
        Ending value (t = 1)
      \param[in] t
        Interpolation parameter [0, 1]
      \return
        Interpolated value
      \par Usage
      \code
        float32 position = lerp(0.0f, 100.0f, 0.5f);   // Result: 50.0f
        float32 alpha = lerp(startAlpha, endAlpha, time);
      \endcode
      \par Output
        Returns value interpolated between start and end
      \par Purpose
        Smooth transitions and animations between values
    *************************************************************************/

    // Floating point overloads use std::lerp (C++20)
    inline constexpr float32 lerp(float32 start, float32 end, float32 t) noexcept
    {
        return std::lerp(start, end, t);
    }

    inline constexpr float64 lerp(float64 start, float64 end, float64 t) noexcept
    {
        return std::lerp(start, end, t);
    }

    // Generic fallback for vector like types (e.g., GGMath::Vector2)
    template<typename T>
    inline constexpr T lerp(const T& start, const T& end, float32 t) noexcept
    {
        return start + (end - start) * t;
    }

    /*************************************************************************
      \brief
        Smooth step interpolation with ease-in and ease-out
      \param[in] t
        Input parameter [0, 1]
      \return
        Smoothed value [0, 1]
      \par Usage
      \code
        float32 smooth = smoothStep(0.5f);      // Result: 0.5f (but smoother curve)
        float32 eased = smoothStep(time);       // Smooth animation curve
      \endcode
      \par Output
        Returns smoothed interpolation value
      \par Purpose
        Creates smooth animation curves with gradual acceleration/deceleration
    *************************************************************************/
    inline constexpr float32 smoothStep(float32 t) noexcept
    {
        t = clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    /*************************************************************************
      \brief
        Checks if two floating point values are approximately equal
      \param[in] a
        First value
      \param[in] b
        Second value
      \param[in] epsilon
        Tolerance for comparison (default: EPSILON)
      \return
        True if values are approximately equal
      \par Usage
      \code
        bool equal = approximately(3.14159f, PI);
        bool close = approximately(userInput, targetValue, 0.1f);
      \endcode
      \par Output
        Returns true if difference is within epsilon tolerance
      \par Purpose
        Safe floating point comparison avoiding precision errors
    *************************************************************************/
    inline bool approximately(float32 a, float32 b, float32 epsilon = GGMath::FLOAT_EPSILON) noexcept
    {
        return std::abs(a - b) < epsilon;
    }

    /*************************************************************************
      \brief
        Returns the sign of a value
      \param[in] value
        Input value
      \return
        1.0f if positive, -1.0f if negative, 0.0f if zero
      \par Usage
      \code
        float32 direction = sign(velocity.x);   // Get movement direction
        float32 multiplier = sign(input);       // Convert input to direction
      \endcode
      \par Output
        Returns -1, 0, or 1 indicating the sign
      \par Purpose
        Extract direction or sign information from values
    *************************************************************************/
    inline constexpr float32 sign(float32 value) noexcept
    {
        return (value > 0.0f) ? 1.0f : (value < 0.0f ? -1.0f : 0.0f);
    }
}