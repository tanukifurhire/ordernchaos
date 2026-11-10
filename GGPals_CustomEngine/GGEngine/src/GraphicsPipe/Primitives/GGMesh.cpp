/*************************************************************************
  \file       GGMesh.cpp
  \author     Abhishek Akilandeswari Patchamuthu
  \par        Email: a.patchamuthu@digipen.edu
  \date       03 October 2025
  \brief
	Implementation of GGMesh functions for GPU rendering and instanced
	data updates.
*************************************************************************/
#include <GGMesh.hpp>

/*************************************************************************
  \brief Updates per-instance data on GPU.
  \param instances Span of InstanceData structs.
  \details
	Lazily creates the SSBO if it does not exist. Resizes the buffer if
	required, otherwise updates existing data with glBufferSubData.
	Finally binds the SSBO to binding point 0.
*************************************************************************/
void GGMesh::UpdateInstanceData(std::span<const InstanceData> instances)

{
	if (instanceSSBO == 0) {
		glGenBuffers(1, &instanceSSBO); // lazily create SSBO
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, instanceSSBO);

	size_t required = instances.size() * sizeof(InstanceData);

	// Always orphan-and-rewrite (glBufferData) instead of glBufferSubData.
	// glBufferSubData overwrites a buffer the GPU may still be reading from
	// the previous glDrawElementsInstanced in the same batch loop.
	// glBufferData orphans the old storage (GPU keeps reading it) and writes
	// to fresh memory, avoiding the write-after-read hazard that causes
	// instance data corruption on AMD drivers.
	glBufferData(GL_SHADER_STORAGE_BUFFER, required, instances.data(), GL_DYNAMIC_DRAW);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, instanceSSBO);
}

void GGMesh::UpdatePickingInstanceData(std::span<const PickingInstanceData> instances)

{
	if (pickingInstanceSSBO == 0) {
		glGenBuffers(1, &pickingInstanceSSBO); // lazily create SSBO
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, pickingInstanceSSBO);

	size_t required = instances.size() * sizeof(PickingInstanceData);

	// Always orphan — same reason as UpdateInstanceData above.
	glBufferData(GL_SHADER_STORAGE_BUFFER, required, instances.data(), GL_DYNAMIC_DRAW);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pickingInstanceSSBO);
}
/*************************************************************************
  \brief Deletes VAO and SSBO associated with the mesh.
  \details
	Frees GPU resources to prevent memory leaks.
*************************************************************************/
void GGMesh::Cleanup() {
	glDeleteVertexArrays(1, &vaoid);
	if (instanceSSBO) {
		glDeleteBuffers(1, &instanceSSBO);
		instanceSSBO = 0;
	}
	if (pickingInstanceSSBO) {
		glDeleteBuffers(1, &pickingInstanceSSBO);
		pickingInstanceSSBO = 0;
	}
}

/*************************************************************************
  \brief Creates the mesh with vertices and indices.
  \param vertices Vector of GGVertex structs.
  \param indices Vector of GLushort indices.
  \param type OpenGL primitive type (default GL_TRIANGLES).
  \param uUseInstancing Whether to create SSBO for instancing.
  \details
	Sets up VBO, VAO, EBO. Configures vertex attributes for position,
	color, and UVs. Prepares SSBO if instancing is enabled.
*************************************************************************/
void GGMesh::CreateMesh(std::vector<GGVertex> const& vertices, std::vector<GLushort> const& indices, GLenum type,bool uUseInstancing) {
	GLuint vbo{}, ebo{};

	glCreateBuffers(1, &vbo);
	glNamedBufferStorage(vbo, vertices.size() * sizeof(GGVertex), vertices.data(), 0);
	  
	glCreateVertexArrays(1, &vaoid);

	glVertexArrayVertexBuffer(vaoid, 0, vbo, 0, sizeof(GGVertex));

	glEnableVertexArrayAttrib(vaoid, 0);
	glVertexArrayAttribFormat(vaoid, 0, 3, GL_FLOAT, GL_FALSE, offsetof(GGVertex, position));
	glVertexArrayAttribBinding(vaoid, 0, 0);
	
	glEnableVertexArrayAttrib(vaoid, 1);
	glVertexArrayAttribFormat(vaoid, 1, 4, GL_FLOAT, GL_FALSE, offsetof(GGVertex, color));
	glVertexArrayAttribBinding(vaoid, 1, 0);

	glEnableVertexArrayAttrib(vaoid, 2);
	glVertexArrayAttribFormat(vaoid, 2, 2, GL_FLOAT, GL_FALSE, offsetof(GGVertex, uv));
	glVertexArrayAttribBinding(vaoid, 2, 0);

	glCreateBuffers(1, &ebo);
	glNamedBufferStorage(ebo, sizeof(GLushort) * indices.size(), reinterpret_cast<const GLvoid*>(indices.data()), GL_DYNAMIC_STORAGE_BIT);
	glVertexArrayElementBuffer(vaoid, ebo);

	if (uUseInstancing) {
		glGenBuffers(1, &instanceSSBO);
	}
	glBindVertexArray(0);

	primitiveType = type;
	drawCount = static_cast<GLuint>(indices.size());
}

