/*************************************************************************
  \file       GGECSGameObjectBridge.hpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       07 November 2025
  \version    1.0
  \brief
	Defines JSON serialization and deserialization templates for ECS components.

  \details
	Provides specialized ComponentSerializer templates for various ECS components,
    enabling conversion between component data and JSON using RapidJSON.
    Used for prefab saving/loading, scene serialization, and editor inspection.

  \dependencies
	<iostream>, <fstream>, <sstream>, <string>,
	<GGECSComponents.hpp>, <GGJSONUtils.hpp>, <GGPrefabManager.hpp>,
    <GGECSWorld.hpp>, <GGECSAssetInterface.hpp>, <uuid.h>
	<string> <optional>

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include <string>
#include <optional>

#include <GGECSInterface.hpp>
#include <GGECSWorld.hpp>
#include <GGECSComponents.hpp>
#include <GGMath/Types.hpp>
#include <GGSystemManagers.hpp>
#include <uuid.h>
#include <GGECSAssetInterface.hpp>
#include <GGPrefabManager.hpp>
#include <GGJSONUtils.hpp>

/*************************************************************************
  \brief
	Template interface for component JSON serialization
  \details
	Simple template for converting components to/from JSON. Each component
	type needs a specialization. Used for save games, level data, and
	debugging output in turn-based gameplay.
  \tparam T
	Component type that implements IsComponent concept
*************************************************************************/
template <IsComponent T>
struct ComponentSerializer;

/*************************************************************************
  \struct ComponentSerializer<Rigidbody2DFloat>
  \brief
    JSON serialization for 2D physics bodies (stub implementation).
  \warning
    Current implementation serializes empty object. Velocity, mass, and flags
    are not persisted. This is intentional for turn-based games where physics
    state is recomputed each turn. For real-time games, extend this to serialize
    vx, vy, mass, and flags fields.
*************************************************************************/
template<>
struct ComponentSerializer<Rigidbody2DFloat> {
	static void ToJSON(Rigidbody2DFloat const& rigidbody, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start Rigidbody2DFloat object

		writer.Key("vx"), writer.Double(rigidbody.vx);
		writer.Key("vy"), writer.Double(rigidbody.vy);
		writer.Key("mass"), writer.Double(rigidbody.mass);
		writer.Key("flags"), writer.Int(rigidbody.flags);

		writer.EndObject(); // end Rigidbody2DFloat object
	}
	static Rigidbody2DFloat FromJSON(const rapidjson::Value& json) {
		Rigidbody2DFloat component{};

		component.vx = json.HasMember("vx") && json["vx"].IsNumber() ? json["vx"].GetFloat() : 0.f;
		component.vy = json.HasMember("vy") && json["vy"].IsNumber() ? json["vy"].GetFloat() : 0.f;
		component.mass = json.HasMember("mass") && json["mass"].IsNumber() ? json["mass"].GetFloat() : 0.f;
		component.flags = json.HasMember("flags") && json["flags"].IsNumber() ? json["flags"].GetInt() : 0;

		return component;
	}
};


/*************************************************************************
  \struct ComponentSerializer<MeshRenderer>
  \brief
    JSON serialization for mesh rendering components (stub implementation).
  \warning
    GGMesh contains non-serializable GPU resources (VAO/VBO handles). Current
    implementation serializes empty object. The correct approach is to serialize
    shaderAssetID and textureAssetID (uint32_t handles resolved via
    GGECSStandardAssetManager) and reconstruct the mesh from asset paths on load.
  \note
    Consider splitting MeshRenderer into MeshReference (serializable asset ID)
    and MeshGPUData (runtime GPU handles) for cleaner separation of concerns.
*************************************************************************/
template<>
struct ComponentSerializer<MeshRenderer> {
	static void ToJSON(MeshRenderer const& meshRenderer, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start MeshRenderer object

		{
			auto uuidOpt = GGECSStandardAssetManager<GGShader>::GetUUIDFromID(meshRenderer.shaderAssetID);
			writer.Key("shaderAssetUUID");
			if (uuidOpt.has_value()) {
				writer.String(uuids::to_string(uuidOpt.value()).c_str());

				// Print the file path for debug
				const std::string& filePath = GGAssetID::GetGGAssetID()->GetFilePath(uuidOpt.value());
				std::cout << "[ToJSON] Shader file path: " << filePath << std::endl;
			}
			else {
				writer.String(""); // fallback
				std::cerr << "[ToJSON] Warning: No UUID found for shaderAssetID "
					<< meshRenderer.shaderAssetID << std::endl;
			}
		}

		writer.Key("colorAdd");
		writer.StartObject();
		writer.Key("r"), writer.Double(meshRenderer.colorAdd.r);
		writer.Key("g"), writer.Double(meshRenderer.colorAdd.g);
		writer.Key("b"), writer.Double(meshRenderer.colorAdd.b);
		writer.Key("a"), writer.Double(meshRenderer.colorAdd.a);
		writer.EndObject();

		writer.Key("colorMultiply");
		writer.StartObject();
		writer.Key("r"), writer.Double(meshRenderer.colorMultiply.r);
		writer.Key("g"), writer.Double(meshRenderer.colorMultiply.g);
		writer.Key("b"), writer.Double(meshRenderer.colorMultiply.b);
		writer.Key("a"), writer.Double(meshRenderer.colorMultiply.a);
		writer.EndObject();

		writer.Key("ySort");
		writer.Bool(meshRenderer.ySort);

		writer.Key("layer");
		writer.Int(meshRenderer.layer);

		writer.Key("pixelPerfectPicking");
		writer.Bool(meshRenderer.pixelPerfectPicking);

		writer.Key("pickableObject");
		writer.Bool(meshRenderer.pickableObject);

		writer.EndObject(); // end MeshRenderer object
	}

	static MeshRenderer FromJSON(const rapidjson::Value& json) {
		MeshRenderer component{};

		// --- Shader ---
		if (json.HasMember("shaderAssetUUID") && json["shaderAssetUUID"].IsString())
		{
			std::optional<uuids::uuid> uuid = uuids::uuid::from_string(
				json["shaderAssetUUID"].GetString()
			);

			if (uuid) {
				auto& assetSystem = GGAssetID::GetGGAssetID();

				if (assetSystem->DoesUUIDExist(uuid.value())) {
					const std::string& filePath = assetSystem->GetFilePath(uuid.value());
					uint32_t assetID = GGECSStandardAssetManager<GGShader>::RegisterAsset(uuid.value());
					component.shaderAssetID = assetID;

					if (assetID)
						std::cout << "[FromJSON] Registered and loaded shader: " << filePath
						<< " with ID " << assetID << std::endl;
					else
						std::cout << "[FromJSON] No Asset ID Registered, default to 0." << std::endl;
				}
				else {
					std::cerr << "[MeshRenderer::FromJSON] Shader UUID does not exist: "<< uuids::to_string(uuid.value()) << std::endl;
					//assetSystem->PrintAll();
				}
			}
			else {
				std::cerr << "[MeshRenderer::FromJSON] Invalid shader UUID string: "
					<< json["shaderAssetUUID"].GetString() << std::endl;
			}
		}

		if (json.HasMember("colorAdd") && json["colorAdd"].IsObject())
		{
			if (json["colorAdd"].HasMember("r") && json["colorAdd"]["r"].IsDouble())
			{
				component.colorAdd.r = static_cast<float>(json["colorAdd"]["r"].GetDouble());
			}

			if (json["colorAdd"].HasMember("g") && json["colorAdd"]["g"].IsDouble())
			{
				component.colorAdd.g = static_cast<float>(json["colorAdd"]["g"].GetDouble());
			}

			if (json["colorAdd"].HasMember("b") && json["colorAdd"]["b"].IsDouble())
			{
				component.colorAdd.b = static_cast<float>(json["colorAdd"]["b"].GetDouble());
			}

			if (json["colorAdd"].HasMember("a") && json["colorAdd"]["a"].IsDouble())
			{
				component.colorAdd.a = static_cast<float>(json["colorAdd"]["a"].GetDouble());
			}
		}

		if (json.HasMember("colorMultiply") && json["colorMultiply"].IsObject())
		{
			if (json["colorMultiply"].HasMember("r") && json["colorMultiply"]["r"].IsDouble())
			{
				component.colorMultiply.r = static_cast<float>(json["colorMultiply"]["r"].GetDouble());
			}

			if (json["colorMultiply"].HasMember("g") && json["colorMultiply"]["g"].IsDouble())
			{
				component.colorMultiply.g = static_cast<float>(json["colorMultiply"]["g"].GetDouble());
			}

			if (json["colorMultiply"].HasMember("b") && json["colorMultiply"]["b"].IsDouble())
			{
				component.colorMultiply.b = static_cast<float>(json["colorMultiply"]["b"].GetDouble());
			}

			if (json["colorMultiply"].HasMember("a") && json["colorMultiply"]["a"].IsDouble())
			{
				component.colorMultiply.a = static_cast<float>(json["colorMultiply"]["a"].GetDouble());
			}
		}

		if (json.HasMember("ySort") && json["ySort"].IsBool())
		{
			component.ySort = json["ySort"].GetBool();
		}

		if (json.HasMember("layer") && json["layer"].IsInt())
		{
			component.layer = json["layer"].GetInt();
		}
		if (json.HasMember("pixelPerfectPicking") && json["pixelPerfectPicking"].IsBool())
		{
			component.pixelPerfectPicking = json["pixelPerfectPicking"].GetBool();
		}
		if (json.HasMember("pickableObject") && json["pickableObject"].IsBool())
		{
			component.pickableObject = json["pickableObject"].GetBool();
		}
		return component;
	}
};

template<>
struct ComponentSerializer<UIComponent> {
	static void ToJSON(UIComponent const& meshRenderer, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start UIComponent object

		{
			auto uuidOpt = GGECSStandardAssetManager<GGShader>::GetUUIDFromID(meshRenderer.shaderAssetID);
			writer.Key("shaderAssetUUID");
			if (uuidOpt.has_value()) {
				writer.String(uuids::to_string(uuidOpt.value()).c_str());

				// Print the file path for debug
				const std::string& filePath = GGAssetID::GetGGAssetID()->GetFilePath(uuidOpt.value());
				std::cout << "[ToJSON] Shader file path: " << filePath << std::endl;
			}
			else {
				writer.String(""); // fallback
				std::cerr << "[ToJSON] Warning: No UUID found for shaderAssetID "
					<< meshRenderer.shaderAssetID << std::endl;
			}
		}

		writer.Key("colorAdd");
		writer.StartObject();
		writer.Key("r"), writer.Double(meshRenderer.colorAdd.r);
		writer.Key("g"), writer.Double(meshRenderer.colorAdd.g);
		writer.Key("b"), writer.Double(meshRenderer.colorAdd.b);
		writer.Key("a"), writer.Double(meshRenderer.colorAdd.a);
		writer.EndObject();

		writer.Key("colorMultiply");
		writer.StartObject();
		writer.Key("r"), writer.Double(meshRenderer.colorMultiply.r);
		writer.Key("g"), writer.Double(meshRenderer.colorMultiply.g);
		writer.Key("b"), writer.Double(meshRenderer.colorMultiply.b);
		writer.Key("a"), writer.Double(meshRenderer.colorMultiply.a);
		writer.EndObject();

		writer.Key("ySort");
		writer.Bool(meshRenderer.ySort);

		writer.Key("layer");
		writer.Int(meshRenderer.layer);
		writer.Key("pixelPerfectPicking");
		writer.Bool(meshRenderer.pixelPerfectPicking);

		writer.Key("pickableObject");
		writer.Bool(meshRenderer.pickableObject);

		writer.EndObject(); // end MeshRenderer object
	}

	static UIComponent FromJSON(const rapidjson::Value& json) {
		UIComponent component{};

		// --- Shader ---
		if (json.HasMember("shaderAssetUUID") && json["shaderAssetUUID"].IsString())
		{
			std::optional<uuids::uuid> uuid = uuids::uuid::from_string(
				json["shaderAssetUUID"].GetString()
			);

			if (uuid) {
				auto& assetSystem = GGAssetID::GetGGAssetID();

				if (assetSystem->DoesUUIDExist(uuid.value())) {
					const std::string& filePath = assetSystem->GetFilePath(uuid.value());
					uint32_t assetID = GGECSStandardAssetManager<GGShader>::RegisterAsset(uuid.value());
					component.shaderAssetID = assetID;

					if (assetID)
						std::cout << "[FromJSON] Registered and loaded shader: " << filePath
						<< " with ID " << assetID << std::endl;
					else
						std::cout << "[FromJSON] No Asset ID Registered, default to 0." << std::endl;
				}
				else {
					std::cerr << "[UIComponent::FromJSON] Shader UUID does not exist: " << uuids::to_string(uuid.value()) << std::endl;
					//assetSystem->PrintAll();
				}
			}
			else {
				std::cerr << "[UIComponent::FromJSON] Invalid shader UUID string: "
					<< json["shaderAssetUUID"].GetString() << std::endl;
			}
		}

		if (json.HasMember("colorAdd") && json["colorAdd"].IsObject())
		{
			if (json["colorAdd"].HasMember("r") && json["colorAdd"]["r"].IsDouble())
			{
				component.colorAdd.r = static_cast<float>(json["colorAdd"]["r"].GetDouble());
			}

			if (json["colorAdd"].HasMember("g") && json["colorAdd"]["g"].IsDouble())
			{
				component.colorAdd.g = static_cast<float>(json["colorAdd"]["g"].GetDouble());
			}

			if (json["colorAdd"].HasMember("b") && json["colorAdd"]["b"].IsDouble())
			{
				component.colorAdd.b = static_cast<float>(json["colorAdd"]["b"].GetDouble());
			}

			if (json["colorAdd"].HasMember("a") && json["colorAdd"]["a"].IsDouble())
			{
				component.colorAdd.a = static_cast<float>(json["colorAdd"]["a"].GetDouble());
			}
		}

		if (json.HasMember("colorMultiply") && json["colorMultiply"].IsObject())
		{
			if (json["colorMultiply"].HasMember("r") && json["colorMultiply"]["r"].IsDouble())
			{
				component.colorMultiply.r = static_cast<float>(json["colorMultiply"]["r"].GetDouble());
			}

			if (json["colorMultiply"].HasMember("g") && json["colorMultiply"]["g"].IsDouble())
			{
				component.colorMultiply.g = static_cast<float>(json["colorMultiply"]["g"].GetDouble());
			}

			if (json["colorMultiply"].HasMember("b") && json["colorMultiply"]["b"].IsDouble())
			{
				component.colorMultiply.b = static_cast<float>(json["colorMultiply"]["b"].GetDouble());
			}

			if (json["colorMultiply"].HasMember("a") && json["colorMultiply"]["a"].IsDouble())
			{
				component.colorMultiply.a = static_cast<float>(json["colorMultiply"]["a"].GetDouble());
			}
		}

		if (json.HasMember("ySort") && json["ySort"].IsBool())
		{
			component.ySort = json["ySort"].GetBool();
		}

		if (json.HasMember("layer") && json["layer"].IsInt())
		{
			component.layer = json["layer"].GetInt();
		}
		if (json.HasMember("pixelPerfectPicking") && json["pixelPerfectPicking"].IsBool())
		{
			component.pixelPerfectPicking = json["pixelPerfectPicking"].GetBool();
		}

		if (json.HasMember("pickableObject") && json["pickableObject"].IsBool())
		{
			component.pickableObject = json["pickableObject"].GetBool();
		}

		return component;
	}
};

/*************************************************************************
  \struct ComponentSerializer<AABBFloat>
  \brief
    JSON serialization for axis-aligned bounding boxes (stub implementation).
  \warning
    Current implementation serializes empty object. AABB bounds are typically
    derived from mesh or sprite dimensions and should be recomputed on load
    rather than persisted. If explicit persistence is required, serialize
    top/bottom/left/right fields similar to Transform2DFloat pattern.
*************************************************************************/
template<>
struct ComponentSerializer<AABBFloat> {
	static void ToJSON(AABBFloat const& aabb, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start AABBFloat object

		writer.Key("top"), writer.Double(aabb.top);
		writer.Key("bottom"), writer.Double(aabb.bottom);
		writer.Key("left"), writer.Double(aabb.left);
		writer.Key("right"), writer.Double(aabb.right);

		writer.EndObject(); // end AABBFloat object
	}

	static AABBFloat FromJSON(const rapidjson::Value& json) {
		AABBFloat component{};

		component.top = json.HasMember("top") && json["top"].IsNumber() ? json["top"].GetFloat() : 0.f;
		component.bottom = json.HasMember("bottom") && json["bottom"].IsNumber() ? json["bottom"].GetFloat() : 0.f;
		component.left = json.HasMember("left") && json["left"].IsNumber() ? json["left"].GetFloat() : 0.f;
		component.right = json.HasMember("right") && json["right"].IsNumber() ? json["right"].GetFloat() : 0.f;

		return component;
	}
};

/*************************************************************************
  \struct ComponentSerializer<Health>
  \brief
    JSON serialization for health/damage components.
  \details
    Serializes both maxHp and currentHp for save/load of game state mid-combat.
    Critical for turn-based games where health must persist across save boundaries.
  \note
    FromJSON is stubbed (returns default-constructed Health). Implementation
    should mirror ToJSON pattern with HasMember checks for maxHp/currentHp fields.
*************************************************************************/
template<>
struct ComponentSerializer<Health> {
	static void ToJSON(Health const& health, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject();

		// Serialize here...
		writer.Key("maxHp"), writer.Int(health.maxHp);
		writer.Key("currentHp"), writer.Int(health.currentHp);

		writer.EndObject();
	}

	static Health FromJSON(const rapidjson::Value& json) {
		Health component{};

		(void)json;

		return component;
	}
};

/*************************************************************************
  \struct ComponentSerializer<MovementInput>
  \brief
    JSON serialization for keyboard input bindings (stub implementation).
  \warning
    Input bindings are typically configuration data, not game state. Consider
    moving to a separate InputConfig system. If persistence is required, serialize
    keyUp/keyDown/keyLeft/keyRight/speed fields as integers and float respectively.
*************************************************************************/
template<>
struct ComponentSerializer<MovementInput> {
	static void ToJSON(MovementInput const& movementInput, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start MovementInput object

		writer.Key("keyUp");
		char str[2] = { static_cast<char>(movementInput.keyUp), '\0' };
		writer.String(str);
		writer.Key("keyDown");
		str[0] = static_cast<char>(movementInput.keyDown);
		writer.String(str);
		writer.Key("keyLeft");
		str[0] = static_cast<char>(movementInput.keyLeft);
		writer.String(str);
		writer.Key("keyRight");
		str[0] = static_cast<char>(movementInput.keyRight);
		writer.String(str);
		writer.Key("speed"), writer.Double(movementInput.speed);

		writer.EndObject(); // end MovementInput object
	}

	static MovementInput FromJSON(const rapidjson::Value& json) {
		MovementInput component{};

		// lambda helper to get key value as int (ASCII code)
		auto getKeyValue = [](const rapidjson::Value& keyData) -> int {
			int keyValue = 0;
			if (keyData.IsInt()) {
				return keyData.GetInt();
			}
			else if (keyData.IsString()) {
				const char* str = keyData.GetString();
				// cast str to int
				keyValue = static_cast<int>(str[0]);
			}
			return keyValue;
		};

		component.keyUp = json.HasMember("keyUp") && json["keyUp"].IsString() ? getKeyValue(json["keyUp"]) : 'W';
		component.keyDown = json.HasMember("keyDown") && json["keyDown"].IsString() ? getKeyValue(json["keyDown"]) : 'S';
		component.keyUp = json.HasMember("keyLeft") && json["keyLeft"].IsString() ? getKeyValue(json["keyLeft"]) : 'A';
		component.keyUp = json.HasMember("keyRight") && json["keyRight"].IsString() ? getKeyValue(json["keyRight"]) : 'D';
		component.speed = json.HasMember("speed") && json["speed"].IsNumber() ? json["speed"].GetFloat() : 200.f;

		return component;
	}
};

/*************************************************************************
  \struct ComponentSerializer<ClickToMove>
  \brief
    JSON serialization for pathfinding target state (stub implementation).
  \warning
    Pathfinding state (targetX, targetY, hasTarget, isNetworked) is runtime
    ephemeral data. Most turn-based games should not persist this; targets are
    recomputed on load. If required for undo/redo systems, implement serialization
    of all four fields.
*************************************************************************/
template<>
struct ComponentSerializer<ClickToMove> {
	static void ToJSON(ClickToMove const& clickToMove, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start ClickToMove object

		writer.Key("targetX"), writer.Double(clickToMove.targetX);
		writer.Key("targetY"), writer.Double(clickToMove.targetY);
		writer.Key("hasTarget"), writer.Bool(clickToMove.hasTarget);
		writer.Key("isNetworked"), writer.Bool(clickToMove.isNetworked);

		writer.EndObject(); // end ClickToMove object
	}

	static ClickToMove FromJSON(const rapidjson::Value& json) {
		ClickToMove component{};

		(void)json;

		return component;
	}
};

/*************************************************************************
  \struct ComponentSerializer<ScreenBounds>
  \brief
    JSON serialization for screen constraint boundaries (stub implementation).
  \warning
    Screen bounds are typically level configuration data, not entity state.
    Consider migrating to a LevelConfig asset. If per-entity bounds are required,
    implement serialization of top/bottom/left/right fields.
*************************************************************************/
template<>
struct ComponentSerializer<ScreenBounds> {
	static void ToJSON(ScreenBounds const& screenBounds, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start ScreenBounds object

		writer.Key("top"), writer.Double(screenBounds.top);
		writer.Key("bottom"), writer.Double(screenBounds.bottom);
		writer.Key("left"), writer.Double(screenBounds.left);
		writer.Key("right"), writer.Double(screenBounds.right);

		writer.EndObject(); // end ScreenBounds object
	}

	static ScreenBounds FromJSON(const rapidjson::Value& json) {
		ScreenBounds component{};

		component.top = json.HasMember("top") && json["top"].IsNumber() ? json["top"].GetFloat() : 0.f;
		component.bottom = json.HasMember("bottom") && json["bottom"].IsNumber() ? json["bottom"].GetFloat() : 0.f;
		component.left = json.HasMember("left") && json["left"].IsNumber() ? json["left"].GetFloat() : 0.f;
		component.right = json.HasMember("right") && json["right"].IsNumber() ? json["right"].GetFloat() : 0.f;

		return component;
	}
};
/*************************************************************************
  \struct ComponentSerializer<EntityName>
  \brief
    JSON serialization for human-readable entity identifiers.
  \details
    Serializes the fixed-size C-string buffer as a JSON string. Deserialization
    uses SetName for automatic null-termination and truncation handling.
  \note
    SetName has a bug in GGECSComponents.hpp (ternary operator evaluates to
    no-op). The deserialization still works because it calls the method, which
    internally uses strcpy for short strings despite the buggy return path.
  \see
    EntityName::GetName, EntityName::SetName
*************************************************************************/
template<>
struct ComponentSerializer<EntityName> {
	static void ToJSON(EntityName const& entityName, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start EntityName object
		
		writer.Key("name"), writer.String(entityName.name);

		writer.EndObject(); // end EntityName object
	}

	static EntityName FromJSON(const rapidjson::Value& json) {
		EntityName component{};

		if (json.HasMember("name") && json["name"].IsString()) {
			component.SetName(json["name"].GetString());
		}

		return component;
	}
};

/*************************************************************************
  \struct ComponentSerializer<PrefabInstance>
  \brief
    JSON serialization for prefab instance linkage.
  \details
    Serializes the prefab ID as "PrefabID" key for explicit naming in JSON.
    Critical for maintaining entity-to-prefab relationships across save/load.
  \warning
    FromJSON is stubbed. Implementation should deserialize "PrefabID" field
    with type validation (IsInt or IsUint) and assign to component.id.
*************************************************************************/
template<>
struct ComponentSerializer<PrefabInstance> {
	static void ToJSON(PrefabInstance const& instance, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start PrefabInstance object

		writer.Key("prefabInstanceName"), writer.String(instance.prefabInstanceName);

		writer.EndObject(); // end PrefabInstance object
	}

	static PrefabInstance FromJSON(const rapidjson::Value& json) {
		PrefabInstance component{};

		if (json.HasMember("prefabInstanceName") && json["prefabInstanceName"].IsString()) {
			std::strncpy(component.prefabInstanceName, json["prefabInstanceName"].GetString(), PrefabInstance::SERIALIZED_SIZE);
			component.prefabInstanceName[PrefabInstance::SERIALIZED_SIZE - 1] = '\0'; // ensure null-termination
		}
		else {
			component.prefabInstanceName[0] = '\0';
		}

		return component;
	}
};
/*************************************************************************
  \struct ComponentSerializer<Sprite>
  \brief
    JSON serialization for sprite texture references.
  \details
    Serializes only textureAssetID; UV bounds and transform are omitted as they
    are typically derived from SpriteAnimator or default to full texture.
    Deserialization reconstructs sprite from asset ID via asset manager lookup.
  \warning
    FromJSON is stubbed. Implementation should deserialize "textureAssetID" as
    uint32_t and validate it exists in GGECSStandardAssetManager<GLuint> before
    constructing Sprite component.
  \note
    Consider serializing textureUVBounds and textureUVTransform if they are
    set per-entity rather than per-animation-frame. Current design assumes
    animation system owns UV manipulation.
*************************************************************************/
template<>
struct ComponentSerializer<Sprite> {
	static void ToJSON(Sprite const& sprite, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start Sprite object

		// --- Texture ---
		std::cout << "[ToJSON] textureAssetID = " << sprite.textureAssetID << std::endl;
		{
			auto uuidOpt = GGECSStandardAssetManager<GLuint>::GetUUIDFromID(sprite.textureAssetID);
			writer.Key("textureAssetUUID");
			if (uuidOpt.has_value()) {
				writer.String(uuids::to_string(uuidOpt.value()).c_str());

				// Print the file path for debug
				const std::string& filePath = GGAssetID::GetGGAssetID()->GetFilePath(uuidOpt.value());
				std::cout << "[ToJSON] Texture file path: " << filePath << std::endl;
			}
			else {
				writer.String(""); // fallback
				std::cerr << "[ToJSON] Warning: No UUID found for textureAssetID "
					<< sprite.textureAssetID << std::endl;
			}
		}

		writer.Key("textureUVBounds");
		writer.StartObject();
		writer.Key("x"), writer.Double(sprite.textureUVBounds.x);
		writer.Key("y"), writer.Double(sprite.textureUVBounds.y);
		writer.Key("z"), writer.Double(sprite.textureUVBounds.z);
		writer.Key("w"), writer.Double(sprite.textureUVBounds.w);
		writer.EndObject();

		writer.Key("textureUVTransform");
		writer.StartArray();
		for (int col{}; col < 4; ++col) {
			writer.StartArray(); // start row
			for (int row{}; row < 4; ++row) {
				writer.Double(sprite.textureUVTransform[col][row]);
			}
			writer.EndArray(); // end row
		}
		writer.EndArray();
		
		writer.EndObject(); // end Sprite object
	}

	static Sprite FromJSON(const rapidjson::Value& json) {
		Sprite component{};

		// --- Texture ---
		if (json.HasMember("textureAssetUUID") && json["textureAssetUUID"].IsString())
		{
			std::optional<uuids::uuid> uuid = uuids::uuid::from_string(
				json["textureAssetUUID"].GetString()
			);

			if (uuid)
			{
				// TODO

				// Check if uuid exists in GGAssetID

				// Then, call RegisterAsset(uuid)

				// RegisterAsset needs to get the Asset from AssetManager

				// So GGAssetID needs a way to lookup raw file paths from its uuidRegistry map

				/*
				* 1. Implement GGAssetID::DoesUUIDExist(uuids::uuid const& uuid) -> bool
				* 2. Implement GGAssetID::GetFilePath(uuids::uuid const& uuid) -> std::string const&
				* 3. Implement RegisterAsset (See RegisterAsset)
				*/

				auto& assetSystem = GGAssetID::GetGGAssetID();

				// Check if UUID exists
				if (assetSystem->DoesUUIDExist(*uuid)) {
					// Get File Path of Asset
					//const std::string& filePath = assetSystem->GetFilePath(*uuid);

					// Register asset in AssetManager to get numeric ID
					uint32_t assetID = GGECSStandardAssetManager<GLuint>::RegisterAsset(*uuid);

					// Get the texture from AssetManager using the UUID file path
					component.textureAssetID = assetID;

					if (assetID)
					{
						/*std::cout << "[FromJSON] Registered and loaded texture: " << filePath
							<< " with ID " << assetID << std::endl;*/
					}
					else
					{
						std::cout << "[FromJSON] No Asset ID Registered, default to 0." << std::endl;
					}
				}
				else {
					std::cerr << "[Sprite::FromJSON] UUID does not exist: " << uuids::to_string(*uuid) << std::endl;
					//assetSystem->PrintAll();
				}
			}
			else {
				std::cerr << "[Sprite::FromJSON] Invalid UUID string for texture: " << json["textureAssetUUID"].GetString() << std::endl;
			}
		}

		return component;
	}
};
/*************************************************************************
  \struct ComponentSerializer<SpriteAnimator>
  \brief
    JSON serialization for sprite animation state.
  \details
    Serializes animationAssetID to restore animation configuration on load.
    Runtime state (currentFrameIndex, currentAnimState, frameTime) is intentionally
    omitted; animations restart from frame 0 on load. For cinematics requiring
    exact frame restoration, extend to serialize all four fields.
  \warning
    FromJSON is stubbed. Implementation should deserialize "animationAssetID"
    and validate against registered animation assets. Consider resetting
    currentFrameIndex/frameTime to zero and using a default animation state
    (e.g., "idle") on load.
*************************************************************************/
template<>
struct ComponentSerializer<SpriteAnimator> {
	static void ToJSON(SpriteAnimator const& animator, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start SpriteAnimator object

		// --- Texture ---
		std::cout << "[ToJSON] animationAssetID = " << animator.animationAssetID << std::endl;
		{
			auto uuidOpt = GGECSStandardAssetManager<GGSpriteAnimationSystem::GGAnimatedSprite>::GetUUIDFromID(animator.animationAssetID);
			writer.Key("animationAssetUUID");
			if (uuidOpt.has_value()) {
				writer.String(uuids::to_string(uuidOpt.value()).c_str());

				// Print the file path for debug
				const std::string& filePath = GGAssetID::GetGGAssetID()->GetFilePath(uuidOpt.value());
				std::cout << "[ToJSON] Animation file path: " << filePath << std::endl;
			}
			else {
				writer.String(""); // fallback
				std::cerr << "[ToJSON] Warning: No UUID found for animationAssetID "
					<< animator.animationAssetID << std::endl;
			}
		}

		writer.EndObject(); // end SpriteAnimator object
	}

	static SpriteAnimator FromJSON(const rapidjson::Value& json) {
		SpriteAnimator component{};

		if (json.HasMember("animationAssetID") && json["animationAssetID"].IsInt())
		{
			component.animationAssetID = json["animationAssetID"].GetInt();
		}

		// --- Texture ---
		if (json.HasMember("animationAssetUUID") && json["animationAssetUUID"].IsString())
		{
			std::optional<uuids::uuid> uuid = uuids::uuid::from_string(
				json["animationAssetUUID"].GetString()
			);

			if (uuid)
			{
				// TODO

				// Check if uuid exists in GGAssetID

				// Then, call RegisterAsset(uuid)

				// RegisterAsset needs to get the Asset from AssetManager

				// So GGAssetID needs a way to lookup raw file paths from its uuidRegistry map

				/*
				* 1. Implement GGAssetID::DoesUUIDExist(uuids::uuid const& uuid) -> bool
				* 2. Implement GGAssetID::GetFilePath(uuids::uuid const& uuid) -> std::string const&
				* 3. Implement RegisterAsset (See RegisterAsset)
				*/

				auto& assetSystem = GGAssetID::GetGGAssetID();

				// Check if UUID exists
				if (assetSystem->DoesUUIDExist(*uuid)) {
					// Get File Path of Asset
					const std::string& filePath = assetSystem->GetFilePath(*uuid);

					// Register asset in AssetManager to get numeric ID
					uint32_t assetID = GGECSStandardAssetManager<GGSpriteAnimationSystem::GGAnimatedSprite>::RegisterAsset(*uuid);

					// Get the texture from AssetManager using the UUID file path
					component.animationAssetID = assetID;

					if (assetID)
					{
						std::cout << "[FromJSON] Registered and loaded texture: " << filePath
							<< " with ID " << assetID << std::endl;
					}
					else
					{
						std::cout << "[FromJSON] No Asset ID Registered, default to 0." << std::endl;
					}
				}
				else {
					std::cerr << "[Sprite::FromJSON] UUID does not exist: " << uuids::to_string(*uuid) << std::endl;
					//assetSystem->PrintAll();
				}
			}
			else {
				std::cerr << "[Sprite::FromJSON] Invalid UUID string for animation: " << json["animationAssetUUID"].GetString() << std::endl;
			}
		}

		return component;
	}
};

/*************************************************************************
	\struct ComponentSerializer<LocalTransform>
	\brief
	JSON serialization for local transform components.
	\details
	Serializes position, scale, rotation, hierarchyDepth, and dirty flag.
	Used to save/load entity transforms within a scene or prefab.
	\note
	LocalTransform only represents the entity�s local space; WorldTransform is
	computed from hierarchy. Deserialization uses default values if fields are
	missing in JSON.
*************************************************************************/
template <>
struct ComponentSerializer<LocalTransform> {
	static void ToJSON(LocalTransform const& localTransform, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start LocalTransform object

		writer.Key("position");
		writer.StartObject();
		writer.Key("x"), writer.Double(localTransform.position.x);
		writer.Key("y"), writer.Double(localTransform.position.y);
		writer.EndObject();

		writer.Key("scale");
		writer.StartObject();
		writer.Key("x"), writer.Double(localTransform.scale.x);
		writer.Key("y"), writer.Double(localTransform.scale.y);
		writer.EndObject();

		writer.Key("rotation"), writer.Double(localTransform.rotation);
		writer.Key("hierarchyDepth"), writer.Uint(localTransform.hierarchyDepth);

		writer.EndObject(); // end LocalTransform object
	}

	static LocalTransform FromJSON(const rapidjson::Value& json) {
		LocalTransform component{};

		if (json.HasMember("position") && json["position"].IsObject()) {
			const auto& pos = json["position"];
			component.position.x = pos.HasMember("x") && pos["x"].IsNumber() ? static_cast<GGMath::float32>(pos["x"].GetDouble()) : 0.f;
			component.position.y = pos.HasMember("y") && pos["y"].IsNumber() ? static_cast<GGMath::float32>(pos["y"].GetDouble()) : 0.f;
		}

		if (json.HasMember("scale") && json["scale"].IsObject()) {
			const auto& scl = json["scale"];
			component.scale.x = scl.HasMember("x") && scl["x"].IsNumber() ? static_cast<GGMath::float32>(scl["x"].GetDouble()) : 1.f;
			component.scale.y = scl.HasMember("y") && scl["y"].IsNumber() ? static_cast<GGMath::float32>(scl["y"].GetDouble()) : 1.f;
		}

		component.rotation = json.HasMember("rotation") && json["rotation"].IsNumber() ? static_cast<GGMath::float32>(json["rotation"].GetDouble()) : 0.0f;
		component.hierarchyDepth = json.HasMember("hierarchyDepth") && json["hierarchyDepth"].IsUint() ? static_cast<uint8_t>(json["hierarchyDepth"].GetUint()) : 0;
		
		// Always concatenate LocalTransform on load
		component.isDirty = true;

		return component;
	}
};

/*************************************************************************
	\struct ComponentSerializer<WorldTransform>
	\brief
	JSON serialization for world transform components.
	\details
	Serializes the 3x3 world matrix and validity flag. Useful for saving the
	fully computed transform of entities, including hierarchy effects.
	\warning
	Runtime-only component: typically recomputed from LocalTransform and
	HierarchyComponent. Persisting WorldTransform is optional and mainly for
	debugging or instant scene restore.
*************************************************************************/
template <>
struct ComponentSerializer<WorldTransform> {
	static void ToJSON(WorldTransform const& worldTransform, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start WorldTransform object

		writer.Key("matrix");
		writer.StartArray();
		for (int col{}; col < 3; ++col) {
			writer.StartArray(); // start row
			for (int row{}; row < 3; ++row) {
				writer.Double(worldTransform.matrix(row, col));
			}
			writer.EndArray(); // end row
		}
		writer.EndArray();

		writer.Key("isValid"), writer.Bool(worldTransform.isValid);

		writer.EndObject(); // end WorldTransform object
	}

	static WorldTransform FromJSON(const rapidjson::Value& json) {
		WorldTransform component{};

		component.isValid = json.HasMember("isValid") && json["isValid"].IsBool() ? json["isValid"].GetBool() : false;

		if (json.HasMember("matrix") && json["matrix"].IsArray()) {
			const auto& cols = json["matrix"];
			for (rapidjson::SizeType col = 0; col < cols.Size() && col < 3; ++col) {
				if (cols[col].IsArray()) {
					const auto& rows = cols[col];
					for (rapidjson::SizeType row = 0; row < rows.Size() && row < 3; ++row) {
						component.matrix(row, col) = rows[row].IsNumber() ? static_cast<GGMath::float32>(rows[row].GetDouble()) : 0.0f;
					}
				}
			}
		}

		return component;
	}
};

/*************************************************************************
	\struct ComponentSerializer<HierarchyComponent>
	\brief
	JSON serialization for entity hierarchy relationships.
	\details
	Serializes parent, firstChild, and nextSibling entity IDs. Critical for
	reconstructing entity hierarchies on prefab or scene load.
	\note
	Deserialization falls back to NULL_ENTITY if fields are missing.
*************************************************************************/
template <>
struct ComponentSerializer<HierarchyComponent> {
	static void ToJSON(HierarchyComponent const& hierarchy, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start HierarchyComponent object

		writer.Key("parent"), writer.Uint(hierarchy.parent.id);
		writer.Key("firstChild"), writer.Uint(hierarchy.firstChild.id);
		writer.Key("nextSibling"), writer.Uint(hierarchy.nextSibling.id);

		writer.EndObject(); // end HierarchyComponent object
	}

	static HierarchyComponent FromJSON(const rapidjson::Value& json) {
		HierarchyComponent component{};

		// component.parent.id = json.HasMember("parent") && json["parent"].IsUint() ? json["parent"].GetUint() : NULL_ENTITY.id;
		if (json.HasMember("parent") && json["parent"].IsUint()) {
			component.parent.id = json["parent"].GetUint();
		}
		else {
			component.parent = NULL_ENTITY;
		}
		// component.firstChild.id = json.HasMember("firstChild") && json["firstChild"].IsUint() ? json["firstChild"].GetUint() : NULL_ENTITY.id;
		if (json.HasMember("firstChild") && json["firstChild"].IsUint()) {
			component.firstChild.id = json["firstChild"].GetUint();
		}
		else {
			component.firstChild = NULL_ENTITY;
		}
		// component.nextSibling.id = json.HasMember("nextSibling") && json["nextSibling"].IsUint() ? json["nextSibling"].GetUint() : NULL_ENTITY.id;
		if (json.HasMember("nextSibling") && json["nextSibling"].IsUint()) {
			component.nextSibling.id = json["nextSibling"].GetUint();
		}
		else {
			component.nextSibling = NULL_ENTITY;
		}

		return component;
	}
};

/*************************************************************************
	\struct ComponentSerializer<NameComponent>
	\brief
	JSON serialization for name components.
	\details
	Serializes a fixed-size name string for human-readable entity identifiers.
	\note
	Null-termination is enforced on deserialization. Useful for editor UI and
	debugging.
*************************************************************************/
template <>
struct ComponentSerializer<NameComponent> {
	static void ToJSON(NameComponent const& component, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start NameComponent object

		writer.Key("name");
		writer.String(component.name);

		writer.EndObject(); // end NameComponent object
	}

	static NameComponent FromJSON(const rapidjson::Value& json) {
		NameComponent component{};

		if (json.HasMember("name") && json["name"].IsString()) {
			std::strncpy(component.name, json["name"].GetString(), NameComponent::SERIALIZED_SIZE);
			component.name[NameComponent::SERIALIZED_SIZE - 1] = '\0'; // ensure null-termination
		}
		else {
			component.name[0] = '\0';
		}

		return component;
	}
};

/*************************************************************************
	\struct ComponentSerializer<ScriptComponent>
	\brief
	JSON serialization for script components.
	\details
	Serializes scriptAssetID as UUID to allow reloading scripts on prefab or
	scene load. Runtime execution state is not serialized.
	\warning
	FromJSON implementation requires UUID validation and registration via
	GGECSStandardAssetManager. Scripts must exist in asset registry for proper
	reconstruction.
*************************************************************************/
template<>
struct ComponentSerializer<ScriptComponent> {
	static void ToJSON(ScriptComponent const& script, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject();

		{
			auto uuidOpt = GGECSStandardAssetManager<GGScriptAsset>::GetUUIDFromID(script.scriptAssetID);
			writer.Key("scriptAssetUUID");
			if (uuidOpt.has_value()) {
				writer.String(uuids::to_string(uuidOpt.value()).c_str());

				// Print the file path for debug
				const std::string& filePath = GGAssetID::GetGGAssetID()->GetFilePath(uuidOpt.value());
				std::cout << "[ToJSON] Script file path: " << filePath << std::endl;

				if (script.scriptInstance > 0)
				{

					auto& scriptSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>();
					EntityInstance* instance = scriptSystem.GetScriptInstance(script.scriptInstance);
					std::cout << "[ToJSON DEBUG] scriptInstance=" << script.scriptInstance
						<< " instance=" << (void*)instance
						<< " type=" << (instance ? (void*)instance->type : nullptr)
						<< " data=" << (instance ? (void*)instance->data : nullptr)
						<< std::endl;
					if (instance != nullptr && instance->type != nullptr && instance->data != nullptr)
					{
						GGScriptMetadata* meta = instance->type;
						std::cout << "[ToJSON] Script instance found. Saving script instance members..." << std::endl;

						for (auto const& field : meta->fieldDescriptors)
						{
							// Skip non-serialized fields
							if (meta->serializedFields.count(field.name))
							{
								writer.Key(field.name.c_str());

								char* dataPtr = static_cast<char*>(instance->data);
								void* fieldPtr = dataPtr + field.offset;

								switch (field.type)
								{
								case FieldDescriptor::TYPE_INT:
								{
									writer.Int(*static_cast<int*>(fieldPtr));
									break;
								}
								case FieldDescriptor::TYPE_FLOAT:
								{
									writer.Double(*static_cast<float*>(fieldPtr));
									break;
								}
								case FieldDescriptor::TYPE_STRING:
								{
									std::string* strPtr = static_cast<std::string*>(fieldPtr);
									// Safety check: ensure string is valid before accessing
									if (strPtr != nullptr)
									{
										writer.String(strPtr->c_str());
									}
									else
									{
										writer.String("");  // Write empty string if pointer is invalid
									}
									break;
								}
								case FieldDescriptor::TYPE_BOOL:
								{
									writer.Bool(*static_cast<bool*>(fieldPtr));
									break;
								}
								}
							}
						}

						for (const auto& field : meta->fieldDescriptors)
						{
							// Skip non textureAsset fields
							if (meta->textureAssetFields.count(field.name))
							{
								writer.Key(field.name.c_str());

								char* dataPtr = static_cast<char*>(instance->data);
								void* fieldPtr = dataPtr + field.offset;

								switch (field.type)
								{
								case FieldDescriptor::TYPE_INT:
								{
									uint32_t id = static_cast<uint32_t>(*static_cast<int*>(fieldPtr));

									std::optional<uuids::uuid> uuid = GGECSStandardAssetManager<GLuint>::GetUUIDFromID(id);

									if (uuid.has_value())
									{
										writer.String(uuids::to_string(uuid.value()).c_str());
									}
									else
									{
										writer.String(""); // fallback: no UUID registered for this ID
									}

									break;
								}
								}
							}

							if (meta->audioAssetFields.count(field.name))
							{
								writer.Key(field.name.c_str());

								char* dataPtr = static_cast<char*>(instance->data);
								void* fieldPtr = dataPtr + field.offset;

								switch (field.type)
								{
								case FieldDescriptor::TYPE_INT:
								{
									uint32_t id = static_cast<uint32_t>(*static_cast<int*>(fieldPtr));

									std::optional<uuids::uuid> uuid = GGECSStandardAssetManager<GGAudioAsset>::GetUUIDFromID(id);

									if (uuid.has_value())
									{
										writer.String(uuids::to_string(uuid.value()).c_str());
									}
									else
									{
										writer.String(""); // fallback: no UUID registered for this ID
									}

									break;
								}
								}

								// serialize audio asset config settings
								auto cfgIt = meta->audioAssetConfigs.find(field.name);
								if (cfgIt != meta->audioAssetConfigs.end())
								{
									const AudioAssetConfig& cfg = cfgIt->second;
									std::string cfgKey = field.name + "_config";
									writer.Key(cfgKey.c_str());
									writer.StartObject();
									writer.Key("volume"), writer.Double(cfg.volume);
									writer.Key("pitch"), writer.Double(cfg.pitchVariance);
									writer.Key("loop"), writer.Bool(cfg.loop);
									writer.Key("oneShot"), writer.Bool(cfg.oneShot);
									writer.EndObject();
								}
							}
						}

						// Recursive serialization of composition fields as nested objects
						lua_State* compSerL = GetEngineInstance().GetLuaVM()->GetState();
						auto& compSerScriptSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>();
						std::function<void(GGScriptMetadata*, EntityInstance*)> serializeComposition;
						serializeComposition = [&](GGScriptMetadata* parentMeta, EntityInstance* parentInst)
						{
							for (const auto& compFieldName : parentMeta->compositionFields)
							{
								auto compFieldIt = parentMeta->fieldNameToIndex.find(compFieldName);
								if (compFieldIt == parentMeta->fieldNameToIndex.end()) continue;

								const FieldDescriptor& compField = parentMeta->fieldDescriptors[compFieldIt->second];
								char* dataPtr = static_cast<char*>(parentInst->data);
								std::string* composedTypeName = static_cast<std::string*>(
									static_cast<void*>(dataPtr + compField.offset));

								if (!composedTypeName || composedTypeName->empty()) continue;

								GGScriptMetadata* composedMeta = compSerScriptSystem.GetMetadata(*composedTypeName);
								if (!composedMeta) continue;

								writer.Key(compFieldName.c_str());
								writer.StartObject();
								writer.Key("__composedType");
								writer.String(composedTypeName->c_str());

								if (parentInst->luaFieldsRef != LUA_NOREF && parentInst->luaFieldsRef != LUA_REFNIL)
								{
									lua_rawgeti(compSerL, LUA_REGISTRYINDEX, parentInst->luaFieldsRef);
									if (lua_istable(compSerL, -1))
									{
										lua_getfield(compSerL, -1, compFieldName.c_str());
										if (lua_isuserdata(compSerL, -1))
										{
											InstanceUserdata* subUD = (InstanceUserdata*)lua_touserdata(compSerL, -1);
											if (subUD && subUD->instance && subUD->instance->data)
											{
												char* subDataPtr = static_cast<char*>(subUD->instance->data);

												// Serialize serialized fields
												for (const auto& innerField : composedMeta->fieldDescriptors)
												{
													if (!composedMeta->serializedFields.count(innerField.name)) continue;
													void* innerPtr = subDataPtr + innerField.offset;
													writer.Key(innerField.name.c_str());
													switch (innerField.type)
													{
													case FieldDescriptor::TYPE_INT:
														writer.Int(*static_cast<int*>(innerPtr)); break;
													case FieldDescriptor::TYPE_FLOAT:
														writer.Double(*static_cast<float*>(innerPtr)); break;
													case FieldDescriptor::TYPE_STRING:
														writer.String(static_cast<std::string*>(innerPtr)->c_str()); break;
													case FieldDescriptor::TYPE_BOOL:
														writer.Bool(*static_cast<bool*>(innerPtr)); break;
													}
												}

												// Serialize textureAsset fields as UUID strings
												for (const auto& innerField : composedMeta->fieldDescriptors)
												{
													if (!composedMeta->textureAssetFields.count(innerField.name)) continue;
													void* innerPtr = subDataPtr + innerField.offset;
													writer.Key(innerField.name.c_str());
													switch (innerField.type)
													{
													case FieldDescriptor::TYPE_INT:
													{
														uint32_t id = static_cast<uint32_t>(*static_cast<int*>(innerPtr));
														std::optional<uuids::uuid> uuid = GGECSStandardAssetManager<GLuint>::GetUUIDFromID(id);
														if (uuid.has_value())
															writer.String(uuids::to_string(uuid.value()).c_str());
														else
															writer.String("");
														break;
													}
													}
												}

												// Recurse into nested composition
												if (!composedMeta->compositionFields.empty())
													serializeComposition(composedMeta, subUD->instance);
											}
										}
										lua_pop(compSerL, 1); // Pop sub-instance userdata or nil
									}
									lua_pop(compSerL, 1); // Pop luaFields table
								}

								writer.EndObject();
							}
						};

						serializeComposition(meta, instance);
					}
				}
			}
			else {
				writer.String(""); // fallback
				std::cerr << "[ToJSON] Warning: No UUID found for scriptAssetID "
					<< script.scriptAssetID << std::endl;
			}
		}

		writer.EndObject();
	}

	static ScriptComponent FromJSON(const rapidjson::Value& json) {
		ScriptComponent component{};

		// --- Texture ---
		if (json.HasMember("scriptAssetUUID") && json["scriptAssetUUID"].IsString())
		{
			std::optional<uuids::uuid> uuid = uuids::uuid::from_string(
				json["scriptAssetUUID"].GetString()
			);

			if (uuid)
			{
				auto& assetSystem = GGAssetID::GetGGAssetID();

				// Check if UUID exists
				if (assetSystem->DoesUUIDExist(*uuid)) {
					// Get File Path of Asset
					//const std::string& filePath = assetSystem->GetFilePath(*uuid);

					// Register asset in AssetManager to get numeric ID
					uint32_t assetID = GGECSStandardAssetManager<GGScriptAsset>::RegisterAsset(*uuid);

					// Get the texture from AssetManager using the UUID file path
					component.scriptAssetID = assetID;

					if (assetID)
					{
						/*std::cout << "[FromJSON] Registered and loaded script: " << filePath
							<< " with ID " << assetID << std::endl;*/
					}
					else
					{
						std::cout << "[FromJSON] No Asset ID Registered, default to 0." << std::endl;
					}
				}
				else {
					std::cerr << "[ScriptComponent::FromJSON] UUID does not exist: " << uuids::to_string(*uuid) << std::endl;
					//assetSystem->PrintAll();
				}
			}
			else {
				std::cerr << "[ScriptComponent::FromJSON] Invalid UUID string for script: " << json["scriptAssetUUID"].GetString() << std::endl;
			}
		}

		return component;
	}
};

template<>
struct ComponentSerializer<ECSGridComponent> {
	static void ToJSON(ECSGridComponent const& grid, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject();

		writer.Key("gridSizeX"), writer.Uint(grid.gridSizeX);
		writer.Key("gridSizeY"), writer.Uint(grid.gridSizeY);

		writer.EndObject();
	}

	static ECSGridComponent FromJSON(const rapidjson::Value& json) {
		ECSGridComponent component{};

		if (json.HasMember("gridSizeX") && json["gridSizeX"].IsUint())
			component.gridSizeX = json["gridSizeX"].GetUint();

		if (json.HasMember("gridSizeY") && json["gridSizeY"].IsUint())
			component.gridSizeY = json["gridSizeY"].GetUint();

		component.gridID =
			Systems::GetECSSystemsRegistry().
			GetSystem<Systems::ECSGrid>().GetGridManager()->CreateGrid(true);

		return component;
	}
};

/*************************************************************************
  \struct ComponentSerializer<AudioSource>
  \brief
	JSON serialization for AudioSource components.
  \details
	Stores audio asset references using UUID strings rather than numeric
	asset IDs. This allows AudioSource components to be reloaded correctly
	across project sessions, prefab instantiation, and scene loads.

	The serializer writes all configurable AudioSource properties:
	  - UUID of the bound audio asset
	  - playback controls (playOnAwake, looping, mute)
	  - output bus routing
	  - volume, pitch, stereo panning
	  - playback state (cursor, play/paused flags)

	Deserialization performs full UUID resolution:
	  - Validates the UUID string
	  - Checks existence in GGAssetID registry
	  - Registers the asset with GGECSStandardAssetManager
	  - Retrieves the correct runtime audioAssetID
  \warning
	- Runtime-only fields such as FMOD::Channel* are never serialized.
	- If a UUID is invalid or missing, audioAssetID defaults to 0.
	- Requires GGAssetID and GGECSStandardAssetManager to be initialized
	  before calling FromJSON.
  \see
	AudioSource, GGECSStandardAssetManager, GGAssetID, FMOD backend
*************************************************************************/
template <>
struct ComponentSerializer<AudioSource> {
	static void ToJSON(AudioSource const& source, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start AudioSource object

		std::cout << "[ToJSON] audioAssetID = " << source.audioAssetID << std::endl;
		{
			auto uuidOpt = GGECSStandardAssetManager<GGAudioAsset>::GetUUIDFromID(source.audioAssetID);
			writer.Key("audioAssetUUID");
			if (uuidOpt.has_value()) {
				writer.String(uuids::to_string(uuidOpt.value()).c_str());

				// print the file path for debug
				const std::string& filePath = GGAssetID::GetGGAssetID()->GetFilePath(uuidOpt.value());
				std::cout << "[ToJSON] Audio file path: " << filePath << std::endl;
			}
			else {
				writer.String(""); // fallback
				std::cerr << "[ToJSON] Warning: No UUID found for audioAssetID "
					<< source.audioAssetID << std::endl;
			}
		}

		writer.Key("outputBus"), writer.Uint(source.outputBus);

		// controls
		writer.Key("playOnAwake"), writer.Bool(source.playOnAwake);
		writer.Key("isLooping"), writer.Bool(source.isLooping);
		writer.Key("isMuted"), writer.Bool(source.isMuted);

		writer.Key("volume"), writer.Double(source.volume);
		writer.Key("pitch"), writer.Double(source.pitch);
		writer.Key("stereoPan"), writer.Double(source.stereoPan);

		writer.Key("playbackCursor"), writer.Uint64(source.playbackCursor);
		writer.Key("justTriggered"), writer.Bool(source.justTriggered);
		writer.Key("isPlaying"), writer.Bool(source.isPlaying);
		writer.Key("isPaused"), writer.Bool(source.isPaused);

		writer.EndObject(); // end AudioSource object
	}

	static AudioSource FromJSON(const rapidjson::Value& json) {
		AudioSource component{};

		if (json.HasMember("audioAssetUUID") && json["audioAssetUUID"].IsString())
		{
			std::optional<uuids::uuid> uuid = uuids::uuid::from_string(
				json["audioAssetUUID"].GetString()
			);

			if (uuid)
			{
				auto& assetSystem = GGAssetID::GetGGAssetID();

				// check if UUID exists
				if (assetSystem->DoesUUIDExist(*uuid)) {
					// get file path of asset
					const std::string& filePath = assetSystem->GetFilePath(*uuid);

					// register asset in AssetManager to get numeric ID
					uint32_t assetID = GGECSStandardAssetManager<GGAudioAsset>::RegisterAsset(*uuid);

					// get the texture from AssetManager using UUID file path
					component.audioAssetID = assetID;

					if (assetID)
					{
						std::cout << "[FromJSON] Registered and loaded audio: " << filePath
							<< " with ID " << assetID << std::endl;
					}
					else
					{
						std::cout << "[FromJSON] No Asset ID Registered, default to 0." << std::endl;
					}
				}
				else {
					std::cerr << "[AudioSource::FromJSON] UUID does not exist: " << uuids::to_string(*uuid) << std::endl;
					//assetSystem->PrintAll();
				}
			}
			else {
				std::cerr << "[AudioSource::FromJSON] Invalid UUID string from audio: " << json["audioAssetUUID"].GetString() << std::endl;
			}
		}

		component.outputBus = json.HasMember("outputBus") && json["outputBus"].IsUint() ? static_cast<uint8_t>(json["outputBus"].GetUint()) : 0;

		component.playOnAwake = json.HasMember("playOnAwake") && json["playOnAwake"].IsBool() ? json["playOnAwake"].GetBool() : false;
		component.isLooping = json.HasMember("isLooping") && json["isLooping"].IsBool() ? json["isLooping"].GetBool() : false;
		component.isMuted = json.HasMember("isMuted") && json["isMuted"].IsBool() ? json["isMuted"].GetBool() : false;

		component.volume = json.HasMember("volume") && json["volume"].IsNumber() ? json["volume"].GetFloat() : 1.f;
		component.pitch = json.HasMember("pitch") && json["pitch"].IsNumber() ? json["pitch"].GetFloat() : 1.f;
		component.stereoPan = json.HasMember("stereoPan") && json["stereoPan"].IsNumber() ? json["stereoPan"].GetFloat() : 0.f;

		component.playbackCursor = json.HasMember("playbackCursor") && json["playbackCursor"].IsUint64() ? json["playbackCursor"].GetUint64() : 0;
		component.justTriggered = json.HasMember("justTriggered") && json["justTriggered"].IsBool() ? json["justTriggered"].GetBool() : false;
		component.isPlaying = json.HasMember("isPlaying") && json["isPlaying"].IsBool() ? json["isPlaying"].GetBool() : false;
		component.isPaused = json.HasMember("isPaused") && json["isPaused"].IsBool() ? json["isPaused"].GetBool() : false;

		return component;
	}
};

template<>
struct ComponentSerializer<TilemapComponent> {
	static void ToJSON(TilemapComponent const& tilemap, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start TilemapComponent object

		{
			auto uuidOpt = GGECSStandardAssetManager<GGTilemap>::GetUUIDFromID(tilemap.tilemapAssetID);
			writer.Key("tilemapAssetUUID");
			if (uuidOpt.has_value()) {
				writer.String(uuids::to_string(uuidOpt.value()).c_str());

				// Print the file path for debug
				const std::string& filePath = GGAssetID::GetGGAssetID()->GetFilePath(uuidOpt.value());
				std::cout << "[ToJSON] Tilemap file path: " << filePath << std::endl;
			}
			else {
				writer.String(""); // fallback
				std::cerr << "[ToJSON] Warning: No UUID found for tilemapAssetID "
					<< tilemap.tilemapAssetID << std::endl;
			}
		}

		writer.Key("perspectiveFactor");
		writer.Double(tilemap.perspectiveFactor);

		writer.EndObject(); // end TilemapComponent object
	}

	static TilemapComponent FromJSON(const rapidjson::Value& json) {
		TilemapComponent component{};

		if (json.HasMember("tilemapAssetUUID") && json["tilemapAssetUUID"].IsString())
		{
			std::optional<uuids::uuid> uuid = uuids::uuid::from_string(
				json["tilemapAssetUUID"].GetString()
			);

			if (uuid)
			{
				auto& assetSystem = GGAssetID::GetGGAssetID();

				// Check if UUID exists
				if (assetSystem->DoesUUIDExist(*uuid)) {
					// Get File Path of Asset
					const std::string& filePath = assetSystem->GetFilePath(*uuid);

					// Register asset in AssetManager to get numeric ID
					uint32_t assetID = GGECSStandardAssetManager<GGTilemap>::RegisterAsset(*uuid);

					// Get the texture from AssetManager using the UUID file path
					component.tilemapAssetID = assetID;

					if (assetID)
					{
						std::cout << "[FromJSON] Registered and loaded script: " << filePath
							<< " with ID " << assetID << std::endl;
					}
					else
					{
						std::cout << "[FromJSON] No Asset ID Registered, default to 0." << std::endl;
					}
				}
				else {
					std::cerr << "[TilemapComponent::FromJSON] UUID does not exist: " << uuids::to_string(*uuid) << std::endl;
					//assetSystem->PrintAll();
				}
			}
			else {
				std::cerr << "[TilemapComponent::FromJSON] Invalid UUID string for script: " << json["tilemapAssetUUID"].GetString() << std::endl;
			}
		}

		if (json.HasMember("perspectiveFactor") && json["perspectiveFactor"].IsDouble())
		{
			component.perspectiveFactor = static_cast<float>(json["perspectiveFactor"].GetDouble());
		}

		return component;
	}
};

/*************************************************************************
  \class EntityJSONInterface
  \brief
    High-level API for world serialization with entity type resolution.
  \details
    Provides batch serialization of entire ECS worlds to JSON format suitable
    for save files, level data export, or network state snapshots. Uses
    reflection via ComponentArray virtual interface to iterate over all component
    types without explicit registration. Entity type names are resolved via
    EntityTypes registry for human-readable JSON output.
  \see
    World, Entity, EntityTypes, ComponentArray::SerializeToJSON
  \par JSON Schema
  \code
    {
      "entities": [
        {
          "name": "Player",
          "prefabID": 100,
          "components": {
            "Transform2DFloat": {...},
            "Rigidbody2DFloat": {...},
            ...
          }
        },
        ...
      ]
    }
  \endcode
*************************************************************************/
class EntityJSONInterface {
public:

	/*************************************************************************
	  \brief
		Serializes entire world to JSON string with human-readable formatting.
	  \param[in,out] world
		ECS world containing entities to serialize.
	  \param[in] et
		Entity type registry for resolving prefab IDs to human-readable names.
	  \return
		Formatted JSON string representation of all living entities and their components.
	  \details
		Iterates through all living entities in the world, resolves their prefab type
		names via EntityTypes lookup, and serializes all attached components using
		ComponentArray's polymorphic SerializeToJSON interface. The output is
		deterministically ordered (entities in creation order, components in
		type ID order) for diffing and version control.

		Entities without PrefabInstance components are labeled "Unknown" in the name
		field and omit the prefabID key. This allows serialization of dynamically
		spawned entities not created from prefabs.
	  \note
		Uses PrettyWriter for readability; substitute with Writer for compact JSON
		if file size is critical. For large worlds (>10k entities), consider
		streaming to file rather than building entire JSON string in memory.
	  \warning
		The loop searches EntityTypes linearly for each entity's type name. For
		worlds with thousands of entities, consider building a reverse map
		(type->name) beforehand or caching the lookup results. Current O(N*M)
		complexity where N=entities, M=registered types.
	  \par Usage
	  \code
		World world;
		EntityTypes entityTypes;
		// ... populate world and entityTypes ...
		std::string json = EntityJSONInterface::SerializeWorld(world, entityTypes);
		std::ofstream("savegame.json") << json;
	  \endcode
	*************************************************************************/
	static std::string SerializeWorld(World& world) {
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

		writer.StartObject();
		writer.Key("entities");
		writer.StartArray();

		for (Entity const& entity : world.GetLivingEntities()) {

			writer.StartObject();

			std::string entityName = "Unknown";

			if (world.HasComponent<PrefabInstance>(entity)) {
				entityName = world.GetComponent<PrefabInstance>(entity)->GetPrefabInstanceName();
			}

			writer.Key("id");
			writer.Uint(entity.id);
			
			writer.Key("generation");
			writer.Uint(entity.generation);

			writer.Key("name");
			writer.String(entityName.c_str());

			writer.Key("components");
			writer.StartArray();

			for (auto const& [typeID, componentArray] : world.GetComponentArrays()) {
				
				// only serialize if the entity actually has this component!!!
				if (!componentArray->HasEntity(entity)) {
					continue;
				}

				writer.StartObject();
				writer.Key("componentID"); writer.Int(typeID);
				componentArray->SerializeToJSON(entity, writer);
				writer.EndObject();
			}

			writer.EndArray();
			writer.EndObject();
		}

		writer.EndArray();
		writer.EndObject();

		return buffer.GetString();
	}
};

template<>
struct ComponentSerializer<TextComponent> {
	static void ToJSON(const TextComponent& component, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start TextComponent object

		// Text content
		writer.Key("text");
		writer.String(component.text);

		

		//// Font ID as UUID (following your pattern for assets)
		//if (component.fontId != 0) {
		//	auto uuidOpt = GGECSStandardAssetManager<GGFont>::GetUUIDFromID(component.fontId);
		//	writer.Key("fontAssetUUID");
		//	if (uuidOpt.has_value()) {
		//		writer.String(uuids::to_string(uuidOpt.value()).c_str());

		//		const std::string& filePath = GGAssetID::GetGGAssetID()->GetFilePath(uuidOpt.value());
		//		std::cout << "[ToJSON] Font file path: " << filePath << std::endl;
		//	}
		//	else {
		//		writer.String(""); // fallback
		//		std::cerr << "[ToJSON] Warning: No UUID found for fontId "
		//			<< component.fontId << std::endl;
		//	}
		//}
		// Placeholder for future UUID system
		// Font asset UUID
		{
			// Use std::string manager for fonts (not GLuint which is for textures)
			auto uuidOpt = GGECSStandardAssetManager<std::string>::GetUUIDFromID(component.fontAssetID);
			writer.Key("fontAssetUUID");
			if (uuidOpt.has_value()) {
				writer.String(uuids::to_string(uuidOpt.value()).c_str());

				const std::string& filePath = GGAssetID::GetGGAssetID()->GetFilePath(uuidOpt.value());
				std::cout << "[ToJSON] Font file path: " << filePath << std::endl;
			}
			else {
				writer.String("");
				std::cerr << "[ToJSON] Warning: No UUID found for fontAssetID "
					<< component.fontAssetID << std::endl;
			}
		}


		//writer.Key("fontAssetUUID");
		//writer.String("PENDING_ASSET_MANAGER_SUPPORT");

		// Shader asset UUID
		{
			auto uuidOpt = GGECSStandardAssetManager<GGShader>::GetUUIDFromID(component.shaderAssetID);
			writer.Key("shaderAssetUUID");
			if (uuidOpt.has_value()) {
				writer.String(uuids::to_string(uuidOpt.value()).c_str());

				const std::string& filePath = GGAssetID::GetGGAssetID()->GetFilePath(uuidOpt.value());
				std::cout << "[ToJSON] Shader file path: " << filePath << std::endl;
			}
			else {
				writer.String(""); // fallback
				std::cerr << "[ToJSON] Warning: No UUID found for shaderAssetID "
					<< component.shaderAssetID << std::endl;
			}
		}

		// Font properties
		writer.Key("fontSize");
		writer.Uint(component.fontSize);

		

		// Color (vec3)
		writer.Key("color");
		writer.StartObject();
		writer.Key("r"), writer.Double(static_cast<double>(component.color.r));
		writer.Key("g"), writer.Double(static_cast<double>(component.color.g));
		writer.Key("b"), writer.Double(static_cast<double>(component.color.b));
		writer.Key("a"), writer.Double(static_cast<double>(component.color.a));
		writer.EndObject();

		// Rendering properties
		writer.Key("layer");
		writer.Int(component.layer);

		writer.Key("ySort");
		writer.Bool(component.ySort);

		writer.Key("visible");
		writer.Bool(component.visible);

		// Alignment
		writer.Key("alignment");
		writer.Uint(static_cast<uint32_t>(component.alignment));

		writer.Key("verticalAlignment");
		writer.Uint(static_cast<uint32_t>(component.verticalAlignment));

		// Text wrapping
		writer.Key("maxWidth");
		writer.Double(static_cast<double>(component.maxWidth));

		writer.Key("lineSpacing");
		writer.Double(static_cast<double>(component.lineSpacing));

		writer.EndObject(); // end TextComponent object
	}

	static TextComponent FromJSON(const rapidjson::Value& json) {
		TextComponent component{};

		// Text content
		if (json.HasMember("text") && json["text"].IsString()) {
			std::strncpy(component.text, json["text"].GetString(), 63);
			component.text[63] = '\0'; // ensure null-termination
		}


		// Font asset UUID
		if (json.HasMember("fontAssetUUID") && json["fontAssetUUID"].IsString()) {
			std::optional<uuids::uuid> uuid = uuids::uuid::from_string(
				json["fontAssetUUID"].GetString()
			);

			if (uuid) {
				auto& assetSystem = GGAssetID::GetGGAssetID();

				if (assetSystem->DoesUUIDExist(*uuid)) {
					const std::string& filePath = assetSystem->GetFilePath(*uuid);
					// Use std::string manager for fonts (not GLuint which is for textures)
					uint32_t assetID = GGECSStandardAssetManager<std::string>::RegisterAsset(*uuid);
					component.fontAssetID = assetID;

					if (assetID)
						std::cout << "[FromJSON] Registered and loaded font: " << filePath
						<< " with ID " << assetID << std::endl;
					else
						std::cout << "[FromJSON] No Asset ID Registered, default to 0." << std::endl;
				}
				else {
					std::cerr << "[TextComponent::FromJSON] Font UUID does not exist: "
						<< uuids::to_string(uuid.value()) << std::endl;
					//assetSystem->PrintAll();
				}
			}
			else {
				std::cerr << "[TextComponent::FromJSON] Invalid font UUID string: "
					<< json["fontAssetUUID"].GetString() << std::endl;
			}
		}

		// Shader asset UUID
		if (json.HasMember("shaderAssetUUID") && json["shaderAssetUUID"].IsString()) {
			std::optional<uuids::uuid> uuid = uuids::uuid::from_string(
				json["shaderAssetUUID"].GetString()
			);

			if (uuid) {
				auto& assetSystem = GGAssetID::GetGGAssetID();

				if (assetSystem->DoesUUIDExist(uuid.value())) {
					const std::string& filePath = assetSystem->GetFilePath(uuid.value());
					uint32_t assetID = GGECSStandardAssetManager<GGShader>::RegisterAsset(uuid.value());
					component.shaderAssetID = assetID;

					if (assetID)
						std::cout << "[FromJSON] Registered and loaded shader: " << filePath
						<< " with ID " << assetID << std::endl;
					else
						std::cout << "[FromJSON] No Asset ID Registered, default to 0." << std::endl;
				}
				else {
					std::cerr << "[TextComponent::FromJSON] Shader UUID does not exist: "
						<< uuids::to_string(uuid.value()) << std::endl;
					//assetSystem->PrintAll();
				}
			}
			else {
				std::cerr << "[TextComponent::FromJSON] Invalid shader UUID string: "
					<< json["shaderAssetUUID"].GetString() << std::endl;
			}
		}

		// Font properties
		if (json.HasMember("fontSize") && json["fontSize"].IsUint()) {
			component.fontSize = json["fontSize"].GetUint();
		}

		

		// Color
		if (json.HasMember("color") && json["color"].IsObject()) {
			const auto& colorObj = json["color"];
			if (colorObj.HasMember("r") && colorObj["r"].IsNumber())
				component.color.r = static_cast<float>(colorObj["r"].GetDouble());
			if (colorObj.HasMember("g") && colorObj["g"].IsNumber())
				component.color.g = static_cast<float>(colorObj["g"].GetDouble());
			if (colorObj.HasMember("b") && colorObj["b"].IsNumber())
				component.color.b = static_cast<float>(colorObj["b"].GetDouble());
			if (colorObj.HasMember("a") && colorObj["a"].IsNumber())
				component.color.a = static_cast<float>(colorObj["a"].GetDouble());
		}

		// Rendering properties
		if (json.HasMember("layer") && json["layer"].IsInt()) {
			component.layer = json["layer"].GetInt();
		}

		if (json.HasMember("ySort") && json["ySort"].IsBool()) {
			component.ySort = json["ySort"].GetBool();
		}

		if (json.HasMember("visible") && json["visible"].IsBool()) {
			component.visible = json["visible"].GetBool();
		}

		// Alignment
		if (json.HasMember("alignment") && json["alignment"].IsUint()) {
			component.alignment = static_cast<uint32_t >(json["alignment"].GetUint());
		}

		if (json.HasMember("verticalAlignment") && json["verticalAlignment"].IsUint()) {
			component.verticalAlignment = static_cast<uint32_t >(
				json["verticalAlignment"].GetUint()
				);
		}

		// Text wrapping
		if (json.HasMember("maxWidth") && json["maxWidth"].IsNumber()) {
			component.maxWidth = static_cast<float>(json["maxWidth"].GetDouble());
		}

		if (json.HasMember("lineSpacing") && json["lineSpacing"].IsNumber()) {
			component.lineSpacing = static_cast<float>(json["lineSpacing"].GetDouble());
		}

		return component;
	}
};

template<>
struct ComponentSerializer<WorldTextComponent> {
	static void ToJSON(const WorldTextComponent& component, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) {
		writer.StartObject(); // start TextComponent object

		// Text content
		writer.Key("text");
		writer.String(component.text);



		//// Font ID as UUID (following your pattern for assets)
		//if (component.fontId != 0) {
		//	auto uuidOpt = GGECSStandardAssetManager<GGFont>::GetUUIDFromID(component.fontId);
		//	writer.Key("fontAssetUUID");
		//	if (uuidOpt.has_value()) {
		//		writer.String(uuids::to_string(uuidOpt.value()).c_str());

		//		const std::string& filePath = GGAssetID::GetGGAssetID()->GetFilePath(uuidOpt.value());
		//		std::cout << "[ToJSON] Font file path: " << filePath << std::endl;
		//	}
		//	else {
		//		writer.String(""); // fallback
		//		std::cerr << "[ToJSON] Warning: No UUID found for fontId "
		//			<< component.fontId << std::endl;
		//	}
		//}
		// Placeholder for future UUID system
		// Font asset UUID
		{
			// Use std::string manager for fonts (not GLuint which is for textures)
			auto uuidOpt = GGECSStandardAssetManager<std::string>::GetUUIDFromID(component.fontAssetID);
			writer.Key("fontAssetUUID");
			if (uuidOpt.has_value()) {
				writer.String(uuids::to_string(uuidOpt.value()).c_str());

				const std::string& filePath = GGAssetID::GetGGAssetID()->GetFilePath(uuidOpt.value());
				std::cout << "[ToJSON] Font file path: " << filePath << std::endl;
			}
			else {
				writer.String("");
				std::cerr << "[ToJSON] Warning: No UUID found for fontAssetID "
					<< component.fontAssetID << std::endl;
			}
		}


		//writer.Key("fontAssetUUID");
		//writer.String("PENDING_ASSET_MANAGER_SUPPORT");

		// Shader asset UUID
		{
			auto uuidOpt = GGECSStandardAssetManager<GGShader>::GetUUIDFromID(component.shaderAssetID);
			writer.Key("shaderAssetUUID");
			if (uuidOpt.has_value()) {
				writer.String(uuids::to_string(uuidOpt.value()).c_str());

				const std::string& filePath = GGAssetID::GetGGAssetID()->GetFilePath(uuidOpt.value());
				std::cout << "[ToJSON] Shader file path: " << filePath << std::endl;
			}
			else {
				writer.String(""); // fallback
				std::cerr << "[ToJSON] Warning: No UUID found for shaderAssetID "
					<< component.shaderAssetID << std::endl;
			}
		}

		// Font properties
		writer.Key("fontSize");
		writer.Uint(component.fontSize);



		// Color (vec3)
		writer.Key("color");
		writer.StartObject();
		writer.Key("r"), writer.Double(static_cast<double>(component.color.r));
		writer.Key("g"), writer.Double(static_cast<double>(component.color.g));
		writer.Key("b"), writer.Double(static_cast<double>(component.color.b));
		writer.Key("a"), writer.Double(static_cast<double>(component.color.a));
		writer.EndObject();

		// Rendering properties
		writer.Key("layer");
		writer.Int(component.layer);

		writer.Key("ySort");
		writer.Bool(component.ySort);

		writer.Key("visible");
		writer.Bool(component.visible);

		// Alignment
		writer.Key("alignment");
		writer.Uint(static_cast<uint32_t>(component.alignment));

		writer.Key("verticalAlignment");
		writer.Uint(static_cast<uint32_t>(component.verticalAlignment));

		// Text wrapping
		writer.Key("maxWidth");
		writer.Double(static_cast<double>(component.maxWidth));

		writer.Key("lineSpacing");
		writer.Double(static_cast<double>(component.lineSpacing));

		writer.EndObject(); // end TextComponent object
	}

	static WorldTextComponent FromJSON(const rapidjson::Value& json) {
		WorldTextComponent component{};

		// Text content
		if (json.HasMember("text") && json["text"].IsString()) {
			std::strncpy(component.text, json["text"].GetString(), 63);
			component.text[63] = '\0'; // ensure null-termination
		}


		// Font asset UUID
		if (json.HasMember("fontAssetUUID") && json["fontAssetUUID"].IsString()) {
			std::optional<uuids::uuid> uuid = uuids::uuid::from_string(
				json["fontAssetUUID"].GetString()
			);

			if (uuid) {
				auto& assetSystem = GGAssetID::GetGGAssetID();

				if (assetSystem->DoesUUIDExist(*uuid)) {
					const std::string& filePath = assetSystem->GetFilePath(*uuid);
					// Use std::string manager for fonts (not GLuint which is for textures)
					uint32_t assetID = GGECSStandardAssetManager<std::string>::RegisterAsset(*uuid);
					component.fontAssetID = assetID;

					if (assetID)
						std::cout << "[FromJSON] Registered and loaded font: " << filePath
						<< " with ID " << assetID << std::endl;
					else
						std::cout << "[FromJSON] No Asset ID Registered, default to 0." << std::endl;
				}
				else {
					std::cerr << "[TextComponent::FromJSON] Font UUID does not exist: "
						<< uuids::to_string(uuid.value()) << std::endl;
					//assetSystem->PrintAll();
				}
			}
			else {
				std::cerr << "[TextComponent::FromJSON] Invalid font UUID string: "
					<< json["fontAssetUUID"].GetString() << std::endl;
			}
		}

		// Shader asset UUID
		if (json.HasMember("shaderAssetUUID") && json["shaderAssetUUID"].IsString()) {
			std::optional<uuids::uuid> uuid = uuids::uuid::from_string(
				json["shaderAssetUUID"].GetString()
			);

			if (uuid) {
				auto& assetSystem = GGAssetID::GetGGAssetID();

				if (assetSystem->DoesUUIDExist(uuid.value())) {
					const std::string& filePath = assetSystem->GetFilePath(uuid.value());
					uint32_t assetID = GGECSStandardAssetManager<GGShader>::RegisterAsset(uuid.value());
					component.shaderAssetID = assetID;

					if (assetID)
						std::cout << "[FromJSON] Registered and loaded shader: " << filePath
						<< " with ID " << assetID << std::endl;
					else
						std::cout << "[FromJSON] No Asset ID Registered, default to 0." << std::endl;
				}
				else {
					std::cerr << "[TextComponent::FromJSON] Shader UUID does not exist: "
						<< uuids::to_string(uuid.value()) << std::endl;
					//assetSystem->PrintAll();
				}
			}
			else {
				std::cerr << "[TextComponent::FromJSON] Invalid shader UUID string: "
					<< json["shaderAssetUUID"].GetString() << std::endl;
			}
		}

		// Font properties
		if (json.HasMember("fontSize") && json["fontSize"].IsUint()) {
			component.fontSize = json["fontSize"].GetUint();
		}



		// Color
		if (json.HasMember("color") && json["color"].IsObject()) {
			const auto& colorObj = json["color"];
			if (colorObj.HasMember("r") && colorObj["r"].IsNumber())
				component.color.r = static_cast<float>(colorObj["r"].GetDouble());
			if (colorObj.HasMember("g") && colorObj["g"].IsNumber())
				component.color.g = static_cast<float>(colorObj["g"].GetDouble());
			if (colorObj.HasMember("b") && colorObj["b"].IsNumber())
				component.color.b = static_cast<float>(colorObj["b"].GetDouble());
			if (colorObj.HasMember("a") && colorObj["a"].IsNumber())
				component.color.a = static_cast<float>(colorObj["a"].GetDouble());
		}

		// Rendering properties
		if (json.HasMember("layer") && json["layer"].IsInt()) {
			component.layer = json["layer"].GetInt();
		}

		if (json.HasMember("ySort") && json["ySort"].IsBool()) {
			component.ySort = json["ySort"].GetBool();
		}

		if (json.HasMember("visible") && json["visible"].IsBool()) {
			component.visible = json["visible"].GetBool();
		}

		// Alignment
		if (json.HasMember("alignment") && json["alignment"].IsUint()) {
			component.alignment = static_cast<uint32_t>(json["alignment"].GetUint());
		}

		if (json.HasMember("verticalAlignment") && json["verticalAlignment"].IsUint()) {
			component.verticalAlignment = static_cast<uint32_t>(
				json["verticalAlignment"].GetUint()
				);
		}

		// Text wrapping
		if (json.HasMember("maxWidth") && json["maxWidth"].IsNumber()) {
			component.maxWidth = static_cast<float>(json["maxWidth"].GetDouble());
		}

		if (json.HasMember("lineSpacing") && json["lineSpacing"].IsNumber()) {
			component.lineSpacing = static_cast<float>(json["lineSpacing"].GetDouble());
		}

		return component;
	}
};

template<>
struct ComponentSerializer<ObjectEffectComponent> {
	static void ToJSON(ObjectEffectComponent const& effect,
		rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer)
	{
		writer.StartObject();

		writer.Key("effectType"), writer.Uint(effect.effectType);
		writer.Key("intensity"), writer.Double(effect.intensity);
		
		writer.Key("effectDuration"), writer.Double(effect.effectDuration);
		writer.Key("loop"), writer.Bool(effect.loop);

		writer.Key("color");
		writer.StartObject();
		writer.Key("r"), writer.Double(effect.color.r);
		writer.Key("g"), writer.Double(effect.color.g);
		writer.Key("b"), writer.Double(effect.color.b);
		writer.Key("a"), writer.Double(effect.color.a);
		writer.EndObject();

		writer.Key("startTime"), writer.Double(effect.startTime);

		writer.EndObject();
	}

	static ObjectEffectComponent FromJSON(const rapidjson::Value& json)
	{
		ObjectEffectComponent component{};

		if (json.HasMember("effectType") && json["effectType"].IsUint())
			component.effectType = json["effectType"].GetUint();

		if (json.HasMember("intensity") && json["intensity"].IsNumber())
			component.intensity = static_cast<float>(json["intensity"].GetDouble());

		

		if (json.HasMember("effectDuration") && json["effectDuration"].IsNumber())
			component.effectDuration = static_cast<float>(json["effectDuration"].GetDouble());

		if (json.HasMember("loop") && json["loop"].IsBool())
			component.loop = json["loop"].GetBool();

		if (json.HasMember("color") && json["color"].IsObject()) {
			const auto& c = json["color"];
			if (c.HasMember("r") && c["r"].IsNumber())
				component.color.r = static_cast<float>(c["r"].GetDouble());
			if (c.HasMember("g") && c["g"].IsNumber())
				component.color.g = static_cast<float>(c["g"].GetDouble());
			if (c.HasMember("b") && c["b"].IsNumber())
				component.color.b = static_cast<float>(c["b"].GetDouble());
			if (c.HasMember("a") && c["a"].IsNumber())
				component.color.a = static_cast<float>(c["a"].GetDouble());
		}

		if (json.HasMember("startTime") && json["startTime"].IsNumber())
			component.startTime = static_cast<float>(json["startTime"].GetDouble());

		return component;
	}
};
