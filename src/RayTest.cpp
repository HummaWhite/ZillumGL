#include "RayTest.h"

RayTest::RayTest(int maxSpp, bool limitSpp):
	MAX_SPP(maxSpp), limitSpp(limitSpp)
{
	screenVB.allocate(sizeof(SCREEN_COORD), SCREEN_COORD, 6);
	screenVA.addBuffer(screenVB, LAYOUT_POS2);

	rayTestShader.load("res/shader/rayTest.shader");
	rayTestShader.set1i("maxSpp", MAX_SPP);
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

	skybox.loadSphere("res/texture/090.hdr");
	camera.setFOV(90.0f);

	this->setupGUI();
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
			curSpp = 0;
		}
	}

	if (this->getKeyStatus(GLFW_KEY_F1) == GLFW_PRESS) F1Pressed = true;
	if (this->getKeyStatus(GLFW_KEY_F1) == GLFW_RELEASE)
	{
		if (F1Pressed)
		{
			if (cursorDisabled) glfwSetInputMode(this->window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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
	curSpp = 0;
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

	glm::vec3 F = camera.pointing();
	glm::vec3 U = glm::vec3(0.0f, 0.0f, 1.0f);
	glm::vec3 R = glm::normalize(glm::cross(F, U));
	U = glm::normalize(glm::cross(R, F));

	rayTestShader.resetTextureMap();
	rayTestShader.setTexture("lastFrame", frameTex[curFrame ^ 1]);

	rayTestShader.setVec3("camF", F);
	rayTestShader.setVec3("camR", R);
	rayTestShader.setVec3("camU", U);
	rayTestShader.setVec3("camPos", camera.pos());
	rayTestShader.set1f("tanFOV", glm::tan(glm::radians(camera.FOV() * 0.5f)));
	rayTestShader.set1f("camAsp", (float)this->windowWidth() / (float)this->windowHeight());

	rayTestShader.set1i("sphereCount", spheres.size());
	const std::string tmp("sphereList[");
	for (int i = 0; i < spheres.size(); i++)
	{
		rayTestShader.setVec3((tmp + std::to_string(i) + "].center").c_str(), spheres[i].center);
		rayTestShader.set1f((tmp + std::to_string(i) + "].radius").c_str(), spheres[i].radius);
		rayTestShader.setVec3((tmp + std::to_string(i) + "].albedo").c_str(), spheres[i].albedo);
		rayTestShader.set1f((tmp + std::to_string(i) + "].metallic").c_str(), spheres[i].metallic);
		rayTestShader.set1f((tmp + std::to_string(i) + "].roughness").c_str(), spheres[i].roughness);
	}

	rayTestShader.setTexture("skybox", skybox.texture());
	rayTestShader.set1i("spp", curSpp);

	renderer.draw(screenVA, rayTestShader);
	frame[curFrame].unbind();

	renderer.clear(0.0f, 0.0f, 0.0f);
	postShader.resetTextureMap();
	postShader.setTexture("frameBuffer", frameTex[curFrame]);

	renderer.draw(screenVA, postShader);
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

	ImGui::Begin("Objects");
	{
		if (spheres.size() > 0)
		{
			ImGui::SliderInt("Select", &sphereIndex, 0, spheres.size() - 1);
			auto& sp = spheres[sphereIndex];
			if (ImGui::DragFloat3("Center", glm::value_ptr(sp.center), 0.1f) ||
				ImGui::DragFloat("Radius", &sp.radius, 0.01f, 0.0f) ||
				ImGui::ColorEdit3("Albedo", glm::value_ptr(sp.albedo)) ||
				ImGui::SliderFloat("Metallic", &sp.metallic, 0.0f, 1.0f) ||
				ImGui::SliderFloat("Roughness", &sp.roughness, 0.001f, 1.0f))
			{
				curSpp = 0;
			}
		}
		if (ImGui::Button("Add"))
		{
			spheres.push_back(TestSphere());
			sphereIndex = spheres.size() - 1;
			curSpp = 0;
		}
		if (ImGui::Button("Remove"))
		{
			auto iter = spheres.begin() + sphereIndex;
			spheres.erase(iter);
			sphereIndex = 0;
			curSpp = 0;
		}
	}
	ImGui::End();

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
