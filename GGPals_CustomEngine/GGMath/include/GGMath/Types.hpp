/*************************************************************************
  \file       Types.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       6 September 2025
  \version    1.0
  \brief
    Defines primitive type aliases in the GGMath namespace.
  \details
    This file provides type aliases for fixed-width integer and floating
    point types to ensure consistent data sizes across platforms. These
    types are essential for networking, serialisation, and cross-platform
    compatibility in game development.
  \dependencies
    <cstdint>
  \see        
    Vector2, Matrix3x3
  \history
    - 1.0 (6 September 2025): Initial version.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <cstdint>

namespace GGMath
{
    // Signed integer types
    using int8 = std::int8_t;       // 8-bit signed integer (-128 to 127)
    using int16 = std::int16_t;     // 16-bit signed integer (-32,768 to 32,767)
    using int32 = std::int32_t;     // 32-bit signed integer (-2,147,483,648 to 2,147,483,647)
    using int64 = std::int64_t;     // 64-bit signed integer

    // Unsigned integer types
    using uint8 = std::uint8_t;     // 8-bit unsigned integer (0 to 255)
    using uint16 = std::uint16_t;   // 16-bit unsigned integer (0 to 65,535)
    using uint32 = std::uint32_t;   // 32-bit unsigned integer (0 to 4,294,967,295)
    using uint64 = std::uint64_t;   // 64-bit unsigned integer

    // Floating point types
    using float32 = float;          // 32-bit floating point
    using float64 = double;         // 64-bit floating point
}