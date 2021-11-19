#pragma once

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "VertexArray.h"
#include "Shader.h"
#include "Pipeline.h"
#include "../util/Error.h"

class MeshData;
class MeshInstance;

using MeshDataPtr = std::shared_ptr<MeshData>;
using MeshInstancePtr = std::shared_ptr<MeshInstance>;

class MeshData
{
private:
	struct VertexType
	{
		glm::vec3 pos;
		glm::vec2 tex;
		glm::vec3 norm;
	};

public:
	MeshData() = default;

	void createGLContext();
	GLuint bufferId() const;
	bool hasGLContext() const { return mHasGLContext; }

	void render(PipelinePtr ctx, ShaderPtr shader);

	static MeshDataPtr create();

public:
	std::vector<glm::vec3> positions;
	std::vector<glm::vec2> texcoords;
	std::vector<glm::vec3> normals;
	std::vector<uint32_t> indices;

private:
	VertexBufferPtr mVertexBuffer;
	VertexBufferPtr mIndexBuffer;

	bool mHasGLContext = false;
};

class MeshInstance
{
public:
	MeshInstance() = default;

	static MeshInstancePtr create();

public:
	int texIndex = -1;
	int matIndex = 0;

	MeshDataPtr meshData;
};