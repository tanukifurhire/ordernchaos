/*************************************************************************
  \file		  GGSystemInterface.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 October 2025
  \version    1.0
  \brief
	Interfaces for engine and ECS system managers.

  \details
	Provides two abstract interfaces:
	  1. SystemManagerInterface – for non-ECS systems (Audio, etc.)
	  2. SystemInterface – for ECS-related systems (Physics, Render, etc.)

  \dependencies
	- GGEvent.hpp
	- World


  \history
	- 1.0 (10 October 2025): Initial version.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#pragma once
#include <GGEvent.hpp>

class World;

/*************************************************************************
  \brief
    System Manager Interface
	
	For Systems such as:
	-	Audio Manager
	-	And whatever stuff you need to run in GGEngine::Update(), that 
		doesn't pertain to the ECS.

  \details
    We'll Inherit from this Interface for any Systems Management code we
	write, in order to assist the Debug Perfomance Visualizer.

  \par Usage
  \code

	// Inheriting from SystemManagerInterface (EXAMPLE)
	class GGExampleSystem : public SystemManagerInterface 
	{
	public:
		bool Init() override;

		void Update() override;

		// Your implementation...
	private:
		// Other implementation...
	}

  \endcode
*************************************************************************/
class SystemManagerInterface {
public:
	virtual bool Init() = 0;

	virtual void Update() = 0;
};

/*************************************************************************
  \brief
	ECS System Interface

	For Systems such as:
	-	ECS Physics System
	-	ECS Render System
	-	And whatever stuff you run in GGEngine::Update(), that pertains to
		the ECS.

  \details
	We'll Inherit from this Interface for any ECS Systems code we
	write, in order to assist the Debug Perfomance Visualizer.

  \par Usage
  \code

	// Inheriting from SystemInterface (EXAMPLE)
	class GGExampleSystem : public SystemInterface
	{
	public:
		void Update(World& world) override;

		// Your implementation...
	private:
		// Other implementation...
	}

  \endcode
*************************************************************************/
class SystemInterface 
{
public:
	virtual bool Init() = 0;

	virtual void Update(World& world) = 0;

protected:
	bool shouldSimulate = false;
	Callback<> onStartSimulate = [&]() { shouldSimulate = true; };
	Callback<> onStopSimulate = [&]() { shouldSimulate = false; };
};