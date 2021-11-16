#pragma once

#include "Accelerator/BVH.h"
#include "EnvironmentMap.h"
#include "Texture.h"
#include "Model.h"

struct SceneBuffer
{
	TextureBufferedPtr vertex;
	TextureBufferedPtr normal;
	TextureBufferedPtr texCoord;
	TextureBufferedPtr index;
	TextureBufferedPtr bound;
	TextureBufferedPtr hitTable;
	TextureBufferedPtr matTexIndex;
	TextureBufferedPtr material;
	TextureBufferedPtr lightPower;
	TextureBufferedPtr lightAlias;
	TextureBufferedPtr lightProb;
	Texture2DArrayPtr textures;
	TextureBufferedPtr texUVScale;
};

class Scene
{
public:
	SceneBuffer genBuffers();

	void addObject(std::shared_ptr<Model> object, uint8_t matIndex);
	void addMaterial(const Material& material);
	void addLight(std::shared_ptr<Model> light, const glm::vec3& radiance);

	template<typename... Args>
	void addOneObject(Args &&...args)
	{
		addObject(std::make_shared<Model>(args...), 0);
	}

	template<typename... Args>
	void addOneLight(Args &&...args, const glm::vec3& radiance)
	{
		addLight(std::make_shared<Model>(args...), radiance);
	}

private:
	std::tuple<std::vector<glm::vec3>, std::vector<int32_t>, std::vector<float>> genLightTable();

public:
	std::vector<std::pair<std::shared_ptr<Model>, uint8_t>> objects;
	std::vector<std::pair<std::shared_ptr<Model>, glm::vec3>> lights;
	std::vector<Material> materials;
	
	EnvironmentMapPtr envMap;
	bool sampleLight = true;
	bool lightEnvUniformSample = false;
	float envStrength = 0.5f;
	float envRotation = 0.0f;
	float lightSum = 0.0f;
	int nLights;

	int objPrimCount = 0;

	BVHSplitMethod bvhBuildMethod = BVHSplitMethod::SAH;
};