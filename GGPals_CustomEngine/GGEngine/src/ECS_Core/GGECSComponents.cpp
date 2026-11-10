/*************************************************************************
  \file       GGECSComponents.cpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       3 October 2025
  \version    1.0
  \brief
    Defines ECS component structures with fixed-size serialization contracts.
  \details
    This file provides the fundamental component archetypes for GGEngine's
    Entity-Component-System architecture. Each component is a POD-like
    structure with compile-time verified serialization sizes, enabling
    efficient memory-mapped I/O, network replication, and cache-coherent
    iteration. Components follow strict size contracts enforced by
    static_assert to prevent structure padding mismatches across compilers
    and platforms. All components expose a COMPONENT_ID constexpr for runtime
    type identification without RTTI overhead.
  \dependencies
    <cstdint>, GGSystemInterface.hpp, GGECSInterface.hpp, GGMesh.hpp,
    GGMath/Types.hpp, glm (via GGMesh)
  \see
    Components (enum), GGECSInterface, GGSystemInterface
  \history
    - 1.0 (3 October 2025): Initial component definitions.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGECSComponents.hpp>
#include <GGECSAssetInterface.hpp>
#include <GGShader.hpp>
#include <GGEngineRender.hpp>
#include <GGSystemManagers.hpp>

/*************************************************************************
  \brief
	Returns the shader associated with this MeshRenderer.
	Falls back to the default shader if no shaderAssetID is set.
  \return
	Constant reference to the GGShader object.
*************************************************************************/
GGShader const& MeshRenderer::GetShader() const {
	if (shaderAssetID && GGECSStandardAssetManager<GGShader>::HasAsset(shaderAssetID)) 
        return GGECSStandardAssetManager<GGShader>::GetAsset(shaderAssetID);

	return Systems::GetSystemsRegistry().GetSystem<Systems::RenderManager>().GetDefaultShader();
}

/*************************************************************************
  \brief
    Resolves texture asset handle to OpenGL texture object.
  \return
    OpenGL texture name for glBindTexture.
  \throws AssetNotFoundException
    If textureAssetID is not registered.
*************************************************************************/
GLuint const Sprite::GetTextureID() const {
	if (textureAssetID && GGECSStandardAssetManager<GLuint>::HasAsset(textureAssetID)) 
        return GGECSStandardAssetManager<GLuint>::GetAsset(textureAssetID);

	return textureAssetID;
}


//GLuint const TextComponent::getFontId()const {
//    if(fontAssetID) return GGECSStandardAssetManager<GLuint>::GetAsset(fontAssetID);
//
//    return fontAssetID;
//}

std::string const TextComponent::getFontPath()const {
    if (fontAssetID && GGECSStandardAssetManager<std::string>::HasAsset(fontAssetID)) 
        return GGECSStandardAssetManager<std::string>::GetAsset(fontAssetID);

    return "Fonts/SourceSans3-Bold.ttf";//fontAssetID;
}

std::string const WorldTextComponent::getFontPath() const
{
    if (fontAssetID && GGECSStandardAssetManager<std::string>::HasAsset(fontAssetID)) 
        return GGECSStandardAssetManager<std::string>::GetAsset(fontAssetID);

    return "Fonts/SourceSans3-Bold.ttf";//fontAssetID;
}
