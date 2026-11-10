/*************************************************************************
  \file       EditorPlatform.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       10 December 2025
  \version    1.0
  \brief
    Helper Utilities to open .lua code files in Visual Studio Code

  \dependencies
    <string>           - Used for lua script paths
  \history
    - 1.0 (10 December 2025): Initial version.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#pragma once

#include <string>

class Platform
{
public:
    /*
    * Example use case: 
    *   Platform::OpenInEditor("../../../Assets/Scripts/YourScript.lua");
    *   
    * What it does:
    *   Opens Visual Studio code (if registered in PATH), the script file that was 
    *   selected in the editor.
    */
	static bool OpenInEditor(std::string const& filePath);
};