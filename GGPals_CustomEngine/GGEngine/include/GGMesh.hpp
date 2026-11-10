/*************************************************************************
  \file       GGMesh.hpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       03 October 2025
  \brief
	Mesh and vertex structures for GPU rendering, including instanced
	rendering support via Shader Storage Buffer Objects (SSBOs).

  \details
	Provides GGVertex and GGMesh structures. GGMesh supports:
	- Vertex and index buffer creation
	- VAO setup
	- Per-instance data updates via SSBOs
	- Cleanup of GPU resources
*************************************************************************/
#pragma once
#include <GGECSMeshRenderer.hpp>
#include <GGShader.hpp>
#include <vector>
#include <span>

/*************************************************************************
  \struct GGVertex
  \brief Represents a single vertex for mesh rendering.
  \details
	Contains position, color, and UV coordinates for the vertex.
*************************************************************************/
struct GGVertex 
{
	glm::vec3 position{};
	glm::vec4 color{};
	glm::vec2 uv{};
};

/*************************************************************************
  \struct GGMesh
  \brief GPU-side mesh representation with instancing support.
  \details
	GGMesh manages vertex and index buffers, VAOs, and optional SSBOs
	for per-instance data.
*************************************************************************/
struct GGMesh 
{
	GLenum primitiveType{ GL_TRIANGLES };
	GLuint vaoid{};
	GLuint instanceSSBO{};
	GLuint pickingInstanceSSBO{};
	size_t _reserved{};  // keeps struct size stable for MeshRenderer serialization
	GLuint drawCount{};

	/*************************************************************************
	  \brief Creates a mesh from vertex and index data.
	  \param vertices Vector of GGVertex describing mesh vertices.
	  \param indices Vector of GLushort for index data.
	  \param type OpenGL primitive type (default GL_TRIANGLES).
	  \param uUseInstancing Whether to enable SSBO for instanced rendering.
	  \details
		Sets up VAO, VBO, EBO, and optionally SSBO for instance data.
	*************************************************************************/
	void CreateMesh(std::vector<GGVertex> const& vertices, std::vector<GLushort> const& indices, GLenum type = GL_TRIANGLES,bool uUseIntancing = false);

	/*************************************************************************
	  \brief Updates per-instance data on the GPU.
	  \param instances Span of InstanceData structs to upload.
	  \details
		Lazily creates the SSBO if needed. Resizes or updates the buffer
		depending on the required size.
	*************************************************************************/
	void UpdateInstanceData(std::span<const InstanceData> instances);
	void UpdatePickingInstanceData(std::span<const PickingInstanceData> instances);
	void Cleanup();
};