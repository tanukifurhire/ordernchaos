/*************************************************************************
  \file       GGJSONUtils.hpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       03 October 2025
  \version    1.0
  \brief
	Declares utility functions for reading, parsing, and writing JSON files.

  \details
	Provides helper methods to streamline JSON handling for the engine:
	- Read JSON file contents into a string.
	- Parse a JSON string into a rapidjson::Document.
	- Serialize a rapidjson::Document and save it back to file.

  \dependencies
	<iostream>, <fstream>, <sstream>, <string>, 
	RapidJSONWrapper.hpp
  \history
	- 1.0 (09 September 2025): Initial version with JSON read/parse/serialize utilities.
  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <RapidJSONWrapper.hpp>

namespace JSONUtils {

	/*************************************************************************
	  \brief Read the contents of a JSON file
	  \param filename Path to the JSON file
	  \return File contents as a string
	*************************************************************************/
	std::string ReadFileContents(const std::string& filename);

	/*************************************************************************
	  \brief Parse a JSON string into a rapidjson::Document
	  \param jsonStr JSON string to parse
	  \return Parsed rapidjson::Document
	*************************************************************************/
	rapidjson::Document ParseJSON(const std::string& jsonStr);

	/*************************************************************************
	  \brief Serialize a rapidjson::Document to a file
	  \param document JSON document to serialize
	  \param filename Output file path
	*************************************************************************/
	bool SerializeJSONToFile(const rapidjson::Document& document, const std::string& filename);

	// this function converts a rapidjson::Value to a std::string
	std::string StringifyJSONValue(const rapidjson::Value& value);
}

