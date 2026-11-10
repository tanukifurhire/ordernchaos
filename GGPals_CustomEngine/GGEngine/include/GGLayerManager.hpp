/*************************************************************************
  \file       GGLayerManager.hpp
  \author     Abhishek Akilandeswai Patchamuthu
  \email      a.patchamuthu@digipen.edu
  \date       6 February 2026
  \version    1.0
  \brief
    Manages object, UI, and tilemap layers for entities in a world.
  \details
    Provides functions to scan the world, enable/disable layers,
    track entities, and support rendering order for UI and object layers.
    Includes singleton pattern for global access.
  \dependencies
    <string>, <vector>, <unordered_map>, <unordered_set>, <algorithm>, <functional>
    GGSystemManagers.hpp
 \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once


#include <string>
#include <iostream>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <functional>
#include <GGSystemManagers.hpp>


/*************************************************************************
  \brief
    Enumeration of supported layer types.
*************************************************************************/
enum class LayerType : int {
    OBJECT = 0,   // MeshRenderer entities
    UI = 1,    // UI entities
    TILEMAP = 2
};

/*************************************************************************
  \brief
    Key used to uniquely identify a layer.
*************************************************************************/
struct LayerKey
{
    int       layer;
    LayerType type;

    /*************************************************************************
  \brief
    Equality operator for unordered maps/sets.
  \param[in] l
    Another LayerKey
  \return
    True if both layer and type are equal
*************************************************************************/
    bool operator==(const LayerKey& l) const {
        return layer == l.layer && type == l.type;
    }
    /*************************************************************************
      \brief
        Less-than operator for sorting
      \param[in] l
        Another LayerKey
      \return
        True if this key is less than the other
    *************************************************************************/
    bool operator<(const LayerKey& l) const {
        if (layer != l.layer) return layer < l.layer;
        return static_cast<int>(type) < static_cast<int>(l.type);
    }
};

/*************************************************************************
  \brief
    Hash function for LayerKey
*************************************************************************/
struct LayerKeyHash
{
    size_t operator()(const LayerKey& k) const {
        return static_cast<size_t>(k.layer) * 2u + static_cast<size_t>(k.type);
    }
};

/*************************************************************************
  \brief
    Represents a layer containing entities.
*************************************************************************/
class GGLayer
{
public:
    GGLayer() = default;
    /*************************************************************************
      \brief
        Construct a layer with a key, name, and active state
      \param[in] k
        Layer key
      \param[in] n
        Layer name
      \param[in] a
        Active flag
    *************************************************************************/
    GGLayer(const LayerKey& k, const std::string& n, bool a)
        : key(k), name(n), active(a) {
    }

    const LayerKey& GetKey()      const { return key; }
    int                        GetLayer()    const { return key.layer; }
    LayerType                  GetType()     const { return key.type; }
    const std::string& GetName()     const { return name; }
    bool                       IsActive()    const { return active; }
    const std::vector<Entity>& GetEntities() const { return entities; }

    void SetName(const std::string& n) { name = n; }
    void Enable() { active = true; }
    void Disable() { active = false; }

    /*************************************************************************
     \brief
       Add an entity to the layer if not already present
     \param[in] e
       Entity to add
   *************************************************************************/
    void AddEntity(Entity e) {
        if (std::find(entities.begin(), entities.end(), e) == entities.end())
            entities.push_back(e);
    }
    void ClearEntities() { entities.clear(); }

private:
    LayerKey key = { 0, LayerType::OBJECT };
    std::string name = "New Layer";
    bool active = true;
    std::vector<Entity> entities;
};

/*************************************************************************
  \brief
    Singleton manager for all layers.
*************************************************************************/

class GGLayerManager
{
public:
    /*************************************************************************
      \brief
        Get singleton instance
      \return
        Reference to GGLayerManager
    *************************************************************************/
    static GGLayerManager& Get() {
        static GGLayerManager instance;
        return instance;
    }

    /*************************************************************************
        \brief
          Perform initial scan of world to populate layers
        \param[in] world
          World containing entities
      *************************************************************************/
    void InitialScan(World& world);

    /*************************************************************************
     \brief
       Check for changes in world, rescan if required
     \param[in] world
       World containing entities
   *************************************************************************/
    void CheckForChanges(World& world);

    /*************************************************************************
      \brief
        Mark layers as dirty for forced rescan
    *************************************************************************/
    void TriggerRescan() { dirty = true; }

    /*************************************************************************
      \brief
        Rebuild layers from current world state
      \param[in] world
        World containing entities
    *************************************************************************/
    void Rescan(World& world);

    /*************************************************************************
        \brief
          Register a layer if not already present
        \param[in] key
          Layer key
        \return
          Pointer to the GGLayer
      *************************************************************************/
    GGLayer* Register(const LayerKey& key);

    /*************************************************************************
    \brief
      Enable a specific layer
    \param[in] key
      Layer key
    \return
      True if successful
  *************************************************************************/
    bool EnableLayer(const LayerKey& key);
    /*************************************************************************
     \brief
       Disable a specific layer
     \param[in] key
       Layer key
     \return
       True if successful
   *************************************************************************/
    bool DisableLayer(const LayerKey& key);
    /*************************************************************************
     \brief
       Toggle a layer's active state
     \param[in] key
       Layer key
     \return
       True if successful
   *************************************************************************/
    bool ToggleLayer(const LayerKey& key);

    /*************************************************************************
    \brief
      Enable all layers of a specific type
    \param[in] type
      LayerType to enable
  *************************************************************************/
    void EnableAllOfType(LayerType type);

    /*************************************************************************
      \brief
        Disable all layers of a specific type
      \param[in] type
        LayerType to disable
    *************************************************************************/
    void DisableAllOfType(LayerType type);

    /*************************************************************************
      \brief
        Enable all layers
    *************************************************************************/
    void EnableAll();

    /*************************************************************************
     \brief
       Disable all layers
   *************************************************************************/
    void DisableAll();

    
    bool           IsEnabled(const LayerKey& key) const;
    GGLayer* GetLayer(const LayerKey& key);
    const GGLayer* GetLayer(const LayerKey& key) const;
    bool           Exists(const LayerKey& key) const;

  //for imui
    std::vector<GGLayer*> GetObjectLayers();     // sorted by layer number
    std::vector<GGLayer*> GetUILayers();         // sorted by layer number
    std::vector<GGLayer*> GetAllLayersSorted();  // sorted by (layer, type)


    /*************************************************************************
      \brief
        Check if entity is disabled
      \param[in] entity
        Entity to check
      \return
        True if entity is in a disabled layer
    *************************************************************************/

    bool IsEntityDisabled(Entity entity) const {
        

        bool result = disabledEntities.find(entity) != disabledEntities.end();

       
       

        return result;
    }

    /*************************************************************************
     \brief
       Get all disabled entities
     \return
       Reference to unordered_set of disabled entities
   *************************************************************************/
    const std::unordered_set<Entity>& GetDisabledEntities() const {
        return disabledEntities;
    }
   
    /*************************************************************************
      \brief
        Clear all layers and reset manager
    *************************************************************************/
    void ClearAll();

    /*************************************************************************
     \brief
       Print all layers to console (debugging)
   *************************************************************************/
    void PrintLayers() const;
   
    /*************************************************************************
     \brief
       Get all tilemap layers
     \return
       Vector of GGLayer pointers
   *************************************************************************/
    std::vector<GGLayer*> GetTilemapLayers() {
        std::vector<GGLayer*> out;
        for (auto& [k, layer] : layers)
            if (k.type == LayerType::TILEMAP) out.push_back(&layer);
        return out;
    }
   

private:
    GGLayerManager() = default;
    GGLayerManager(const GGLayerManager&) = delete;
    GGLayerManager& operator=(const GGLayerManager&) = delete;
    GGLayerManager(GGLayerManager&&) = delete;
    GGLayerManager& operator=(GGLayerManager&&) = delete;

    std::unordered_map<LayerKey, GGLayer, LayerKeyHash> layers;

    // rescan snapshot
    bool   dirty = true;
    size_t lastObjectCount = 0;
    size_t lastUICount = 0;
    size_t lastTextCount = 0;
    size_t lastTilemapCount = 0;
    size_t lastWorldTextCount = 0;
    std::unordered_set<LayerKey, LayerKeyHash> lastKeys;

    std::unordered_set<Entity> disabledEntities;

    void AddToDisabled(Entity e);
    void RemoveFromDisabled(Entity e);
   
};

