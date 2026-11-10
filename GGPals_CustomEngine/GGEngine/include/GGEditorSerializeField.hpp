/*************************************************************************
  \file       GGEditorSerializeField.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
    Provides macros for declaring ECS component fields and generating
    component inspectors for the editor.

  \details
    This header defines a set of macros that simplify:
    - Declaring component fields with default values.
    - Indicating inspector widget types (rotation, color, asset ID, position, string).
    - Defining components with metadata required by the ECS.
    - Generating inspectors with optional removal buttons.

    All macros are built on top of GGECSSerializeField.hpp, which
    provides the underlying inspector and field tracking functionality.

  \dependencies
    - GGECSSerializeField.hpp
    - GGMath (for vector types and float32)
    - ImGui (via GGECSSerializeField.hpp)

  \history
    - 1.0 (10 October 2025): Initial version.

  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <GGECSSerializeField.hpp>

/*************************************************************************
  \brief
    Clean field declaration macro.
  \details
    Use this to declare component fields with a clean syntax.
    Handles the common case of field declaration with default values.
*************************************************************************/
#define SERIALIZE_FIELD(type, name, default_val) \
    type name = default_val

/*************************************************************************
  \brief
    Specialized field declaration macros for better inspector UX.
  \details
    These variants provide the same declaration syntax but hint at
    the intended inspector widget type through the macro name.
*************************************************************************/

// Rotation field (will use degree slider in inspector)
#define SERIALIZE_ROTATION(name, default_radians) \
    GGMath::float32 name = default_radians

// Color field (will use color picker in inspector)  
#define SERIALIZE_COLOR(type, name, default_val) \
    type name = default_val

// Asset ID field (will use asset browser in inspector)
#define SERIALIZE_ASSET_ID(name, default_val) \
    uint32_t name = default_val

// Position field (will use position widget in inspector)
#define SERIALIZE_POSITION(name, default_val) \
    GGMath::Vector2 name = default_val

// String field with fixed size
#define SERIALIZE_CSTRING(name, size, default_val) \
    char name[size] = default_val

#define ENUM_ALIGNMENT ENUM_ALIGNMENT
#define ENUM_VERTICAL_ALIGNMENT ENUM_VERTICAL_ALIGNMENT

#define EFFECT_TYPE EFFECT_TYPE 
#define SCREEN_EFFECT_TYPE SCREEN_EFFECT_TYPE
/*************************************************************************
  \brief
    Component definition helper macros.
  \details
    These macros help you define components with the standard ECS metadata
    while keeping everything clean and readable.
*************************************************************************/

// Start a component definition with automatic ID and size
#define BEGIN_COMPONENT(ComponentName, ComponentID, SerializedSize) \
    struct ComponentName { \
        static constexpr uint16_t COMPONENT_ID = static_cast<uint16_t>(ComponentID); \
        static constexpr size_t SERIALIZED_SIZE = SerializedSize;

// End component definition and define inspector fields  
#define END_COMPONENT_WITH_INSPECTOR(ComponentName, field_list) \
    }; \
    static_assert(sizeof(ComponentName) == ComponentName::SERIALIZED_SIZE, \
                  #ComponentName " SERIALIZED_SIZE does not match struct size!"); \
    GENERATE_COMPONENT_INSPECTOR_WITH_REMOVAL(ComponentName, field_list)

#define END_COMPONENT(ComponentName) \
    }; \
    static_assert(sizeof(ComponentName) == ComponentName::SERIALIZED_SIZE, \
                  #ComponentName " SERIALIZED_SIZE does not match struct size!");