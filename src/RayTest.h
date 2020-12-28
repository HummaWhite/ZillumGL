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

	void setupGUI();
	void renderGUI();

	struct TestSphere
	{
		glm::vec3 center = glm::vec3(0.0f);
		float radius = 1.0f;
		glm::vec3 albedo = glm::vec3(1.0f);
		float metallic = 0.0f;
		float roughness = 1.0f;
	};

private:
	Buffer screenVB;
	VertexArray screenVA;

	Shader rayTestShader, postShader;

	Camera camera = Camera({ 0.0f, -3.0f, 2.0f });

	FrameBuffer frame[2];
	Texture frameTex[2];
	int curFrame = 0;

	Skybox skybox;

	const int MAX_SPP;
	int curSpp = 0;
	bool limitSpp;

	bool verticalSync = true;

	std::vector<TestSphere> spheres;
	int sphereIndex = 0;

	bool cursorDisabled = false;
	bool firstCursorMove = true;
	int lastCursorX = 0;
	int lastCursorY = 0;
	bool F1Pressed = false;
};