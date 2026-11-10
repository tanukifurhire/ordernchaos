/*************************************************************************
  \file       GGECSAudioSystem.cpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       27 November 2025
  \version    1.0
  \brief
	Defines the GGECSAudioSystem, the ECS audio playback system responsible
	for updating AudioSource components and driving audio through FMOD.

  \details
	Implements per-frame audio behavior for ECS entities, including:
	- Triggering audio playback based on AudioSource component state
	- Handling play-on-awake, one-shot triggers, and runtime playback flags
	- Integrating with GGFMODWrapper for audio routing and control
	- Supporting editor Play / Pause toolbar behavior
	- Stopping ECS-driven audio when exiting Play mode

  \dependencies
	GGECSAudioSystem.hpp, GGEngine.hpp, GGAudioManager.hpp, StopPlayToolbar.hpp

  \history
	- 1.0 (27 November 2025): Initial implementation of ECS audio playback
	  integrated with editor play/pause controls.

  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/
#include <GGECSAudioSystem.hpp>

/*************************************************************************
  \brief
	Initialize ECS audio system
  \return
	True if initialization succeeded
  \details
	Currently a stub setup required for engine audio initialization.
*************************************************************************/
bool GGECSAudioSystem::Init() {
	auto& audioMgr = *(GGEngine::GetEngine().GetAudioSystem());
	
	onPauseCallback =
		[&]()
	{
		audioMgr.PauseAllSounds();
	};

	onResumeCallback =
		[&]()
	{
		audioMgr.ResumeAllSounds();
	};

	onStopCallback =
		[&]()
	{
		audioMgr.StopAllECSSounds();
	};

	GlobalEventBus::onPlayModeStart += onStartSimulate;
	GlobalEventBus::onStopPlayMode += onStopSimulate;

	GlobalEventBus::onStopPlayMode += onStopCallback;

	GlobalEventBus::onPause += onPauseCallback;
	GlobalEventBus::onResume += onResumeCallback;

	return true;
}

/*************************************************************************
  \brief
	Update ECS audio playback for all AudioSource components
  \param world
	ECS world containing AudioSource components
  \details
	- Stops ECS audio when the editor is not in Play mode
	- Pauses or resumes audio depending on toolbar state
	- Handles playOnAwake and justTriggered playback logic
	- Routes active AudioSource components to the FMOD audio manager
*************************************************************************/
void GGECSAudioSystem::Update(World& world) {
	auto& audioMgr = *(GGEngine::GetEngine().GetAudioSystem());

	if (!shouldSimulate) return;

	/// lambda used to process AudioSource components for playback
	std::function<void(Entity, AudioSource&)> playAudio =
		[&](Entity ent, AudioSource& source)
		{

			// if playOnAwake set on true, play the sound once the scene starts playing
			if (source.playOnAwake && !source.hasPlayedOnAwake) {
				source.justTriggered = true;
			}

			if (source.justTriggered) {
				if (source.audioAssetID != 0) {

					std::cout << "[AudioSystem] Playing ID: " << source.audioAssetID << std::endl;

					AudioResult result = audioMgr.PlayAudioSource(source);
					if (result != AudioResult::SUCCESS) {
						std::string entityName = "Unnamed";
						if (world.HasComponent<NameComponent>(ent)) {
							entityName = world.GetComponent<NameComponent>(ent)->GetName();
						}
						std::cerr << "[GGECSAudioSystem] Failed to play AudioSource on Entity " << entityName << "\n";
					}
					if (!source.hasPlayedOnAwake) { source.hasPlayedOnAwake = true; } // play sound, then set hasPlayedOnAwake = true
				}

				source.justTriggered = false;
			}

			if (source.runtimeChannel)
			{
				bool playing = false;

				if (source.runtimeChannel->isPlaying(&playing) != FMOD_OK || !playing)
				{
					source.runtimeChannel = nullptr;
				}
			}

			// apply runtime changes
			if (source.runtimeChannel != nullptr) {
				auto& channel = source.runtimeChannel;

				float finalVolume = source.volume * source.configVolume;
				audioMgr.SetChannelVolume(channel, finalVolume);
			}
		};

	world.ForEach<AudioSource>(playAudio);
}