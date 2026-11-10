/*************************************************************************
  \file       GGEngineRender.hpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       3/10/2025
  \version    1.0
  \brief
	Defines the GGEngineRender class, the core rendering subsystem of the
	GG Engine.
  \details
	GGEngineRender manages the engine's OpenGL rendering pipeline, including
	window management, shader compilation, texture/font atlas systems, and
	default rendering resources. It provides a unified interface for 2D
	rendering operations, text rendering via FreeType and font atlases, and
	serves as the primary graphics abstraction layer for the engine.

	Key responsibilities:
	- GLFW window lifecycle and viewport management
	- Default shader/mesh/texture resource initialization
	- Font atlas generation and text rendering via FreeType
	- Orthographic projection setup and coordinate space transformations
	- Integration with AssetManager for resource loading with fallback paths

	The class implements the SystemManagerInterface, allowing it to be
	managed by the engine's system registry. It provides world-to-screen
	coordinate transformations via WORLD_TO_SCREEN_SCALE constant.

  \dependencies
	- FreeType 2 (FT_FREETYPE_H)
	- OpenGL/GLFW (via GGWindow)
	- GLM (matrix transformations)
	- GGMesh, GGShader, GGFontAtlas
	- GGSystemInterface (for system lifecycle)
	- SOIL2 (image loading fallback)

  \see
	GGWindow, GGShader, GGMesh, GGFontAtlas, SystemManagerInterface

  \history
	- 1.0: Initial implementation with AssetManager integration

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H

#include <GGWindow.hpp>
#include <GGMesh.hpp>
#include <GGSystemInterface.hpp>
#include <GGFontAtlas.hpp>


#include <map>
#include <GGPickingFramebuffer.hpp>
#include <GGGameViewFrameBuffer.hpp>
#include <GGEditorCamera2D.hpp>
#include <GGGameCamera2D.hpp>
#include <GGCamera2D.hpp>
#include <GGLogger.hpp>

/*************************************************************************
  \class GGEngineRender
  \brief
	Core rendering system of GGEngine.
  \details
	Handles 2D rendering, text rendering via FontAtlas, orthographic projection,
	ECS integration, shader and texture management, framebuffer handling, and
	game/editor view rendering.
*************************************************************************/
class GGEngineRender : public SystemManagerInterface {
public:
	static constexpr float WORLD_TO_SCREEN_SCALE = 1.f;
	
	/*************************************************************************
	  \brief
		Constructs the GGEngineRender object.
	  \details
		Initializes member variables and prepares default resources for rendering.
	*************************************************************************/
	GGEngineRender();

	/*************************************************************************
	  \brief
		Destructs the GGEngineRender object.
	  \details
		Cleans up GLFW window, OpenGL resources, and font atlas.
	*************************************************************************/
	~GGEngineRender();

	/*************************************************************************
	  \brief
		Loads default assets and initializes FreeType.
	  \return
		True if loading succeeds, false otherwise.
	  \details
		Sets up the FreeType library and loads the default font from the
		AssetManager or fallback paths.
	*************************************************************************/
	bool Load();

	/*************************************************************************
	  \brief
		Initializes the rendering system.
	  \return
		True if initialization succeeds, false otherwise.
	  \details
		Sets up OpenGL context, default shaders, framebuffers, projection, and
		integrates with ECS systems.
	*************************************************************************/
	bool Init() override;

	/*************************************************************************
	  \brief
		Updates the rendering system per frame.
	  \details
		Handles window updates, camera updates, overlay updates, and draws
		2D objects to the screen.
	*************************************************************************/
	void Update() override;

	
	GGWindow const& GetMainWindow() const {
		return mainWindow;
	}

	/*************************************************************************
	  \brief
		Sets the default orthographic projection.
	  \param width 
	    Width of the viewport.
	  \param height 
		Height of the viewport.
	  \details
			Called to maintain correct scaling and aspect ratio when the window
			size changes.
	*************************************************************************/
	void SetDefaultProjection(int width, int height);

	struct Character {
		GLuint			texid;
		glm::ivec2		size;
		glm::ivec2		bearing;
		FT_Pos			advance;
	};

	struct Font {
		GLuint			vaoid{};
		GLuint			vboid{};
		FT_Face			face{};
	};

	struct ImageData {
		GLuint			texid{};
		GLubyte*		imgData = nullptr;
		int				imgWidth{};
		int				imgHeight{};
		int				nrChannels{};
	};

	/*************************************************************************
	  \brief
		Calculates the size of a text string when rendered.
	  \param text Text to measure.
	  \param scale Scale factor for the font.
	  \return
		2D vector containing the width and height of the rendered text.
	  \details
		Uses the FontAtlas to compute text dimensions.
	*************************************************************************/
	glm::vec2 GetPrintSize(std::string const& text, float scale);

	/*************************************************************************
	  \brief
		Renders a string of text on the screen.
	  \param s Shader to use for rendering.
	  \param text Text to render.
	  \param x X position in screen coordinates.
	  \param y Y position in screen coordinates.
	  \param scale Font scale factor.
	  \param color RGB color of the text.
	  \details
		Uses the FontAtlas and a shader to draw text with the specified
		parameters.
	*************************************************************************/
	void RenderText(GGShader& s, std::string text, float x, float y, float scale, glm::vec3 color);

	GGMesh const& GetDefaultMesh() { return defaultMesh; }

	uint32_t const GetDefaultShaderAssetID() const { return defaultObjShader; }
	
	uint32_t const GetDefaultTextureAssetID() const { return defaultObjTexture; }
	uint32_t const GetDefaultShaderTextureAssetID() const { return defaultShaderTexture; }

	GGShader const& GetDefaultShader() { return defaultShader; }

	GGShader& GetDefaultFontShader() { return defaultFontShader; }
	GGShader const& GetDefaultPickingShader() { return defaultPickingShader; }

	bool GetFontAtlasValid() const
	{
		return fontAtlas != nullptr && fontAtlas->IsValid();
	}

	glm::ivec2 GetFontAtlasDimensions() const
	{
		if (fontAtlas && fontAtlas->IsValid())
			return fontAtlas->GetAtlasDimensions();
		return glm::ivec2(0, 0);
	}

	glm::mat4 const& GetDefaultProjection()  const{ return defaultProjection; }

	glm::mat4 const& GetDefaultWorldScale() { return defaultWorldScale; }
	glm::mat4 const& GetDefaultModelMatrix() { return  defaultModelMatrix; }

	glm::vec4 const& GetDefaultUVBounds() const { return defaultUVBounds; }
	glm::mat4 const& GetDefaultUVTransform() const { return defaultUVTransform; }
	glm::mat4 GetDefaultCamerView() { return defaultCameraView; }

	/*************************************************************************
	  \brief
		Creates the framebuffer for the game view.
	  \param width Width of the framebuffer.
	  \param height Height of the framebuffer.
	  \details
		Used for offscreen rendering of the game view.
	*************************************************************************/
	void CreateGameViewFramebuffer(int width, int height);

	/*************************************************************************
	  \brief
		Resizes the game view framebuffer.
	  \param width New width of the framebuffer.
	  \param height New height of the framebuffer.
	  \details
		Updates textures and renderbuffers to match the new dimensions.
	*************************************************************************/
	void ResizeGameViewFramebuffer(int width, int height);
	
	/*************************************************************************
	  \brief
		Binds the game view framebuffer for rendering.
	  \details
		Sets the viewport to match the framebuffer size.
	*************************************************************************/
	void BindGameViewFramebuffer();

	/*************************************************************************
	  \brief
		Unbinds the game view framebuffer.
	  \details
		Returns rendering to the default window framebuffer.
	*************************************************************************/
	void UnbindGameViewFramebuffer();

	/*************************************************************************
	  \brief
		Renders overlay text and FPS counter.
	  \details
		Uses the FontAtlas and shader to display debug or UI overlays on the
		screen.
	*************************************************************************/
	void RenderFontOverlays();

	/*************************************************************************
	  \brief
		Prints a 4x4 matrix to the console.
	  \param mat Matrix to print.
	  \param name Optional name label for the matrix.
	  \details
		Useful for debugging projection, view, and model matrices.
	*************************************************************************/
	void PrintMat4(const glm::mat4& mat, const std::string& name);

	
	GGPickingFramebuffer pickingFramebuffer;

	GameViewFrameBuffer viewBuffer;

	int gameViewWidth = 0;
	int gameViewHeight = 0;

	/*************************************************************************
	  \brief
		Initializes the game camera.
	  \param width Initial viewport width.
	  \param height Initial viewport height.
	  \details
		Sets up the game camera for rendering and world-space transformations.
	*************************************************************************/
	void InitializeGameCamera(float width, float height);

	/*************************************************************************
	  \brief
		Updates the game camera each frame.
	  \param deltaTime Time elapsed since last frame.
	  \details
		Updates internal camera state including shake, smoothing, and follow
		logic. Script control remains external.
	*************************************************************************/
	void UpdateGameCamera(float deltaTime);

	/*************************************************************************
	  \brief
		Returns the game camera instance for scripts or systems.
	  \return
		Pointer to the GGGameCamera2D owned by the render manager.
	*************************************************************************/
	GGGameCamera2D* GetGameCamera() { return gameCamera.get(); }

	/**
	 * \brief Get the game camera instance (const)
	 * \return Const pointer to the game camera
	 */
	const GGGameCamera2D* GetGameCamera() const { return gameCamera.get(); }

	/*************************************************************************
	  \brief
		Returns a const pointer to the game camera instance.
	  \return
		Const pointer to the GGGameCamera2D.
	*************************************************************************/
	void ResetGameCamera();

	// Set weak reference to editor camera
	/*************************************************************************
	  \brief
		Sets a weak reference to the editor camera.
	  \param camera Pointer to the editor camera instance.
	  \details
		Used to allow the render manager to access the editor camera without
		taking ownership.
	*************************************************************************/
	void SetEditorCamera(GGEditorCamera2D* camera) {
		editorCamera = camera;
	}

	/*************************************************************************
	  \brief
		Retrieves the projection matrix from the active editor camera.
	  \return
		4x4 projection matrix. Returns default projection if editor camera is null.
	*************************************************************************/
	glm::mat4 GetCameraProjection() const {
		if ( editorCamera)
			return editorCamera->GetProjectionMatrix();
		return GetDefaultProjection();
	}

	/*************************************************************************
	  \brief
		Retrieves the view matrix from the active editor camera.
	  \return
		4x4 view matrix. Returns identity matrix if editor camera is null.
	*************************************************************************/
	glm::mat4 GetCameraView() const {
		if (editorCamera)
			return editorCamera->GetViewMatrix4D();
		return glm::mat4(1.0f);
	}

	/*************************************************************************
	  \brief
		Retrieves the combined view matrix for the editor camera.
	  \return
		4x4 view matrix. Returns identity if editor camera is null.
	  \details
		This function currently returns only the view matrix, not combined with projection.
	*************************************************************************/
	glm::mat4 GetCameraViewProjection() const {
		return GetCameraView();
	}

	/*************************************************************************
	  \brief
		Returns the currently active camera based on editor mode.
	  \return
		Pointer to the active GGCamera2D instance (editor or game camera).
	*************************************************************************/
	GGCamera2D* GetActiveCamera()
	{
		switch (currentMode) {
		case EditorMode::gameView:
			return gameCamera.get(); // store copy
		case EditorMode::editorView:
			return editorCamera; // store copy
		default:
			return editorCamera;
		}
	}

	/*************************************************************************
	  \brief
		Retrieves the projection matrix of the game camera.
	  \return
		4x4 projection matrix. Returns default projection if game camera is null.
	*************************************************************************/
	glm::mat4 GetGameCameraViewProjection()const {
		if ( gameCamera)
			return gameCamera->GetProjectionMatrix();
		return GetDefaultProjection();
	}

	/*************************************************************************
	  \brief
		Retrieves the view matrix of the game camera.
	  \return
		4x4 view matrix. Returns identity matrix if game camera is null.
	*************************************************************************/
	glm::mat4 GetGameCameraView() const {
		if (gameCamera)
			return gameCamera->GetViewMatrix4D();
		return glm::mat4(1.0f);
	}

	// --- Setters ---
	/*************************************************************************
	  \brief
		Enables or disables the overlay text display.
	  \param enabled True to enable overlay text, false to disable.
	*************************************************************************/
	void SetOverlayTextEnabled(bool enabled) { overlayTextEnabled = enabled; }
	/*************************************************************************
	  \brief
		Sets the text content for overlay display.
	  \param text The string to display on the overlay.
	*************************************************************************/
	void SetOverlayText(const std::string& text) { overlayText = text; }

	/*************************************************************************
	  \brief
		Sets the screen position of the overlay text.
	  \param x X coordinate in screen space.
	  \param y Y coordinate in screen space.
	*************************************************************************/
	void SetOverlayTextPosition(float x, float y) { overlayTextX = x; overlayTextY = y; }

	/*************************************************************************
	  \brief
		Sets the scale of the overlay text.
	  \param scale Font scale factor.
	*************************************************************************/
	void SetOverlayTextScale(float scale) { overlayTextScale = scale; }

	/*************************************************************************
	  \brief
		Sets the color of the overlay text.
	  \param color RGB color vector.
	*************************************************************************/
	void SetOverlayTextColor(const glm::vec3& color) { overlayTextColor = color; }

	/*************************************************************************
	  \brief
		Enables or disables the FPS counter overlay.
	  \param show True to display FPS, false to hide.
	*************************************************************************/
	void SetOverlayShowFPS(bool show) { overlayShowFPS = show; }

	/*************************************************************************
	  \brief
		Checks if overlay text is enabled.
	  \return True if overlay text is enabled.
	*************************************************************************/
	bool GetOverlayShowFPS() const { return overlayShowFPS; }

	// --- Getters ---
	/*************************************************************************
	  \brief
		Checks if overlay text is enabled.
	  \return True if overlay text is enabled.
	*************************************************************************/
	bool IsOverlayTextEnabled() const { return overlayTextEnabled; }

	/*************************************************************************
	  \brief
		Gets the current overlay text content.
	  \return The string displayed as overlay text.
	*************************************************************************/
	const std::string& GetOverlayText() const { return overlayText; }

	/*************************************************************************
	  \brief
		Gets the X position of the overlay text.
	  \return X coordinate in screen space.
	*************************************************************************/
	float GetOverlayTextX() const { return overlayTextX; }

	/*************************************************************************
	  \brief
		Gets the Y position of the overlay text.
	  \return Y coordinate in screen space.
	*************************************************************************/
	float GetOverlayTextY() const { return overlayTextY; }

	/*************************************************************************
	  \brief
		Gets the scale of the overlay text.
	  \return Font scale factor.
	*************************************************************************/
	float GetOverlayTextScale() const { return overlayTextScale; }

	/*************************************************************************
	  \brief
		Gets the color of the overlay text.
	  \return RGB color vector of the overlay text.
	*************************************************************************/
	const glm::vec3& GetOverlayTextColor() const { return overlayTextColor; }

	/*************************************************************************
	  \brief
		Checks if the FPS overlay is currently shown.
	  \return True if FPS display is enabled.
	*************************************************************************/
	bool IsOverlayShowFPS() const { return overlayShowFPS; };


	enum class EditorMode {
		editorView,
		gameView,
		invalidmode
	};
	EditorMode currentMode = EditorMode::invalidmode;

	/*************************************************************************
	  \brief
		Returns the active projection matrix based on current editor mode.
	  \return Projection matrix of the active camera or default.
	*************************************************************************/
	glm::mat4 const& GetActiveProjection() const {
		switch (currentMode) {
		case EditorMode::gameView:
			activeProjection = GetGameCameraViewProjection(); // store copy
			break;
		case EditorMode::editorView:
			activeProjection = GetCameraProjection(); // store copy
			break;
		default:
			activeProjection = GetDefaultProjection();
			break;
		}
		return activeProjection;
	}

	/*************************************************************************
	  \brief
		Returns the active view matrix based on current editor mode.
	  \return View matrix of the active camera or default.
	*************************************************************************/
	glm::mat4 const& GetActiveView() const {
		switch (currentMode) {
		case EditorMode::gameView:
			activeView = GetGameCameraView();
			break;
		case EditorMode::editorView:
			activeView = GetCameraView();
			break;
		default:
			activeView = defaultCameraView;
			break;
		}
		return activeView;
	}

protected:
	
	GGEditorCamera2D* editorCamera = nullptr;
	
	//GGGameCamera2D* gameCamera= nullptr;

	// Game camera (OWNED by RenderManager)
	std::unique_ptr<GGGameCamera2D> gameCamera;
	GGCamera2D basicCamera;
	// Window object wrapping GLFWwindow
	GGWindow mainWindow;

	// Default Shaders (Will be set in config.json)
	// Essentially this should be the fallback if let's say
	// our other shaders don't work...
	GGShader defaultShader;
	GGShader defaultFontShader;
	GGShader defaultPickingShader;
	
	uint32_t defaultObjShader{};
	uint32_t defaultObjTexture{};
	uint32_t defaultShaderTexture{};

	glm::mat4 defaultProjection = glm::mat4(1.f);
	glm::mat4 defaultModelMatrix = glm::mat4(1.f);
	glm::mat4 defaultWorldScale = glm::mat4(1.f);

	// Target Aspect Ratio (Will be set in config.json)
	GLfloat targetAspectRatio{1.f};

	// Default Quad (can be used for almost anything really)
	GGMesh defaultMesh;

	// Default Texture
	ImageData defaultTex;

	//
	ImageData spriteTex;
	
	// FreeType library stuff
	FT_Library ft{};
	Font defaultFont{};

	GLuint brickTexture;
	// This will create the default fonts too ig
	void CreateDefaults();

	static void FramebufferResizeCallback(GLFWwindow* window, int width, int height);

	// Default UV bounds for the default mesh (x_min, y_min, x_max, y_max)
	glm::vec4 defaultUVBounds = glm::vec4(0.f, 0.f, 1.f, 1.f);

	// Default UV transformation (identity)
	glm::mat4 defaultUVTransform = glm::mat4(1.f);


	GGFontAtlas* fontAtlas = nullptr;
	
	//GLuint gameViewFBO = 0;
	//GLuint gameViewTexture = 0;
	//GLuint gameViewRBO = 0;  // For depth/stencil
	bool overlayTextEnabled = false;
	std::string overlayText = "";
	float overlayTextX = 0;
	float overlayTextY = 0;
	float overlayTextScale = 1.0f;
	glm::vec3 overlayTextColor = { 1.0f, 1.0f, 1.0f };

	bool overlayShowFPS = false;
	//bool m_render2dInitialized = false;
	glm::mat4 defaultCameraView = glm::mat4(1.f);
	mutable glm::mat4 activeProjection =glm::mat4(1.f);
	mutable glm::mat4 activeView = glm::mat4(1.f);
};