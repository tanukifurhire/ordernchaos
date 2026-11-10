/*************************************************************************
  \file       GGScriptAsset.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
    Defines script-related structures and RAII helpers for Lua integration.

  \details
    Provides:
      - GGScriptAsset for storing Lua module/function references.
      - Metadata structures for script type description and serialization.
      - EntityInstance wrapper for ECS-bound script data.
      - RAII guards for Lua stack safety, allocated memory, and instance cleanup.
*************************************************************************/

#pragma once

#include <GGLogger.hpp>
#include <GGMemoryManager.hpp>
#include <GGECSInterface.hpp>
#include <string>
#include <array>
#include <vector>

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

typedef struct lua_State lua_State;

/*************************************************************************
  \struct GGScriptAsset
  \brief 
    Stores references to Lua script module and functions.
  \details
    - moduleRef: Reference to the Lua module table.
    - startFunctionRef: Reference to the 'start' function.
    - updateFunctionRef: Reference to the 'update' function.
*************************************************************************/
struct GGScriptAsset
{
	int moduleRef = -2;
	int startFunctionRef = -2;
	int updateFunctionRef = -2;
};

/*************************************************************************
  \struct FieldDescriptor
  \brief 
    Describes a single field of a script type for serialization.
  \details
    - name: Field name.
    - offset: Offset within the data structure.
    - type: Enum indicating field type (int, float, string, bool).
    - size: Size of the field in bytes.
*************************************************************************/
struct FieldDescriptor
{
	std::string name{};
	size_t offset{};
	enum Type { TYPE_INT, TYPE_FLOAT, TYPE_STRING, TYPE_BOOL } type{};
	size_t size{};
};

struct AudioAssetConfig
{
    float volume = 1.0f;
    float pitchVariance = 1.0f;
    bool loop = false;
    bool oneShot = false;
};

/*************************************************************************
  \struct GGScriptMetadata
  \brief 
    Metadata describing a Lua script type.
  \details
    - defaultValueRef: Lua reference for default value table.
    - metatableRef: Lua reference to the metatable.
    - typeName: Name of the script type.
    - serializedFields: Set of field names to serialize.
    - fields: Set of all field names.
    - fieldDescriptors: Vector describing all fields.
    - fieldNameToIndex: Map from field name to descriptor index.
    - functions: Map from function name to Lua reference.
    - dataSize: Size of the data structure in bytes.
*************************************************************************/
struct GGScriptMetadata
{
	int defaultValueRef = -2;
	int metatableRef = -2;

	std::string typeName{};

	std::unordered_set<std::string> serializedFields;
    std::unordered_set<std::string> textureAssetFields;
    std::unordered_set<std::string> compositionFields;
    std::unordered_set<std::string> audioAssetFields;
    std::unordered_map<std::string, AudioAssetConfig> audioAssetConfigs;
	std::unordered_set<std::string> fields;

	std::vector<FieldDescriptor> fieldDescriptors;
	std::unordered_map<std::string, size_t> fieldNameToIndex;
	std::unordered_map<std::string, int> functions;
	size_t dataSize{};
};

/*************************************************************************
  \struct EntityInstance
  \brief
    Represents a script-bound ECS entity instance.
  \details
    - entity: ECS entity ID.
    - type: Pointer to GGScriptMetadata describing the type.
    - data: Pointer to instance data in memory (for Serializable/TextureAsset fields).
    - luaFieldsRef: Lua registry reference to table storing non-serializable fields.
*************************************************************************/
struct EntityInstance
{
	Entity entity = NULL_ENTITY;
	GGScriptMetadata* type = nullptr;
	void* data = nullptr;
	int luaFieldsRef = LUA_NOREF;  // Lua table for non-serializable fields

};

/*************************************************************************
  \struct InstanceUserdata
  \brief 
    Wrapper for passing EntityInstance to Lua.
*************************************************************************/
struct InstanceUserdata
{
	EntityInstance* instance = nullptr;
};

/*************************************************************************
  \class LuaStackGuard
  \brief 
    RAII helper to track Lua stack balance.
  \details
    - Records initial stack top on construction.
    - On destruction, restores stack and logs warning if imbalance detected.
*************************************************************************/
class LuaStackGuard
{
public:
    explicit LuaStackGuard(lua_State* L)
        : L(L), initialTop(lua_gettop(L))
    {
    }

    ~LuaStackGuard()
    {
        int currentTop = lua_gettop(L);
        if (currentTop != initialTop)
        {
            // Restore stack to initial state
            lua_settop(L, initialTop);
            GGLogger::Warning("Lua stack imbalance detected (expected "
                + std::to_string(initialTop)
                + ", got "
                + std::to_string(currentTop));
        }
    }

    // Disable copy/move
    LuaStackGuard(const LuaStackGuard&) = delete;
    LuaStackGuard& operator=(const LuaStackGuard&) = delete;

private:
    lua_State* L;
    int initialTop;
};

/*************************************************************************
  \class MemoryGuard
  \brief 
    RAII wrapper for malloc'd memory.
  \details
    - Frees memory in destructor unless released manually.
*************************************************************************/
// Memory guard - ensures malloc'd memory is freed on exception
class MemoryGuard
{
public:
    explicit MemoryGuard(void* ptr) : ptr(ptr) {}

    ~MemoryGuard()
    {
        if (ptr)
        {
            GGMemoryManager::GetInstance().Free(ptr);
        }
    }

    void* Release()
    {
        void* temp = ptr;
        ptr = nullptr;
        return temp;
    }

    MemoryGuard(const MemoryGuard&) = delete;
    MemoryGuard& operator=(const MemoryGuard&) = delete;

private:
    void* ptr;
};

/*************************************************************************
  \class EntityInstanceGuard
  \brief
    RAII wrapper for EntityInstance cleanup.
  \details
    - Ensures string fields are properly destroyed.
    - Releases Lua field table reference.
    - Frees instance data and deletes the EntityInstance object.
*************************************************************************/
// EntityInstance guard - ensures proper cleanup on exception
class EntityInstanceGuard
{
public:
    explicit EntityInstanceGuard(EntityInstance* instance, lua_State* L = nullptr)
        : instance(instance), L(L)
    {
    }

    ~EntityInstanceGuard()
    {
        if (instance)
        {
            // Clean up string fields before freeing data
            if (instance->type && instance->data)
            {
                for (const auto& field : instance->type->fieldDescriptors)
                {
                    if (field.type == FieldDescriptor::TYPE_STRING)
                    {
                        char* dataPtr = static_cast<char*>(instance->data);
                        void* fieldPtr = dataPtr + field.offset;
                        // Call destructor for std::string
                        static_cast<std::string*>(fieldPtr)->~basic_string();
                    }
                }
            }

            if (instance->data)
            {
                GGMemoryManager::GetInstance().Free(instance->data);
            }

            // Release Lua fields table reference
            if (L && instance->luaFieldsRef != LUA_NOREF)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, instance->luaFieldsRef);
            }

            instance->~EntityInstance();
            GGMemoryManager::GetInstance().Free(instance);
        }
    }

    EntityInstance* Release()
    {
        EntityInstance* temp = instance;
        instance = nullptr;
        return temp;
    }

    EntityInstanceGuard(const EntityInstanceGuard&) = delete;
    EntityInstanceGuard& operator=(const EntityInstanceGuard&) = delete;

private:
    EntityInstance* instance;
    lua_State* L;
};

