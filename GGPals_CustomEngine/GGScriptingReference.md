# GGEngine Scripting Reference

## Table of Contents

- [Introduction](#introduction)
- [Script Type System](#script-type-system)
  - [DefineType](#definetype)
  - [Composition ⭐ NEW](#composition)
- [Entity Component System (ECS)](#entity-component-system-ecs)
  - [Entity Management](#entity-management)
  - [Transform Component](#transform-component)
  - [Rigidbody Component](#rigidbody-component)
  - [AABB Component](#aabb-component)
  - [MeshRenderer Component](#meshrenderer-component)
  - [EntityName Component](#entityname-component)
  - [Grid Component](#grid-component)
  - [Animation Component](#animation-component)
  - [Tilemap Component](#tilemap-component)
  - [Query System](#query-system)
- [Event System](#event-system)
  - [Event.define](#eventdefine)
  - [Event.subscribe](#eventsubscribe)
  - [Event.emitLocal](#eventemitlocal)
  - [Event.emitNetworked](#eventemitnetworked)
- [Scenes](#scenes)
  - [Scene Switching](#scene-switching)
- [Prefabs](#prefabs)
  - [Prefab Spawning](#prefab-spawning)
- [Input System](#input-system)
  - [Keyboard Input](#keyboard-input)
  - [Mouse Input](#mouse-input)
- [Grid System](#grid-system)
  - [Grid Management](#grid-management)
  - [Tile Queries](#tile-queries)
  - [Pathfinding](#pathfinding)
  - [Coordinate Conversion](#coordinate-conversion)
  - [Tile Operations](#tile-color-operations)
- [Networking (RPC)](#networking-rpc)
  - [RPC.startServer](#rpcstartserver)
  - [RPC.connectToServer](#rpcconnecttoserver)
  - [RPC.call](#rpccall)
- [Audio System](#audio-system)
  - [Sound Effects](#sound-effects)
  - [Music](#music)
- [Time System](#time-system)
- [Debug & Logging](#debug--logging)
  - [Logging](#logging)
  - [Profiling](#profiling)
  - [Debug Utilities](#debug-utilities)

---

## Introduction

GGEngine uses **Lua** as its primary scripting language, providing a flexible and performant gameplay programming layer without requiring C++ compilation. The engine follows an **Entity-Component-System (ECS)** architecture where Lua scripts define custom component types and behavior logic.

**Core Architecture:**
- **DefineType** creates script component classes with serializable fields, lifecycle methods (Start/Update), and custom functions
- **ECS bindings** provide direct access to engine components (Transform, Rigidbody, etc.) via table-based APIs
- **Event system** enables decoupled communication between scripts using type-safe schemas
- **RPC system** handles networked function calls for multiplayer synchronization using deterministic client-server architecture
- **Grid system** powers turn-based gameplay with pathfinding, tile manipulation, and win condition checking

**Execution Model:**
Scripts are attached to entities via `ScriptComponent`. The engine calls `Start()` once on entity creation and `Update()` every frame. All Lua code runs on the main thread with protected calls (no engine crashes from script errors). Component data is copied between Lua and C++ as tables—modifications require explicit `setComponent()` calls.

**Memory Model:**
Entity handles are lightuserdata (uint64_t) packing ID + generation counter. Components are small POD structs (8-20 bytes) copied by value. The engine validates all entity handles on Lua→C++ crossing to prevent use-after-free. Lua's garbage collector manages script objects independently from ECS entities.

[↑ Back to Top](#ggengine-scripting-reference)

---

## Script Type System

### DefineType

**Signature:** `DefineType(typeName, definition) -> class_table`

Creates a custom script component type with serializable fields and lifecycle methods. Returns a class table that can be instantiated by the ECS system when attached to entities.

**Parameters:**
- `typeName` (string) — Unique identifier for this script type (e.g., "PlayerController")
- `definition` (table) — Type definition with the following structure:
  - `Serializable` (table, optional) — Fields visible in Inspector and saved to disk
  - `Contains` (table, optional) — **Composition fields** — embed another script type as a named sub-instance (see [Composition](#composition))
  - Regular fields — Non-serialized variables (initialized per instance)
  - `Start` (function, optional) — Called once when entity is created: `function(self, entity)`
  - `Update` (function, optional) — Called every frame: `function(self, entity)`
  - Custom methods — Any additional functions: `function(self, entity, ...args)`

**Return Value:**
- Class table with metatable configured for instantiation

**Usage Example:**
```lua
local class = DefineType("PlayerController",
{
    -- Serializable fields (editable in Inspector)
    Serializable = 
    {
        moveSpeed = 100,      -- number
        health = 100,         -- number
        playerName = "Hero",  -- string
        isInvincible = false  -- boolean
    },
    
    -- Non-serializable runtime state
    isGrounded = false,
    jumpCooldown = 0,
    
    -- Lifecycle: called once on entity creation
    Start = function(self, entity)
        Log.info("Player initialized: " .. self.playerName)
        
        -- Subscribe to events
        Event.subscribe("OnDamage", function(params)
            PlayerController.TakeDamage(self, entity, params.amount)
        end)
    end,
    
    -- Lifecycle: called every frame
    Update = function(self, entity)
        -- Reduce cooldown
        if self.jumpCooldown > 0 then
            self.jumpCooldown = self.jumpCooldown - Time.getDeltaTime()
        end
        
        -- Handle movement input
        if Input.isKeyPressed(KEY_W) then
            PlayerController.Move(self, entity, 0, self.moveSpeed)
        end
        
        if Input.isKeyTriggered(KEY_SPACE) and self.jumpCooldown <= 0 then
            PlayerController.Jump(self, entity)
        end
    end,
    
    -- Custom methods
    Move = function(self, entity, dx, dy)
        local transform = ECS.getTransform(entity)
        if transform then
            local dt = Time.getDeltaTime()
            transform.x = transform.x + dx * dt
            transform.y = transform.y + dy * dt
            ECS.setTransform(entity, transform)
        end
    end,
    
    Jump = function(self, entity)
        if self.isGrounded then
            local rb = ECS.getRigidbody(entity)
            rb.vy = -500  -- Negative is up in screen coordinates
            ECS.setRigidbody(entity, rb)
            self.jumpCooldown = 0.5
        end
    end,
    
    TakeDamage = function(self, entity, amount)
        if not self.isInvincible then
            self.health = self.health - amount
            if self.health <= 0 then
                Event.emitLocal("OnPlayerDeath", {player = entity})
            end
        end
    end
})

return class
```

**Serializable Field Types:**
- `number` — Floating-point or integer values
- `string` — Text strings
- `boolean` — true/false values

**Important Notes:**
- Field names must be unique within the type (no redefinition)
- **Functions**, **threads**, and **userdata** cannot be serialized
- Tentatively, **tables** cannot be defined as variables (to be fixed)
- `self` refers to the instance, `entity` is the owning entity handle
- Custom methods should use `TypeName.MethodName(self, entity, ...)` pattern
- Script files must `return` the class table at the end

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Script Type System](#script-type-system)

---

## Composition

> ⭐ **New Feature** — Composition allows a script type to embed another fully-functional script type as a named member field. The engine automatically creates the sub-instance when the parent is instantiated, and the sub-instance is accessible as a first-class object that supports method calls, field access, and nested composition chains.

### Overview

Composition is declared using the `Contains` block inside `DefineType`. Each entry maps a **field name** to a **type name string**. The referenced type must itself be a type registered via `DefineType` (in any loaded script file).

```lua
Contains =
{
    fieldName = "AnotherTypeName"
}
```

At runtime the engine:
1. Allocates a full `EntityInstance` for each composition field.
2. Recursively creates sub-instances for any nested `Contains` fields in the composed type.
3. Stores the sub-instance as a Lua userdata in the parent's Lua fields table, accessible by the declared field name.

The composed field behaves exactly like any other script instance — you can read/write its fields and call its methods using `:` syntax.

### Rules & Constraints

- The value in `Contains` must be a **string** (the type name) — not a table reference.
- The referenced type name must be registered via `DefineType` in a loaded script. Missing types produce a **warning** at startup; the field will be `nil` at runtime.
- **Circular composition is not allowed** (e.g., type A containing type B that contains type A). The engine detects cycles at startup and logs an error.
- Field names in `Contains` must be unique across the entire type definition (no redefinition with `Serializable`, `TextureAsset`, `AudioAsset`, or regular fields).
- Composed sub-instances are **not serialized** to disk — they are always freshly constructed from their own type defaults at instantiation time.

### Usage Example — Simple Composition

```lua
-- Define a sub-type first (can be in a separate file)
local stateMachine = DefineType("MyStateMachine",
{
    currentState = nil,

    Init = function(self)
        self.currentState = "idle"
    end,

    ChangeState = function(self, newState)
        self.currentState = newState
    end
})

return stateMachine
```

```lua
-- Parent type embeds MyStateMachine via Contains
local class = DefineType("MyCharacter",
{
    Contains =
    {
        stateMachine = "MyStateMachine"   -- field name = type name string
    },

    Serializable =
    {
        health = 100
    },

    Init = function(self, entity)
        self.stateMachine:Init()          -- call methods on the sub-instance
    end,

    Update = function(self, entity)
        Log.info("State: " .. tostring(self.stateMachine.currentState))
    end
})

return class
```

### Usage Example — Nested Composition (Deep Chains)

Composition can be nested arbitrarily deep. The engine recursively creates sub-instances for all levels.

```lua
-- Leaf type
DefineType("ClientDeployState", { ... })

-- Intermediate type composes multiple leaf types
DefineType("ClientUnitStates",
{
    Contains =
    {
        deployState        = "ClientDeployState",
        readyState         = "ClientReadyState",
        projectedMoveState = "ClientProjectedMoveState",
        replayMoveState    = "ClientReplayMoveState"
    },

    Init = function(self, stateMachine, entity)
        self.deployState:Init(stateMachine, entity)
        self.readyState:Init(stateMachine, entity)
        -- ...
    end
})

-- Mid-level type composes the intermediate type
DefineType("ClientUnitStateMachine",
{
    Contains =
    {
        states = "ClientUnitStates"   -- embeds ClientUnitStates, which itself has 4 sub-instances
    },

    InitStateMachine = function(self, unit, entity)
        self.states:Init(self, entity)
        self.currentState = self.states.deployState
        self.currentState:Enter(entity)
    end
})

-- Top-level entity type composes the state machine
DefineType("ClientUnit",
{
    Contains =
    {
        stateMachine = "ClientUnitStateMachine"
    },

    Init = function(self, entity)
        self.stateMachine:InitStateMachine(self, entity)
    end,

    Update = function(self, entity)
        self.stateMachine:UpdateStateMachine(entity)
    end
})
```

### Usage Example — Multiple Composed Fields

A type can contain multiple composed fields of different (or even the same) types:

```lua
DefineType("ClientGameManager",
{
    Contains =
    {
        orderGrid  = "ClientGrid",
        chaosGrid  = "ClientGrid",       -- same type, different instances
        abilities  = "ClientAbilities",
        turnPhases = "ClientTurnPhases"
    },

    Serializable =
    {
        playerFaction = 1
    },

    Init = function(self, entity)
        ClientGameManager_instance = self
    end
})
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Script Type System](#script-type-system)

---

## Entity Component System (ECS)

The ECS module provides direct access to engine components through table-based APIs. All component data is copied between Lua and C++—modifications require explicit `setComponent()` calls.

## Entity Management

### ECS.createEntity

**Signature:** `ECS.createEntity() -> entity`

Creates a new empty entity with a unique ID and generation counter.

**Return Value:**
- Entity handle (lightuserdata) for use with all ECS functions

**Usage Example:**
```lua
function Start()
    -- Create a new projectile
    local projectile = ECS.createEntity()
    
    -- Add components
    ECS.addTransform(projectile, 100, 200, 1, 1, 0)
    ECS.addRigidbody(projectile, 300, 0)  -- Moving right
    ECS.addAABB(projectile, 8, 8)
    
    Log.info("Created projectile: " .. tostring(projectile))
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.destroyEntity

**Signature:** `ECS.destroyEntity(entity)`

Destroys an entity and removes all its components. The entity handle becomes invalid and must not be used after this call.

**Parameters:**
- `entity` (lightuserdata) — Entity to destroy

**Usage Example:**
```lua
function Update(self, entity)
    -- Destroy entity when it goes off-screen
    local transform = ECS.getTransform(entity)
    if transform.x > 1920 or transform.x < 0 then
        Log.info("Entity left screen, destroying")
        ECS.destroyEntity(entity)
    end
end
```

**Important Notes:**
- Entity handle is invalidated immediately
- Using a destroyed entity handle causes a Lua error
- All components are automatically removed
- Subscriptions to events remain (clean up manually if needed)

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

## Transform Component

### ECS.addTransform

**Signature:** `ECS.addTransform(entity, x, y, scaleX, scaleY, rotation)`

Adds a Transform2DFloat component to an entity with position, scale, and rotation.

**Parameters:**
- `entity` (lightuserdata) — Target entity
- `x` (number) — World X position
- `y` (number) — World Y position  
- `scaleX` (number) — X-axis scale factor (1.0 = normal)
- `scaleY` (number) — Y-axis scale factor (1.0 = normal)
- `rotation` (number) — Rotation in degrees (0-360)

**Usage Example:**
```lua
function Start()
    local player = ECS.createEntity()
    
    -- Position at (500, 300), normal scale, no rotation
    ECS.addTransform(player, 500, 300, 1, 1, 0)
    
    -- Create a larger, rotated obstacle
    local obstacle = ECS.createEntity()
    ECS.addTransform(obstacle, 800, 400, 2.5, 2.5, 45)
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.getTransform

**Signature:** `ECS.getTransform(entity) -> transform_table`

Retrieves a copy of the entity's Transform2DFloat component as a Lua table.

**Parameters:**
- `entity` (lightuserdata) — Target entity

**Return Value:**
- Table with fields: `x, y, sx, sy, rotation`
- `nil` if entity doesn't have Transform component

**Usage Example:**
```lua
function Update(self, entity)
    local transform = ECS.getTransform(entity)
    if transform then
        -- Read current position
        Log.info("Position: " .. transform.x .. ", " .. transform.y)
        
        -- Check if near origin
        if transform.x < 50 and transform.y < 50 then
            Log.warning("Too close to origin!")
        end
    end
end
```

**Important Notes:**
- Returns a copy—modifications to the table don't affect the component
- Must call `ECS.setTransform()` to apply changes
- Returns `nil` if component is missing (check before use)

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

## ECS.setTransform

**Signature:** `ECS.setTransform(entity, transform_table)`

Updates an entity's Transform2DFloat component with values from a table. Supports partial updates (missing fields preserve existing values).

**Parameters:**
- `entity` (lightuserdata) — Target entity
- `transform_table` (table) — Table with optional fields: `x, y, sx, sy, rotation`

**Usage Example:**
```lua
function Update(self, entity)
    local transform = ECS.getTransform(entity)
    
    -- Move right at 100 units/second
    local dt = Time.getDeltaTime()
    transform.x = transform.x + 100 * dt
    
    -- Apply changes
    ECS.setTransform(entity, transform)
    
    -- Partial update: only change rotation
    ECS.setTransform(entity, { rotation = 90 })
end
```

**Important Notes:**
- Entity must already have Transform component (add with `addTransform` first)
- Missing fields keep their current values
- All modifications must go through this function

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

## Rigidbody Component

### ECS.addRigidbody

**Signature:** `ECS.addRigidbody(entity, velocityX, velocityY)`

Adds a Rigidbody2DFloat component to an entity with initial velocity.

**Parameters:**
- `entity` (lightuserdata) — Target entity
- `velocityX` (number) — Initial X velocity (units/second)
- `velocityY` (number) — Initial Y velocity (units/second)

**Usage Example:**
```lua
function Start()
    local bullet = ECS.createEntity()
    ECS.addTransform(bullet, 100, 200, 1, 1, 0)
    
    -- Launch bullet to the right
    ECS.addRigidbody(bullet, 500, 0)
    
    -- Falling object
    local rock = ECS.createEntity()
    ECS.addTransform(rock, 300, 0, 1, 1, 0)
    ECS.addRigidbody(rock, 0, 300)  -- Fall downward
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.getRigidbody

**Signature:** `ECS.getRigidbody(entity) -> rigidbody_table`

Retrieves a copy of the entity's Rigidbody2DFloat component.

**Parameters:**
- `entity` (lightuserdata) — Target entity

**Return Value:**
- Table with fields: `vx, vy`
- `nil` if entity doesn't have Rigidbody component

**Usage Example:**
```lua
function Update(self, entity)
    local rb = ECS.getRigidbody(entity)
    if rb then
        local speed = math.sqrt(rb.vx * rb.vx + rb.vy * rb.vy)
        Log.info("Current speed: " .. speed)
        
        -- Check if moving
        if speed < 0.1 then
            Log.info("Entity is stationary")
        end
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.setRigidbody

**Signature:** `ECS.setRigidbody(entity, rigidbody_table)`

Updates an entity's Rigidbody2DFloat component with new velocity values.

**Parameters:**
- `entity` (lightuserdata) — Target entity
- `rigidbody_table` (table) — Table with optional fields: `vx, vy`

**Usage Example:**
```lua
function Update(self, entity)
    -- Stop horizontal movement
    local rb = ECS.getRigidbody(entity)
    rb.vx = 0
    ECS.setRigidbody(entity, rb)
    
    -- Apply gravity
    rb = ECS.getRigidbody(entity)
    rb.vy = rb.vy + 980 * Time.getDeltaTime()
    ECS.setRigidbody(entity, rb)
    
    -- Partial update: bounce vertically
    ECS.setRigidbody(entity, { vy = -rb.vy * 0.8 })
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

## AABB Component

### ECS.addAABB

**Signature:** `ECS.addAABB(entity, halfWidth, halfHeight)`

Adds an AABBFloat (axis-aligned bounding box) component for collision detection.

**Parameters:**
- `entity` (lightuserdata) — Target entity
- `halfWidth` (number) — Half-width of bounding box
- `halfHeight` (number) — Half-height of bounding box

**Usage Example:**
```lua
function Start()
    local player = ECS.createEntity()
    ECS.addTransform(player, 0, 0, 1, 1, 0)
    
    -- 32x32 sprite = 16 half-width/height
    ECS.addAABB(player, 16, 16)
    
    -- Larger enemy: 64x48 = 32x24 half-extents
    local enemy = ECS.createEntity()
    ECS.addAABB(enemy, 32, 24)
end
```

**Important Notes:**
- AABB is centered on entity's transform position
- Half-extents allow symmetric collision boxes
- Requires Transform component to function

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.getAABB

**Signature:** `ECS.getAABB(entity) -> aabb_table`

Retrieves a copy of the entity's AABBFloat component.

**Parameters:**
- `entity` (lightuserdata) — Target entity

**Return Value:**
- Table with fields: `halfWidth, halfHeight`
- `nil` if entity doesn't have AABB component

**Usage Example:**
```lua
function Update(self, entity)
    local aabb = ECS.getAABB(entity)
    if aabb then
        local fullWidth = aabb.halfWidth * 2
        local fullHeight = aabb.halfHeight * 2
        Log.info("Collision box size: " .. fullWidth .. "x" .. fullHeight)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.setAABB

**Signature:** `ECS.setAABB(entity, aabb_table)`

Updates an entity's AABBFloat component dimensions.

**Parameters:**
- `entity` (lightuserdata) — Target entity
- `aabb_table` (table) — Table with optional fields: `halfWidth, halfHeight`

**Usage Example:**
```lua
function Update(self, entity)
    -- Grow collision box over time
    local aabb = ECS.getAABB(entity)
    aabb.halfWidth = aabb.halfWidth + 10 * Time.getDeltaTime()
    aabb.halfHeight = aabb.halfHeight + 10 * Time.getDeltaTime()
    ECS.setAABB(entity, aabb)
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.checkAABBOverlap

**Signature:** `ECS.checkAABBOverlap(entity1, entity2) -> boolean`

Checks if two entities' AABBs overlap in world space.

**Parameters:**
- `entity1` (lightuserdata) — First entity
- `entity2` (lightuserdata) — Second entity

**Return Value:**
- `true` if AABBs overlap, `false` otherwise

**Usage Example:**
```lua
function Update(self, entity)
    local enemies = ECS.getEntitiesWithComponent("EnemyAI")
    
    for i, enemy in ipairs(enemies) do
        if ECS.checkAABBOverlap(entity, enemy) then
            Log.info("Collision with enemy!")
            Event.emitLocal("OnPlayerHit", { enemy = enemy })
        end
    end
end
```

**Important Notes:**
- Both entities must have Transform and AABB components
- Uses world-space positions from Transform
- Simple rectangle overlap test (no rotation support)

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

## MeshRenderer Component

### ECS.addMeshRenderer

**Signature:** `ECS.addMeshRenderer(entity, shaderAssetID)`

Adds a MeshRenderer component for sprite/mesh rendering.

**Parameters:**
- `entity` (lightuserdata) — Target entity
- `shaderAssetID` (integer) - Shader asset ID to assign to the texture

**Usage Example:**
```lua
function Start()
    -- 0 uses default shader
    ECS.addMeshRenderer(entity, 0)
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.getMeshRenderer

**Signature:** `ECS.getMeshRenderer(entity) -> renderer_table`

Retrieves a copy of the entity's MeshRenderer component.

**Parameters:**
- `entity` (lightuserdata) — Target entity

**Return Value:**
- Table with fields: `shaderAssetID, colorAdd, colorMultiply`
- `nil` if entity doesn't have MeshRenderer component

**Usage Example:**
```lua
function Update(self, entity)
    local renderer = ECS.getMeshRenderer(entity)
    if renderer then
        Log.info("Shader: " .. renderer.shaderAssetID)
        Log.info("ColorAdd R: " .. renderer.colorAdd.r)
        Log.info("ColorAdd G: " .. renderer.colorAdd.g)
        Log.info("ColorAdd B: " .. renderer.colorAdd.b)
        Log.info("ColorAdd A: " .. renderer.colorAdd.a)

        Log.info("ColorMultiply R: " .. renderer.colorMultiply.r)
        Log.info("ColorMultiply G: " .. renderer.colorMultiply.g)
        Log.info("ColorMultiply B: " .. renderer.colorMultiply.b)
        Log.info("ColorMultiply A: " .. renderer.colorMultiply.a)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.setMeshRenderer

**Signature:** `ECS.setMeshRenderer(entity, renderer_table)`

Updates an entity's MeshRenderer component.

**Parameters:**
- `entity` (lightuserdata) — Target entity
- `renderer_table` (table) — Table with fields: `shaderAssetID, colorAdd, colorMultiply`

**Usage Example:**
```lua
function Update(self, entity)
    -- Flash red when hit
    if self.justHit then
        ECS.setMeshRenderer(entity, {
            0,
            colorAdd = {r = 1.0, g = 0.0, b = 0.0, a = 1.0},
            colorMultiply = { r = 1.0, g = 1.0, b = 1.0, a = 1.0}
        })
        self.justHit = false
    end
    
    -- Fade out over time
    local renderer = ECS.getMeshRenderer(entity)
    renderer.colorMultiply.a = renderer.colorMultiply.a - 0.5 * Time.getDeltaTime()
    ECS.setMeshRenderer(entity, renderer)
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

## EntityName Component

### ECS.addEntityName

**Signature:** `ECS.addEntityName(entity, name)`

Adds an EntityName component for debugging and entity lookup.

**Parameters:**
- `entity` (lightuserdata) — Target entity
- `name` (string) — Unique name (must be unique across all entities)

**Usage Example:**
```lua
function Start()
    local player = ECS.createEntity()
    ECS.addEntityName(player, "MainPlayer")
    
    local boss = ECS.createEntity()
    ECS.addEntityName(boss, "BossEnemy")
end
```

**Important Notes:**
- Names must be unique (duplicate names overwrite previous)
- Primarily for debugging and editor visualization
- Enables entity lookup via `findEntityByName`

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.getEntityName

**Signature:** `ECS.getEntityName(entity) -> string`

Retrieves the name of an entity.

**Parameters:**
- `entity` (lightuserdata) — Target entity

**Return Value:**
- Name string or `nil` if entity has no name

**Usage Example:**
```lua
function Update(self, entity)
    local name = ECS.getEntityName(entity)
    if name then
        Log.info("Entity: " .. name)
    else
        Log.info("Entity: unnamed")
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.setEntityName

**Signature:** `ECS.setEntityName(entity, name)`

Changes an entity's name.

**Parameters:**
- `entity` (lightuserdata) — Target entity
- `name` (string) — New unique name

**Usage Example:**
```lua
function Update(self, entity)
    -- Rename when state changes
    if self.health <= 0 then
        local currentName = ECS.getEntityName(entity)
        ECS.setEntityName(entity, currentName .. "_Dead")
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.findEntityByName

**Signature:** `ECS.findEntityByName(name) -> entity`

Searches for an entity by its name.

**Parameters:**
- `name` (string) — Name to search for

**Return Value:**
- Entity handle or `nil` if not found

**Usage Example:**
```lua
function Start()
    local player = ECS.findEntityByName("MainPlayer")
    if player then
        Log.info("Found player entity")
        ECS.addTransform(player, 0, 0, 1, 1, 0)
    else
        Log.error("Player entity not found!")
    end
end
```

**Important Notes:**
- Linear search through all named entities (O(n) complexity)
- Returns first match only
- Cache the result if used frequently

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

## Grid Component

### ECS.addGridComponent

**Signature:** `ECS.addGridComponent(entity, gridSizeX, gridSizeY)`

Adds a Grid component to an entity, specifying grid dimensions.

**Parameters:**
- `entity` (lightuserdata) — Target entity
- `gridSizeX` (integer) — Grid width in tiles
- `gridSizeY` (integer) — Grid height in tiles

**Usage Example:**
```lua
function Start()
    local gridEntity = ECS.createEntity()
    ECS.addGridComponent(gridEntity, 6, 4)  -- 6x4 grid
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.getGridComponent

**Signature:** `ECS.getGridComponent(entity) -> grid_table`

Retrieves grid component data.

**Parameters:**
- `entity` (lightuserdata) — Target entity

**Return Value:**
- Table with fields: `gridID, gridSizeX, gridSizeY`
- `nil` if entity doesn't have Grid component

**Usage Example:**
```lua
function Update(self, entity)
    local grid = ECS.getGridComponent(entity)
    if grid then
        Log.info("Grid: " .. grid.gridSizeX .. "x" .. grid.gridSizeY)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.setGridComponent

**Signature:** `ECS.setGridComponent(entity, grid_table)`

Updates grid component properties.

**Parameters:**
- `entity` (lightuserdata) — Target entity
- `grid_table` (table) — Table with optional fields: `gridSizeX, gridSizeY`

**Usage Example:**
```lua
function Update(self, entity)
    ECS.setGridComponent(entity, {
        gridSizeX = 8,
        gridSizeY = 6
    })
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

## Animation Component

### ECS.getAnimation

**Signature:** `ECS.getAnimation(entity) -> anim_table`

Retrieves animation component data.

**Parameters:**
- `entity` (lightuserdata) — Target entity

**Return Value:**
- Table with fields: `animationAssetID, frameTime, currentState, currentFrameIndex`
- `nil` if entity doesn't have Animation component

**Usage Example:**
```lua
function Update(self, entity)
    local anim = ECS.getAnimation(entity)

    if not anim then return end

    Log.info("Current state: " .. anim.currentState)
    Log.info("Anim asset ID: " .. anim.animationAssetID)
    Log.info("Frame time: " .. anim.frameTime)
    Log.info("Current frame: " .. anim.currentFrameIndex)
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.setAnimation

**Signature:** `ECS.setAnimation(entity, newState) -> void`

Sets the animation state of the animation

**Parameters:**
- `entity` (lightuserdata) - Target entity
- `newState` (string) - Name of the new state to change to

**Usage Example:**
```lua
function Update(self, entity)
    ECS.setAnimation(entity, "moveState")
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

## Tilemap Component

### ECS.getTilemapComponent

**Signature:** `ECS.getTilemapComponent(entity) -> table { tilemapID, perspectiveFactor }`

Gets the tilemap component info `tilemapID` and `perspectiveFactor` in a table. 

**Parameters:**
- `entity` (lightuserdata) - Target entity

**Return Value:**
- Table with fields `tilemapID` and `perspectiveFactor`
- `nil` if entity does not have TilemapComponent

**Usage Example:**
```lua
function Update(self, entity)
    local tilemap = ECS.getTilemapComponent(entity)

    if tilemap then
        Log.info("TilemapID: " .. tilemap.tilemapID)
        Log.info("Perspective Factor: " .. tilemap.perspectiveFactor)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

## Query System

### ECS.getEntitiesWithComponent

**Signature:** `ECS.getEntitiesWithComponent(componentName) -> entity_array`

Returns all entities that have a specific component type.

**Parameters:**
- `componentName` (string) — Component type name

**Return Value:**
- Array table of entity handles (may be empty)

**Usage Example:**
```lua
function Update(self, entity)
    -- Process all enemies
    local enemies = ECS.getEntitiesWithComponent("EnemyAI")
    for i, enemy in ipairs(enemies) do
        local transform = ECS.getTransform(enemy)
        Log.info("Enemy " .. i .. " at: " .. transform.x .. ", " .. transform.y)
    end
    
    -- Count projectiles
    local projectiles = ECS.getEntitiesWithComponent("Projectile")
    Log.info("Active projectiles: " .. #projectiles)
end
```

**Important Notes:**
- Component names are case-sensitive
- Returns empty array if no matches
- Use sparingly in Update() (consider caching results)

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.getEntityCount

**Signature:** `ECS.getEntityCount() -> number`

Returns the total number of active entities.

**Return Value:**
- Integer count of entities

**Usage Example:**
```lua
function Update(self, entity)
    local count = ECS.getEntityCount()
    Log.info("Total entities: " .. count)
    
    if count > 1000 then
        Log.warning("High entity count, performance may suffer")
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

### ECS.getAllEntities

**Signature:** `ECS.getAllEntities() -> entity_array`

Returns an array of all active entity handles.

**Return Value:**
- Array table of all entity handles

**Usage Example:**
```lua
function Start()
    local allEntities = ECS.getAllEntities()
    
    for i, entity in ipairs(allEntities) do
        local name = ECS.getEntityName(entity) or "unnamed"
        Log.info("Entity " .. i .. ": " .. name)
    end
end
```

**Important Notes:**
- Expensive with many entities (O(n) allocation + copy)
- Use `getEntitiesWithComponent` for filtered queries
- Results are a snapshot (modifications don't affect array)

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to ECS](#entity-component-system-ecs)

---

## Event System

The event system provides type-safe, schema-validated communication between scripts without tight coupling. Events can be local-only (client-side animations, UI) or networked (synchronized game state).

### Event.define

**Signature:** `Event.define(eventName, schema)`

Registers an event type with a parameter schema for validation.

**Parameters:**
- `eventName` (string) — Unique event identifier
- `schema` (table) — Parameter definitions with field names mapping to type strings:
  - `"number"` — Floating-point or integer
  - `"string"` — Text string
  - `"boolean"` — true/false
  - `"entity"` — Validated entity handle

**Usage Example:**
```lua
-- In Events.lua or initialization script
Event.define("OnClick", {
    worldX = "number",
    worldY = "number"
})

Event.define("OnDamage", {
    target = "entity",
    amount = "number",
    damageType = "string",
    isCritical = "boolean"
})

Event.define("OnUnitMoved", {
    unit = "entity",
    fromX = "number",
    fromY = "number",
    toX = "number",
    toY = "number"
})
```

**Important Notes:**
- Must be called before emitting or subscribing to the event
- Define all events in a central location (e.g., `Events.lua`)
- Schema validation occurs at runtime when events are emitted
- Invalid parameters cause Lua errors with descriptive messages

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Event System](#event-system)

---

### Event.subscribe

**Signature:** `Event.subscribe(eventName, callback)`

Subscribes a callback function to an event.

**Parameters:**
- `eventName` (string) — Event to subscribe to (must be defined first)
- `callback` (function) — Function to call when event is emitted: `function(params)`

**Usage Example:**
```lua
function Start(self, entity)
    -- Subscribe to click events
    Event.subscribe("OnClick", function(params)
        Log.info("Click at: " .. params.worldX .. ", " .. params.worldY)
        
        -- Check if clicked on this entity
        local transform = ECS.getTransform(entity)
        if transform then
            local dx = params.worldX - transform.x
            local dy = params.worldY - transform.y
            local distance = math.sqrt(dx*dx + dy*dy)
            
            if distance < 50 then
                Log.info("Entity was clicked!")
                self.selected = true
            end
        end
    end)
    
    -- Subscribe to damage events
    Event.subscribe("OnDamage", function(params)
        if params.target == entity then
            self.health = self.health - params.amount
            Log.warning("Took " .. params.amount .. " damage!")
        end
    end)
end
```

**Important Notes:**
- Callback receives a table with event parameters
- Multiple callbacks can subscribe to the same event
- No automatic unsubscribe (subscriptions persist until manually removed)
- Callbacks execute in subscription order

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Event System](#event-system)

---

### Event.emitLocal

**Signature:** `Event.emitLocal(eventName, params)`

Emits a local-only event (not synchronized over network).

**Parameters:**
- `eventName` (string) — Event to emit (must be defined first)
- `params` (table) — Event parameters matching the schema

**Usage Example:**
```lua
function Update(self, entity)
    -- Emit click event from mouse input
    if Input.isMouseButtonTriggered(MOUSE_LEFT) then
        local pos = Input.getMouseWorldPosition()
        Event.emitLocal("OnClick", {
            worldX = pos.x,
            worldY = pos.y
        })
    end
    
    -- Emit damage event
    if self.shouldDealDamage then
        Event.emitLocal("OnDamage", {
            target = self.targetEntity,
            amount = 25,
            damageType = "fire",
            isCritical = false
        })
        self.shouldDealDamage = false
    end
end
```

**Important Notes:**
- Parameters are validated against schema (type checking)
- Missing required fields cause Lua errors
- Extra fields are ignored (no strict mode)
- Local events do not trigger network synchronization

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Event System](#event-system)

---

### Event.emitNetworked (NOT WORKING)

**Signature:** `Event.emitNetworked(eventName, params)`

Emits a networked event that is synchronized to all connected clients.

**Parameters:**
- `eventName` (string) — Event to emit (must be defined and marked as networked)
- `params` (table) — Event parameters matching the schema

**Usage Example:**
```lua
function Update(self, entity)
    -- Networked unit movement (synced to all clients)
    if self.shouldMove then
        local transform = ECS.getTransform(entity)
        
        Event.emitNetworked("OnUnitMoved", {
            unit = entity,
            fromX = transform.x,
            fromY = transform.y,
            toX = self.targetX,
            toY = self.targetY
        })
        
        self.shouldMove = false
    end
end
```

**Important Notes:**
- Only events marked as networked can be emitted this way
- Parameters are serialized and transmitted to all clients
- Received on remote clients as normal event subscription
- Use for authoritative game state only (not visual effects)

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Event System](#event-system)

---

## Scenes

The Scene Manager allows users to switch scenes from Lua. 

## Scene Switching

### Scene.setActive

**Signature:** `Scene.setActive(sceneName) -> boolean`

Switches the currently playing scene to the new scene. 

**Parameters:**
- `sceneName` (string) - Name of the new scene to switch to (eg. `"MyScene"`)

**Return Value:**
- `true` if scene switch was successful, `false` otherwise

**Usage Example:**
```lua
function Update(self, entity)
    if Scene.setActive("MyScene") then
        Log.info("Scene changed to MyScene successfully")
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Scenes](#scenes)

---

## Prefabs

The Prefabs manager allows spawning of different object archetypes (also known as Prefabs) within the ECS. 

## Prefab Spawning

### Prefab.spawn

**Signature:** `Prefab.spawn(prefabName, posX, posY, parentEntity) -> entity`

Spawns an entity from a prefab name. If x and y coordinates are provided and the entity has a Transform component, sets the position. If parent entity is provided, the spawned entity will be parented to it (independent of prefab's internal hierarchy). Returns the spawned entity handle, or nil if prefab doesn't exist. 

The function spawns the prefab hierarchy (parent + all children) but only returns the root handle. 

**Parameters:**
- `prefabName` (string) - Name of the prefab to spawn
- `posX (Optional)` (number/double) - X position of the spawned prefab
- `posY (Optional)` (number/double) - Y position of the spawned prefab
- `parentEntity (Optional)` (lightuserdata) - parent of the entity to spawn to

**Return Values:**
- `entity`  (lightuserdata) - Handle of the root entity spawned, nil if prefab doesn't exist

**Usage Example:**
```lua
function Update(self, entity)
    local enemy = Prefab.spawn("Prop")

    if enemy then
        Log.info("Prefab 'Prop' spawned successfully")
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Input System](#prefabs)

---

## Camera System

GGEngine operates with only one game camera, which resets back to { 0.0, 0.0 } upon Start or switching scenes. 

### Camera.getPosition

**Signature:** `Camera.getPosition() -> { x, y }`

Gets the Camera's current world position.

**Return Values:**
- `{ x, y }` (table) world position of the camera

**Usage Example:**
```lua
function Update(self, entity)
    local cameraPos = Camera.getPosition()

    if cameraPos then
        Log.info("Camera world pos: " .. cameraPos.x .. ", " .. cameraPos.y)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Input System](#input-system)

---

### Camera.setPosition

**Signature:** `Camera.setPosition(x, y)`

**Parameters:**
- `x` (number) X world position to set the camera to
- `y` (number) Y world position to set the camera to

**Usage Examples:**
```lua
function Update(self, entity)
    Camera.setPosition(1.0, 5.0)

    local cameraPos = Camera.getPosition()

    if cameraPos then
        -- This should print: "Camera world pos: 1.0, 5.0:"
        Log.info("Camera world pos: " .. cameraPos.x .. ", " .. cameraPos.y)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Input System](#input-system)

---

### Camera.setFollowEntity

**Signature:** `Camera.setFollowEntity(entityHandle)`

Sets the camera's follow target to the targeted entity

**Parameters:**
- `entityHandle` (lightuserdata) Entity to follow

**Usage Examples:**
```lua
function Update(self, entity)
    -- Camera should now follow the Entity's position
    Camera.setFollowEntity(entity)
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Input System](#input-system)

---

### Camera.clearFollowTargets

**Signature:** `Camera.clearFollowTargets()`

Clears the game camera's follow target

**Usage Examples:**
```lua
function Update(self, entity)
    -- Game camera should now stay in place
    Camera.clearFollowTargets()
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Input System](#input-system)

---

## Input System

The input system provides direct access to keyboard and mouse state. All input functions query the current frame's state and are only valid during Update() calls.

## Keyboard Input

### Input.isKeyPressed

**Signature:** `Input.isKeyPressed(keyCode) -> boolean`

Checks if a key is currently held down.

**Parameters:**
- `keyCode` (integer) — Key code constant (e.g., `KEY_W`, `KEY_SPACE`)

**Return Value:**
- `true` if key is pressed, `false` otherwise

**Usage Example:**
```lua
function Update(self, entity)
    local transform = ECS.getTransform(entity)
    local speed = 200
    local dt = Time.getDeltaTime()
    
    -- WASD movement
    if Input.isKeyPressed(KEY_W) then
        transform.y = transform.y - speed * dt
    end
    if Input.isKeyPressed(KEY_S) then
        transform.y = transform.y + speed * dt
    end
    if Input.isKeyPressed(KEY_A) then
        transform.x = transform.x - speed * dt
    end
    if Input.isKeyPressed(KEY_D) then
        transform.x = transform.x + speed * dt
    end
    
    ECS.setTransform(entity, transform)
end
```

**Available Key Constants:**
- Letters: `KEY_A` through `KEY_Z`
- Numbers: `KEY_0` through `KEY_9`
- Special: `KEY_SPACE`, `KEY_ENTER`, `KEY_ESCAPE`, `KEY_TAB`, `KEY_BACKSPACE`
- Arrows: `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`
- Function: `KEY_F1` through `KEY_F12`
- Modifiers: `KEY_LEFT_SHIFT`, `KEY_LEFT_CONTROL`, `KEY_LEFT_ALT`

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Input System](#input-system)

---

### Input.isKeyTriggered

**Signature:** `Input.isKeyTriggered(keyCode) -> boolean`

Checks if a key was pressed this frame (edge detection).

**Parameters:**
- `keyCode` (integer) — Key code constant

**Return Value:**
- `true` only on the frame the key goes from up to down

**Usage Example:**
```lua
function Update(self, entity)
    -- Jump on spacebar press (not hold)
    if Input.isKeyTriggered(KEY_SPACE) then
        local rb = ECS.getRigidbody(entity)
        rb.vy = -500
        ECS.setRigidbody(entity, rb)
        Log.info("Jump!")
    end
    
    -- Shoot weapon (single shot per press)
    if Input.isKeyTriggered(KEY_F) then
        self:FireWeapon()
    end
    
    -- Toggle debug mode
    if Input.isKeyTriggered(KEY_F3) then
        self.debugMode = not self.debugMode
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Input System](#input-system)

---

### Input.isKeyReleased

**Signature:** `Input.isKeyReleased(keyCode) -> boolean`

Checks if a key was released this frame (edge detection).

**Parameters:**
- `keyCode` (integer) — Key code constant

**Return Value:**
- `true` only on the frame the key goes from down to up

**Usage Example:**
```lua
function Update(self, entity)
    -- Charge shot mechanic
    if Input.isKeyPressed(KEY_SPACE) then
        self.chargeLevel = self.chargeLevel + Time.getDeltaTime()
    end
    
    if Input.isKeyReleased(KEY_SPACE) then
        self:FireChargedShot(self.chargeLevel)
        self.chargeLevel = 0
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Input System](#input-system)

---

## Mouse Input

### Input.getMousePosition

**Signature:** `Input.getMousePosition() -> {x, y}`

Returns mouse position in screen coordinates (pixels from top-left).

**Return Value:**
- Table with fields: `x, y` (screen coordinates)

**Usage Example:**
```lua
function Update(self, entity)
    local mouse = Input.getMousePosition()
    Log.info("Mouse screen pos: " .. mouse.x .. ", " .. mouse.y)
    
    -- Check if in UI region
    if mouse.x < 200 and mouse.y < 100 then
        Log.info("Mouse over UI panel")
    end
end
```

**Important Notes:**
- Origin (0, 0) is top-left corner of window
- Values are in pixels
- Y increases downward
- Use `getMouseWorldPosition` for gameplay logic

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Input System](#input-system)

---

### Input.getMouseWorldPosition

**Signature:** `Input.getMouseWorldPosition() -> {x, y}`

Returns mouse position in world coordinates (accounts for camera transform).

**Return Value:**
- Table with fields: `x, y` (world coordinates)

**Usage Example:**
```lua
function Update(self, entity)
    local worldPos = Input.getMouseWorldPosition()
    
    -- Convert to grid coordinates
    local gridX = math.floor(worldPos.x / 64)
    local gridY = math.floor(worldPos.y / 64)
    Log.info("Mouse over grid cell: " .. gridX .. ", " .. gridY)
    
    -- Rotate entity to face mouse
    local transform = ECS.getTransform(entity)
    local dx = worldPos.x - transform.x
    local dy = worldPos.y - transform.y
    local angle = math.atan2(dy, dx) * 180 / math.pi
    transform.rotation = angle
    ECS.setTransform(entity, transform)
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Input System](#input-system)

---

### Input.isMouseButtonPressed

**Signature:** `Input.isMouseButtonPressed(button) -> boolean`

Checks if a mouse button is currently held down.

**Parameters:**
- `button` (integer) — Button index: `0` (left), `1` (right), `2` (middle)

**Return Value:**
- `true` if button is pressed, `false` otherwise

**Usage Example:**
```lua
function Update(self, entity)
    -- Drag object while left mouse held
    if Input.isMouseButtonPressed(0) then
        local worldPos = Input.getMouseWorldPosition()
        local transform = ECS.getTransform(entity)
        transform.x = worldPos.x
        transform.y = worldPos.y
        ECS.setTransform(entity, transform)
    end
end
```

**Mouse Button Constants:**
- `0` or `MOUSE_LEFT` — Left button
- `1` or `MOUSE_RIGHT` — Right button
- `2` or `MOUSE_MIDDLE` — Middle button (scroll wheel click)

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Input System](#input-system)

---

### Input.isMouseButtonTriggered

**Signature:** `Input.isMouseButtonTriggered(button) -> boolean`

Checks if a mouse button was clicked this frame (edge detection).

**Parameters:**
- `button` (integer) — Button index

**Return Value:**
- `true` only on the frame the button goes from up to down

**Usage Example:**
```lua
function Update(self, entity)
    -- Place building on left click
    if Input.isMouseButtonTriggered(0) then
        local worldPos = Input.getMouseWorldPosition()
        Event.emitLocal("OnPlaceBuilding", {
            x = worldPos.x,
            y = worldPos.y
        })
    end
    
    -- Open context menu on right click
    if Input.isMouseButtonTriggered(1) then
        local worldPos = Input.getMouseWorldPosition()
        self:ShowContextMenu(worldPos.x, worldPos.y)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Input System](#input-system)

---

### Input.isMouseButtonReleased

**Signature:** `Input.isMouseButtonReleased(button) -> boolean`

Checks if a mouse button was released this frame (edge detection).

**Parameters:**
- `button` (integer) — Button index

**Return Value:**
- `true` only on the frame the button goes from down to up

**Usage Example:**
```lua
function Update(self, entity)
    -- Start drag on press
    if Input.isMouseButtonTriggered(0) then
        self.isDragging = true
    end
    
    -- End drag on release
    if Input.isMouseButtonReleased(0) then
        if self.isDragging then
            Event.emitLocal("OnDragComplete", {
                entity = entity
            })
            self.isDragging = false
        end
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Input System](#input-system)

---

## Grid System

The grid system provides turn-based gameplay infrastructure with tile management, pathfinding, area operations, and win condition checking. Designed for 6x4 grids with deterministic networking support.

## Grid Management

### Grid.getActiveGrid

**Signature:** `Grid.getActiveGrid() -> gridID`

Returns the currently active grid ID.

**Return Value:**
- Integer grid ID

**Usage Example:**
```lua
function Update(self, entity)
    local activeGrid = Grid.getActiveGrid()
    local tile = Grid.getTile(activeGrid, 0, 0)
    
    if tile then
        Log.info("Active grid's origin tile health: " .. tile.health)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Grid System](#grid-system)

---

## Tile Queries

### Grid.getTile

**Signature:** `Grid.getTile(gridID, x, y) -> tile_table`

Retrieves tile data at specified grid coordinates.

**Parameters:**
- `gridID` (integer) — Grid ID
- `x` (integer) — X coordinate (0-5 for 6-wide grid)
- `y` (integer) — Y coordinate (0-3 for 4-tall grid)

**Return Value:**
- Table with tile data or `nil` if coordinates invalid:
  - `position` (table) — `{x, y}` grid coordinates
  - `isPassable` (boolean) — Can units move through
  - `isVisible` (boolean) — Revealed by fog of war
  - `isDestroyed` (boolean) — Health <= 0
  - `isHighlighted` (boolean) — Visual highlight active
  - `isOccupied` (boolean) — Has unit on it

**Usage Example:**
```lua
function Update(self, entity)
    -- getOrderGrid to be deprecated, use ECS.getGridComponent() instead
    local grid = Grid.getOrderGrid()
    local tile = Grid.getTile(grid, 2, 1)
    
    if tile then
        Log.info("Tile at (2,1):")
        Log.info("  Passable: " .. tostring(tile.isPassable))
        Log.info("  Occupied: " .. tostring(tile.isOccupied))
        
        if tile.isOccupied then
            Log.info("  Unit ID: " .. tile.unitID)
        end
    else
        Log.error("Invalid tile coordinates")
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Grid System](#grid-system)

---

### Grid.getTilesInRange

**Signature:** `Grid.getTilesInRange(gridID, centerX, centerY, range) -> tile_array`

Gets all tiles within Manhattan distance range.

**Parameters:**
- `gridID` (integer) — Grid ID
- `centerX` (integer) — Center X coordinate
- `centerY` (integer) — Center Y coordinate
- `range` (integer) — Manhattan distance radius

**Return Value:**
- Array of tile tables (may be empty)

**Usage Example:**
```lua
function Update(self, entity)
    -- getOrderGrid to be deprecated, use ECS.getGridComponent() instead
    local grid = Grid.getOrderGrid()
    
    -- Get all tiles within 2 spaces of (3, 2)
    local tiles = Grid.getTilesInRange(grid, 3, 2, 2)
    
    Log.info("Found " .. #tiles .. " tiles in range")
    for i, tile in ipairs(tiles) do
        Log.info("Tile " .. i .. ": (" .. 
            tile.position.x .. ", " .. tile.position.y .. ")")
    end
end
```

**Important Notes:**
- Uses Manhattan distance: |dx| + |dy|
- Includes center tile
- Returns only valid tiles (clips at grid boundaries)

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Grid System](#grid-system)

---

### Grid.getTilesInArea

**Signature:** `Grid.getTilesInArea(gridID, centerX, centerY, width, height) -> tile_array`

Gets all tiles in a rectangular area.

**Parameters:**
- `gridID` (integer) — Grid ID
- `centerX` (integer) — Center X coordinate
- `centerY` (integer) — Center Y coordinate
- `width` (integer) — Area width in tiles
- `height` (integer) — Area height in tiles

**Return Value:**
- Array of tile tables (may be empty)

**Usage Example:**
```lua
function Update(self, entity)
    -- getOrderGrid to be deprecated, use ECS.getGridComponent() instead
    local grid = Grid.getOrderGrid()
    
    -- Get 3x2 area centered at (2, 1)
    local tiles = Grid.getTilesInArea(grid, 2, 1, 3, 2)
    
    for i, tile in ipairs(tiles) do
        if not tile.isDestroyed then
            Log.info("Tile (" .. tile.position.x .. ", " .. 
                tile.position.y .. ") still standing")
        end
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Grid System](#grid-system)

---

### Grid.highlightTiles

**Signature:** `Grid.highlightTiles(gridID, positions, clearPrevious)`

Highlights specified tiles for visual feedback.

**Parameters:**
- `gridID` (integer) — Grid ID
- `positions` (array) — Array of position tables: `{{x=0, y=0}, {x=1, y=0}, ...}`
- `clearPrevious` (boolean, optional) — Clear previous highlights (default: `true`)

**Usage Example:**
```lua
function Update(self, entity)
    -- getOrderGrid to be deprecated, use ECS.getGridComponent() instead
    local grid = Grid.getOrderGrid()
    
    -- Highlight movement range
    if self.selectedUnit then
        local unitPos = self.selectedUnit.position
        local range = 3
        
        local tiles = Grid.getTilesInRange(grid, unitPos.x, unitPos.y, range)
        local positions = {}
        
        for i, tile in ipairs(tiles) do
            if tile.isPassable then
                table.insert(positions, {x = tile.position.x, y = tile.position.y})
            end
        end
        
        Grid.highlightTiles(grid, positions, true)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Grid System](#grid-system)

---

## Pathfinding

### Grid.validateMovement

**Signature:** `Grid.validateMovement(gridID, fromX, fromY, toX, toY, maxRange, unitID) -> result_table`

Validates and pathfinds movement from one tile to another.

**Parameters:**
- `gridID` (integer) — Grid ID
- `fromX` (integer) — Start X coordinate
- `fromY` (integer) — Start Y coordinate
- `toX` (integer) — Target X coordinate
- `toY` (integer) — Target Y coordinate
- `maxRange` (integer) — Maximum movement distance
- `unitID` (integer, optional) — Unit ID for pathfinding (default: 0)

**Return Value:**
- Movement result table:
  - `canMove` (boolean) — Movement is valid
  - `path` (array) — Array of position tables: `{{x, y}, {x, y}, ...}`
  - `reason` (string) — Failure reason if `canMove` is false
  - `validationTimeMs` (number) — Pathfinding execution time
  - `tilesChecked` (integer) — Number of tiles evaluated
  - `algorithmUsed` (string) — Pathfinding algorithm used

**Usage Example:**
```lua
function Update(self, entity)
    -- getOrderGrid to be deprecated, use ECS.getGridComponent() instead
    local grid = Grid.getOrderGrid()
    
    -- Validate unit movement
    if self.moveCommand then
        local result = Grid.validateMovement(
            grid,
            self.unitX, self.unitY,
            self.targetX, self.targetY,
            3,  -- Max range of 3
            self.unitID
        )
        
        if result.canMove then
            Log.info("Movement valid! Path length: " .. #result.path)
            
            -- Execute movement along path
            for i, pos in ipairs(result.path) do
                Log.info("Step " .. i .. ": (" .. pos.x .. ", " .. pos.y .. ")")
            end
            
            self:MoveAlongPath(result.path)
        else
            Log.warning("Cannot move: " .. result.reason)
        end
        
        Log.info("Pathfinding time: " .. result.validationTimeMs .. "ms")
        Log.info("Algorithm: " .. result.algorithmUsed)
        
        self.moveCommand = false
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Grid System](#grid-system)

---

### Grid.setPathfindingAlgorithm

**Signature:** `Grid.setPathfindingAlgorithm(algorithm)`

Sets the pathfinding algorithm for future validations.

**Parameters:**
- `algorithm` (string) — Algorithm name: `"SIMPLE"` or `"BFS"`

**Usage Example:**
```lua
function Start()
    -- Use BFS for more accurate pathfinding
    Grid.setPathfindingAlgorithm("BFS")
    
    -- Use SIMPLE for faster, less accurate pathfinding
    -- Grid.setPathfindingAlgorithm("SIMPLE")
end
```

**Important Notes:**
- `"SIMPLE"` — Fast Manhattan distance check (no obstacles)
- `"BFS"` — Breadth-first search with obstacle avoidance

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Grid System](#grid-system)

---

## Coordinate Conversion

### Grid.worldToGrid

**Signature:** `Grid.worldToGrid(gridID, worldX, worldY) -> grid_position`

Converts world coordinates to grid tile coordinates.

**Parameters:**
- `gridID` (integer) — Grid ID
- `worldX` (number) — World X coordinate
- `worldY` (number) — World Y coordinate

**Return Value:**
- Position table `{x, y}` with integer grid coordinates
- `nil` if coordinates are outside grid bounds

**Usage Example:**
```lua
function Update(self, entity)
    -- getOrderGrid to be deprecated, use ECS.getGridComponent() instead
    local grid = Grid.getOrderGrid()
    
    -- Convert mouse to grid position
    if Input.isMouseButtonTriggered(0) then
        local worldPos = Input.getMouseWorldPosition()
        local gridPos = Grid.worldToGrid(grid, worldPos.x, worldPos.y)
        
        if gridPos then
            Log.info("Clicked grid tile: (" .. gridPos.x .. ", " .. gridPos.y .. ")")
            
            local tile = Grid.getTile(grid, gridPos.x, gridPos.y)
            if tile then
                Log.info("Tile health: " .. tile.health)
            end
        else
            Log.info("Clicked outside grid")
        end
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Grid System](#grid-system)

---

### Grid.gridToWorld

**Signature:** `Grid.gridToWorld(gridID, gridX, gridY) -> world_position`

Converts grid tile coordinates to world coordinates (tile center).

**Parameters:**
- `gridID` (integer) — Grid ID
- `gridX` (integer) — Grid X coordinate
- `gridY` (integer) — Grid Y coordinate

**Return Value:**
- Position table `{x, y}` with world coordinates

**Usage Example:**
```lua
function Update(self, entity)
    -- getOrderGrid to be deprecated, use ECS.getGridComponent() instead
    local grid = Grid.getOrderGrid()
    
    -- Position unit at grid tile (2, 1)
    local worldPos = Grid.gridToWorld(grid, 2, 1)
    
    local transform = ECS.getTransform(entity)
    transform.x = worldPos.x
    transform.y = worldPos.y
    ECS.setTransform(entity, transform)
    
    Log.info("Unit positioned at world: " .. worldPos.x .. ", " .. worldPos.y)
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Grid System](#grid-system)

---

### Grid.getMouseWorldPerspective(perspective)

**Signature:** `Grid.getMouseWorldPerspective(perspective) -> { x, y }`

Gets the current mouse world position accounting for the tilemap's `perspectiveFactor`. Usually used in conjunction with `Grid.worldToGrid(gridID, worldX, worldY)`

**Parameters:**
- `perspective` (number) perspective factor of the tilemap

**Return Value:**
- `{ x, y }` Table with `x` and `y` members

**Usage Example:**
```lua
function Update(self, entity)
    local tilemap = ECS.getTilemapComponent(entity)
    local grid = ECS.getGridComponent(entity)

    if tilemap and grid then
        local pos = Grid.getMouseWorldPerspective(tilemap.perspectiveFactor)

        if pos then
            local gridPos = Grid.worldToGrid(grid.gridID, pos.x, pos.y)
            Log.info("Grid Position with perspective: " .. gridPos.x .. ", " .. gridPos.y)
        end
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Grid System](#grid-system)

---

## Tile Color Operations

### Grid.setTileTexture

**Signature:** `Grid.setTileTexture(gridID, gridX, gridY, layer, texID)`

Sets the current texture of the tile to the assigned Texture ID

**Parameters:**
- `gridID` (integer) grid ID to reference
- `gridX` (integer) x position of the tile
- `gridY` (integer) y position of the tile
- `layer` (integer) tilemap layer referenced - usually 0
- `texID` (integer) ID of the texture to set

**Usage Example:**
```lua
function Update(self, entity)
    local grid = ECS.getGridComponent(entity)

    if grid then
        -- Set texture of grid tile (2, 1) to TILE_DESTROYED_TEX_ID
        Grid.setTileTexture(grid.gridID, 2, 1, 0, TILE_DESTROYED_TEX_ID)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Grid System](#grid-system)

---
### Grid.setTileColorAdd

**Signature:** `Grid.setTileColorAdd(gridID, gridX, gridY, layer, { r, g, b, a })`

Sets the current color of the tile to the assigned color

**Parameters:**
- `gridID` (integer) grid ID to reference
- `gridX` (integer) x position of the tile
- `gridY` (integer) y position of the tile
- `layer` (integer) tilemap layer referenced - usually 0
- `{ r, g, b, a }` (table) color to set the tile to

**Usage Example:**
```lua
function Update(self, entity)
    local grid = ECS.getGridComponent(entity)

    if grid then
        -- Set color of grid tile (2, 1) to TILE_RED
        Grid.setTileColorAdd(grid.gridID, 2, 1, 0, TILE_RED)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Grid System](#grid-system)

---

### Grid.setTileColorMultiply

**Signature:** `Grid.setTileColorMultiply(gridID, gridX, gridY, layer, { r, g, b, a })`

Sets the current multiply color of the tile to the assigned color

**Parameters:**
- `gridID` (integer) grid ID to reference
- `gridX` (integer) x position of the tile
- `gridY` (integer) y position of the tile
- `layer` (integer) tilemap layer referenced - usually 0
- `{ r, g, b, a }` (table) multiply color to set the tile to

**Usage Example:**
```lua
function Update(self, entity)
    local grid = ECS.getGridComponent(entity)

    if grid then
        -- Set color multiply of grid tile (2, 1) to TILE_RED
        Grid.setTileColorMultiply(grid.gridID, 2, 1, 0, TILE_RED)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Grid System](#grid-system)

---

## Networking (RPC)

The RPC (Remote Procedure Call) system enables networked multiplayer using a client-server architecture. Functions can be called across the network with automatic serialization.

### RPC.startServer

**Signature:** `RPC.startServer(port) -> boolean`

Initializes and starts the game server.

**Parameters:**
- `port` (integer, optional) — Server port (default: 9081)

**Return Value:**
- `true` if server started successfully, `false` on failure

**Usage Example:**
```lua
local class = DefineType("NetworkManager",
{
    Serializable = 
    {
        isServer = false
    },
    
    Start = function(self, entity)
        if self.isServer then
            if RPC.startServer() then
                Log.info("Server started on default port")
            else
                Log.error("Failed to start server")
            end
        end
    end
})

return class
```

**Important Notes:**
- Only one server can run per machine
- Returns `false` if server is already running
- Default port is 9081 (defined in engine)
- Must be called before any RPC calls

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Networking (RPC)](#networking-rpc)

---

### RPC.connectToServer

**Signature:** `RPC.connectToServer(address, port) -> boolean`

Connects the client to a game server.

**Parameters:**
- `address` (string, optional) — Server address (default: "eisenfaust.ddns.net")
- `port` (integer, optional) — Server port (default: 9081)

**Return Value:**
- `true` if connection initiated successfully, `false` on failure

**Usage Example:**
```lua
local class = DefineType("NetworkManager",
{
    Serializable =
    {
        isServer = false,
        serverAddress = "localhost"
    },
    
    Start = function(self, entity)
        if not self.isServer then
            if RPC.connectToServer(self.serverAddress, 9081) then
                Log.info("Connecting to server: " .. self.serverAddress)
            else
                Log.error("Failed to connect to server")
            end
        end
    end
})

return class
```

**Important Notes:**
- Connection is asynchronous (returns immediately)
- Check connection status in subsequent frames
- Returns `false` if already connected
- Default address points to development server

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Networking (RPC)](#networking-rpc)

---

### RPC.call

**Signature:** `RPC.call(functionName, ...args)`

Calls a Lua function over the network. Automatically routes to server (from client) or all clients (from server).

**Parameters:**
- `functionName` (string) — Global Lua function name to call remotely
- `...args` (variadic) — Function arguments (supports: boolean, number, string, entity)

**Supported Argument Types:**
- `boolean` — true/false values
- `number` — Integers or floating-point (auto-detected)
- `string` — Text strings
- `entity` — Entity handles (lightuserdata)

**Usage Example:**
```lua
-- Global RPC functions (defined at file scope)
function ClientRPC(message)
    Log.info("Server says: " .. message)
end

function ServerRPC(playerID, action)
    Log.info("Client " .. playerID .. " action: " .. action)
    
    -- Broadcast to all clients
    RPC.call("ClientRPC", "Action processed: " .. action)
end

local class = DefineType("NetworkManager",
{
    Serializable = 
    {
        isServer = false
    },
    
    Start = function(self, entity)
        if self.isServer then
            RPC.startServer()
        else
            RPC.connectToServer()
        end
        
        Event.subscribe("OnClick", function(params)
            NetworkManager.OnClickHandler(self, entity, params)
        end)
    end,
    
    OnClickHandler = function(self, entity, params)
        if not self.isServer then
            -- Client sends to server
            RPC.call("ServerRPC", 1, "ClickAttack")
        end
    end
})

return class
```

**Advanced Example with Multiple Arguments:**
```lua
-- Server-side function
function OnUnitMove(unitEntity, fromX, fromY, toX, toY, isJump)
    Log.info("Unit moved from (" .. fromX .. ", " .. fromY .. ") to (" .. 
        toX .. ", " .. toY .. ")")
    if isJump then
        Log.info("Unit performed jump move")
    end
    
    -- Broadcast to all clients
    RPC.call("ClientUnitMoveConfirm", unitEntity, toX, toY)
end

-- Client calls server
RPC.call("OnUnitMove", playerUnit, 2, 1, 3, 2, false)
```

**Important Notes:**
- Function must be defined at global scope (not inside DefineType)
- Client→Server: Always sends to server
- Server→Clients: Broadcasts to all connected clients
- Entity handles are packed/unpacked automatically
- Unsupported types (tables, functions) cause Lua errors
- No return values (fire-and-forget messaging)

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Networking (RPC)](#networking-rpc)

---

## Audio System

The audio system provides playback control for sound effects and music using FMOD integration.

### Sound Effects

#### Audio.playSound

**Signature:** `Audio.playSound(assetPath, volume) -> handle`

Plays a sound effect with specified volume.

**Parameters:**
- `assetPath` (string) — Asset path or name (e.g., "explosion.wav")
- `volume` (number) — Volume 0.0 (silent) to 1.0 (full)

**Return Value:**
- Sound handle (integer) for later control

**Usage Example:**
```lua
function Update(self, entity)
    -- Play sound on collision
    if self.justCollided then
        local explosionSound = Audio.playSound("explosion.wav", 0.8)
        self.explosionHandle = explosionSound
        self.justCollided = false
    end
    
    -- Play UI click
    if Input.isMouseButtonTriggered(0) then
        Audio.playSound("ui_click.wav", 0.5)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Audio System](#audio-system)

---

#### Audio.stopSound

**Signature:** `Audio.stopSound(handle)`

Stops a playing sound effect.

**Parameters:**
- `handle` (integer) — Sound handle from `playSound`

**Usage Example:**
```lua
function Update(self, entity)
    -- Stop sound when entity is destroyed
    if self.health <= 0 then
        if self.loopingSoundHandle then
            Audio.stopSound(self.loopingSoundHandle)
        end
        ECS.destroyEntity(entity)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Audio System](#audio-system)

---

#### Audio.setVolume

**Signature:** `Audio.setVolume(handle, volume)`

Changes the volume of a playing sound.

**Parameters:**
- `handle` (integer) — Sound handle
- `volume` (number) — New volume 0.0 to 1.0

**Usage Example:**
```lua
function Update(self, entity)
    -- Fade out sound over time
    if self.soundHandle and self.isFading then
        self.soundVolume = self.soundVolume - 0.5 * Time.getDeltaTime()
        
        if self.soundVolume <= 0 then
            Audio.stopSound(self.soundHandle)
            self.isFading = false
        else
            Audio.setVolume(self.soundHandle, self.soundVolume)
        end
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Audio System](#audio-system)

---

### Music

#### Audio.playMusic

**Signature:** `Audio.playMusic(assetPath, volume, loop) -> handle`

Plays background music with looping option.

**Parameters:**
- `assetPath` (string) — Music asset path (e.g., "battle_theme.ogg")
- `volume` (number) — Volume 0.0 to 1.0
- `loop` (boolean) — Loop continuously

**Return Value:**
- Music handle (integer)

**Usage Example:**
```lua
function Start(self, entity)
    -- Play looping battle music
    local musicHandle = Audio.playMusic("battle_theme.ogg", 0.6, true)
    self.battleMusicHandle = musicHandle
    
    -- Play victory jingle (no loop)
    if self.gameWon then
        Audio.playMusic("victory.ogg", 0.8, false)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Audio System](#audio-system)

---

#### Audio.stopMusic

**Signature:** `Audio.stopMusic()`

Stops all currently playing music.

**Usage Example:**
```lua
function Update(self, entity)
    -- Stop music on game over
    if self.health <= 0 then
        Audio.stopMusic()
        Audio.playSound("game_over.wav", 1.0)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Audio System](#audio-system)

---

## Time System

#### Time.getDeltaTime

**Signature:** `Time.getDeltaTime() -> number`

Returns time elapsed since last frame in seconds.

**Return Value:**
- Delta time in seconds (typically 0.016 for 60 FPS)

**Usage Example:**
```lua
function Update(self, entity)
    local dt = Time.getDeltaTime()
    
    -- Frame-rate independent movement
    local speed = 200
    local transform = ECS.getTransform(entity)
    transform.x = transform.x + speed * dt
    ECS.setTransform(entity, transform)
    
    -- Timer countdown
    self.cooldown = self.cooldown - dt
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Time System](#time-system)

---

#### Time.getTotalTime

**Signature:** `Time.getTotalTime() -> number`

Returns total time since engine start in seconds.

**Return Value:**
- Elapsed time in seconds

**Usage Example:**
```lua
function Update(self, entity)
    local elapsed = Time.getTotalTime()
    
    -- Sine wave animation
    local wave = math.sin(elapsed * 2)
    local transform = ECS.getTransform(entity)
    transform.y = 300 + wave * 50
    ECS.setTransform(entity, transform)
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Time System](#time-system)

---

#### Time.getFPS

**Signature:** `Time.getFPS() -> number`

Returns current frames per second.

**Return Value:**
- FPS as floating-point number

**Usage Example:**
```lua
function Update(self, entity)
    local fps = Time.getFPS()
    
    if fps < 30 then
        Log.warning("Low FPS detected: " .. fps)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Time System](#time-system)

---

#### Time.getFrameCount

**Signature:** `Time.getFrameCount() -> number`

Returns total frames rendered since engine start.

**Return Value:**
- Frame count as integer

**Usage Example:**
```lua
function Update(self, entity)
    local frame = Time.getFrameCount()
    
    -- Trigger every 60 frames
    if frame % 60 == 0 then
        Log.info("One second elapsed at 60 FPS")
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Time System](#time-system)

---

## Debug & Logging

### Logging

#### Log.info

**Signature:** `Log.info(message)`

Logs an informational message.

**Parameters:**
- `message` (string) — Message to log

**Usage Example:**
```lua
function Start(self, entity)
    Log.info("=== Script Initialized ===")
    Log.info("Entity ID: " .. tostring(entity))
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Debug & Logging](#debug--logging)

---

#### Log.warning

**Signature:** `Log.warning(message)`

Logs a warning message (displayed in yellow/orange).

**Parameters:**
- `message` (string) — Warning message

**Usage Example:**
```lua
function Update(self, entity)
    if self.health < 20 then
        Log.warning("Health critical: " .. self.health)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Debug & Logging](#debug--logging)

---

#### Log.error

**Signature:** `Log.error(message)`

Logs an error message (displayed in red).

**Parameters:**
- `message` (string) — Error message

**Usage Example:**
```lua
function Update(self, entity)
    local transform = ECS.getTransform(entity)
    if not transform then
        Log.error("Entity missing Transform component!")
        return
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Debug & Logging](#debug--logging)

---

### Profiling

#### Profile.start

**Signature:** `Profile.start(name)`

Starts a named profiling timer.

**Parameters:**
- `name` (string) — Unique profile name

**Usage Example:**
```lua
function Update(self, entity)
    Profile.start("pathfinding")
    
    -- Expensive pathfinding operation
    local result = Grid.validateMovement(grid, 0, 0, 5, 3, 10, 0)
    
    Profile.stop("pathfinding")  -- Logs elapsed time
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Debug & Logging](#debug--logging)

---

#### Profile.stop

**Signature:** `Profile.stop(name)`

Stops a profiling timer and logs elapsed time in milliseconds.

**Parameters:**
- `name` (string) — Profile name to stop

**Usage Example:**
```lua
function Start(self, entity)
    Profile.start("initialization")
    
    -- Initialization code
    for i = 1, 100 do
        local e = ECS.createEntity()
        ECS.addTransform(e, i * 10, 0, 1, 1, 0)
    end
    
    Profile.stop("initialization")
    -- Logs: [Profile] initialization: 12.5ms
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Debug & Logging](#debug--logging)

---

### Debug Utilities

#### Debug.stackTrace

**Signature:** `Debug.stackTrace() -> string`

Returns current Lua call stack as a formatted string.

**Return Value:**
- Stack trace string with function names and line numbers

**Usage Example:**
```lua
function Update(self, entity)
    if self.debugMode then
        local trace = Debug.stackTrace()
        Log.info("Call stack:\n" .. trace)
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Debug & Logging](#debug--logging)

---

#### Debug.getMemory

**Signature:** `Debug.getMemory() -> number`

Returns current Lua memory usage in kilobytes.

**Return Value:**
- Memory usage in KB

**Usage Example:**
```lua
function Update(self, entity)
    local memory = Debug.getMemory()
    Log.info("Lua memory: " .. memory .. " KB")
    
    if memory > 10000 then
        Log.warning("High memory usage detected")
    end
end
```

[↑ Back to Top](#ggengine-scripting-reference) | [↑ Back to Debug & Logging](#debug--logging)

---

## End of Reference

[↑ Back to Top](#ggengine-scripting-reference)