/*************************************************************************
  \file       GGECSSpriteAnimator.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
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
#pragma once

#include <optional> 
#include <GGSystemInterface.hpp>
#include <GGECSComponents.hpp>

/*************************************************************************
  \class GGSpriteAnimationSystem
  \brief
    ECS system for frame-based sprite animation with state management.
  \details
    Manages a registry of GGAnimatedSprite assets and iterates over entities
    with SpriteAnimator + Sprite components to advance animation state.
    Each frame, the system:
    1. Accumulates delta time in SpriteAnimator::frameTime
    2. Checks if frame duration (1.0 / animFPS) exceeded
    3. Advances currentFrameIndex within the active AnimState range
    4. Updates Sprite::textureAssetID and Sprite::textureUVBounds
    5. Handles looping/clamping based on AnimState::shouldLoop
    
    The system uses a std::function callback (updateSpriteAnims) to enable
    hot-reloading of animation logic without recompilation. This pattern
    allows runtime behavior modification for debugging or mod support.
  \note
    Animation assets are stored by index, not by string keys. Systems must
    maintain external mapping from animation names to indices if string-based
    lookup is required. This design trades convenience for cache locality
    during iteration.
  \warning
    The system assumes single-threaded execution. Concurrent modification of
    anims vector during Update() will cause data races. Protect with mutex
    or restrict AddAnimation() to initialization phase only.
  \see
    SpriteAnimator, Sprite, GGAnimatedSprite
*************************************************************************/
class GGSpriteAnimationSystem : public SystemInterface
{
public:
/*************************************************************************
  \struct GGAnimatedSprite
  \brief
    Asset definition for multi-state, multi-page sprite animation.
  \details
    Encapsulates all data required to play a sprite animation sequence:
    - Texture pages for frame storage (supports atlas spanning)
    - Animation states with frame ranges and looping behavior
    - Timing parameters (FPS, precomputed frame duration)
    - UV layout parameters for frame extraction
    
    Loaded from JSON manifests with schema:
    {
      "texturePages": ["page1.png", "page2.png"],
      "animFPS": 12.0,
      "spritesPerRow": 8,
      "uvBounds": [0.0, 0.0, 1.0, 1.0],
      "animStates": [
        {"name": "idle", "page": 0, "start": 0, "end": 3, "loop": true},
        {"name": "run", "page": 0, "start": 4, "end": 11, "loop": true}
      ]
    }
    
    The class is move-only (deleted copy constructor/assignment) to prevent
    expensive vector copies when transferring animations into the system registry.
  \note
    Uses Rule of Five with move semantics. Default-generated destructors handle
    std::vector cleanup automatically. The move constructor is explicitly defined
    to demonstrate transfer semantics, though compiler-generated version would
    be identical.
  \warning
    texturePages stores asset IDs (uint32_t), not OpenGL texture handles. Systems
    must resolve these via GGECSStandardAssetManager<GLuint> before binding.
  \see
    AnimState, GGSpriteAnimationSystem::AddAnimation
*************************************************************************/
	struct GGAnimatedSprite
	{
/*************************************************************************
  \struct AnimState
  \brief
    Named animation state with frame range and playback configuration.
  \details
    Defines a contiguous range of frames [startIndex, endIndex] on a specific
    texture page, along with loop behavior. Multiple states can reference the
    same texture page for different animation clips (e.g., idle and run on
    one atlas, attack on another).
    
    Frame indexing is zero-based and relative to the texture atlas layout
    defined by spritesPerRow. The system computes UV coordinates as:
    ```cpp
    uint32_t row = frameIndex / spritesPerRow;
    uint32_t col = frameIndex % spritesPerRow;
    float u0 = col / float(spritesPerRow);
    float v0 = row / float(spritesPerRow);
    // Apply to uvBounds...
    ```
  \note
    State transitions are not automatic. Systems must call
    SpriteAnimator::SetNextAnimationState() explicitly to change states.
    Consider implementing a state machine graph for complex animation logic.
  \see
    GGAnimatedSprite, SpriteAnimator::SetNextAnimationState
*************************************************************************/
		struct AnimState
		{
            struct AnimStateEvent
            {
                std::string eventHandle{};
                uint32_t frame{};
            };

			std::string stateName{};
			uint32_t pageIndex{};
			uint32_t startIndex{};
			uint32_t endIndex{};
			bool shouldLoop{false};
            std::vector<AnimStateEvent> events{};
		};

		GGAnimatedSprite(std::string const& filePath);

		GGAnimatedSprite(GGAnimatedSprite&& other) noexcept
			: texturePages(std::move(other.texturePages))
            , pageFrameCapacity(std::move(other.pageFrameCapacity))
			, animStates(std::move(other.animStates))
			, animFPS(other.animFPS)
			, frameTime(other.frameTime)
            , pages(std::move(other.pages))
            , sourceUUID(std::move(other.sourceUUID))
            , sourcePath(std::move(other.sourcePath))
		{
		}

		GGAnimatedSprite& operator=(GGAnimatedSprite&& other) noexcept
		{
			if (this != &other)
			{
				texturePages = std::move(other.texturePages);
                pageFrameCapacity = std::move(other.pageFrameCapacity);
				animStates = std::move(other.animStates);
				animFPS = other.animFPS;
				frameTime = other.frameTime;
                pages = std::move(other.pages);
                sourceUUID = std::move(other.sourceUUID);
                sourcePath = std::move(other.sourcePath);
			}
			return *this;
		}

		std::vector<uint32_t> texturePages;

        // number of frames each page can hold
        std::vector<uint32_t> pageFrameCapacity;

		std::vector<AnimState> animStates;

		double animFPS{};

		double frameTime{};

        struct PageData {
            uint32_t textureAssetID{};
            int32_t spritesPerRow{};
            int32_t numRows{};
            glm::vec4 uvBounds{};
        };
        std::vector<PageData> pages;

        // UUID of the .gganim source file
        std::optional<AssetUUID> sourceUUID;

        // Path to the animation file
        std::string sourcePath;

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
        GGAnimatedSprite(const AssetUUID& uuid);
	};

	bool Init() override;

	void Update(World& world) override;

/*************************************************************************
  \brief
    Registers new animation asset with the system.
  \param[in] sprite
    GGAnimatedSprite to transfer into internal registry.
  \details
    Moves the animation into the anims vector, assigning it the next available
    index. Systems create SpriteAnimator components with animationAssetID set
    to this index for O(1) lookup during Update().
    
    The move-only semantics prevent accidental copies of large animation assets.
    Typical workflow:
    ```cpp
    GGAnimatedSprite anim("player_idle.json");
    uint32_t animID = animSystem.AddAnimation(std::move(anim));
    entity.AddComponent<SpriteAnimator>({animID, 0, 0, 0.0});
    ```
  \warning
    Not thread-safe. Call only during initialization or protected by mutex.
    Adding animations during Update() while iterating over entities causes
    undefined behavior if reallocation occurs.
  \note
    Consider returning the assigned index (anims.size() - 1) to enable
    immediate use without separate index tracking. Current void return requires
    caller to maintain animation-to-index mapping externally.
  \par Improvement Suggestion
  \code
    uint32_t AddAnimation(GGAnimatedSprite&& sprite) {
        anims.push_back(std::move(sprite));
        return static_cast<uint32_t>(anims.size() - 1);
    }
  \endcode
*************************************************************************/
    uint32_t AddAnimation(GGAnimatedSprite&& sprite);

/*************************************************************************
  \brief
    Retrieves immutable reference to animation asset by index.
  \param[in] index
    Animation asset ID from SpriteAnimator::animationAssetID.
  \return
    Const reference to GGAnimatedSprite (valid until next AddAnimation call).
  \warning
    No bounds checking. Passing invalid index causes undefined behavior
    (likely segfault or garbage data). Consider returning std::optional or
    throwing std::out_of_range in debug builds for safer access.
  \note
    Used by Update() to resolve animation metadata during frame advancement.
    The const reference prevents accidental modification of shared animation
    assets, enforcing immutability after loading.
  \par Usage
  \code
    const GGAnimatedSprite& anim = animSystem.GetAnimation(animID);
    Sprite::textureAssetID = anim.texturePages[state.pageIndex];
  \endcode
*************************************************************************/
	GGAnimatedSprite const& GetAnimation(uint32_t index) const;

/*************************************************************************
    \brief
    Register a new animation asset using its unique UUID.
    \param[in] uuid
    The UUID of the .gganim animation file to register.
    \return
    The numeric animationAssetID (index) assigned in the system.
    \details
    If the UUID has already been registered, the existing index is returned.
    Otherwise, the animation is loaded from disk via its UUID, assigned an
    internal index, and cached for future access. This ensures fast runtime
    lookups (O(1)) while maintaining UUID traceability in edit mode.
*************************************************************************/
    uint32_t RegisterAnimationByUUID(const AssetUUID& uuid);

/*************************************************************************
    \brief
    Register a new animation asset using its relative file path.
    \param[in] relativePath
    The relative path to the .gganim animation file (e.g. "Animations/Hero.gganim").
    \return
    The numeric animationAssetID (index) assigned in the system.
    \details
    This function is primarily for backward compatibility or manual
    registration by file path. If the animation is already registered,
    the existing index is returned. Otherwise, it loads the animation
    from disk, stores it in the system, and updates both path and UUID
    lookup maps.
*************************************************************************/
    uint32_t RegisterAnimationByPath(const std::string& relativePath);

/*************************************************************************
  \brief
    Retrieve a registered animation asset by its UUID.
  \param[in] uuid
    The UUID of the animation to fetch.
  \return
    Constant reference to the stored GGAnimatedSprite.
  \throws
    std::runtime_error if the UUID is not found in the lookup map.
  \details
    Provides O(1) access through the UUID - index map. This is mainly used
    in editor mode and asset interfaces to resolve animations without
    relying on numeric indices.
*************************************************************************/
    GGAnimatedSprite const& GetAnimationByUUID(const AssetUUID& uuid) const;

/*************************************************************************
  \brief
    Retrieve a registered animation asset by its file path.
  \param[in] relativePath
    The relative path of the animation file to fetch.
  \return
    Constant reference to the stored GGAnimatedSprite.
  \throws
    std::runtime_error if the path is not found in the lookup map.
  \details
    Allows direct access to animations using their relative file paths.
    Commonly used for editor tools or when UUID information is not yet
    available.
*************************************************************************/
    GGAnimatedSprite const& GetAnimationByPath(const std::string& relativePath) const;

/*************************************************************************
  \brief
    Clears all loaded animations and resets lookup maps.
    Call when switching projects to unload stale animation data.
*************************************************************************/
    void ClearAnimations();

    inline std::unordered_map<std::string, uint32_t> const& GetPathToIndexMap() const { return pathToIndexMap; }
private:

    Callback<> onReloadAssetsCallback;

    Callback<> onPlayModeCallback; // Callback when entering play mode
    Callback<> onStopModeCallback; // Callback when stopping play mode
	
	std::vector<GGAnimatedSprite> anims;

/*************************************************************************
  \brief
    Lookup map linking animation UUIDs to their internal vector indices.
  \details
    Enables O(1) retrieval of animations by UUID during edit-time operations.
    Only used for asset management and hot reload, never in runtime loops.
*************************************************************************/
    std::unordered_map<AssetUUID, uint32_t> uuidToIndexMap;

/*************************************************************************
    \brief
    Lookup map linking animation file paths to their internal vector indices.
    \details
    Provides fast reverse lookup from a relative path (e.g. "Animations/Hero.gganim")
    to the corresponding index within the animation vector. Used by editors
    and asset systems for consistency and reference validation.
*************************************************************************/
    std::unordered_map<std::string, uint32_t> pathToIndexMap;

/*************************************************************************
  \var GGSpriteAnimationSystem::updateSpriteAnims
  \brief
    Hot-swappable callback for per-entity animation update logic.
  \details
    Function signature: void(Entity, Sprite&, SpriteAnimator&)
    
    Initialized in Init() with default frame advancement logic. Can be replaced
    at runtime for debugging or custom behaviors:
    ```cpp
    // Override for slow-motion effect
    animSystem.updateSpriteAnims = [](Entity e, Sprite& s, SpriteAnimator& a) {
        a.frameTime += deltaTime * 0.5; // Half speed
        // ... rest of default logic ...
    };
    ```
    
    This pattern decouples animation logic from system structure, enabling
    gameplay programmers to modify animation behavior without touching system
    internals. Useful for:
    - Time dilation effects (slow-mo, time freeze)
    - Animation event callbacks (play sound on specific frames)
    - Debug visualization (highlight UV bounds, frame indices)
  \note
    std::function has slight overhead vs direct function pointer (~10ns per
    call). For maximum performance, consider template-based callback policy
    or devirtualization via PGO if profiling shows this as a bottleneck.
  \warning
    Callback captures `this` pointer. Ensure system lifetime exceeds all
    callback invocations. If callback is stored externally (e.g., in a scripting
    system), use std::weak_ptr or manual lifetime management to prevent dangling
    references.
*************************************************************************/
	std::function
		<void(Entity,
		Sprite&,
		SpriteAnimator&)> updateSpriteAnims;
};