#include "RayTest.h"

RayTest::RayTest(int maxSpp, bool limitSpp):
	MAX_SPP(maxSpp), limitSpp(limitSpp)
{
	screenVB.allocate(sizeof(SCREEN_COORD), SCREEN_COORD, 6);
	screenVA.addBuffer(screenVB, LAYOUT_POS2);

	rayTestShader.load("res/shader/rayTest.shader");
	postShader.load("res/shader/postEffects.shader");
	postShader.set1f("gamma", 2.2f);
	glfwSetInputMode(this->window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

RayTest::~RayTest()
{
}

void RayTest::init()
{
	for (int i = 0; i < 2; i++)
	{
		frame[i].generate(this->windowWidth(), this->windowHeight());
		frameTex[i].generate2D(this->windowWidth(), this->windowHeight(), GL_RGB32F);
		frameTex[i].setFilterAndWrapping(GL_LINEAR, GL_CLAMP_TO_EDGE);
		frame[i].attachTexture(GL_COLOR_ATTACHMENT0, frameTex[i]);
		frame[i].activateAttachmentTargets({ GL_COLOR_ATTACHMENT0 });
	}

	skybox.loadHDRPanorama("res/texture/084.hdr");
	camera.setFOV(90.0f);

	scene.addMaterial(Material{ glm::vec3(1.0f), 0.0f, 1.0f });
	scene.addMaterial(Material{ glm::vec3(1.0f, 0.71f, 0.29f), 1.0f, 0.25f });

	auto teapot = std::make_shared<Model>("res/model/teapot20.obj", 1, glm::vec3(0.0f));
	auto quad = std::make_shared<Model>("res/model/square.obj", 0, glm::vec3(0.0f), 100.0f);
	scene.addObject(teapot);
	scene.addObject(quad);

	sceneBuffers = scene.genBuffers();

	boxCount = sceneBuffers.bound->size() / sizeof(AABB);
	vertexCount = sceneBuffers.vertex->size() / sizeof(glm::vec3);
	triangleCount = sceneBuffers.index->size() / sizeof(uint32_t) / 3;

	vertexBuffer.generateTexBuffer(*sceneBuffers.vertex, GL_RGB32F);
	normalBuffer.generateTexBuffer(*sceneBuffers.normal, GL_RGB32F);
	indexBuffer.generateTexBuffer(*sceneBuffers.index, GL_R32I);
	boundBuffer.generateTexBuffer(*sceneBuffers.bound, GL_RGB32F);
	sizeIndexBuffer.generateTexBuffer(*sceneBuffers.sizeIndex, GL_R32I);

	rayTestShader.setTexture("vertices", vertexBuffer, 1);
	rayTestShader.setTexture("normals", normalBuffer, 2);
	rayTestShader.setTexture("indices", indexBuffer, 3);
	rayTestShader.setTexture("bounds", boundBuffer, 4);
	rayTestShader.setTexture("sizeIndices", sizeIndexBuffer, 5);
	rayTestShader.setTexture("skybox", skybox, 6);
	rayTestShader.set1f("bvhDepth", glm::log2((float)boxCount));

	materials = scene.materialSet();

	camera.setPos({ 0.0f, -5.0f, 2.0f });
	camera.setFOV(85.0f);
	camera.setAspect((float)this->windowWidth() / this->windowHeight());

	this->setupGUI();
	this->reset();
}

void RayTest::renderLoop()
{
	this->processKey(0, 0, 0, 0);
	VerticalSyncStatus(verticalSync);

	renderer.clear(0.0f, 0.0f, 0.0f);
	this->renderFrame();

	if (curSpp < MAX_SPP || !limitSpp)
	{
		curSpp++;
		curFrame ^= 1;
	}

	this->renderGUI();

	this->swapBuffers();
	this->display();
}

void RayTest::terminate()
{
}

void RayTest::processKey(int key, int scancode, int action, int mode)
{
	if (this->getKeyStatus(GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		this->setTerminateStatus(true);
	}

	const int keys[] = { GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D,
		GLFW_KEY_Q, GLFW_KEY_E, GLFW_KEY_R, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_SPACE };

	for (auto pressedKey : keys)
	{
		if (this->getKeyStatus(pressedKey) == GLFW_PRESS)
		{
			camera.move(pressedKey);
			reset();
		}
	}

	if (this->getKeyStatus(GLFW_KEY_F1) == GLFW_PRESS) F1Pressed = true;
	if (this->getKeyStatus(GLFW_KEY_F1) == GLFW_RELEASE)
	{
		if (F1Pressed)
		{
			if (cursorDisabled)
			{
				glfwSetInputMode(this->window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				firstCursorMove = true;
			}
			else glfwSetInputMode(this->window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			cursorDisabled ^= 1;
			F1Pressed = false;
		}
	}
}

void RayTest::processCursor(float posX, float posY)
{
	if (!cursorDisabled) return;
	if (firstCursorMove == 1)
	{
		lastCursorX = posX;
		lastCursorY = posY;
		firstCursorMove = false;
		return;
	}

	float offsetX = (posX - lastCursorX) * CAMERA_ROTATE_SENSITIVITY;
	float offsetY = (posY - lastCursorY) * CAMERA_ROTATE_SENSITIVITY;
	glm::vec3 offset = glm::vec3(-offsetX, -offsetY, 0);
	camera.rotate(offset);

	lastCursorX = posX;
	lastCursorY = posY;
	reset();
}

void RayTest::processScroll(float offsetX, float offsetY)
{
}

void RayTest::processResize(int width, int height)
{
}

void RayTest::renderFrame()
{
	frame[curFrame].bind();
	renderer.clear(0.0f, 0.0f, 0.0f);

	glm::vec3 F = camera.front();
	glm::vec3 U = camera.up();
	glm::vec3 R = camera.right();

	rayTestShader.setTexture("lastFrame", frameTex[curFrame ^ 1], 0);
	rayTestShader.set1i("showBVH", showBVH);

	rayTestShader.setVec3("camF", F);
	rayTestShader.setVec3("camR", R);
	rayTestShader.setVec3("camU", U);
	rayTestShader.setVec3("camPos", camera.pos());
	rayTestShader.set1f("tanFOV", glm::tan(glm::radians(camera.FOV() * 0.5f)));
	rayTestShader.set1f("camAsp", (float)this->windowWidth() / (float)this->windowHeight());
	//rayTestShader.set1i("boxIndex", boxIndex);

	rayTestShader.set1i("spp", curSpp);

	renderer.draw(screenVA, rayTestShader);
	frame[curFrame].unbind();

	renderer.clear(0.0f, 0.0f, 0.0f);
	postShader.setTexture("frameBuffer", frameTex[curFrame], 0);

	renderer.draw(screenVA, postShader);
}

void RayTest::reset()
{
	curSpp = 0;
	for (int i = 0; i < materials.size(); i++)
	{
		rayTestShader.setVec3(("materials[" + std::to_string(i) + "].albedo").c_str(), materials[i].albedo);
		rayTestShader.set1f(("materials[" + std::to_string(i) + "].metallic").c_str(), materials[i].metallic);
		rayTestShader.set1f(("materials[" + std::to_string(i) + "].roughness").c_str(), materials[i].roughness);
	}
}

void RayTest::setupGUI()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsClassic();
	ImGui_ImplGlfw_InitForOpenGL(this->window(), true);
	ImGui_ImplOpenGL3_Init("#version 450");
}

void RayTest::renderGUI()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("RayTest");
	{
		ImGui::Text("BVH Nodes: %d\nTriangles: %d\nVertices: %d", boxCount, triangleCount, vertexCount);
		if (ImGui::Checkbox("Show BVH", &showBVH)) reset();
		
		if (materials.size() > 0)
		{
			ImGui::SliderInt("Material", &materialIndex, 0, materials.size() - 1);
			auto& m = materials[materialIndex];
			if (
				ImGui::ColorEdit3("Albedo", glm::value_ptr(m.albedo)) ||
				ImGui::SliderFloat("Metallic", &m.metallic, 0.0f, 1.0f) ||
				ImGui::SliderFloat("Roughness", &m.roughness, 0.001f, 1.0f))
			{
				reset();
			}
		}

		ImGui::Checkbox("Vertical Sync", &verticalSync);
		ImGui::Text("SPP: %4d/%4d", curSpp, MAX_SPP);
		ImGui::Text("x: %.3f y: %.3f z: %.3f  FOV: %.1f", camera.pos().x, camera.pos().y, camera.pos().z, camera.FOV());
		ImGui::Text("Render Time: %.3f ms, FPS: %.3f", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Text("\n");
		ImGui::Text("WASD / Lshift / Space  - move");
		ImGui::Text("Mouse                  - view");
		ImGui::Text("F1                     - release mouse");
		ImGui::Text("ESC                    - exit");
		if (ImGui::Button("Exit"))
		{
			this->setTerminateStatus(true);
		}
	}
	ImGui::End();

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
