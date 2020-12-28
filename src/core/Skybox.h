#pragma once

#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Texture.h"
#include "Shader.h"
#include "Shape.h"

class Skybox
{
public:
	Skybox() : m_Type(0), m_Shape(nullptr) {}
	~Skybox();

	enum
	{
		CUBE = 0,
		SPHERE
	};

	void loadSphere(const char* filePath);
	void loadCube(std::vector<std::string> filePaths, GLuint colorType = GL_SRGB);
	void setProjection(const glm::mat4& projMatrix);
	void setView(const glm::mat4& viewMatrix);
	void draw();
	void draw(const Texture& tex);

	Texture& texture() { return *m_SkyboxTex; }

	static Shader m_Shader;

private:
	int m_Type;
	Shape* m_Shape;
	Texture* m_SkyboxTex;
};
