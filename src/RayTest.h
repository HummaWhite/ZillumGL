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

	Texture skybox;

	const int MAX_SPP;
	int curSpp = 0;
	bool limitSpp;

	bool verticalSync = true;

	Scene scene;
	SceneBuffer sceneBuffers;
	Texture vertexBuffer;
	Texture normalBuffer;
	Texture indexBuffer;
	Texture boundBuffer;
	Texture sizeIndexBuffer;
	//Texture materialBuffer;
	std::vector<Material> materials;
	int materialIndex = 0;

	int boxIndex = 0;
	int boxCount;
	int triangleCount;
	int vertexCount;

	bool showBVH = false;
	bool cursorDisabled = false;
	bool firstCursorMove = true;
	int lastCursorX = 0;
	int lastCursorY = 0;
	bool F1Pressed = false;
};