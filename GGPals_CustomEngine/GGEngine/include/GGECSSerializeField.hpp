/*************************************************************************
  \file		  GGECSSerializeField.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief	  Defines macros for creating ImGui-based ECS component inspectors.

  \details
	Provides inspector field macros for integers, floats, vectors, colors,
	asset IDs, audio properties, rotations, and matrices. Supports single
	entity inspection, multi-entity inspection, read-only fields, and
	removal buttons.
  \dependencies
	- imgui.h
	- imgui_internal.h
	- glm/glm.hpp
	- cstdint
	- string

  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/glm.hpp>
#include <string>
#include <cstring>
#include <cstdint>

/*************************************************************************
 \brief
	Tracks whether any inspector fields have been modified.
	Should be placed at the beginning of a component inspector.
*************************************************************************/
#define INSPECTOR_BEGIN_MODIFIED_TRACKING bool modified = false;

/*************************************************************************
 \brief
	Creates an integer draggable field in the inspector.
 \param[in] label
	Label displayed in the inspector.
 \param[in,out] value
	Integer variable to modify.
	If the user changes the value, 'modified' will be set to true.
*************************************************************************/
#define INSPECTOR_FIELD_INT(label, value) \
	{ if (ImGui::DragInt(label, &value, 1.0f)) modified = true; }

/*************************************************************************
 \brief
	Creates an integer slider with a specific range in the inspector.
 \param[in] label
	Label displayed in the inspector.
 \param[in,out] value
	Integer variable to modify.
 \param[in] min_val
	Minimum allowed value.
 \param[in] max_val
	Maximum allowed value.
*************************************************************************/
#define INSPECTOR_FIELD_INT_RANGE(label, value, min_val, max_val) \
	{ if (ImGui::SliderInt(label, &value, min_val, max_val)) modified = true; }

/*************************************************************************
 \brief
	Creates a floating point draggable field in the inspector.
 \param[in] label
	Label displayed in the inspector.
 \param[in,out] value
	Float variable to modify.
*************************************************************************/
#define INSPECTOR_FIELD_UINT(label, value) \
	{ int tmp = static_cast<int>(value); \
		if (ImGui::DragInt(label, &tmp, 1.0f, 0)) \
		{ \
			value = static_cast<uint32_t>(tmp); \
			modified = true; \
		} \
	}

/*************************************************************************
 \brief
	Creates a color picker for 3-component RGB values.
 \param[in] label
	Label displayed in the inspector.
 \param[in,out] value
	glm::vec3 variable representing the color.
*************************************************************************/
#define INSPECTOR_FIELD_FLOAT(label, value) \
	{ if (ImGui::DragFloat(label, &value, 0.01f)) modified = true; }

/*************************************************************************
 \brief
	Generates a component inspector function for a given component type.
 \param[in] ComponentType
	Type of the component.
 \param[in] FieldList
	Macro that defines all fields to inspect in the component.
 \return
	Returns true if any field was modified, false otherwise.
*************************************************************************/
#define INSPECTOR_FIELD_FLOAT_RANGE(label, value, min_val, max_val) \
	{ if (ImGui::SliderFloat(label, &value, min_val, max_val)) modified = true; }

/*************************************************************************
 \brief
	Creates a boolean checkbox field in the inspector.
 \param[in] label
	Label displayed in the inspector.
 \param[in,out] value
	Boolean variable to modify. Sets 'modified' to true if changed.
*************************************************************************/
#define INSPECTOR_FIELD_BOOL(label, value) \
	{ if (ImGui::Checkbox(label, &value)) modified = true; }

/*************************************************************************
 \brief
	Creates a string input field in the inspector.
 \param[in] label
	Label displayed in the inspector.
 \param[in,out] value
	std::string variable to modify. Sets 'modified' to true if changed.
*************************************************************************/
#define INSPECTOR_FIELD_STRING(label, value) \
	{ char buffer[256]; \
		strncpy(buffer, value.c_str(), sizeof(buffer) - 1); \
		buffer[sizeof(buffer) - 1] = '\0'; \
		if (ImGui::InputText(label, buffer, sizeof(buffer))) { \
			value = std::string(buffer); \
			modified = true; \
		} \
	}

/*************************************************************************
 \brief
	Creates a cstring input field in the inspector.
 \param[in] label
	Label displayed in the inspector.
 \param[in,out] value
	cstring variable to modify. Sets 'modified' to true if changed.
*************************************************************************/
#define INSPECTOR_FIELD_CSTRING(label, value) \
	{ if (ImGui::InputText(label, value, sizeof(value))) modified = true; }

/*************************************************************************
 \brief
	Creates a cstring input field in the inspector with multiple line.
 \param[in] label
	Label displayed in the inspector.
 \param[in,out] value
	cstring variable to modify. Sets 'modified' to true if changed.
*************************************************************************/
#define INSPECTOR_FIELD_CSTRING_MULTILINE(label, value) \
{ \
    if (ImGui::InputTextMultiline(label, value, sizeof(value), ImVec2(0, 100))) \
        modified = true; \
}

/*************************************************************************
 \brief
	Creates a vec2 draggable field in the inspector.
 \param[in] label
	Label displayed in the inspector.
 \param[in,out] value
	vec2 variable to modify. Sets 'modified' to true if changed.
*************************************************************************/
#define INSPECTOR_FIELD_VEC2(label, value) \
	{ if (ImGui::DragFloat2(label, &value.x, 0.01f)) modified = true; }

/*************************************************************************
 \brief
	Creates a vec3 draggable field in the inspector.
 \param[in] label
	Label displayed in the inspector.
 \param[in,out] value
	vec3 variable to modify. Sets 'modified' to true if changed.
*************************************************************************/
#define INSPECTOR_FIELD_VEC3(label, value) \
	{ if (ImGui::DragFloat3(label, &value.x, 0.01f)) modified = true; }

/*************************************************************************
 \brief
	Creates a vec4 draggable field in the inspector.
 \param[in] label
	Label displayed in the inspector.
 \param[in,out] value
	vec4 variable to modify. Sets 'modified' to true if changed.
*************************************************************************/
#define INSPECTOR_FIELD_VEC4(label, value) \
	{ if (ImGui::DragFloat4(label, &value.x, 0.01f)) modified = true; }

#define INSPECTOR_FIELD_COLOR3(label, value) \
	{ if (ImGui::ColorEdit3(label, &value.x)) modified = true; }

#define INSPECTOR_FIELD_COLOR4(label, value) \
	{ if (ImGui::ColorEdit4(label, &value.x)) modified = true; }

#define INSPECTOR_FIELD_MAT4(label, value) \
	if (ImGui::TreeNode(label)) { \
		for (int i = 0; i < 4; i++) \
		{ \
			std::string rowLabel = std::string("Row ") + std::to_string(i); \
			if (ImGui::DragFloat4(rowLabel.c_str(), &value[i].x, 0.01f)) modified = true; \
		} \
		ImGui::TreePop(); \
	}


#define INSPECTOR_FIELD_ENUM_ALIGNMENT(label, value)\
   { \
       const char* alignmentOptions[] ={"Left","Center","Right"};\
       int currentAlignment = static_cast<int>(value); \
       ImGui::PushID(label); \
       if (ImGui::Combo(label, &currentAlignment, alignmentOptions, 3)) { \
			value = static_cast<uint32_t>(currentAlignment); \
			modified = true; \
		} \
		ImGui::PopID(); \
    }

#define INSPECTOR_FIELD_ENUM_VERTICAL_ALIGNMENT(label, value) \
	{ \
		const char* vAlignmentOptions[] = { "Top", "Middle", "Bottom", "Baseline" }; \
		int currentVAlignment = static_cast<int>(value); \
		ImGui::PushID(label); \
		if (ImGui::Combo(label, &currentVAlignment, vAlignmentOptions, 4)) { \
			value = static_cast<uint32_t>(currentVAlignment); \
			modified = true; \
		} \
		ImGui::PopID(); \
	}
#define INSPECTOR_FIELD_EFFECT_TYPE(label, value) \
{ \
    const char* effectOptions[] = { "None","Fog", "Fade", "Bloom", "Outline", "Square Dissolve", "Shine","ShineLeftToRight","Burn Dissolve","Pixel Dissolve","Pulse" }; \
    int currentEffect = static_cast<int>(value); \
    ImGui::PushID(label); \
    if (ImGui::Combo(label, &currentEffect, effectOptions,11)) { \
        value = static_cast<uint32_t>(currentEffect); \
        modified = true; \
    } \
    ImGui::PopID(); \
}

#define INSPECTOR_FIELD_SCREEN_EFFECT(label, value) \
{ \
    const char* screenEffectOptions[] = { "None","Fish Eye", "Vignnete" }; \
    int currentEffect = static_cast<int>(value); \
    ImGui::PushID(label); \
    if (ImGui::Combo(label, &currentEffect, screenEffectOptions, 3)) { \
        value = static_cast<uint32_t>(currentEffect); \
        modified = true; \
    } \
    ImGui::PopID(); \
}
/*************************************************************************
 \brief
	Displays and edits a 2D position vector in the inspector.
 \param[in] label
	The text label to show for this field.
 \param[in,out] value
	The glm::vec2 value representing the position.
 \note
	Uses ImGui::DragFloat2 to allow interactive modification.
*************************************************************************/
#define INSPECTOR_FIELD_POSITION(label, value) \
    { \
        ImGui::AlignTextToFramePadding(); \
        ImGui::Text(label); \
        ImGui::SameLine(); \
        ImGui::SetNextItemWidth(-1); \
        if (ImGui::DragFloat2(("##" label), &value.x, 0.1f, -10000.f, 10000.f, "%.2f")) modified = true; \
    }

/*************************************************************************
 \brief
	Displays and edits a 2D scale vector in the inspector.
 \param[in] label
	The text label to show for this field.
 \param[in,out] value
	The glm::vec2 value representing the scale.
 \note
	Uses ImGui::DragFloat2 to allow interactive modification.
*************************************************************************/
#define INSPECTOR_FIELD_SCALE(label, value) \
	{ \
        ImGui::AlignTextToFramePadding(); \
        ImGui::Text(label); \
        ImGui::SameLine(); \
        ImGui::SetNextItemWidth(-1); \
        if (ImGui::DragFloat2(("##" label), &value.x, 0.1f, -10000.f, 10000.f, "%.2f")) modified = true; \
    }

/*************************************************************************
 \brief
	Displays and edits a rotation value in the inspector.
 \param[in] label
	The text label to show for this field.
 \param[in,out] value
	The float value representing rotation in radians.
 \note
	Converts between radians and degrees for user-friendly display
	with ImGui::SliderFloat.
*************************************************************************/
#define INSPECTOR_FIELD_ROTATION(label, value) \
	{ GGMath::float32 degrees = value * 180.f / 3.14159265f; \
		ImGui::AlignTextToFramePadding(); \
		ImGui::Text(label); \
		ImGui::SameLine(); \
		ImGui::SetNextItemWidth(-1); \
		if (ImGui::SliderFloat("##" label, &degrees, -180.f, 180.f, "%.1f")) { \
			value = degrees * 3.14159265f / 180.f; \
			modified = true; \
		}\
	}

/*************************************************************************
 \brief
	Displays and allows drag-and-drop editing of an audio asset ID.
 \param[in] label
	The text label to show for this field.
 \param[in,out] value
	The uint32_t representing the audio asset ID.
*************************************************************************/
#define INSPECTOR_FIELD_AUDIO_ASSET_ID(label, value) \
	{ \
		ImGui::PushID(label); \
		ImGui::Button((std::string(label) + ": " + std::to_string(value)).c_str(), ImVec2(250, 0)); \
		if (ImGui::BeginDragDropTarget()) \
        { \
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_AUDIO")) \
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

/*************************************************************************
 \brief
	Displays and allows drag-and-drop editing of a texture asset ID.
 \param[in] label
	The text label to show for this field.
 \param[in,out] value
	The uint32_t representing the texture asset ID.
*************************************************************************/
#define INSPECTOR_FIELD_TEXTURE_ASSET_ID(label, value) \
	{ \
		ImGui::PushID(label); \
		ImGui::Button((std::string(label) + ": " + std::to_string(value)).c_str(), ImVec2(250, 0)); \
		if (ImGui::BeginDragDropTarget()) \
		{ \
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_TEXTURE")) \
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

/*************************************************************************
 \brief
	Displays and allows drag-and-drop editing of a shader asset ID.
 \param[in] label
	The text label to show for this field.
 \param[in,out] value
	The uint32_t representing the shader asset ID.
*************************************************************************/
#define INSPECTOR_FIELD_SHADER_ASSET_ID(label, value) \
	{ \
		ImGui::PushID(label); \
		ImGui::Button((std::string(label) + ": " + std::to_string(value)).c_str(), ImVec2(250, 0)); \
        if (ImGui::BeginDragDropTarget()) \
        { \
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_SHADER")) \
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

/*************************************************************************
 \brief
	Displays and allows drag-and-drop editing of a prefab asset ID.
 \param[in] label
	The text label to show for this field.
 \param[in,out] value
	The uint32_t representing the prefab asset ID.
*************************************************************************/
#define INSPECTOR_FIELD_PREFAB_ASSET_ID(label, value) \
	{ \
		ImGui::PushID(label); \
		ImGui::Button((std::string(label) + ": " + std::to_string(value)).c_str(), ImVec2(250, 0)); \
        if (ImGui::BeginDragDropTarget()) \
        { \
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PREFAB")) \
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



/*************************************************************************
 \brief
	Displays and allows drag-and-drop editing of a tilemap asset ID.
 \param[in] label
	The text label to show for this field.
 \param[in,out] value
	The uint32_t representing the tilemap asset ID.
*************************************************************************/
#define INSPECTOR_FIELD_TILEMAP_ASSET_ID(label, value) \
	{ \
		ImGui::PushID(label); \
		ImGui::Button((std::string(label) + ": " + std::to_string(value)).c_str(), ImVec2(250, 0)); \
        if (ImGui::BeginDragDropTarget()) \
        { \
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_TILEMAP")) \
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

// Audio Inspector Fields ------------------------------------------------
#define INSPECTOR_FIELD_AUDIO_FLOAT(label, value, minVal, maxVal, defaultVal) \
{ \
    float oldValue = value; \
\
    /* label */ \
    ImGui::TextUnformatted(label); \
    ImGui::SameLine(); \
\
    /* compute widths */ \
    float fullWidth   = ImGui::GetContentRegionAvail().x; \
    float inputWidth  = 65.0f; \
    float sliderWidth = fullWidth - inputWidth - 10.0f; \
    if (sliderWidth < 50.0f) sliderWidth = fullWidth; \
\
    /* slider */ \
    ImGui::SetNextItemWidth(sliderWidth); \
    ImGui::SliderFloat(("##slider_" label), &value, minVal, maxVal, "%.2f", \
                       ImGuiSliderFlags_AlwaysClamp); \
\
    /* input box */ \
    ImGui::SameLine(); \
    ImGui::SetNextItemWidth(inputWidth); \
    if (ImGui::InputFloat(("##input_" label), &value, 0, 0, "%.2f")) \
        modified = true; \
\
    /* clamp */ \
    value = ImClamp(value, minVal, maxVal); \
\
    /* right-click context reset */ \
    if (ImGui::BeginPopupContextItem(("##ctx_" label))) { \
        if (ImGui::MenuItem("Reset")) { \
            value = defaultVal; \
            modified = true; \
        } \
        ImGui::EndPopup(); \
    } \
\
    if (value != oldValue) \
	{ \
		modified = true; \
	} \
}

/*************************************************************************
 \brief
	Displays and edits an audio output bus ID.
 \param[in] label
	The text label to show for this field.
 \param[in,out] value
	The uint8_t representing the output bus.
*************************************************************************/
#define INSPECTOR_FIELD_AUDIO_OUTPUT_BUS(label, value) \
    { \
        int tmp = static_cast<int>(value); \
        if (ImGui::DragInt(label, &tmp, 1, 0)) \
        { \
            value = static_cast<uint8_t>(tmp); \
            modified = true; \
        } \
    }

/*************************************************************************
 \brief
	Displays and edits an audio volume property (0.0�1.0).
 \param[in] label
	The text label to show for this field.
 \param[in,out] value
	The float value representing volume.
*************************************************************************/
#define INSPECTOR_FIELD_AUDIO_VOLUME(label, value) \
    { \
        INSPECTOR_FIELD_AUDIO_FLOAT(label, value, 0.f, 1.f, 1.f) \
    }

/*************************************************************************
 \brief
	Displays and edits an audio pitch property (-3.0�3.0).
 \param[in] label
	The text label to show for this field.
 \param[in,out] value
	The float value representing pitch.
*************************************************************************/
#define INSPECTOR_FIELD_AUDIO_PITCH(label, value) \
    { \
        INSPECTOR_FIELD_AUDIO_FLOAT(label, value, 0.5f, 2.f, 1.f) \
    }

/*************************************************************************
 \brief
	Displays and edits an audio pan property (-1.0�1.0).
 \param[in] label
	The text label to show for this field.
 \param[in,out] value
	The float value representing pan.
*************************************************************************/
#define INSPECTOR_FIELD_AUDIO_PAN(label, value) \
    { \
        INSPECTOR_FIELD_AUDIO_FLOAT(label, value, -1.f, 1.f, 0.f) \
    }
// -----------------------------------------------------------------------

/*************************************************************************
 \brief
	Displays and allows drag-and-drop editing of a scene asset ID.
 \param[in] label
	The text label to show for this field.
 \param[in,out] value
	The uint32_t representing the scene asset ID.
*************************************************************************/
#define INSPECTOR_FIELD_SCENE_ASSET_ID(label, value) \
	{ \
		ImGui::PushID(label); \
		ImGui::Button((std::string(label) + ": " + std::to_string(value)).c_str(), ImVec2(250, 0)); \
        if (ImGui::BeginDragDropTarget()) \
        { \
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_SCENE")) \
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

/*************************************************************************
 \brief
	Displays and allows drag-and-drop editing of an animation asset ID.
 \param[in] label
	The text label to show for this field.
 \param[in,out] value
	The uint32_t representing the animation asset ID.
*************************************************************************/
#define INSPECTOR_FIELD_ANIM_ASSET_ID(label, value) \
	{ \
		ImGui::PushID(label); \
		ImGui::Button((std::string(label) + ": " + std::to_string(value)).c_str(), ImVec2(250, 0)); \
		if (ImGui::BeginDragDropTarget()) \
		{ \
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ANIM")) \
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

// font id
#define INSPECTOR_FIELD_FONT_ASSET_ID(label, value) \
	{ \
		ImGui::PushID(label); \
		ImGui::Button((std::string(label) + ": " + std::to_string(value)).c_str(), ImVec2(250, 0)); \
		if (ImGui::BeginDragDropTarget()) \
		{ \
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FONT")) \
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

/*************************************************************************
 \brief
	Generic inspector field macro that expands to the appropriate
	type-specific inspector field for a component.
 \param[in] type
	The type of the field (matches a defined INSPECTOR_FIELD_* macro).
 \param[in] name
	The member name of the component to inspect.
*************************************************************************/
#define INSPECTOR_FIELD(type, name) \
	INSPECTOR_FIELD_##type(#name, component.name);

/*************************************************************************
 \brief
	Inspector field macro with a custom display label.
 \param[in] type
	The type of the field.
 \param[in] name
	The member name of the component to inspect.
 \param[in] displayLabel
	The custom label to display in the inspector.
*************************************************************************/
#define INSPECTOR_FIELD_LABELED(type, name, displayLabel) \
	INSPECTOR_FIELD_##type(displayLabel, component.name);

/*************************************************************************
 \brief
	Inspector field macro for a value constrained to a range.
 \param[in] type
	The type of the field.
 \param[in] name
	The member name of the component to inspect.
 \param[in] min_val
	Minimum allowed value for the field.
 \param[in] max_val
	Maximum allowed value for the field.
*************************************************************************/
#define INSPECTOR_FIELD_RANGED(type, name, min_val, max_val) \
	INSPECTOR_FIELD_##type##_RANGE(#name, component.name, min_val, max_val);

/*************************************************************************
 \brief
	Conditional inspector field macro. Displays the field only if
	the given condition evaluates to true.
 \param[in] type
	The type of the field.
 \param[in] name
	The member name of the component to inspect.
 \param[in] condition
	Boolean condition determining whether to show the field.
*************************************************************************/
#define INSPECTOR_FIELD_CONDITIONAL(type, name, condition) \
	if (condition) \
	{ \
		INSPECTOR_FIELD_##type(#name, component.name); \
	}

/*************************************************************************
 \brief
	Read-only inspector field macro. Displays the value without allowing
	user edits.
 \param[in] type
	The type of the field (must match a READONLY type macro).
 \param[in] name
	The member name of the component to inspect.
*************************************************************************/
#define INSPECTOR_FIELD_READONLY(type, name) \
	INSPECTOR_FIELD_READONLY_##type(#name, component.name);

/*************************************************************************
 \brief
	Displays a read-only integer field in the inspector.
 \param[in] label
	The label text to show.
 \param[in] value
	The integer value to display.
*************************************************************************/
#define INSPECTOR_FIELD_READONLY_INT(label, value) \
    ImGui::Text("%s: %d", label, value)

/*************************************************************************
 \brief
	Displays a read-only unsigned integer field in the inspector.
 \param[in] label
	The label text to show.
 \param[in] value
	The unsigned integer value to display.
*************************************************************************/
#define INSPECTOR_FIELD_READONLY_UINT(label, value) \
    ImGui::Text("%s: %u", label, value)

/*************************************************************************
 \brief
	Displays a read-only float field in the inspector.
 \param[in] label
	The label text to show.
 \param[in] value
	The float value to display.
*************************************************************************/
#define INSPECTOR_FIELD_READONLY_FLOAT(label, value) \
    ImGui::Text("%s: %.3f", label, value)

/*************************************************************************
 \brief
	Displays a read-only 2D vector field in the inspector.
 \param[in] label
	The label text to show.
 \param[in] value
	The 2D vector (vec2) value to display.
*************************************************************************/
#define INSPECTOR_FIELD_READONLY_VEC2(label, value) \
    ImGui::Text("%s: (%.2f, %.2f)", label, value.x, value.y)

/*************************************************************************
 \brief
	Displays a read-only 3D vector field in the inspector.
 \param[in] label
	The label text to show.
 \param[in] value
	The 3D vector (vec3) value to display.
*************************************************************************/
#define INSPECTOR_FIELD_READONLY_VEC3(label, value) \
    ImGui::Text("%s: (%.2f, %.2f, %.2f)", label, value.x, value.y, value.z)

/*************************************************************************
 \brief
	Generates a component inspector function for a given component type.
 \param[in] ComponentType
	Type of the component.
 \param[in] FieldList
	Macro that defines all fields to inspect in the component.
 \return
	Returns true if any field was modified, false otherwise.
*************************************************************************/
#define GENERATE_COMPONENT_INSPECTOR(ComponentType, FieldList) \
	inline bool InspectComponent##ComponentType(ComponentType& component) { \
		bool changed = false; \
		ImGui::PushID(#ComponentType); \
		if (ImGui::CollapsingHeader(#ComponentType, ImGuiTreeNodeFlags_DefaultOpen)) \
		{ \
			ImGui::Indent(); \
			INSPECTOR_BEGIN_MODIFIED_TRACKING \
			FieldList(INSPECTOR_FIELD) \
			ImGui::Unindent(); \
			changed = modified; \
		} \
		ImGui::PopID(); \
		return changed; \
	}

/*************************************************************************
 \brief
	Generates a component inspector with a custom display name.
 \param[in] ComponentType
	Type of the component.
 \param[in] DisplayName
	Header label for the collapsing inspector.
 \param[in] FieldList
	Macro that defines all fields to inspect in the component.
 \return
	Returns true if any field was modified, false otherwise.
*************************************************************************/
#define GENERATE_COMPONENT_INSPECTOR_NAMED(ComponentType, DisplayName, FieldList) \
	inline bool InspectComponent##ComponentType(ComponentType& component) \
	{ \
		bool changed = false; \
		ImGui::PushID(#ComponentType); \
		if (ImGui::CollapsingHeader(DisplayName, ImGuiTreeNodeFlags_DefaultOpen)) \
		{ \
			ImGui::Indent(); \
			INSPECTOR_BEGIN_MODIFIED_TRACKING \
			FieldList(INSPECTOR_FIELD) \
			ImGui::Unindent(); \
			changed = modified; \
		} \
		ImGui::PopID(); \
		return changed; \
	}

enum class ComponentInspectorResult
{
	NoChange,
	Modified,
	RemoveRequested
};

/*************************************************************************
 \brief
	Generates a component inspector with a "Remove" button.
 \param[in] ComponentType
	Type of the component.
 \param[in] FieldList
	Macro that defines all fields to inspect in the component.
 \return
	ComponentInspectorResult:
	  - NoChange: nothing modified
	  - Modified: fields modified
	  - RemoveRequested: user clicked remove button
*************************************************************************/
#define GENERATE_COMPONENT_INSPECTOR_WITH_REMOVAL(ComponentType, FieldList) \
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

/*************************************************************************
 \brief
	Generates an inspector for all components of a single entity.
 \param[in] ComponentList
	Macro that enumerates all component types to inspect.
*************************************************************************/
#define GENERATE_ENTITY_INSPECTOR(ComponentList) \
    inline void InspectEntity(Entity entity, World& world) { \
        ImGui::Text("Entity ID: %u (Generation: %u)", entity.id, entity.generation); \
        ImGui::Separator(); \
        ComponentList(INSPECT_ENTITY_COMPONENT) \
    }

/*************************************************************************
 \brief
	Inspects a single component of a given type on an entity.
 \param[in] ComponentType
	The type of the component to inspect.
 \note
	Checks if the entity has the component before calling the
	corresponding component inspector function.
*************************************************************************/
#define INSPECT_ENTITY_COMPONENT(ComponentType) \
    if (world.HasComponent<ComponentType>(entity)) { \
        auto& component = world.GetComponent<ComponentType>(entity); \
        InspectComponent##ComponentType(component); \
    }

/*************************************************************************
 \brief
	Generates an inspector for multiple selected entities.
 \param[in] ComponentList
	Macro that enumerates all component types to inspect.
*************************************************************************/
#define GENERATE_MULTI_ENTITY_INSPECTOR(ComponentList) \
    inline void InspectMultipleEntities(const std::vector<Entity>& entities, World& world) { \
        if (entities.empty()) return; \
        \
        ImGui::Text("Selected Entities: %zu", entities.size()); \
        ImGui::Separator(); \
        \
        ComponentList(INSPECT_MULTI_ENTITY_COMPONENT) \
    }

/*************************************************************************
 \brief
	Inspects a component across multiple selected entities.
 \param[in] ComponentType
	The type of the component to inspect.
 \note
	- Collects all entities that have this component.
	- Displays a unified inspector if all selected entities have it.
	- Otherwise, indicates that not all entities possess the component.
*************************************************************************/
#define INSPECT_MULTI_ENTITY_COMPONENT(ComponentType) \
    { \
        std::vector<ComponentType*> components; \
        for (Entity entity : entities) { \
            if (world.HasComponent<ComponentType>(entity)) { \
                components.push_back(&world.GetComponent<ComponentType>(entity)); \
            } \
        } \
        \
        if (!components.empty()) { \
            ImGui::Text(#ComponentType " (%zu/%zu entities)", components.size(), entities.size()); \
            if (components.size() == entities.size()) { \
                /* All entities have this component - show unified editor */ \
                InspectComponent##ComponentType(*components[0]); \
                /* TODO: Apply changes to all components */ \
            } else { \
                ImGui::Text("  (Not all entities have this component)"); \
            } \
        } \
    }

/*************************************************************************
 \brief
	Generates a fast component inspector that updates only when open or forced.
 \param[in] ComponentType
	Type of the component.
 \param[in] FieldList
	Macro that defines all fields to inspect in the component.
 \param[in] forceUpdate
	Forces the inspector to refresh widgets even if previously closed.
 \return
	True if the inspector is open, false otherwise.
*************************************************************************/
#define GENERATE_FAST_COMPONENT_INSPECTOR(ComponentType, FieldList) \
    inline bool InspectComponent##ComponentType##Fast(ComponentType& component, bool forceUpdate = false) { \
        static bool wasOpen = false; \
        bool isOpen = ImGui::CollapsingHeader(#ComponentType, ImGuiTreeNodeFlags_DefaultOpen); \
        \
        /* Only update widgets when header is open or state changed */ \
        if (isOpen && (isOpen != wasOpen || forceUpdate)) { \
            ImGui::Indent(); \
            FieldList(INSPECTOR_FIELD) \
            ImGui::Unindent(); \
        } \
        \
        wasOpen = isOpen; \
        return isOpen; \
    }

