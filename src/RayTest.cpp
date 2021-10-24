#include "RayTest.h"

RayTest::RayTest()
{
	glfwSetInputMode(this->window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	namespace fs = std::filesystem;
	fs::path envMapDir("res/texture/");

	fs::directory_iterator list(envMapDir);
	
	for (const auto& it : list)
	{
		std::string str = it.path().generic_u8string();
		if (str.find(".hdr") != str.npos || str.find(".png") != str.npos)
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
	int work_grp_cnt[3];

	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &work_grp_cnt[0]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &work_grp_cnt[1]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &work_grp_cnt[2]);

	printf("max global (total) work group counts x:%i y:%i z:%i\n",
		work_grp_cnt[0], work_grp_cnt[1], work_grp_cnt[2]);

	int work_grp_size[3];

	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &work_grp_size[0]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &work_grp_size[1]);
	glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &work_grp_size[2]);

	printf("max local (in one shader) work group sizes x:%i y:%i z:%i\n",
		work_grp_size[0], work_grp_size[1], work_grp_size[2]);

	int work_grp_inv;
	glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &work_grp_inv);
	printf("max local work group invocations %i\n", work_grp_inv);

	screenVB.allocate(sizeof(SCREEN_COORD), SCREEN_COORD, 6);
	screenVA.addBuffer(screenVB, LAYOUT_POS2);

	rayTestShader.setExtension("#extension GL_EXT_texture_array : enable\n");
	rayTestShader.setComputeShaderSizeHint(workgroupSizeX, workgroupSizeY, 1);
	rayTestShader.load("compute.shader");
	postShader.load("postEffects.shader");

	frameTex.generate2D(this->windowWidth(), this->windowHeight(), GL_RGBA32F);
	frameTex.setFilterAndWrapping(GL_LINEAR, GL_CLAMP_TO_EDGE);
	glBindImageTexture(0, frameTex.ID(), 0, false, 0, GL_READ_WRITE, GL_RGBA32F);
	postShader.setTexture("frameBuffer", frameTex, 0);

	scene.envMap = std::make_shared<EnvironmentMap>();
	scene.envMap->load("res/texture/none.png");
	camera.setFOV(50.0f);
	//camera.setPos({ 1.583f, -4.044f, 4.9f });
	camera.setPos({ 0.0f, -3.7f, 1.0f });
	//camera.setPos({ 0.0f, -15.0f, 4.2f });
	//camera.lookAt({ 0.0f, 0.0f, 0.0f });
	camera.setAspect((float)this->windowWidth() / this->windowHeight());

	sceneBuffers = scene.genBuffers();

	boxCount = sceneBuffers.bound->size() / sizeof(AABB);
	vertexCount = sceneBuffers.vertex->size() / sizeof(glm::vec3);
	triangleCount = sceneBuffers.index->size() / sizeof(uint32_t) / 3;

	//haltonTex = Sampler::genHaltonSeqTexture(sampleNum, sampleDim);
	sobolTex = Sampler::genSobolSeqTexture(sampleNum, sampleDim);
	noiseTex = Sampler::genNoiseTexture(this->windowWidth(), this->windowHeight());

	rayTestShader.setTexture("vertices", sceneBuffers.vertex, 1);
	rayTestShader.setTexture("normals", sceneBuffers.normal, 2);
	rayTestShader.setTexture("texCoords", sceneBuffers.texCoord, 3);
	rayTestShader.setTexture("indices", sceneBuffers.index, 4);
	rayTestShader.setTexture("bounds", sceneBuffers.bound, 5);
	rayTestShader.setTexture("hitTable", sceneBuffers.hitTable, 6);
	rayTestShader.setTexture("matTexIndices", sceneBuffers.matTexIndex, 7);
	rayTestShader.setTexture("envMap", scene.envMap->getEnvMap(), 8);
	rayTestShader.setTexture("envAliasTable", scene.envMap->getAliasTable(), 9);
	rayTestShader.setTexture("envAliasProb", scene.envMap->getAliasProb(), 10);
	rayTestShader.setTexture("materials", sceneBuffers.material, 11);
	rayTestShader.setTexture("matTypes", sceneBuffers.material, 11);
	rayTestShader.setTexture("lightPower", sceneBuffers.lightPower, 12);
	rayTestShader.setTexture("lightAlias", sceneBuffers.lightAlias, 13);
	rayTestShader.setTexture("lightProb", sceneBuffers.lightProb, 14);
	rayTestShader.setTexture("textures", sceneBuffers.textures, 15);
	rayTestShader.setTexture("texUVScale", sceneBuffers.texUVScale, 16);
	//rayTestShader.setTexture("haltonSeq", *haltonTex, 15);
	rayTestShader.setTexture("sobolSeq", sobolTex, 17);
	rayTestShader.setTexture("noiseTex", noiseTex, 18);
	rayTestShader.set1i("nLights", scene.nLights);
	rayTestShader.set1f("lightSum", scene.lightSum);
	rayTestShader.set1f("envSum", scene.envMap->sum());
	rayTestShader.set1i("objPrimCount", scene.objPrimCount);
	rayTestShader.set1f("bvhDepth", glm::log2((float)boxCount));
	rayTestShader.set1i("bvhSize", boxCount);
	rayTestShader.set1i("sampleDim", sampleDim);
	rayTestShader.set1i("sampleNum", sampleNum);
	rayTestShader.set2i("frameSize", this->windowWidth(), this->windowHeight());

	postShader.set1i("toneMapping", toneMapping);

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

	if (curSpp < maxSpp || !limitSpp) curSpp++;

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
	if (!cursorDisabled) return;
	camera.changeFOV(offsetY);
	reset();
}

void RayTest::processResize(int width, int height)
{
}

void RayTest::renderFrame()
{
	rayTestShader.set1i("spp", curSpp);
	rayTestShader.set1i("freeCounter", freeCounter);

	if (curSpp < maxSpp || !limitSpp)
	{
		int xNum = (this->windowWidth() + workgroupSizeX - 1) / workgroupSizeX;
		int yNum = (this->windowHeight() + workgroupSizeY - 1) / workgroupSizeY;

		rayTestShader.enable();
		glDispatchCompute(xNum, yNum, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	}

	renderer.clear(0.0f, 0.0f, 0.0f);
	
	this->setViewport(0, 0, this->windowWidth(), this->windowHeight());
	renderer.draw(screenVA, postShader);
}

void RayTest::reset()
{
	curSpp = 0;
	sceneBuffers.material->write(0, scene.materials.size() * sizeof(Material), scene.materials.data());
	rayTestShader.set1i("showBVH", showBVH);

	rayTestShader.set1i("aoMode", aoMode);
	if (aoMode) rayTestShader.setVec3("aoCoef", aoCoef);

	rayTestShader.setVec3("camF", camera.front());
	rayTestShader.setVec3("camR", camera.right());
	rayTestShader.setVec3("camU", camera.up());
	rayTestShader.setVec3("camPos", camera.pos());
	rayTestShader.set1f("tanFOV", glm::tan(glm::radians(camera.FOV() * 0.5f)));
	rayTestShader.set1f("camAsp", (float)this->windowWidth() / (float)this->windowHeight());
	rayTestShader.set1f("lensRadius", lensRadius);
	rayTestShader.set1f("focalDist", focalDist);

	rayTestShader.set1f("envStrength", scene.envStrength);
	rayTestShader.set1i("sampleLight", scene.sampleLight);
	rayTestShader.set1i("lightEnvUniformSample", scene.lightEnvUniformSample);

	rayTestShader.set1i("roulette", roulette);
	rayTestShader.set1f("rouletteProb", rouletteProb);
	rayTestShader.set1i("maxBounce", maxBounce);
	rayTestShader.set1i("matIndex", materialIndex);

	rayTestShader.set1i("sampler", sampler);
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
		if (ImGui::Checkbox("Show BVH", &showBVH) ||
			ImGui::Checkbox("Ambient Occlusion", &aoMode)) reset();

		if (aoMode)
		{
			if (ImGui::DragFloat3("AO Coef", glm::value_ptr(aoCoef), 0.01f, 0.0f)) reset();
		}

		if (ImGui::SliderInt("Max Bounces", &maxBounce, 0, 60)) reset();

		if (ImGui::Checkbox("Roulette", &roulette)) reset();
		if (roulette)
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(-100);
			if (ImGui::SliderFloat("Prob", &rouletteProb, 0.0f, 1.0f)) reset();
		}
		
		if (scene.materials.size() > 0)
		{
			ImGui::SliderInt("Material", &materialIndex, 0, scene.materials.size() - 1);
			auto& m = scene.materials[materialIndex];
			const char* matTypes[] = { "MetalWorkflow", "Dielectric", "ThinDielectric" };
			if (ImGui::Combo("Type", &m.type, matTypes, IM_ARRAYSIZE(matTypes)) ||
				ImGui::ColorEdit3("Albedo", glm::value_ptr(m.albedo)) ||
				ImGui::SliderFloat("Roughness", &m.roughness, 0.0f, 1.0f))
			{	
				reset();
			}

			if (m.type == Material::MetalWorkflow) m.metIor = glm::min<float>(m.metIor, 1.0f);

			if (ImGui::SliderFloat(
				m.type == Material::MetalWorkflow ? "Metallic" : "Ior",
				&m.metIor, 0.0f,
				m.type == Material::MetalWorkflow ? 1.0f : 4.0f)) reset();
		}

		const char* samplers[] = { "Independent", "Sobol" };
		if (ImGui::Combo("Sampler", &sampler, samplers, IM_ARRAYSIZE(samplers))) reset();

		int tmp;
		ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.2f);
		if (ImGui::Combo("EnvMap", &tmp, envStr.c_str(), envList.size()))
		{
			scene.envMap = std::make_shared<EnvironmentMap>();
			scene.envMap->load(envList[tmp]);
			rayTestShader.setTexture("envMap", scene.envMap->getEnvMap(), 8);
			rayTestShader.setTexture("envAliasTable", scene.envMap->getAliasTable(), 9);
			rayTestShader.setTexture("envAliasProb", scene.envMap->getAliasProb(), 10);
			rayTestShader.set1f("envSum", scene.envMap->sum());
			reset();
		}

		if (ImGui::Checkbox("Sample Direct Light", &scene.sampleLight) ||
			ImGui::Checkbox("LightEnvUniform", &scene.lightEnvUniformSample)) reset();

		ImGui::SameLine();
		ImGui::SetNextItemWidth(-100);
		if (ImGui::DragFloat("Strength", &scene.envStrength, 0.01f, 0.0f)) reset();
		ImGui::PopItemWidth();

		const char* tones[] = { "None", "Reinhard", "Filmic", "ACES" };
		if (ImGui::Combo("ToneMapping", &toneMapping, tones, IM_ARRAYSIZE(tones)))
		{
			postShader.set1i("toneMapping", toneMapping);
		}

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
		auto f = camera.front();
		ImGui::Text("Dir x: %f, y: %f, z: %f\n", f.x, f.y, f.z);
		if (ImGui::DragFloat3("Position", (float*)&camera, 0.1f)) reset();
		if (ImGui::SliderFloat("FOV", (float*)&camera + 15, 0.0f, 90.0f)) reset();
		if (ImGui::DragFloat("FocalDistance", &focalDist, 0.01f, 0.004f, 100.0f)) reset();
		if (ImGui::DragFloat("LensRadius", &lensRadius, 0.001f, 0.0f, 10.0f)) reset();

		if (ImGui::Checkbox("Limit SPP", &limitSpp) && curSpp > maxSpp) reset();
		if (limitSpp) 
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(-100);
			if (ImGui::DragInt("Max SPP", &maxSpp, 1.0f, 0, 131072) && curSpp > maxSpp) reset();
		}

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
