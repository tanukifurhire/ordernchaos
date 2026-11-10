/*************************************************************************
  \file       GGAudioAsset.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       25 September 2025
  \version    2.0
  \brief
    Audio asset implementation with explicit cleanup.
  \details
    Handles audio asset loading and provides information queries.
    Uses explicit Release() method for proper cleanup order.
  \dependencies
    GGAudioAsset.hpp, GGAudioManager.hpp, <fmod_errors.h>, <iostream>, <sstream>
  \see
    GGAudioAsset.hpp, GGAssetManager.hpp, GGAudioManager.hpp
  \history
    - 1.0 (16 September 2025): Initial singleton version.
    - 2.0 (25 September 2025): Simplified version with explicit cleanup.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <GGAudioAsset.hpp>
#include <GGAudioManager.hpp>
#include <fmod_errors.h>
#include <iostream>
#include <sstream>

/*************************************************************************
  \brief
    Constructor that loads audio file using raw pointer to audio system
*************************************************************************/
GGAudioAsset::GGAudioAsset(const std::string& filePath, GGFMODWrapper* audioSystem)
    : fmodSound(nullptr)
    , filePath(filePath)
    , isReleased(false)
{
    if (audioSystem && audioSystem->IsInitialized())
    {
        fmodSound = audioSystem->CreateSound(filePath);

        if (fmodSound)
        {
            std::cout << "SUCCESS: Loaded audio asset: " << filePath << std::endl;
        }
        else
        {
            std::cout << "ERROR: Failed to load audio asset: " << filePath << std::endl;
        }
    }
    else
    {
        std::cout << "ERROR: Audio system not available for: " << filePath << std::endl;
    }
}

/*************************************************************************
  \brief
    Destructor - audio should already be released via Release() method
*************************************************************************/
GGAudioAsset::~GGAudioAsset()
{
    if (fmodSound && !isReleased)
    {
        std::cout << "WARNING: Audio asset destroyed without explicit release: "
            << filePath << std::endl;
        // Attempt direct FMOD release as fallback
        FMOD_RESULT result = fmodSound->release();
        if (result != FMOD_OK)
        {
            std::cout << "FMOD direct release warning for " << filePath
                << ": " << FMOD_ErrorString(result) << std::endl;
        }
    }
}

/*************************************************************************
  \brief
    Explicitly release audio resources through audio system
*************************************************************************/
void GGAudioAsset::Release(GGFMODWrapper* audioSystem)
{
    if (isReleased || !fmodSound)
    {
        return; // Already released or no sound to release
    }

    if (audioSystem && audioSystem->IsInitialized())
    {
        // Proper cleanup through audio system
        audioSystem->ReleaseSound(fmodSound);
        std::cout << "Audio asset released through audio system: " << filePath << std::endl;
    }
    else
    {
        // Fallback: direct FMOD release
        std::cout << "WARNING: Audio system unavailable, releasing sound directly: " << filePath << std::endl;
        FMOD_RESULT result = fmodSound->release();
        if (result != FMOD_OK)
        {
            std::cout << "FMOD direct release warning for " << filePath
                << ": " << FMOD_ErrorString(result) << std::endl;
        }
    }

    fmodSound = nullptr;
    isReleased = true;
}

/*************************************************************************
  \brief
    Get audio asset length in milliseconds
*************************************************************************/
unsigned int GGAudioAsset::GetLengthMS() const
{
    if (!fmodSound || isReleased)
    {
        return 0;
    }

    unsigned int length = 0;
    FMOD_RESULT result = fmodSound->getLength(&length, FMOD_TIMEUNIT_MS);

    if (result != FMOD_OK)
    {
        std::cout << "WARNING: Failed to get audio asset length for " << filePath
            << ": " << FMOD_ErrorString(result) << std::endl;
        return 0;
    }

    return length;
}

/*************************************************************************
  \brief
    Get human-readable audio format information
*************************************************************************/
std::string GGAudioAsset::GetFormatInfo() const
{
    if (!fmodSound || isReleased)
    {
        return "Invalid or released audio asset";
    }

    FMOD_SOUND_TYPE soundType;
    FMOD_SOUND_FORMAT soundFormat;
    int channels;
    int bits;

    FMOD_RESULT result = fmodSound->getFormat(&soundType, &soundFormat, &channels, &bits);

    if (result != FMOD_OK)
    {
        std::cout << "WARNING: Failed to get audio asset format for " << filePath
            << ": " << FMOD_ErrorString(result) << std::endl;
        return "Unknown format";
    }

    // Get default frequency
    float frequency = 0.0f;
    result = fmodSound->getDefaults(&frequency, nullptr);
    if (result != FMOD_OK)
    {
        frequency = 44100.0f; // Default fallback
    }

    // Build format string
    std::stringstream formatStream;

    // Sound type
    switch (soundType)
    {
    case FMOD_SOUND_TYPE_WAV:
        formatStream << "WAV ";
        break;
    case FMOD_SOUND_TYPE_OGGVORBIS:
        formatStream << "OGG ";
        break;
    case FMOD_SOUND_TYPE_MPEG:
        formatStream << "MP3 ";
        break;
    case FMOD_SOUND_TYPE_FLAC:
        formatStream << "FLAC ";
        break;
    case FMOD_SOUND_TYPE_AIFF:
        formatStream << "AIFF ";
        break;
    default:
        formatStream << "Unknown ";
        break;
    }

    // Frequency
    formatStream << static_cast<int>(frequency) << "Hz ";

    // Bit depth
    formatStream << bits << "bit ";

    // Channel configuration
    if (channels == 1)
    {
        formatStream << "Mono";
    }
    else if (channels == 2)
    {
        formatStream << "Stereo";
    }
    else
    {
        formatStream << channels << "ch";
    }

    return formatStream.str();
}

/*************************************************************************
  \brief
    Get audio asset size in bytes
*************************************************************************/
size_t GGAudioAsset::GetSizeBytes() const
{
    if (!fmodSound || isReleased)
    {
        return 0;
    }

    unsigned int lengthBytes = 0;
    FMOD_RESULT result = fmodSound->getLength(&lengthBytes, FMOD_TIMEUNIT_RAWBYTES);

    if (result != FMOD_OK)
    {
        std::cout << "WARNING: Failed to get audio asset size for " << filePath
            << ": " << FMOD_ErrorString(result) << std::endl;
        return 0;
    }

    return static_cast<size_t>(lengthBytes);
}

/*************************************************************************
  \brief
    Check if this is a streaming audio asset
*************************************************************************/
bool GGAudioAsset::IsStreaming() const
{
    if (!fmodSound || isReleased)
    {
        return false;
    }

    FMOD_MODE mode;
    FMOD_RESULT result = fmodSound->getMode(&mode);

    if (result != FMOD_OK)
    {
        std::cout << "WARNING: Failed to get audio asset mode for " << filePath
            << ": " << FMOD_ErrorString(result) << std::endl;
        return false;
    }

    // Check if sound was created with streaming mode
    return (mode & FMOD_CREATESTREAM) != 0;
}