/*************************************************************************
  \file       GGJSONConfig.hpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       03 October 2025
  \version    1.0
  \brief
	Declares configuration accessors for GGEngine settings parsed from JSON.

  \details
	Provides a central interface to retrieve engine configuration values from
	a JSON document. This includes settings for:
	- Engine information and debug flags.
	- Graphics (window, viewport, renderer).
	- Physics (enable toggle, gravity, timestep).
	- TileMap dimensions and layer info.
	- Audio volumes, polyphony, and mute states.
	- Input bindings for movement and actions.
	- Network connectivity options.
	- File system paths for engine assets.

  \dependencies
	<string>
	GGJSONUtils.hpp
  \see
	GGJSONConfig.cpp, GGJSONUtils.hpp
  \history
	- 1.0 (9 September 2025): Initial version with core JSON config accessors.
  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <string>
#include <GGJSONUtils.hpp>

namespace GGEngineConfig {

	extern std::string configStr;
	extern rapidjson::Document configDoc;

	/*************************************************************************
	  \brief Engine information accessors
	*************************************************************************/
	namespace GGEngineInfo {
		std::string EngineName();
		bool DebugMode();
	}

	/*************************************************************************
	  \brief Engine debug display flags
	*************************************************************************/
	namespace GGEngineDebug {
		bool ShowFPS();
		bool DrawColliders();
	}

	/*************************************************************************
	  \brief Graphics configuration accessors
	*************************************************************************/
	namespace GGEngineGraphics {

		namespace GGEngineWindow {
			std::string WindowName();
			int WindowWidth();
			int WindowHeight();
			bool IsFullScreen();
			bool IsResizable();
		}

		namespace GGEngineViewport {
			int ViewportWidth();
			int ViewportHeight();
		}

		namespace GGEngineRenderer {
			int FrameRateCap();
		}
	}

	/*************************************************************************
	  \brief Physics configuration accessors
	*************************************************************************/
	namespace GGEnginePhysics {
		bool IsEnabled();
		double Gravity();
		double FixedTimeStep();
	}

	/*************************************************************************
	  \brief TileMap configuration accessors
	*************************************************************************/
	namespace GGEngineTileMap {
		int TileWidth();
		int TileHeight();
		int MaxLayers();
		std::string DefaultLayer();
		std::string CollisionLayer();
	}

	/*************************************************************************
	  \brief Audio configuration accessors
	*************************************************************************/
	namespace GGEngineAudio {
		double MasterVolume();
		double BGMVolume();
		double SFXVolume();
		int MaxPolyphony();
		bool MuteMaster();
		bool MuteMusic();
		bool MuteSFX();

		void SetMasterVolume(double value);
		void SetBGMVolume(double value);
		void SetSFXVolume(double value);
		void SaveAudioSettings();
	}

	/*************************************************************************
	 \brief Input bindings accessors
    *************************************************************************/
	namespace GGEngineInput {
		std::string MoveUp();
		std::string MoveDown();
		std::string MoveLeft();
		std::string MoveRight();
		std::string Jump();
		std::string Dash();
		std::string Pause();
	}

	/*************************************************************************
	  \brief Network configuration accessors
	*************************************************************************/
	namespace GGEngineNetwork {
		bool IsEnabled();
		std::string ServerIP();
		int MaxPlayers();
	}

	/*************************************************************************
	  \brief File system path accessors
	*************************************************************************/
	namespace GGEnginePath {
		std::string AssetPath();
		std::string SpritesheetPath();
		std::string TilesetPath();
		std::string AudioPath();
	}

	/*************************************************************************
	  \brief Release mode default scene accessors
	*************************************************************************/
	namespace GGDefaultGameSettings {
		std::string GetDefaultGameScene();
		std::string GetProjectName();
		double GetAudioFadeDuration();
	}
}