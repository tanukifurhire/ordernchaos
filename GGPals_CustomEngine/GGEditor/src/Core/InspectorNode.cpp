/*************************************************************************
  \file		  InspectorNode.cpp
  \author	  Tai Xiao Xuan, Tok Duan Jie Nelson
  \par        Email: xiaoxuan.tai@digipen.edu, t.duanjienelson@digipen.edu
  \date       22 November 2025
  \version    1.1
  \contribute Tai Xiao Xuan(80%), Nelson(20% Undo/ redo to components in RenderEntityInspector()),
  \brief
    Implements the InspectorNode for GGEngine editor.
  \details
	Handles displaying entity components in the inspector, adding/removing
	components, tracking modifications, and integrating undo/redo commands.
  \dependencies
	EditorSceneBase.hpp, GGECSWorld.hpp, GGObjectsFactory.hpp, EditorViewNode.hpp, GGEngine.hpp
  \history
	- 1.0 (02 November 2025): Initial version.
	- 1.1 (22 November 2025): Added Undo/ Redo logic to components in RenderEntityInspector()
  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <InspectorNode.hpp>
#include <GGSystemManagers.hpp>
#include <EditorEventBus.hpp>
#include <EditorUndoStack.hpp>
#include <TransformCommand.hpp>
#include <ComponentCommand.hpp>
#include <ComponentModifyCommand.hpp>
#include <EditorPlatform.hpp>

#include <imgui_internal.h>
#include <GGEngine.hpp>
#include <GGECSAssetInterface.hpp>

#pragma region Script Serialize macros

// Recursive helper to render composition fields in the inspector
static void RenderCompositionFields(GGScriptMetadata* parentMeta, EntityInstance* parentInst,
    GGECSScriptSystem& scriptSystem, lua_State* compL)
{
    for (const auto& compFieldName : parentMeta->compositionFields)
    {
        auto compFieldIt = parentMeta->fieldNameToIndex.find(compFieldName);
        if (compFieldIt == parentMeta->fieldNameToIndex.end()) continue;
        const FieldDescriptor& compField = parentMeta->fieldDescriptors[compFieldIt->second];
        char* compDataPtr = static_cast<char*>(parentInst->data);
        std::string* composedTypeName = static_cast<std::string*>(
            static_cast<void*>(compDataPtr + compField.offset));
        if (!composedTypeName || composedTypeName->empty()) continue;
        GGScriptMetadata* composedMeta = scriptSystem.GetMetadata(*composedTypeName);
        if (!composedMeta) continue;

        ImGui::PushID(compFieldName.c_str());
        if (ImGui::TreeNode(compFieldName.c_str(), "%s (%s)", compFieldName.c_str(), composedTypeName->c_str()))
        {
            if (parentInst->luaFieldsRef != LUA_NOREF && parentInst->luaFieldsRef != LUA_REFNIL)
            {
                lua_rawgeti(compL, LUA_REGISTRYINDEX, parentInst->luaFieldsRef);
                if (lua_istable(compL, -1))
                {
                    lua_getfield(compL, -1, compFieldName.c_str());
                    if (lua_isuserdata(compL, -1))
                    {
                        InstanceUserdata* subUD = (InstanceUserdata*)lua_touserdata(compL, -1);
                        if (subUD && subUD->instance && subUD->instance->data)
                        {
                            char* subDataPtr = static_cast<char*>(subUD->instance->data);

                            // Serialized fields
                            for (const auto& innerField : composedMeta->fieldDescriptors)
                            {
                                if (!composedMeta->serializedFields.count(innerField.name)) continue;
                                void* innerPtr = subDataPtr + innerField.offset;
                                ImGui::PushID(innerField.name.c_str());
                                switch (innerField.type)
                                {
                                case FieldDescriptor::TYPE_INT: {
                                    int val = *static_cast<int*>(innerPtr);
                                    if (ImGui::InputInt(innerField.name.c_str(), &val))
                                        *static_cast<int*>(innerPtr) = val;
                                    break;
                                }
                                case FieldDescriptor::TYPE_FLOAT: {
                                    float val = *static_cast<float*>(innerPtr);
                                    if (ImGui::InputFloat(innerField.name.c_str(), &val))
                                        *static_cast<float*>(innerPtr) = val;
                                    break;
                                }
                                case FieldDescriptor::TYPE_STRING: {
                                    std::string* strVal = static_cast<std::string*>(innerPtr);
                                    char buf[256];
                                    strncpy(buf, strVal->c_str(), sizeof(buf) - 1);
                                    buf[sizeof(buf) - 1] = '\0';
                                    if (ImGui::InputText(innerField.name.c_str(), buf, sizeof(buf)))
                                        *strVal = buf;
                                    break;
                                }
                                case FieldDescriptor::TYPE_BOOL: {
                                    bool val = *static_cast<bool*>(innerPtr);
                                    if (ImGui::Checkbox(innerField.name.c_str(), &val))
                                        *static_cast<bool*>(innerPtr) = val;
                                    break;
                                }
                                }
                                ImGui::PopID();
                            }

                            // TextureAsset fields
                            for (const auto& innerField : composedMeta->fieldDescriptors)
                            {
                                if (!composedMeta->textureAssetFields.count(innerField.name)) continue;
                                void* innerPtr = subDataPtr + innerField.offset;
                                ImGui::PushID(innerField.name.c_str());
                                switch (innerField.type)
                                {
                                case FieldDescriptor::TYPE_INT: {
                                    int* valuePtr = static_cast<int*>(innerPtr);
                                    ImGui::Button((std::string(innerField.name.c_str()) + ": " + std::to_string(*valuePtr)).c_str(), ImVec2(250, 0));
                                    if (ImGui::BeginDragDropTarget())
                                    {
                                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_TEXTURE"))
                                        {
                                            IM_ASSERT(payload->DataSize == sizeof(uint32_t));
                                            uint32_t droppedAssetID = *(const uint32_t*)payload->Data;
                                            *valuePtr = droppedAssetID;
                                        }
                                        ImGui::EndDragDropTarget();
                                    }
                                    break;
                                }
                                }
                                ImGui::PopID();
                            }

                            // Recurse into nested composition
                            if (!composedMeta->compositionFields.empty())
                                RenderCompositionFields(composedMeta, subUD->instance, scriptSystem, compL);
                        }
                    }
                    lua_pop(compL, 1); // Pop sub-instance userdata or nil
                }
                lua_pop(compL, 1); // Pop luaFields table
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}
// TODO: SERIALIZE ALL SERIALIZABLE FIELDS
#define INSPECTOR_FIELD_SCRIPT_ASSET_ID(label, value) \
	{ \
		ImGui::PushID(label); \
		std::string typeName = "None"; \
		std::string filePath; \
		if (value > 0) \
		{ \
			std::optional<uuids::uuid> uuidOpt = GGECSStandardAssetManager<GGScriptAsset>::GetUUIDFromID(value); \
			if (uuidOpt) \
			{ \
				typeName = GGAssetID::GetGGAssetID()->GetFilePath(*uuidOpt); \
				filePath = typeName; \
			} \
		} \
		ImGui::Button((std::string(label) + ": (" + std::to_string(value) + ") [" + typeName + "]").c_str(), ImVec2(350, 0)); \
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) \
		{ \
			if (!filePath.empty()) Platform::OpenInEditor(filePath); \
		} \
		if (ImGui::BeginDragDropTarget()) \
        { \
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_SCRIPT")) \
            { \
                IM_ASSERT(payload->DataSize == sizeof(uint32_t)); \
                uint32_t droppedAssetID = *(const uint32_t*)payload->Data; \
				value = droppedAssetID; \
				modified = true; \
            } \
            ImGui::EndDragDropTarget(); \
        } \
		ImGui::PopID(); \
	} 

#define INSPECTOR_FIELD_SCRIPT_INSTANCE(label, value) \
	{ \
		if (value > 0) \
		{ \
			ImGui::AlignTextToFramePadding(); \
			auto& scriptSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>(); \
			EntityInstance* instance = scriptSystem.GetScriptInstance(value); \
			if (instance != nullptr && instance->type) \
			{ \
				GGScriptMetadata* meta = instance->type; \
				ImGui::Text("Serializable Fields:"); \
				ImGui::Indent(); \
				for (const auto& field : meta->fieldDescriptors) \
				{ \
					if (meta->serializedFields.count(field.name) ==  0) continue; \
					char* dataPtr = static_cast<char*>(instance->data); \
					void* fieldPtr = dataPtr + field.offset; \
					ImGui::PushID(field.name.c_str()); \
					switch(field.type) \
					{ \
						case FieldDescriptor::TYPE_INT: { \
							int* valuePtr = static_cast<int*>(fieldPtr); \
							if (ImGui::InputInt(field.name.c_str(), valuePtr)) \
							{ \
							} \
							break; \
						} \
						case FieldDescriptor::TYPE_FLOAT: { \
							float* valuePtr = static_cast<float*>(fieldPtr); \
							if (ImGui::InputFloat(field.name.c_str(), valuePtr)) \
							{ \
							} \
							break; \
						} \
						case FieldDescriptor::TYPE_STRING: { \
							std::string* valuePtr = static_cast<std::string*>(fieldPtr); \
							char buffer[256]; \
							strncpy(buffer, valuePtr->c_str(), sizeof(buffer) - 1); \
							buffer[sizeof(buffer) - 1] = '\0'; \
							if (ImGui::InputText(field.name.c_str(), buffer, sizeof(buffer))) \
							{ \
								*valuePtr = buffer; \
							} \
							break; \
						} \
						case FieldDescriptor::TYPE_BOOL: { \
							bool* valuePtr = static_cast<bool*>(fieldPtr); \
							if (ImGui::Checkbox(field.name.c_str(), valuePtr)) \
							{ \
							} \
							break; \
						} \
					} \
					ImGui::PopID(); \
				} \
				ImGui::PushStyleColor(ImGuiCol_Separator, IM_COL32(255, 255, 255, 100)); \
				ImGui::Separator(); \
				ImGui::PopStyleColor(); \
				ImGui::Unindent(); \
				ImGui::Text("TextureAsset Fields:"); \
				ImGui::Indent(); \
				for (const auto& field : meta->fieldDescriptors) \
				{ \
					if (meta->textureAssetFields.count(field.name) ==  0) continue; \
					char* dataPtr = static_cast<char*>(instance->data); \
					void* fieldPtr = dataPtr + field.offset; \
					ImGui::PushID(field.name.c_str()); \
					switch(field.type) \
					{ \
						case FieldDescriptor::TYPE_INT: { \
							int* valuePtr = static_cast<int*>(fieldPtr); \
							{ \
								ImGui::Button((std::string(field.name.c_str()) + ": " + std::to_string(*valuePtr)).c_str(), ImVec2(250, 0)); \
								if (ImGui::BeginDragDropTarget()) \
								{ \
									if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_TEXTURE")) \
									{ \
										IM_ASSERT(payload->DataSize == sizeof(uint32_t)); \
										uint32_t droppedAssetID = *(const uint32_t*)payload->Data; \
										*valuePtr = droppedAssetID; \
									} \
									ImGui::EndDragDropTarget(); \
								} \
							} \
							break; \
						} \
					} \
					ImGui::PopID(); \
				} \
				ImGui::PushStyleColor(ImGuiCol_Separator, IM_COL32(255, 255, 255, 100)); \
				ImGui::Separator(); \
				ImGui::PopStyleColor(); \
				ImGui::Unindent(); \
				ImGui::Text("AudioAsset Fields:"); \
				ImGui::Text("(Pool Size: %zu)", meta->audioAssetFields.size()); \
				ImGui::Spacing(); \
				ImGui::Indent(10.f); \
				std::vector<FieldDescriptor> sortedFields = meta->fieldDescriptors; \
				std::sort(sortedFields.begin(), sortedFields.end(), [](const FieldDescriptor& a, const FieldDescriptor& b) \
				{ \
					return a.name < b.name; \
				}); \
				for (const auto& field : sortedFields) \
				{ \
					if (meta->audioAssetFields.count(field.name) == 0) continue; \
					char* dataPtr = static_cast<char*>(instance->data); \
					int* valuePtr = static_cast<int*>(static_cast<void*>(dataPtr + field.offset)); \
					\
					ImGui::PushID(field.name.c_str()); \
					\
					ImVec2 cursorPos = ImGui::GetCursorScreenPos(); \
					ImU32 dotColor = (*valuePtr > 0) \
						? IM_COL32(74, 154, 92, 255) \
						: IM_COL32(85, 85, 85, 255); \
					ImGui::GetWindowDrawList()->AddCircleFilled( \
						ImVec2(cursorPos.x - 5.f, cursorPos.y + 9.f), 4.f, dotColor); \
					\
					if (*valuePtr <= 0) \
					{ \
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.f)); \
					} \
					bool nodeOpen = ImGui::TreeNode(field.name.c_str(), "%s", field.name.c_str()); \
					if (*valuePtr <= 0) \
					{ \
						ImGui::PopStyleColor(); \
					} \
					\
					if (nodeOpen) \
					{ \
						if (*valuePtr > 0) \
						{ \
							ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.23f, 0.10f, 1.f)); \
							ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.32f, 0.15f, 1.f)); \
							ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.42f, 0.75f, 0.42f, 1.f)); \
						} \
						ImGui::Button((field.name + "  (" + std::to_string(*valuePtr) + ")").c_str(), ImVec2(-1, 0)); \
						if (*valuePtr > 0) \
						{ \
							ImGui::PopStyleColor(3); \
						} \
						if (ImGui::BeginDragDropTarget()) \
						{ \
							if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_AUDIO")) \
							{ \
								IM_ASSERT(payload->DataSize == sizeof(uint32_t)); \
								*valuePtr = (int)(*(const uint32_t*)payload->Data); \
							} \
							ImGui::EndDragDropTarget(); \
						} \
						\
						if (*valuePtr > 0) \
						{ \
							AudioAssetConfig& cfg = meta->audioAssetConfigs[field.name]; \
							\
							ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.f)); \
							ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 2.f)); \
							ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,  ImVec2(4.f, 3.f)); \
							if (ImGui::BeginTable("##audiocfg", 2, ImGuiTableFlags_None)) \
							{ \
								ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 100.f); \
								ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch); \
								\
								ImGui::TableNextRow(); \
								ImGui::TableSetColumnIndex(0); \
								ImGui::AlignTextToFramePadding(); \
								ImGui::TextUnformatted("Volume"); \
								ImGui::TableSetColumnIndex(1); \
								ImGui::SetNextItemWidth(-1); \
								ImGui::DragFloat("##vol", &cfg.volume, 0.01f, 0.f, 1.f, "%.2f", ImGuiSliderFlags_AlwaysClamp); \
								if (ImGui::BeginPopupContextItem("##vol_ctx")) \
								{ \
									if (ImGui::MenuItem("Reset to default")) { cfg.volume = 1.0f; } \
									ImGui::EndPopup(); \
								} \
								\
								ImGui::TableNextRow(); \
								ImGui::TableSetColumnIndex(0); \
								ImGui::AlignTextToFramePadding(); \
								ImGui::TextUnformatted("Pitch"); \
								ImGui::TableSetColumnIndex(1); \
								ImGui::SetNextItemWidth(-1); \
								ImGui::DragFloat("##pv", &cfg.pitchVariance, 0.01f, 0.5f, 2.f, "%.2f", ImGuiSliderFlags_AlwaysClamp); \
								if (ImGui::BeginPopupContextItem("##pv_ctx")) \
								{ \
									if (ImGui::MenuItem("Reset to default")) { cfg.pitchVariance = 1.0f; } \
									ImGui::EndPopup(); \
								} \
								\
								ImGui::TableNextRow(); \
								ImGui::TableSetColumnIndex(0); \
								ImGui::AlignTextToFramePadding(); \
								ImGui::TextUnformatted("Flags"); \
								ImGui::TableSetColumnIndex(1); \
								ImGui::SetNextItemWidth(-1); \
								ImGui::Checkbox("Loop", &cfg.loop); \
								ImGui::SameLine(); \
								ImGui::Checkbox("One Shot", &cfg.oneShot); \
								\
								ImGui::EndTable(); \
							} \
							ImGui::PopStyleVar(3); \
						} \
						\
						ImGui::TreePop(); \
					} \
					\
					ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.25f, 0.25f, 0.25f, 1.f)); \
					ImGui::Separator(); \
					ImGui::PopStyleColor(); \
					\
					ImGui::PopID(); \
				} \
				ImGui::Unindent(10.f); \
				if (!meta->compositionFields.empty()) \
				{ \
					ImGui::Text("Composition Fields:"); \
					ImGui::Indent(); \
					lua_State* compL = GetEngineInstance().GetLuaVM()->GetState(); \
					RenderCompositionFields(meta, instance, scriptSystem, compL); \
					ImGui::Unindent(); \
				} \
			} \
		} \
	}

#define GENERATE_SCRIPT_INSPECTOR_WITH_REMOVAL(ComponentType, FieldList) \
    inline ComponentInspectorResult InspectComponent##ComponentType(ComponentType& component) \
	{ \
		ImGui::PushID(#ComponentType); \
		float buttonWidth = ImGui::CalcTextSize("Remove").x + ImGui::GetStyle().FramePadding.x * 2.0f; \
		float headerWidth = ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x; \
		ImGui::SetNextItemAllowOverlap(); \
		ImGui::SetNextItemWidth(headerWidth); \
		bool headerOpen = ImGui::CollapsingHeader(#ComponentType, ImGuiTreeNodeFlags_DefaultOpen); \
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x); \
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); \
		bool removeClicked = ImGui::SmallButton("Remove"); \
		ImGui::PopStyleColor(); \
		\
		ComponentInspectorResult result = ComponentInspectorResult::NoChange; \
		if (removeClicked) \
		{ \
			ImGui::PopID(); \
			return ComponentInspectorResult::RemoveRequested; \
		} \
		else if (headerOpen) \
		{ \
			ImGui::Indent(); \
			INSPECTOR_BEGIN_MODIFIED_TRACKING \
			FieldList(INSPECTOR_FIELD) \
			ImGui::Unindent(); \
			if (modified) result = ComponentInspectorResult::Modified; \
		} \
		ImGui::PopID(); \
		return result; \
	}

GENERATE_SCRIPT_INSPECTOR_WITH_REMOVAL(ScriptComponent, SCRIPT_INSPECTOR_FIELDS)

#pragma endregion

/*************************************************************************
  \brief 
	Constructor for InspectorNode.
  \details 
	Sets up lambda callback handles for initialization, updating,
	and docking.
*************************************************************************/
InspectorNode::InspectorNode()
{
	initCallbackHandle = [this]() -> void { this->Init(); };
	updateCallbackHandle = [this]() -> void { this->Update(); };
	dockBuildCallbackHandle = [this]() -> void { this->DockBuild(); };
}

/*************************************************************************
  \brief 
	Initializes the InspectorNode.
  \details 
	Registers an entity selected callback with the EditorEventBus.
*************************************************************************/
void InspectorNode::Init() {
	entitySelectedCallback =
		[&](Entity entity)
		{
			SetSelectedEntity(entity);
		};
	EditorEventBus::onEntitySelected += entitySelectedCallback;
}

/*************************************************************************
  \brief 
	Builds the docking layout for the Inspector window.
*************************************************************************/
void InspectorNode::DockBuild() {
	ImGui::DockBuilderDockWindow("Inspector", Systems::GetSystemsRegistry().GetSystem<Systems::EditorManager>().dockRight);
}

/*************************************************************************
  \brief 
	Displays the overrides panel for a selected entity.
  \param 
	entityName Name of the currently selected entity.
  \param 
	sceneName Name of the scene containing the entity.
*************************************************************************/
void InspectorNode::DisplayOverrideOptions(const char* entityName, std::string& sceneName) {
	// overrides panel
	static bool overridesOpen = true;
	overridesOpen = ImGui::CollapsingHeader("Overrides", ImGuiTreeNodeFlags_DefaultOpen);
	if (overridesOpen) {
		ImGui::BeginChild("OverridesPanel", ImVec2(0, 225), true, ImGuiWindowFlags_MenuBar);
		if (ImGui::BeginMenuBar()) {
			ImGui::TextDisabled("Overrides");
			ImGui::EndMenuBar();
		}

		ImGui::Spacing();
		ImGui::Text("Review, Revert or Apply Overrides");
		ImGui::Spacing();
		ImGui::Text("on");
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), entityName);
		ImGui::Text("in");
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), sceneName.c_str());
		ImGui::Spacing();
		ImGui::Separator();

		// entity foldout
		static bool entityFoldout = true;
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.25f, 0.25f, 1.0f)); // grey
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.45f, 0.45f, 0.45f, 1.0f));
		entityFoldout = ImGui::CollapsingHeader(entityName, ImGuiTreeNodeFlags_DefaultOpen);
		if (entityFoldout) {
			ImGui::Indent(15);
			ImGui::Spacing();
			// placeholders ---------------------------
			ImGui::Text("Box Collider");
			ImGui::Text("Health (Script)");
			ImGui::Text("Scene Object Guid (Script)");
			ImGui::Text("Banana");
			// ----------------------------------------
			ImGui::Spacing();
			ImGui::Unindent(15);
		}
		ImGui::PopStyleColor(3);
		ImGui::Separator();

		// action buttons
		float buttonWidth = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.25f, 1.0f)); // grey
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.45f, 0.45f, 1.0f));

		if (ImGui::Button("Revert All", ImVec2(buttonWidth, 0))) {

			// revert logic code...

		}

		ImGui::SameLine();
		if (ImGui::Button("Apply All", ImVec2(buttonWidth, 0))) {

			// apply logic code...

		}

		ImGui::PopStyleColor(3);

		ImGui::EndChild();
	}
}

/*************************************************************************
  \brief 
	Displays transform editing options for the selected entity.
  \param 
	world Reference to the current World.
  \param 
	objectsFactory Reference to the object factory.
  \param 
	posX Reference to the entity's X position.
  \param 
	posY Reference to the entity's Y position.
  \param 
	rot Reference to the entity's rotation.
*************************************************************************/
void InspectorNode::DisplayTransformOptions(World& world [[maybe_unused]], ObjectsFactory& objectsFactory [[maybe_unused]], float& posX, float& posY, float& rot) {
	static bool transformOpen = true;

	transformOpen = ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen);
	if (transformOpen) {
		ImGui::BeginChild("TransformPanel", ImVec2(0, 75), true);
		ImGui::Spacing();

		if (ImGui::BeginTable("TransformTable", 2, ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 80.f);
			ImGui::TableSetupColumn("Values");

			// position row
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Position");

			ImGui::TableSetColumnIndex(1);
			{
				float totalWidth = ImGui::GetContentRegionAvail().x;
				float spacing = ImGui::GetStyle().ItemSpacing.x;
				float labelWidth = ImGui::CalcTextSize("X").x + ImGui::CalcTextSize("Y").x + spacing * 4;
				float inputWidth = (totalWidth - labelWidth) * 0.5f;

				ImGui::Text("X");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(inputWidth);
				ImGui::InputFloat("##PosX", &posX, 0, 0, "%.2f");

				ImGui::SameLine();
				ImGui::Text("Y");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(inputWidth);
				ImGui::InputFloat("##PosY", &posY, 0, 0, "%.2f");
			}

			ImGui::Spacing();

			// rotation row
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Rotation");

			ImGui::TableSetColumnIndex(1);
			{
				float totalWidth = ImGui::GetContentRegionAvail().x;
				float spacing = ImGui::GetStyle().ItemSpacing.x;
				float labelWidth = ImGui::CalcTextSize("Z").x + spacing * 2;
				float inputWidth = totalWidth - labelWidth;

				ImGui::Text(" ");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(inputWidth);
				ImGui::InputFloat("##RotZ", &rot, 0, 0, "%.2f");
			}

			ImGui::EndTable();
		}

		ImGui::EndChild();
	}
}

/*************************************************************************
  \brief 
	Updates the Inspector window each frame.
  \details 
	Handles selection, input detection, and renders components
	inspector or add-component popup.
*************************************************************************/
void InspectorNode::Update() {
	ImGui::SetNextWindowSize(ImVec2(800, 700), ImGuiCond_Always);

	if (ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
	{
		if (selectedEntity.IsValid() && GGEngine::GetEngine().GetSceneManager().GetActiveScene().IsEntityAlive(selectedEntity))
		{
			RenderEntityInspector();
			RenderAddComponent();
		}
		else
		{
			RenderNoEntitySelected();
		}

	}

	ImGui::End();
}

/*************************************************************************
  \brief 
	Renders all components of the currently selected entity.
  \details 
	Handles modification tracking, undo/redo, and removal of
	components.
*************************************************************************/
#pragma region RenderEntityInspector() with modifying components
void InspectorNode::RenderEntityInspector()
{
	auto& prefabManager = GGEngine::GetEngine().GetPrefabManager();
	auto& objectsFactory = GGEngine::GetEngine().GetObjectsFactory();
	auto& sceneManager = GGEngine::GetEngine().GetSceneManager();
	auto& world = sceneManager.GetActiveScene();

	ImGui::Text("Entity ID: %u", selectedEntity.id);
	ImGui::Text("Generation: %u", selectedEntity.generation);

	ImGui::Separator();

	// Scope all widget IDs to the selected entity so ImGui flushes
	// its internal edit buffer when switching between entities
	ImGui::PushID(static_cast<int>(selectedEntity.id));

	// for linking to prefab inspector
	char prefabName[64] = "No Prefabs Found"; // placeholder
	if (world.HasComponent<PrefabInstance>(selectedEntity)) {

		PrefabInstance* prefabInst = world.GetComponent<PrefabInstance>(selectedEntity);
		if (prefabInst && prefabManager.HasPrefab(prefabInst->GetPrefabInstanceName())) {

			strncpy(prefabName, (prefabInst->GetPrefabInstanceName()).c_str(), sizeof(prefabName) - 1);
			prefabName[sizeof(prefabName) - 1] = '\0'; // ensure null-termination

			if (ImGui::CollapsingHeader("Prefab Options", ImGuiTreeNodeFlags_DefaultOpen)) {

				ImGui::Spacing();
				ImGui::Text("A prefab ");
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.0f, 0.7f, 1.0f, 1.0f), prefabName);
				ImGui::SameLine();
				ImGui::Text(" has been found");
				ImGui::Spacing();

				if (ImGui::Button("Edit Prefab")) {
					sceneManager.SetActiveScene("PrefabEditorWorld");
					auto& activeWorld = sceneManager.GetActiveScene();
					// clear the previous prefab editing scene first
					objectsFactory.DestroyAllSpawnedObjects(activeWorld);
					objectsFactory.InstantiateObjects(prefabInst->GetPrefabInstanceName(), prefabManager, activeWorld, 1);
				}
			}
		}
	}

	ImGui::Separator();

	if (world.HasComponent<NameComponent>(selectedEntity))
	{
		NameComponent* name = world.GetComponent<NameComponent>(selectedEntity);
		if (name)
		{
			static NameComponent beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			if (isEditing && editingEntity != selectedEntity)
			{
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			NameComponent componentBeforeFrame = *name;
			auto result = InspectComponentNameComponent(*name);

			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState = componentBeforeFrame;
					isEditing = true;
					editingEntity = selectedEntity;
				}
			}

			if (isEditing && !ImGui::IsAnyItemActive())
			{
				PushModifyComponentCommand(selectedEntity, beforeState, *name);
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			// removal of name component is prohibited
			/*if (result == ComponentInspectorResult::RemoveRequested)
			{
				PushRemoveComponentCommand<NameComponent>(selectedEntity, world);
				world.RemoveComponent<NameComponent>(selectedEntity);
			}*/
		}
	}

	if (world.HasComponent<LocalTransform>(selectedEntity))
	{
		LocalTransform* transform = world.GetComponent<LocalTransform>(selectedEntity);
		if (transform)
		{
			static TransformSnapshot beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			if (isEditing && editingEntity != selectedEntity) {
				isEditing = false;
				editingEntity = NULL_ENTITY;
				beforeState = TransformSnapshot{};
			}

			LocalTransform transformBeforeEditingThisFrame = *transform;
			auto result = InspectComponentLocalTransform(*transform);

			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState.position = transformBeforeEditingThisFrame.position;
					beforeState.scale = transformBeforeEditingThisFrame.scale;
					beforeState.rotation = transformBeforeEditingThisFrame.rotation;
					isEditing = true;
					editingEntity = selectedEntity;
				}
				transform->isDirty = true;
			}

			if (isEditing && !ImGui::IsAnyItemActive())
			{
				TransformSnapshot afterState;
				afterState.position = transform->position;
				afterState.scale = transform->scale;
				afterState.rotation = transform->rotation;

				if (beforeState != afterState) {
					auto cmd = std::make_unique<TransformCommand>(selectedEntity, beforeState, afterState);
					GetUndoStack()->Push(std::move(cmd));
				}

				isEditing = false;
				editingEntity = NULL_ENTITY;
				beforeState = TransformSnapshot{};
			}
		}
	}

	if (world.HasComponent<AABBFloat>(selectedEntity))
	{
		// AABBFloat
		AABBFloat* aabb = world.GetComponent<AABBFloat>(selectedEntity);
		if (aabb)
		{
			//static variables for tracking edits
			static AABBFloat beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			// reset if switched to different entity
			if (isEditing && editingEntity != selectedEntity)
			{
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			// save state before inspection
			AABBFloat componentBeforeFrame = *aabb;
			auto result = InspectComponentAABBFloat(*aabb);

			// if user started editing, save the "before" state
			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState = componentBeforeFrame;
					isEditing = true;
					editingEntity = selectedEntity;
				}
			}

			// If user finished editing (released mouse/ keyboard), create undo command
			if (isEditing && !ImGui::IsAnyItemActive())
			{
				PushModifyComponentCommand(selectedEntity, beforeState, *aabb);
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			// if user clicked "Remove", save state and remove
			if (result == ComponentInspectorResult::RemoveRequested)
			{
				PushRemoveComponentCommand<AABBFloat>(selectedEntity, world);
				world.RemoveComponent<AABBFloat>(selectedEntity);
			}
		}
	}

	// ScreenBounds
	if (world.HasComponent<ScreenBounds>(selectedEntity))
	{
		ScreenBounds* screenBounds = world.GetComponent<ScreenBounds>(selectedEntity);
		if (screenBounds)
		{
			static ScreenBounds beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			if (isEditing && editingEntity != selectedEntity) {
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			ScreenBounds componentBeforeFrame = *screenBounds;
			auto result = InspectComponentScreenBounds(*screenBounds);

			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState = componentBeforeFrame;
					isEditing = true;
					editingEntity = selectedEntity;
				}
			}

			if (isEditing && !ImGui::IsAnyItemActive())
			{
				PushModifyComponentCommand(selectedEntity, beforeState, *screenBounds);
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				PushRemoveComponentCommand<ScreenBounds>(selectedEntity, world);
				world.RemoveComponent<ScreenBounds>(selectedEntity);
			}
		}
	}

	// Rigidbody2DFloat
	if (world.HasComponent<Rigidbody2DFloat>(selectedEntity))
	{
		Rigidbody2DFloat* rigidBody = world.GetComponent<Rigidbody2DFloat>(selectedEntity);
		if (rigidBody)
		{
			static Rigidbody2DFloat beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			if (isEditing && editingEntity != selectedEntity)
			{
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			Rigidbody2DFloat componentBeforeFrame = *rigidBody;
			auto result = InspectComponentRigidbody2DFloat(*rigidBody);

			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState = componentBeforeFrame;
					isEditing = true;
					editingEntity = selectedEntity;
				}
			}

			if (isEditing && !ImGui::IsAnyItemActive())
			{
				PushModifyComponentCommand(selectedEntity, beforeState, *rigidBody);
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				PushRemoveComponentCommand<Rigidbody2DFloat>(selectedEntity, world);
				world.RemoveComponent<Rigidbody2DFloat>(selectedEntity);
			}
		}
	}

	// MeshRenderer
	if (world.HasComponent<MeshRenderer>(selectedEntity))
	{
		MeshRenderer* meshRenderer = world.GetComponent<MeshRenderer>(selectedEntity);
		if (meshRenderer)
		{
			static MeshRenderer beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			if (isEditing && editingEntity != selectedEntity)
			{
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			MeshRenderer componentBeforeFrame = *meshRenderer;
			auto result = InspectComponentMeshRenderer(*meshRenderer);

			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState = componentBeforeFrame;
					isEditing = true;
					editingEntity = selectedEntity;
				}
			}

			if (isEditing && !ImGui::IsAnyItemActive())
			{
				PushModifyComponentCommand(selectedEntity, beforeState, *meshRenderer);
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				PushRemoveComponentCommand<MeshRenderer>(selectedEntity, world);
				world.RemoveComponent<MeshRenderer>(selectedEntity);
			}
		}
	}

	// UIComponent
	if (world.HasComponent<UIComponent>(selectedEntity))
	{
		UIComponent* uiComponent = world.GetComponent<UIComponent>(selectedEntity);
		if (uiComponent)
		{
			static UIComponent beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			if (isEditing && editingEntity != selectedEntity)
			{
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			UIComponent componentBeforeFrame = *uiComponent;
			auto result = InspectComponentUIComponent(*uiComponent);

			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState = componentBeforeFrame;
					isEditing = true;
					editingEntity = selectedEntity;
				}
			}

			if (isEditing && !ImGui::IsAnyItemActive())
			{
				PushModifyComponentCommand(selectedEntity, beforeState, *uiComponent);
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				PushRemoveComponentCommand<UIComponent>(selectedEntity, world);
				world.RemoveComponent<UIComponent>(selectedEntity);
			}
		}
	}

	// Sprite
	if (world.HasComponent<Sprite>(selectedEntity))
	{
		Sprite* sprite = world.GetComponent<Sprite>(selectedEntity);
		if (sprite)
		{
			static Sprite beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			if (isEditing && editingEntity != selectedEntity)
			{
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			Sprite componentBeforeFrame = *sprite;
			auto result = InspectComponentSprite(*sprite);

			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState = componentBeforeFrame;
					isEditing = true;
					editingEntity = selectedEntity;
				}
			}

			if (isEditing && !ImGui::IsAnyItemActive())
			{
				PushModifyComponentCommand(selectedEntity, beforeState, *sprite);
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				PushRemoveComponentCommand<Sprite>(selectedEntity, world);
				world.RemoveComponent<Sprite>(selectedEntity);
			}
		}
	}

	// SpriteAnimator
	if (world.HasComponent<SpriteAnimator>(selectedEntity))
	{
		SpriteAnimator* spriteAnimator = world.GetComponent<SpriteAnimator>(selectedEntity);
		if (spriteAnimator)
		{
			static SpriteAnimator beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			if (isEditing && editingEntity != selectedEntity)
			{
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			SpriteAnimator componentBeforeFrame = *spriteAnimator;
			auto result = InspectComponentSpriteAnimator(*spriteAnimator);

			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState = componentBeforeFrame;
					isEditing = true;
					editingEntity = selectedEntity;
				}
			}

			if (isEditing && !ImGui::IsAnyItemActive())
			{
				PushModifyComponentCommand(selectedEntity, beforeState, *spriteAnimator);
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				PushRemoveComponentCommand<SpriteAnimator>(selectedEntity, world);
				world.RemoveComponent<SpriteAnimator>(selectedEntity);
			}
		}
	}

	// ECSGridComponent
	if (world.HasComponent<ECSGridComponent>(selectedEntity))
	{
		ECSGridComponent* grid = world.GetComponent<ECSGridComponent>(selectedEntity);
		if (grid)
		{
			static ECSGridComponent beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			if (isEditing && editingEntity != selectedEntity)
			{
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			ECSGridComponent componentBeforeFrame = *grid;
			auto result = InspectComponentECSGridComponent(*grid);

			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState = componentBeforeFrame;
					isEditing = true;
					editingEntity = selectedEntity;
				}
			}

			if (isEditing && !ImGui::IsAnyItemActive())
			{
				PushModifyComponentCommand(selectedEntity, beforeState, *grid);
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				PushRemoveComponentCommand<ECSGridComponent>(selectedEntity, world);
				Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSGrid>().GetGridManager()->DestroyGrid(grid->gridID);
				world.RemoveComponent<ECSGridComponent>(selectedEntity);
			}
		}
	}

	// AudioSourceComponent
	if (world.HasComponent<AudioSource>(selectedEntity))
	{
		AudioSource* audioSource = world.GetComponent<AudioSource>(selectedEntity);
		if (audioSource)
		{
			static AudioSource beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			// reset if switched to different entity
			if (isEditing && editingEntity != selectedEntity)
			{
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			// save state before inspection
			AudioSource componentBeforeFrame = *audioSource;
			auto result = InspectComponentAudioSource(*audioSource);

			// user started editing this frame
			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState = componentBeforeFrame;
					isEditing = true;
					editingEntity = selectedEntity;
				}
			}

			// user finished editing (no ImGui item active)
			if (isEditing && !ImGui::IsAnyItemActive())
			{
				PushModifyComponentCommand(selectedEntity, beforeState, *audioSource);
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			// user clicked Remove
			if (result == ComponentInspectorResult::RemoveRequested)
			{
				PushRemoveComponentCommand<AudioSource>(selectedEntity, world);
				world.RemoveComponent<AudioSource>(selectedEntity);
			}
		}
	}

	// TilemapComponent
	if (world.HasComponent<TilemapComponent>(selectedEntity))
	{
		TilemapComponent* tilemap = world.GetComponent<TilemapComponent>(selectedEntity);
		if (tilemap)
		{
			static TilemapComponent beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			if (isEditing && editingEntity != selectedEntity)
			{
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			TilemapComponent componentBeforeFrame = *tilemap;
			auto result = InspectComponentTilemapComponent(*tilemap);

			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState = componentBeforeFrame;
					isEditing = true;
					editingEntity = selectedEntity;
				}
			}

			if (isEditing && !ImGui::IsAnyItemActive())
			{
				PushModifyComponentCommand(selectedEntity, beforeState, *tilemap);
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				PushRemoveComponentCommand<TilemapComponent>(selectedEntity, world);
				world.RemoveComponent<TilemapComponent>(selectedEntity);
			}
		}
	}

	// ScriptComponenet
	if (world.HasComponent<ScriptComponent>(selectedEntity))
	{
		ScriptComponent* script = world.GetComponent<ScriptComponent>(selectedEntity);
		if (script)
		{
			static ScriptComponent beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			if (isEditing && editingEntity != selectedEntity)
			{
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			ScriptComponent componentBeforeFrame = *script;
			auto result = InspectComponentScriptComponent(*script);

			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState = componentBeforeFrame;
					isEditing = true;
					editingEntity = selectedEntity;
				}
				script->scriptInstance = Systems::GetECSSystemsRegistry()
					.GetSystem<Systems::ECSScript>().CreateScriptInstance(world, selectedEntity);
			}

			if (isEditing && !ImGui::IsAnyItemActive())
			{
				PushModifyComponentCommand(selectedEntity, beforeState, *script);
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				PushRemoveComponentCommand<ScriptComponent>(selectedEntity, world);
				world.RemoveComponent<ScriptComponent>(selectedEntity);
			}
		}
	}


	// TextComponent
	if (world.HasComponent<TextComponent>(selectedEntity))
	{
		TextComponent* textComp = world.GetComponent<TextComponent>(selectedEntity);
		if (textComp)
		{
			static TextComponent beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			// Reset if switched to different entity
			if (isEditing && editingEntity != selectedEntity)
			{
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			// Save state before inspection
			TextComponent componentBeforeFrame = *textComp;
			auto result = InspectComponentTextComponent(*textComp);

			// User started editing this frame
			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState = componentBeforeFrame;
					isEditing = true;
					editingEntity = selectedEntity;
				}
			}

			// User finished editing (no ImGui item active)
			if (isEditing && !ImGui::IsAnyItemActive())
			{
				PushModifyComponentCommand(selectedEntity, beforeState, *textComp);
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			// User clicked Remove
			if (result == ComponentInspectorResult::RemoveRequested)
			{
				PushRemoveComponentCommand<TextComponent>(selectedEntity, world);
				world.RemoveComponent<TextComponent>(selectedEntity);
			}
		}
	}

	//world text comp
	if(world.HasComponent<WorldTextComponent>(selectedEntity))
	{
		WorldTextComponent* worldTextComp = world.GetComponent<WorldTextComponent>(selectedEntity);
		if (worldTextComp)
		{
			static WorldTextComponent beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			// Reset if switched to different entity
			if (isEditing && editingEntity != selectedEntity)
			{
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			// Save state before inspection
			WorldTextComponent componentBeforeFrame = *worldTextComp;
			auto result = InspectComponentWorldTextComponent(*worldTextComp);

			// User started editing this frame
			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState = componentBeforeFrame;
					isEditing = true;
					editingEntity = selectedEntity;
				}
			}

			// User finished editing (no ImGui item active)
			if (isEditing && !ImGui::IsAnyItemActive())
			{
				PushModifyComponentCommand(selectedEntity, beforeState, *worldTextComp);
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			// User clicked Remove
			if (result == ComponentInspectorResult::RemoveRequested)
			{
				PushRemoveComponentCommand<WorldTextComponent>(selectedEntity, world);
				world.RemoveComponent<WorldTextComponent>(selectedEntity);
			}
		}
	}


	//objectEffect comp
	if (world.HasComponent<ObjectEffectComponent>(selectedEntity))
	{
		ObjectEffectComponent* worldTextComp = world.GetComponent<ObjectEffectComponent>(selectedEntity);
		if (worldTextComp)
		{
			static ObjectEffectComponent beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			// Reset if switched to different entity
			if (isEditing && editingEntity != selectedEntity)
			{
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			// Save state before inspection
			ObjectEffectComponent componentBeforeFrame = *worldTextComp;
			auto result = InspectComponentObjectEffectComponent(*worldTextComp);

			// User started editing this frame
			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState = componentBeforeFrame;
					isEditing = true;
					editingEntity = selectedEntity;
				}
			}

			// User finished editing (no ImGui item active)
			if (isEditing && !ImGui::IsAnyItemActive())
			{
				PushModifyComponentCommand(selectedEntity, beforeState, *worldTextComp);
				isEditing = false;
				editingEntity = NULL_ENTITY;
			}

			// User clicked Remove
			if (result == ComponentInspectorResult::RemoveRequested)
			{
				PushRemoveComponentCommand<ObjectEffectComponent>(selectedEntity, world);
				world.RemoveComponent<ObjectEffectComponent>(selectedEntity);
			}
		}
	}

	ImGui::PopID();
} // end InspectorNode::RenderEntityInspector()
#pragma endregion

#pragma region Old RenderEntityInspector()
#if 0
void InspectorNode::RenderEntityInspector()
{
	auto& prefabManager = GGEngine::GetEngine().GetPrefabManager();
	auto& objectsFactory = GGEngine::GetEngine().GetObjectsFactory();
	auto& sceneManager = GGEngine::GetEngine().GetSceneManager();
	auto& world = sceneManager.GetActiveScene();

	ImGui::Text("Entity ID: %u", selectedEntity.id);
	ImGui::Text("Generation: %u", selectedEntity.generation);

	ImGui::Separator();

	// for linking to prefab inspector
	char prefabName[64] = "No Prefabs Found"; // placeholder
	if (world.HasComponent<PrefabInstance>(selectedEntity)) {

		PrefabInstance* prefabInst = world.GetComponent<PrefabInstance>(selectedEntity);
		if (prefabInst && prefabManager.HasPrefab(prefabInst->GetPrefabInstanceName())) {

			strncpy(prefabName, (prefabInst->GetPrefabInstanceName()).c_str(), sizeof(prefabName) - 1);
			prefabName[sizeof(prefabName) - 1] = '\0'; // ensure null-termination

			if (ImGui::CollapsingHeader("Prefab Options", ImGuiTreeNodeFlags_DefaultOpen)) {

				ImGui::Spacing();
				ImGui::Text("A prefab ");
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.0f, 0.7f, 1.0f, 1.0f), prefabName);
				ImGui::SameLine();
				ImGui::Text(" has been found");
				ImGui::Spacing();

				if (ImGui::Button("Edit Prefab")) {
					// open prefab browser

					sceneManager.SetActiveScene("PrefabEditorScene");
					auto& activeWorld = sceneManager.GetActiveScene();
					// clear the previous prefab editing scene first
					objectsFactory.DestroyAllSpawnedObjects(activeWorld);
					objectsFactory.InstantiateObjects(prefabInst->GetPrefabInstanceName(), prefabManager, activeWorld, 1);
				}
			}
		}
	}

	ImGui::Separator();

	if (world.HasComponent<NameComponent>(selectedEntity))
	{
		NameComponent* name = world.GetComponent<NameComponent>(selectedEntity);
		if (name)
		{
			[[maybe_unused]] auto result = InspectComponentNameComponent(*name);
		}
	}

	if (world.HasComponent<LocalTransform>(selectedEntity))
	{
		LocalTransform* transform = world.GetComponent<LocalTransform>(selectedEntity);
		if (transform)
		{
			static TransformSnapshot beforeState{};
			static bool isEditing = false;
			static Entity editingEntity = NULL_ENTITY;

			if (isEditing && editingEntity != selectedEntity) {
				isEditing = false;
				editingEntity = NULL_ENTITY;
				beforeState = TransformSnapshot{};
			}

			LocalTransform transformBeforeEditingThisFrame = *transform;
			auto result = InspectComponentLocalTransform(*transform);

			if (result == ComponentInspectorResult::Modified)
			{
				if (!isEditing || editingEntity != selectedEntity)
				{
					beforeState.position = transformBeforeEditingThisFrame.position;
					beforeState.scale = transformBeforeEditingThisFrame.scale;
					beforeState.rotation = transformBeforeEditingThisFrame.rotation;
					isEditing = true;
					editingEntity = selectedEntity;
				}
				transform->isDirty = true;
			}

			if (isEditing && !ImGui::IsAnyItemActive())
			{
				TransformSnapshot afterState;
				afterState.position = transform->position;
				afterState.scale = transform->scale;
				afterState.rotation = transform->rotation;

				if (beforeState != afterState) {
					auto cmd = std::make_unique<TransformCommand>(selectedEntity, beforeState, afterState);
					GetUndoStack()->Push(std::move(cmd));
				}

				isEditing = false;
				editingEntity = NULL_ENTITY;
				beforeState = TransformSnapshot{};
			}
		}
	}

	if (world.HasComponent<AABBFloat>(selectedEntity))
	{
		AABBFloat* aabb = world.GetComponent<AABBFloat>(selectedEntity);
		if (aabb)
		{
			auto result = InspectComponentAABBFloat(*aabb);

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				world.RemoveComponent<AABBFloat>(selectedEntity);
			}
		}
	}
	

	if (world.HasComponent<ScreenBounds>(selectedEntity))
	{
		ScreenBounds* screenBounds = world.GetComponent<ScreenBounds>(selectedEntity);
		if (screenBounds)
		{
			auto result = InspectComponentScreenBounds(*screenBounds);

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				world.RemoveComponent<ScreenBounds>(selectedEntity);
			}
		}
	}

	if (world.HasComponent<Rigidbody2DFloat>(selectedEntity))
	{
		Rigidbody2DFloat* rigidBody = world.GetComponent<Rigidbody2DFloat>(selectedEntity);
		if (rigidBody)
		{
			auto result = InspectComponentRigidbody2DFloat(*rigidBody);

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				world.RemoveComponent<Rigidbody2DFloat>(selectedEntity);
			}
		}
	}

	if (world.HasComponent<MeshRenderer>(selectedEntity))
	{
		MeshRenderer* meshRenderer = world.GetComponent<MeshRenderer>(selectedEntity);
		if (meshRenderer)
		{
			auto result = InspectComponentMeshRenderer(*meshRenderer);

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				world.RemoveComponent<MeshRenderer>(selectedEntity);
			}
		}
	}

	if (world.HasComponent<Sprite>(selectedEntity))
	{
		Sprite* sprite = world.GetComponent<Sprite>(selectedEntity);
		if (sprite)
		{
			auto result = InspectComponentSprite(*sprite);

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				world.RemoveComponent<Sprite>(selectedEntity);
			}
		}
	}

	if (world.HasComponent<SpriteAnimator>(selectedEntity))
	{
		SpriteAnimator* spriteAnimator = world.GetComponent<SpriteAnimator>(selectedEntity);
		if (spriteAnimator)
		{
			auto result = InspectComponentSpriteAnimator(*spriteAnimator);

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				world.RemoveComponent<SpriteAnimator>(selectedEntity);
			}
		}
	}

	if (world.HasComponent<ECSGridComponent>(selectedEntity))
	{
		ECSGridComponent* grid = world.GetComponent<ECSGridComponent>(selectedEntity);
		if (grid)
		{
			auto result = InspectComponentECSGridComponent(*grid);

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSGrid>().GetGridManager()->DestroyGrid(grid->gridID);
				world.RemoveComponent<ECSGridComponent>(selectedEntity);
			}

			else if (result == ComponentInspectorResult::Modified)
			{
				
			}
		}
	}

	if (world.HasComponent<TilemapComponent>(selectedEntity))
	{
		TilemapComponent* tilemap = world.GetComponent<TilemapComponent>(selectedEntity);
		if (tilemap)
		{
			auto result = InspectComponentTilemapComponent(*tilemap);

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				world.RemoveComponent<TilemapComponent>(selectedEntity);
			}

			else if (result == ComponentInspectorResult::Modified)
			{
				// TODO: Grid size change
			}
		}
	}

	if (world.HasComponent<ScriptComponent>(selectedEntity))
	{
		ScriptComponent* script = world.GetComponent<ScriptComponent>(selectedEntity);
		if (script)
		{
			auto result = InspectComponentScriptComponent(*script);

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				world.RemoveComponent<ScriptComponent>(selectedEntity);
			}

			if (result == ComponentInspectorResult::Modified)
			{
				script->scriptInstance = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>().CreateScriptInstance(world, selectedEntity);
			}
		}
	}

	if (world.HasComponent<AudioSource>(selectedEntity))
	{
		AudioSource* audioSource = world.GetComponent<AudioSource>(selectedEntity);
		if (audioSource)
		{
			auto result = InspectComponentAudioSource(*audioSource);

			if (result == ComponentInspectorResult::RemoveRequested)
			{
				world.RemoveComponent<AudioSource>(selectedEntity);
			}
		}

	}
}
#endif
#pragma endregion

/*************************************************************************
  \brief 
	Renders a placeholder UI when no entity is selected.
*************************************************************************/
void InspectorNode::RenderNoEntitySelected()
{

}

/*************************************************************************
  \brief 
	Renders the Add Component popup.
  \details 
	Allows the user to add components to the selected entity.
*************************************************************************/
void InspectorNode::RenderAddComponent()
{
    ImGui::Separator();

    if (ImGui::Button("Add Component", ImVec2(-1, 0))) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::Text("Add Component:");
        ImGui::Separator();

		World& activeWorld = GGEngine::GetEngine().GetActiveWorld();

		if (!activeWorld.HasComponent<NameComponent>(selectedEntity))
		{
			if (ImGui::MenuItem("Name"))
			{
				activeWorld.AddComponent<NameComponent>(selectedEntity, NameComponent());
				PushAddComponentCommand(selectedEntity, NameComponent());
				ImGui::CloseCurrentPopup();
			}
		}

		if (!activeWorld.HasComponent<AABBFloat>(selectedEntity))
		{
			if (ImGui::MenuItem("AABB Collider"))
			{
				activeWorld.AddComponent<AABBFloat>(selectedEntity, AABBFloat());
				PushAddComponentCommand(selectedEntity, AABBFloat());
				ImGui::CloseCurrentPopup();
			}
		}

		if (!activeWorld.HasComponent<Rigidbody2DFloat>(selectedEntity))
		{
			if (ImGui::MenuItem("Rigidbody"))
			{
				activeWorld.AddComponent<Rigidbody2DFloat>(selectedEntity, Rigidbody2DFloat());
				PushAddComponentCommand(selectedEntity, Rigidbody2DFloat());
				ImGui::CloseCurrentPopup();
			}
		}

		if (!activeWorld.HasComponent<MeshRenderer>(selectedEntity))
		{
			if (ImGui::MenuItem("Mesh Renderer"))
			{
				activeWorld.AddComponent<MeshRenderer>(selectedEntity, MeshRenderer());
				PushAddComponentCommand(selectedEntity, MeshRenderer());
				ImGui::CloseCurrentPopup();
			}
		}

		if (!activeWorld.HasComponent<UIComponent>(selectedEntity))
		{
			if (ImGui::MenuItem("UI Component"))
			{
				activeWorld.AddComponent<UIComponent>(selectedEntity, UIComponent());
				PushAddComponentCommand(selectedEntity, UIComponent());
				ImGui::CloseCurrentPopup();
			}
		}

		if (!activeWorld.HasComponent<Sprite>(selectedEntity))
		{
			if (ImGui::MenuItem("Sprite Renderer"))
			{
				activeWorld.AddComponent<Sprite>(selectedEntity, Sprite());
				PushAddComponentCommand(selectedEntity, Sprite());
				ImGui::CloseCurrentPopup();
			}
		}

		if (!activeWorld.HasComponent<SpriteAnimator>(selectedEntity))
		{
			if (ImGui::MenuItem("Sprite Animator"))
			{
				activeWorld.AddComponent<SpriteAnimator>(selectedEntity, SpriteAnimator());
				PushAddComponentCommand(selectedEntity, SpriteAnimator());
				ImGui::CloseCurrentPopup();
			}
		}

		if (!activeWorld.HasComponent<ECSGridComponent>(selectedEntity))
		{
			if (ImGui::MenuItem("Grid Component"))
			{
				activeWorld.AddComponent<ECSGridComponent>(selectedEntity, ECSGridComponent());
				ECSGridComponent* grid = activeWorld.GetComponent<ECSGridComponent>(selectedEntity);

				if (grid)
				{
					grid->gridID = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSGrid>().GetGridManager()->CreateGrid(true);
					PushAddComponentCommand(selectedEntity, *grid);
				}
				ImGui::CloseCurrentPopup();
			}
		}

		if (!activeWorld.HasComponent<AudioSource>(selectedEntity))
		{
			if (ImGui::MenuItem("Audio Source"))
			{
				activeWorld.AddComponent<AudioSource>(selectedEntity, AudioSource());
				PushAddComponentCommand(selectedEntity, AudioSource());
				ImGui::CloseCurrentPopup();
			}
		}

		if (!activeWorld.HasComponent<TilemapComponent>(selectedEntity))
		{
			if (ImGui::MenuItem("Tilemap Component"))
			{
				activeWorld.AddComponent<TilemapComponent>(selectedEntity, TilemapComponent());
				PushAddComponentCommand(selectedEntity, TilemapComponent());
				ImGui::CloseCurrentPopup();
			}
		}

		if (!activeWorld.HasComponent<ScriptComponent>(selectedEntity))
		{
			if (ImGui::MenuItem("Script"))
			{
				activeWorld.AddComponent<ScriptComponent>(selectedEntity, ScriptComponent());
				PushAddComponentCommand(selectedEntity, ScriptComponent());
				ImGui::CloseCurrentPopup();
			}
		}


		if (!activeWorld.HasComponent<TextComponent>(selectedEntity))
		{
			if (ImGui::MenuItem("Text Component"))
			{
				//TextComponent newText{};
				//// Set some sensible defaults
				//strncpy(newText.text, "New Text", sizeof(newText.text) - 1);
				//strncpy(newText.fontPath, "Fonts/SourceSans3-Bold.ttf", sizeof(newText.fontPath) - 1);
				//newText.fontSize = 48;
				//newText.color = glm::vec3(1.0f, 1.0f, 1.0f); // White
				//newText.layer = 10;
				//newText.visible = true;
				//newText.ySort = false;

				activeWorld.AddComponent<TextComponent>(selectedEntity, TextComponent());
				PushAddComponentCommand(selectedEntity, TextComponent());
				ImGui::CloseCurrentPopup();
			}
		}

		if (!activeWorld.HasComponent<WorldTextComponent>(selectedEntity))
		{
			if (ImGui::MenuItem("World Text Component"))
			{
			

				activeWorld.AddComponent<WorldTextComponent>(selectedEntity, WorldTextComponent());
				PushAddComponentCommand(selectedEntity, WorldTextComponent());
				ImGui::CloseCurrentPopup();
			}
		}

		if (!activeWorld.HasComponent<ObjectEffectComponent>(selectedEntity))
		{
			if (ImGui::MenuItem("Object Effect"))
			{
				activeWorld.AddComponent<ObjectEffectComponent>(selectedEntity, ObjectEffectComponent());
				PushAddComponentCommand(selectedEntity, ObjectEffectComponent());
				ImGui::CloseCurrentPopup();
			}
		}

        ImGui::EndPopup();
    }
}