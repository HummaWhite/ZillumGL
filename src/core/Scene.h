#pragma once

#include "Accelerator/BVH.h"
#include "EnvironmentMap.h"
#include "Texture.h"
#include "Model.h"
#include "Resource.h"
#include "Math/AliasTable.h"

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

	void addObject(ModelInstancePtr object);
	void addMaterial(const Material& material);
	void addLight(ModelInstancePtr light, const glm::vec3& radiance);

	int numLights() const { return lights.size(); }

public:
	std::vector<ModelInstancePtr> objects;
	std::vector<std::pair<ModelInstancePtr, glm::vec3>> lights;
	std::vector<Material> materials;
	
	EnvironmentMapPtr envMap;
	bool sampleLight = true;
	bool lightEnvUniformSample = false;
	float envStrength = 0.5f;
	float envRotation = 0.0f;
	float lightSumPdf = 0.0f;

	int objPrimCount = 0;

	BVHSplitMethod bvhBuildMethod = BVHSplitMethod::SAH;
};