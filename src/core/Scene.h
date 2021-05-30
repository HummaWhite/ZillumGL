#pragma once

#include "Accelerator/BVH.h"
#include "EnvironmentMap.h"
#include "Texture.h"
#include "Model.h"

struct SceneBuffer
{
	std::shared_ptr<BufferTexture> vertex;
	std::shared_ptr<BufferTexture> normal;
	std::shared_ptr<BufferTexture> texCoord;
	std::shared_ptr<BufferTexture> index;
	std::shared_ptr<BufferTexture> bound;
	std::shared_ptr<BufferTexture> hitTable;
	std::shared_ptr<BufferTexture> matIndex;
	std::shared_ptr<BufferTexture> material;
	std::shared_ptr<BufferTexture> lightPower;
	std::shared_ptr<BufferTexture> lightAlias;
	std::shared_ptr<BufferTexture> lightProb;
};

class Scene
{
public:
	SceneBuffer genBuffers();

	void addObject(std::shared_ptr<Model> object, uint8_t matIndex);
	void addMaterial(const Material& material);
	void addLight(std::shared_ptr<Model> light, const glm::vec3& radiance);

private:
	std::tuple<std::vector<glm::vec3>, std::vector<int32_t>, std::vector<float>> genLightTable();

public:
	std::vector<std::pair<std::shared_ptr<Model>, uint8_t>> objects;
	std::vector<std::pair<std::shared_ptr<Model>, glm::vec3>> lights;
	std::vector<Material> materials;

	std::shared_ptr<EnvironmentMap> envMap;
	bool sampleLight = false;
	bool lightEnvUniformSample = false;
	float envStrength = 1.0f;
	float lightSum = 0.0f;
	int nLights;

	int objPrimCount = 0;

	BVHSplitMethod bvhBuildMethod = BVHSplitMethod::SAH;
};