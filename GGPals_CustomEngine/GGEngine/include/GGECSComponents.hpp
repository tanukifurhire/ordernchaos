/*************************************************************************
  \file       GGECSComponents.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       3 October 2025
  \version    1.0
  \brief
    Defines ECS component structures with fixed-size serialization contracts.
  \details
    This file provides the fundamental component archetypes for GGEngine's
    Entity-Component-System architecture. Each component is a POD-like
    structure with compile-time verified serialization sizes, enabling
    efficient memory-mapped I/O, network replication, and cache-coherent
    iteration. Components follow strict size contracts enforced by
    static_assert to prevent structure padding mismatches across compilers
    and platforms. All components expose a COMPONENT_ID constexpr for runtime
    type identification without RTTI overhead.
  \dependencies
    <cstdint>, GGSystemInterface.hpp, GGECSInterface.hpp, GGMesh.hpp,
    GGMath/Types.hpp, glm (via GGMesh)
  \see
    Components (enum), GGECSInterface, GGSystemInterface
  \history
    - 1.0 (3 October 2025): Initial component definitions.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include <cstdint>
#include <fmod.hpp>
#include <GGSystemInterface.hpp>
#include <GGECSInterface.hpp>
#include <GGMesh.hpp>
#include <GGMath/Types.hpp>
#include <GGMath/Vector2.hpp>
#include <GGMath/Matrix3x3.hpp>

#include <GGEditorSerializeField.hpp>
#include <GGEngineEventBus.hpp>

#define INT INT
#define UINT UINT  
#define FLOAT FLOAT
#define BOOL BOOL
#define VEC2 VEC2
#define VEC3 VEC3
#define VEC4 VEC4
#define MAT4 MAT4
#define ROTATION ROTATION
#define COLOR3 COLOR3
#define COLOR4 COLOR4
#define TEXTURE_ASSET_ID TEXTURE_ASSET_ID
#define SHADER_ASSET_ID SHADER_ASSET_ID
#define AUDIO_ASSET_ID AUDIO_ASSET_ID
#define PREFAB_ASSET_ID PREFAB_ASSET_ID
#define SCRIPT_ASSET_ID SCRIPT_ASSET_ID
#define SCENE_ASSET_ID SCENE_ASSET_ID
#define SCRIPT_INSTANCE SCRIPT_INSTANCE
#define ANIM_ASSET_ID ANIM_ASSET_ID
#define AUDIO_ASSET_ID AUDIO_ASSET_ID
#define TILEMAP_ASSET_ID TILEMAP_ASSET_ID
#define POSITION POSITION
#define SCALE SCALE
#define STRING STRING
#define CSTRING CSTRING
#define CSTRING_MULTILINE CSTRING_MULTILINE
#define ENUM_ALIGNMENT ENUM_ALIGNMENT
#define ENUM_VERTICAL_ALIGNMENT ENUM_VERTICAL_ALIGNMENT
#define FONT_ASSET_ID FONT_ASSET_ID
#define EFFECT_TYPE EFFECT_TYPE 
#define SCREEN_EFFECT_TYPE SCREEN_EFFECT_TYPE
// Please update the ComponentIDs enum if you modify the COMPONENT_ID here!

/*************************************************************************
  \struct AABBFloat
  \brief
    Axis-aligned bounding box for 2D collision detection.
  \details
    Represents a non-rotated rectangular collision volume defined by
    extents in world space. Used by spatial partitioning systems for
    broad-phase collision detection and frustum culling. The bounds are
    stored as individual edge coordinates rather than center+half-extents
    for direct rasterization compatibility.
  \note
    Assumes Y-up coordinate system where top > bottom. No validation is
    performed on construction; systems must ensure top >= bottom and
    right >= left to avoid degenerate AABBs.
  \see
    Components::AABB_FLOAT, CollisionSystem
*************************************************************************/
#define AABBFLOAT_INSPECTOR_FIELDS(X) \
    X(FLOAT, top); \
    X(FLOAT, bottom); \
    X(FLOAT, left); \
    X(FLOAT, right);

BEGIN_COMPONENT(AABBFloat, Components::AABB_FLOAT, 16)
    SERIALIZE_FIELD(GGMath::float32, top, 0.f);
    SERIALIZE_FIELD(GGMath::float32, bottom, 0.f);
    SERIALIZE_FIELD(GGMath::float32, left, 0.f);
    SERIALIZE_FIELD(GGMath::float32, right, 0.f);
END_COMPONENT_WITH_INSPECTOR(AABBFloat, AABBFLOAT_INSPECTOR_FIELDS)

/*************************************************************************
  \struct MeshRenderer
  \brief
    Renderable mesh component with material properties.
  \details
    Encapsulates mesh geometry, shader program, texture reference, and
    color modulation for the rendering pipeline. Asset references use
    indirection through uint32_t handles resolved by GGECSStandardAssetManager.
    Color channels use additive and multiplicative blending:
    finalColor = (baseColor * colorMultiply) + colorAdd

    The component stores UV transformation matrices for texture atlas
    mapping and animation. Mesh data is stored inline for cache locality
    during rendering system iteration.
  \warning
    Large SERIALIZED_SIZE (72 bytes) may cause cache line splits. Consider
    splitting into MeshRenderer + MaterialComponent if profiling shows
    cache thrashing during rendering passes.
  \see
    GGMesh, GGShader, GGECSStandardAssetManager, RenderingSystem
*************************************************************************/
#define MESHRENDERER_INSPECTOR_FIELDS(X) \
    X(SHADER_ASSET_ID, shaderAssetID); \
    X(INT, layer); \
    X(BOOL, ySort); \
    X(BOOL , pixelPerfectPicking);\
    X(BOOL ,  pickableObject);\
    X(COLOR4, colorAdd); \
    X(COLOR4, colorMultiply);

BEGIN_COMPONENT(MeshRenderer, Components::MESH_RENDERER, 80)
    GGMesh mesh;
    SERIALIZE_ASSET_ID(shaderAssetID, 0);
    SERIALIZE_FIELD(int32_t, layer, 0);
    SERIALIZE_FIELD(bool, ySort, 0);
    SERIALIZE_FIELD(bool, pixelPerfectPicking, 0);
    SERIALIZE_FIELD(bool, pickableObject, 1);
    SERIALIZE_COLOR(glm::vec4, colorAdd, glm::vec4(0.f, 0.f, 0.f, 1.f));
    SERIALIZE_COLOR(glm::vec4, colorMultiply, glm::vec4(1.f));
    GGMath::float32 perspectiveFactor = 0.f;

    GGShader const& GetShader() const;
END_COMPONENT_WITH_INSPECTOR(MeshRenderer, MESHRENDERER_INSPECTOR_FIELDS)

/*************************************************************************
    \struct UIComponent
    \brief
    Lightweight rendering component for UI elements.
    \details
    Provides shader assignment, draw-layer ordering, Y-sorting behavior,
    and color modulation for UI quads. Unlike MeshRenderer, this component
    does not store mesh data � UI geometry is procedurally generated by the
    rendering system. Color multiplication and addition follow the same
    pipeline as MeshRenderer for consistent material blending.
    \note
    Intended for 2D UI rendering only; does not participate in world-space
    transforms unless paired with a Transform component.
    \see
    UISystem, RenderingSystem
*************************************************************************/
#define UICOMPONENT_INSPECTOR_FIELDS(X) \
    X(SHADER_ASSET_ID, shaderAssetID); \
    X(INT, layer); \
    X(BOOL, ySort); \
    X(BOOL , pixelPerfectPicking);\
    X(BOOL,pickableObject);\
    X(COLOR4, colorAdd); \
    X(COLOR4, colorMultiply);

BEGIN_COMPONENT(UIComponent, Components::UI_COMPONENT, 44)
    SERIALIZE_ASSET_ID(shaderAssetID, 0);
    SERIALIZE_FIELD(int32_t, layer, 0);
    SERIALIZE_FIELD(bool, ySort, 0);
    SERIALIZE_FIELD(bool, pixelPerfectPicking, 0);
    SERIALIZE_FIELD(bool, pickableObject, 1);
    SERIALIZE_COLOR(glm::vec4, colorAdd, glm::vec4(0.f, 0.f, 0.f, 1.f));
    SERIALIZE_COLOR(glm::vec4, colorMultiply, glm::vec4(1.f));
END_COMPONENT_WITH_INSPECTOR(UIComponent, UICOMPONENT_INSPECTOR_FIELDS)

/*************************************************************************
  \struct MovementInput
  \brief
    Keyboard-driven character controller configuration.
  \details
    Maps keyboard scancodes to directional movement commands Systems
    query input state and apply velocity changes scaled by speed parameter.
    Default WASD layout with 200 units/sec velocity magnitude.
  \warning
    Stores raw integer key codes without validation. Systems must handle
    invalid/unmapped keys gracefully. Consider migrating to InputAction
    abstraction for rebindable controls and gamepad support.
  \see
    InputSystem, Rigidbody2DFloat
*************************************************************************/
#define MOVEMENTINPUT_INSPECTOR_FIELDS(X) \
    X(INT, keyUp); \
    X(INT, keyDown); \
    X(INT, keyLeft); \
    X(INT, keyRight); \
    X(FLOAT, speed); 

BEGIN_COMPONENT(MovementInput, Components::MOVEMENT_INPUT, 20)
    SERIALIZE_FIELD(int, keyUp, 0);
    SERIALIZE_FIELD(int, keyDown, 0);
    SERIALIZE_FIELD(int, keyLeft, 0);
    SERIALIZE_FIELD(int, keyRight, 0);
    SERIALIZE_FIELD(float, speed, 0.f);
    
    MovementInput(int up = 'W', int down = 'S', int left = 'A', int right = 'D', float spd = 200.0f)
        : keyUp(up), keyDown(down), keyLeft(left), keyRight(right), speed(spd) {
    }
    END_COMPONENT_WITH_INSPECTOR(MovementInput, MOVEMENTINPUT_INSPECTOR_FIELDS)

/*************************************************************************
  \struct Rigidbody2DFloat
  \brief
    2D physics body with velocity and mass properties.
  \details
    Integrates with physics system for velocity-based motion and collision
    response. The flags field is a bitmask for physics properties (static,
    kinematic, trigger, etc.). Systems apply forces and impulses to velocity
    components, which are integrated into Transform2DFloat positions.
  \note
    Zero mass is valid for static bodies (infinite effective mass). Systems
    must check for divide-by-zero before computing acceleration = force/mass.
  \see
    PhysicsSystem, Transform2DFloat, AABBFloat
*************************************************************************/
#define RIGIDBODY2DFLOAT_INSPECTOR_FIELDS(X) \
    X(FLOAT, vx); \
    X(FLOAT, vy); \
    X(FLOAT, mass); \
    X(UINT, flags);

BEGIN_COMPONENT(Rigidbody2DFloat, Components::RIGIDBODY2D_FLOAT, 16)
    SERIALIZE_FIELD(GGMath::float32, vx, 0.f);
    SERIALIZE_FIELD(GGMath::float32, vy, 0.f);
    SERIALIZE_FIELD(GGMath::float32, mass, 1.f);
    SERIALIZE_FIELD(uint32_t, flags, 0);
    END_COMPONENT_WITH_INSPECTOR(Rigidbody2DFloat, RIGIDBODY2DFLOAT_INSPECTOR_FIELDS)

/*************************************************************************
  \struct EntityName
  \brief
    Human-readable entity identifier for debugging and editor display.
  \details
    Fixed-size C-string buffer for entity names. Used by editor systems
    for hierarchy display and debugging tools. Not intended for runtime
    entity lookup (use hash-based IDs instead for O(1) queries).
  \warning
    SetName truncates silently if newName exceeds 63 characters (64th byte
    reserved for null terminator). No warning or error reported to caller.
  \see
    EditorSystem, Components::ENTITY_NAME
*************************************************************************/
#define ENTITYNAME_INSPECTOR_FIELDS(X) \
    X(CSTRING, name);

BEGIN_COMPONENT(EntityName, Components::ENTITY_NAME, 64)
    SERIALIZE_CSTRING(name, 64, "Object");
    std::string GetName() const { return std::string(name); }
    void SetName(std::string const& newName) { newName.length() <= SERIALIZED_SIZE ? std::strcpy(name, newName.c_str()) : newName; }
END_COMPONENT_WITH_INSPECTOR(EntityName, ENTITYNAME_INSPECTOR_FIELDS)

/*************************************************************************
  \struct ClickToMove
  \brief
    Point-and-click pathfinding target component.
  \details
    Stores destination coordinates for click-to-move navigation. Systems
    compute paths to (targetX, targetY) and update entity velocity. The
    hasTarget flag enables/disables pathfinding behavior. isNetworked flag
    marks entities requiring network replication of movement commands.
  \note
    No pathfinding algorithm is embedded; systems must implement A*, navmesh
    queries, or direct steering behaviors externally.
  \see
    NavigationSystem, Rigidbody2DFloat
*************************************************************************/
struct ClickToMove {
    float targetX = 0.0f;
    float targetY = 0.0f;
    bool hasTarget = false;
    bool isNetworked = false;

    static constexpr uint16_t COMPONENT_ID = static_cast<uint16_t>(Components::CLICK_TO_MOVE);
    static constexpr size_t SERIALIZED_SIZE = 12;
};

static_assert(sizeof(ClickToMove) == ClickToMove::SERIALIZED_SIZE, "SERIALIZED_SIZE does not match struct size!");

/*************************************************************************
  \struct PrefabInstance
  \brief
    Marks entity as instance of a prefab template.
  \details
    Links entity to prefab definition for batch updates and editor workflows.
    When prefab asset is modified, systems can iterate all entities with
    matching prefab IDs to propagate changes. Minimal 4-byte footprint for
    wide deployment across entity populations.
  \see
    PrefabSystem, EntityFactory, Components::PREFAB_INSTANCE
*************************************************************************/
struct PrefabInstance {
    static constexpr uint16_t COMPONENT_ID = static_cast<uint16_t>(Components::PREFAB_INSTANCE);
    static constexpr size_t SERIALIZED_SIZE = 64;

    char prefabInstanceName[64] = "Unknown Prefab";

    std::string GetPrefabInstanceName() const { return std::string(prefabInstanceName); }
    void SetPrefabInstanceName(const std::string& newName)
    {
        size_t copyLen = std::min(newName.length(), SERIALIZED_SIZE - 1);
        std::memcpy(prefabInstanceName, newName.c_str(), copyLen);
        prefabInstanceName[copyLen] = '\0';
    }
};

static_assert(sizeof(PrefabInstance) == PrefabInstance::SERIALIZED_SIZE, "SERIALIZED_SIZE does not match struct size!");

/*************************************************************************
  \struct Sprite
  \brief
    2D sprite rendering component with texture atlas support.
  \details
    Simpler alternative to MeshRenderer for 2D games using quad primitives.
    Stores texture reference and UV mapping parameters for sprite sheet
    animation. The UV bounds define the sub-rectangle of the texture atlas,
    while the UV transform matrix enables rotation, scaling, and translation
    of texture coordinates for effects like sprite distortion.
  \note
    Does not store mesh data; rendering system generates quads procedurally.
    Smaller memory footprint than MeshRenderer but less flexible for custom
    geometry.
  \see
    SpriteRenderingSystem, SpriteAnimator, GGECSStandardAssetManager
*************************************************************************/
#define SPRITE_INSPECTOR_FIELDS(X) \
    X(TEXTURE_ASSET_ID, textureAssetID) \
    X(VEC4, textureUVBounds) \
    X(MAT4, textureUVTransform)

BEGIN_COMPONENT(Sprite, Components::SPRITE, 84)
    SERIALIZE_ASSET_ID(textureAssetID, 0);
    SERIALIZE_FIELD(glm::vec4, textureUVBounds, glm::vec4(0.f, 0.f, 1.f, 1.f));
    SERIALIZE_FIELD(glm::mat4, textureUVTransform, glm::mat4(1.f));
    /*************************************************************************
    \brief
        Resolves texture asset handle to OpenGL texture object.
    \return
        OpenGL texture name for glBindTexture.
    \throws AssetNotFoundException
        If textureAssetID is not registered.
    *************************************************************************/
    GLuint const GetTextureID() const;
END_COMPONENT_WITH_INSPECTOR(Sprite, SPRITE_INSPECTOR_FIELDS)
 
/*************************************************************************
  \struct SpriteAnimator
  \brief
    Frame-based sprite animation controller.
  \details
    Manages sprite sheet animations via frame indices and animation states.
    Systems advance frameTime by delta time and update currentFrameIndex
    when thresholds are exceeded. The animationAssetID references a data-driven
    animation definition (frame durations, UV coordinates, state transitions).
  \note
    Animation state transitions are data-driven; call SetNextAnimationState
    to queue state changes (e.g., "idle" -> "run"). Systems apply transitions
    at next frame boundary to avoid mid-frame corruption.
  \see
    SpriteAnimationSystem, Sprite, AnimationAsset
*************************************************************************/
#define SPRITEANIMATOR_INSPECTOR_FIELDS(X) \
    X(ANIM_ASSET_ID, animationAssetID); \

BEGIN_COMPONENT(SpriteAnimator, Components::SPRITE_ANIMATOR, 24)
    SERIALIZE_ASSET_ID(animationAssetID, 0);
    uint32_t currentFrameIndex{ 0 };
    uint32_t currentAnimState{ 0 };
    double frameTime{ 0.0 };

    /*************************************************************************
      \brief
        Queues transition to new animation state.
      \param[in] newState
        String identifier for target animation state (e.g., "attack").
      \details
        Converts state name to hash and queues transition. Actual state change
        occurs at next frame boundary to prevent visual artifacts. Systems must
        validate that newState exists in animation asset before applying.
    *************************************************************************/
    void SetNextAnimationState(std::string const& newState);
END_COMPONENT_WITH_INSPECTOR(SpriteAnimator, SPRITEANIMATOR_INSPECTOR_FIELDS)

/*************************************************************************
  \struct ScreenBounds
  \brief
    Screen-space clamping bounds for UI and camera constraints.
  \details
    Defines rectangular region in screen coordinates (pixel space or NDC)
    for clamping entity positions. Used by camera systems to prevent
    scrolling beyond level boundaries, and UI systems to constrain
    draggable elements within viewport.
  \note
    Coordinate system is context-dependent: rendering systems may interpret
    as NDC [-1,1], while UI systems may use pixel coordinates. Document
    system-specific conventions in system headers.
  \see
    CameraSystem, UISystem, Components::SCREEN_BOUNDS
*************************************************************************/
#define SCREENBOUNDS_INSPECTOR_FIELDS(X) \
    X(FLOAT, top); \
    X(FLOAT, bottom); \
    X(FLOAT, left); \
    X(FLOAT, right);

BEGIN_COMPONENT(ScreenBounds, Components::SCREEN_BOUNDS, 16)
    SERIALIZE_FIELD(GGMath::float32, top, 0.f);
    SERIALIZE_FIELD(GGMath::float32, bottom, 0.f);
    SERIALIZE_FIELD(GGMath::float32, left, 0.f);
    SERIALIZE_FIELD(GGMath::float32, right, 0.f);
END_COMPONENT_WITH_INSPECTOR(ScreenBounds, SCREENBOUNDS_INSPECTOR_FIELDS)

#define NAMECOMPONENT_INSPECTOR_FIELDS(X) \
    X(CSTRING, name);

BEGIN_COMPONENT(NameComponent, Components::NAME_COMPONENT, 128)
    SERIALIZE_CSTRING(name, 128, "Unnamed Entity");
    std::string GetName() const { return std::string(name); }
    void SetName(std::string const& newName) { newName.length() <= SERIALIZED_SIZE ? std::strcpy(name, newName.c_str()) : newName; }
END_COMPONENT_WITH_INSPECTOR(NameComponent, NAMECOMPONENT_INSPECTOR_FIELDS)

#define LOCALTRANSFORM_INSPECTOR_FIELDS(X) \
    X(POSITION, position); \
    X(SCALE, scale); \
    X(ROTATION, rotation); \

BEGIN_COMPONENT(LocalTransform, Components::LOCAL_TRANSFORM, 24)
    SERIALIZE_POSITION(position, GGMath::Vector2(0.0f, 0.0f));
    SERIALIZE_POSITION(scale, GGMath::Vector2(1.0f, 1.0f));
    SERIALIZE_ROTATION(rotation, 0.f);

    uint8_t hierarchyDepth = 0;
    bool isDirty = true;
    uint16_t _padding = 0;
END_COMPONENT_WITH_INSPECTOR(LocalTransform, LOCALTRANSFORM_INSPECTOR_FIELDS)

/*************************************************************************
  \struct WorldTransform
  \brief
    Cached world-space transform matrix for hierarchical transforms.
  \details
    Stores the final world transformation of an entity after combining its
    LocalTransform with parent transforms. Updated by hierarchy systems
    each frame. The isValid flag prevents use of stale matrices during
    partial updates or when hierarchy is modified.
  \warning
    Systems must set isValid = false whenever LocalTransform or parent
    hierarchy changes, ensuring recomputation during TransformSystem update.
  \see
    LocalTransform, HierarchyComponent, TransformSystem
*************************************************************************/
struct WorldTransform {
    static constexpr uint16_t COMPONENT_ID = static_cast<uint16_t>(Components::WORLD_TRANSFORM);
    static constexpr size_t SERIALIZED_SIZE = 40;

    GGMath::Matrix3x3 matrix; 
    bool isValid = false; 
    uint8_t _padding[3] = { 0 }; 
};
static_assert(sizeof(WorldTransform) == WorldTransform::SERIALIZED_SIZE, "SERIA+LIZED_SIZE does not match struct size!");

/*************************************************************************
  \struct HierarchyComponent
  \brief
    Defines parent�child�sibling relationships for scene graph traversal.
  \details
    Implements a lightweight tree structure:
      - parent:      direct parent entity
      - firstChild:  head of linked-list of children
      - nextSibling: next child under the same parent

    Enables O(1) insertion/removal in hierarchical transforms and scene graph
    management. Systems use this to propagate transforms and perform depth
    ordering.
  \note
    Does not enforce acyclic graphs; SceneManager must prevent cycles.
  \see
    LocalTransform, WorldTransform, SceneManager, TransformUpdateSystem
*************************************************************************/
struct HierarchyComponent {
    static constexpr uint16_t COMPONENT_ID = static_cast<uint16_t>(Components::HIERARCHY_COMPONENT);
    static constexpr size_t SERIALIZED_SIZE = 24;

    Entity parent = NULL_ENTITY;
    Entity firstChild = NULL_ENTITY; 
    Entity nextSibling = NULL_ENTITY; 
};
static_assert(sizeof(HierarchyComponent) == HierarchyComponent::SERIALIZED_SIZE, "SERIALIZED_SIZE does not match struct size!");

/*************************************************************************
  \struct ScriptComponent
  \brief
    Attaches a runtime script instance to an ECS entity.
  \details
    Holds references to:
      - scriptAssetID: the script source file (Lua, AngelScript, etc.)
      - scriptInstance: engine-managed script object handle
      - moduleID / startFunctionID: cached IDs for fast callback lookup

    Script systems instantiate, tick, and destroy the script instance
    based on these identifiers. The component itself holds no script logic
    � it only tracks runtime references.
  \warning
    scriptInstance and function IDs are engine-internal and should not be
    manipulated manually.
  \see
    ScriptSystem, AssetManager, Components::SCRIPT_COMPONENT
*************************************************************************/
#define SCRIPT_INSPECTOR_FIELDS(X) \
    X(SCRIPT_ASSET_ID, scriptAssetID); \
    X(SCRIPT_INSTANCE, scriptInstance);

BEGIN_COMPONENT(ScriptComponent, Components::SCRIPT_COMPONENT, 24)
    SERIALIZE_ASSET_ID(scriptAssetID, 0);
    SERIALIZE_FIELD(size_t, scriptInstance, 0);
    int32_t moduleID{ -2 };
    int32_t startFunctionID{ -2 };
    END_COMPONENT(ScriptComponent)

/*************************************************************************
    \struct ECSGridComponent
    \brief
    Defines 2D grid metadata for tilemap or level-layout systems.
    \details
    Holds grid resolution (in tiles) and an internally assigned gridID
    used by systems to reference grid storage. The component does not store
    per-tile data itself � tilemap and grid systems manage that externally.
    \note
    Changing grid dimensions may invalidate cached tilemap data.
    \see
    TilemapComponent, GridSystem, Components::GRID_COMPONENT
*************************************************************************/
#define GRID_INSPECTOR_FIELDS(X) \
    X(UINT, gridSizeX); \
    X(UINT, gridSizeY); \

BEGIN_COMPONENT(ECSGridComponent, Components::GRID_COMPONENT, 12)
    SERIALIZE_FIELD(uint32_t, gridSizeX, 6);
    SERIALIZE_FIELD(uint32_t, gridSizeY, 4);
    uint32_t gridID{};
    END_COMPONENT_WITH_INSPECTOR(ECSGridComponent, GRID_INSPECTOR_FIELDS)

/*************************************************************************
    \struct TilemapComponent
    \brief
    Links an entity to a tilemap asset and its rendering parameters.
    \details
    Stores:
        - tilemapAssetID: reference to tilemap definition (tilesets, layers)
        - perspectiveFactor: depth scaling for pseudo-3D parallax
        - tilemapInstance: runtime-generated handle for tilemap data

    Tilemap systems load and render tiles based on this configuration.
    \see
    TilemapSystem, ECSGridComponent, Components::TILEMAP_COMPONENT
*************************************************************************/
#define TILEMAP_INSPECTOR_FIELDS(X) \
    X(TILEMAP_ASSET_ID, tilemapAssetID); \
    X(FLOAT, perspectiveFactor);

BEGIN_COMPONENT(TilemapComponent, Components::TILEMAP_COMPONENT, 12)
    SERIALIZE_ASSET_ID(tilemapAssetID, 0);
    SERIALIZE_FIELD(GGMath::float32, perspectiveFactor, 0);
    uint32_t tilemapInstance{};
    END_COMPONENT_WITH_INSPECTOR(TilemapComponent, TILEMAP_INSPECTOR_FIELDS)

/*************************************************************************
    \struct AudioSource
    \brief
    ECS component representing a single audio emitter.
    \details
    Holds playback parameters for integration with FMOD:
        - audioAssetID: sound buffer reference
        - outputBus: routing target (Music, SFX, UI, Ambience, etc.)
        - volume, pitch, stereo pan
        - isLooping, playOnAwake, isMuted
        - playbackCursor for seeking + scrubbing
        - runtimeChannel: pointer to active FMOD::Channel

    Audio systems interpret justTriggered to begin playback and update
    isPlaying / isPaused flags based on channel state.
    \warning
    runtimeChannel is not serialized and is only valid during runtime.
    \see
    AudioSystem, GGFMODWrapper, Components::AUDIO_SOURCE
*************************************************************************/
#define AUDIOSOURCE_INSPECTOR_FIELDS(X) \
    X(AUDIO_ASSET_ID, audioAssetID); \
    X(AUDIO_OUTPUT_BUS, outputBus); \
    X(BOOL, isMuted); \
    X(BOOL, playOnAwake); \
    X(BOOL, isLooping); \
    X(AUDIO_VOLUME, volume); \
    X(AUDIO_PITCH, pitch); \
    X(AUDIO_PAN, stereoPan); \

BEGIN_COMPONENT(AudioSource, Components::AUDIO_SOURCE, 56)
    SERIALIZE_ASSET_ID(audioAssetID, 0);
    SERIALIZE_FIELD(uint8_t, outputBus, 0);

    SERIALIZE_FIELD(bool, playOnAwake, false);
    SERIALIZE_FIELD(bool, isLooping, false);
    SERIALIZE_FIELD(bool, isMuted, false);

    SERIALIZE_FIELD(float, volume, 1.f);
    SERIALIZE_FIELD(float, pitch, 1.f);
    SERIALIZE_FIELD(float, stereoPan, 0.f);

    SERIALIZE_FIELD(uint64_t, playbackCursor, 0);
    SERIALIZE_FIELD(bool, justTriggered, false);
    SERIALIZE_FIELD(bool, isPlaying, false);
    SERIALIZE_FIELD(bool, isPaused, false);

    bool hasPlayedOnAwake = false;
    FMOD::Channel* runtimeChannel = nullptr;
    float configVolume = 1.f;

END_COMPONENT_WITH_INSPECTOR(AudioSource, AUDIOSOURCE_INSPECTOR_FIELDS)

// use to recalculate component size
//template<int N> struct ShowSize;
//ShowSize<sizeof(AudioSource)> _;

template<>
struct ComponentTypeInfo<LocalTransform> {
    static constexpr uint16_t GetTypeID() { return static_cast<uint16_t>(Components::LOCAL_TRANSFORM); }
    static constexpr const char* GetTypeName() { return "LocalTransform"; }
};

template<>
struct ComponentTypeInfo<WorldTransform> {
    static constexpr uint16_t GetTypeID() { return static_cast<uint16_t>(Components::WORLD_TRANSFORM); }
    static constexpr const char* GetTypeName() { return "WorldTransform"; }
};
template<>
struct ComponentTypeInfo<HierarchyComponent> {
    static constexpr uint16_t GetTypeID() { return static_cast<uint16_t>(Components::HIERARCHY_COMPONENT); }
    static constexpr const char* GetTypeName() { return "HierarchyComponent"; }
};

template<>
struct ComponentTypeInfo<NameComponent> {
    static constexpr uint16_t GetTypeID() { return static_cast<uint16_t>(Components::NAME_COMPONENT); }
    static constexpr const char* GetTypeName() { return "NameComponent"; }
};

template<>
struct ComponentTypeInfo<ECSGridComponent> {
    static constexpr uint16_t GetTypeID() { return static_cast<uint16_t>(Components::GRID_COMPONENT); }
    static constexpr const char* GetTypeName() { return "GridComponent"; }
};


/*************************************************************************
  \struct TextComponent
  \brief
    Text rendering component with font, shader, and styling properties.
  \details
    Encapsulates text string, font reference, shader program, and text
    styling properties for the text rendering pipeline. Asset references
    use indirection through uint32_t handles resolved by GGECSStandardAssetManager.
    Color uses RGB values for text display.

    The component stores alignment and wrapping parameters for text layout.
    Unlike MeshRenderer, text geometry is procedurally generated by the
    rendering system based on the text string and font metrics.
  \note
    Text buffer is limited to 63 characters (64th byte reserved for null
    terminator). Longer strings will be truncated silently.
  \see
    TextRenderingSystem, GGFont, GGShader, GGECSStandardAssetManager
*************************************************************************/
#define TEXTCOMPONENT_INSPECTOR_FIELDS(X) \
    X(CSTRING_MULTILINE, text); \
    X(SHADER_ASSET_ID, shaderAssetID); \
    X(FONT_ASSET_ID,fontAssetID); \
    X(UINT, fontSize); \
    X(COLOR4, color); \
    X(INT, layer); \
    X(BOOL, ySort); \
    X(ENUM_ALIGNMENT, alignment); \
    X(ENUM_VERTICAL_ALIGNMENT, verticalAlignment); \
    X(FLOAT, maxWidth); \
    X(BOOL, visible);\
    X(FLOAT,lineSpacing);
BEGIN_COMPONENT(TextComponent, Components::TEXT_COMPONENT, 316)
SERIALIZE_CSTRING(text, 256, "text");
//SERIALIZE_CSTRING(fontPath, 64, "Fonts/SourceCodePro-Black.ttf");
SERIALIZE_ASSET_ID(shaderAssetID, 0);
SERIALIZE_ASSET_ID(fontAssetID, 0);
//uint32_t fontID{};

SERIALIZE_FIELD(unsigned int, fontSize, 48);
SERIALIZE_COLOR(glm::vec4, color, glm::vec4(1.f, 1.f, 1.f,1.f));

SERIALIZE_FIELD(int32_t, layer, 0);
SERIALIZE_FIELD(bool, ySort, false);
uint8_t _pad0[3] = { 0 };
enum class Alignment : uint32_t {
    LEFT,
    CENTER,
    RIGHT
};
SERIALIZE_FIELD(uint32_t, alignment, static_cast<uint32_t>(Alignment::LEFT));
enum class VerticalAlignment : uint32_t {
    TOP,
    MIDDLE,
    BOTTOM,
    BASELINE
};
SERIALIZE_FIELD(uint32_t, verticalAlignment, static_cast<uint32_t>(VerticalAlignment::BASELINE));

SERIALIZE_FIELD(float, maxWidth, 0.0f);
SERIALIZE_FIELD(bool, visible, true);
uint8_t _pad1[4] = { 0 };
SERIALIZE_FIELD(float, lineSpacing, 1.2f);
//GLuint const getFontId()const;
std::string const getFontPath()const;
END_COMPONENT_WITH_INSPECTOR(TextComponent, TEXTCOMPONENT_INSPECTOR_FIELDS)


//struct TextComponent {
//    static constexpr uint16_t COMPONENT_ID = static_cast<uint16_t>(Components::TEXT_COMPONENT);
//    static constexpr size_t SERIALIZED_SIZE = 180;
//
//    char text[64] = { 0 };  // Text to display
//    char fontPath[64] = { 0 };
//    uint32_t shaderAssetID{};
//    uint32_t fontId{};
//
//              // Path to //font file
//    unsigned int fontSize = 48;    // Font size in pixels
//
//               
//    glm::vec3 color = { 1.f, 1.f, 1.f }; // Text color (RGB)
//
//    int layer = 0;                 // Render layer
//    bool ySort = false;            // Enable Y-sorting
//    uint8_t _pad0[3] = { 0 };
//
//    // Alignment (relative to entity position)
//    enum class Alignment {
//        LEFT,
//        CENTER,
//        RIGHT
//    } alignment = Alignment::LEFT;
//
//    // Vertical alignment
//    enum class VerticalAlignment {
//        TOP,
//        MIDDLE,
//        BOTTOM,
//        BASELINE  // Use font baseline (default)
//    } verticalAlignment = VerticalAlignment::BASELINE;
//
//    // Optional: text wrapping
//    float maxWidth = 0.0f;  // 0 = no wrapping
//
//    // //Visibility
//    bool visible = true;
//    uint8_t _pad1[4] = { 0 };
//};
//static_assert(sizeof(TextComponent) == TextComponent::SERIALIZED_SIZE, "SERIALIZED_SIZE does not match struct size!");

/*************************************************************************
  \struct WorldTextComponent
  \brief
    Text rendering component that renders in WORLD space (like MeshRenderer),
    NOT in UI/screen space. Used for floating damage numbers, world labels, etc.
  \details
    Unlike TextComponent (which uses GetScreenProjectionMatrix + zoom for UI),
    WorldTextComponent goes through GetActiveProjection + GetActiveView so it
    lives in the same coordinate space as all other world objects.
    Pair with WorldTransform and set position in world coords directly.
  \see
    GGECSMeshRenderer::RenderObjects, TextComponent
*************************************************************************/
#define WORLDTEXTCOMPONENT_INSPECTOR_FIELDS(X) \
    X(CSTRING_MULTILINE, text); \
    X(SHADER_ASSET_ID, shaderAssetID); \
    X(FONT_ASSET_ID,fontAssetID); \
    X(UINT, fontSize); \
    X(COLOR4, color); \
    X(INT, layer); \
    X(BOOL, ySort); \
    X(ENUM_ALIGNMENT, alignment); \
    X(ENUM_VERTICAL_ALIGNMENT, verticalAlignment); \
    X(FLOAT, maxWidth); \
    X(BOOL, visible);\
    X(FLOAT,lineSpacing);

BEGIN_COMPONENT(WorldTextComponent, Components::WORLD_TEXT_COMPONENT, 316)
SERIALIZE_CSTRING(text, 256, "text");
//SERIALIZE_CSTRING(fontPath, 64, "Fonts/SourceCodePro-Black.ttf");
SERIALIZE_ASSET_ID(shaderAssetID, 0);
SERIALIZE_ASSET_ID(fontAssetID, 0);
//uint32_t fontID{};

SERIALIZE_FIELD(unsigned int, fontSize, 48);
SERIALIZE_COLOR(glm::vec4, color, glm::vec4(1.f, 1.f, 1.f, 1.f));

SERIALIZE_FIELD(int32_t, layer, 0);
SERIALIZE_FIELD(bool, ySort, false);
uint8_t _pad0[3] = { 0 };
enum class Alignment : uint32_t {
    LEFT,
    CENTER,
    RIGHT
};
SERIALIZE_FIELD(uint32_t, alignment, static_cast<uint32_t>(Alignment::LEFT));
enum class VerticalAlignment : uint32_t {
    TOP,
    MIDDLE,
    BOTTOM,
    BASELINE
};
SERIALIZE_FIELD(uint32_t, verticalAlignment, static_cast<uint32_t>(VerticalAlignment::BASELINE));

SERIALIZE_FIELD(float, maxWidth, 0.0f);
SERIALIZE_FIELD(bool, visible, true);
SERIALIZE_FIELD(float, lineSpacing, 1.2f);
uint8_t _pad1[4] = { 0 };
//GLuint const getFontId()const;
std::string const getFontPath()const;
END_COMPONENT_WITH_INSPECTOR(WorldTextComponent, WORLDTEXTCOMPONENT_INSPECTOR_FIELDS)

/*************************************************************************
  \struct ObjectEffectComponent
  \brief
    Generic object effect component for visual effects such as fog, bloom,
    shine, outline, dissolve, etc. Can be looped, timed, and customized with
    intensity and color.
*************************************************************************/

#define OBJECTEFFECTCOMPONENT_INSPECTOR_FIELDS(X) \
    X(FLOAT, intensity); \
    X(COLOR4, color); \
    X(FLOAT, effectDuration); \
    X(BOOL, loop); \
    X(EFFECT_TYPE, effectType);\
    X(FLOAT,startTime);

BEGIN_COMPONENT(ObjectEffectComponent, Components::OBJECT_EFFECT_COMPONENT, 36)
// Generic effect intensity / strength
SERIALIZE_FIELD(float, intensity, 1.f);

// Effect color
SERIALIZE_COLOR(glm::vec4, color, glm::vec4(1.f, 1.f, 1.f, 1.f));

SERIALIZE_FIELD(float, effectDuration, 1.f);

// Looping flag
SERIALIZE_FIELD(bool, loop, false);
uint8_t _pad0[3] = { 0 };
// Optional helper enum for effect types
enum EffectType : uint32_t {
    NONE = 0,
    FOG,
    FADE,
    BLOOM,
    OUTLINE,
    SQUARE_DISSOLVE,
    SHINE,
    SHINERTL,
    BURN_DISSOLVE,
    PIXEL_DISSOLVE,
    PULSE
};

SERIALIZE_FIELD(uint32_t, effectType, static_cast<uint32_t>(EffectType::NONE));
SERIALIZE_FIELD(float, startTime, 0.f);
END_COMPONENT_WITH_INSPECTOR(ObjectEffectComponent, OBJECTEFFECTCOMPONENT_INSPECTOR_FIELDS)
