/*************************************************************************
  \file       GGAudioManager.cpp
  \author     Zafir Haqqym,
              Tan Jia Qi,Trish
  \par        Email: zafir.h@digipen.edu
                     jiaqitrish.tan@digipen.edu
  \contribution
    Zafir Haqqym(70%), Tan Jia Qi, Trish(30%)
  \date       24 September 2025
  \version    2.1
  \brief
    FMOD wrapper (audio manager) with audio control functions.
  \details
    Provides audio functionality with error handling and logging.
  \dependencies
    GGAudioManager.hpp, <fmod_errors.h>, <iostream>, <algorithm>
  \see
    GGAudioManager.hpp, GGAudioAsset.hpp
  \history
    - 1.0 (16 September 2025): Initial singleton implementation.
    - 2.0 (23 September 2025): Converted to object-based approach.
    - 2.1 (24 September 2025): Added audio control functions with error handling.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <GGAudioManager.hpp>
#include <GGECSAssetInterface.hpp>
#include <fmod_errors.h>
#include <iostream>
#include <algorithm>
#include <GGEngine.hpp>

/*************************************************************************
  \brief
    Constructor for FMOD wrapper instance
  \details
    Initializes internal pointers and state flags.
*************************************************************************/
GGFMODWrapper::GGFMODWrapper()
    : fmodSystem(nullptr)
    , isInitialized(false)
{
}

/*************************************************************************
  \brief
    Destructor with automatic cleanup
  \details
    Automatically shuts down FMOD system and releases all resources.
*************************************************************************/
GGFMODWrapper::~GGFMODWrapper()
{
    Shutdown();
}

/*************************************************************************
  \brief
    Initialize FMOD system - implements SystemManagerInterface
  \return
    True if FMOD system successfully initialized, false otherwise.
*************************************************************************/
bool GGFMODWrapper::Init()
{
    // Just call our existing Initialize() method - keeps everything working
    // the same way it did before, but now we also satisfy the system interface
    return Initialize();
}

/*************************************************************************
  \brief
    Initialize FMOD system with default configuration for audio
  \return
    True if initialization is successful, false otherwise.
  \details
    Sets up 32 audio channels, creates the FMOD system, and initializes
    audio buses (Master + sub-buses).
*************************************************************************/
bool GGFMODWrapper::Initialize()
{
    if (isInitialized)
    {
        std::cout << "FMOD Wrapper already initialized" << std::endl;
        return true;
    }

    std::cout << "Initializing FMOD Wrapper for audio..." << std::endl;

    FMOD_RESULT result = FMOD::System_Create(&fmodSystem);
    if (result != FMOD_OK)
    {
        std::cout << "FMOD System creation failed: " << FMOD_ErrorString(result) << std::endl;
        return false;
    }

    // Initialize with 32 channels for audio
    result = fmodSystem->init(32, FMOD_INIT_NORMAL, nullptr);
    if (result != FMOD_OK)
    {
        std::cout << "FMOD init failed: " << FMOD_ErrorString(result) << std::endl;
        fmodSystem->release();
        fmodSystem = nullptr;
        return false;
    }

    InitializeAudioBuses();

    

    isInitialized = true;
    std::cout << "SUCCESS: FMOD Wrapper initialized for  audio with 32 channels" << std::endl;
    return true;
}

/*************************************************************************
  \brief
    Create and initialize all audio buses (master + sub-buses).

  \details
    Retrieves the FMOD master channel group and constructs all predefined
    sub-buses such as Music, SFX, UI, and Ambience. Each created channel
    group is attached under the master bus hierarchy and stored internally
    for later volume/mute control.

  \note
    This function is called automatically during FMOD initialization.
*************************************************************************/
void GGFMODWrapper::InitializeAudioBuses() 
{
    // master
    FMOD::ChannelGroup* masterGroup = nullptr;
    FMOD_RESULT result = fmodSystem->getMasterChannelGroup(&masterGroup);
    if (result != FMOD_OK) {
        std::cout << "[GGFMODWRAPPER] Failed to get master group: " << FMOD_ErrorString(result) << std::endl;
    }
    audioBuses[static_cast<size_t>(AudioBusID::MASTER)] = { AudioBusID::MASTER, masterGroup, 1.f, false };

    // sub-buses
    FMOD::ChannelGroup* musicGroup = nullptr;
    fmodSystem->createChannelGroup("Music", &musicGroup);
    masterGroup->addGroup(musicGroup);
    audioBuses[static_cast<size_t>(AudioBusID::MUSIC)] = { AudioBusID::MUSIC, musicGroup, 1.f, false };

    FMOD::ChannelGroup* sfxGroup = nullptr;
    fmodSystem->createChannelGroup("SFX", &sfxGroup);
    masterGroup->addGroup(sfxGroup);
    audioBuses[static_cast<size_t>(AudioBusID::SFX)] = { AudioBusID::SFX, sfxGroup, 1.f, false };

    FMOD::ChannelGroup* uiGroup = nullptr;
    fmodSystem->createChannelGroup("UI", &uiGroup);
    masterGroup->addGroup(uiGroup);
    audioBuses[static_cast<size_t>(AudioBusID::UI)] = { AudioBusID::UI, uiGroup, 1.f, false };

    FMOD::ChannelGroup* ambienceGroup = nullptr;
    fmodSystem->createChannelGroup("Ambience", &ambienceGroup);
    masterGroup->addGroup(ambienceGroup);
    audioBuses[static_cast<size_t>(AudioBusID::AMBIENCE)] = { AudioBusID::AMBIENCE, ambienceGroup, 1.f, false };
}

/*************************************************************************
  \brief
    Shutdown FMOD system and release all resources
  \details
    Stops all playing sounds, releases all tracked sounds, and
    properly shuts down FMOD system to prevent memory leaks.
*************************************************************************/
void GGFMODWrapper::Shutdown()
{
    if (!isInitialized)
    {
        return;
    }

    std::cout << "FMOD Wrapper shutting down..." << std::endl;

    // Stop all sounds first
    StopAllSounds();

    // Release all tracked sounds
    ReleaseAllSounds();

    if (fmodSystem)
    {
        FMOD_RESULT result = fmodSystem->close();
        if (result != FMOD_OK)
        {
            std::cout << "FMOD close warning: " << FMOD_ErrorString(result) << std::endl;
        }

        result = fmodSystem->release();
        if (result != FMOD_OK)
        {
            std::cout << "FMOD release warning: " << FMOD_ErrorString(result) << std::endl;
        }

        fmodSystem = nullptr;
    }

    isInitialized = false;
    std::cout << "SUCCESS: FMOD Wrapper shutdown complete" << std::endl;
}

/*************************************************************************
  \brief
    Update FMOD system - call once per frame
  \details
    Performs essential FMOD updates such as stream processing and
    channel management. Must be called every frame.
*************************************************************************/
void GGFMODWrapper::Update()
{
    if (isInitialized && fmodSystem)
    {
        FMOD_RESULT result = fmodSystem->update();
        if (result != FMOD_OK)
        {
            std::cout << "FMOD update warning: " << FMOD_ErrorString(result) << std::endl;
        }
    }

    auto& timeMgr = Systems::GetSystemsRegistry().GetSystem<Systems::TimeManager>();

    if (masterFade.active && masterFade.group)
    {
        masterFade.elapsed += (float)timeMgr.GetDeltaTime();

        float t = masterFade.elapsed / masterFade.duration;
        t = std::min(t, 1.0f);

        float v = masterFade.startVolume + (masterFade.targetVolume - masterFade.startVolume) * t;

        masterFade.group->setVolume(v);

        if (t >= 1.0f)
        {
            masterFade.active = false;
        }
    }

    if (isPendingSceneSwitch) {
        sceneTransitionTimer += (float)timeMgr.GetDeltaTime();;

        if (sceneTransitionTimer >= fadeOutDuration) {
            // complete the scene switch
            GGEngine::GetEngine().GetSceneManager().CompleteSceneSwitch(pendingSceneKey);
            auto& scriptSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>();
            scriptSystem.EndSceneTransition();

            FadeMasterIn(static_cast<float>(GGEngineConfig::GGDefaultGameSettings::GetAudioFadeDuration()));

            isPendingSceneSwitch = false;
            pendingSceneKey.clear();
        }

        return;
    }
}

/*************************************************************************
  \brief
    Create FMOD sound object from file path
  \param[in] filepath
    Path to audio file.
  \return
    Pointer to FMOD::Sound on success, nullptr on failure.
*************************************************************************/
FMOD::Sound* GGFMODWrapper::CreateSound(const std::string& filepath)
{
    if (!isInitialized || !fmodSystem)
    {
        std::cout << "ERROR: FMOD Wrapper not initialized, cannot create sound" << std::endl;
        return nullptr;
    }

    FMOD::Sound* sound = nullptr;
    FMOD_RESULT result = fmodSystem->createSound(filepath.c_str(), FMOD_DEFAULT, nullptr, &sound);

    if (result == FMOD_OK && sound)
    {
        activeSounds.push_back(sound);
        std::cout << "FMOD Wrapper: Created sound from " << filepath
            << " (Total sounds: " << activeSounds.size() << ")" << std::endl;
        return sound;
    }
    else
    {
        std::cout << "FMOD Wrapper: Failed to create sound from " << filepath
            << " - " << FMOD_ErrorString(result) << std::endl;
        return nullptr;
    }
}

/*************************************************************************
  \brief
    Release a specific FMOD sound object
  \param[in] sound
    FMOD::Sound pointer to release.
  \details
    Removes the sound from tracking list and releases FMOD resources.
*************************************************************************/
void GGFMODWrapper::ReleaseSound(FMOD::Sound* sound)
{
    if (!sound)
    {
        return;
    }

    auto soundIterator = std::find(activeSounds.begin(), activeSounds.end(), sound);
    if (soundIterator != activeSounds.end())
    {
        activeSounds.erase(soundIterator);
        std::cout << "FMOD Wrapper: Removed sound from tracking (Remaining: "
            << activeSounds.size() << ")" << std::endl;
    }

    FMOD_RESULT result = sound->release();
    if (result != FMOD_OK)
    {
        std::cout << "FMOD Wrapper: Warning during sound release: "
            << FMOD_ErrorString(result) << std::endl;
    }
}

/*************************************************************************
  \brief
    Release all tracked FMOD sound objects
*************************************************************************/
void GGFMODWrapper::ReleaseAllSounds()
{
    std::cout << "FMOD Wrapper: Releasing " << activeSounds.size()
        << " tracked sounds..." << std::endl;

    for (FMOD::Sound* sound : activeSounds)
    {
        if (sound)
        {
            FMOD_RESULT result = sound->release();
            if (result != FMOD_OK)
            {
                std::cout << "FMOD Wrapper: Warning during bulk sound release: "
                    << FMOD_ErrorString(result) << std::endl;
            }
        }
    }

    activeSounds.clear();
    std::cout << "FMOD Wrapper: All tracked sounds released" << std::endl;
}

/*************************************************************************
  \brief
    Play a sound and return its channel for control.
  \param[in] sound
    FMOD::Sound pointer to play.
  \param[out] channel
    Optional FMOD::Channel pointer to receive the playing channel.
  \param[in] paused
    Whether the sound should start in paused state.
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::PlayAudio(FMOD::Sound* sound, FMOD::Channel** channel, bool paused)
{
    if (!isInitialized || !fmodSystem)
    {
        std::cout << "WARNING: Cannot play sound - FMOD not initialized" << std::endl;
        return AudioResult::SYSTEM_NOT_INITIALIZED;
    }

    if (!sound)
    {
        std::cout << "WARNING: Cannot play sound - sound pointer is null" << std::endl;
        return AudioResult::INVALID_SOUND;
    }

    // get Master Group
    FMOD::ChannelGroup* masterGroup = nullptr;
    FMOD_RESULT result = fmodSystem->getMasterChannelGroup(&masterGroup);
    if (result != FMOD_OK || !masterGroup)
    {
        return ConvertFMODResult(result, "PlayAudio - GetMasterChannelGroup");
    }

    // route the sound to the Master Bus
    FMOD::Channel* tempChannel = nullptr;
    result = fmodSystem->playSound(sound, masterGroup, paused, &tempChannel);
    AudioResult audioRes = ConvertFMODResult(result, "PlayAudio - PlaySound");
    if (audioRes != AudioResult::SUCCESS)
    {
        return audioRes;
    }

    if (channel)
    {
        *channel = tempChannel;
    }

    return AudioResult::SUCCESS;
}

/*************************************************************************
  \brief
    Stop a specific audio channel.
  \param[in] channel
    FMOD::Channel pointer to stop.
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::StopChannel(FMOD::Channel* channel)
{
    if (!ValidateChannel(channel, "StopChannel"))
    {
        return AudioResult::INVALID_CHANNEL;
    }

    FMOD_RESULT result = channel->stop();
    return ConvertFMODResult(result, "StopChannel");
}

/*************************************************************************
  \brief
    Stop all currently playing sounds routed through the master bus.
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::StopAllSounds()
{
    if (!isInitialized || !fmodSystem)
    {
        return AudioResult::SYSTEM_NOT_INITIALIZED;
    }

    FMOD::ChannelGroup* masterGroup = nullptr;
    FMOD_RESULT result = fmodSystem->getMasterChannelGroup(&masterGroup);

    if (result != FMOD_OK || !masterGroup)
    {
        return ConvertFMODResult(result, "StopAllSounds - GetMasterChannelGroup");
    }

    result = masterGroup->stop();
    return ConvertFMODResult(result, "StopAllSounds");
}

/*************************************************************************
  \brief
    Stops all audio currently tracked by the ECS audio playback list.

  \details
    Iterates through all stored ECS audio playback entries. Any channel
    that is still playing will be stopped, and the tracking container is
    cleared afterward.

  \return
    AudioResult::SUCCESS always returns success after cleanup.
*************************************************************************/
AudioResult GGFMODWrapper::StopAllECSSounds()
{
    for (auto& audio : currentlyPlaying)
    {
        if (!audio.channel)
            continue;

        bool isPlaying = false;
        audio.channel->isPlaying(&isPlaying);

        if (isPlaying)
        {
            audio.channel->stop();
        }
    }

    currentlyPlaying.clear();
    return AudioResult::SUCCESS;
}

/*************************************************************************
  \brief
    Pauses all sounds routed through the FMOD master bus.

  \details
    Retrieves the master channel group and sets its paused state to true.
    All child channel groups (Music, SFX, UI, Ambience) are paused as well.

  \return
    AudioResult::SYSTEM_NOT_INITIALIZED if FMOD is not running.
    Otherwise, result of the FMOD pause operation.
*************************************************************************/
AudioResult GGFMODWrapper::PauseAllSounds()
{
    if (!isInitialized || !fmodSystem) {
        return AudioResult::SYSTEM_NOT_INITIALIZED;
    }

    FMOD::ChannelGroup* masterGroup = nullptr;
    FMOD_RESULT result = fmodSystem->getMasterChannelGroup(&masterGroup);

    if (result != FMOD_OK || !masterGroup)
    {
        return ConvertFMODResult(result, "PauseAllSounds - GetMasterChannelGroup");
    }

    result = masterGroup->setPaused(true);
    return ConvertFMODResult(result, "PauseAllSounds");
}

/*************************************************************************
  \brief
    Resumes all audio previously paused on the master bus.

  \details
    Retrieves the FMOD master bus and sets its paused state to false,
    restoring playback for all routed channel groups.

  \return
    AudioResult::SYSTEM_NOT_INITIALIZED if FMOD is not running.
    Otherwise, result of the FMOD resume operation.
*************************************************************************/
AudioResult GGFMODWrapper::ResumeAllSounds()
{
    if (!isInitialized || !fmodSystem) {
        return AudioResult::SYSTEM_NOT_INITIALIZED;
    }

    FMOD::ChannelGroup* masterGroup = nullptr;
    FMOD_RESULT result = fmodSystem->getMasterChannelGroup(&masterGroup);
    
    if (result != FMOD_OK || !masterGroup)
    {
        return ConvertFMODResult(result, "ResumeAllSounds - GetMasterChannelGroup");
    }

    result = masterGroup->setPaused(false);
    return ConvertFMODResult(result, "ResumeAllSounds");
}

/*************************************************************************
  \brief
    Pause a specific audio channel.
  \param[in] channel
    FMOD::Channel pointer to pause.
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::PauseChannel(FMOD::Channel* channel)
{
    if (!ValidateChannel(channel, "PauseChannel"))
    {
        return AudioResult::INVALID_CHANNEL;
    }

    FMOD_RESULT result = channel->setPaused(true);
    return ConvertFMODResult(result, "PauseChannel");
}

/*************************************************************************
  \brief
    Resume a paused audio channel.
  \param[in] channel
    FMOD::Channel pointer to resume.
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::ResumeChannel(FMOD::Channel* channel)
{
    if (!ValidateChannel(channel, "ResumeChannel"))
    {
        return AudioResult::INVALID_CHANNEL;
    }

    FMOD_RESULT result = channel->setPaused(false);
    return ConvertFMODResult(result, "ResumeChannel");
}

/*************************************************************************
  \brief
    Set volume for a specific audio channel.
  \param[in] channel
    FMOD::Channel pointer to modify.
  \param[in] volume
    Desired volume level (0.0 to 1.0).
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::SetChannelVolume(FMOD::Channel* channel, float volume)
{
    if (!ValidateChannel(channel, "SetChannelVolume"))
    {
        return AudioResult::INVALID_CHANNEL;
    }

    // Clamp volume to valid range
    volume = std::max(0.0f, std::min(1.0f, volume));

    FMOD_RESULT result = channel->setVolume(volume);
    return ConvertFMODResult(result, "SetChannelVolume");
}

/*************************************************************************
  \brief
    Set pitch for a specific audio channel.
  \param[in] channel
    FMOD::Channel pointer to modify.
  \param[in] pitch
    Desired pitch level (-3.0 to 3.0).
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::SetChannelPitch(FMOD::Channel* channel, float pitch)
{
    if (!ValidateChannel(channel, "SetChannelPitch"))
    {
        return AudioResult::INVALID_CHANNEL;
    }

    // clamp pitch to valid range (0.5 is one octave down, 2.0 is one octave up)
    pitch = std::max(0.5f, std::min(2.0f, pitch));

    FMOD_RESULT result = channel->setPitch(pitch);
    return ConvertFMODResult(result, "SetChannelPitch");
}

/*************************************************************************
  \brief
    Set stereo pan for a specific audio channel.
  \param[in] channel
    FMOD::Channel pointer to modify.
  \param[in] pan
    Desired pan (-1.0 = left, 1.0 = right).
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::SetChannelPan(FMOD::Channel* channel, float pan)
{
    if (!ValidateChannel(channel, "SetChannelPan"))
    {
        return AudioResult::INVALID_CHANNEL;
    }

    // clamp pan to valid range
    pan = std::max(-1.f, std::min(1.f, pan));

    //channel->setMode(FMOD_2D); // because fmod's mode is 3D by default
    FMOD_RESULT result = channel->setPan(pan);
    return ConvertFMODResult(result, "SetChannelPan");
}

/*************************************************************************
  \brief
    Mute or unmute a specific audio channel.
  \param[in] channel
    FMOD::Channel pointer to modify.
  \param[in] mute
    True to mute, false to unmute.
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::SetChannelMute(FMOD::Channel* channel, bool mute)
{
    if (!ValidateChannel(channel, "SetChannelMute"))
    {
        return AudioResult::INVALID_CHANNEL;
    }

    FMOD_RESULT result = channel->setMute(mute);
    return ConvertFMODResult(result, "SetChannelMute");
}

/*************************************************************************
  \brief
    Enable or disable looping on a specific channel.
  \param[in] channel
    FMOD::Channel pointer to modify.
  \param[in] loop
    True to enable looping, false to disable.
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::SetChannelLoop(FMOD::Channel* channel, bool loop)
{
    if (!ValidateChannel(channel, "SetChannelLoop"))
    {
        return AudioResult::INVALID_CHANNEL;
    }

    FMOD_RESULT result = channel->setMode(loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
    return ConvertFMODResult(result, "SetChannelLoop");
}

/*************************************************************************
  \brief
    Set master volume for all audio.
  \param[in] volume
    Desired volume level (0.0 to 1.0).
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::SetMasterVolume(float volume)
{
    if (!isInitialized || !fmodSystem)
    {
        return AudioResult::SYSTEM_NOT_INITIALIZED;
    }

    // Clamp volume to valid range
    volume = std::max(0.0f, std::min(1.0f, volume));

    // Store the volume for UI display
    currentMasterVolume = volume;

    FMOD::ChannelGroup* masterGroup = nullptr;
    FMOD_RESULT result = fmodSystem->getMasterChannelGroup(&masterGroup);

    if (result != FMOD_OK || !masterGroup)
    {
        return ConvertFMODResult(result, "SetMasterVolume - GetMasterChannelGroup");
    }

    result = masterGroup->setVolume(volume);
    return ConvertFMODResult(result, "SetMasterVolume");
}

/*************************************************************************
  \brief
    Get the current volume of a specific audio channel.
  \param[in] channel
    FMOD::Channel pointer to query.
  \param[out] volume
    Output pointer to receive current volume (0.0 to 1.0).
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::GetChannelVolume(FMOD::Channel* channel, float* volume)
{
    if (!volume)
    {
        std::cout << "WARNING: GetChannelVolume - volume output pointer is null" << std::endl;
        return AudioResult::NULL_PARAMETER;
    }

    if (!ValidateChannel(channel, "GetChannelVolume"))
    {
        return AudioResult::INVALID_CHANNEL;
    }

    FMOD_RESULT result = channel->getVolume(volume);
    return ConvertFMODResult(result, "GetChannelVolume");
}

/*************************************************************************
  \brief
    Check if a channel is currently playing.
  \param[in] channel
    FMOD::Channel pointer to query.
  \param[out] isPlaying
    Output pointer to receive playback state (true if playing).
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::IsChannelPlaying(FMOD::Channel* channel, bool* isPlaying)
{
    if (!isPlaying)
    {
        std::cout << "WARNING: IsChannelPlaying - isPlaying output pointer is null" << std::endl;
        return AudioResult::NULL_PARAMETER;
    }

    if (!ValidateChannel(channel, "IsChannelPlaying"))
    {
        *isPlaying = false;
        return AudioResult::INVALID_CHANNEL;
    }

    FMOD_RESULT result = channel->isPlaying(isPlaying);
    return ConvertFMODResult(result, "IsChannelPlaying");
}

/*************************************************************************
  \brief
    Set playback position of a specific channel (for looping or seeking).
  \param[in] channel
    FMOD::Channel pointer to modify.
  \param[in] position
    Playback position in milliseconds.
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::SetChannelPosition(FMOD::Channel* channel, unsigned int position)
{
    if (!ValidateChannel(channel, "SetChannelPosition"))
    {
        return AudioResult::INVALID_CHANNEL;
    }

    FMOD_RESULT result = channel->setPosition(position, FMOD_TIMEUNIT_MS);
    return ConvertFMODResult(result, "SetChannelPosition");
}

/*************************************************************************
  \brief
    Get current playback position of a specific channel.
  \param[in] channel
    FMOD::Channel pointer to query.
  \param[out] position
    Output pointer to receive current playback position in milliseconds.
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::GetChannelPosition(FMOD::Channel* channel, unsigned int* position)
{
    if (!position)
    {
        std::cout << "WARNING: GetChannelPosition - position output pointer is null" << std::endl;
        return AudioResult::NULL_PARAMETER;
    }

    if (!ValidateChannel(channel, "GetChannelPosition"))
    {
        return AudioResult::INVALID_CHANNEL;
    }

    FMOD_RESULT result = channel->getPosition(position, FMOD_TIMEUNIT_MS);
    return ConvertFMODResult(result, "GetChannelPosition");
}

/*************************************************************************
  \brief
    Convert AudioResult enum to human-readable string.
  \param[in] result
    AudioResult code to convert.
  \return
    Corresponding string describing the result.
*************************************************************************/
std::string GGFMODWrapper::GetResultString(AudioResult result)
{
    switch (result)
    {
    case AudioResult::SUCCESS:
        return "SUCCESS";
    case AudioResult::INVALID_SOUND:
        return "INVALID_SOUND - Sound pointer is null or invalid";
    case AudioResult::INVALID_CHANNEL:
        return "INVALID_CHANNEL - Channel pointer is null or invalid";
    case AudioResult::SYSTEM_NOT_INITIALIZED:
        return "SYSTEM_NOT_INITIALIZED - FMOD system not initialized";
    case AudioResult::FMOD_ERROR:
        return "FMOD_ERROR - Internal FMOD error occurred";
    case AudioResult::NULL_PARAMETER:
        return "NULL_PARAMETER - Required parameter is null";
    default:
        return "UNKNOWN_ERROR";
    }
}

/*************************************************************************
  \brief
    Convert FMOD_RESULT to AudioResult with logging.
  \param[in] fmodResult
    FMOD_RESULT code from FMOD API.
  \param[in] operation
    Operation name for logging context.
  \return
    Corresponding AudioResult enum.
*************************************************************************/
AudioResult GGFMODWrapper::ConvertFMODResult(FMOD_RESULT fmodResult, const std::string& operation)
{
    if (fmodResult == FMOD_OK)
    {
        return AudioResult::SUCCESS;
    }

    // Log the FMOD error
    std::cout << "WARNING: " << operation << " failed - " << FMOD_ErrorString(fmodResult) << std::endl;

    // Convert to our error codes
    switch (fmodResult)
    {
    case FMOD_ERR_INVALID_HANDLE:
    case FMOD_ERR_CHANNEL_STOLEN:
        return AudioResult::INVALID_CHANNEL;
    case FMOD_ERR_UNINITIALIZED:
        return AudioResult::SYSTEM_NOT_INITIALIZED;
    default:
        return AudioResult::FMOD_ERROR;
    }
}

/*************************************************************************
  \brief
    Validate a channel pointer and log warnings if invalid.
  \param[in] channel
    FMOD::Channel pointer to validate.
  \param[in] operation
    Operation name for logging.
  \return
    True if channel is valid, false otherwise.
*************************************************************************/
bool GGFMODWrapper::ValidateChannel(FMOD::Channel* channel, const std::string& operation)
{
    if (!channel)
    {
        std::cout << "WARNING: " << operation << " - channel pointer is null" << std::endl;
        return false;
    }

    // Additional check: verify channel is still valid with FMOD
    bool isPlaying = false;
    FMOD_RESULT result = channel->isPlaying(&isPlaying);

    if (result == FMOD_ERR_INVALID_HANDLE || result == FMOD_ERR_CHANNEL_STOLEN)
    {
        std::cout << "WARNING: " << operation << " - channel is no longer valid" << std::endl;
        return false;
    }

    return true;
}

/*************************************************************************
  \brief
    Test playback of a cached audio asset.
  \param[in] audioAsset
    GGAudioAsset reference to test.
*************************************************************************/
void GGFMODWrapper::TestAudioPlayback(const GGAudioAsset& audioAsset)
{
    std::cout << "\n=== Testing Audio Playback: " << audioAsset.GetFilePath() << " ===" << std::endl;

    if (!IsInitialized())
    {
        std::cout << "ERROR: Audio system not initialized" << std::endl;
        return;
    }

    if (!audioAsset.IsValid())
    {
        std::cout << "ERROR: Invalid audio asset" << std::endl;
        return;
    }

    FMOD::Channel* channel = nullptr;
    AudioResult result = PlayAudio(audioAsset.GetFMODSound(), &channel); // Use existing sound

    if (result == AudioResult::SUCCESS)
    {
        std::cout << "SUCCESS: Playing " << audioAsset.GetFilePath() << std::endl;
        SetChannelVolume(channel, 0.7f);
        std::cout << "Set volume to 70%" << std::endl;
    }
    else
    {
        std::cout << "ERROR: Failed to play sound - " << GetResultString(result) << std::endl;
    }

    std::cout << "=== Test Complete ===" << std::endl;
}

/*************************************************************************
  \brief
    Get a list of currently playing audio channels.

  \details
    Cleans up finished channels before returning the currently playing list.
    Channels that have stopped or failed FMOD checks are removed automatically.

  \return
    std::vector of PlayingAudio structs representing active audio channels.
*************************************************************************/
std::vector<GGFMODWrapper::PlayingAudio> GGFMODWrapper::GetCurrentlyPlayingAudio() const
{
    // Clean up finished audio entries
    auto it = currentlyPlaying.begin();
    while (it != currentlyPlaying.end())
    {
        bool isPlaying = false;
        FMOD_RESULT result = it->channel->isPlaying(&isPlaying);

        if (result != FMOD_OK || !isPlaying)
        {
            it = currentlyPlaying.erase(it);
        }
        else
        {
            ++it;
        }
    }

    return currentlyPlaying;
}

/*************************************************************************
  \brief
    Get a list of currently previewing audio channels (editor-only).

  \details
    Cleans up finished preview channels before returning the list.
    Used for temporary playback previews in editor mode.

  \return
    std::vector of PlayingAudio structs representing active preview audio.
*************************************************************************/
std::vector<GGFMODWrapper::PlayingAudio> GGFMODWrapper::GetCurrentlyPreviewingAudio() const
{
    // clean up finished audio entries
    auto it = currentlyPreviewing.begin();
    while (it != currentlyPreviewing.end())
    {
        bool isPlaying = false;
        FMOD_RESULT result = it->channel->isPlaying(&isPlaying);

        if (result != FMOD_OK || !isPlaying)
        {
            it = currentlyPreviewing.erase(it);
        }
        else
        {
            ++it;
        }
    }

    return currentlyPreviewing;
}

/*************************************************************************
  \brief
    Play an audio asset by path and track it for UI display.
  \param[in] assetPath
    File path of the audio asset.
  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::PlayAudioByPath(const std::string& assetPath)
{
    if (!isInitialized || !fmodSystem)
    {
        return AudioResult::SYSTEM_NOT_INITIALIZED;
    }

    // Get audio asset from engine
    try
    {
        extern GGEngine& GetEngineInstance();
        const GGAudioAsset& audioAsset = GetEngineInstance().GetAssetManager()->GetAudioAsset(assetPath);
        
        FMOD::Channel* channel = nullptr;
        AudioResult result = PlayAudio(audioAsset.GetFMODSound(), &channel);

        if (result == AudioResult::SUCCESS && channel)
        {
            // Track this playing audio
            currentlyPlaying.emplace_back(assetPath, channel);
        }

        return result;
    }
    catch (const std::exception& e)
    {
        std::cout << "ERROR: Failed to play audio by path: " << assetPath << " - " << e.what() << std::endl;
        return AudioResult::INVALID_SOUND;
    }
}

/*************************************************************************
  \brief
    Get the current master volume level for all audio.

  \return
    Float value representing the master volume (0.0�1.0).
*************************************************************************/
float GGFMODWrapper::GetMasterVolume() const
{
    return currentMasterVolume;
}

/*************************************************************************
  \brief
    Retrieve the FMOD channel group for a specific audio bus.

  \param[in] busID
    Enum identifying the audio bus (MASTER, MUSIC, SFX, UI, AMBIENCE).

  \return
    Pointer to the FMOD::ChannelGroup associated with the bus.
*************************************************************************/
FMOD::ChannelGroup* GGFMODWrapper::GetChannelGroupFromBus(AudioBusID busID) 
{
    return audioBuses[static_cast<size_t>(busID)].channelGroup;
}

/*************************************************************************
  \brief
    Play an AudioSource and apply all its properties.

  \details
    Retrieves the audio asset, plays it, routes it to the correct bus,
    applies volume, pitch, pan, mute, looping, and updates playback state.

  \param[in,out] source
    AudioSource struct containing playback settings and state.

  \return
    AudioResult indicating success or failure of playback.
*************************************************************************/
AudioResult GGFMODWrapper::PlayAudioSource(AudioSource& source)
{
    if (!IsInitialized() || !fmodSystem) {
        return AudioResult::SYSTEM_NOT_INITIALIZED;
    }

    if (source.audioAssetID == 0) {
        return AudioResult::INVALID_SOUND;
    }

    // check if audio asset ID has been registered
    if (!GGECSStandardAssetManager<GGAudioAsset>::HasAsset(source.audioAssetID)) {
        std::cout << "[GGAudioManager] Audio asset ID not registered: " << source.audioAssetID << "\n";
        return AudioResult::INVALID_SOUND;
    }

    // get registered asset
    const GGAudioAsset& asset = GGECSStandardAssetManager<GGAudioAsset>::GetAsset(source.audioAssetID);

    FMOD::Sound* sound = asset.GetFMODSound();

    // create sound on demand in Release Game
    if (!sound) {
        const std::string& path = asset.GetFilePath();

        if (path.empty()) {
            std::cout << "[GGAudioManager] Audio asset has no file path (ID: " << source.audioAssetID << ")\n";
            return AudioResult::INVALID_SOUND;
        }

        sound = CreateSound(path);
        if (!sound) {
            std::cout << "[GGAudioManager] Failed to create FMOD sound: " << path << "\n";
            return AudioResult::INVALID_SOUND;
        }
    }

    // play the sound
    FMOD::Channel* channel = nullptr;
    AudioResult result = PlayAudio(asset.GetFMODSound(), &channel, false);
    if (result != AudioResult::SUCCESS) {
        return result;
    }

    source.runtimeChannel = channel;

    // adds tracking for stopping later
    currentlyPlaying.emplace_back(asset.GetFilePath(), channel);
    currentlyPlaying.back().sourcePtr = &source;

    // route to the correct AudioBus
    FMOD::ChannelGroup* busGroup = GetChannelGroupFromBus(static_cast<AudioBusID>(source.outputBus));
    if (busGroup) {
        channel->setChannelGroup(busGroup);
    }

    const AudioBus& bus = audioBuses[static_cast<size_t>(source.outputBus)];

    // apply audio properties
    float finalVolume = source.volume * bus.volume * source.configVolume;
    SetChannelVolume(channel, finalVolume);
    SetChannelPitch(channel, source.pitch);
    SetChannelPan(channel, source.stereoPan);
    SetChannelMute(channel, source.isMuted);
    SetChannelLoop(channel, source.isLooping);

    // playback states
    source.isPlaying = true;
    source.isPaused = false;
    source.justTriggered = true;
    unsigned int pos = 0;
    GetChannelPosition(channel, &pos);
    source.playbackCursor = pos;

    return AudioResult::SUCCESS;
}

/*************************************************************************
  \brief
    Update playback cursors for all currently playing audio sources.

  \details
    Iterates over currentlyPlaying and updates each source's playbackCursor
    with the current FMOD channel position.
*************************************************************************/
void GGFMODWrapper::UpdatePlaybackCursors() 
{
    for (auto& audio : currentlyPlaying) {
        if (audio.channel) {
            bool isPlaying = false;
            if (IsChannelPlaying(audio.channel, &isPlaying) == AudioResult::SUCCESS && isPlaying) {
                unsigned int pos = 0;
                GetChannelPosition(audio.channel, &pos);

                if (audio.sourcePtr) {
                    audio.sourcePtr->playbackCursor = pos;
                }
            }
        }
    }
}

/*************************************************************************
  \brief
    Get the playback cursor position of a given AudioSource.

  \param[in] source
    AudioSource to query.

  \return
    Playback position in milliseconds.
*************************************************************************/
uint64_t GGFMODWrapper::GetPlaybackCursor(const AudioSource& source) const {
    return source.playbackCursor;
}

/*************************************************************************
  \brief
    Play an audio asset as an editor preview (separate from ECS audio).

  \param[in] assetPath
    File path of the audio asset to preview.

  \return
    AudioResult indicating success or failure.
*************************************************************************/
AudioResult GGFMODWrapper::PlayEditorAudioPreview(const std::string& assetPath)
{
    try
    {
        extern GGEngine& GetEngineInstance();
        const GGAudioAsset& asset = GetEngineInstance().GetAssetManager()->GetAudioAsset(assetPath);

        FMOD::Channel* channel = nullptr;
        AudioResult result = PlayAudio(asset.GetFMODSound(), &channel, false);

        if (result == AudioResult::SUCCESS && channel)
        {
            currentlyPreviewing.emplace_back(assetPath, channel);
        }

        return result;
    }
    catch (const std::exception& e)
    {
        std::cout << "EDITOR PREVIEW ERROR: " << e.what() << std::endl;
        return AudioResult::INVALID_SOUND;
    }
}

/*************************************************************************
  \brief
    Fade the master audio bus out over time.

  \param[in] seconds
    Duration of the fade-out in seconds.

  \details
    Gradually interpolates the master channel group's volume from the
    current master volume down to silence over the specified duration.
    The fade is processed incrementally during Update() and does not
    block execution.

    This function is commonly used for scene transitions or global
    audio shutdown effects.
*************************************************************************/
void GGFMODWrapper::FadeMasterOut(float seconds) 
{
    AudioBus& master = audioBuses[(size_t)AudioBusID::MASTER];

    if (!master.channelGroup) {
        return;
    }

    masterFade.group = master.channelGroup;
    masterFade.startVolume = currentMasterVolume;
    masterFade.targetVolume = 0.0f;
    masterFade.duration = std::max(seconds, 0.001f);
    masterFade.elapsed = 0.0f;
    masterFade.active = true;
}

/*************************************************************************
  \brief
    Fade the master audio bus in over time.

  \param[in] seconds
    Duration of the fade-in in seconds.

  \details
    Gradually interpolates the master channel group's volume from silence
    back to the stored master volume over the specified duration.
    The fade is processed incrementally during Update().

    Typically called after a scene transition completes.
*************************************************************************/
void GGFMODWrapper::FadeMasterIn(float seconds)
{
    AudioBus& master = audioBuses[(size_t)AudioBusID::MASTER];
    if (!master.channelGroup) return;

    masterFade.group = master.channelGroup;
    masterFade.startVolume = 0.0f;
    masterFade.targetVolume = currentMasterVolume;
    masterFade.duration = std::max(seconds, 0.001f);
    masterFade.elapsed = 0.0f;
    masterFade.active = true;
}

/*************************************************************************
  \brief
    Check whether the master audio bus is currently fading.

  \return
    True if a master fade-in or fade-out operation is active.

  \details
    This function reflects the internal fade state managed by the
    BusFade structure and is useful for coordinating scene transitions
    or preventing overlapping fade operations.
*************************************************************************/
bool GGFMODWrapper::IsMasterFading() const
{
    return masterFade.active;
}

/*************************************************************************
  \brief
    Begin a scene transition with audio fade-out.

  \param[in] sceneKey
    Key of the scene to switch to after the fade-out completes.
  \param[in] fadeDuration
    Duration of the master audio fade-out in seconds.

  \details
    Initiates a coordinated scene transition by:
    1. Notifying the ECS script system that a transition has begun
    2. Fading out the master audio bus over the specified duration
    3. Deferring the actual scene switch until the fade-out completes

    If the master volume is already zero, the scene switch occurs
    immediately without performing a fade.

    The transition is finalized during Update() once the fade
    duration has elapsed.
*************************************************************************/
void GGFMODWrapper::StartSceneTransition(std::string& sceneKey, float fadeDuration) 
{
    if (isPendingSceneSwitch) {
        return;
    }

    auto& scriptSystem = Systems::GetECSSystemsRegistry().GetSystem<Systems::ECSScript>();
    scriptSystem.BeginSceneTransition();

    if (GetMasterVolume() <= 0.0f) {
        // volume is 0, skip the fade and switch immediately
        GGEngine::GetEngine().GetSceneManager().CompleteSceneSwitch(sceneKey);
        return;
    }
    
    FadeMasterOut(fadeDuration);
    isPendingSceneSwitch = true;
    pendingSceneKey = sceneKey;
    sceneTransitionTimer = 0.0f;
    fadeOutDuration = fadeDuration;
}