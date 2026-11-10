/**************************************************************************
  \file       GGShader.cpp
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
#include "GGShader.hpp"
#include <GGLogger.hpp>
#include <glm/gtc/type_ptr.hpp>

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
GGShader::GGShader(std::string const& vertexPath, std::string const& fragmentPath) {
	std::string vertexCode;
	std::string fragmentCode;
	std::ifstream vShaderFile;
	std::ifstream fShaderFile;

	// Enable exceptions on file streams
	vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

	try {
		// Open shader files
		vShaderFile.open(vertexPath);
		fShaderFile.open(fragmentPath);

		std::stringstream vShaderStream, fShaderStream;

		// Read file contents into streams
		vShaderStream << vShaderFile.rdbuf();
		fShaderStream << fShaderFile.rdbuf();

		// Close file handles
		vShaderFile.close();
		fShaderFile.close();

		// Convert streams to strings
		vertexCode = vShaderStream.str();
		fragmentCode = fShaderStream.str();
	}
	catch (std::ifstream::failure e) {
		std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ" << std::endl;
	}

	const char* vShaderCode = vertexCode.c_str();
	const char* fShaderCode = fragmentCode.c_str();

	unsigned int vertex{}, fragment{};
	int success{};
	char infoLog[512]{};

	// Compile vertex shader
	vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, &vShaderCode, NULL);
	glCompileShader(vertex);

	bool compileOk = true;

	glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertex, 512, NULL, infoLog);
		GGLogger::Error("[SHADER] VERTEX COMPILATION FAILED (" + vertexPath + "): " + infoLog);
		compileOk = false;
	}

	// Compile fragment shader
	fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &fShaderCode, NULL);
	glCompileShader(fragment);

	glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragment, 512, NULL, infoLog);
		GGLogger::Error("[SHADER] FRAGMENT COMPILATION FAILED (" + fragmentPath + "): " + infoLog);
		compileOk = false;
	}

	// Link shaders into a program
	id = glCreateProgram();
	glAttachShader(id, vertex);
	glAttachShader(id, fragment);
	glLinkProgram(id);

	glGetProgramiv(id, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(id, 512, NULL, infoLog);
		GGLogger::Error("[SHADER] LINK FAILED (" + vertexPath + " + " + fragmentPath + "): " + infoLog);
		compileOk = false;
	}

	if (!compileOk) {
		glDeleteProgram(id);
		id = 0;
	}

	if (id != 0)
		Use();

	glDeleteShader(vertex);
	glDeleteShader(fragment);
}

void GGShader::Use() const {
	glUseProgram(id);
}

void GGShader::UnUse() const {
	glUseProgram(0);
}

void GGShader::SetBool(std::string const& name, bool value) const {
	glUniform1i(glGetUniformLocation(id, name.c_str()), (int)value);
}

void GGShader::SetInt(std::string const& name, int value) const {
	glUniform1i(glGetUniformLocation(id, name.c_str()), value);
}

void GGShader::SetFloat(std::string const& name, float value) const {
	glUniform1f(glGetUniformLocation(id, name.c_str()), value);
}

void GGShader::SetVec4(std::string const& name, float valueX, float valueY, float valueZ, float valueW) const {
	glUniform4f(glGetUniformLocation(id, name.c_str()), valueX, valueY, valueZ, valueW);
}

void GGShader::SetMat4(std::string const& name, glm::mat4 const& value) const {
	glUniformMatrix4fv(glGetUniformLocation(id, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
}

void GGShader::SetMat3(std::string const& name, glm::mat3 const& value) const {
	glUniformMatrix3fv(glGetUniformLocation(id, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
}

void GGShader::SetVec2(const std::string& name, float x, float y) const
{
	glUniform2f(glGetUniformLocation(id, name.c_str()), x, y);
}

void GGShader::SetVec2(const std::string& name, const glm::vec2& value) const
{
	glUniform2fv(glGetUniformLocation(id, name.c_str()), 1, &value[0]);
}
