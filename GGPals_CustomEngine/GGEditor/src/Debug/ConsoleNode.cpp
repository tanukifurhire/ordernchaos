/*************************************************************************
  \file       ConsoleNode.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief      Implements the ConsoleNode for the GGEngine editor.

  \details
    Provides the logic for the Console docked window inside the editor.
    This includes initialization, per-frame updates, docking behavior,
    and rendering of console log messages retrieved from GGLogger.
    The ConsoleNode supports colored message output, auto-scrolling,
    and manual clearing of the console through ImGui controls.
  \dependencies
    - ConsoleNode.hpp
    - GGSystemManagers.hpp
    - GGLogger.hpp
    - ImGui (imgui_internal.h)
  \history
    - 1.0 (10 October 2025): Initial version.

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <ConsoleNode.hpp>
#include <GGSystemManagers.hpp>

#include <imgui_internal.h>

#include <GGLogger.hpp>

/*************************************************************************
  \brief
    Constructs a ConsoleNode and sets up its callback handles
    for initialization, update, and dock building.
*************************************************************************/
ConsoleNode::ConsoleNode()
{
	initCallbackHandle = [this]() -> void { this->Init(); };
	updateCallbackHandle = [this]() -> void { this->Update(); };
	dockBuildCallbackHandle = [this]() -> void { this->DockBuild(); };
}

/*************************************************************************
  \brief
    Initializes the ConsoleNode. Currently performs no operations.
*************************************************************************/
void ConsoleNode::Init()
{

}

/*************************************************************************
  \brief
    Builds and docks the Console window in the editor UI.
*************************************************************************/
void ConsoleNode::DockBuild()
{
	ImGui::DockBuilderDockWindow("Console", Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().dockBottom);
}

/*************************************************************************
  \brief
    Updates the Console window each frame, setting its size
    and managing ImGui rendering.
*************************************************************************/
void ConsoleNode::Update()
{
	ImGui::SetNextWindowSize(ImVec2(800, 900), ImGuiCond_Always);

	if (ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoResize))
	{
		RenderConsole();
	}

	ImGui::End();
}

/*************************************************************************
  \brief
    Renders the contents of the console window including
    messages, auto-scroll functionality, and the "Clear Console" button.
*************************************************************************/
void ConsoleNode::RenderConsole()
{
	if (ImGui::Button("Clear Console"))
	{
		GGLogger::Clear();
	}

	ImGui::SameLine();
	ImGui::Checkbox("Auto-scroll", &autoScrollConsole);
	ImGui::SameLine();
	ImGui::Checkbox("Error", &showErrors);
	ImGui::SameLine();
	ImGui::Checkbox("Warning", &showWarnings);
	ImGui::SameLine();
	ImGui::Checkbox("Info", &showInfo);

	ImGui::Separator();

	ImGui::BeginChild("ConsoleOutput", ImVec2(0, 150), true, ImGuiWindowFlags_HorizontalScrollbar);

    const auto& messages = GGLogger::GetMessages();
    for (const auto& message : messages)
    {
        bool isError = message.find("[ERROR]") != std::string::npos;
        bool isWarning = message.find("[WARNING]") != std::string::npos;
        bool isInfo = !isError && !isWarning;

        // Filter messages based on checkbox states
        if ((isError && !showErrors) || (isWarning && !showWarnings) || (isInfo && !showInfo))
        {
            continue;
        }

        // Color code messages based on type
        if (isError)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f)); // Red
        }
        else if (isWarning)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f)); // Yellow
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f)); // Light gray
        }

        ImGui::TextWrapped("%s", message.c_str());
        ImGui::PopStyleColor();
    }

    // Auto-scroll to bottom
    if (autoScrollConsole && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();

    ImGui::Text("Messages: %zu", messages.size());
}