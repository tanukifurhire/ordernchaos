/*************************************************************************
  \file       GGAudioManager.hpp
  \author     Zafir Haqqym,
              Tan Jia Qi, Trish
  \par        Email: zafir.h@digipen.edu
                     jiaqitrish.tan@digipen.edu
  \contribution
    Zafir Haqqym(70%), Tan Jia Qi, Trish(30%)
  \date       24 September 2025
  \version    2.1
  \brief
    FMOD wrapper (audio manager) with audio control functions.
  \details
    Provides FMOD functionality through object instances with full
    audio playback control.
  \dependencies
    <fmod.hpp>, <fmod_errors.h>, <string>, <vector>
  \see
    GGAudioAsset, GGAssetManager
  \history
    - 1.0 (16 September 2025): Initial singleton version.
    - 2.0 (23 September 2025): Converted to object-based approach.
    - 2.1 (24 September 2025): Added audio control functions.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <fmod.hpp>
#include <string>
#include <vector>
#include <memory>
#include <GGAudioAsset.hpp>
#include <GGSystemInterface.hpp>
#include <chrono>
#include <GGECSComponents.hpp>

/*************************************************************************
  \brief
    Audio operation result codes for error handling
*************************************************************************/
enum class AudioResult : int
{
    SUCCESS = 0,
    INVALID_SOUND = 1,
    INVALID_CHANNEL = 2,
    SYSTEM_NOT_INITIALIZED = 3,
    FMOD_ERROR = 4,
    NULL_PARAMETER = 5
};

/*************************************************************************
  \enum AudioBusID
  \brief Identifiers for built-in FMOD audio buses (Master, Music, SFX, etc.)
*************************************************************************/
enum class AudioBusID : uint8_t
{
    MASTER = 0,
    MUSIC = 1,
    SFX = 2,
    UI = 3,
    AMBIENCE = 4,

    MAX_COUNT = 5
};

/*************************************************************************
  \struct AudioBus
  \brief Container representing a logical audio bus with volume and mute state
*************************************************************************/
struct AudioBus
{
    AudioBusID id{ 0 };
    FMOD::ChannelGroup* channelGroup{ nullptr };
    float volume{ 1.f };
    bool isMuted{ false };
};

/*************************************************************************
  \brief
    Enhanced FMOD wrapper with complete 2D audio control
  \par Usage
  \code
    // Create and initialize audio system
    auto audioSystem = std::make_unique<GGFMODWrapper>();
    audioSystem->Initialize();

    // Load and play sound
    FMOD::Sound* sound = audioSystem->CreateSound("jump.wav");
    FMOD::Channel* channel = nullptr;
    AudioResult result = audioSystem->PlayAudio(sound, &channel);

    if (result == AudioResult::SUCCESS)
    {
        audioSystem->SetChannelVolume(channel, 0.5f);
    }

    // Cleanup
    audioSystem->Update();  // Call per frame
  \endcode
*************************************************************************/
class GGFMODWrapper : public SystemManagerInterface
{
public:
    /*************************************************************************
      \brief
        Constructor for FMOD wrapper instance
    *************************************************************************/
    GGFMODWrapper();

    /*************************************************************************
      \brief
        Destructor with automatic cleanup
    *************************************************************************/
    ~GGFMODWrapper();

    /*************************************************************************
      \brief
        Initialize FMOD system with default configuration for 2D audio
      \return
        True if FMOD system initialized successfully
      \details
        This is the original FMOD initialization method. Sets up the FMOD
        system with 32 channels for 2D game audio. Called by Init() method
        to maintain compatibility with both direct usage and system interface.
    *************************************************************************/
    bool Initialize();

    /*************************************************************************
      \brief Initialize built-in audio buses (Master, Music, SFX, UI, Ambience)
    *************************************************************************/
    void InitializeAudioBuses();

    /*************************************************************************
      \brief
        Initialize FMOD system - implements SystemManagerInterface
      \return
        True if FMOD system initialized successfully
      \details
        This method satisfies the SystemManagerInterface::Init() requirement
        by delegating to the existing Initialize() method. This allows the
        FMOD wrapper to work with the unified system management architecture
        while keeping the original API intact for backward compatibility.
    *************************************************************************/
    bool Init() override;

    /*************************************************************************
      \brief
        Shutdown FMOD system and release all resources
      \details
        Stops all playing sounds, releases all tracked sound objects, and
        properly shuts down the FMOD system. Called automatically by the
        destructor to ensure clean resource cleanup.
    *************************************************************************/
    void Shutdown();

    /*************************************************************************
      \brief
        Update FMOD system - serves both FMOD needs and SystemManagerInterface
      \details
        This method serves a dual purpose:
        1. Performs essential FMOD housekeeping (stream processing, channel management)
        2. Implements SystemManagerInterface::Update() for unified system management

        Unlike AssetManager's stub Update(), this does real work that FMOD requires
        every frame. Must be called once per frame for proper audio functionality.
        The 'override' keyword indicates this also satisfies the system interface.
    *************************************************************************/
    void Update() override;

    /*************************************************************************
      \brief
        Create FMOD sound object from file path
      \param[in] filepath
        Complete path to audio file
      \return
        Pointer to FMOD sound object, or nullptr if creation failed
    *************************************************************************/
    FMOD::Sound* CreateSound(const std::string& filepath);

    /*************************************************************************
      \brief
        Release a specific FMOD sound object
      \param[in] sound
        Pointer to FMOD sound to release
    *************************************************************************/
    void ReleaseSound(FMOD::Sound* sound);

    /*************************************************************************
      \brief
        Release all tracked FMOD sound objects
    *************************************************************************/
    void ReleaseAllSounds();

    /*************************************************************************
      \brief
        Play sound and return channel for control
      \param[in] sound
        FMOD sound object to play
      \param[out] channel
        Pointer to receive the channel handle (optional, can be nullptr)
      \param[in] paused
        Start sound paused if true
      \return
        AudioResult indicating success or failure
      \par Usage
      \code
        FMOD::Channel* channel = nullptr;
        AudioResult result = audioSystem->PlayAudio(jumpSound, &channel);
        if (result == AudioResult::SUCCESS)
        {
            // Sound is playing, channel is valid
        }
      \endcode
    *************************************************************************/
    AudioResult PlayAudio(FMOD::Sound* sound, FMOD::Channel** channel = nullptr, bool paused = false);

    /*************************************************************************
      \brief
        Stop a specific audio channel
      \param[in] channel
        Channel to stop
      \return
        AudioResult indicating success or failure
      \par Usage
      \code
        audioSystem->StopChannel(channel);
      \endcode
    *************************************************************************/
    AudioResult StopChannel(FMOD::Channel* channel);

    /*************************************************************************
      \brief
        Stop all currently playing sounds
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult StopAllSounds();

    /*************************************************************************
      \brief
        Stop all currently playing ECS-managed audio channels
      \details
        Iterates through all tracked ECS audio channels and stops playback.
        This does not affect editor preview audio.
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult StopAllECSSounds();

    /*************************************************************************
      \brief
        Pause all currently playing audio channels (ECS + preview)
      \details
        Pauses every active channel tracked by the FMOD wrapper. Useful
        for global pause functionality like game pause.
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult PauseAllSounds();

    /*************************************************************************
      \brief
        Resume all previously paused audio channels (ECS + preview)
      \details
        Resumes playback for every channel paused by PauseAllSounds().
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult ResumeAllSounds();

    /*************************************************************************
      \brief
        Pause a specific audio channel
      \param[in] channel
        Channel to pause
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult PauseChannel(FMOD::Channel* channel);

    /*************************************************************************
      \brief
        Resume a paused audio channel
      \param[in] channel
        Channel to resume
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult ResumeChannel(FMOD::Channel* channel);

    /*************************************************************************
      \brief
        Set volume for a specific channel
      \param[in] channel
        Channel to adjust volume for
      \param[in] volume
        Volume level (0.0 to 1.0)
      \return
        AudioResult indicating success or failure
      \par Usage
      \code
        // Set to half volume
        audioSystem->SetChannelVolume(channel, 0.5f);
      \endcode
    *************************************************************************/
    AudioResult SetChannelVolume(FMOD::Channel* channel, float volume);

    /*************************************************************************
      \brief
        Set pitch (playback speed) for a specific audio channel
      \param[in] channel
        FMOD channel to modify
      \param[in] pitch
        Pitch multiplier
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult SetChannelPitch(FMOD::Channel* channel, float pitch);

    /*************************************************************************
      \brief
        Set stereo panning for a specific audio channel
      \param[in] channel
        FMOD channel to modify
      \param[in] pan
        Pan value
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult SetChannelPan(FMOD::Channel* channel, float pan);

    /*************************************************************************
      \brief
        Mute or unmute a specific audio channel
      \param[in] channel
        FMOD channel to modify
      \param[in] mute
        True to mute, false to unmute
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult SetChannelMute(FMOD::Channel* channel, bool mute);

    /*************************************************************************
      \brief
        Enable or disable looping for a specific audio channel
      \param[in] channel
        FMOD channel to modify
      \param[in] loop
        True to loop the sound, false to play once
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult SetChannelLoop(FMOD::Channel* channel, bool loop);

    /*************************************************************************
      \brief
        Set master volume for all audio
      \param[in] volume
        Master volume level (0.0 to 1.0)
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult SetMasterVolume(float volume);

    /*************************************************************************
      \brief
        Get current volume for a specific channel
      \param[in] channel
        Channel to query
      \param[out] volume
        Current volume level
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult GetChannelVolume(FMOD::Channel* channel, float* volume);

    /*************************************************************************
      \brief
        Check if a channel is currently playing
      \param[in] channel
        Channel to check
      \param[out] isPlaying
        True if channel is playing
      \return
        AudioResult indicating success or failure
      \par Usage
      \code
        bool playing = false;
        if (audioSystem->IsChannelPlaying(channel, &playing) == AudioResult::SUCCESS)
        {
            if (playing)
            {
                std::cout << "Sound is still playing" << std::endl;
            }
        }
      \endcode
    *************************************************************************/
    AudioResult IsChannelPlaying(FMOD::Channel* channel, bool* isPlaying);

    /*************************************************************************
      \brief
        Set channel position for looping sounds or seeking
      \param[in] channel
        Channel to set position for
      \param[in] position
        Position in milliseconds
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult SetChannelPosition(FMOD::Channel* channel, unsigned int position);

    /*************************************************************************
      \brief
        Get current playback position of channel
      \param[in] channel
        Channel to query
      \param[out] position
        Current position in milliseconds
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult GetChannelPosition(FMOD::Channel* channel, unsigned int* position);

    /*************************************************************************
      \brief
        Get direct access to FMOD system for advanced operations
      \return
        Pointer to FMOD system instance, or nullptr if not initialized
    *************************************************************************/
    FMOD::System* GetSystem() const { return fmodSystem; }

    /*************************************************************************
      \brief
        Check if FMOD wrapper is properly initialized
      \return
        True if FMOD system is ready for use
    *************************************************************************/
    bool IsInitialized() const { return isInitialized; }

    /*************************************************************************
      \brief
        Get count of currently tracked sound objects
      \return
        Number of active sound objects being managed
    *************************************************************************/
    size_t GetActiveSoundCount() const { return activeSounds.size(); }

    /*************************************************************************
      \brief
        Convert AudioResult to human-readable string
      \param[in] result
        AudioResult to convert
      \return
        String description of the result
      \par Usage
      \code
        AudioResult result = audioSystem->PlayAudio(sound, &channel);
        if (result != AudioResult::SUCCESS)
        {
            std::cout << "Audio error: " << audioSystem->GetResultString(result) << std::endl;
        }
      \endcode
    *************************************************************************/
    static std::string GetResultString(AudioResult result);

    /*************************************************************************
      \brief
        Prevent copying because FMOD resources can't be shared safely
      \details
        We delete the copy constructor and assignment operator because:

        1. FMOD::System* can only be owned by one wrapper at a time
        2. If someone copies this object, both copies would try to release
           the same FMOD system when they're destroyed
        3. This would cause a double-free crash that's really hard to debug
        4. Better to make copying impossible than spend hours debugging crashes

        If you really need multiple audio systems (you probably don't),
        create separate GGFMODWrapper instances instead of copying.
    *************************************************************************/
    // Don't let people accidentally copy this audio system
    // Copying would mean two objects trying to control the same FMOD system = instant crash
    // This is like having two people trying to drive the same car at once which is a bad idea lol
    GGFMODWrapper(const GGFMODWrapper&) = delete;
    GGFMODWrapper& operator=(const GGFMODWrapper&) = delete;

    /*************************************************************************
      \brief
        Test basic audio functionality using cached audio asset
      \param[in] audioAsset
        Reference to cached audio asset from AssetManager
      \details
        Tests audio playback using existing FMOD sound from AssetManager
        cache instead of creating duplicate sound. Demonstrates proper
        integration between AssetManager and audio system.
    *************************************************************************/
    void TestAudioPlayback(const GGAudioAsset& audioAsset);

    /*************************************************************************
      \brief
        Structure to track currently playing audio
    *************************************************************************/
    struct PlayingAudio
    {
        std::string assetPath;
        FMOD::Channel* channel;
        AudioSource* sourcePtr;
        std::chrono::steady_clock::time_point startTime;

        PlayingAudio(const std::string& path, FMOD::Channel* ch, AudioSource* src = nullptr)
            : assetPath(path), channel(ch), sourcePtr(src), startTime(std::chrono::steady_clock::now())
        {
        }
    };

    /*************************************************************************
      \brief
        Get list of currently playing audio files
      \return
        Vector of currently playing audio information
    *************************************************************************/
    std::vector<PlayingAudio> GetCurrentlyPlayingAudio() const;

    /*************************************************************************
      \brief
        Get currently previewing audio (editor-only)
      \details
        Cleans up finished preview channels automatically.
      \return
        Vector of PlayingAudio structs
    *************************************************************************/
    std::vector<PlayingAudio> GetCurrentlyPreviewingAudio() const;

    /*************************************************************************
      \brief
        Play audio asset by path and track it for UI display
      \param[in] assetPath
        Path of audio asset to play
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult PlayAudioByPath(const std::string& assetPath);

    /*************************************************************************
      \brief
        Get master volume level
      \return
        Current master volume (0.0 to 1.0)
    *************************************************************************/
    float GetMasterVolume() const;

    /*************************************************************************
      \brief Retrieve FMOD channel group associated with an audio bus
      \param busID Identifier of the bus
      \return ChannelGroup pointer or nullptr on failure
    *************************************************************************/
    FMOD::ChannelGroup* GetChannelGroupFromBus(AudioBusID busID);

    /*************************************************************************
      \brief
        Play an ECS AudioSource and apply all properties (volume, pitch, pan, loop)
      \param[in,out] source
        AudioSource to play
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult PlayAudioSource(AudioSource& source);

    /*************************************************************************
      \brief
        Update playback cursor positions for all currently playing ECS audio
      \details
        Updates AudioSource::playbackCursor for UI display or logic
    *************************************************************************/
    void UpdatePlaybackCursors();

    /*************************************************************************
      \brief
        Get playback cursor of a specific AudioSource
      \param[in] source
        AudioSource to query
      \return
        Playback position in milliseconds
    *************************************************************************/
    uint64_t GetPlaybackCursor(const AudioSource& source) const;

    /*************************************************************************
      \brief
        Play an audio asset as an editor preview (non-ECS)
      \param[in] path
        Path to audio asset
      \return
        AudioResult indicating success or failure
    *************************************************************************/
    AudioResult PlayEditorAudioPreview(const std::string& path);

    /*************************************************************************
      \brief
        Fade the master audio bus out over time
      \param[in] seconds
        Duration of the fade-out in seconds
      \details
        Gradually reduces the master bus volume from its current level
        down to silence over the specified duration. This is commonly
        used for scene transitions or global audio shutdown effects.

        The fade operation is processed incrementally during Update()
        and does not block execution.
    *************************************************************************/
    void FadeMasterOut(float seconds);

    /*************************************************************************
      \brief
        Fade the master audio bus in over time
      \param[in] seconds
        Duration of the fade-in in seconds
      \details
        Gradually increases the master bus volume from silence (or its
        current level) up to the stored master volume over the specified
        duration. Typically used after a scene transition completes.

        The fade operation is processed incrementally during Update().
    *************************************************************************/
    void FadeMasterIn(float seconds);

    /*************************************************************************
      \brief
        Check whether the master audio bus is currently fading
      \return
        True if a master fade-in or fade-out is currently active
      \details
        Used to determine whether audio transitions are still in progress.
        This is especially useful for coordinating scene switches or
        preventing overlapping fade operations.
    *************************************************************************/
    bool IsMasterFading() const;
    
    /*************************************************************************
      \brief
        Start a scene transition with audio fade-out
      \param[in,out] sceneKey
        Key of the scene to switch to after fade-out completes
      \param[in] fadeDuration
        Duration of the audio fade-out before switching scenes
      \details
        Initiates a coordinated scene transition by:
        1. Fading out the master audio bus over the specified duration
        2. Deferring the actual scene switch until the fade completes
        3. Allowing audio to fade back in after the new scene loads

        This function does not immediately switch scenes. The transition
        is finalized during Update() once the fade-out finishes.
    *************************************************************************/
    void StartSceneTransition(std::string& sceneKey, float fadeDuration);

private:
    FMOD::System* fmodSystem = nullptr;     // FMOD system instance
    bool isInitialized = false;             // Initialization state flag

    // Track all created sounds for automatic cleanup
    std::vector<FMOD::Sound*> activeSounds;

    // Track currently playing audio for UI display
    mutable std::vector<PlayingAudio> currentlyPlaying;
    mutable std::vector<PlayingAudio> currentlyPaused;

    mutable std::vector<PlayingAudio> currentlyPreviewing;

    float currentMasterVolume = 1.0f;

    std::array<AudioBus, static_cast<size_t>(AudioBusID::MAX_COUNT)> audioBuses;
    std::unordered_map<AudioSource*, FMOD::Channel*> sourceToChannel;

    // for scene switching (fade in/outs) ------
    struct BusFade {
        FMOD::ChannelGroup* group = nullptr;
        float startVolume = 1.f;
        float targetVolume = 1.f;
        float duration = 0.f;
        float elapsed = 0.f;
        bool active = false;
    };

    BusFade masterFade;

    bool isPendingSceneSwitch = false;
    std::string pendingSceneKey;
    float sceneTransitionTimer = 0.0f;
    float fadeOutDuration = 0.0f;
    // -----------------------------------------

    /*************************************************************************
      \brief
        Convert FMOD_RESULT to AudioResult with logging
      \param[in] fmodResult
        FMOD result to convert
      \param[in] operation
        Description of operation for logging
      \return
        Corresponding AudioResult
    *************************************************************************/
    AudioResult ConvertFMODResult(FMOD_RESULT fmodResult, const std::string& operation);

    /*************************************************************************
      \brief
        Validate channel pointer and log errors
      \param[in] channel
        Channel to validate
      \param[in] operation
        Operation name for logging
      \return
        True if channel is valid
    *************************************************************************/
    bool ValidateChannel(FMOD::Channel* channel, const std::string& operation);
};