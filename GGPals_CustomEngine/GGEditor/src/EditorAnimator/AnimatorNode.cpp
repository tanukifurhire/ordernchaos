/*************************************************************************
  \file		  AnimatorNode.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief	  Implements the AnimatorNode used by the GGEngine editor.
  \details
	Provides the functionality for the Animator docking window within the
	editor. This includes initializing the node, handling per-frame updates,
	and building the ImGui docking layout for the Animator panel. The window
	currently serves as a placeholder for future animation editing tools and
	visualization features.
  \dependencies
	- AnimatorNode.hpp
	- GGSystemManagers.hpp
	- ImGui (imgui_internal.h)
  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <AnimatorNode.hpp>
#include <GGSystemManagers.hpp>

#include <imgui_internal.h>

/*************************************************************************
  \brief
	Constructs an AnimatorNode and sets up its callback handles
	for initialization, update, and dock building.
*************************************************************************/
AnimatorNode::AnimatorNode()
{
	initCallbackHandle = [this]() -> void { this->Init(); };
	updateCallbackHandle = [this]() -> void { this->Update(); };
	dockBuildCallbackHandle = [this]() -> void { this->DockBuild(); };
}

/*************************************************************************
  \brief
	Initializes the AnimatorNode. Currently performs no operations.
*************************************************************************/
void AnimatorNode::Init()
{

}

/*************************************************************************
  \brief
	Builds and docks the Animator window in the editor UI.
*************************************************************************/
void AnimatorNode::DockBuild()
{
	ImGui::DockBuilderDockWindow("Animator", Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().dockBottom);
}

/*************************************************************************
  \brief
	Updates the Animator window each frame, setting its size
	and managing ImGui rendering.
*************************************************************************/
void AnimatorNode::Update()
{
	ImGui::SetNextWindowSize(ImVec2(800, 700), ImGuiCond_Always);

	if (ImGui::Begin("Animator", nullptr, ImGuiWindowFlags_NoResize))
	{

	}

	ImGui::End();
}