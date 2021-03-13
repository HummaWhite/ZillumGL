#pragma once

#include "Accelerator/BVH.h"
#include "Texture.h"
#include "Model.h"

struct SceneBuffer
{
	std::shared_ptr<BufferTexture> vertex;
	std::shared_ptr<BufferTexture> normal;
	std::shared_ptr<BufferTexture> texCoord;
	std::shared_ptr<BufferTexture> index;
	std::shared_ptr<BufferTexture> bound;
	std::shared_ptr<BufferTexture> sizeIndex;
	std::shared_ptr<BufferTexture> matIndex;
};

struct Material
{
	enum { MetalWorkflow = 0, Dielectric };

	Material(const glm::vec3& albedo, float metallic, float roughness) :
		albedo(albedo), metallic(metallic), roughness(roughness), type(MetalWorkflow) {}

	Material(const glm::vec3& albedo, float ior, float roughness, int nul) :
		albedo(albedo), ior(ior), roughness(roughness), type(Dielectric) {}

	glm::vec3 albedo;
	float metallic;
	float roughness;
	int type;
	float ior;
};

struct Light
{
	glm::vec3 pa;
	glm::vec3 pb;
	glm::vec3 pc;
	glm::vec3 radiosity;
};

class Scene
{
public:
	SceneBuffer genBuffers();

	void addObject(std::shared_ptr<Model> object);
	void addMaterial(const Material& material);
	void addLight(const Light& light);
	void addLight(const glm::vec3& pa, const glm::vec3& pb, const glm::vec3& pc, const glm::vec3& radiosity);

	void setBVHBuildMethod(BVHSplitMethod method);

	std::vector<Material> materialSet() const { return materials; }
	std::vector<Light> lightSet() const { return lights; }

private:
	std::vector<std::shared_ptr<Model>> objects;
	std::vector<Material> materials;
	std::vector<Light> lights;
	BVHSplitMethod bvhBuildMethod = BVHSplitMethod::SAH;
};