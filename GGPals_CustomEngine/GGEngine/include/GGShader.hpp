/**************************************************************************
  \file       GGShader.hpp
  \author     Tai Xiao Xuan
  \par        Email: xiaoxuan.tai@digipen.edu
  \date       3 October 2025
  \version    1.0
  \brief
	Defines the GGShader class for OpenGL shader program management.
  \details
	The GGShader class encapsulates OpenGL shader compilation, linking,
	and uniform variable management. It handles loading shader source from
	files, compiling vertex and fragment shaders, and provides a clean
	interface for setting uniform variables commonly used in graphics
	applications. The class follows RAII principles for automatic resource
	cleanup through implicit destructor management.
  \dependencies
	GLEW, GLFW, GLM, <string>, <fstream>, <sstream>, <iostream>
  \see
	glCreateShader, glCreateProgram, glUseProgram
  \history
	- 1.0 (3 October 2025): Initial implementation.
  \copyright
	Copyright (C) 2025 DigiPen Institute of Technology.
**************************************************************************/

#pragma once

#define GLFW_INCLUDE_NONE
#include <gl/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>


/*************************************************************************
  \brief
	OpenGL shader program wrapper for graphics pipeline management.
  \details
	Manages the complete lifecycle of an OpenGL shader program including
	compilation of vertex and fragment shaders, program linking, and
	uniform variable updates. Resources are automatically released when
	the object is destroyed (Rule of Zero).
*************************************************************************/
class GGShader {
public:
	GGShader(){}

	/*************************************************************************
	  \brief Constructs and compiles a shader program from file paths.
	  \param[in] vertexPath
		Filesystem path to the vertex shader source code (.vert/.glsl).
	  \param[in] fragmentPath
		Filesystem path to the fragment shader source code (.frag/.glsl).
	  \details
		Loads shader source from disk, compiles both vertex and fragment
		shaders, links them into a program, and automatically activates
		the program. Compilation/link errors are printed to std::cout.
		Shader objects are deleted after successful linking.
	  \warning
		If file I/O fails, compilation errors occur, or linking fails,
		error messages are printed but execution continues with potentially
		invalid shader state. Consider adding exception handling.
	*************************************************************************/
	GGShader(std::string const& vertexPath, std::string const& fragmentPath);

	void Use() const;

	void UnUse() const;

	void SetBool(std::string const& name, bool value) const;

	void SetInt(std::string const& name, int value) const;
	
	void SetFloat(std::string const& name, float value) const;

	void SetVec4(std::string const& name, float valueX, float valueY, float valueZ, float valueW) const;

	void SetMat4(std::string const& name, glm::mat4 const& value) const;

	void SetMat3(std::string const& name, glm::mat3 const& value) const;

	void SetVec2(const std::string& name, float x, float y) const;

	void SetVec2(const std::string& name, const glm::vec2& value) const;

	GLuint GetShaderID() const {
		return id;
	}

protected:
	unsigned int id{};
};
