/*************************************************************************
  \file       GGColor2DIndexing.hpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       28 October 2025
  \brief
    Utility for encoding and decoding 2D entity IDs as RGB colors
    for use in color-based framebuffer picking.
*************************************************************************/
#pragma once
#include <glm/glm.hpp>
#include <cstdint>

class Color2DIndexing {
public:
    // Convert object ID to RGB color for 2D sprite rendering
    static glm::vec4 IDToColor(uint32_t id) {
        if (id == 0) return glm::vec4(1.0f); // White (background / invalid)

        uint8_t r = (id & 0xFF);
        uint8_t g = (id >> 8) & 0xFF;
        uint8_t b = (id >> 16) & 0xFF;

        // Normalize to [0, 1]
        return glm::vec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
    }

    // Extract object ID from RGB pixel data
    static uint32_t ColorToID(uint8_t r, uint8_t g, uint8_t b) {
        // White background = no object
        if (r == 255 && g == 255 && b == 255)
            return INVALID_ID;

        return (static_cast<uint32_t>(r))
            | (static_cast<uint32_t>(g) << 8)
            | (static_cast<uint32_t>(b) << 16);
    }

    // Reserved color IDs
    static constexpr uint32_t BACKGROUND_ID = 0x000000;  // Black (none)
    static constexpr uint32_t INVALID_ID = 0xFFFFFF;  // White (background)
};