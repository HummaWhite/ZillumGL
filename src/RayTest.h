#pragma once

#include "Includer.h"

class RayTest :
	public EngineBase
{
public:
	RayTest();
	~RayTest();

private:
	void init();
	void renderLoop();
	void terminate();

	void processKey(int key, int scancode, int action, int mode);
	void processCursor(float posX, float posY);
	void processScroll(float offsetX, float offsetY);
	void processResize(int width, int height);

	void renderFrame();
	void reset();

	void setupGUI();
	void renderGUI();

private:
	PipelinePtr pipeline;

	VertexBufferPtr screenVB;

	ShaderPtr rayTestShader, postShader;

	Camera camera = Camera({ 0.0f, -3.0f, 2.0f });

	Texture2DPtr frameTex;
	int curFrame = 0;
	int freeCounter = 0;

	int maxSpp = 64;
	int curSpp = 0;
	bool limitSpp = false;

	bool verticalSync = false;

	Scene scene;
	SceneBuffer sceneBuffers;
	
	int materialIndex = 0;

	std::vector<std::string> envList;
	std::string envStr;

	int boxIndex = 0;
	int boxCount;
	int triangleCount;
	int vertexCount;

	int toneMapping = 2;
	bool showBVH = true;
	bool cursorDisabled = false;
	bool firstCursorMove = true;
	int lastCursorX = 0;
	int lastCursorY = 0;
	bool F1Pressed = false;
	bool aoMode = false;
	glm::vec3 aoCoef = glm::vec3(1.0f);
	int maxBounce = 3;
	bool roulette = false;
	float rouletteProb = 0.6f;
	int envMapIndex;
	bool pause = false;

	int tileSize = 2048;

	float focalDist = 1.0f;
	float lensRadius = 0.0f;

	const int sampleNum = 131072;
	const int sampleDim = 256;
	int sampler = 0;
	TextureBufferedPtr haltonTex;
	TextureBufferedPtr sobolTex;
	Texture2DPtr noiseTex;

	int workgroupSizeX = 32;
	int workgroupSizeY = 48;
};