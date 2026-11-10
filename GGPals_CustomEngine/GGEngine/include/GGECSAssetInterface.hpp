/*************************************************************************
  \file       GGECSAssetInterface.hpp
  \author     Tai Xiao Xuan, Tok Duan Jie Nelson
  \par        Email: xiaoxuan.tai@digipen.edu, t.duanjiennelson@digipen.edu
  \date       5 November 2025
  \version    2.0
  \contribute Xiao Xuan(90%), Nelson(10% added UUID)
  \brief
    ECS asset management interface with compile-time type safety.
  \details
    Provides a type-safe, template-based asset registration and retrieval
    system for the GGEngine ECS architecture. Uses C++20 concepts to constrain
    acceptable asset types (GLuint textures, GGAudioAsset, GGShader) and
    leverages compile-time polymorphism to eliminate virtual dispatch overhead.
    Assets are identified by uint32_t handles and resolved to filesystem paths
    at runtime, with actual asset loading delegated to GGAssetManager.
  \dependencies
    <cstdint>, <unordered_map>, <memory>, <concepts>, <string>, <stdexcept>,
    GL/glew.h, GGShader.hpp, GGAssetManager.hpp, GGEngine.hpp
  \see
    GGAssetManager, GGEngine, GGShader, GGAudioAsset
  \history
    - 1.0 (3 October 2025): Initial version with concept-based constraints.
    - 2.0 (5 November 2025): Added handling of Prefabs, Scripts and Scene
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include <GGAssetID.hpp>
#include <GGEngine.hpp>
#include <GGSystemManagers.hpp>
#include <GGTilemapData.hpp>

#include <cstdint>
#include <unordered_map>
#include <memory>
#include <concepts>
#include <vector>
#include <GL/glew.h>
#include <GGShader.hpp>
#include <GGScriptAsset.hpp>
#include <GGECSWorld.hpp>
#include <string>
#include <optional>
#include <stdexcept>

/*************************************************************************
  \class AssetNotFoundException
  \brief
    Exception thrown when asset lookup fails for a given ID.
  \details
    Derives from std::runtime_error and formats a descriptive message
    containing the missing asset's ID. This exception indicates a logic
    error where code attempts to retrieve an unregistered asset handle.
  \see
    GGECSStandardAssetManager::GetAsset
*************************************************************************/
class AssetNotFoundException : public std::runtime_error {
public:
/*************************************************************************
  \brief
    Constructs exception for missing asset.
  \param[in] id
    The asset ID that was not found in the registry.
  \par Usage
  \code
    throw AssetNotFoundException(42);
    // Output: "Asset with ID 42 not found"
  \endcode
*************************************************************************/
	AssetNotFoundException(uint32_t id)
		: std::runtime_error("Asset with ID " + std::to_string(id) + " not found") {}
};

/*************************************************************************
  \class AssetFailedToLoadException
  \brief
    Exception thrown when an asset file cannot be loaded successfully.
  \details
    Indicates a runtime failure during asset resolution or deserialization.
    Typically thrown when loading Prefabs, Scenes, Scripts, or other assets
    whose loading process is not guaranteed to succeed. Includes the filepath
    in the error message for easier debugging.

  \see
    GGECSStandardAssetManager::RegisterAsset
*************************************************************************/
class AssetFailedToLoadException : public std::runtime_error {
public:
    /*************************************************************************
      \brief
        Constructs an exception for a failed asset load operation.
      \param[in] str
        Filesystem path of the asset that failed to load.
      \note
        Error message format:
          "Asset with filepath <path> cannot be loaded"
    *************************************************************************/
    AssetFailedToLoadException(std::string const& str)
        : std::runtime_error("Asset with filepath " + str + " cannot be loaded") {}
};

/*************************************************************************
  \brief
    Forward declarations for asset types supported by the ECS asset manager.
  \details
    These declarations allow the template-based asset manager to reference
    Prefab and Script asset types without requiring the full definitions
    at header-parse time. Full type information is only needed inside
    RegisterAsset or GetAsset, making the forward declarations sufficient.
*************************************************************************/
class GGAudioAsset;
struct PrefabManager::Prefab;
struct GGScriptAsset;

/*************************************************************************
  \concept IsStandardAsset
  \brief
    Concept constraining asset types to engine-supported formats.
  \tparam T
    Type to validate against standard asset types.
  \details
    Restricts template instantiation to GLuint (OpenGL texture handles),
    GGAudioAsset (audio resources), and GGShader (shader programs). This
    compile-time constraint prevents misuse of the asset management system
    and enables if-constexpr branching for type-specific loading logic.
  \note
    Expandable via disjunction if additional asset types are introduced.
    Consider using std::variant<GLuint, GGAudioAsset, GGShader> for
    runtime polymorphism if dynamic asset type discovery is required.
*************************************************************************/
template<typename T>
concept IsStandardAsset =   std::same_as<T, GLuint> || 
                            std::same_as<T, GGAudioAsset> || 
                            std::same_as<T, GGShader> || 
                            std::same_as<T, PrefabManager::Prefab> || 
                            std::same_as<T, GGScriptAsset> || 
                            std::same_as<T, World> || 
                            std::same_as<T, GGSpriteAnimationSystem::GGAnimatedSprite> ||
                            std::same_as<T, std::string> ||
                            std::same_as<T, GGTilemap>;

/*************************************************************************
  \class GGECSStandardAssetManager
  \brief
    Static asset registry with compile-time type specialization.
  \tparam Asset
    Must satisfy IsStandardAsset concept (GLuint, GGAudioAsset, GGShader).
  \details
    Maintains a global mapping from uint32_t asset IDs to filesystem paths.
    Uses template specialization to dispatch to the correct GGAssetManager
    loading function at compile-time. The static inline map ensures single
    definition across translation units (C++17 feature). Thread-safety is
    not guaranteed; external synchronization required for concurrent access.
  \see
    GGAssetManager, GGEngine::GetAssetManager
  \par Design Rationale
    The static inline map pattern eliminates the need for explicit template
    instantiation while maintaining type safety. The if-constexpr branching
    compiles away unreachable code paths, ensuring zero-cost abstraction.
*************************************************************************/
template<IsStandardAsset Asset> 
class GGECSStandardAssetManager {
public:

    /*************************************************************************
     \brief 
        Register an asset using its UUID and assign it a unique ID

     \param[in] 
        uuid The asset's universally unique identifier
     \return 
        Unique 32-bit asset ID for ECS usage, or 0 on failure

     \details 
        This overload automatically assigns a unique ID from an internal
        pool and creates a bidirectional mapping between the UUID and ID:
            1. Initializes the available ID pool (1 to MAX_ASSETS) on first call
            2. Returns existing ID if UUID is already registered (idempotent)
            3. Validates UUID exists in the GGAssetID registry
            4. Allocates next available ID from the queue
            5. Creates UUID-to-ID mapping
            6. Loads asset into appropriate manager based on type
            7. Logs the registration for debugging

        Type-specific loading resolved at compile-time:
            - GLuint: GetTexture(path)
            - GGAudioAsset: GetAudioAsset(path)
            - GGScriptAsset: GetScript(path)
            - World: AddNewScene(path)
            - GGShader: GetShaderByKey(path) (default)

     \return
        Returns 0 if UUID is invalid or no IDs are available, otherwise
        returns the newly assigned or existing asset ID

     \note 
        This function is idempotent - calling it multiple times with the
        same UUID returns the same ID without reloading. The ID pool is
        shared across all instances of this template specialization.

     \par Usage
     \code
       // Get UUID from asset system
       auto uuid = GGAssetID::GetGGAssetID()->GetUUIDFromPath("textures/hero.png");

       // Register with auto-assigned ID
       uint32_t texId = GGECSStandardAssetManager<GLuint>::RegisterAsset(*uuid);

       // Safe to call again - returns same ID
       uint32_t sameId = GGECSStandardAssetManager<GLuint>::RegisterAsset(*uuid);
     \endcode
     *************************************************************************/
    static inline uint32_t RegisterAsset(const AssetUUID& uuid)
    {
        // Initialize availableIDs
        if (uuidToIDMap.empty() && availableIDs.empty())
        {
            for (uint32_t i{ 1 }; i < static_cast<uint32_t>(MAX_ASSETS); i++)
            {
                availableIDs.push(i);
            }
        }

        // Check if there's already an existing entry on uuidToIDMap
        for (auto& [id, assetUUID] : uuidToIDMap)
        {
            if (assetUUID == uuid)
            {
                return id;
            }
        }

        auto& assetSystem = GGAssetID::GetGGAssetID();

        // Check whether UUID is valid
        if (!assetSystem->DoesUUIDExist(uuid)) {
            std::cerr << "[GGECSStandardAssetManager] Invalid UUID, cannot register asset.\n";
            return 0;
        }

        // Ensure there�s at least one available ID
        if (availableIDs.empty()) {
            std::cerr << "[GGECSStandardAssetManager] No more available asset IDs!\n";
            return 0;
        }

        // Get a new ID from queue
        uint32_t newID = availableIDs.front();

        // Lookup the file path from the UUID
        const std::string& filePath = assetSystem->GetFilePath(uuid);
        //std::cout << " the file path that is accessed" << filePath << std::endl;

        if constexpr (std::same_as<Asset, GLuint>)
        {
            
                //std::cout << " the file path that is accessed" << filePath << std::endl;
                GGEngine::GetEngine().GetAssetManager()->GetTexture(filePath);
            
        }
        else  if constexpr (std::same_as<Asset, std::string>)
        {
            //GGEngine::GetEngine().GetAssetManager()->GetTexture(filePath);
            if (filePath.find(".ttf") != std::string::npos) {
                // Use the Font loader instead of the Texture loader

               
                GGEngine::GetEngine().GetAssetManager()->GetFontPath(filePath);
            }
            else {
                //
              
            }
        }
        
        else if constexpr (std::same_as<Asset, GGAudioAsset>) GGEngine::GetEngine().GetAssetManager()->GetAudioAsset(filePath);
        
        else if constexpr (std::same_as<Asset, GGScriptAsset>) GGEngine::GetEngine().GetAssetManager()->GetScript(filePath);
        
        else if constexpr (std::same_as<Asset, PrefabManager::Prefab>) {
            const std::string & fullPath = GGEngine::GetEngine().GetAssetManager()->ResolvePath(filePath);
            GGEngine::GetEngine().GetPrefabManager().LoadPrefabsFromFile(fullPath);
        }
        
        else if constexpr (std::same_as<Asset, World>) {
            const std::string& fullPath = GGEngine::GetEngine().GetAssetManager()->ResolvePath(filePath);
            if (!GGEngine::GetEngine().GetSceneManager().AddNewScene(fullPath, GGEngine::GetEngine().GetPrefabManager()))
            {
                throw AssetFailedToLoadException(filePath);
            }
        }

        else if constexpr (std::same_as<Asset, GGSpriteAnimationSystem::GGAnimatedSprite>)
        {
            auto& animSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSSpriteAnimator>();
            newID = animSystem.RegisterAnimationByPath(filePath);
        }

        else if constexpr (std::same_as<Asset, GGTilemap>)
        {
            auto& gridSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSGrid>();
            gridSystem.RegisterTileMap(filePath);
        }
        
        else GGEngine::GetEngine().GetAssetManager()->GetShaderByKey(filePath);

        availableIDs.pop();

        // Save mapping only AFTER successfully getting asset
        uuidToIDMap[newID] = uuid;

        /*std::cout << "[GGECSStandardAssetManager] Registered UUID "
            << uuids::to_string(uuid)
            << " as ID " << newID
            << " (" << filePath << ")\n";*/

        return newID;
    }

/*************************************************************************
  \brief
    Checks if an asset ID is registered.
  \param[in] id
    Asset identifier to query.
  \return
    true if the ID exists in the registry, false otherwise.
  \details
    Performs a simple map lookup without triggering asset loading or
    validation. This is a lightweight check suitable for guard clauses.
  \par Usage
  \code
    if (GGECSStandardAssetManager<GLuint>::HasAsset(HERO_TEX)) {
        // Safe to call GetAsset
    } else {
        // Handle missing asset registration
    }
  \endcode
*************************************************************************/
	static inline bool HasAsset(uint32_t const id)
	{
        auto it = uuidToIDMap.find(id);
        if (it == uuidToIDMap.end()) {
            return false;
        }
		
		return true;
	}

/*************************************************************************
  \brief
    Clears all registered assets and resets the ID pool.
    Call when switching projects to avoid stale IDs from the old project.
*************************************************************************/
    static inline void ClearAll()
    {
        uuidToIDMap.clear();
        // Reset the ID pool so IDs start from 1 again
        while (!availableIDs.empty()) availableIDs.pop();
    }

/*************************************************************************
  \brief
    Retrieves a const reference to a loaded asset.
  \param[in] id
    Asset identifier registered via RegisterAsset.
  \return
    Const reference to the asset object (type determined by template param).
  \throws AssetNotFoundException
    If the ID is not registered in assetToPathMap.
  \details
    Looks up the asset's filesystem path and delegates to GGAssetManager's
    type-specific accessor. The returned reference is valid for the lifetime
    of the GGAssetManager's internal cache. Callers must not assume ownership
    or retain pointers across asset manager reloads.
    
    Type resolution (compile-time):
    - GLuint: Returns OpenGL texture handle (uint32_t)
    - GGAudioAsset: Returns audio resource object reference
    - GGShader: Returns shader program object reference
  \note
    The const reference prevents modification of cached assets, enforcing
    read-only access patterns. If mutable access is required, the asset
    manager's internal API must be extended (not recommended for thread-safety).
  \par Usage
  \code
    try {
        GLuint tex = GGECSStandardAssetManager<GLuint>::GetAsset(HERO_TEX);
        glBindTexture(GL_TEXTURE_2D, tex);
    } catch (const AssetNotFoundException& e) {
        std::cerr << e.what() << std::endl;
    }
  \endcode
*************************************************************************/
	static inline Asset const& GetAsset(uint32_t const id) 
	{
		// Find asset 
		auto it = uuidToIDMap.find(id);

        if (it == uuidToIDMap.end())
        {
            throw AssetNotFoundException(id);
        }

        if constexpr (std::same_as<Asset, GLuint>) {
           
            std::string filePath = GGAssetID::GetGGAssetID()->GetFilePath(it->second);

        
    
            return GGEngine::GetEngine().GetAssetManager()->GetTexture(filePath);

        }
        else if constexpr (std::same_as<Asset, std::string>) {

            std::string filePath = GGAssetID::GetGGAssetID()->GetFilePath(it->second);

            // Check if this is actually a font
            if (filePath.find(".ttf") != std::string::npos) {
               
                std::replace(filePath.begin(), filePath.end(), '\\', '/');
              /*  std::cout << "[GetAsset] Font Atlas Texture ID: " << id << " follow uuid " << (it != uuidToIDMap.find(id) ? "yes " : "no ")
                    << " for font: " << filePath << std::endl;*/
                 return GGEngine::GetEngine().GetAssetManager()->GetFontPath(filePath);
                
            }

            else {
                // This satisfies the compiler and catches logic errors
                throw std::runtime_error("[GetAsset] Expected .ttf for string asset, but found: " + filePath);
            }
        }
        else if constexpr (std::same_as<Asset, GGAudioAsset>)
            return GGEngine::GetEngine().GetAssetManager()->GetAudioAsset(
                GGAssetID::GetGGAssetID()->GetFilePath(it->second)
            );
        else if constexpr (std::same_as<Asset, PrefabManager::Prefab>)
            return GGEngine::GetEngine().GetPrefabManager().GetPrefabByName(
                GGAssetID::GetGGAssetID()->GetFilePath(it->second)
            );

        else if constexpr (std::same_as<Asset, GGScriptAsset>)
        {
            //return GGEngine::GetEngine().GetAssetManager()->GetScript(
              //  GGAssetID::GetGGAssetID()->GetFilePath(it->second)
            //);
            const std::string& filePath = GGAssetID::GetGGAssetID()->GetFilePath(it->second);
            if (filePath.empty())
                throw AssetNotFoundException(id);
            return GGEngine::GetEngine().GetAssetManager()->GetScript(filePath);
        }

        else if constexpr (std::same_as<Asset, World>)
        {
            return GGEngine::GetEngine().GetSceneManager().GetLoadedSceneByName(
                GGAssetID::GetGGAssetID()->GetFilePath(it->second)
            );
        }

        else if constexpr (std::same_as<Asset, GGSpriteAnimationSystem::GGAnimatedSprite>) 
        {
            auto& animSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSSpriteAnimator>();
            return animSystem.GetAnimationByPath(
                GGAssetID::GetGGAssetID()->GetFilePath(it->second)
            );
        }

        else if constexpr (std::same_as<Asset, GGTilemap>)
        {
            auto& gridSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSGrid>();
            return gridSystem.GetTileMap(
                GGAssetID::GetGGAssetID()->GetFilePath(it->second)
            );
        }

		else 
            return GGEngine::GetEngine().GetAssetManager()->GetShaderByKey(
                GGAssetID::GetGGAssetID()->GetFilePath(it->second)
            );
        
	}

/*************************************************************************
  \brief
    Retrieves the UUID associated with a given asset ID.
  \param[in] id
    The asset ID to query.
  \return
    std::optional<AssetUUID> � UUID if found, std::nullopt otherwise.
  \details
    Enables reverse lookup for serialization (e.g., ToJSON).
*************************************************************************/
    static inline std::optional<AssetUUID> GetUUIDFromID(uint32_t id)
    {
        auto it = uuidToIDMap.find(id);
        if (it != uuidToIDMap.end())
            return it->second;

        return std::nullopt;
    }

/*************************************************************************
  \brief
    Returns the resolved file paths of all currently registered assets.
  \return
    Vector of file path strings for every UUID in the registry.
  \details
    Used to lazily register composed script types that are known to the
    asset system but have not yet been registered with the script system.
    Paths are resolved relative to their UUID entries via GGAssetID.
*************************************************************************/
    static inline std::vector<std::string> GetAllRegisteredPaths()
    {
        auto& assetSystem = GGAssetID::GetGGAssetID();
        std::vector<std::string> paths;
        paths.reserve(uuidToIDMap.size());
        for (const auto& [id, uuid] : uuidToIDMap)
            paths.push_back(assetSystem->GetFilePath(uuid));
        return paths;
    }

private:
    /*************************************************************************
      \brief
        Maximum number of assets that can be registered for this Asset type.
      \details
        Defines the ID range [1, MAX_ASSETS). Value chosen to balance memory
        footprint and scalability for large projects.
    *************************************************************************/
    static constexpr size_t MAX_ASSETS = 16384;

    /*************************************************************************
      \brief
        Pool of reusable asset IDs.
      \details
        Acts as the allocator for asset IDs. Populated lazily on the first
        call to RegisterAsset. IDs are issued sequentially and never reused
        unless manually freed (not currently supported).
    *************************************************************************/
    static inline std::queue<uint32_t> availableIDs;

    /*************************************************************************
      \brief
        Mapping of assigned ECS asset IDs to their corresponding UUIDs.
      \details
        Serves as the authoritative registration record for this asset type.
        Enables:
          - Duplicate-prevention during registration
          - Reverse lookup (ID → UUID)
          - Asset fetching via filesystem path resolution
    *************************************************************************/
    static inline std::unordered_map<uint32_t, AssetUUID> uuidToIDMap;
};