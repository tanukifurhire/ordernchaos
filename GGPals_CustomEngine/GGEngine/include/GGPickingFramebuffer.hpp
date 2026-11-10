#pragma once
/*************************************************************************
  \file       GGPickingFramebuffer.hpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       7 November 2025
  \version    1.0
  \brief
    2D framebuffer object picking system for GGEngine.
  \details
    Provides an efficient 2D object picking mechanism by encoding ECS Entity IDs
    into a color buffer (RGB8). Integrates with GGEngineRender and Render2D
    systems. Supports dynamic resizing, clearing, and reading pixels for
    selection logic.
*************************************************************************/
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdint>
#include <glm/glm.hpp>



class GGPickingFramebuffer {
private:
    GLuint m_fbo = 0;
    GLuint m_pickingTexture = 0;    // GL_RGB8 sufficient for 2D color indexing
    GLuint m_depthTexture = 0;      // Optional depth for layering
    unsigned int m_width = 0, m_height = 0;

public:
    unsigned int getWidth() {
        return m_width;
    }
    unsigned int getHeight() {
        return m_height;
    }

    /*************************************************************************
      \brief Initialize the picking framebuffer.
      \param width Width of the framebuffer in pixels.
      \param height Height of the framebuffer in pixels.
      \return True if framebuffer is successfully created and complete, false otherwise.
      \details
        Creates a framebuffer object, RGB8 color texture, and optional
        depth/stencil renderbuffer. Sets up draw buffers and checks framebuffer
        completeness.
    *************************************************************************/
    bool Init(unsigned int width, unsigned int height);

    /*************************************************************************
      \brief Bind framebuffer for rendering.
      \details
        Sets the framebuffer as the current render target and adjusts the
        viewport to match its dimensions.
    *************************************************************************/
    void EnableWriting();       // Bind framebuffer for rendering

    /*************************************************************************
      \brief Unbind framebuffer.
      \details
        Restores the default framebuffer. Caller should reset the viewport
        to the window dimensions if necessary.
    *************************************************************************/
    void DisableWriting();      // Restore default framebuffer

    /*************************************************************************
      \brief Clear the picking framebuffer.
      \details
        Clears the color attachment to white (reserved as background/invalid)
        and clears the depth buffer.
    *************************************************************************/
    void Clear();              // Clear picking texture to background color

    /*************************************************************************
      \brief Resize the picking framebuffer.
      \param width New width in pixels.
      \param height New height in pixels.
      \details
        Deletes existing textures and framebuffer objects, then reinitializes
        them with the new dimensions.
    *************************************************************************/
    void Resize(unsigned int width, unsigned int height);

    /*************************************************************************
      \brief Cleanup GPU resources.
      \details
        Deletes the framebuffer, color texture, and depth/stencil renderbuffer.
        Resets width and height to zero.
    *************************************************************************/
    void Cleanup();            // Destructor cleanup

    struct PickingResult {
        uint32_t EntityID = 0;      // ECS Entity ID from existing system
        uint32_t LayerID = 0;       // Rendering layer (integrates with render2d)
        glm::vec2 WorldPosition{ 0.0f }; // World position of picked point

        bool IsValid() const { return EntityID != 0 && EntityID != 0xFFFFFF; }
    };
    GLuint GetTexture() {
        return m_pickingTexture;
    }

    /*************************************************************************
      \brief Read a pixel from the picking framebuffer.
      \param x X coordinate of the pixel.
      \param y Y coordinate of the pixel.
      \return PickingResult containing the decoded EntityID.
      \details
        Reads the RGB color at the specified pixel and decodes it into an
        ECS Entity ID. White (255,255,255) is reserved as an invalid background.
    *************************************************************************/
    PickingResult ReadPixel(unsigned int x, unsigned int y);

    // Integration with existing GGEngineRender constants
    static constexpr float PICKING_SCALE = 64.f;;


};