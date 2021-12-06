#pragma once

#include "Accelerator/BVH.h"
#include "EnvironmentMap.h"
#include "Texture.h"
#include "Model.h"
#include "Resource.h"
#include "Camera.h"
#include "Math/AliasTable.h"
#include "Sampler.h"

const std::string SceneConfigPath = "res/scene.xml";

struct SceneGLContext
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
	void load();
	void saveToFile(const File::path& path);

	void createGLContext();

	void addObject(ModelInstancePtr object);
	void addMaterial(const Material& material);
	void addLight(ModelInstancePtr light, const glm::vec3& radiance);

public:
	std::vector<ModelInstancePtr> objects;
	std::vector<std::pair<ModelInstancePtr, glm::vec3>> lights;
	std::vector<Material> materials;
	
	EnvironmentMapPtr envMap;
	bool sampleLight = true;
	bool lightEnvUniformSample = false;
	float lightPortion = 0.5f;
	float envRotation = 0.0f;
	float lightSumPdf = 0.0f;
	int nLightTriangles = 0;
	int objPrimCount = 0;

	Camera camera;
	float focalDist;
	float lensRadius;
	int toneMapping = 2;
	
	int filmWidth, filmHeight;

	int sampler;
	const int SampleNum = 131072;
	const int SampleDim = 256;
	TextureBufferedPtr sobolTex;
	Texture2DPtr noiseTex;

	bool aoMode;
	int maxBounce;
	glm::vec3 aoCoef = glm::vec3(1.0f);

	SceneGLContext glContext;
};