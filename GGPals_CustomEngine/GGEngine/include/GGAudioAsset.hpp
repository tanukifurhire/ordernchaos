/*************************************************************************
  \file       GGAudioAsset.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       25 September 2025
  \version    2.0
  \brief
    Audio asset wrapper with lifetime management.
  \details
    Owns FMOD sound handle and provides asset information. Cleanup is handled
    by the asset manager calling explicit Release() before audio system shutdown.
  \dependencies
    <string>, <fmod.hpp>
  \see
    GGAssetManager, GGFMODWrapper
  \history
    - 1.0 (16 September 2025): Initial singleton version.
    - 2.0 (25 September 2025): Simplified version with explicit cleanup.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once
#include <string>
#include <fmod.hpp>

/*************************************************************************
  \brief
    Simplified audio asset wrapper with explicit cleanup
  \details
    Stores FMOD sound handle and provides audio information. Asset manager
    is responsible for calling Release() before audio system shutdown to
    ensure proper cleanup order without complex lifetime management.
  \par Usage
  \code
    // Created through asset manager
    GGAudioAsset audioAsset("../../Assets/Audio/SFX/jump.wav", audioSystem);

    // Use for playback
    if (audioAsset.IsValid())
    {
        FMOD::Channel* channel = nullptr;
        audioSystem->PlayAudio(audioAsset.GetFMODSound(), &channel);
    }

    // Asset manager calls this before destroying audio system
    audioAsset.Release(audioSystem);
  \endcode
*************************************************************************/
class GGAudioAsset
{
public:
    /*************************************************************************
      \brief
        Constructor that loads audio file
      \param[in] filePath
        Complete path to audio file
      \param[in] audioSystem
        Raw pointer to audio system for loading
    *************************************************************************/
    GGAudioAsset(const std::string& filePath, class GGFMODWrapper* audioSystem);

    /*************************************************************************
      \brief
        Destructor - audio cleanup should already be done via Release()
    *************************************************************************/
    ~GGAudioAsset();

    /*************************************************************************
      \brief
        Explicitly release audio resources through audio system
      \param[in] audioSystem
        Audio system to use for safe cleanup
      \details
        Must be called by asset manager before audio system shutdown.
        Safe to call multiple times.
    *************************************************************************/
    void Release(class GGFMODWrapper* audioSystem);

    /*************************************************************************
      \brief
        Check if audio asset was loaded successfully
    *************************************************************************/
    bool IsValid() const { return fmodSound != nullptr && !isReleased; }

    /*************************************************************************
      \brief
        Get the FMOD sound object for playback
    *************************************************************************/
    FMOD::Sound* GetFMODSound() const { return fmodSound; }

    /*************************************************************************
      \brief
        Get the original file path of this audio asset
    *************************************************************************/
    const std::string& GetFilePath() const { return filePath; }

    /*************************************************************************
      \brief
        Get audio asset length in milliseconds
    *************************************************************************/
    unsigned int GetLengthMS() const;

    /*************************************************************************
      \brief
        Get human-readable audio format information
    *************************************************************************/
    std::string GetFormatInfo() const;

    /*************************************************************************
      \brief
        Get audio asset size in bytes
    *************************************************************************/
    size_t GetSizeBytes() const;

    /*************************************************************************
      \brief
        Check if this is a streaming audio asset
    *************************************************************************/
    bool IsStreaming() const;

    /*************************************************************************
      \brief
        Get audio asset type identifier
    *************************************************************************/
    static std::string GetAssetType() { return "AudioAsset"; }

private:
    FMOD::Sound* fmodSound;         // FMOD sound handle
    std::string filePath;           // Original file path
    bool isReleased;                // Track if explicitly released

    // Prevent copying
    GGAudioAsset(const GGAudioAsset&) = delete;
    GGAudioAsset& operator=(const GGAudioAsset&) = delete;
};