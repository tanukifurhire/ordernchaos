/*************************************************************************
  \file       GGECSSpriteAnimator.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu(70%), celestejiaxuan.tong@digipen.edu(30%)
  \date       3 October 2025
  \version    1.0
  \brief
	Defines sprite animation system with multi-page texture atlas support.
  \details
	This file implements the GGSpriteAnimationSystem for managing frame-based
	sprite animations across multiple texture pages. The system supports
	named animation states (e.g., "idle", "run", "attack"), configurable
	frame rates, and looping/non-looping playback modes. Animations are
	loaded from JSON manifests describing frame layouts and state machines.
	The system updates SpriteAnimator components each frame, advancing
	frame indices and updating Sprite UV bounds for texture atlas mapping.

	Design Philosophy:
	- Data-driven animation configuration via JSON
	- Support for texture page spanning (animations across multiple atlases)
	- Zero-allocation per-frame updates via pre-computed UV transformations
	- ECS-friendly with function-based iteration over component arrays
  \dependencies
	GGSystemInterface.hpp, GGECSComponents.hpp, <vector>, <string>,
	<functional>, glm (via Sprite component)
  \see
	SystemInterface, SpriteAnimator, Sprite, World
  \history
	- 1.0 (3 October 2025): Initial implementation with state machine support.
  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGSystemManagers.hpp>
#include <GGECSAssetInterface.hpp>
#include <GGECSWorld.hpp>
#include <GGLayerManager.hpp>

/*************************************************************************
  \brief
	Constructs a GGAnimatedSprite from a given file path.
  \param[in] filePath
	Path to the .gganim JSON file describing the sprite animation.
  \details
	Loads animation metadata including texture references, frame counts,
	UV bounds, and animation states. Supports legacy texture ID and UUID-based
	asset management. Throws std::runtime_error if the JSON is invalid.
*************************************************************************/
GGSpriteAnimationSystem::GGAnimatedSprite::GGAnimatedSprite(std::string const& filePath)
{
	//std::cout << "LOADING ANIMATED SPRITE FROM " << filePath << std::endl;

	sourcePath = filePath;

	// Make a mutable copy of the file path (filePath was const)
	std::string fixedPath = filePath;

	std::string assetBase = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath();
	if (assetBase.empty())
	{
		assetBase = "../../../Assets/";
	}
	if (fixedPath.find(assetBase) == std::string::npos)
	{
			fixedPath = assetBase + fixedPath;
	}

	rapidjson::Document animDoc = JSONUtils::ParseJSON(JSONUtils::ReadFileContents(fixedPath));

	if (!animDoc.IsObject() || !animDoc.HasMember("anims") || !animDoc["anims"].IsArray())
	{
		std::cout << filePath << " must contain an anims array at root level!" << std::endl;
		throw std::runtime_error("Invalid Spritesheet Format! " + filePath + " must contain an anims array at root level!");
	}

	/////////////////////////
	// Reset Internal Data
	/////////////////////////
	texturePages.clear();
	pageFrameCapacity.clear();
	animStates.clear();
	pages.clear();

	auto animArray = animDoc["anims"].GetArray();


	// Load Each Spritesheet Page
	for (rapidjson::SizeType animIndex = 0; animIndex < animArray.Size(); ++animIndex)
	{
		const auto& anim = animArray[animIndex];


		//////////////////////
		// Load Texture Asset
		/////////////////////
		uint32_t texAssetID = 0;

		// Try UUID first (preferred), fallback to legacy ID
		if (anim.HasMember("textureAssetUUID") && anim["textureAssetUUID"].IsString()) {

			// Convert the UUID string inside JSON into an actual uuid object
			std::optional<uuids::uuid> uuid = uuids::uuid::from_string(anim["textureAssetUUID"].GetString());

			// If UUID is valid and exists in asset registry
			if (uuid && GGAssetID::GetGGAssetID()->DoesUUIDExist(uuid.value())) {

				// Register the texture via UUID and returns textureAssetID
				texAssetID = GGECSStandardAssetManager<GLuint>::RegisterAsset(uuid.value());
			}
		}
		// UUID not found, fallback to legacy integer ID
		else if (anim.HasMember("textureAssetID") && anim["textureAssetID"].IsInt()) {
			texAssetID = static_cast<uint32_t>(anim["textureAssetID"].GetInt());
		}

		// Store this texture reference in the animation texturePages array
		texturePages.push_back(texAssetID);


		/////////////////////////////////
		// Required animation parameters
		/////////////////////////////////
		if (!anim.HasMember("animFPS") || !anim["animFPS"].IsDouble())
		{
			std::cout << filePath << " must contain animFPS double member!" << std::endl;
			throw std::runtime_error("Invalid Spritesheet Format! " + filePath + " must contain animFPS double member!");
		}

		// Converts FPS to seconds per frame
		animFPS = anim["animFPS"].GetDouble();
		frameTime = animFPS > 0.0 ? 1.0 / animFPS : 0.0;

		//////////////////////////////////////
		// Validate and store textureUVBounds
		//////////////////////////////////////
		if (!anim.HasMember("textureUVBounds") || !anim["textureUVBounds"].IsObject())
		{
			std::cout << filePath << " must contain textureUVBounds object member!" << std::endl;
			throw std::runtime_error("Invalid Spritesheet Format! " + filePath + " must contain textureUVBounds object member!");
		}

		const auto& uvBoundsObj = anim["textureUVBounds"];
		if (!uvBoundsObj.HasMember("sMin") || !uvBoundsObj["sMin"].IsDouble() ||
			!uvBoundsObj.HasMember("tMin") || !uvBoundsObj["tMin"].IsDouble() ||
			!uvBoundsObj.HasMember("sMax") || !uvBoundsObj["sMax"].IsDouble() ||
			!uvBoundsObj.HasMember("tMax") || !uvBoundsObj["tMax"].IsDouble())
		{
			std::cout << filePath << " textureUVBounds must contain sMin, tMin, sMax, tMax as doubles!" << std::endl;
			throw std::runtime_error("Invalid Spritesheet Format! " + filePath + " textureUVBounds must contain sMin, tMin, sMax, tMax as doubles!");
		}

		// Local per-page
		PageData page{};
		page.textureAssetID = texAssetID;

		if (!anim.HasMember("spritesPerRow") || !anim["spritesPerRow"].IsInt())
		{
			std::cout << filePath << " must contain spritesPerRow int member!" << std::endl;
			throw std::runtime_error("Invalid Spritesheet Format! " + filePath + " must contain spritesPerRow int member!");
		}
		page.spritesPerRow = anim["spritesPerRow"].GetInt();

		if (!anim.HasMember("numRows") || !anim["numRows"].IsInt())
		{
			std::cout << filePath << " must contain numRows int member!" << std::endl;
			throw std::runtime_error("Invalid Spritesheet Format! " + filePath + " must contain numRows int member!");
		}
		page.numRows = anim["numRows"].GetInt();


		page.uvBounds = glm::vec4(
			uvBoundsObj["sMin"].GetDouble(),
			uvBoundsObj["tMin"].GetDouble(),
			uvBoundsObj["sMax"].GetDouble(),
			uvBoundsObj["tMax"].GetDouble()
		);

		pages.push_back(page);

		// per-page frame capacity
		pageFrameCapacity.push_back( static_cast<uint32_t>(page.spritesPerRow * page.numRows));

		/////////////////////////////////////
		// Anim States (Only from first page)
		/////////////////////////////////////
		if (animIndex == 0) {
			// Validate and store animStates
			if (!anim.HasMember("animStates") || !anim["animStates"].IsArray())
			{
				std::cout << filePath << " must contain animStates array member!" << std::endl;
				throw std::runtime_error("Invalid Spritesheet Format! " + filePath + " must contain animStates array member!");
			}

			for (const auto& state : anim["animStates"].GetArray())
			{
				if (!state.HasMember("stateName") || !state["stateName"].IsString() ||
					!state.HasMember("startIndex") || !state["startIndex"].IsUint() ||
					!state.HasMember("endIndex") || !state["endIndex"].IsUint() ||
					!state.HasMember("shouldLoop") || !state["shouldLoop"].IsBool())
				{
					std::cout << filePath << " animState must contain stateName (string), startIndex (uint), endIndex (uint), shouldLoop (bool)!" << std::endl;
					throw std::runtime_error("Invalid Spritesheet Format! " + filePath + " animState members are invalid!");
				}

				// Load animation state metadata
				AnimState animState;
				animState.stateName = state["stateName"].GetString();
				animState.startIndex = state["startIndex"].GetUint();
				animState.endIndex = state["endIndex"].GetUint();
				animState.shouldLoop = state["shouldLoop"].GetBool();

				// Legacy Value (not used in cross page)
				animState.pageIndex = 0;

				// Handle callback events
				// Check if JSON contains "events"
				if (state.HasMember("events") && state["events"].IsArray())
				{
					// Loop through each event definition inside the "events" array
					for (const auto& event : state["events"].GetArray())
					{
						if (event.HasMember("name") && event["name"].IsString())
						{
							// Creates a new event struct
							AnimState::AnimStateEvent animStateEvent{};

							// Stores the event name e.g. idle
							animStateEvent.eventHandle = event["name"].GetString();

							// Checks if the event specifies an explicit frame index
							if (event.HasMember("frame") && event["frame"].IsInt())
							{
								// Stores the frame number relative to this animation state
								animStateEvent.frame = event["frame"].GetInt();
								animState.events.push_back(animStateEvent);
								continue;
							}

							// Default behavior when no frame is specified 
							// (event fires on the last frame of the animation state)
							animStateEvent.frame = animState.endIndex - animState.startIndex;
							animState.events.push_back(animStateEvent);
						}
					}
				}

				animStates.push_back(animState);
			}
		}
	}
}

/*************************************************************************
  \brief
	Constructor for UUID-based animation loading.
  \param[in] uuid
	Unique AssetUUID corresponding to a .gganim file.
  \details
	Uses the UUID system to resolve the animation file path and delegates
	loading to the existing file-path constructor. Records both the UUID
	and source file path for reference and editor tracking.
*************************************************************************/
GGSpriteAnimationSystem::GGAnimatedSprite::GGAnimatedSprite(const AssetUUID& uuid) 
{
	// Access the asset system can return a filepath when given a UUID
	auto& assetSystem = GGAssetID::GetGGAssetID();

	// Make a mutable copy of the file path (filePath was const)
	std::string filePath = assetSystem->GetFilePath(uuid);

	std::string assetBase = GGEngine::GetEngine().GetAssetManager()->GetAssetBasePath();
	if (assetBase.empty())
	{
		assetBase = "../../../Assets/"; // game build fallback
	}
	if (filePath.find(assetBase) == std::string::npos)
	{
		filePath = assetBase + filePath;
	}

	// Create a temporary GGAnimatedSprite using the file path then copy its data into this (empty)object
	*this = GGAnimatedSprite(filePath);

	sourceUUID = uuid;			// Track which UUID this animation was loaded from
	sourcePath = filePath;		// Keep the path to the .gganim file
}

/*************************************************************************
  \brief
	Initializes the GGSpriteAnimationSystem.
  \details
	Registers callbacks for play mode start/stop and asset reloads.
	Sets up the lambda for updating sprite animations each frame, including
	frame index progression and UV bounds calculation.
*************************************************************************/
bool GGSpriteAnimationSystem::Init()
{
	//std::cout << "[ECSSpriteAnimator] Init() called" << std::endl;

	GlobalEventBus::onPlayModeStart += onStartSimulate;
	GlobalEventBus::onPause += onStopSimulate;
	GlobalEventBus::onResume += onStartSimulate;
	GlobalEventBus::onStopPlayMode += onStopSimulate;

	updateSpriteAnims = [&](Entity entity, Sprite& sprite, SpriteAnimator& animator) {
		(void)entity;
#ifdef GG_EDITOR_BUILD
		auto& layerManager = GGLayerManager::Get();
		if (layerManager.IsEntityDisabled(entity)) {
			return;  // Skip disabled entity
		}
#endif
		double deltaTime = Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>().GetDeltaTime();
		if (deltaTime == 0.f) return;

		GGAnimatedSprite const& animation = GetAnimation(animator.animationAssetID);

		if (animator.currentAnimState >= animation.animStates.size()) {
			std::cout << "ERROR: Invalid animation state " << animator.currentAnimState
				<< " (max: " << animation.animStates.size() - 1 << ")" << std::endl;
			animator.currentAnimState = 0;
		}

		GGAnimatedSprite::AnimState const& state = animation.animStates[animator.currentAnimState];

		animator.frameTime += deltaTime;

		if (animator.frameTime >= animation.frameTime) {
			uint32_t maxFrameIndex = state.endIndex - state.startIndex;

			if (animator.currentFrameIndex >= maxFrameIndex) {
				if (state.shouldLoop) {
					animator.currentFrameIndex = 0;
				}
			}
			else
			{
				animator.currentFrameIndex++;
			}

			if (!animation.animStates[animator.currentAnimState].events.empty())
			{
				for (const auto& event : animation.animStates[animator.currentAnimState].events)
				{
					if (event.frame == animator.currentFrameIndex)
					{
						auto& world = GetEngineInstance().GetSceneManager().GetActiveScene();
						
						if (shouldSimulate)
						{
							Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>()
								.InvokeVoidFunction(entity, world, event.eventHandle);

							if (!world.IsEntityAlive(entity)) return;
						}
					}
				}
			}

			animator.frameTime = 0.0;
		}

		///////////////////////////////
		// Cross-page frame resolution
		///////////////////////////////
		// Absolute frame index across ALL pages
		uint32_t absoluteFrameIndex = animator.currentFrameIndex + state.startIndex;

		// Clamp absolute frame index to total capacity (Safety check if absoluteFrameIndex exceeds total capacity)
		if (!animation.pageFrameCapacity.empty()) {
			uint32_t total = 0;
			// Sum all page frame capacities
			for (auto cap : animation.pageFrameCapacity) {
				total += cap;
			}

			// If frames exist, clamp to last valid frame else default to 0
			if (absoluteFrameIndex >= total) {
				if (total > 0) {
					absoluteFrameIndex = total - 1;
				}
				else {
					absoluteFrameIndex = 0;
				}
			}
		}

		// Which texture page to use
		uint32_t targetPageIndex = 0;

		// Frame index inside that page
		uint32_t localFrameIndex = absoluteFrameIndex;

		// Keeps track of frame offsets while scanning pages
		uint32_t accumulator = 0;

		// Loop through each spritesheet page
		for (size_t i = 0; i < animation.pageFrameCapacity.size(); ++i) {

			// Number of frames this page can hold
			uint32_t capacity = animation.pageFrameCapacity[i];

			// Does the absolute frame fall inside this page?
			if (absoluteFrameIndex < accumulator + capacity) {

				// Found the correct page
				targetPageIndex = static_cast<uint32_t>(i);

				// Converts absolute frame to local page frame
				localFrameIndex = absoluteFrameIndex - accumulator;
				break;
			}

			// Move frame offset forward for next page
			accumulator += capacity;

		}
		
		if (targetPageIndex >= animation.pages.size()) {
			// Safety fallback, should never happen in valid data
			return;
		}

		const auto& page = animation.pages[targetPageIndex];

		// Determines column in the sprite grid
		int32_t columnIndex = static_cast<int32_t>(localFrameIndex % page.spritesPerRow);

		// Determines row in the sprite grid
		int32_t rowIndex = static_cast<int32_t>(localFrameIndex / page.spritesPerRow);

		// Flips Y-axis because OpenGL UV origin is bottom-left
		int32_t openGLRowIndex = (page.numRows - 1) - rowIndex;

		// Size of one sprite frame in UV space
		float frameWidth = page.uvBounds.z - page.uvBounds.x;
		float frameHeight = page.uvBounds.w - page.uvBounds.y;

		// Bottom left UV coordinate of the frame
		float minU = page.uvBounds.x + static_cast<float>(columnIndex) * frameWidth;
		float minV = page.uvBounds.y + static_cast<float>(openGLRowIndex) * frameHeight;

		// Top right UV coordinate of the frame
		float maxU = minU + frameWidth;
		float maxV = minV + frameHeight;

		// Automatic Texture Page Switch
		sprite.textureAssetID = page.textureAssetID;
		sprite.textureUVBounds = glm::vec4(minU, minV, maxU, maxV);
	};

	onReloadAssetsCallback =
	[&]()
	{
		for (auto& anim : anims)
		{
			//std::cout << "Reloading anim " << anim.sourcePath << std::endl;
			anim = GGAnimatedSprite(anim.sourcePath);
		}
	};

	GlobalEventBus::onReloadAssets += onReloadAssetsCallback;
	return true;
}

/*************************************************************************
  \brief
	Updates all sprite animations in the given world.
  \param[in,out] world
	ECS world instance containing Sprite and SpriteAnimator components.
  \details
	Iterates over all entities with Sprite and SpriteAnimator components
	and updates the frame index and UV bounds for each animation state.
*************************************************************************/
void GGSpriteAnimationSystem::Update(World& world)
{
	if (anims.empty()) return;
	world.ForEach<Sprite, SpriteAnimator>(updateSpriteAnims);
}

/*************************************************************************
  \brief
	Adds a new animation to the system.
  \param[in] sprite
	GGAnimatedSprite instance to add (moved into the system).
  \return
	Index of the animation within the system for reference by SpriteAnimator.
  \details
	Registers the animation by UUID and/or file path, and stores it in
	the internal animation vector.
*************************************************************************/
uint32_t GGSpriteAnimationSystem::AddAnimation(GGAnimatedSprite&& sprite)
{
	// determine the next index where this animation will be stored
	uint32_t index = static_cast<uint32_t>(anims.size());

	if (sprite.sourceUUID.has_value()) {
		// Store UUID to the animation index mapping
		uuidToIndexMap[sprite.sourceUUID.value()] = index;
	}
	if (!sprite.sourcePath.empty()) {
		// Store path to the animation index mapping
		pathToIndexMap[sprite.sourcePath] = index;
	}

	// Move the animation into the vector
	anims.push_back(std::move(sprite));

	// Return the index so SpriteAnimator can reference this animation
	return index;
}

/*************************************************************************
  \brief
	Retrieves a const reference to an animation by its index.
  \param[in] index
	Index of the animation in the system.
  \return
	Const reference to the GGAnimatedSprite at the specified index.
  \details
	If the index is out of bounds, returns the first animation. Throws
	if no animations exist.
*************************************************************************/
GGSpriteAnimationSystem::GGAnimatedSprite const& GGSpriteAnimationSystem::GetAnimation(uint32_t index) const
{
	if (anims.size() == 0) throw std::runtime_error("ERROR: AnimationSystem no animations found!");
	if (index >= anims.size()) return anims[0];
	return anims[index];
}

/*************************************************************************
  \brief
	Sets the next animation state by name for a SpriteAnimator.
  \param[in] newState
	Name of the animation state to switch to.
  \details
	Updates the currentAnimState and resets currentFrameIndex if the new
	state is different from the current state.
*************************************************************************/
void SpriteAnimator::SetNextAnimationState(std::string const& newState)
{
	uint32_t i{};

	auto const& animation = Systems::GetECSSystemsRegistry()
		.GetSystem<Systems::ECSSpriteAnimator>().GetAnimation(animationAssetID);

	for (auto const& states : animation.animStates)
	{
		if (states.stateName == newState)
		{
			// If already in this state
			if (currentAnimState == i)
			{
				// Always restart same state
				currentFrameIndex = 0;
				frameTime = 0.0;
				return;
			}


			currentFrameIndex = 0;
			currentAnimState = i;
			return;
		}

		i++;
	}
}

/*************************************************************************
  \brief
	Registers an animation by its UUID if not already registered.
  \param[in] uuid
	AssetUUID of the animation to register.
  \return
	Index of the animation in the system.
*************************************************************************/
uint32_t GGSpriteAnimationSystem::RegisterAnimationByUUID(const AssetUUID& uuid) {
	// Check if animation uuid is alr registered
	auto it = uuidToIndexMap.find(uuid);

	if (it != uuidToIndexMap.end()) {
		// Already present then return existing animation index
		return it->second;
	}

	// Load new animation from UUID using the constructor (loads the .gganim file)
	GGAnimatedSprite animation(uuid);
	
	// Delegate to AddAnimation() to handle maps/resgister and index
	return AddAnimation(std::move(animation));
}

/*************************************************************************
  \brief
	Registers an animation by its file path if not already registered.
  \param[in] relativePath
	Relative path of the .gganim file.
  \return
	Index of the animation in the system.
*************************************************************************/
uint32_t GGSpriteAnimationSystem::RegisterAnimationByPath(const std::string& relativePath) {

	// Check if animation path is alr registered
	auto it = pathToIndexMap.find(relativePath);

	if (it != pathToIndexMap.end()) {
		// Already present then return existing animation index
		return it->second;
	}

	// Load new animation from file-path using the constructor (loads the .gganim file)
	GGAnimatedSprite animation(relativePath);

	// Delegate to AddAnimation() to handle maps/resgister and index
	return AddAnimation(std::move(animation));
}

/*************************************************************************
  \brief
	Retrieves a const reference to an animation by UUID.
  \param[in] uuid
	AssetUUID of the animation to retrieve.
  \return
	Const reference to the GGAnimatedSprite corresponding to the UUID.
  \throws
	std::runtime_error if the UUID is not registered.
*************************************************************************/
GGSpriteAnimationSystem::GGAnimatedSprite const& 
GGSpriteAnimationSystem::GetAnimationByUUID(const AssetUUID& uuid) const {
	// Look up animation index by UUID
	auto it = uuidToIndexMap.find(uuid);

	if (it == uuidToIndexMap.end()) {
		// If UUID not registered, error
		throw std::runtime_error("Animation UUID not found!");
	}

	// Else return the animation stored at that index
	return anims[it->second];
}

/*************************************************************************
  \brief
	Retrieves a const reference to an animation by file path.
  \param[in] relativePath
	Relative path of the .gganim file.
  \return
	Const reference to the GGAnimatedSprite corresponding to the file path.
  \throws
	std::runtime_error if the file path is not registered.
*************************************************************************/
GGSpriteAnimationSystem::GGAnimatedSprite const& 
GGSpriteAnimationSystem::GetAnimationByPath(const std::string& relativePath) const {
	// Look up animation index by filepath
	auto it = pathToIndexMap.find(relativePath);
	if (it == pathToIndexMap.end()) {
		// If filepath not registered, error
		throw std::runtime_error("Animation Path not found!");
	}

	// Else return the animation stored at that index
	return anims[it->second];
}

/*************************************************************************
  \brief
	Clears all loaded animations and resets lookup maps.
	Call when switching projects to unload stale animation data.
*************************************************************************/
void GGSpriteAnimationSystem::ClearAnimations()
{
	anims.clear();
	pathToIndexMap.clear();
	uuidToIndexMap.clear();
}