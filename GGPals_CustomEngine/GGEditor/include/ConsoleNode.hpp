/*************************************************************************
  \file		  ConsoleNode.hpp
  \author	  Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief	  Declares the ConsoleNode used for the Console dock tab 
              in the GGEngine editor.

  \details
	Defines the ConsoleNode class, which provides a dockable console
	window for displaying engine log messages inside the editor UI.
	The console supports colored message output, message counting,
	auto-scroll behavior, clearing logs, and integrates into the
	editor�s scene-node system through initialization, update, and
	docking callbacks.
  \dependencies
	- EditorSceneBase.hpp
	- <memory>
	- ImGui (rendered in the .cpp through imgui_internal.h)
	- GGLogger (via ConsoleNode.cpp)

  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <EditorSceneBase.hpp>
#include <memory>

class ConsoleNode : public EditorSceneBase
{
public:
	/*************************************************************************
	  \brief
		Constructs a ConsoleNode and sets up its callback handles
		for initialization, update, and dock building.
	*************************************************************************/
	ConsoleNode();

	/*************************************************************************
	  \brief
		Initializes the ConsoleNode. Currently performs no operations.
	*************************************************************************/
	virtual void Init() override;

	/*************************************************************************
	  \brief
		Builds and docks the Console window in the editor UI.
	*************************************************************************/
	virtual void DockBuild() override;

	/*************************************************************************
	  \brief
		Updates the Console window each frame, setting its size
		and managing ImGui rendering.
	*************************************************************************/
	virtual void Update() override;

	/*************************************************************************
	  \brief
		Renders the contents of the console window including
		messages, auto-scroll functionality, and the "Clear Console" button.
	*************************************************************************/
	void RenderConsole();

private:
	bool autoScrollConsole{ true };
	bool showErrors{ true };
	bool showWarnings{ true };
	bool showInfo{ false };
};

/*************************************************************************
  \brief
	Provides access to a singleton instance of ConsoleNode.
  \return
	Reference to the unique_ptr managing the ConsoleNode instance.
*************************************************************************/
inline auto& GetConsoleNode()
{
	static std::unique_ptr<ConsoleNode> console = std::make_unique<ConsoleNode>();
	return console;
}