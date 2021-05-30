#pragma once

#include <iostream>
#include <memory>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Texture.h"

struct Material
{
	enum { MetalWorkflow = 0, Dielectric };

	Material() : Material(glm::vec3(1.0f)) {}

	Material(const glm::vec3& albedo) :
		albedo(albedo), metIor(0.0f), roughness(1.0f), type(MetalWorkflow) {}

	Material(const glm::vec3& albedo, float metIor, float roughness, int type) :
		albedo(albedo), metIor(metIor), roughness(roughness), type(type) {}

	glm::vec3 albedo;
	float metIor;
	float roughness;
	int type;
};

struct Mesh
{
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec2> texCoords;
	std::vector<uint32_t> indices;
	uint32_t matIndex;
};

class Model
{
public:
	Model(const char* filePath, const glm::vec3& pos = glm::vec3(0.0f), const glm::vec3& scale = glm::vec3(1.0f), const glm::vec3& rotation = glm::vec3(0.0f));
	~Model();

	bool loadModel(const char* filePath);

	std::vector<Mesh> meshes() const { return m_Meshes; }
	std::vector<Material> materials() const { return m_Materials; }

	void setPos(glm::vec3 pos);
	void setPos(float x, float y, float z);
	void move(glm::vec3 vec);
	void rotateObjectSpace(float angle, glm::vec3 axis);
	void rotateWorldSpace(float abgle, glm::vec3 axis);
	void setScale(glm::vec3 scale);
	void setScale(float xScale, float yScale, float zScale);
	void setRotation(glm::vec3 angle);
	void setRotation(float yaw, float pitch, float roll);
	void setSize(float size);

	glm::vec3 pos() const { return m_Pos; }
	glm::vec3 scale() const { return m_Scale; }
	glm::vec3 rotation() const { return m_Rotation; }
	glm::mat4 modelMatrix() const;
	std::string name() const { return m_Name; }

private:
	void processNode(aiNode* node, const aiScene* scene);
	Mesh processMesh(aiMesh* mesh);

private:
	std::vector<Mesh> m_Meshes;
	std::vector<Material> m_Materials;
	glm::vec3 m_Pos = glm::vec3(0.0f);
	glm::vec3 m_Scale = glm::vec3(1.0f);
	glm::vec3 m_Rotation = glm::vec3(0.0f);
	glm::mat4 m_RotMatrix = glm::mat4(1.0f);
	bool m_LoadedFromFile = false;
	std::string m_Name;

	static glm::mat4 constRot;
};