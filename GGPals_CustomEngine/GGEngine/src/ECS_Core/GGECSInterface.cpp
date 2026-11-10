/*************************************************************************
  \file       GGECSInterface.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       8 September 2025
  \version    1.0
  \brief
    Defines the GGEngine ECS Interface.
  \details
    GGEngine's ECS provides basic functionality such as Entity and Component
    interfaces for other programmer's reference, and basic functionality such
    as AddComponent, RemoveComponent and GetComponent.
  \dependencies
    <cstdint>, <iostream>, <cassert>, <vector>, <unordered_map>, <unordered_set>,
    <memory>, <functional>, <tuple>, <queue>, <typeinfo>
  \see
    Vector2Int, Vector3, Matrix3x3
  \history
    - 1.0 (6 September 2025): Initial version.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGECSInterface.hpp>
#include <GGECSGameObjectBridge.hpp>

/*************************************************************************
  \brief
    Serializes entity's component to JSON.
  \param[in] entity
    Entity whose component to serialize.
  \param[in,out] writer
    RapidJSON writer for output.
  \details
    Writes component type name (from typeid().name()) as JSON key, then
    delegates to ComponentSerializer<T>::ToJSON for component data serialization.
    If entity does not have component, no output is written.
  \note
    Implementation not shown but inferred from design patterns. Likely checks
    GetComponent(entity) != nullptr before serializing.
  \see
    ComponentSerializer<T>::ToJSON
*************************************************************************/
template <IsComponent T>
void ComponentArray<T>::SerializeToJSON(Entity entity, rapidjson::PrettyWriter<rapidjson::StringBuffer>& writer) const {
    auto it = m_entityToIndex.find(entity);
    if (it == m_entityToIndex.end()) {
        return; // if entity not found, return
    }

    writer.Key(ComponentTypeInfo<T>::GetTypeName());
    ComponentSerializer<T>::ToJSON(m_components[it->second], writer);
} 

template class ComponentArray<Rigidbody2DFloat>;
template class ComponentArray<MeshRenderer>;
template class ComponentArray<AABBFloat>;
template class ComponentArray<MovementInput>;
template class ComponentArray<Health>;
template class ComponentArray<EntityName>;
template class ComponentArray<PrefabInstance>;
template class ComponentArray<ClickToMove>;
template class ComponentArray<Sprite>;
template class ComponentArray<SpriteAnimator>;
template class ComponentArray<ScreenBounds>;
template class ComponentArray<LocalTransform>;
template class ComponentArray<WorldTransform>;
template class ComponentArray<HierarchyComponent>;
template class ComponentArray<NameComponent>;
template class ComponentArray<ScriptComponent>;
template class ComponentArray<ECSGridComponent>;
template class ComponentArray<AudioSource>;
template class ComponentArray<TilemapComponent>;
template class ComponentArray<UIComponent>;
template class ComponentArray<TextComponent>;
template class ComponentArray<WorldTextComponent>;
template class ComponentArray<ObjectEffectComponent>;