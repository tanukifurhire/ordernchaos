/*************************************************************************
  \file       GGPickingFramebuffer.cpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       7 November 2025
  \version    1.0
  \brief
    Implementation of GGPickingFramebuffer for GGEngine.
  \details
    Handles creation, binding, clearing, resizing, and pixel reading of an
    offscreen framebuffer used for color-based picking of entities.
*************************************************************************/
// GGPickingFramebuffer.cpp
#include "GGPickingFramebuffer.hpp"
#include <iostream>
#include <vector>


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
bool GGPickingFramebuffer::Init(unsigned int width, unsigned int height)
{
    Cleanup();

    m_width = width;
    m_height = height;

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    // Color attachment: RGB8 (we encode IDs into RGB)
    glGenTextures(1, &m_pickingTexture);
    glBindTexture(GL_TEXTURE_2D, m_pickingTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, m_width, m_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_pickingTexture, 0);

    // Depth renderbuffer (optional but recommended)
    glGenRenderbuffers(1, &m_depthTexture);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthTexture);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthTexture);

    GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "[GGPickingFramebuffer] Framebuffer not complete!" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

/*************************************************************************
  \brief Bind framebuffer for rendering.
  \details
    Sets the framebuffer as the current render target and adjusts the
    viewport to match its dimensions.
*************************************************************************/
void GGPickingFramebuffer::EnableWriting()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
}

/*************************************************************************
  \brief Unbind framebuffer.
  \details
    Restores the default framebuffer. Caller should reset the viewport
    to the window dimensions if necessary.
*************************************************************************/
void GGPickingFramebuffer::DisableWriting()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
   
}

/*************************************************************************
  \brief Clear the picking framebuffer.
  \details
    Clears the color attachment to white (reserved as background/invalid)
    and clears the depth buffer.
*************************************************************************/
void GGPickingFramebuffer::Clear()
{
   
    // Clear to white (reserved as background/invalid)
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   
}

/*************************************************************************
  \brief Resize the picking framebuffer.
  \param width New width in pixels.
  \param height New height in pixels.
  \details
    Deletes existing textures and framebuffer objects, then reinitializes
    them with the new dimensions.
*************************************************************************/
void GGPickingFramebuffer::Resize(unsigned int width, unsigned int height)
{
    if (width == m_width && height == m_height) return;

    m_width = width;
    m_height = height;

    // Recreate textures / RBO
    if (m_pickingTexture) {
        glDeleteTextures(1, &m_pickingTexture);
        m_pickingTexture = 0;
    }
    if (m_depthTexture) {
        glDeleteRenderbuffers(1, &m_depthTexture);
        m_depthTexture = 0;
    }
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }

    Init(m_width, m_height);
}

/*************************************************************************
  \brief Cleanup GPU resources.
  \details
    Deletes the framebuffer, color texture, and depth/stencil renderbuffer.
    Resets width and height to zero.
*************************************************************************/
void GGPickingFramebuffer::Cleanup()
{
    if (m_pickingTexture) { glDeleteTextures(1, &m_pickingTexture); m_pickingTexture = 0; }
    if (m_depthTexture) { glDeleteRenderbuffers(1, &m_depthTexture); m_depthTexture = 0; }
    if (m_fbo) { glDeleteFramebuffers(1, &m_fbo); m_fbo = 0; }
    m_width = m_height = 0;
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
GGPickingFramebuffer::PickingResult GGPickingFramebuffer::ReadPixel(unsigned int x, unsigned int y)
{
    PickingResult result;
    if (m_pickingTexture == 0) { 
        std::cout << "ERROR: Picking texture is 0!" << std::endl;
        return result; 
    }


    // Clamp query coords
    if (x >= m_width || y >= m_height) {
        std::cout << "ERROR: Coordinates out of bounds: (" << x << "," << y << ") vs buffer size (" << m_width << "," << m_height << ")" << std::endl;
        return result;
    }
    // Read from FBO
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);

    
    std::vector<unsigned char> pixel(3);
    glReadPixels(static_cast<GLint>(x), static_cast<GLint>(y), 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel.data());
    /*std::cout << "Read pixel at (" << x << "," << y << "): RGB("
        << (int)pixel[0] << "," << (int)pixel[1] << "," << (int)pixel[2] << ")" << std::endl;*/


    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    uint8_t r = pixel[0], g = pixel[1], b = pixel[2];

    // White (255,255,255) reserved for background (invalid)
    if (r == 255 && g == 255 && b == 255) {
        result.EntityID = 0;
        return result;
    }

    uint32_t id = static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16);
    result.EntityID = id;
    //std::cout << "Decoded EntityID: " << id << std::endl;
    return result;
}