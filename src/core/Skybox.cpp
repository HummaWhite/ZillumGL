#include "Skybox.h"
#include "Renderer.h"

Shader Skybox::m_Shader;

Skybox::~Skybox()
{
	if (m_Shape != nullptr) delete m_Shape;
}

void Skybox::loadSphere(const char* filePath)
{
	m_Type = SPHERE;
	m_SkyboxTex = new Texture;
	m_SkyboxTex->loadFloat(filePath);
	m_SkyboxTex->setFilterAndWrapping(GL_LINEAR, GL_CLAMP_TO_EDGE);

	m_Shader.load("res/shader/skyboxSphere.shader");
	m_Shader.setTexture("sky", *m_SkyboxTex);
	m_Shape = new Sphere(30, 15, 1.0f, Shape::VERTEX);
	m_Shape->setupVA();
}

void Skybox::setProjection(const glm::mat4& projMatrix)
{
	m_Shader.setMat4("proj", projMatrix);
}

void Skybox::setView(const glm::mat4& viewMatrix)
{
	m_Shader.setMat4("view", viewMatrix);
}

void Skybox::draw()
{
	m_Shader.setTexture("sky", *m_SkyboxTex);
	renderer.draw(m_Shape->VA(), m_Shader);
}

void Skybox::draw(const Texture& tex)
{
	m_Shader.setTexture("sky", tex);
	renderer.draw(m_Shape->VA(), m_Shader);
}
