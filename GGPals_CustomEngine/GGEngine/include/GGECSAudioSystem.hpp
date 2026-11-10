/*************************************************************************
  \file       GGECSAudioSystem.hpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       27 November 2025
  \version    1.0
  \brief
	Declares the GGECSAudioSystem, the ECS audio update system responsible
	for integrating AudioSource components with the FMOD audio backend.

  \details
	Provides functionality to:
	- Update AudioSource components each frame during Play mode.
	- Trigger audio playback via GGFMODWrapper using ECS component data.
	- Support play-on-awake behavior and one-shot sound triggering.
	- Handle global pause and resume states via the editor toolbar.
	- Stop ECS-driven audio when exiting Play mode.

  \dependencies
	GGSystemInterface.hpp, GGEngine.hpp, GGAudioManager.hpp

  \history
	- 1.0 (27 November 2025): Initial version providing ECS audio playback
	  and integration with editor Play/Pause controls.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGSystemInterface.hpp>
#include <GGEngine.hpp>
#include <GGAudioManager.hpp>

/*************************************************************************
  \class GGECSAudioSystem
  \brief
	ECS audio playback system that updates AudioSource components and
	interfaces with the GGFMODWrapper.
  \details
	- Integrates ECS audio data with the FMOD audio backend
	- Supports pause/resume state for editor toolbar controls
	- Called by the engine's system manager each frame
*************************************************************************/
class GGECSAudioSystem : public SystemInterface {
public:
	/*************************************************************************
	  \brief
		Initialize the ECS audio system
	  \return
		True on successful initialization
	  \details
		Loads the audio subsystem from GGEngine and prepares audio state.
	*************************************************************************/
	bool Init() override;

	/*************************************************************************
	  \brief
		Update all ECS AudioSource components
	  \param world
		Reference to ECS world containing AudioSource components
	  \details
		Applies playback, pause, resume, and cursor updates per frame.
	*************************************************************************/
	void Update(World& world) override;

	/*************************************************************************
	  \brief
		Pause ECS-driven audio playback
	  \details
		Used by editor toolbar controls (StopPlayToolbar.cpp)
	*************************************************************************/
	void Pause() { isPaused = true; }

	/*************************************************************************
	  \brief
		Resume ECS-driven audio playback
	  \details
		Restores audio updates after a paused state
	*************************************************************************/
	void Resume() { isPaused = false; }
	
	/*************************************************************************
	  \brief
		Check whether the ECS audio system is currently paused
	  \return
		True if audio updates are paused
	*************************************************************************/
	bool IsPaused() const { return isPaused; } // return pause status

private:
	/*************************************************************************
	  \brief
		Boolean for paused state
	*************************************************************************/
	bool isPaused = false;

	Callback<> onPauseCallback;
	Callback<> onResumeCallback;

	Callback<> onStopCallback;
};