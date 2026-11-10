/*************************************************************************
  \file       Constants.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       6 September 2025
  \version    1.0
  \brief
    Mathematical constants for 2D game development
  \dependencies
    <numbers>, Types.hpp
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <numbers>
#include "Types.hpp"

namespace GGMath
{
    // STL mathematical constants
    constexpr float32 PI = std::numbers::pi_v<float32>;           // Pi (3.14159...)
    constexpr float32 EULERS_NUMBER = std::numbers::e_v<float32>; // Euler's number (2.71828...)

    // Circle rotations in radians
    constexpr float32 FULL_CIRCLE = 2.0f * PI;                   // 360 degrees
    constexpr float32 HALF_CIRCLE = PI;                          // 180 degrees
    constexpr float32 QUARTER_CIRCLE = PI * 0.5f;                // 90 degrees

    // Angle conversions
    constexpr float32 DEGREES_TO_RADIANS = PI / 180.0f;
    constexpr float32 RADIANS_TO_DEGREES = 180.0f / PI;

    // Float comparison tolerance
    constexpr float32 FLOAT_EPSILON = 1e-6f;
}