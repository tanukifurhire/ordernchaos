/*************************************************************************
  \file       GGGameViewFrameBuffer.cpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email:  a.patchamuthu@digipen.edu
  \date       7 november 2025
  \version    1.0
  \brief
    Framebuffer implementation for game view rendering in GGEngine.
  \details
    Provides offscreen rendering target for ImGui viewport integration
    with automatic resource management and manual init/cleanup support.
*************************************************************************/
#include <GGGameViewFrameBuffer.hpp>
#include <iostream>
#include <glm/glm.hpp>
#include <GL/glew.h>

/*************************************************************************
  \brief 
    Initializes framebuffer and its attachments.
  \param[in] 
    width  Desired framebuffer width in pixels.
  \param[in] 
    height Desired framebuffer height in pixels.
  \return 
    True if initialization succeeds, false on failure.
  \details
    Allocates color texture + depth/stencil renderbuffer, attaches both,
    and verifies framebuffer completeness.
 *************************************************************************/
bool GameViewFrameBuffer::Init(unsigned int width, unsigned int height) {

    Cleanup();

    if (width == 0 || height == 0) {
        std::cerr << " invalid dimensions " << width << "x" << height << std::endl;
        return false;
    }

    viewWidth = width;
    viewHeight = height;

    // Create framebuffer object
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create color texture attachment
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, viewWidth, viewHeight, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, texture, 0);

    // Create renderbuffer for depth and stencil
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, viewWidth, viewHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER, rbo);

    // Check framebuffer completeness
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "GameViewFrameBuffer::Init - Framebuffer incomplete! Status: 0x"
            << std::hex << status << std::dec << std::endl;
        Cleanup();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
}


/*************************************************************************
  \brief 
    Binds this framebuffer as the current render target.
  \details
    Call before rendering the scene intended for the editor game view.
 *************************************************************************/
void GameViewFrameBuffer::BindBuffer()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, viewWidth, viewHeight);
}

/*************************************************************************
  \brief 
    Unbinds the framebuffer, restoring default render target.
  \details
    Typically called after scene rendering is complete.
*************************************************************************/
void GameViewFrameBuffer::UnbindBuffer()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


/*************************************************************************
  \brief Clears color, depth, and stencil buffers.
  \details
    Must be called after BindBuffer() to affect this framebuffer.
*************************************************************************/
void GameViewFrameBuffer::Clear() {
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}



/*************************************************************************
  \brief 
    Resizes framebuffer attachments to match new dimensions.
  \details
    Reallocates texture+RBO storage. Safe to call when viewport changes.
*************************************************************************/
void GameViewFrameBuffer::Resize(unsigned int width, unsigned int height) {
    // Avoid unnecessary resizes
    if (width == viewWidth && height == viewHeight)
        return;

    if (width == 0 || height == 0)
    {
        std::cerr << "[Framebuffer::Resize] Invalid dimensions: "
            << width << "x" << height << std::endl;
        return;
    }

    // Update stored size
    viewWidth = width;
    viewHeight = height;

   
    if (texture) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
    if (rbo) {
        glDeleteRenderbuffers(1, &rbo);
        rbo = 0;
    }
    if (fbo) {
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }

   
    Init(viewWidth, viewHeight);
    glViewport(0, 0, viewWidth, viewHeight);
    std::cout << "[Framebuffer::Resize] Recreated framebuffer at "
        << width << "x" << height << std::endl;
}
/*************************************************************************
  \brief 
    Releases all GL resources owned by this framebuffer.
*************************************************************************/
void GameViewFrameBuffer::Cleanup() {
    if (fbo) {
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }
    if (texture) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
    if (rbo) {
        glDeleteRenderbuffers(1, &rbo);
        rbo = 0;
    }
    viewWidth = 0;
    viewHeight = 0;
}

/*************************************************************************
  \brief 
    Returns the color texture used by this framebuffer.
  \return 
    GL texture ID that can be passed to ImGui::Image().
*************************************************************************/
unsigned int GameViewFrameBuffer::GetFrameTexture() const
{
    return texture;
}