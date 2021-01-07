#pragma once

#include "Accelerator/BVH.h"
#include "Buffer.h"
#include "Model.h"

struct SceneBuffer
{
	std::shared_ptr<Buffer> vertex;
	std::shared_ptr<Buffer> normal;
	std::shared_ptr<Buffer> index;
	std::shared_ptr<Buffer> bound;
	std::shared_ptr<Buffer> sizeIndex;
	std::shared_ptr<Buffer> material;
};

struct Material
{
	glm::vec3 albedo;
	float metallic;
	float roughness;
};

class Scene
{
public:
	SceneBuffer genBuffers();

	void addObject(std::shared_ptr<Model> object);
	void addMaterial(const Material& material);

	std::vector<Material> materialSet() const { return materials; }

private:
	std::vector<std::shared_ptr<Model>> objects;
	std::vector<Material> materials;
};