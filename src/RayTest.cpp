#include "RayTest.h"

RayTest::RayTest(int maxSpp, bool limitSpp):
	MAX_SPP(maxSpp), limitSpp(limitSpp)
{
	glfwSetInputMode(this->window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	namespace fs = std::filesystem;
	fs::path envMapDir("res/texture/");

	fs::directory_iterator list(envMapDir);
	
	for (const auto& it : list)
	{
		std::string str = it.path().generic_u8string();
		if (str.find(".hdr") != str.npos)
		{
			envList.push_back(str);
			envStr += str + '\0';
		}
	}
}

RayTest::~RayTest()
{
}

void RayTest::init()
{
	screenVB.allocate(sizeof(SCREEN_COORD), SCREEN_COORD, 6);
	screenVA.addBuffer(screenVB, LAYOUT_POS2);

	rayTestShader.load("rayTest.shader");
	postShader.load("postEffects.shader");

	for (int i = 0; i < 2; i++)
	{
		frame[i].generate(this->windowWidth(), this->windowHeight());
		frameTex[i].generate2D(this->windowWidth(), this->windowHeight(), GL_RGB32F);
		frameTex[i].setFilterAndWrapping(GL_LINEAR, GL_CLAMP_TO_EDGE);
		frame[i].attachTexture(GL_COLOR_ATTACHMENT0, frameTex[i]);
		frame[i].activateAttachmentTargets({ GL_COLOR_ATTACHMENT0 });
	}

	envMap = std::make_shared<EnvironmentMap>();
	envMap->load("res/texture/082.hdr");
	camera.setFOV(90.0f);
	camera.setPos({ 1.583f, -4.044f, 4.9f });
	//camera.setPos({ 0.0f, -8.0f, 0.0f });
	camera.setAspect((float)this->windowWidth() / this->windowHeight());

	scene.addMaterial(Material{ glm::vec3(1.0f), 0.0f, 1.0f });
	scene.addMaterial(Material{ glm::vec3(1.0f), 1.0f, 0.1f });
	scene.addMaterial(Material{ glm::vec3(1.0f, 0.2f, 0.2f), 0.0f, 0.2f });
	scene.addMaterial(Material{ glm::vec3(0.0f), 0.0f, 0.15f });
	scene.addMaterial(Material{ glm::vec3(1.0f), 1.0f, 0.15f });

	//auto car = std::make_shared<Model>("res/model/Huracan.obj", 1, glm::vec3(0.0f), glm::vec3(1.0f));

	//scene.addObject(car);
	//scene.addObject(std::make_shared<Model>("res/model/sphere80.obj", 1, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f), glm::vec3(0.0f, 90.0f, 0.0f)));
	scene.addObject(std::make_shared<Model>("res/model/teapot20.obj", 1, glm::vec3(-4.0f, 2.0f, 0.0f)));
	scene.addObject(std::make_shared<Model>("res/model/square.obj", 0, glm::vec3(0.0f), glm::vec3(100.0f)));
	scene.addObject(std::make_shared<Model>("res/model/suzanne.obj", 2, glm::vec3(4.0f, 2.0f, 1.5f), glm::vec3(1.5f)));
	scene.addObject(std::make_shared<Model>("res/model/bunny.obj", 3, glm::vec3(-4.5f, -2.0f, 0.4f)));
	scene.addObject(std::make_shared<Model>("res/model/cube.obj", 4, glm::vec3(0.0, 7.0f, 2.0f), glm::vec3(4.0f, 4.0f, 4.0f), glm::vec3(-45.0f)));

	sceneBuffers = scene.genBuffers();

	boxCount = sceneBuffers.bound->size() / sizeof(AABB);
	vertexCount = sceneBuffers.vertex->size() / sizeof(glm::vec3);
	triangleCount = sceneBuffers.index->size() / sizeof(uint32_t) / 3;

	rayTestShader.setTexture("vertices", *sceneBuffers.vertex, 1);
	rayTestShader.setTexture("normals", *sceneBuffers.normal, 2);
	rayTestShader.setTexture("texCoords", *sceneBuffers.texCoord, 3);
	rayTestShader.setTexture("indices", *sceneBuffers.index, 4);
	rayTestShader.setTexture("bounds", *sceneBuffers.bound, 5);
	rayTestShader.setTexture("sizeIndices", *sceneBuffers.sizeIndex, 6);
	rayTestShader.setTexture("matIndices", *sceneBuffers.matIndex, 7);
	rayTestShader.setTexture("envMap", envMap->getEnvMap(), 8);
	rayTestShader.setTexture("envCdfTable", envMap->getCdfTable(), 9);
	rayTestShader.set1f("bvhDepth", glm::log2((float)boxCount));

	materials = scene.materialSet();
	lights = scene.lightSet();

	this->setupGUI();
	this->reset();
}

void RayTest::renderLoop()
{
	this->processKey(0, 0, 0, 0);
	VerticalSyncStatus(verticalSync);

	renderer.clear(0.0f, 0.0f, 0.0f);
	this->renderFrame();
	freeCounter++;

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

	rayTestShader.setTexture("lastFrame", frameTex[curFrame ^ 1], 0);
	rayTestShader.set1i("freeCounter", freeCounter);
	rayTestShader.set1i("spp", curSpp);

	renderer.draw(screenVA, rayTestShader);
	frame[curFrame].unbind();

	renderer.clear(0.0f, 0.0f, 0.0f);
	postShader.setTexture("frameBuffer", frameTex[curFrame], 0);
	//postShader.setTexture("frameBuffer", envMap->getCdfTable(), 0);

	renderer.draw(screenVA, postShader);
}

void RayTest::reset()
{
	curSpp = 0;
	for (int i = 0; i < materials.size(); i++)
	{
		rayTestShader.setVec3(("materials[" + std::to_string(i) + "].albedo").c_str(), materials[i].albedo);
		rayTestShader.set1f(("materials[" + std::to_string(i) + "].metallic").c_str(), materials[i].metallic);
		rayTestShader.set1f(("materials[" + std::to_string(i) + "].roughness").c_str(), glm::mix(0.0132f, 1.0f, materials[i].roughness));
		//rayTestShader.set1f(("materials[" + std::to_string(i) + "].anisotropic").c_str(), materials[i].anisotropic);
	}

	rayTestShader.set1i("lightCount", lights.size());

	for (int i = 0; i < lights.size(); i++)
	{
		rayTestShader.setVec3(("lights[" + std::to_string(i) + "].va").c_str(), lights[i].pa);
		rayTestShader.setVec3(("lights[" + std::to_string(i) + "].vb").c_str(), lights[i].pb);
		rayTestShader.setVec3(("lights[" + std::to_string(i) + "].vc").c_str(), lights[i].pc);
		auto norm = glm::normalize(glm::cross(lights[i].pb - lights[i].pa, lights[i].pc - lights[i].pa));
		rayTestShader.setVec3(("lights[" + std::to_string(i) + "].norm").c_str(), norm);
		rayTestShader.setVec3(("lights[" + std::to_string(i) + "].radiosity").c_str(), lights[i].radiosity);
	}
	rayTestShader.set1f("envStrength", envStrength);
	rayTestShader.set1i("envImportance", envImportanceSample);
	rayTestShader.set1i("showBVH", showBVH);

	rayTestShader.setVec3("camF", camera.front());
	rayTestShader.setVec3("camR", camera.right());
	rayTestShader.setVec3("camU", camera.up());
	rayTestShader.setVec3("camPos", camera.pos());
	rayTestShader.set1f("tanFOV", glm::tan(glm::radians(camera.FOV() * 0.5f)));
	rayTestShader.set1f("camAsp", (float)this->windowWidth() / (float)this->windowHeight());

	postShader.set1i("toneMapping", toneMapping);
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
				ImGui::SliderFloat("Roughness", &m.roughness, 0.0f, 1.0f) ||
				ImGui::SliderFloat("Anisotropic", &m.anisotropic, 0.0f, 1.0f))
			{
				reset();
			}
		}

		if (lights.size() > 0)
		{
			ImGui::SliderInt("Light", &lightIndex, 0, lights.size() - 1);
			auto& lt = lights[lightIndex];
			if (ImGui::DragFloat3("Radiosity", (float*)&lt.radiosity, 0.1f, 0.0f) ||
				ImGui::DragFloat3("Va", (float*)&lt.pa, 0.1f) ||
				ImGui::DragFloat3("Vb", (float*)&lt.pb, 0.1f) ||
				ImGui::DragFloat3("Vc", (float*)&lt.pc, 0.1f))
			{
				reset();
			}
		}

		int tmp;
		ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.2f);
		if (ImGui::Combo("EnvMap", &tmp, envStr.c_str(), envList.size()))
		{
			envMap = std::make_shared<EnvironmentMap>();
			envMap->load(envList[tmp]);
			rayTestShader.setTexture("envMap", envMap->getEnvMap(), 8);
			rayTestShader.setTexture("envCdfTable", envMap->getCdfTable(), 9);
			reset();
		}

		if (ImGui::Checkbox("Env Importance", &envImportanceSample))
		{
			reset();
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(-100);
		if (ImGui::DragFloat("Strength", &envStrength, 0.01f, 0.0f)) reset();
		ImGui::PopItemWidth();

		const char* tones[] = { "None", "Reinhard", "Filmic", "ACES" };
		if (ImGui::Combo("ToneMapping", &toneMapping, tones, IM_ARRAYSIZE(tones))) reset();

		if (ImGui::Button("Save Image"))
		{
			int w = this->windowWidth();
			int h = this->windowHeight();
			uint8_t* buf = new uint8_t[w * h * 3];

			glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, buf);

			std::string name = "screenshots/save" + std::to_string((long long)time(0)) + ".png";
			stbi_write_png(name.c_str(), w, h, 3, buf + w * (h - 1) * 3, -w * 3);

			delete[] buf;
		}

		ImGui::Checkbox("Vertical Sync", &verticalSync);
		if (ImGui::DragFloat3("Position", (float*)&camera, 0.1f)) reset();
		if (ImGui::SliderFloat("FOV", (float*)&camera + 15, 0.0f, 90.0f)) reset();
		if (ImGui::Button("Exit"))
		{
			this->setTerminateStatus(true);
		}
	}
	ImGui::End();

	if (ImGui::Begin("Example: Simple overlay", nullptr,
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav))
	{
		ImGui::Text("SPP: %d", curSpp);
		ImGui::Text("Render Time: %.3f ms, FPS: %.3f", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
	}
	ImGui::End();

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
