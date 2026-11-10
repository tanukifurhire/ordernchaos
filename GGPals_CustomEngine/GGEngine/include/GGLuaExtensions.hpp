/*************************************************************************
  \file       GGLuaExtensions.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       2 February 2026
  \version    1.2
  \brief
    Extended ECS component bindings and utility systems for Lua.
  \details
    Provides additional ECS components (EntityName, AABB, MeshRenderer, TextComponent),
    query systems for entity searches, audio playback, and time utilities.
    These extensions enable complete game logic implementation in Lua
    without requiring C++ gameplay code.
  \dependencies
    lua.h, GGECSWorld.hpp, GGECSComponents.hpp, GGAudioManager.hpp, GGTime.hpp
  \see
    GGLuaBindings, GGECSWorld, GGAudioManager
  \history
    - 1.0 (1 November 2025): Initial implementation.
    - 1.1 (31 January 2026): Added TextComponent bindings for dynamic text
                             rendering and editing from Lua scripts.
    - 1.2 (2 February 2026): Exposed fontAssetID and shaderAssetID in
                             TextComponent bindings for dynamic font switching.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include <cstdint>
#include "GGECSInterface.hpp"

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

// Forward declarations
class World;
class GGLuaVM;

void SetWorldBinding(GGLuaVM& luaVM, World& world);

/*************************************************************************
  \brief
    Registers extended ECS component bindings.
  \param[in] luaVM
    Lua VM to register functions in.
  \param[in] world
    ECS World instance for component operations.
  \details
    Extends the global "ECS" table with additional component types and
    query systems. Does not replace existing bindings.
  \par Components Added
    - EntityName: Named entities for debugging and lookup
    - AABB: Axis-aligned bounding boxes for collision
    - MeshRenderer: Sprite/mesh rendering control
  \par Query Systems Added
    - Entity searches by component type
    - Entity count and enumeration
  \note
    Must be called AFTER RegisterECSBindings() as it extends the
    existing ECS table rather than creating a new one.
*************************************************************************/
void RegisterExtendedECSBindings(GGLuaVM& luaVM, World& world);

/*************************************************************************
  \brief
    Registers Audio API in Lua global namespace.
  \param[in] luaVM
    Lua VM to register functions in.
  \details
    Creates global "Audio" table for sound effect and music playback.
    Handles are integers that can be stored for later manipulation.
  \par Usage
  \code
    RegisterAudioBindings(luaVM);

    // In Lua:
    local shotSound = Audio.playSound("laser.wav", 0.8)
    Audio.playMusic("battle_theme.mp3", 0.5, true)
  \endcode
*************************************************************************/
void RegisterAudioBindings(GGLuaVM& luaVM);

/*************************************************************************
  \brief
    Registers Time API in Lua global namespace.
  \param[in] luaVM
    Lua VM to register functions in.
  \details
    Creates global "Time" table for frame timing and profiling.
    All time values are in seconds unless otherwise specified.
*************************************************************************/
void RegisterTimeBindings(GGLuaVM& luaVM);

/*************************************************************************
  ENTITYNAME COMPONENT FUNCTIONS
  Provides named entity support for debugging and lookup
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.addEntityName(entity, name)
  \param[in] 1 (Lua stack)
    Entity handle (lightuserdata).
  \param[in] 2
    Name string (must be unique).
  \details
    Adds EntityName component with specified name. Names must be unique
    across all entities. Overwrites existing name if present.
  \par Lua Usage
  \code
    local player = ECS.createEntity()
    ECS.addEntityName(player, "Player1")
  \endcode
*************************************************************************/
int lua_ECS_AddEntityName(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.getEntityName(entity) -> string
  \param[in] 1 (Lua stack)
    Entity handle.
  \return (Lua stack)
    Name string or nil if no name.
  \par Lua Usage
  \code
    local name = ECS.getEntityName(entity)
    print("Entity name:", name or "unnamed")
  \endcode
*************************************************************************/
int lua_ECS_GetEntityName(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.setEntityName(entity, name)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    New name string.
  \details
    Changes existing EntityName. Entity must already have the component.
  \par Lua Usage
  \code
    ECS.setEntityName(player, "Player1_Dead")
  \endcode
*************************************************************************/
int lua_ECS_SetEntityName(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.findEntityByName(name) -> entity or nil
  \param[in] 1 (Lua stack)
    Name string to search for.
  \return (Lua stack)
    Entity handle or nil if not found.
  \details
    Linear search through all entities with EntityName component.
    Returns first match found.
  \par Lua Usage
  \code
    local player = ECS.findEntityByName("Player1")
    if player then
        -- Found the player entity
    end
  \endcode
*************************************************************************/
int lua_ECS_FindEntityByName(lua_State* L);

/*************************************************************************
  AABB COLLISION COMPONENT FUNCTIONS
  Axis-aligned bounding boxes for 2D collision detection
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.addAABB(entity, halfWidth, halfHeight)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Half width (number).
  \param[in] 3
    Half height (number).
  \details
    Adds AABBFloat component with specified half-extents. AABB is
    centered on entity's transform position.
  \par Lua Usage
  \code
    -- 32x32 sprite with AABB
    ECS.addAABB(entity, 16, 16)
  \endcode
*************************************************************************/
int lua_ECS_AddAABB(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.getAABB(entity) -> {halfWidth, halfHeight}
  \param[in] 1 (Lua stack)
    Entity handle.
  \return (Lua stack)
    Table with halfWidth, halfHeight fields.
  \par Lua Usage
  \code
    local aabb = ECS.getAABB(entity)
    print("Size:", aabb.halfWidth * 2, "x", aabb.halfHeight * 2)
  \endcode
*************************************************************************/
int lua_ECS_GetAABB(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.setAABB(entity, table)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Table with halfWidth, halfHeight fields.
  \details
    Updates existing AABB component dimensions.
  \par Lua Usage
  \code
    ECS.setAABB(entity, {halfWidth = 20, halfHeight = 20})
  \endcode
*************************************************************************/
int lua_ECS_SetAABB(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.checkAABBOverlap(entity1, entity2) -> boolean
  \param[in] 1 (Lua stack)
    First entity handle.
  \param[in] 2
    Second entity handle.
  \return (Lua stack)
    Boolean true if AABBs overlap.
  \details
    Both entities must have Transform2DFloat and AABBFloat components.
    Uses transform positions to place AABBs in world space.
  \par Lua Usage
  \code
    if ECS.checkAABBOverlap(player, enemy) then
        -- Collision detected
        takeDamage()
    end
  \endcode
*************************************************************************/
int lua_ECS_CheckAABBOverlap(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.getWorldPosition(entity) -> {x, y} or nil
  \param[in] 1 (Lua stack)
    Entity handle.
  \return (Lua stack)
    Table with x, y fields representing world position, or nil if entity
    does not have a WorldTransform component.
  \details
    Returns the world position extracted from the entity's WorldTransform
    matrix. This is the final computed position after hierarchy transforms.
  \par Lua Usage
  \code
    local pos = ECS.getWorldPosition(entity)
    if pos then
        print("World position:", pos.x, pos.y)
    else
        print("Entity has no WorldTransform")
    end
  \endcode
*************************************************************************/
int lua_ECS_GetWorldPosition(lua_State* L);

/*************************************************************************
  HIERARCHY FUNCTIONS
  Controls entity parent-child relationships from Lua
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.setParent(parent, child)
  \param[in] 1 (Lua stack)
    Entity handle for the parent entity
  \param[in] 2 (Lua stack)
    Entity handle for the child entity
  \details
    Sets the parent of the child entity. Both entities must have
    LocalTransform components. Handles cycle detection and unlinking
    from any previous parent.
  \par Lua Usage
  \code
    local parent = ECS.findEntityByName("ParentObj")
    local child  = ECS.findEntityByName("ChildObj")
    ECS.setParent(parent, child)
  \endcode
*************************************************************************/
int lua_ECS_SetParent(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.getChildren(entity) -> table
  \param[in] 1 (Lua stack)
    Entity handle for the parent entity
  \return
    A Lua table (array) of entity handles for all direct children
  \details
    Returns a table containing the entity handles of all direct children
    of the given entity. Returns an empty table if the entity has no
    children or no HierarchyComponent.
  \par Lua Usage
  \code
    local children = ECS.getChildren(parent)
    for i, child in ipairs(children) do
        -- do something with child
    end
  \endcode
*************************************************************************/
int lua_ECS_GetChildren(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.destroyEntity(entity)
  \param[in] 1 (Lua stack)
    Entity handle to destroy
  \details
    Immediately destroys the entity and all its descendants. Properly
    unlinks from parent hierarchy and recursively destroys children.
    Do NOT call this during a ForEach iteration.
  \par Lua Usage
  \code
    ECS.destroyEntity(entity)
  \endcode
*************************************************************************/
int lua_ECS_DestroyEntity(lua_State* L);

/*************************************************************************
  MESHRENDERER COMPONENT FUNCTIONS
  Controls sprite/mesh rendering from Lua
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.addMeshRenderer(entity, shaderID)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Shader asset ID (integer).
  \param[in] 3
    Texture asset ID (integer, optional).
  \details
    Adds MeshRenderer component for sprite rendering. Shader ID is
    required, texture ID can be 0 for untextured.
  \par Lua Usage
  \code
    ECS.addMeshRenderer(entity, SPRITE_SHADER)
  \endcode
*************************************************************************/
int lua_ECS_AddMeshRenderer(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.getMeshRenderer(entity) -> table
  \param[in] 1 (Lua stack)
    Entity handle.
  \return (Lua stack)
    Table with shaderAssetID, textureAssetID, color fields.
  \par Lua Usage
  \code
    local renderer = ECS.getMeshRenderer(entity)
    print("Using texture:", renderer.textureAssetID)
  \endcode
*************************************************************************/
int lua_ECS_GetMeshRenderer(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.setMeshRenderer(entity, table)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Table with optional fields: shaderAssetID, textureAssetID, color.
  \details
    Updates MeshRenderer properties. Color is table with r,g,b,a fields.
  \par Lua Usage
  \code
    -- Change to hit texture and red tint
    ECS.setMeshRenderer(entity, {
        textureAssetID = HIT_TEXTURE,
        color = {r = 1, g = 0, b = 0, a = 1}
    })
  \endcode
*************************************************************************/
int lua_ECS_SetMeshRenderer(lua_State* L);

int lua_ECS_AddSprite(lua_State* L);

int lua_ECS_GetSprite(lua_State* L);

int lua_ECS_SetSprite(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.addMeshRenderer(entity, shaderID, textureID)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Shader asset ID (integer).
  \param[in] 3
    Texture asset ID (integer, optional).
  \details
    Adds MeshRenderer component for sprite rendering. Shader ID is
    required, texture ID can be 0 for untextured.
  \par Lua Usage
  \code
    ECS.addMeshRenderer(entity, SPRITE_SHADER, PLAYER_TEXTURE)
  \endcode
*************************************************************************/
int lua_ECS_AddUIComponent(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.getMeshRenderer(entity) -> table
  \param[in] 1 (Lua stack)
    Entity handle.
  \return (Lua stack)
    Table with shaderAssetID, textureAssetID, color fields.
  \par Lua Usage
  \code
    local renderer = ECS.getMeshRenderer(entity)
    print("Using texture:", renderer.textureAssetID)
  \endcode
*************************************************************************/
int lua_ECS_GetUIComponent(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.setMeshRenderer(entity, table)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Table with optional fields: shaderAssetID, textureAssetID, color.
  \details
    Updates MeshRenderer properties. Color is table with r,g,b,a fields.
  \par Lua Usage
  \code
    -- Change to hit texture and red tint
    ECS.setMeshRenderer(entity, {
        textureAssetID = HIT_TEXTURE,
        color = {r = 1, g = 0, b = 0, a = 1}
    })
  \endcode
*************************************************************************/
int lua_ECS_SetUIComponent(lua_State* L);

/*************************************************************************
  GRID COMPONENT FUNCTIONS
  Get grid references from Lua
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.addGridComponent(entity, gridSizeX, gridSizeY)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Grid Size X (integer)
  \param[in] 3
    Grid Size Y (integer)
  \details
    Adds Grid Component to entity, specifying gridSizeX and Y
  \par Lua Usage
  \code
    ECS.addGridComponent(entity, 6, 4)
  \endcode
*************************************************************************/
int lua_ECS_AddGridComponent(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.getGridComponent(entity) -> table
  \param[in] 1 (Lua stack)
    Entity handle.
  \return (Lua stack)
    Table with gridID, gridSizeX, gridSizeY fields.
  \par Lua Usage
  \code
    local grid = ECS.getGridComponent(entity)
    print("Using grid:", grid.gridID)
  \endcode
*************************************************************************/
int lua_ECS_GetGridComponent(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.setGridComponent(entity, table)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Table with optional fields: gridSizeX, gridSizeY.
  \details
    Updates Grid Component properties.
  \par Lua Usage
  \code
    -- Change to hit texture and red tint
    ECS.setGridComponent(entity, {
        gridSizeX = 6,
        gridSizeY = 4
    })
  \endcode
*************************************************************************/
int lua_ECS_SetGridComponent(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.getTilemapComponent(entity) -> table
  \param[in] 1 (Lua stack)
    Entity handle.
  \return (Lua stack)
    Table with tilemapID and perspectiveFactor fields.
  \par Lua Usage
  \code
    local tilemap = ECS.getTilemapComponent(entity)
    print("Using grid:", grid.gridID)
  \endcode
*************************************************************************/
int lua_ECS_GetTilemapComponent(lua_State* L);

/*************************************************************************
  TEXTCOMPONENT FUNCTIONS
  Dynamic text rendering and styling
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.addTextComponent(entity, text, fontAssetID, fontSize)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Text string (up to 255 characters).
  \param[in] 3
    Font asset ID (integer).
  \param[in] 4
    Font size (optional, default 48).
  \details
    Adds TextComponent for text rendering with font atlas. Initializes
    with white color, default alignment, and visible=true.
  \par Lua Usage
  \code
    ECS.addTextComponent(entity, "Hello World", 5, 48)
  \endcode
*************************************************************************/
int lua_ECS_AddTextComponent(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.getTextComponent(entity) -> table
  \param[in] 1 (Lua stack)
    Entity handle.
  \return (Lua stack)
    Table with text, fontAssetID, shaderAssetID, fontSize, color, layer,
    visible, alignment, verticalAlignment, maxWidth, ySort fields.
  \par Lua Usage
  \code
    local textData = ECS.getTextComponent(entity)
    print("Text:", textData.text)
    print("Color:", textData.color.r, textData.color.g, textData.color.b)
  \endcode
*************************************************************************/
int lua_ECS_GetTextComponent(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.setTextComponent(entity, table)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Table with optional fields: text, fontAssetID, shaderAssetID, fontSize,
    color, layer, visible, alignment, verticalAlignment, maxWidth, ySort.
  \details
    Updates TextComponent properties. Only provided fields are modified.
    Color is a table {r, g, b} with 0-1 range.
    Alignment: 0=LEFT, 1=CENTER, 2=RIGHT
    VerticalAlignment: 0=TOP, 1=MIDDLE, 2=BOTTOM, 3=BASELINE
  \par Lua Usage
  \code
    -- Update text content and color
    ECS.setTextComponent(entity, {
        text = "New Text",
        color = {r=1, g=0, b=0},
        fontSize = 64,
        visible = true
    })
  \endcode
*************************************************************************/
int lua_ECS_SetTextComponent(lua_State* L);

/*************************************************************************
  WORLDTEXTCOMPONENT FUNCTIONS
  Dynamic text rendering and styling
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.addTextComponent(entity, text, fontAssetID, fontSize)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Text string (up to 255 characters).
  \param[in] 3
    Font asset ID (integer).
  \param[in] 4
    Font size (optional, default 48).
  \details
    Adds TextComponent for text rendering with font atlas. Initializes
    with white color, default alignment, and visible=true.
  \par Lua Usage
  \code
    ECS.addTextComponent(entity, "Hello World", 5, 48)
  \endcode
*************************************************************************/
int lua_ECS_AddWorldTextComponent(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.getTextComponent(entity) -> table
  \param[in] 1 (Lua stack)
    Entity handle.
  \return (Lua stack)
    Table with text, fontAssetID, shaderAssetID, fontSize, color, layer,
    visible, alignment, verticalAlignment, maxWidth, ySort fields.
  \par Lua Usage
  \code
    local textData = ECS.getTextComponent(entity)
    print("Text:", textData.text)
    print("Color:", textData.color.r, textData.color.g, textData.color.b)
  \endcode
*************************************************************************/
int lua_ECS_GetWorldTextComponent(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.setTextComponent(entity, table)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Table with optional fields: text, fontAssetID, shaderAssetID, fontSize,
    color, layer, visible, alignment, verticalAlignment, maxWidth, ySort.
  \details
    Updates TextComponent properties. Only provided fields are modified.
    Color is a table {r, g, b} with 0-1 range.
    Alignment: 0=LEFT, 1=CENTER, 2=RIGHT
    VerticalAlignment: 0=TOP, 1=MIDDLE, 2=BOTTOM, 3=BASELINE
  \par Lua Usage
  \code
    -- Update text content and color
    ECS.setTextComponent(entity, {
        text = "New Text",
        color = {r=1, g=0, b=0},
        fontSize = 64,
        visible = true
    })
  \endcode
*************************************************************************/
int lua_ECS_SetWorldTextComponent(lua_State* L);

/*************************************************************************
  QUERY SYSTEM FUNCTIONS
  Find and enumerate entities
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.getEntitiesWithComponent(componentName) -> {entities}
  \param[in] 1 (Lua stack)
    Component type name (string).
  \return (Lua stack)
    Array table of entity handles.
  \details
    Returns all entities that have the specified component type.
    Supported types: LocalTransform, Rigidbody2DFloat, AABBFloat,
    MeshRenderer, EntityName.
  \par Lua Usage
  \code
    local enemies = ECS.getEntitiesWithComponent("EnemyAI")
    for i, enemy in ipairs(enemies) do
        updateEnemy(enemy)
    end
  \endcode
*************************************************************************/
int lua_ECS_GetEntitiesWithComponent(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.getEntityCount() -> number
  \return (Lua stack)
    Total number of active entities.
  \par Lua Usage
  \code
    local count = ECS.getEntityCount()
    print("Active entities:", count)
  \endcode
*************************************************************************/
int lua_ECS_GetEntityCount(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.getAllEntities() -> {entities}
  \return (Lua stack)
    Array table of all entity handles.
  \details
    Returns every active entity in the world. Use sparingly as this
    can be expensive with many entities.
  \par Lua Usage
  \code
    local all = ECS.getAllEntities()
    for i, entity in ipairs(all) do
        -- Process every entity
    end
  \endcode
*************************************************************************/
int lua_ECS_GetAllEntities(lua_State* L);

/*************************************************************************
  AUDIO SYSTEM FUNCTIONS
*************************************************************************/

/*************************************************************************
  \brief
    Lua: Audio.playSound(assetPath, volume) -> handle
  \param[in] 1 (Lua stack)
    Asset path or name (string).
  \param[in] 2
    Volume 0.0 to 1.0 (number).
  \return (Lua stack)
    Sound handle for later control.
  \par Lua Usage
  \code
    local handle = Audio.playSound("explosion.wav", 0.8)
  \endcode
*************************************************************************/
int lua_Audio_PlaySound(lua_State* L);

/*************************************************************************
  \brief
    Lua: Audio.stopSound(handle)
  \param[in] 1 (Lua stack)
    Sound handle from playSound.
  \par Lua Usage
  \code
    Audio.stopSound(explosionHandle)
  \endcode
*************************************************************************/
int lua_Audio_StopSound(lua_State* L);

/*************************************************************************
  \brief
    Lua: Audio.setVolume(handle, volume)
  \param[in] 1 (Lua stack)
    Sound handle.
  \param[in] 2
    Volume 0.0 to 1.0.
  \par Lua Usage
  \code
    Audio.setVolume(musicHandle, 0.3)
  \endcode
*************************************************************************/
int lua_Audio_SetVolume(lua_State* L);

/*************************************************************************
  \brief
    Lua: Audio.playMusic(assetPath, volume, loop) -> handle
  \param[in] 1 (Lua stack)
    Asset path (string).
  \param[in] 2
    Volume 0.0 to 1.0.
  \param[in] 3
    Loop boolean.
  \return (Lua stack)
    Music handle.
  \par Lua Usage
  \code
    Audio.playMusic("battle_theme.ogg", 0.5, true)
  \endcode
*************************************************************************/
int lua_Audio_PlayMusic(lua_State* L);

/*************************************************************************
  \brief
    Lua: Audio.stopMusic()
  \details
    Stops all currently playing music tracks.
  \par Lua Usage
  \code
    Audio.stopMusic()
  \endcode
*************************************************************************/
int lua_Audio_StopMusic(lua_State* L);

/*************************************************************************
  TIME SYSTEM FUNCTIONS
*************************************************************************/

/*************************************************************************
  \brief
    Lua: Time.getDeltaTime() -> number
  \return (Lua stack)
    Seconds since last frame.
  \par Lua Usage
  \code
    local dt = Time.getDeltaTime()
    entity.x = entity.x + velocity * dt
  \endcode
*************************************************************************/
int lua_Time_GetDeltaTime(lua_State* L);

/*************************************************************************
  \brief
    Lua: Time.getTotalTime() -> number
  \return (Lua stack)
    Seconds since engine start.
  \par Lua Usage
  \code
    local elapsed = Time.getTotalTime()
    local wave = math.sin(elapsed * 2)
  \endcode
*************************************************************************/
int lua_Time_GetTotalTime(lua_State* L);

/*************************************************************************
  \brief
    Lua: Time.getFPS() -> number
  \return (Lua stack)
    Current frames per second.
  \par Lua Usage
  \code
    local fps = Time.getFPS()
    if fps < 30 then
        reduceGraphicsQuality()
    end
  \endcode
*************************************************************************/
int lua_Time_GetFPS(lua_State* L);

/*************************************************************************
  \brief
    Lua: Time.getFrameCount() -> number
  \return (Lua stack)
    Total frames rendered since start.
  \par Lua Usage
  \code
    -- Animate every 10 frames
    if Time.getFrameCount() % 10 == 0 then
        nextAnimationFrame()
    end
  \endcode
*************************************************************************/
int lua_Time_GetFrameCount(lua_State* L);


/*************************************************************************
  OBJECTEFFECTCOMPONENT FUNCTIONS
  Visual effects: fog, fade, bloom, outline, dissolve, shine
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.addObjectEffectComponent(entity, effectType)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Effect type integer (optional, default 0 = NONE).
    Use ECS.EffectType constants for readability.
  \details
    Adds ObjectEffectComponent with default values (intensity=1, duration=1,
    loop=false, white color). Use setObjectEffectComponent to configure
    after adding.
  \par Lua Usage
  \code
    ECS.addObjectEffectComponent(entity)
    ECS.addObjectEffectComponent(entity, ECS.EffectType.SHINE)
  \endcode
*************************************************************************/
int lua_ECS_AddObjectEffectComponent(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.getObjectEffectComponent(entity) -> table
  \param[in] 1 (Lua stack)
    Entity handle.
  \return (Lua stack)
    Table with effectType, intensity, effectDuration, loop, color fields.
  \par Return Table Fields
    - effectType (integer): Active effect (use ECS.EffectType constants)
    - intensity (number): Effect strength
    - effectDuration (number): Duration in seconds
    - loop (boolean): Whether effect loops
    - color (table): Effect color {r, g, b, a} with values 0.0-1.0
  \par Lua Usage
  \code
    local fx = ECS.getObjectEffectComponent(entity)
    print("Effect:", fx.effectType)
    print("Intensity:", fx.intensity)
    print("Color:", fx.color.r, fx.color.g, fx.color.b, fx.color.a)
  \endcode
*************************************************************************/
int lua_ECS_GetObjectEffectComponent(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.setObjectEffectComponent(entity, table)
  \param[in] 1 (Lua stack)
    Entity handle.
  \param[in] 2
    Table with optional ObjectEffectComponent properties to update.
  \details
    Updates ObjectEffectComponent properties. Only fields present in the
    table are modified - omitted fields retain their current values.
  \par Supported Table Fields (all optional)
    - effectType (integer): Use ECS.EffectType constants
    - intensity (number): Effect strength (e.g. shine band width, fog density)
    - effectDuration (number): Duration in seconds
    - loop (boolean): Loop the effect
    - color (table): Effect color {r, g, b, a} with values 0.0-1.0
  \par ECS.EffectType Constants
    - ECS.EffectType.NONE
    - ECS.EffectType.FOG
    - ECS.EffectType.FADE
    - ECS.EffectType.BLOOM
    - ECS.EffectType.OUTLINE
    - ECS.EffectType.SQUARE_DISSOLVE
    - ECS.EffectType.SHINE
  \par Lua Usage
  \code
    -- Looping shine effect
    ECS.setObjectEffectComponent(entity, {
        effectType     = ECS.EffectType.SHINE,
        intensity      = 0.15,
        effectDuration = 1.5,
        loop           = true,
        color          = { r = 1.0, g = 1.0, b = 1.0, a = 1.0 }
    })

    -- One-shot fade out
    ECS.setObjectEffectComponent(entity, {
        effectType     = ECS.EffectType.FADE,
        effectDuration = 2.0,
        loop           = false
    })

    -- Outline with custom color
    ECS.setObjectEffectComponent(entity, {
        effectType = ECS.EffectType.OUTLINE,
        intensity  = 2.0,
        color      = { r = 1.0, g = 0.0, b = 0.0, a = 1.0 }
    })

    -- Disable effect
    ECS.setObjectEffectComponent(entity, { effectType = ECS.EffectType.NONE })
  \endcode
*************************************************************************/
int lua_ECS_SetObjectEffectComponent(lua_State* L);

void RegisterObjectEffectsBindings(GGLuaVM& luaVM);

/*************************************************************************
  SCRIPT INSTANCE FUNCTIONS
  Access script component data ("self") from external Lua code
*************************************************************************/

/*************************************************************************
  \brief
    Lua: ECS.getScriptInstance(entity) -> userdata or nil
  \param[in] 1 (Lua stack)
    Entity handle (lightuserdata).
  \return (Lua stack)
    The script instance userdata (same as "self" inside script callbacks)
    with the script type's metatable set, or nil if the entity has no
    ScriptComponent or its instance is not yet initialized.
  \details
    Retrieves the script component's runtime instance data for the given
    entity. The returned userdata behaves exactly like the "self" argument
    passed to script lifecycle functions (Init, Start, Update, etc.),
    allowing external scripts to read and write another entity's script
    fields via the type's __index/__newindex metamethods.
  \par Lua Usage
  \code
    -- Access another entity's script data from outside
    local enemy = ECS.findEntityByName("Boss")
    local enemySelf = ECS.getScriptInstance(enemy)
    if enemySelf then
        print("Boss health:", enemySelf.health)
        enemySelf.health = enemySelf.health - 10
    end
  \endcode
  \note
    The returned userdata is a fresh wrapper pointing to the same
    underlying EntityInstance. Changes made through it are immediately
    visible to the script's own "self".
*************************************************************************/
int lua_ECS_GetScriptInstance(lua_State* L);

/*************************************************************************
  \brief
    Lua: ECS.isEntityAlive(entity) -> boolean
  \param[in] 1 (Lua stack)
    Entity handle (lightuserdata).
  \return (Lua stack)
    True if the entity handle is still valid (not destroyed), false
    otherwise. Does NOT raise a Lua error for invalid handles.
  \details
    Safe validity check for entity handles. Unlike most ECS functions
    which call luaL_error on invalid handles, this returns false so it
    can be used as a guard in event callbacks that may fire after the
    entity has been destroyed.
  \par Lua Usage
  \code
    Event.subscribe("SomeEvent", function(params)
        if not ECS.isEntityAlive(entity) then return end
        self.value = params.value
    end)
  \endcode
*************************************************************************/
int lua_ECS_IsEntityAlive(lua_State* L);
