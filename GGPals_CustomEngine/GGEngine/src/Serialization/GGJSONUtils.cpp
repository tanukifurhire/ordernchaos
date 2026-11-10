/*************************************************************************
  \file       GGJSONUtils.cpp
  \author     Tan Jia Qi, Trish
  \par        Email: jiaqitrish.tan@digipen.edu
  \date       03 October 2025
  \version    1.1
  \brief
	Declares utility functions for reading, parsing, and writing JSON files.

  \details
	Provides helper methods to streamline JSON handling for the engine:
	- Read JSON file contents into a string.
	- Parse a JSON string into a rapidjson::Document.
	- Serialize a rapidjson::Document and save it back to file.
	- - Convert a rapidjson::Value to a string for runtime storage and retrieval
      (e.g., storing Prefab component data as strings).

  \dependencies
	<iostream>, <fstream>, <sstream>, <string>, <filesystem>
	RapidJSONWrapper.hpp

  \history
	- 1.0 (9 September 2025): Initial version with JSON read/parse/serialize utilities.
	- 1.1 (3 October 2025): Added StringifyJSONValue() for serializing rapidjson::Value to string.
  
  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
*************************************************************************/

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include "RapidJSONWrapper.hpp"

namespace JSONUtils {

	/*************************************************************************
	  \brief Read the contents of a JSON file
	  \param filename Path to the JSON file
	  \return File contents as a string
	*************************************************************************/
	std::string ReadFileContents(const std::string& filename) {
		// std::cout << "Current working dir: " << std::filesystem::current_path() << "\n";
		std::ifstream file(filename);
		if (!file.is_open()) {
			// throw std::runtime_error("Failed to open file: " + filename + " :(");
			std::cerr << "failed to open file: " << filename << std::endl;
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		file.close();


		return buffer.str();
	}

	/*************************************************************************
	  \brief Parse a JSON string into a rapidjson::Document
	  \param jsonStr JSON string to parse
	  \return Parsed rapidjson::Document
	*************************************************************************/
	rapidjson::Document ParseJSON(const std::string& jsonStr) {
		rapidjson::Document document;
		if (document.Parse(jsonStr.c_str()).HasParseError()) {
			// throw std::runtime_error("Error parsing JSON :(");
			std::cerr << "failed to parse JSON" << std::endl;
		}
		return std::move(document);
	}

	/*************************************************************************
	  \brief Serialize a rapidjson::Document to a file
	  \param document JSON document to serialize
	  \param filename Output file path
	*************************************************************************/
	bool SerializeJSONToFile(const rapidjson::Document& document, const std::string& filename) {
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		document.Accept(writer);

		std::ofstream file(filename);
		if (!file.is_open()) {
			std::cerr << "Failed to create or open file: " << filename << "\n";
			return false;
		}

		file << buffer.GetString();
		file.close();

		return true;
	}

	/*************************************************************************
	  \brief Convert a RapidJSON value into a JSON-formatted string
	  \param value Reference to the rapidjson::Value to serialize
	  \return JSON string representing the given value
	*************************************************************************/
	std::string StringifyJSONValue(const rapidjson::Value& value) {
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		value.Accept(writer);
		return buffer.GetString();
	}

}