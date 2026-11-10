/*************************************************************************
  \file       RapidJSONWrapper.hpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       03 October 2025
  \version    1.0
  \brief
    Provides a wrapper header for including and configuring RapidJSON
    with MSVC-specific warning suppressions.

  \details
    - Silences common MSVC and CppCoreCheck warnings that arise when
      using RapidJSON (e.g., uninitialized variables, deprecated CRT functions).
    - Ensures cleaner integration of RapidJSON in the project without
      cluttering build output with irrelevant warnings.
    - Includes core RapidJSON headers for parsing, writing, and managing JSON.

  \dependencies
    RapidJSON library (<rapidjson/document.h>, <rapidjson/writer.h>,
    <rapidjson/prettywriter.h>, <rapidjson/stringbuffer.h>)

  \history
    - 1.0 (03 October 2025): Initial version with warning suppression
      and common RapidJSON includes.
  \copyright
    Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#ifndef RAPIDJSON_WRAPPER_HPP
#define RAPIDJSON_WRAPPER_HPP

// silence MSVC iterator base warnings
#define _SILENCE_ALL_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING

// suppress common RapidJSON warnings from MSVC / CppCoreCheck
#pragma warning(push)
#pragma warning(disable : 26495) // uninitialized member variable
#pragma warning(disable : 26439) // function should be noexcept
#pragma warning(disable : 33010) // unchecked lower bound for enum index
#pragma warning(disable : 4996)  // deprecated/unsafe CRT functions
#pragma warning(disable : 5054)  // operator between enum types

// RapidJSON headers
#include <../rapidjson/document.h>      
#include <../rapidjson/writer.h>        
#include <../rapidjson/prettywriter.h>  
#include <../rapidjson/stringbuffer.h> 

#pragma warning(pop)

#endif // RAPIDJSON_WRAPPER_HPP