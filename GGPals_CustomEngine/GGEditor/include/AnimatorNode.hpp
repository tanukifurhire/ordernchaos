/*************************************************************************
  \file		  AnimatorNode.hpp
  \author	  Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief	  Declares the AnimatorNode used for the Animator dock tab 
              in the GGEngine editor.

  \details	  
	Defines the AnimatorNode class, which integrates with the editor's
	scene system to provide a dedicated Animator panel within the ImGui
	docking layout. This node registers initialization, update, and
	docking callbacks, enabling the editor to create, size, and render
	the Animator window. The class currently serves as a foundation for
	future animation-editing tools and visualization interfaces.
  \dependencies
	- EditorSceneBase.hpp
	- <memory>
	- ImGui (via implementation in the corresponding .cpp file)
  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <EditorSceneBase.hpp>
#include <memory>

class AnimatorNode : public EditorSceneBase
{
public:
	/*************************************************************************
	  \brief
		Constructs an AnimatorNode and sets up its callback handles
		for initialization, update, and dock building.
	*************************************************************************/
	AnimatorNode();

	/*************************************************************************
	  \brief
		Initializes the AnimatorNode. Currently performs no operations.
	*************************************************************************/
	virtual void Init() override;

	/*************************************************************************
	  \brief
		Builds and docks the Animator window in the editor UI.
	*************************************************************************/
	virtual void DockBuild() override;

	/*************************************************************************
	  \brief
		Updates the Animator window each frame, setting its size
		and managing ImGui rendering.
	*************************************************************************/
	virtual void Update() override;
};

/*************************************************************************
  \brief
	Provides access to a singleton instance of AnimatorNode.
  \return
	Reference to the unique_ptr managing the AnimatorNode instance.
*************************************************************************/
inline auto& GetAnimatorNode()
{
	static std::unique_ptr<AnimatorNode> animator = std::make_unique<AnimatorNode>();
	return animator;
}