/*************************************************************************
  \file       PrefabInspectorNode.cpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       07 November 2025
  \version    1.0
  \brief
    Implements the PrefabInspectorNode class, which provides an ImGui-based
    editor interface for inspecting and editing prefabs in the ECS world.

  \details
    PrefabInspectorNode allows developers to:
    - Display a selected prefab from the hierarchy panel.
    - Edit basic transform properties (position and scale) of a prefab instance.
    - Save modified prefab instances as new prefabs using PrefabManager.
    - Destroy all spawned objects in the current prefab world.
    - Dock the inspector window in the editor layout.

    The class maintains its own initialization, update, and docking callbacks,
    ensuring the inspector is an isolated workspace for prefab editing.

  \dependencies
    <PrefabInspectorNode.hpp> <HierarchyNode.hpp> <GGSystemManagers.hpp> <GGSelectables.hpp>
    <InspectorNode.hpp> <GGLogger.hpp> <imgui_internal.h> <GGEngine.hpp>

  \history
    - 1.0 (07 November 2025): Initial implementation of PrefabInspectorNode
      with ImGui interface for prefab editing, saving, and object management.

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <PrefabInspectorNode.hpp>
#include <HierarchyNode.hpp> // to get selected entity in the hierarchy panel
#include <GGSystemManagers.hpp>
#include <InspectorNode.hpp>
#include <GGLogger.hpp>

#include <imgui_internal.h>
#include <GGEngine.hpp>
#include <EditorEventBus.hpp>

/*************************************************************************
    \brief Constructor for PrefabInspectorNode
    \details Initializes the callback handles for Init, Update, and DockBuild
*************************************************************************/
PrefabInspectorNode::PrefabInspectorNode()
{
	initCallbackHandle = [this]() -> void { this->Init(); };
	updateCallbackHandle = [this]() -> void { this->Update(); };
	dockBuildCallbackHandle = [this]() -> void { this->DockBuild(); };
}

/*************************************************************************
    \brief Initialize the PrefabInspectorNode
    \details Currently empty, reserved for future setup or initialization
*************************************************************************/
void PrefabInspectorNode::Init() {
	

}

/*************************************************************************
    \brief Dock the Prefab Inspector window in the editor layout
    \details Uses ImGui's DockBuilder to dock the window to the right side
             as defined by the EditorManager's docking settings
*************************************************************************/
void PrefabInspectorNode::DockBuild() {
	ImGui::DockBuilderDockWindow("PrefabInspector", Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().dockRight);
}

/*************************************************************************
    \brief Update and render the Prefab Inspector GUI
    \details Handles:
             - Displaying the selected prefab from the hierarchy panel
             - Editing transform components (position and scale)
             - Prefab save functionality via PrefabManager
             - Destroying all spawned objects in the current prefab world
             - Rendering ImGui widgets and handling user input
*************************************************************************/
void PrefabInspectorNode::Update() {
	ImGui::SetNextWindowSize(ImVec2(800, 700), ImGuiCond_Always);

	auto& prefabManager = GGEngine::GetEngine().GetPrefabManager();
	auto& objectsFactory = GGEngine::GetEngine().GetObjectsFactory();
	auto& sceneManager = GGEngine::GetEngine().GetSceneManager();
    auto& prefabWorld = sceneManager.GetActiveScene();

	if (ImGui::Begin("PrefabInspector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
		Entity selectedPrefab = GetHierarchyNode()->GetSelectedEntity();
		// if an entity has been selected
		if (selectedPrefab != NULL_ENTITY) {
			// prefab editing
			ImGui::Separator();
			static float inputWidth{ 80.f };
			static float xPos{ 0.f }, yPos{ 0.f };
			static float sx{ 1.f }, sy{ 1.f };

			if (selectedPrefab.IsValid()) {
                // get the entity's transform
                if (auto* transform = prefabWorld.GetComponent<LocalTransform>(selectedPrefab)) {
                    // initialize sliders with current values
                    xPos = transform->position.x;
                    yPos = transform->position.y;
                    sx = transform->scale.x;
                    sy = transform->scale.y;

                    // sliders for position
                    ImGui::Text("Position: ");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(inputWidth);
                    if (ImGui::DragFloat("xPos", &xPos, 0.1f, -10.f, 10.f)) {
                        transform->position.x = xPos;
                    }
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(inputWidth);
                    if (ImGui::DragFloat("yPos", &yPos, 0.1f, -10.f, 10.f)) {
                        transform->position.y = yPos;
                    }

                    ImGui::Spacing();

                    // sliders for scale
                    ImGui::Text("Scale X: ");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(inputWidth);
                    if (ImGui::DragFloat("sx", &sx, 0.1f, 0.1f, 10.f)) {
                        transform->scale.x = sx;
                    }

                    ImGui::Text("Scale Y: ");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(inputWidth);
                    if (ImGui::DragFloat("sy", &sy, 0.1f, 0.1f, 10.f)) {
                        transform->scale.y = sy;
                    }

                    transform->isDirty = true;
                }
                else {
                    ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Selected prefab has no Transform component!");
                }
			}
            else {
                ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "No prefab selected. Select one to edit.");
            }
		}

        // prefab save option
        ImGui::Separator();
        if (selectedPrefab.IsValid()) {
            static char prefabSaveName[128] = ""; // buffer for prefab name input

            // show original prefab name if available
            if (auto* prefabInst = prefabWorld.GetComponent<PrefabInstance>(selectedPrefab)) {
                auto originalName = (prefabWorld.GetComponent<PrefabInstance>(selectedPrefab) ? prefabWorld.GetComponent<PrefabInstance>(selectedPrefab)->GetPrefabInstanceName() : "Unknown Prefab");
                ImGui::Text("Original Prefab: %s", originalName.c_str());
                // pre-fill input buffer with original name if empty
                if (strlen(prefabSaveName) == 0) {
                    strncpy(prefabSaveName, originalName.c_str(), sizeof(prefabSaveName));
                }
            }

            ImGui::InputText("##Save As", prefabSaveName, IM_ARRAYSIZE(prefabSaveName));

            if (ImGui::Button("Save Prefab")) {
                std::string newName(prefabSaveName);
                if (!newName.empty()) {
                    bool success = prefabManager.SavePrefab(selectedPrefab, newName, prefabWorld);
                    //sceneManager.PropagatePrefabChanges(prefabManager);
                    sceneManager.SyncPrefabInstances(newName, prefabManager);
                    if (success) {
                        GGLogger::Info("Prefab saved as: " + newName);
                        EditorEventBus::onAssetSystemRefresh.Emit();
                    }
                    else {
                        GGLogger::Error("Failed to save prefab: " + newName);
                    }
                }
            }
        }
        else {
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Select a root entity first to save as prefab.");
        }

        // destroy prefab object
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.0f, 0.0f, 1.0f));
        if (ImGui::Button("DestroyAllObjects")) {
            if (!(prefabWorld.GetLivingEntities()).empty()) {
                objectsFactory.DestroyAllSpawnedObjects(prefabWorld);
                GGLogger::Info("All objects destroyed");
            }
            else {
                GGLogger::Info("No objects exist to be destroyed");
            }
        }
        ImGui::PopStyleColor();
	}

	ImGui::End();
}