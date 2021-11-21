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
	// GL_MAX_COMPUTE_WORK_GROUP_COUNT
	// GL_MAX_COMPUTE_WORK_GROUP_SIZE
	// GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS
	VertexArray::initDefaultLayouts();

	const float ScreenCoord[] =
	{
		0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
	};

	int width = windowWidth();
	int height = windowHeight();

	PipelineCreateInfo plInfo;
	plInfo.primType = PrimitiveType::Triangles;
	plInfo.faceToDraw = DrawFace::FrontBack;
	plInfo.polyMode = PolygonMode::Fill;
	plInfo.clearBuffer = BufferBit::Color | BufferBit::Depth;
	pipeline = Pipeline::create(plInfo);

	screenVB = VertexBuffer::createTyped<glm::vec2>(ScreenCoord, 6);

	rayTraceShader = Shader::create("compute.shader", { workgroupSizeX, workgroupSizeY, 1 },
		"#extension GL_EXT_texture_array : enable\n");
	postShader = Shader::create("postEffects.shader");

	frameTex = Texture2D::createEmpty(width, height, TextureFormat::Col4x32f);

	Pipeline::bindTextureToImage(frameTex, 0, 0, ImageAccess::ReadWrite, TextureFormat::Col4x32f);
	postShader->setTexture("uFrame", frameTex, 0);

	scene.envMap = EnvironmentMap::create("res/texture/none.png");
	camera.setFOV(50.0f);
	camera.setPos({ 0.0f, -3.7f, 1.0f });
	camera.setAspect((float)this->windowWidth() / this->windowHeight());

	sceneBuffers = scene.genBuffers();

	boxCount = sceneBuffers.bound->size() / sizeof(AABB);
	vertexCount = sceneBuffers.vertex->size() / sizeof(glm::vec3);
	triangleCount = sceneBuffers.index->size() / sizeof(uint32_t) / 3;

	sobolTex = Sampler::genSobolSeqTexture(sampleNum, sampleDim);
	noiseTex = Sampler::genNoiseTexture(this->windowWidth(), this->windowHeight());

	rayTraceShader->setTexture("uVertices", sceneBuffers.vertex, 1);
	rayTraceShader->setTexture("uNormals", sceneBuffers.normal, 2);
	rayTraceShader->setTexture("uTexCoords", sceneBuffers.texCoord, 3);
	rayTraceShader->setTexture("uIndices", sceneBuffers.index, 4);
	rayTraceShader->setTexture("uBounds", sceneBuffers.bound, 5);
	rayTraceShader->setTexture("uHitTable", sceneBuffers.hitTable, 6);
	rayTraceShader->setTexture("uMatTexIndices", sceneBuffers.matTexIndex, 7);
	rayTraceShader->setTexture("uEnvMap", scene.envMap->envMap(), 8);
	rayTraceShader->setTexture("uEnvAliasTable", scene.envMap->aliasTable(), 9);
	rayTraceShader->setTexture("uEnvAliasProb", scene.envMap->aliasProb(), 10);
	rayTraceShader->setTexture("uMaterials", sceneBuffers.material, 11);
	rayTraceShader->setTexture("uMatTypes", sceneBuffers.material, 11);
	rayTraceShader->setTexture("uLightPower", sceneBuffers.lightPower, 12);
	rayTraceShader->setTexture("uLightAlias", sceneBuffers.lightAlias, 13);
	rayTraceShader->setTexture("uLightProb", sceneBuffers.lightProb, 14);
	rayTraceShader->setTexture("uTextures", sceneBuffers.textures, 15);
	rayTraceShader->setTexture("uTexUVScale", sceneBuffers.texUVScale, 16);
	rayTraceShader->setTexture("uSobolSeq", sobolTex, 17);
	rayTraceShader->setTexture("uNoiseTex", noiseTex, 18);
	rayTraceShader->set1i("uNumLightTriangles", scene.nLightTriangles);
	rayTraceShader->set1f("uLightSum", scene.lightSumPdf);
	rayTraceShader->set1f("uEnvSum", scene.envMap->sumPdf());
	rayTraceShader->set1i("uObjPrimCount", scene.objPrimCount);
	rayTraceShader->set1f("uBvhDepth", glm::log2((float)boxCount));
	rayTraceShader->set1i("uBvhSize", boxCount);
	rayTraceShader->set1i("uSampleDim", sampleDim);
	rayTraceShader->set1i("uSampleNum", sampleNum);
	rayTraceShader->set2i("uFrameSize", width, height);

	postShader->set1i("uToneMapper", toneMapping);

	this->setupGUI();
	this->reset();
}

void RayTest::renderLoop()
{
	this->processKey(0, 0, 0, 0);
	VerticalSyncStatus(verticalSync);

	pipeline->clear({ 0.0f, 0.0f, 0.0f, 1.0f });
	renderFrame();

	freeCounter++;
	if (curSpp < maxSpp || !limitSpp)
		curSpp++;

	renderGUI();
	swapBuffers();
	display();
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
	rayTraceShader->set1i("uSpp", curSpp);
	rayTraceShader->set1i("uFreeCounter", freeCounter);

	if (curSpp < maxSpp || !limitSpp)
	{
		int xNum = (windowWidth() + workgroupSizeX - 1) / workgroupSizeX;
		int yNum = (windowHeight() + workgroupSizeY - 1) / workgroupSizeY;

		Pipeline::dispatchCompute(xNum, yNum, 1, rayTraceShader);
		Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);
	}
	pipeline->clear({ 0.0f, 0.0f, 0.0f, 1.0f });	
	pipeline->setViewport(0, 0, windowWidth(), windowHeight());
	pipeline->draw(screenVB, VertexArray::layoutPos2(), postShader);
}

void RayTest::reset()
{
	curSpp = 0;
	sceneBuffers.material->write(0, scene.materials.size() * sizeof(Material), scene.materials.data());
	rayTraceShader->set1i("uShowBVH", showBVH);

	rayTraceShader->set1i("uAoMode", aoMode);
	if (aoMode) rayTraceShader->setVec3("uAoCoef", aoCoef);

	rayTraceShader->setVec3("uCamF", camera.front());
	rayTraceShader->setVec3("uCamR", camera.right());
	rayTraceShader->setVec3("uCamU", camera.up());
	rayTraceShader->setVec3("uCamPos", camera.pos());
	rayTraceShader->set1f("uTanFOV", glm::tan(glm::radians(camera.FOV() * 0.5f)));
	rayTraceShader->set1f("uCamAsp", (float)windowWidth() / windowHeight());
	rayTraceShader->set1f("uLensRadius", lensRadius);
	rayTraceShader->set1f("uFocalDist", focalDist);

	rayTraceShader->set1f("uLightSamplePortion", scene.envStrength);
	rayTraceShader->set1i("uSampleLight", scene.sampleLight);
	rayTraceShader->set1i("uLightEnvUniformSample", scene.lightEnvUniformSample);
	rayTraceShader->set1f("uEnvRotation", scene.envRotation);

	rayTraceShader->set1i("uMaxBounce", maxBounce);
	rayTraceShader->set1i("uMatIndex", materialIndex);

	rayTraceShader->set1i("uSampler", sampler);
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

		if (ImGui::Combo("EnvMap", &envMapIndex, envStr.c_str(), envList.size()))
		{
			scene.envMap = EnvironmentMap::create(envList[envMapIndex]);
			rayTraceShader->setTexture("uEnvMap", scene.envMap->envMap(), 8);
			rayTraceShader->setTexture("uEnvAliasTable", scene.envMap->aliasTable(), 9);
			rayTraceShader->setTexture("uEnvAliasProb", scene.envMap->aliasProb(), 10);
			rayTraceShader->set1f("uEnvSum", scene.envMap->sumPdf());
			reset();
		}

		if (ImGui::SliderAngle("EnvMap Rotation", &scene.envRotation, -180.0f, 180.0f))
			reset();

		if (ImGui::Checkbox("Sample Direct Light", &scene.sampleLight)) reset();
		if (scene.nLightTriangles > 0)
		{
			if (ImGui::Checkbox("Manual Sample Portion", &scene.lightEnvUniformSample))
				reset();
		}

		if (scene.lightEnvUniformSample)
			if (ImGui::SliderFloat("LightPortion", &scene.envStrength, 0.0f, 1.0f)) reset();

		const char* tones[] = { "None", "Reinhard", "Filmic", "ACES" };
		if (ImGui::Combo("ToneMapping", &toneMapping, tones, IM_ARRAYSIZE(tones)))
		{
			postShader->set1i("uToneMapper", toneMapping);
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
