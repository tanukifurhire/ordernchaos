/*************************************************************************
  \file       GGLuaDefineType.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
	Lua bindings for defining scripted types and their fields.
  \details
	Provides Lua-accessible functions to define new types with fields,
	functions, and serialized members. Also provides metamethods for
	scripted instance indexing, assignment, and garbage collection.
  \dependencies
	- GGLuaVM
	- GGScriptMetadata
	- ECSScript system
  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGLuaDefineType.hpp>

#include <GGSystemManagers.hpp>

/*************************************************************************
  \brief
	Registers the DefineType Lua function in the given Lua VM.
  \param[in,out] luaVM
	Reference to the Lua VM.
*************************************************************************/
void RegisterDefineTypeBindings(GGLuaVM& luaVM)
{
	lua_State* L = luaVM.GetState();

	lua_pushcfunction(L, lua_DefineType);
	lua_setglobal(L, "DefineType");

	std::cout << "[LuaVars] Vars bindings registered" << std::endl;
}

/*************************************************************************
  \brief
	Implements the Lua DefineType function, creating a new type with the given
	name and field definitions.
  \param[in] L
	Pointer to the Lua state.
  \return
	Returns 1 value on the Lua stack: the created type table.
*************************************************************************/
int lua_DefineType(lua_State* L)
{
	// We expect DefineType("TypeName", { definition })
	const char* typeName = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);
	int definitionTableIndex = 2;

	GGScriptMetadata meta{};
	std::unordered_set<std::string> definedFields;
	meta.typeName = typeName;
	
	// Creating the type table which will be returned
	lua_newtable(L);
	int typeIndex = lua_gettop(L);

	lua_pushstring(L, typeName);
	lua_setfield(L, typeIndex, "__type_name");

	// Processing Serializable block
	lua_getfield(L, 2, "Serializable");
	if (lua_istable(L, -1))
	{
		lua_pushnil(L);
		while (lua_next(L, -2) != 0)
		{
			const char* fieldName = lua_tostring(L, -2);
			int valueType = lua_type(L, -1);

			if (!fieldName)
			{
				lua_pop(L, 3);
				return luaL_error(L, "Serializable field key must be a string!");
			}

			if (definedFields.count(fieldName))
			{
				lua_pop(L, 3);
				return luaL_error(L, "Redefinition of identifier '%s'", fieldName);
			}

			if (valueType != LUA_TNUMBER &&
				valueType != LUA_TSTRING &&
				valueType != LUA_TBOOLEAN &&
				valueType != LUA_TTABLE)
			{
				lua_pop(L, 3);
				return luaL_error(L, "Serializable field '%s' has unsupported type: %s"
					, fieldName, lua_typename(L, valueType));
			}

			//std::cout << "Found field " << fieldName << std::endl;

			definedFields.insert(fieldName);
			meta.serializedFields.insert(fieldName);

			// Copy to type table
			lua_pushvalue(L, -2); // key
			lua_pushvalue(L, -2); // value
			lua_settable(L, typeIndex);

			lua_pop(L, 1); // Pop value
		}
	}
	lua_pop(L, 1); // Pop Serializable

	// Processing TextureAssets block
	lua_getfield(L, 2, "TextureAsset");
	if (lua_istable(L, -1))
	{
		lua_pushnil(L);
		while (lua_next(L, -2) != 0)
		{
			const char* fieldName = lua_tostring(L, -2);
			int valueType = lua_type(L, -1);

			if (!fieldName)
			{
				lua_pop(L, 3);
				return luaL_error(L, "TextureAsset field key must be a string!");
			}

			if (definedFields.count(fieldName))
			{
				lua_pop(L, 3);
				return luaL_error(L, "Redefinition of identifier '%s'", fieldName);
			}

			if (valueType != LUA_TNUMBER)
			{
				lua_pop(L, 3);
				return luaL_error(L, "TextureAsset field '%s' has unsupported type: %s"
					, fieldName, lua_typename(L, valueType));
			}

			//std::cout << "Found field " << fieldName << std::endl;

			definedFields.insert(fieldName);
			meta.textureAssetFields.insert(fieldName);

			// Copy to type table
			lua_pushvalue(L, -2); // key
			lua_pushvalue(L, -2); // value
			lua_settable(L, typeIndex);

			lua_pop(L, 1); // Pop value
		}
	}
	lua_pop(L, 1); // Pop TextureAssets

	// Processing Composes block
	lua_getfield(L, 2, "Contains");
	if (lua_istable(L, -1))
	{
		lua_pushnil(L);
		while (lua_next(L, -2) != 0)
		{
			const char* fieldName = lua_tostring(L, -2);
			int valueType = lua_type(L, -1);

			if (!fieldName)
			{
				lua_pop(L, 3);
				return luaL_error(L, "Composition field key must be a string!");
			}

			if (definedFields.count(fieldName))
			{
				lua_pop(L, 3);
				return luaL_error(L, "Redefinition of identifier '%s'", fieldName);
			}

			if (valueType != LUA_TSTRING)
			{
				lua_pop(L, 3);
				return luaL_error(L, "Composition field '%s' has unsupported type : % s"
					, fieldName, lua_typename(L, valueType));
			}

			//std::cout << "Found Composition field " << fieldName << std::endl;

			definedFields.insert(fieldName);
			meta.compositionFields.insert(fieldName);

			// Copy to type table
			lua_pushvalue(L, -2); // key
			lua_pushvalue(L, -2); // value - this contains the name of the table type we want to set as the member!

			lua_settable(L, typeIndex);

			lua_pop(L, 1); // Pop value
		}
	}
	lua_pop(L, 1); // Pop Compose table

	// Processing AudioAssets block
	lua_getfield(L, 2, "AudioAsset");
	if (lua_istable(L, -1))
	{
		lua_pushnil(L);
		while (lua_next(L, -2) != 0)
		{
			const char* fieldName = lua_tostring(L, -2);
			int valueType = lua_type(L, -1);

			if (!fieldName)
			{
				lua_pop(L, 3);
				return luaL_error(L, "AudioAsset field key must be a string!");
			}

			if (definedFields.count(fieldName))
			{
				lua_pop(L, 3);
				return luaL_error(L, "Redefinition of identifier '%s'", fieldName);
			}

			AudioAssetConfig config{};

			if (valueType == LUA_TNUMBER)
			{
				// Plain number � default config, copy value as-is
				lua_pushvalue(L, -2);
				lua_pushvalue(L, -2);
				lua_settable(L, typeIndex);
			}
			else if (valueType == LUA_TTABLE)
			{
				// Table form: { asset = 0, volume = 1.0, pitchVariance = 0.0, loop = false, oneShot = false }
				lua_getfield(L, -1, "asset");
				if (!lua_isnumber(L, -1))
				{
					lua_pop(L, 4);
					return luaL_error(L, "AudioAsset field '%s' table must have a numeric 'asset' field", fieldName);
				}
				int assetId = (int)lua_tointeger(L, -1);
				lua_pop(L, 1);

				lua_getfield(L, -1, "volume");
				if (lua_isnumber(L, -1)) config.volume = (float)lua_tonumber(L, -1);
				lua_pop(L, 1);

				lua_getfield(L, -1, "pitchVariance");
				if (lua_isnumber(L, -1)) config.pitchVariance = (float)lua_tonumber(L, -1);
				lua_pop(L, 1);

				lua_getfield(L, -1, "loop");
				if (lua_isboolean(L, -1)) config.loop = (bool)lua_toboolean(L, -1);
				lua_pop(L, 1);

				lua_getfield(L, -1, "oneShot");
				if (lua_isboolean(L, -1)) config.oneShot = (bool)lua_toboolean(L, -1);
				lua_pop(L, 1);

				// Store only the asset integer in the type table
				lua_pushvalue(L, -2);
				lua_pushinteger(L, assetId);
				lua_settable(L, typeIndex);
			}
			else
			{
				lua_pop(L, 3);
				return luaL_error(L, "AudioAsset field '%s' must be a number or table", fieldName);
			}

			//std::cout << "Found field " << fieldName << std::endl;

			definedFields.insert(fieldName);
			meta.audioAssetFields.insert(fieldName);
			meta.audioAssetConfigs[fieldName] = config;

			lua_pop(L, 1); // Pop value
		}
	}
	lua_pop(L, 1); // Pop AudioAssets

	// Handling regular fields
	lua_pushnil(L);
	while (lua_next(L, definitionTableIndex) != 0)
	{
		const char* key = lua_tostring(L, -2);

		if (!key)
		{
			lua_pop(L, 2);
			return luaL_error(L, "Field key must be string!");
		}

		// Skip keywords/special blocks
		if (strcmp(key, "Serializable") != 0 && 
			strcmp(key, "TextureAsset") != 0 && 
			strcmp(key, "AudioAsset") != 0 &&
			strcmp(key, "Contains") != 0)
		{
			int valueType = lua_type(L, -1);

			if (definedFields.count(key))
			{
				lua_pop(L, 2);
				return luaL_error(L, "Redefinition of identifier '%s'", key);
			}

			if (valueType == LUA_TTHREAD || valueType == LUA_TUSERDATA)
			{
				lua_pop(L, 2);
				return luaL_error(L, "Field '%s' has unsupported type: %s",
					key, lua_typename(L, valueType));
			}

			if (valueType == LUA_TFUNCTION)
			{
				//std::cout << "Found function " << key << std::endl;
				lua_pushvalue(L, -1);
				int funcRef = luaL_ref(L, LUA_REGISTRYINDEX);
				meta.functions[key] = funcRef;

				lua_pushvalue(L, -2);
				lua_pushvalue(L, -2);
				lua_settable(L, typeIndex);

				definedFields.insert(key);
				lua_pop(L, 1);
				continue;
			}

			//std::cout << "Found field " << key << std::endl;
			definedFields.insert(key);
			meta.fields.insert(key);

			// Copy back to type table (flattens)
			lua_pushvalue(L, -2);	// key
			lua_pushvalue(L, -2);	// value
			lua_settable(L, typeIndex);
		}

		lua_pop(L, 1);
	}

	lua_newtable(L);

	lua_pushvalue(L, typeIndex);
	lua_setfield(L, -2, "__index");

	lua_setmetatable(L, typeIndex);

	lua_pushvalue(L, typeIndex);
	meta.defaultValueRef = luaL_ref(L, LUA_REGISTRYINDEX);

	Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>().StoreNewMetadata(typeName, meta);

	CreateTypeMetatable(L, *Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>().GetMetadata(typeName));

	lua_pushvalue(L, typeIndex);
	lua_setglobal(L, typeName);

	return 1;
}

/*************************************************************************
  \brief
	Creates a metatable for a given GGScriptMetadata type and stores it in
	the Lua registry.
  \param[in] L
	Pointer to the Lua state.
  \param[in,out] meta
	Metadata describing the type for which the metatable is created.
*************************************************************************/
void CreateTypeMetatable(lua_State* L, GGScriptMetadata& meta)
{
	if (meta.metatableRef != LUA_NOREF)
	{
		return;
	}

	luaL_newmetatable(L, meta.typeName.c_str());

	lua_pushcfunction(L, InstanceIndex);
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, InstanceNewIndex);
	lua_setfield(L, -2, "__newindex");

	lua_pushcfunction(L, InstanceGC);
	lua_setfield(L, -2, "__gc");

	lua_pushlightuserdata(L, &meta);
	lua_setfield(L, -2, "__type_meta");

	meta.metatableRef = luaL_ref(L, LUA_REGISTRYINDEX);
}

/*************************************************************************
  \brief
	Handles Lua indexing for multiple inheritance types.
  \param[in] L
	Pointer to the Lua state.
  \return
	Returns the value found in the type table or parent types, or nil if not found.
*************************************************************************/
int lua_MultiInheritIndex(lua_State* L)
{
	const char* key = lua_tostring(L, 2);

	lua_pushvalue(L, lua_upvalueindex(2)); // Get type table
	lua_getfield(L, -1, key);
	if (!lua_isnil(L, -1))
	{
		return 1; // Found in own table
	}
	lua_pop(L, 2);

	lua_pushvalue(L, lua_upvalueindex(1)); // Get parent array
	size_t numParents = lua_objlen(L, -1);

	for (size_t i{ 1 }; i <= numParents; i++)
	{
		lua_rawgeti(L, -1, static_cast<int>(i));
		const char* parentName = lua_tostring(L, -1);
		lua_pop(L, -1);

		lua_getglobal(L, parentName);
		if (lua_istable(L, -1))
		{
			lua_getfield(L, -1, key);

			if (!lua_isnil(L, -1))
			{
				lua_remove(L, -2); // Remove parent table
				lua_remove(L, -2); // Remvoe parents array
				
				return 1;
			}

			lua_pop(L, 1);
		}
		lua_pop(L, 1);
	}

	lua_pop(L, 1);
	lua_pushnil(L);
	return 1;
}

/*************************************************************************
  \brief
	Handles Lua __index metamethod for a scripted instance.
  \param[in] L
	Pointer to the Lua state.
  \return
	Returns the value of the requested field on the Lua stack, or nil if not found.
*************************************************************************/
int InstanceIndex(lua_State* L)
{
	// L[1] = userdata (instance wrapper)
	// L[2] = key (field or method name)

	if (!lua_isuserdata(L, 1))
	{
		return luaL_error(L, "InstanceIndex: Expected userdata, got %s",
			lua_typename(L, lua_type(L, 1)));
	}

	InstanceUserdata* ud = (InstanceUserdata*)lua_touserdata(L, 1);
	if (!ud)
	{
		return luaL_error(L, "InstanceIndex: NULL userdata pointer");
	}

	if (!ud->instance)
	{
		return luaL_error(L, "InstanceIndex: NULL instance pointer");
	}

	if (!lua_isstring(L, 2))
	{
		lua_pushnil(L);
		return 1;
	}

	const char* key = lua_tostring(L, 2);
	EntityInstance* instance = ud->instance;
	GGScriptMetadata* meta = instance->type;

	if (!meta)
	{
		return luaL_error(L, "InstanceIndex: NULL metadata pointer");
	}

	// First, check if field has C++ storage (Serializable/TextureAsset)
	// Skip composition fields - they resolve from Lua table as userdata sub-instances
	auto fieldIt = meta->fieldNameToIndex.find(key);
	if (fieldIt != meta->fieldNameToIndex.end() && meta->compositionFields.count(key) == 0)
	{
		// Field has C++ storage - retrieve from instance data
		size_t fieldIdx = fieldIt->second;

		if (fieldIdx >= meta->fieldDescriptors.size())
		{
			return luaL_error(L, "InstanceIndex: Field index %zu out of bounds (size: %zu)",
				fieldIdx, meta->fieldDescriptors.size());
		}

		const FieldDescriptor& fieldDescriptor = meta->fieldDescriptors[fieldIdx];

		if (!instance->data)
		{
			return luaL_error(L, "InstanceIndex: NULL instance data pointer for field '%s'", key);
		}

		char* data = static_cast<char*>(instance->data);
		void* fieldPtr = data + fieldDescriptor.offset;

		switch (fieldDescriptor.type)
		{
		case FieldDescriptor::TYPE_INT:
			lua_pushinteger(L, *static_cast<int*>(fieldPtr));
			break;
		case FieldDescriptor::TYPE_FLOAT:
			lua_pushnumber(L, *static_cast<float*>(fieldPtr));
			break;
		case FieldDescriptor::TYPE_STRING:
			lua_pushstring(L, static_cast<std::string*>(fieldPtr)->c_str());
			break;
		case FieldDescriptor::TYPE_BOOL:
			lua_pushboolean(L, *static_cast<bool*>(fieldPtr));
			break;
		default:
			return luaL_error(L, "InstanceIndex: Unknown field type %d for field '%s'",
				fieldDescriptor.type, key);
		}

		return 1;
	}

	// Field not in C++ storage - check Lua table
	if (instance->luaFieldsRef != LUA_NOREF && instance->luaFieldsRef != LUA_REFNIL)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, instance->luaFieldsRef);
		if (lua_istable(L, -1))
		{
			lua_getfield(L, -1, key);
			if (!lua_isnil(L, -1))
			{
				lua_remove(L, -2);  // Remove the table, keep the value
				return 1;
			}
			lua_pop(L, 1);  // Pop nil
		}
		lua_pop(L, 1);  // Pop table or non-table value
	}

	// Check type functions (enables self:DoThis() syntax)
	auto funcIt = meta->functions.find(key);
	if (funcIt != meta->functions.end())
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, funcIt->second);
		return 1;
	}

	// Field not found anywhere - return nil
	lua_pushnil(L);
	return 1;
}

/*************************************************************************
  \brief
	Handles Lua __newindex metamethod for a scripted instance.
  \param[in] L
	Pointer to the Lua state.
  \return
	Sets the field to the given value, or raises an error if types mismatch.
*************************************************************************/
int InstanceNewIndex(lua_State* L)
{
	// L[1] = userdata (instance wrapper)
	// L[2] = key (field name)
	// L[3] = value

	if (!lua_isuserdata(L, 1))
	{
		return luaL_error(L, "InstanceNewIndex: Expected userdata, got %s",
			lua_typename(L, lua_type(L, 1)));
	}

	InstanceUserdata* ud = (InstanceUserdata*)lua_touserdata(L, 1);
	if (!ud)
	{
		return luaL_error(L, "InstanceNewIndex: NULL userdata pointer");
	}

	if (!ud->instance)
	{
		return luaL_error(L, "InstanceNewIndex: NULL instance pointer");
	}

	if (!lua_isstring(L, 2))
	{
		lua_pushnil(L);
		return 1;
	}

	const char* key = lua_tostring(L, 2);
	EntityInstance* instance = ud->instance;
	GGScriptMetadata* meta = instance->type;

	// First, check if field has C++ storage (Serializable/TextureAsset)
	// Skip composition fields - they are stored as userdata sub-instances in Lua table
	auto fieldIt = meta->fieldNameToIndex.find(key);
	if (fieldIt != meta->fieldNameToIndex.end() && meta->compositionFields.count(key) == 0)
	{
		// Field has C++ storage - set in instance data
		size_t fieldIdx = fieldIt->second;

		if (fieldIdx >= meta->fieldDescriptors.size())
		{
			return luaL_error(L, "InstanceNewIndex: Field index %zu out of bounds (size: %zu)",
				fieldIdx, meta->fieldDescriptors.size());
		}

		const FieldDescriptor& fieldDescriptor = meta->fieldDescriptors[fieldIdx];

		if (!instance->data)
		{
			return luaL_error(L, "InstanceNewIndex: NULL instance data pointer for field '%s'", key);
		}

		char* dataPtr = static_cast<char*>(instance->data);
		void* fieldPtr = dataPtr + fieldDescriptor.offset;

		switch (fieldDescriptor.type)
		{
		case FieldDescriptor::TYPE_INT:
			if (!lua_isnumber(L, 3))
			{
				return luaL_error(L, "InstanceNewIndex: Field '%s' expects integer", key);
			}
			*static_cast<int*>(fieldPtr) = static_cast<int>(lua_tointeger(L, 3));
			break;
		case FieldDescriptor::TYPE_FLOAT:
			if (!lua_isnumber(L, 3))
			{
				return luaL_error(L, "InstanceNewIndex: Field '%s' expects number", key);
			}
			*static_cast<float*>(fieldPtr) = static_cast<float>(lua_tonumber(L, 3));
			break;
		case FieldDescriptor::TYPE_STRING:
			if (!lua_isstring(L, 3)) {
				return luaL_error(L, "InstanceNewIndex: Field '%s' expects string", key);
			}
			*static_cast<std::string*>(fieldPtr) = lua_tostring(L, 3);
			break;
		case FieldDescriptor::TYPE_BOOL:
			if (!lua_isboolean(L, 3)) {
				return luaL_error(L, "InstanceNewIndex: Field '%s' expects boolean", key);
			}
			*static_cast<bool*>(fieldPtr) = lua_toboolean(L, 3);
			break;
		default:
			return luaL_error(L, "InstanceNewIndex: Unknown field type %d for field '%s'",
				fieldDescriptor.type, key);
		}

		return 0;
	}

	// Field not in C++ storage - set in Lua table
	if (instance->luaFieldsRef != LUA_NOREF && instance->luaFieldsRef != LUA_REFNIL)
	{
		lua_rawgeti(L, LUA_REGISTRYINDEX, instance->luaFieldsRef);
		if (lua_istable(L, -1))
		{
			lua_pushvalue(L, 3);  // Push the value to set
			lua_setfield(L, -2, key);
			lua_pop(L, 1);  // Pop the table
			return 0;
		}
		lua_pop(L, 1);  // Pop non-table value
	}

	// Prevent overwriting type functions
	if (meta->functions.find(key) != meta->functions.end())
	{
		return luaL_error(L, "InstanceNewIndex: Cannot overwrite function '%s'", key);
	}

	// Field not found in defined fields - error
	return luaL_error(L, "InstanceNewIndex: Cannot set unknown field '%s'", key);
}

/*************************************************************************
  \brief
	Handles Lua garbage collection (__gc) for a scripted instance.
  \param[in] L
	Pointer to the Lua state.
  \return
	Cleans up the InstanceUserdata without deleting the underlying instance.
*************************************************************************/
int InstanceGC(lua_State* L)
{
	if (!lua_isuserdata(L, 1)) return 0;

	InstanceUserdata* ud = (InstanceUserdata*)lua_touserdata(L, 1);

	if (!ud) return 0;

	// We don't need to delete, just invalidate the userdata pointer
	// Delete is handled by C++ when we clear instances
	ud->instance = nullptr;

	return 0;
}