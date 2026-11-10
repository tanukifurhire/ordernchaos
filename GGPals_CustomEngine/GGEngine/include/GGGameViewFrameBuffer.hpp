/*************************************************************************
  \file       GameViewFrameBuffer.hpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email:  a.patchamuthu@digipen.edu
  \date       7 November 2025
  \version    1.0
  \brief
	Framebuffer class interface for game view rendering in GGEngine.
  \details
	Declares an offscreen framebuffer used to render the game scene
	into an ImGui viewport. Provides initialization, resize handling,
	cleanup, and texture access for editor rendering.
*************************************************************************/
//GGameViewFrameBuffer
//this buffer is for importing view into imgii ciew or editor in game engine

#pragma once
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cstdint>

/*************************************************************************
  \class      GameViewFrameBuffer
  \brief      Offscreen framebuffer used for rendering the game view.
  \details
	This framebuffer provides a color texture and a depth-stencil buffer
	so the game scene can be rendered into an ImGui viewport inside the
	engine editor. The resulting color texture is displayed using ImGui.
*************************************************************************/
class GameViewFrameBuffer {

public:
	GameViewFrameBuffer() = default;
	~GameViewFrameBuffer() { Cleanup(); }

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
	bool Init(unsigned int width, unsigned int height);

	/*************************************************************************
	  \brief
		Binds this framebuffer as the current render target.
	  \details
		Call before rendering the scene intended for the editor game view.
	 *************************************************************************/
	void BindBuffer();

	/*************************************************************************
	  \brief
		Unbinds the framebuffer, restoring default render target.
	  \details
		Typically called after scene rendering is complete.
	*************************************************************************/
	void UnbindBuffer();

	/*************************************************************************
	  \brief
		Resizes framebuffer attachments to match new dimensions.
	  \details
		Reallocates texture+RBO storage. Safe to call when viewport changes.
	*************************************************************************/
	void Resize(unsigned int width, unsigned int height);

	/*************************************************************************
	  \brief
		Releases all GL resources owned by this framebuffer.
	*************************************************************************/
	void Cleanup();

	/*************************************************************************
	  \brief Clears color, depth, and stencil buffers.
	  \details
		Must be called after BindBuffer() to affect this framebuffer.
	*************************************************************************/
	void Clear();

	unsigned int  getWidth() {
		return viewWidth;
	}
	unsigned int getHeight() {
		return viewHeight;
	}

	/*************************************************************************
	  \brief
		Returns the color texture used by this framebuffer.
	  \return
		GL texture ID that can be passed to ImGui::Image().
	*************************************************************************/
	unsigned int GetFrameTexture() const;




private:
	unsigned int fbo = 0;
	unsigned int texture = 0;
	unsigned int rbo = 0;//rbo is depth and stencil buffer

	unsigned int viewWidth = 0;
	unsigned int viewHeight = 0;
};