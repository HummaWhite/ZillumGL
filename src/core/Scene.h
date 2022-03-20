#pragma once

#include "../accelerator/BVH.h"
#include "../math/AliasTable.h"
#include "EnvironmentMap.h"
#include "Texture.h"
#include "Model.h"
#include "Resource.h"
#include "Camera.h"
#include "Sampler.h"

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
	void load(const File::path& path);
	void saveToFile(const File::path& path);

	void createGLContext();
	void clear();

	void addObject(ModelInstancePtr object);
	void addMaterial(const Material& material);
	void addLight(ModelInstancePtr light, const glm::vec3& radiance);

public:
	std::vector<ModelInstancePtr> objects;
	std::vector<std::pair<ModelInstancePtr, glm::vec3>> lights;
	std::vector<Material> materials;
	
	EnvironmentMapPtr envMap;
	float lightSumPdf = 0.0f;
	int nLightTriangles = 0;
	int objPrimCount = 0;

	SceneGLContext glContext;
	int vertexCount;
	int triangleCount;
	int boxCount;

	Camera camera;
	
	int filmWidth, filmHeight;

	int sampler;
	const int SampleNum = 131072;
	const int SampleDim = 256;
	TextureBufferedPtr sobolTex;
	Texture2DPtr noiseTex;

	bool aoMode;
	glm::vec3 aoCoef = glm::vec3(1.0f);

	float envRotation = 0.0f;
};