#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"
#include "Mesh.h"
#include "Material.h"

class ModelInstance;
using ModelInstancePtr = std::shared_ptr<ModelInstance>;

class ModelInstance
{
public:
	friend class Resource;

	ModelInstance() = default;
	~ModelInstance();

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
	void setName(const std::string& name) { mName = name; }
	void setPath(const File::path& path) { mPath = path; }

	glm::vec3 pos() const { return mPos; }
	glm::vec3 scale() const { return mScale; }
	glm::vec3 rotation() const { return mRotation; }
	glm::mat4 modelMatrix() const;
	std::string name() const { return mName; }
	File::path path() const { return mPath; }

	std::vector<MeshInstancePtr>& meshInstances() { return mMeshInstances; }
	std::vector<Material>& materials() { return mMaterials; }

	ModelInstancePtr copy() const;

private:
	std::vector<MeshInstancePtr> mMeshInstances;
	std::vector<Material> mMaterials;
	glm::vec3 mPos = glm::vec3(0.0f);
	glm::vec3 mScale = glm::vec3(1.0f);
	glm::vec3 mRotation = glm::vec3(0.0f);
	glm::mat4 mRotMatrix = glm::mat4(1.0f);

	std::string mName;
	File::path mPath;
};