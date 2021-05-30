#pragma once

#include "Includer.h"

class RayTest :
	public EngineBase
{
public:
	RayTest(int maxSpp, bool limitSpp);
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
	Buffer screenVB;
	VertexArray screenVA;

	Shader rayTestShader, postShader;

	Camera camera = Camera({ 0.0f, -3.0f, 2.0f });

	FrameBuffer frame[2];
	Texture frameTex[2];
	int curFrame = 0;
	int freeCounter = 0;

	const int MAX_SPP;
	int curSpp = 0;
	bool limitSpp;

	bool verticalSync = false;

	Scene scene;
	SceneBuffer sceneBuffers;
	
	int materialIndex = 0;

	Texture albedoMap;
	Texture matallicMap;
	Texture roughnessMap;
	Texture normalMap;

	std::vector<std::string> envList;
	std::string envStr;

	int boxIndex = 0;
	int boxCount;
	int triangleCount;
	int vertexCount;

	int toneMapping = 2;
	bool showBVH = false;
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
};