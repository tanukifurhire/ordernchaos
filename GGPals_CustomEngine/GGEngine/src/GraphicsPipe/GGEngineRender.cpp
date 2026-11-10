/*************************************************************************
  \file       GGEngineRender.cpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       3 October 2025
  \version    1.0
  \brief
	Implementation of GGEngineRender, the core rendering system of GGEngine.
  \details
	Implements 2D rendering, text rendering via FontAtlas and FreeType,
	orthographic projection setup, ECS integration, shader and texture
	management, and framebuffer handling for game/editor views.
*************************************************************************/
#include <GGHelper.hpp>

#include <GGEngineRender.hpp>

#include <GGWindow.hpp>
#include <GGrender2d.hpp>

#include <GGSystemManagers.hpp>

#include <GGJSONConfig.hpp>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <SOIL2/SOIL2.h>


#include <GGECSAssetInterface.hpp>
#include <string>
#include <GGColor2DIndexing.hpp>

/*************************************************************************
  \brief
	Constructs the GGEngineRender object.
  \details
	Initializes member variables and prepares default resources for rendering.
*************************************************************************/
GGEngineRender::GGEngineRender() {
	brickTexture={};
}


/*************************************************************************
  \brief
	Destructor for GGEngineRender, cleans up all allocated resources.
  \details
	Deletes OpenGL buffers, cleans up 2D rendering, framebuffer objects,
	and terminates GLFW.
*************************************************************************/
GGEngineRender::~GGEngineRender() {
	// clean up

	glDeleteBuffers(1, &defaultTex.texid);
	
	//if (m_render2dInitialized)
	render2d::cleanup();

	pickingFramebuffer.Cleanup();
	
	glfwTerminate();
}

/*************************************************************************
  \brief
	Loads default assets and initializes FreeType.
  \return
	True if loading succeeds, false otherwise.
  \details
	Sets up the FreeType library and loads the default font from the
	AssetManager or fallback paths.
*************************************************************************/
bool GGEngineRender::Load() {
	if (FT_Init_FreeType(&ft)) {
		std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
		return false;
	}

	// UPDATED: Use AssetManager path resolution instead of hardcoded path
	const std::string engineFontPath = "../../../EngineAssets/Fonts/SourceSans3-Bold.ttf";

	if (FT_New_Face(ft, engineFontPath.c_str(), 0, &defaultFont.face)) {
		std::cout << "ERROR::FREETYPE: Failed to load font from: " << engineFontPath << std::endl;
		return false;
	}

		std::cout << "SUCCESS: Loaded font from AssetManager: " << engineFontPath << std::endl;

	FT_Set_Pixel_Sizes(defaultFont.face, 0, 48);

	if (FT_Load_Char(defaultFont.face, 'X', FT_LOAD_RENDER)) {
		std::cout << "ERROR::FREETYPE: Failed to load Glyph" << std::endl;
		return false;
	}

	return true;
}


///// used to dubug
//static void TestEntityIDEncoding() {
//	std::cout << "\n=== Testing Entity ID Encoding ===" << std::endl;
//
//	uint32_t testIDs[] = { 1, 2, 5, 6, 255, 256, 65535, 16777215 };
//
//	for (uint32_t id : testIDs) {
//		glm::vec4 color = Color2DIndexing::IDToColor(id);
//
//		// Simulate what the shader does - convert to 0-255 range
//		uint8_t r = static_cast<uint8_t>(color.r * 255.0f);
//		uint8_t g = static_cast<uint8_t>(color.g * 255.0f);
//		uint8_t b = static_cast<uint8_t>(color.b * 255.0f);
//
//		// Decode back (same as ReadPixel does)
//		uint32_t decoded = r | (g << 8) | (b << 16);
//
//		std::cout << "ID " << id
//			<< "  Color(" << color.r << "," << color.g << "," << color.b << ")"
//			<< "  RGB(" << (int)r << "," << (int)g << "," << (int)b << ")"
//			<< "  Decoded ID " << decoded
//			<< (id == decoded ? " correct" : "  MISMATCH!")
//			<< std::endl;
//	}
//
//	std::cout << "=== End Test ===" << std::endl;
//}

/*************************************************************************
  \brief
	Initializes the rendering system and window.
  \return
	True if initialization succeeded, false otherwise.
  \details
	Creates the main window, sets viewport, loads shaders, initializes
	2D rendering, sets frame rate limits, and configures input manager
	picking framebuffer.
*************************************************************************/
bool GGEngineRender::Init() {
	if (!GGHelper::InitWindow(mainWindow,
		GGEngineConfig::GGEngineGraphics::GGEngineWindow::WindowWidth(),
		GGEngineConfig::GGEngineGraphics::GGEngineWindow::WindowHeight(),
		GGEngineConfig::GGEngineGraphics::GGEngineWindow::WindowName()
		))
	{
		
		return false;
	}

	glViewport(0, 0, mainWindow.GetWindowWidth(), mainWindow.GetWindowHeight());

	glfwSetFramebufferSizeCallback(mainWindow.GetGLFWwindow(), GGEngineRender::FramebufferResizeCallback);

	// Fallback to hardcoded paths if AssetManager fails
	const std::string engineShaderPath = "../../../EngineAssets/Shaders/";
	defaultShader = GGShader( engineShaderPath + "DefaultShader.vert", engineShaderPath + "DefaultShader.frag");
	defaultFontShader = GGShader(engineShaderPath + "DefaultFontShader.vert", engineShaderPath + "DefaultFontShader.frag");
	defaultPickingShader = GGShader(engineShaderPath + "PickingShader.vert", engineShaderPath + "PickingShader.frag");

	if (defaultShader.GetShaderID() == 0)
		std::cout << "CRITICAL: DefaultShader failed to compile � nothing will render" << std::endl;

	GGLogger::Info("Renderer Init create defaults!");
	CreateDefaults();
	render2d::init((defaultProjection));
	//m_render2dInitialized = true;

	glfwSwapInterval(0);

	// Configure frame limiting
	auto& timeManager = Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>();
	timeManager.SetTargetFPS(GGEngineConfig::GGEngineGraphics::GGEngineRenderer::FrameRateCap());

	auto& renderMgr = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>();
	
	auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();

	GGLogger::Info("Renderer Init set input framebuffer!");

	inputMgr.SetPickingFramebuffer(&renderMgr.pickingFramebuffer);

	GGLogger::Info("Renderer Init set viewport size!");

	// Set initial viewport size for input manager
	inputMgr.UpdateViewportSize(glm::vec2(
		static_cast<float>(mainWindow.GetWindowWidth()),
		static_cast<float>(mainWindow.GetWindowHeight())
	));

	GGLogger::Info("Renderer Init set resolution!");
	
	mainWindow.SetResolution(GGEngineConfig::GGEngineGraphics::GGEngineWindow::WindowWidth(), GGEngineConfig::GGEngineGraphics::GGEngineWindow::WindowHeight());

	GGLogger::Info("Renderer Init set aspect ratio!");
	mainWindow.GetWindowConfig().SetAspectRatio(16.0f / 9.0f);

	
#ifndef GG_EDITOR_BUILD
	if (GGEngineConfig::GGEngineGraphics::GGEngineWindow::IsFullScreen()) {
		// Borderless, NOT exclusive fullscreen.
		//
		// WindowMode::FULLSCREEN calls glfwSetWindowMonitor with a real monitor,
		// which sets the display mode. Windows responds to a mode set by
		// resizing and repositioning every other window on the desktop to fit,
		// and it does not put them back afterwards - the reported symptom was
		// Discord coming back shrunk into the top-left corner after quitting.
		// Multi-adapter setups (virtual displays, remote-desktop adapters) make
		// this worse because the mode set can reconfigure those displays too.
		//
		// Borderless covers the monitor without changing its mode, so nothing
		// outside the game moves. It also alt-tabs cleanly and lets the Steam
		// overlay composite properly, both of which matter for the Steam build.
		GGLogger::Info("Renderer Init set borderless fullscreen!");
		GGHelper::SetInitialWindowMode(mainWindow, WindowMode::BORDERLESS_WINDOWED);
	}
	else {
		GGLogger::Info("Renderer Init set windowed!");
		GGHelper::SetInitialWindowMode(mainWindow, WindowMode::WINDOWED);
	}
#else
	GGHelper::SetInitialWindowMode(mainWindow, WindowMode::WINDOWED);
#endif
	
	GGLogger::Info("Renderer Init success!");

	return true;
}

/*************************************************************************
  \brief
	Sets up default projection, model matrix, and creates default mesh.
  \details
	Constructs orthographic projection, default scaling, default mesh
	vertices/indices, default texture, and font atlas.
*************************************************************************/
void GGEngineRender::CreateDefaults() {
	// Construct the default projection matrix
	defaultProjection = glm::mat4(1.f);
	
	defaultProjection = glm::ortho(-static_cast<GLfloat>(mainWindow.GetWindowWidth()) / 2.0f /basicCamera.GetZoom(), static_cast<GLfloat>(mainWindow.GetWindowWidth()) / 2.0f / basicCamera.GetZoom(), -static_cast<GLfloat>(mainWindow.GetWindowHeight()) / 2.0f / basicCamera.GetZoom(), static_cast<GLfloat>(mainWindow.GetWindowHeight()) / 2.0f / basicCamera.GetZoom());

	// Construct the default model matrix
	defaultModelMatrix = glm::mat4(1.f);
	defaultModelMatrix = glm::translate(defaultModelMatrix, { 0.f, 0.f, 0.f });
	defaultModelMatrix = glm::scale(defaultModelMatrix, glm::vec3(targetAspectRatio * static_cast<float>(mainWindow.GetWindowHeight()) / 2.f, static_cast<float>(mainWindow.GetWindowHeight()) / 2.f, 1.f));

	defaultWorldScale = glm::scale(glm::mat4(1.f), { WORLD_TO_SCREEN_SCALE, WORLD_TO_SCREEN_SCALE, 1.f });

	// We create the default Mesh vertices data
	std::vector<GGVertex> vertices = {
		{ {  0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.f }, { 1.0f, 0.0f } },
		{ {  0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.f }, { 1.0f, 1.0f } },
		{ { -0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.f }, { 0.0f, 1.0f } },
		{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.f }, { 0.0f, 0.0f } }
	};
	// Create the default Mesh indices data
	std::vector<GLushort> indices = {
		0, 1, 3, 2
	};
	defaultMesh.CreateMesh(vertices, indices, GL_TRIANGLE_STRIP,true);
	// Default UV bounds: full texture
	defaultUVBounds = glm::vec4(0.f, 0.f, 1.f, 1.f);

	// Default UV transform: identity
	defaultUVTransform = glm::mat4(1.f);

	// FontAtlas (replaces legacy 128 individual textures)
	std::cout << "Initializing modern FontAtlas system..." << std::endl;

	try
	{
		GGFontAtlas& fAtlas = GGFontAtlasManager::GetFontAtlas("../../../EngineAssets/Fonts/SourceSans3-Bold.ttf", 48);

		if (fAtlas.IsValid())
		{
			// Store the atlas reference in member pointer
			fontAtlas = &fAtlas;

			auto dimensions = fAtlas.GetAtlasDimensions();
			std::cout << "SUCCESS: FontAtlas ready and stored" << std::endl;
		}
		else
		{
			std::cout << "ERROR: FontAtlas creation failed" << std::endl;
			fontAtlas = nullptr;
		}
	}
	catch (const std::exception& e)
	{
		std::cout << "ERROR: FontAtlas failed: " << e.what() << std::endl;
		fontAtlas = nullptr;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glGenVertexArrays(1, &defaultFont.vaoid);
	glGenBuffers(1, &defaultFont.vboid);
	glBindVertexArray(defaultFont.vaoid);
	glBindBuffer(GL_ARRAY_BUFFER, defaultFont.vboid);

	float font_vertices[6][4] = {
		{0.f, 1.f,	0.0f, 0.0f},
		{0.f, 0.f,	0.0f, 1.0f},
		{1.f, 0.f,	1.0f, 1.0f},

		{0.f, 1.f,	0.0f, 0.0f},
		{1.f, 0.f,	1.0f, 1.0f},
		{1.f, 1.f,	1.0f, 0.0f}
	};

	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, font_vertices, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

/*************************************************************************
  \brief
	Updates the default projection and model matrices based on window size.
  \param width
	New width of the window.
  \param height
	New height of the window.
  \details
	Recomputes the orthographic projection matrix and default model
	matrix using the target aspect ratio. Updates 2D renderer projection.
*************************************************************************/
void GGEngineRender::SetDefaultProjection(int width, int height) {
	glm::mat4 ortho_proj = glm::mat4(1.f);

	ortho_proj = glm::ortho(-static_cast<GLfloat>(width) / 2.f / basicCamera.GetZoom(), static_cast<GLfloat>(width) / 2.f / basicCamera.GetZoom(), -static_cast<GLfloat>(height) / 2.f / basicCamera.GetZoom(), static_cast<GLfloat>(height) / 2.f / basicCamera.GetZoom());

	mainWindow.SetSize(width, height);

	defaultModelMatrix = glm::mat4(1.f);

	defaultModelMatrix = glm::translate(defaultModelMatrix, glm::vec3(0.f, 0.f, 0.f));

	defaultModelMatrix = glm::scale(defaultModelMatrix, glm::vec3(targetAspectRatio * static_cast<float>(mainWindow.GetWindowHeight()) / 2.f, static_cast<float>(mainWindow.GetWindowHeight()) / 2.f, 1.f));
	defaultProjection = ortho_proj;
	render2d::SetProjection(defaultProjection);
	//defaultShader.SetMat4("projection", defaultProjection);
}


/*************************************************************************
  \brief
	Updates the rendering system each frame.
  \details
	Updates the game camera, clears buffers, handles fullscreen toggle,
	and updates the input manager with the current camera matrix.
*************************************************************************/
void GGEngineRender::Update() {
	
	
	auto& timeMgr = Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>();
	float deltaTime = static_cast<float>(timeMgr.GetDeltaTime());
	UpdateGameCamera(deltaTime);
	glClearColor(.28f, .35f, .45f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// All rendering happens here (ECS systems will render to FBO)
	auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();
	if (inputMgr.GGInputCheckTriggered(GGKey_F11)) {
		std::cout << "fullscreen toggle presed\n";
		mainWindow.ToggleFullscreen();
	}


#ifndef  GG_EDITOR_BUILD
	if (!mainWindow.IsFullscreen()) {
		mainWindow.SetResizable(GGEngineConfig::GGEngineGraphics::GGEngineWindow::IsResizable());
		mainWindow.SetResolution(1600, 900);
	}
#endif 
	

	if (Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().gameCamera) {
		glm::mat4 cameraViewProj = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().gameCamera->GetProjectionMatrix() *
			Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().gameCamera->GetViewMatrix4D();
		inputMgr.UpdateCameraMatrix(cameraViewProj);
		gameCamera->ApplyBounds();
	}
}

/*
* @brief Gets the print size of a text as a vec2 to help center the RenderText.
*/
// GetPrintSize using FontAtlas
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
glm::vec2 GGEngineRender::GetPrintSize(std::string const& text, float scale)
{
	if (fontAtlas && fontAtlas->IsValid())
	{
		return fontAtlas->GetTextSize(text, scale);
	}

	// Return empty if font atlas not available
	std::cout << "WARNING: GetPrintSize called but FontAtlas not available" << std::endl;
	return glm::vec2(0, 0);
}

/*
* @brief Renders a string of text onto the screen at positions {x}, {y} and scale {scale}, with vec3 {color} using Shader s
* 
* @remarks Due for change, probably towards end of M1? We should be changing to Texture Atlas instead.
*/
// RenderText using FontAtlas
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
void GGEngineRender::RenderText(GGShader& shader, std::string text, float x, float y, float scale, glm::vec3 color)
{
	if (!GetActiveCamera()) return;

	if (fontAtlas && fontAtlas->IsValid())
	{
		// Set SCREEN SPACE projection matrix for the shader
		shader.Use();
		shader.SetMat4("projection", GetActiveCamera()->GetScreenProjectionMatrix());

		// Use FontAtlas for rendering
		fontAtlas->RenderText(shader.GetShaderID(), text, x, y, scale, color);

		shader.UnUse();
	}
	else
	{
		std::cout << "ERROR: RenderText called but FontAtlas not available" << std::endl;
	}
}
/*************************************************************************
  \brief
	Callback function for GLFW framebuffer resize events.
  \param window
	Pointer to GLFW window.
  \param width
	New framebuffer width.
  \param height
	New framebuffer height.
  \details
	Updates viewport, projection, game camera, input manager, and resizes
	framebuffers accordingly.
*************************************************************************/
void GGEngineRender::FramebufferResizeCallback([[maybe_unused]] GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
	
	Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().SetDefaultProjection(width, height);


	Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().mainWindow.SetSize(width, height);
	Systems::GetSystemsRegistry()
		.GetSystem<Systems::RenderManager>()
		.ResizeGameViewFramebuffer(width, height);

#ifndef _DEBUG


	// Update camera viewport size
	if (Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().gameCamera) {
		Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().gameCamera->SetViewportSize(glm::vec2(
			static_cast<float>(width),
			static_cast<float>(height)
		));
	}

	// Update input manager viewport
	auto& inputMgr = Systems::GetSystemsRegistry().GetSystem<Systems::InputManager>();
	inputMgr.UpdateViewportSize(glm::vec2(
		static_cast<float>(width),
		static_cast<float>(height)
	));
	// Update camera matrix for input manager
	if (Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().gameCamera) {
		glm::mat4 cameraViewProj = Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().gameCamera->GetProjectionMatrix() *
			Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().gameCamera->GetViewMatrix4D();
		inputMgr.UpdateCameraMatrix(cameraViewProj);
	}

	std::cout << "Resized game view framebuffer: " << width << "x" << height << std::endl;
#else

#endif
}

/*************************************************************************
  \brief
	Initializes the game view framebuffer and picking framebuffer.
  \param width
	Width of framebuffer.
  \param height
	Height of framebuffer.
*************************************************************************/
void GGEngineRender::CreateGameViewFramebuffer(int width, int height) {
	gameViewWidth = width;
	gameViewHeight = height;

	viewBuffer.Init(width, height);
	pickingFramebuffer.Init(width, height);

	/*std::cout << "Picking FBO size: "
		<< pickingFramebuffer.getWidth() << "x"
		<< pickingFramebuffer.getHeight() << std::endl;*/

}

/*************************************************************************
  \brief
	Prints a 4x4 matrix to console.
  \param mat
	Matrix to print.
  \param name
	Optional name for the matrix.
*************************************************************************/
void GGEngineRender::PrintMat4(const glm::mat4& mat, const std::string& name = "Matrix") {
	std::cout << name << ":\n";
	for (int row = 0; row < 4; ++row) {
		std::cout << "| ";
		for (int col = 0; col < 4; ++col) {
			std::cout << mat[col][row] << " ";  
		}
		std::cout << "|\n";
	}
	std::cout << std::endl;
}

/*************************************************************************
  \brief
	Initializes the 2D game camera.
  \param width
	Viewport width.
  \param height
	Viewport height.
*************************************************************************/
void GGEngineRender::InitializeGameCamera(float width, float height)
{
	gameCamera = std::make_unique<GGGameCamera2D>();
	gameCamera->SetViewportSize(glm::vec2(width, height));

	std::cout << "[RenderManager] Game camera initialized: "
		<< width << "x" << height << std::endl;
}

/*************************************************************************
  \brief
	Updates the game camera per frame.
  \param deltaTime
	Time elapsed since last frame.
*************************************************************************/
void GGEngineRender::UpdateGameCamera(float deltaTime)
{
	if (gameCamera) {
		
		gameCamera->Update(deltaTime);
	}
}

/*************************************************************************
  \brief
	Resets the game camera to default position, zoom, and clears follow target.
*************************************************************************/
void GGEngineRender::ResetGameCamera()
{
	if (!gameCamera) return;

	gameCamera->SetPosition(glm::vec2(0.0f));
	gameCamera->SetZoom(64.0f);
	gameCamera->ClearFollowTarget();
	std::cout << "[RenderManager] Game camera reset\n";
}


/*************************************************************************
  \brief
	Resizes the game view and picking framebuffers.
  \param width
	New width.
  \param height
	New height.
*************************************************************************/
void GGEngineRender::ResizeGameViewFramebuffer(int width, int height) {
	// resize the picking buffer ,view buffer do edioter imgui
	gameViewHeight = height;
		gameViewWidth = width;
	viewBuffer.Resize(width, height);
	pickingFramebuffer.Resize(width, height);

#ifndef _DEBUG


#else
	viewBuffer.Resize(width, height);

#endif
	
}

/*************************************************************************
  \brief
	Binds the game view framebuffer and sets viewport.
*************************************************************************/
void GGEngineRender::BindGameViewFramebuffer() {

	viewBuffer.BindBuffer();
	// Just bind the framebuffer and set viewport
	// Projection is already set by Create/Resize methods
	
	glViewport(0, 0, gameViewWidth, gameViewHeight);
}

/*************************************************************************
  \brief
	Unbinds the game view framebuffer and restores main window viewport.
*************************************************************************/
void GGEngineRender::UnbindGameViewFramebuffer() {


	viewBuffer.UnbindBuffer();
	glViewport(0, 0, mainWindow.GetWindowWidth(), mainWindow.GetWindowHeight());

}

/*************************************************************************
  \brief
	Renders overlay text such as FPS counter or debug info.
  \details
	Uses default font shader and FontAtlas to draw overlays at specified
	positions, scales, and colors.
*************************************************************************/
void GGEngineRender::RenderFontOverlays()
{
	if (!GetActiveCamera()) return;

	// Use the engine font shader
	auto& fontShader = GetDefaultFontShader();
	
	// --- Draw Custom Overlay Text ---
	if (overlayTextEnabled)
	{
		RenderText(fontShader, overlayText, overlayTextX, overlayTextY, overlayTextScale, overlayTextColor);
	}

	// --- Draw FPS Counter ---
	if (overlayShowFPS)
	{
		auto& timeMgr = Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>();
		

		double fps = timeMgr.GetFPS();
		std::string fpsText = "FPS: " + std::to_string((int)fps);

		// Convert to centered projection coordinates
		float halfW = gameViewWidth * 0.5f;
		float halfH = gameViewHeight * 0.5f;

		auto textDimensions = GetPrintSize(fpsText, 1.0f);

		float x = halfW - textDimensions.x;
		float y = halfH - textDimensions.y;

		RenderText(fontShader, fpsText, x, y, 1.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	}
}
