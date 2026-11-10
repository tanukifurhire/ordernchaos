/*************************************************************************
  \file       GGLayerManager.cpp
  \author     Abhishek Akilandeswai Patchamuthu
  \email      a.patchamuthu@digipen.edu
  \date       6 February 2026
  \version    1.0
  \brief
    Implementation of GGLayerManager for managing object, UI, and tilemap layers.
  \details
    Provides functionality to scan the world, register layers, enable/disable layers,
    maintain entity-layer mapping, and support rendering order.
 \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGLayerManager.hpp>
#include <unordered_map>

/*************************************************************************
  \brief
    Perform initial scan of the world and populate layers with entities.
  \param[in] world
    World object containing entities
*************************************************************************/
void GGLayerManager::InitialScan(World& world)
{
    layers.clear();
    lastKeys.clear();
    dirty = false;
    lastObjectCount = 0;
    lastUICount = 0;
    lastTextCount = 0;
    lastTilemapCount = 0;
    lastWorldTextCount = 0;
    world.ForEach<MeshRenderer>(
        std::function<void(Entity, MeshRenderer&)>(
            [&](Entity entity, MeshRenderer& renderer)
            {
                LayerKey key{ renderer.layer, LayerType::OBJECT };
                GGLayer* layer = Register(key);
                if (layer) layer->AddEntity(entity);
                lastKeys.insert(key);
                lastObjectCount++;
            }
        )
    );

    world.ForEach<WorldTextComponent>(
        std::function<void(Entity, WorldTextComponent&)>(
            [&](Entity entity, WorldTextComponent& renderer)
            {
                LayerKey key{ renderer.layer, LayerType::OBJECT };
                GGLayer* layer = Register(key);
                if (layer) layer->AddEntity(entity);
                lastKeys.insert(key);
                lastWorldTextCount++;
            }
        )
    );



    world.ForEach<UIComponent>(
        std::function<void(Entity, UIComponent&)>(
            [&](Entity entity, UIComponent& ui)
            {
                LayerKey key{ ui.layer, LayerType::UI };
                GGLayer* layer = Register(key);
                if (layer) layer->AddEntity(entity);
                lastKeys.insert(key);
                lastUICount++;
            }
        )
    );


    world.ForEach<TextComponent>(
        std::function<void(Entity, TextComponent&)>(
            [&](Entity entity, TextComponent& text)
            {
                LayerKey key{ text.layer, LayerType::UI };
                GGLayer* layer = Register(key);
                if (layer) layer->AddEntity(entity);
                lastKeys.insert(key);
                lastTextCount++;
            }
        )
    );
    world.ForEach<TilemapComponent, ECSGridComponent>(
        std::function<void(Entity, TilemapComponent&, ECSGridComponent&)>(
            [&](Entity entity, TilemapComponent&, ECSGridComponent&)
            {
                // All tilemaps go to layer 0 of TILEMAP type
                LayerKey key{ 0, LayerType::TILEMAP };
                GGLayer* layer = Register(key);
                if (layer) layer->AddEntity(entity);
                lastKeys.insert(key);
                lastTilemapCount++;
            }
        )
    );
    std::cout << "[GGLayerManager] InitialScan"
        << layers.size() << " layers." << std::endl;
}

/*************************************************************************
  \brief
    Check if the world has changed since last scan and trigger a rescan if necessary.
  \param[in] world
    World object containing entities
*************************************************************************/
void GGLayerManager::CheckForChanges(World& world)
{


    if (dirty) {
        Rescan(world);
        return;
    }


    size_t curObject = 0, curUI = 0, curText = 0, curTilemap = 0, curWorldText = 0;


    world.ForEach<MeshRenderer>(
        std::function<void(Entity, MeshRenderer&)>(
            [&](Entity, MeshRenderer&) { curObject++; }
        )
    );
    world.ForEach<UIComponent>(
        std::function<void(Entity, UIComponent&)>(
            [&](Entity, UIComponent&) { curUI++; }
        )
    );
    world.ForEach<TextComponent>(
        std::function<void(Entity, TextComponent&)>(
            [&](Entity, TextComponent&) { curText++; }
        )
    );

    world.ForEach<TilemapComponent, ECSGridComponent>(
        std::function<void(Entity, TilemapComponent&, ECSGridComponent&)>(
            [&](Entity, TilemapComponent&, ECSGridComponent&) {curTilemap++; }
        )
    );
    world.ForEach<WorldTextComponent>(
        std::function<void(Entity, WorldTextComponent&)>(
            [&](Entity, WorldTextComponent&) { curWorldText++; }
        )
    );

    if (curObject != lastObjectCount ||
        curUI != lastUICount ||
        curText != lastTextCount || curWorldText != lastWorldTextCount)
    {
        Rescan(world);
        return;
    }


    std::unordered_set<LayerKey, LayerKeyHash> curKeys;
    curKeys.reserve(lastKeys.size() + 4);

    world.ForEach<MeshRenderer>(
        std::function<void(Entity, MeshRenderer&)>(
            [&](Entity, MeshRenderer& r) { curKeys.insert({ r.layer, LayerType::OBJECT }); }
        )
    );

    world.ForEach<WorldTextComponent>(
        std::function<void(Entity, WorldTextComponent&)>(
            [&](Entity, WorldTextComponent& r) { curKeys.insert({ r.layer, LayerType::OBJECT }); }
        )
    );

    world.ForEach<UIComponent>(
        std::function<void(Entity, UIComponent&)>(
            [&](Entity, UIComponent& u) { curKeys.insert({ u.layer, LayerType::UI }); }
        )
    );
    world.ForEach<TextComponent>(
        std::function<void(Entity, TextComponent&)>(
            [&](Entity, TextComponent& t) { curKeys.insert({ t.layer, LayerType::UI }); }
        )
    );

    world.ForEach<TilemapComponent, ECSGridComponent>(
        std::function<void(Entity, TilemapComponent&, ECSGridComponent&)>(
            [&](Entity, TilemapComponent&, ECSGridComponent&) { curKeys.insert({ 0, LayerType::TILEMAP });  }
        )
    );


    if (curKeys != lastKeys) {
        Rescan(world);
    }


}

/*************************************************************************
  \brief
    Rebuild the layers from the current world state.
  \param[in] world
    World containing entities
*************************************************************************/
void GGLayerManager::Rescan(World& world)
{


    // Preserve active/disabled state — only clear entity lists.
    for (auto& [key, layer] : layers)
        layer.ClearEntities();

    lastKeys.clear();
    dirty = false;
    lastObjectCount = 0;
    lastUICount = 0;
    lastTextCount = 0;
    lastTilemapCount = 0;
    lastWorldTextCount = 0;

    // Rebuild from MeshRenderer
    world.ForEach<MeshRenderer>(
        std::function<void(Entity, MeshRenderer&)>(
            [&](Entity entity, MeshRenderer& renderer)
            {
                LayerKey key{ renderer.layer, LayerType::OBJECT };
                GGLayer* layer = Register(key);
                if (layer) layer->AddEntity(entity);
                lastKeys.insert(key);
                lastObjectCount++;
            }
        )
    );

    // Rebuild from MeshRenderer
    world.ForEach<WorldTextComponent>(
        std::function<void(Entity, WorldTextComponent&)>(
            [&](Entity entity, WorldTextComponent& renderer)
            {
                LayerKey key{ renderer.layer, LayerType::OBJECT };
                GGLayer* layer = Register(key);
                if (layer) layer->AddEntity(entity);
                lastKeys.insert(key);
                lastWorldTextCount++;
            }
        )
    );

    // Rebuild from UIComponent
    world.ForEach<UIComponent>(
        std::function<void(Entity, UIComponent&)>(
            [&](Entity entity, UIComponent& ui)
            {
                LayerKey key{ ui.layer, LayerType::UI };
                GGLayer* layer = Register(key);
                if (layer) layer->AddEntity(entity);
                lastKeys.insert(key);
                lastUICount++;
            }
        )
    );

    // Rebuild from TextComponent
    world.ForEach<TextComponent>(
        std::function<void(Entity, TextComponent&)>(
            [&](Entity entity, TextComponent& text)
            {
                LayerKey key{ text.layer, LayerType::UI };
                GGLayer* layer = Register(key);
                if (layer) layer->AddEntity(entity);
                lastKeys.insert(key);
                lastTextCount++;
            }
        )
    );
    world.ForEach<TilemapComponent, ECSGridComponent>(
        std::function<void(Entity, TilemapComponent&, ECSGridComponent&)>(
            [&](Entity entity, TilemapComponent&, ECSGridComponent&)
            {
                LayerKey key{ 0, LayerType::TILEMAP };
                GGLayer* layer = Register(key);
                if (layer) layer->AddEntity(entity);
                lastKeys.insert(key);
                lastTilemapCount++;
            }
        )
    );
    for (auto it = layers.begin(); it != layers.end(); ) {
        if (it->second.GetEntities().empty()) {
            it = layers.erase(it);
        }
        else {
            ++it;
        }
    }

    std::cout << "[GGLayerManager] Rescan " << layers.size() << " layers." << std::endl;
}

/*************************************************************************
  \brief
    Register a new layer or return existing one
  \param[in] key
    LayerKey to register
  \return
    Pointer to GGLayer
*************************************************************************/
GGLayer* GGLayerManager::Register(const LayerKey& key)
{
    auto it = layers.find(key);
    if (it != layers.end()) return &it->second;

    std::string name;
    switch (key.type) {
    case LayerType::OBJECT:
        name = "Object" + std::to_string(key.layer);
        break;
    case LayerType::UI:
        name = "UI" + std::to_string(key.layer);
        break;
    case LayerType::TILEMAP:
        name = "Tilemap";
        break;
    }

    auto [inserted, success] = layers.emplace(key, GGLayer(key, name, true));
    return &inserted->second;
}


/*************************************************************************
  \brief
    Enable a specific layer and all its entities
  \param[in] key
    LayerKey to enable
  \return
    True if layer exists
*************************************************************************/
bool GGLayerManager::EnableLayer(const LayerKey& key)
{
    auto it = layers.find(key);
    if (it == layers.end()) return false;
    it->second.Enable();
    for (Entity entity : it->second.GetEntities()) {
        RemoveFromDisabled(entity);
    }
    return true;
}

/*************************************************************************
  \brief
    Disable a specific layer and all its entities
  \param[in] key
    LayerKey to disable
  \return
    True if layer exists
*************************************************************************/
bool GGLayerManager::DisableLayer(const LayerKey& key)
{
    auto it = layers.find(key);
    if (it == layers.end()) return false;
    for (Entity entity : it->second.GetEntities()) {
        AddToDisabled(entity);
    }

    it->second.Disable();
    return true;
}

/*************************************************************************
  \brief
    Toggle the active state of a layer
  \param[in] key
    LayerKey to toggle
  \return
    True if layer exists
*************************************************************************/
bool GGLayerManager::ToggleLayer(const LayerKey& key)
{
    auto it = layers.find(key);
    if (it == layers.end()) return false;
    if (it->second.IsActive()) it->second.Disable();
    else                       it->second.Enable();
    return true;
}

/*************************************************************************
  \brief
    Enable all layers of a given type
  \param[in] type
    LayerType to enable
*************************************************************************/
void GGLayerManager::EnableAllOfType(LayerType type)
{
    for (auto& [k, layer] : layers)
        if (k.type == type) {
            layer.Enable();
            for (Entity entity : layer.GetEntities()) {
                RemoveFromDisabled(entity);
            }
        }
}
/*************************************************************************
  \brief
    Disable all layers of a given type
  \param[in] type
    LayerType to disable
*************************************************************************/
void GGLayerManager::DisableAllOfType(LayerType type)
{
    for (auto& [k, layer] : layers)
        if (k.type == type) {
            layer.Disable();
            for (Entity entity : layer.GetEntities()) {
                AddToDisabled(entity);
            }

        }
}

/*************************************************************************
  \brief
    Enable all layers
*************************************************************************/

void GGLayerManager::EnableAll()
{
    for (auto& [k, layer] : layers) {
        layer.Enable();
        for (Entity entity : layer.GetEntities()) {
            RemoveFromDisabled(entity);
        }
    }
}

/*************************************************************************
  \brief
    Disable all layers
*************************************************************************/

void GGLayerManager::DisableAll()
{
    for (auto& [k, layer] : layers) {
        layer.Disable();
        for (Entity entity : layer.GetEntities()) {
            AddToDisabled(entity);
        }

    }
}

/*************************************************************************
  \brief
    Check if a layer is enabled
  \param[in] key
    LayerKey to check
  \return
    True if enabled
*************************************************************************/
bool GGLayerManager::IsEnabled(const LayerKey& key) const
{
    auto it = layers.find(key);
    if (it == layers.end()) return false;
    return it->second.IsActive();
}

/*************************************************************************
  \brief
    Get layer pointer by key
  \param[in] key
    LayerKey to retrieve
  \return
    Pointer to layer or nullptr
*************************************************************************/
GGLayer* GGLayerManager::GetLayer(const LayerKey& key)
{
    auto it = layers.find(key);
    return (it != layers.end()) ? &it->second : nullptr;
}

const GGLayer* GGLayerManager::GetLayer(const LayerKey& key) const
{
    auto it = layers.find(key);
    return (it != layers.end()) ? &it->second : nullptr;
}

/*************************************************************************
  \brief
    Check if layer exists
  \param[in] key
    LayerKey to check
  \return
    True if exists
*************************************************************************/
bool GGLayerManager::Exists(const LayerKey& key) const
{
    return layers.find(key) != layers.end();
}

/*************************************************************************
     \brief
       Get all object layers, sorted by layer number
     \return
       Vector of GGLayer pointers
 *************************************************************************/
std::vector<GGLayer*> GGLayerManager::GetObjectLayers()
{
    std::vector<GGLayer*> out;
    for (auto& [k, layer] : layers)
        if (k.type == LayerType::OBJECT) out.push_back(&layer);

    std::sort(out.begin(), out.end(),
        [](const GGLayer* a, const GGLayer* b) { return a->GetLayer() < b->GetLayer(); });
    return out;
}

/*************************************************************************
     \brief
       Get all UI layers, sorted by layer number
     \return
       Vector of GGLayer pointers
   *************************************************************************/
std::vector<GGLayer*> GGLayerManager::GetUILayers()
{
    std::vector<GGLayer*> out;
    for (auto& [k, layer] : layers)
        if (k.type == LayerType::UI) out.push_back(&layer);

    std::sort(out.begin(), out.end(),
        [](const GGLayer* a, const GGLayer* b) { return a->GetLayer() < b->GetLayer(); });
    return out;
}

/*************************************************************************
      \brief
        Get all layers sorted by (layer, type)
      \return
        Vector of GGLayer pointers
    *************************************************************************/

std::vector<GGLayer*> GGLayerManager::GetAllLayersSorted()
{
    std::vector<GGLayer*> out;
    out.reserve(layers.size());
    for (auto& [k, layer] : layers) out.push_back(&layer);

    std::sort(out.begin(), out.end(),
        [](const GGLayer* a, const GGLayer* b) { return a->GetKey() < b->GetKey(); });
    return out;
}

/*************************************************************************
      \brief
        Clear all layers and reset manager
*************************************************************************/
void GGLayerManager::ClearAll()
{
    layers.clear();
    lastKeys.clear();
    lastObjectCount = lastUICount = lastTextCount = 0;
    dirty = true;
    disabledEntities.clear();
}

/*************************************************************************
  \brief
    Add an entity to the disabled set
  \param[in] e
    Entity to disable
*************************************************************************/
void GGLayerManager::AddToDisabled(Entity e)
{
    disabledEntities.insert(e);
}

/*************************************************************************
  \brief
    Remove an entity from the disabled set
  \param[in] e
    Entity to enable
*************************************************************************/
void GGLayerManager::RemoveFromDisabled(Entity e)
{
    disabledEntities.erase(e);
}

/*************************************************************************
  \brief
    Print all layers and their states to console (debugging)
*************************************************************************/
void GGLayerManager::PrintLayers() const
{
    std::cout << "\n========== LAYERS ==========" << std::endl;
    std::cout << "Total: " << layers.size() << std::endl;

    // Collect and sort by (layer, type) for clean output
    std::vector<const GGLayer*> sorted;
    sorted.reserve(layers.size());
    for (const auto& [k, layer] : layers) sorted.push_back(&layer);
    std::sort(sorted.begin(), sorted.end(),
        [](const GGLayer* a, const GGLayer* b) { return a->GetKey() < b->GetKey(); });

    LayerType lastType = LayerType::OBJECT;
    bool first = true;

    for (const GGLayer* layer : sorted) {
        if (first || layer->GetType() != lastType) {
            if (!first) std::cout << std::endl;
            std::cout << (layer->GetType() == LayerType::OBJECT
                ? "  --- OBJECT LAYERS ---"
                : "  --- UI LAYERS ---") << std::endl;
            lastType = layer->GetType();
            first = false;
        }

        std::cout << "    [" << layer->GetLayer() << "] "
            << layer->GetName()
            << (layer->IsActive() ? " ENABLED" : " DISABLED")
            << " (" << layer->GetEntities().size() << " entities)"
            << std::endl;
    }

    std::cout << "============================\n" << std::endl;
}  