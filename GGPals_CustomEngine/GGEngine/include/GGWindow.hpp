/*************************************************************************
  \file       GGWindow.hpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       03 October 2025
  \brief
	Wrapper class for GLFW window management, handling window size, title,
	and FPS display updates.

  \details
	GGWindow encapsulates GLFWwindow creation and management, including:
	- Window size and title control
	- FPS display in the window title
	- Getter and setter for the native GLFWwindow pointer
	- Default window size and name
*************************************************************************/
#pragma once
#include <string>
#include <GGJSONConfig.hpp>
#include <GGWindowConfig.hpp>


struct GLFWwindow;

class GGWindow {
public:
	/*************************************************************************
	  \brief Default constructor.
	  \details
		Initializes a GGWindow object with default width, height, and name.
	*************************************************************************/
	GGWindow();

	/*************************************************************************
	  \brief Constructs a GGWindow with specified size and name.
	  \param width Initial window width in pixels.
	  \param height Initial window height in pixels.
	  \param name Initial window title.
	*************************************************************************/
	GGWindow(int width, int height, std::string const& name);

	/*************************************************************************
	  \brief Initializes the GGWindow object.
	  \param width Window width in pixels.
	  \param height Window height in pixels.
	  \param name Window title.
	  \details
		Sets the window size and name; actual GLFW window creation should
		be done separately.
	*************************************************************************/
	void Init(int width, int height, std::string const& name);


	/*************************************************************************
	  \brief Destructor.
	  \details
		Destroys the GLFW window if it exists.
	*************************************************************************/
	~GGWindow();

	int GetWindowWidth()const;
	int GetWindowHeight()const;
	GLFWwindow* GetGLFWwindow()const;

	/*************************************************************************
	  \brief Sets the window size.
	  \param width New window width in pixels.
	  \param height New window height in pixels.
	  \details
		Updates internal width/height values and resizes the GLFW window if it exists.
	*************************************************************************/
	void SetSize(int width, int height);

	/*************************************************************************
	  \brief Updates the window title to display FPS.
	  \param fps Frames per second to display.
	  \details
		Combines the default window name with FPS information in the title.
	*************************************************************************/
	void setfps(int fps);

	/*************************************************************************
	  \brief Sets the window title.
	  \param name New window title.
	  \details
		Updates both the internal title string and the GLFW window if it exists.
	*************************************************************************/
	void SetName(std::string const& name);
	std::string GetName()const;
	void SetGLFWwindow(GLFWwindow* win);
	std::string defaultname { "GGPals!" };

	/*************************************************************************
	  \brief Sets the window mode (windowed, fullscreen, borderless).
	  \param mode The desired WindowMode.
	  \details
		Delegates to the GGWindowConfig to handle the mode transition.
	*************************************************************************/
	void SetWindowMode(WindowMode mode);

	/*************************************************************************
	  \brief Toggles between windowed and fullscreen modes.
	  \details
		Convenience function to switch between windowed and fullscreen.
	*************************************************************************/
	void ToggleFullscreen();

	/*************************************************************************
	  \brief Gets the current window mode.
	  \return The current WindowMode.
	*************************************************************************/
	WindowMode GetWindowMode() const;

	/*************************************************************************
	  \brief Checks if the window is in fullscreen mode.
	  \return True if fullscreen, false if windowed.
	*************************************************************************/
	bool IsFullscreen() const;

	/*************************************************************************
	  \brief Sets the resolution using width and height.
	  \param width Desired width in pixels.
	  \param height Desired height in pixels.
	  \details
		Changes the resolution while maintaining the current window mode.
	*************************************************************************/
	void SetResolution(int width, int height);

	/*************************************************************************
	  \brief Sets the resolution using a Resolution object.
	  \param res The desired Resolution.
	*************************************************************************/
	void SetResolution(const Resolution& res);

	/*************************************************************************
	  \brief Sets the window to use the best available resolution.
	  \details
		Automatically detects and applies the native monitor resolution.
	*************************************************************************/
	void SetBestResolution();

	/*************************************************************************
	  \brief Gets the current resolution.
	  \return The current Resolution.
	*************************************************************************/
	Resolution GetCurrentResolution() const;

	/*************************************************************************
	  \brief Gets the best available resolution for this system.
	  \return The optimal Resolution for the primary monitor.
	*************************************************************************/
	Resolution GetBestResolution() const;

	/*************************************************************************
	  \brief Gets all available resolutions for the primary monitor.
	  \return Vector of available Resolution objects.
	*************************************************************************/
	std::vector<Resolution> GetAvailableResolutions() const;

	// Window configuration access
	GGWindowConfig& GetWindowConfig() { return windowConfig; }
	const GGWindowConfig& GetWindowConfig() const { return windowConfig; }

	/*************************************************************************
	  \brief Enable or disable window resizing.
	  \param resizable True to allow resizing, false to fix size.
	*************************************************************************/
	void SetResizable(bool resizable);

	/*************************************************************************
	  \brief Check if window is resizable.
	  \return True if resizable, false otherwise.
	*************************************************************************/
	bool IsResizable() const;

private:
	int WindowWidth { 960 };
	int WindowHeight{ 540 };
	std::string windowName{ "GGPals!" };
	GLFWwindow* window{ nullptr };
	GGWindowConfig windowConfig;

};