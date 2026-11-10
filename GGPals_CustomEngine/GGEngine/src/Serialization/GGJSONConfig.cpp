/*************************************************************************
  \file       GGJSONConfig.cpp
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
	GGJSONUtils.hpp, GGJSONConfig.hpp
  \see
	GGJSONConfig.cpp, GGJSONUtils.hpp
  \history
	- 1.0 (9 September 2025): Initial version with core JSON config accessors.
  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <GGJSONUtils.hpp>
#include <GGJSONConfig.hpp>
#include <string>

namespace GGEngineConfig {

	std::string configStr = JSONUtils::ReadFileContents("../../config/config.json");
	rapidjson::Document configDoc = JSONUtils::ParseJSON(configStr);

	/*************************************************************************
	  \brief Engine information accessors
	*************************************************************************/
	namespace GGEngineInfo {
		std::string EngineName()			{ return configDoc["info"]["name"].GetString(); }
		bool DebugMode()					{ return configDoc["info"]["debugMode"].GetBool(); }
	}

	/*************************************************************************
	  \brief Engine debug display flags
	*************************************************************************/
	namespace GGEngineDebug {
		bool ShowFPS()						{ return configDoc["debug"]["showFPS"].GetBool(); }
		bool DrawColliders()				{ return configDoc["debug"]["drawColliders"].GetBool(); }
	}

	/*************************************************************************
	  \brief Graphics configuration accessors
	*************************************************************************/
	namespace GGEngineGraphics {

		namespace GGEngineWindow {
			std::string WindowName() { return configDoc["graphics"]["window"]["windowName"].GetString(); }
			int WindowWidth() { return configDoc["graphics"]["window"]["windowWidth"].GetInt(); }
			int WindowHeight() { return configDoc["graphics"]["window"]["windowHeight"].GetInt(); }
			bool IsFullScreen() { return configDoc["graphics"]["window"]["fullscreen"].GetBool(); }
			bool IsResizable() { return configDoc["graphics"]["window"]["resizable"].GetBool(); }
		}

		namespace GGEngineViewport {
			int ViewportWidth() { return configDoc["graphics"]["viewport"]["viewportWidth"].GetInt(); }
			int ViewportHeight() { return configDoc["graphics"]["viewport"]["viewportHeight"].GetInt(); }
		}

		namespace GGEngineRenderer {
			int FrameRateCap() { return configDoc["graphics"]["renderer"]["frameRateCap"].GetInt(); }
		}
	}

	/*************************************************************************
	  \brief Physics configuration accessors
	*************************************************************************/
	namespace GGEnginePhysics {
		bool IsEnabled()				{ return configDoc["physics"]["enabled"].GetBool(); }
		double Gravity()				{ return configDoc["physics"]["gravity"].GetDouble(); }
		double FixedTimeStep()			{ return configDoc["physics"]["fixedTimeStep"].GetDouble(); }
	}

	/*************************************************************************
	  \brief TileMap configuration accessors
	*************************************************************************/
	namespace GGEngineTileMap {
		int TileWidth()					{ return configDoc["tilemap"]["tileWidth"].GetInt(); }
		int TileHeight()				{ return configDoc["tilemap"]["tileHeight"].GetInt(); }
		int MaxLayers()					{ return configDoc["tilemap"]["maxLayers"].GetInt(); }
		std::string DefaultLayer()		{ return configDoc["tilemap"]["defaultLayer"].GetString(); }
		std::string CollisionLayer()	{ return configDoc["tilemap"]["collisionLayer"].GetString(); }
	}

	/*************************************************************************
	  \brief Audio configuration accessors
	*************************************************************************/
	namespace GGEngineAudio {
		double MasterVolume()			{ return configDoc["audio"]["masterVolume"].GetDouble(); }
		double BGMVolume()				{ return configDoc["audio"]["bgmVolume"].GetDouble(); }
		double SFXVolume()				{ return configDoc["audio"]["sfxVolume"].GetDouble(); }
		int MaxPolyphony()				{ return configDoc["audio"]["maxPolyphony"].GetInt(); }
		bool MuteMaster()				{ return configDoc["audio"]["muteMaster"].GetBool(); }
		bool MuteMusic()				{ return configDoc["audio"]["muteMusic"].GetBool(); }
		bool MuteSFX()					{ return configDoc["audio"]["muteSFX"].GetBool(); }

		// setters:
		void SetMasterVolume(double value)	{ configDoc["audio"]["masterVolume"].SetDouble(std::round(value * 10.0) / 10.0); }
		void SetBGMVolume(double value)		{ configDoc["audio"]["bgmVolume"].SetDouble(std::round(value * 10.0) / 10.0); }
		void SetSFXVolume(double value)		{ configDoc["audio"]["sfxVolume"].SetDouble(std::round(value * 10.0) / 10.0); }
		void SaveAudioSettings()
		{
			JSONUtils::SerializeJSONToFile(configDoc, "../../config/config.json");
		}
	}

	/*************************************************************************
	 \brief Input bindings accessors
	*************************************************************************/
	namespace GGEngineInput {
		std::string MoveUp()			{ return configDoc["input"]["moveUp"].GetString(); }
		std::string MoveDown()			{ return configDoc["input"]["moveDown"].GetString(); }
		std::string MoveLeft()			{ return configDoc["input"]["moveLeft"].GetString(); }
		std::string MoveRight()			{ return configDoc["input"]["moveRight"].GetString(); }
		std::string Jump()				{ return configDoc["input"]["jump"].GetString(); }
		std::string Dash()				{ return configDoc["input"]["dash"].GetString(); }
		std::string Pause()				{ return configDoc["input"]["pause"].GetString(); }
	}

	/*************************************************************************
	  \brief Network configuration accessors
	*************************************************************************/
	namespace GGEngineNetwork {
		bool IsEnabled()				{ return configDoc["network"]["enabled"].GetBool(); }
		std::string ServerIP()			{ return configDoc["network"]["serverIP"].GetString(); }
		int MaxPlayers()				{ return configDoc["network"]["maxPlayers"].GetInt(); }
	}

	/*************************************************************************
	  \brief File system path accessors
	*************************************************************************/
	namespace GGEnginePath {
		std::string AssetPath()			{ return configDoc["paths"]["assetPath"].GetString(); }
		std::string SpritesheetPath()	{ return configDoc["paths"]["spritesheetPath"].GetString(); }
		std::string TilesetPath()		{ return configDoc["paths"]["tileSetPath"].GetString(); }
		std::string AudioPath()			{ return configDoc["paths"]["audioPath"].GetString(); }
	}

	namespace GGDefaultGameSettings {
		std::string GetProjectName() { return configDoc["gameSettings"]["projectName"].GetString(); }
		std::string GetDefaultGameScene() { return configDoc["gameSettings"]["defaultGameScene"].GetString(); }
		double GetAudioFadeDuration() { return configDoc["gameSettings"]["audioFadeDuration"].GetDouble(); }
	}

}