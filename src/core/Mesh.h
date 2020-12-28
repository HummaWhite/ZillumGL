#pragma once

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "VertexArray.h"
#include "Shader.h"
#include "Renderer.h"
#include "Shape.h"

class Mesh
{
public:
	struct POS3_UV2_NORM3
	{
		glm::vec3 pos;
		glm::vec2 uv;
		glm::vec3 norm;
	};

	struct POS3_UV2_NORM3_TAN3
	{
		glm::vec3 pos;
		glm::vec2 uv;
		glm::vec3 norm;
		glm::vec3 tan;
	};

	struct POS3_UV2_NORM3_TAN3_BTN3
	{
		glm::vec3 pos;
		glm::vec2 uv;
		glm::vec3 norm;
		glm::vec3 tan;
		glm::vec3 btn;
	};

	struct TextureMesh
	{
		unsigned int ID;
		std::string type;
	};

	void loadMesh(const void* data, int count, std::vector<GLuint> indices, BufferLayout layout, bool batchedData = false);
	void loadShape(Shape& shape);
	void addTexture(TextureMesh* tex);
	void draw(Shader& shader);

private:
	VertexArray m_VA;
	Buffer m_VB, m_EB;
	std::vector<TextureMesh*> m_Textures;
};