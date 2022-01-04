#include "PathTracer.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <set>
#include <map>

const int WorkgroupSizeX = 48;
const int WorkgroupSizeY = 32;

const float ScreenCoord[] =
{
	0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
	0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
};

std::vector<std::string> envList;
std::string envStr;

NAMESPACE_BEGIN(PathTracer)

namespace CSGroupNum
{
	int numX;
	int numY;
}

namespace InputRecord
{
	int lastCursorX = 0;
	int lastCursorY = 0;
	bool firstCursorMove = true;
	bool cursorDisabled = false;

	std::set<int> pressedKeys;
}

namespace Count
{
	int boxCount;
	int triangleCount;
	int vertexCount;
}

namespace GLContext
{
	PipelinePtr pipeline;

	ShaderPtr rayTraceShader;
	ShaderPtr postShader;

	Texture2DPtr frameTex;
	VertexBufferPtr screenVB;

	Scene scene;
}

namespace Settings
{
	int uiMeshIndex = 0;
	int uiMatIndex = 0;
	int uiEnvIndex;

	bool showBVH = true;
	bool cursorDisabled = false;

	bool pause = false;
	bool verticalSync = false;
}

namespace SampleCounter
{
	bool limitSpp = false;
	int maxSpp = 64;
	int curSpp = 0;
	int freeCounter = 0;
}

void calcCSGroupNum(int width, int height)
{
	CSGroupNum::numX = (width + WorkgroupSizeX - 1) / WorkgroupSizeX;
	CSGroupNum::numY = (height + WorkgroupSizeY - 1) / WorkgroupSizeY;
}

void count()
{
	using namespace SampleCounter;
	freeCounter++;
	if (curSpp < maxSpp || !limitSpp)
		curSpp++;
}

void resetCounter()
{
	SampleCounter::curSpp = 0;
	GLContext::rayTraceShader->set1i("uSpp", SampleCounter::curSpp);
}

void updateCameraUniforms()
{
	using namespace GLContext;
	rayTraceShader->setVec3("uCamF", scene.camera.front());
	rayTraceShader->setVec3("uCamR", scene.camera.right());
	rayTraceShader->setVec3("uCamU", scene.camera.up());
	rayTraceShader->setVec3("uCamPos", scene.camera.pos());
	rayTraceShader->set1f("uTanFOV", glm::tan(glm::radians(scene.camera.FOV() * 0.5f)));
	rayTraceShader->set1f("uCamAsp", scene.camera.aspect());
	rayTraceShader->set1f("uLensRadius", scene.lensRadius);
	rayTraceShader->set1f("uFocalDist", scene.focalDist);
}

void processKeys()
{
	using namespace InputRecord;
	const int Keys[] = { GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D,
		GLFW_KEY_Q, GLFW_KEY_E, GLFW_KEY_R, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_SPACE };

	bool updated = false;
	for (auto key : Keys)
	{
		if (pressedKeys.find(key) != pressedKeys.end())
		{
			GLContext::scene.camera.move(key);
			updated = true;
		}
	}

	if (updated)
	{
		updateCameraUniforms();
		resetCounter();
	}
}

void init(int width, int height, GLFWwindow* window)
{
	glfwSetKeyCallback(window, keyCallback);
	glfwSetCursorPosCallback(window, cursorCallback);
	glfwSetScrollCallback(window, scrollCallback);
	glfwSetWindowSizeCallback(window, resizeCallback);

	VertexArray::initDefaultLayouts();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsClassic();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 450");

	using namespace GLContext;
	PipelineCreateInfo plInfo;
	plInfo.primType = PrimitiveType::Triangles;
	plInfo.faceToDraw = DrawFace::FrontBack;
	plInfo.polyMode = PolygonMode::Fill;
	plInfo.clearBuffer = BufferBit::Color | BufferBit::Depth;
	pipeline = Pipeline::create(plInfo);
	pipeline->setViewport(0, 0, width, height);

	screenVB = VertexBuffer::createTyped<glm::vec2>(ScreenCoord, 6);

	scene.load();
	scene.createGLContext();
	auto& sceneBuffers = scene.glContext;

	Count::boxCount = sceneBuffers.bound->size() / sizeof(AABB);
	Count::vertexCount = sceneBuffers.vertex->size() / sizeof(glm::vec3);
	Count::triangleCount = sceneBuffers.index->size() / sizeof(uint32_t) / 3;

	frameTex = Texture2D::createEmpty(width, height, TextureFormat::Col4x32f);
	
	rayTraceShader = Shader::create("compute.shader", { WorkgroupSizeX, WorkgroupSizeY, 1 },
		"#extension GL_EXT_texture_array : enable\n");
	Pipeline::bindTextureToImage(frameTex, 0, 0, ImageAccess::ReadWrite, TextureFormat::Col4x32f);
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
	rayTraceShader->setTexture("uSobolSeq", scene.sobolTex, 17);
	rayTraceShader->setTexture("uNoiseTex", scene.noiseTex, 18);
	rayTraceShader->set1i("uNumLightTriangles", scene.nLightTriangles);
	rayTraceShader->set1f("uLightSum", scene.lightSumPdf);
	rayTraceShader->set1f("uEnvSum", scene.envMap->sumPdf());
	rayTraceShader->set1i("uObjPrimCount", scene.objPrimCount);
	rayTraceShader->set1f("uBvhDepth", glm::log2((float)Count::boxCount));
	rayTraceShader->set1i("uBvhSize", Count::boxCount);
	rayTraceShader->set1i("uSampleDim", scene.SampleDim);
	rayTraceShader->set1i("uSampleNum", scene.SampleNum);
	rayTraceShader->set2i("uFrameSize", width, height);

	postShader = Shader::create("postEffects.shader");
	postShader->set1i("uToneMapper", scene.toneMapping);
	postShader->setTexture("uFrame", frameTex, 0);

	calcCSGroupNum(width, height);

	rayTraceShader->set1i("uShowBVH", Settings::showBVH);
	rayTraceShader->set1i("uAoMode", scene.aoMode);
	rayTraceShader->setVec3("uAoCoef", scene.aoCoef);
	rayTraceShader->set1i("uMaxBounce", scene.maxBounce);
	rayTraceShader->set1f("uLightSamplePortion", scene.lightPortion);
	rayTraceShader->set1i("uSampleLight", scene.sampleLight);
	rayTraceShader->set1i("uLightEnvUniformSample", scene.lightEnvUniformSample);
	rayTraceShader->set1f("uEnvRotation", scene.envRotation);
	rayTraceShader->set1i("uMatIndex", Settings::uiMatIndex);
	rayTraceShader->set1i("uSampler", scene.sampler);
	updateCameraUniforms();

	VerticalSyncStatus(Settings::verticalSync);
	Resource::clear();
}

void mainLoop()
{
	processKeys();
	trace();
	count();
	renderGUI();
}

void trace()
{
	using namespace GLContext;
	using namespace SampleCounter;
	rayTraceShader->set1i("uSpp", curSpp);
	rayTraceShader->set1i("uFreeCounter", freeCounter);

	if (curSpp < maxSpp || !limitSpp)
	{
		Pipeline::dispatchCompute(CSGroupNum::numX, CSGroupNum::numY, 1, rayTraceShader);
		Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);
	}
	pipeline->clear({ 0.0f, 0.0f, 0.0f, 1.0f });
	pipeline->draw(screenVB, VertexArray::layoutPos2(), postShader);
}

void renderGUI()
{
	using namespace GLContext;
	using namespace Settings;
	using namespace SampleCounter;
	using namespace Count;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open", "Ctrl+O"));
			if (ImGui::MenuItem("Save", "Ctrl+S"));

			if (ImGui::MenuItem("Save Image", "Ctrl+P"))
			{
				std::string name = "screenshots/save" + std::to_string((long long)time(0)) + ".png";
				auto img = pipeline->readFramePixels();
				stbi_flip_vertically_on_write(true);
				stbi_write_png(name.c_str(), img->width(), img->height(), 3, img->data(), img->width() * 3);
				Error::bracketLine<0>("Save " + name + " " + std::to_string(img->width()) + "x"
					+ std::to_string(img->height()));
			}

			if (ImGui::MenuItem("Exit", "Esc"));
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Settings"))
		{
			if (ImGui::MenuItem("Display BVH", nullptr, &showBVH))
			{
				rayTraceShader->set1i("uShowBVH", Settings::showBVH);
				resetCounter();
			}

			if (ImGui::MenuItem("Vertical Sync", nullptr, &verticalSync))
				VerticalSyncStatus(verticalSync);

			if (ImGui::MenuItem("Limit SPP", nullptr, &limitSpp) && curSpp > maxSpp)
				resetCounter();
			if (limitSpp)
			{
				if (ImGui::DragInt("Max SPP", &maxSpp, 1.0f, 0, 131072) && curSpp > maxSpp)
					resetCounter();
			}

			if (ImGui::MenuItem("Ambient Occlusion", nullptr, &scene.aoMode))
			{
				rayTraceShader->set1i("uAoMode", scene.aoMode);
				if (!Settings::showBVH)
					resetCounter();
			}

			if (scene.aoMode && ImGui::DragFloat3("AO Coef", glm::value_ptr(scene.aoCoef), 0.01f, 0.0f))
			{
				rayTraceShader->setVec3("uAoCoef", scene.aoCoef);
				resetCounter();
			}

			if (ImGui::SliderInt("Max Bounces", &scene.maxBounce, 0, 60))
			{
				rayTraceShader->set1i("uMaxBounce", scene.maxBounce);
				resetCounter();
			}

			const char* samplers[] = { "Independent", "Sobol" };
			if (ImGui::Combo("Sampler", &scene.sampler, samplers, IM_ARRAYSIZE(samplers)))
			{
				rayTraceShader->set1i("uSampler", scene.sampler);
				resetCounter();
			}

			if (ImGui::Checkbox("Sample Direct Light", &scene.sampleLight))
			{
				rayTraceShader->set1i("uSampleLight", scene.sampleLight);
				resetCounter();
			}

			const char* tones[] = { "None", "Reinhard", "Filmic", "ACES" };
			if (ImGui::Combo("ToneMapping", &scene.toneMapping, tones, IM_ARRAYSIZE(tones)))
				postShader->set1i("uToneMapper", scene.toneMapping);

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Scene"))
		{
			if (scene.materials.size() > 0)
			{
				if (ImGui::SliderInt("Material", &uiMatIndex, 0, scene.materials.size() - 1))
				{
					rayTraceShader->set1i("uMatIndex", Settings::uiMatIndex);
					if (showBVH)
						resetCounter();
				}

				auto& m = scene.materials[uiMatIndex];
				const char* matTypes[] = { "MetalWorkflow", "Dielectric", "ThinDielectric" };
				if (ImGui::Combo("Type", &m.type, matTypes, IM_ARRAYSIZE(matTypes)) ||
					ImGui::ColorEdit3("Albedo", glm::value_ptr(m.albedo)) ||
					ImGui::SliderFloat("Roughness", &m.roughness, 0.0f, 1.0f))
				{
					scene.glContext.material->write(sizeof(Material) * uiMatIndex, sizeof(Material), &m);
					resetCounter();
				}

				if (m.type == Material::MetalWorkflow) m.metIor = glm::min<float>(m.metIor, 1.0f);

				if (ImGui::SliderFloat(
					m.type == Material::MetalWorkflow ? "Metallic" : "Ior",
					&m.metIor, 0.0f,
					m.type == Material::MetalWorkflow ? 1.0f : 4.0f))
				{
					scene.glContext.material->write(sizeof(Material) * uiMatIndex, sizeof(Material), &m);
					resetCounter();
				}
			}

			if (ImGui::Combo("EnvMap", &uiEnvIndex, envStr.c_str(), envList.size()))
			{
				scene.envMap = EnvironmentMap::create(envList[uiEnvIndex]);
				rayTraceShader->setTexture("uEnvMap", scene.envMap->envMap(), 8);
				rayTraceShader->setTexture("uEnvAliasTable", scene.envMap->aliasTable(), 9);
				rayTraceShader->setTexture("uEnvAliasProb", scene.envMap->aliasProb(), 10);
				rayTraceShader->set1f("uEnvSum", scene.envMap->sumPdf());
				resetCounter();
			}

			if (ImGui::SliderAngle("EnvMap Rotation", &scene.envRotation, -180.0f, 180.0f))
			{
				rayTraceShader->set1f("uEnvRotation", scene.envRotation);
				resetCounter();
			}

			if (scene.nLightTriangles > 0 &&
				ImGui::Checkbox("Manual Sample Portion", &scene.lightEnvUniformSample))
			{
				rayTraceShader->set1i("uLightEnvUniformSample", scene.lightEnvUniformSample);
				resetCounter();
			}

			if (scene.lightEnvUniformSample &&
				ImGui::SliderFloat("LightPortion", &scene.lightPortion, 1e-4f, 1.0f - 1e-4f))
			{
				rayTraceShader->set1f("uLightSamplePortion", scene.lightPortion);
				resetCounter();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Camera"))
		{
			auto pos = scene.camera.pos();
			auto fov = scene.camera.FOV();
			auto angle = scene.camera.angle();
			if (ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.1f) ||
				ImGui::DragFloat3("Angle", glm::value_ptr(angle), 0.05f) ||
				ImGui::SliderFloat("FOV", &fov, 0.0f, 90.0f) ||
				ImGui::DragFloat("FocalDistance", &scene.focalDist, 0.01f, 0.004f, 100.0f) ||
				ImGui::DragFloat("LensRadius", &scene.lensRadius, 0.001f, 0.0f, 10.0f))
			{
				scene.camera.setPos(pos);
				scene.camera.setAngle(angle);
				scene.camera.setFOV(fov);
				updateCameraUniforms();
				resetCounter();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Statistics"))
		{
			ImGui::Text("BVH Nodes:    %d", boxCount);
			ImGui::Text("Triangles:    %d", triangleCount);
			ImGui::Text("Vertices:     %d", vertexCount);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			ImGui::BulletText("F1            Switch view/edit mode");
			ImGui::BulletText("Q/E	       Rotate camera");
			ImGui::BulletText("R             Reset camera rotation");
			ImGui::BulletText("Scroll        Zoom in/out");
			ImGui::BulletText("W/A/S/D       Move camera horizonally");
			ImGui::BulletText("Shift/Space   Move camera vertically");
			ImGui::BulletText("Esc           Quit");
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	if (ImGui::Begin("Example: Simple overlay", nullptr,
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav))
	{
		ImGui::Text("SPP: %d", SampleCounter::curSpp);
		ImGui::Text("Render Time: %.3f ms, FPS: %.3f", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	using namespace InputRecord;

	if (action == GLFW_PRESS)
		pressedKeys.insert(key);
	else if (action == GLFW_RELEASE)
		pressedKeys.erase(key);

	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
	{
		if (cursorDisabled)
			firstCursorMove = true;
		cursorDisabled ^= 1;
		glfwSetInputMode(window, GLFW_CURSOR, cursorDisabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	}
}

void cursorCallback(GLFWwindow* window, double posX, double posY)
{
	using namespace InputRecord;

	if (!cursorDisabled) return;
	if (firstCursorMove)
	{
		lastCursorX = posX;
		lastCursorY = posY;
		firstCursorMove = false;
		return;
	}

	float offsetX = posX - lastCursorX;
	float offsetY = posY - lastCursorY;
	glm::vec3 offset = glm::vec3(offsetX, -offsetY, 0) * Camera::SensitivityRotate;
	GLContext::scene.camera.rotate(offset);

	lastCursorX = posX;
	lastCursorY = posY;
	updateCameraUniforms();
	resetCounter();
}

void scrollCallback(GLFWwindow* window, double offsetX, double offsetY)
{
	if (!InputRecord::cursorDisabled)
		return;
	GLContext::scene.camera.changeFOV(offsetY);
	updateCameraUniforms();
	resetCounter();
}

void resizeCallback(GLFWwindow* window, int width, int height)
{
	Error::bracketLine<0>("Resize to " + std::to_string(width) + "x" + std::to_string(height));

	using namespace GLContext;
	frameTex = Texture2D::createEmpty(width, height, TextureFormat::Col4x32f);
	Pipeline::bindTextureToImage(frameTex, 0, 0, ImageAccess::ReadWrite, TextureFormat::Col4x32f);
	pipeline->setViewport(0, 0, width, height);
	scene.camera.setAspect(static_cast<float>(width) / height);

	rayTraceShader->set2i("uFrameSize", width, height);
	rayTraceShader->set1f("uCamAsp", scene.camera.aspect());
	postShader->setTexture("uFrame", frameTex, 0);
	calcCSGroupNum(width, height);
	
	resetCounter();
}

NAMESPACE_END(PathTracer)