#include "Application.h"

#include <functional>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <random>
#include <ctime>
#include <map>

#include "core/Buffer.h"
#include "core/Camera.h"
#include "core/CheckError.h"
#include "core/FrameBuffer.h"
#include "core/GLSizeofType.h"
#include "core/Model.h"
#include "core/RenderBuffer.h"
#include "core/Pipeline.h"
#include "core/Scene.h"
#include "core/Texture.h"
#include "core/VertexArray.h"
#include "core/VerticalSync.h"
#include "core/Sampler.h"
#include "gui/FileSelector.h"
#include "gui/Editor.h"

std::pair<int, int> getWindowSize(GLFWwindow* window)
{
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	return { width, height };
}

NAMESPACE_BEGIN(Application)

const int PostProcBlockX = 48;
const int PostProcBlockY = 32;
const int UnitPostOut = 6;
const int UnitPostIn = 7;

GLFWwindow* mainWindow = nullptr;

std::shared_ptr<NaivePathIntegrator> naivePathTracer;
std::shared_ptr<LightPathIntegrator> lightTracer;
std::shared_ptr<TriplePathIntegrator> triplePathTracer;
std::shared_ptr<GlobalQueuePathIntegrator> globalQueuePathTracer;
std::shared_ptr<BlockQueuePathIntegrator> blockQueuePathTracer;
std::shared_ptr<SharedQueuePathIntegrator> sharedQueuePathTracer;
std::shared_ptr<BVHDisplayIntegrator> bvhDisplayer;
std::shared_ptr<RasterView> rasterViewer;
IntegratorPtr integrator;

namespace InputRecord
{
	int lastCursorX = 0;
	int lastCursorY = 0;
	bool firstCursorMove = true;
	bool cursorDisabled = false;

	std::set<int> pressedKeys;
	double lastTime = 0;
	double accFrameTime = 0;
}

namespace GLContext
{
	PipelinePtr pipeline;
	VertexBufferPtr screenVB;
	Scene scene;
	ShaderPtr postShader;
	glm::ivec2 renderSize;
	glm::ivec2 windowSize;
	Texture2DPtr resultTex;
}

namespace GUI
{
	int modelIndex = 0;
	int meshIndex;
	int matIndex = 0;
	int lightIndex = 0;
	int envIndex;
	int integIndex = 0;
	int toneIndex = 1;
	FileSelector fileSelector;
}

namespace Config
{
	bool cursorDisabled = false;

	bool verticalSync = false;

	bool preview = true;
	int previewScale = 4;

	int toneMapping = 1;

	bool limitTime = false;
	double maxTime = 30.0;
	double renderBeginTime;

	bool rendering = false;
	bool paused = false;
	bool linkCameras = false;
	bool reloaded = false;
	bool sceneGeomChanged = false;

	bool renderWindowIsOpen = true;
}

float getMainCameraAsp()
{
	return static_cast<float>(GLContext::renderSize.x) / GLContext::renderSize.y;
}

float getPreviewCameraAsp()
{
	return static_cast<float>(GLContext::windowSize.x) / GLContext::windowSize.y;
}

void resetPreviewCamera()
{
	GLContext::scene.resetPreviewCamera();
	GLContext::scene.previewCamera.setAspect(getPreviewCameraAsp());
}

void setMainCameraCurrent()
{
	GLContext::scene.setCameraCurrent();
	GLContext::scene.camera.setAspect(getMainCameraAsp());
}

void reset(ResetLevel level)
{
	using namespace GLContext;
	using namespace Config;
	glm::ivec2 resetFrameSize = preview ? renderSize / previewScale : renderSize;
	integrator->setStatus({ &scene, resetFrameSize });
	integrator->setShouldReset();
	if (resetFrameSize != integrator->getFrame()->size() || level == ResetLevel::ResetFrame)
		integrator->recreateFrameTex(resetFrameSize.x, resetFrameSize.y);

	if (resultTex->size() != renderSize || level == ResetLevel::ResetFrame)
	{
		resultTex = Texture2D::createEmpty(renderSize.x, renderSize.y, TextureFormat::Col4x32f);
		Pipeline::bindTextureToImage(resultTex, UnitPostOut, 0, ImageAccess::WriteOnly, TextureFormat::Col4x32f);
		GLContext::postShader->set2i("uFilmSize", renderSize.x, renderSize.y);
		scene.camera.setAspect(getMainCameraAsp());
	}

	if (reloaded)
	{
		rasterViewer->setStatus({ &scene, resetFrameSize });
		rasterViewer->reset({ &scene, windowSize });
		rasterViewer->setIndex(0, 0);
		reloaded = false;
	}
	renderBeginTime = ImGui::GetTime();
}

void regenerateScene(bool resetTextures)
{
	GLContext::scene.createGLContext(resetTextures);
	Config::reloaded = true;
	Config::sceneGeomChanged = false;
	Pipeline::clearBindingRecord();
	integrator->reset({ &GLContext::scene, GLContext::renderSize, ResetLevel::FullReset });
	rasterViewer->reset({ &GLContext::scene, GLContext::renderSize, ResetLevel::FullReset });
}

bool isPressing(int keyCode)
{
	auto res = InputRecord::pressedKeys.find(keyCode);
	return res != InputRecord::pressedKeys.end();
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	using namespace InputRecord;

	if (action == GLFW_PRESS)
		pressedKeys.insert(key);
	else if (action == GLFW_RELEASE)
		pressedKeys.erase(key);

	if (action == GLFW_PRESS)
	{
		if (key == GLFW_KEY_ESCAPE)
			glfwSetWindowShouldClose(window, true);
		else if (key == GLFW_KEY_F1)
		{
			if (cursorDisabled)
				firstCursorMove = true;
			cursorDisabled ^= 1;
			glfwSetInputMode(window, GLFW_CURSOR, cursorDisabled ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
		}
		if (!isPressing(GLFW_KEY_LEFT_CONTROL) && !isPressing(GLFW_KEY_RIGHT_CONTROL))
			return;

		if (key == GLFW_KEY_P)
		{
			Config::preview ^= 1;
			reset();
		}
		else if (key == GLFW_KEY_V)
		{
			Config::verticalSync ^= 1;
			VerticalSyncStatus(Config::verticalSync);
		}
	}
}

void cursorCallback(GLFWwindow* window, double posX, double posY)
{
	using namespace InputRecord;
	if (!cursorDisabled)
		return;

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
	GLContext::scene.previewCamera.rotate(offset);

	lastCursorX = posX;
	lastCursorY = posY;

	if (Config::linkCameras)
	{
		setMainCameraCurrent();
		reset();
	}
}

void scrollCallback(GLFWwindow* window, double offsetX, double offsetY)
{
	if (!InputRecord::cursorDisabled)
		return;
	GLContext::scene.previewCamera.changeFOV(offsetY);
	if (Config::linkCameras)
	{
		setMainCameraCurrent();
		reset();
	}
}

void resizeCallback(GLFWwindow* window, int width, int height)
{
	using namespace GLContext;
	pipeline->setViewport(0, 0, width, height);
	scene.previewCamera.setAspect(static_cast<float>(width) / height);
	windowSize = { width, height };
}

void init(const std::string& title, const File::path& scenePath)
{
	if (glfwInit() != GLFW_TRUE)
		Error::exit("failed to init GLFW");

	int nMonitors;
	auto monitors = glfwGetMonitors(&nMonitors);
	auto mode = glfwGetVideoMode(monitors[0]);
	int windowWidth = mode->width * 0.75;
	int windowHeight = mode->height * 0.75;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_DECORATED, true);
	glfwWindowHint(GLFW_RESIZABLE, true);
	glfwWindowHint(GLFW_VISIBLE, false);

	mainWindow = glfwCreateWindow(windowWidth, windowHeight, title.c_str(), nullptr, nullptr);
	glfwMakeContextCurrent(mainWindow);

	glfwSetKeyCallback(mainWindow, keyCallback);
	glfwSetCursorPosCallback(mainWindow, cursorCallback);
	glfwSetScrollCallback(mainWindow, scrollCallback);
	glfwSetWindowSizeCallback(mainWindow, resizeCallback);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		Error::exit("failed to init GLAD");

	glEnable(GL_DEPTH_TEST);

	int sharedMemSize;
	glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE, &sharedMemSize);
	Error::line("GL max shared memory size:\t" + std::to_string(sharedMemSize >> 10) + " KB");

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsClassic();
	ImGui_ImplGlfw_InitForOpenGL(mainWindow, true);
	ImGui_ImplOpenGL3_Init("#version 450");

	auto& guiStyle = ImGui::GetStyle();
	guiStyle.FrameRounding = 1.0f;
	guiStyle.FramePadding.y = 2.0f;
	guiStyle.ItemSpacing.y = 6.0f;
	guiStyle.GrabRounding = 1.0f;

	VertexArray::initDefaultLayouts();

	using namespace GLContext;
	PipelineCreateInfo plInfo;
	plInfo.primType = PrimitiveType::Triangles;
	plInfo.faceToDraw = DrawFace::FrontBack;
	plInfo.polyMode = PolygonMode::Fill;
	plInfo.clearBuffer = BufferBit::Color | BufferBit::Depth;
	pipeline = Pipeline::create(plInfo);
	pipeline->setViewport(0, 0, windowWidth, windowHeight);

	const float ScreenCoord[] =
	{
		0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
	};
	screenVB = VertexBuffer::createTyped<glm::vec2>(ScreenCoord, 6);

	scene.load(scenePath);
	scene.createGLContext(true);

	int width = scene.filmWidth;
	int height = scene.filmHeight;
	renderSize = { width, height };

	naivePathTracer = std::make_shared<NaivePathIntegrator>();
	naivePathTracer->init(&scene, width, height, pipeline);
	lightTracer = std::make_shared<LightPathIntegrator>();
	lightTracer->init(&scene, width, height, pipeline);
	triplePathTracer = std::make_shared<TriplePathIntegrator>();
	triplePathTracer->init(&scene, width, height, pipeline);

	globalQueuePathTracer = std::make_shared<GlobalQueuePathIntegrator>();
	globalQueuePathTracer->init(&scene, width, height, pipeline);
	blockQueuePathTracer = std::make_shared<BlockQueuePathIntegrator>();
	blockQueuePathTracer->init(&scene, width, height, pipeline);
	sharedQueuePathTracer = std::make_shared<SharedQueuePathIntegrator>();
	sharedQueuePathTracer->init(&scene, width, height, pipeline);

	bvhDisplayer = std::make_shared<BVHDisplayIntegrator>();
	bvhDisplayer->init(&scene, width, height, pipeline);
	rasterViewer = std::make_shared<RasterView>();
	rasterViewer->init(&scene, windowWidth, windowHeight, pipeline);
	rasterViewer->setStatus({ &scene, { windowWidth, windowHeight } });

	integrator = naivePathTracer;

	resultTex = Texture2D::createEmpty(width, height, TextureFormat::Col4x32f);
	Pipeline::bindTextureToImage(resultTex, UnitPostOut, 0, ImageAccess::WriteOnly, TextureFormat::Col4x32f);

	postShader = Shader::createFromText("post_proc.glsl", glm::ivec3{ PostProcBlockX, PostProcBlockY, 1 });
	postShader->set2i("uFilmSize", width, height);
	postShader->set1i("uToneMapper", Config::toneMapping);

	VerticalSyncStatus(Config::verticalSync);
	resetPreviewCamera();
	setMainCameraCurrent();
	reset();
}

void captureImage()
{
	std::string name = "screenshots/save" + std::to_string((long long)time(0)) + ".png";
	//auto img = GLContext::pipeline->readFramePixels();
	auto img = GLContext::resultTex->readFromDevice();
	stbi_flip_vertically_on_write(true);
	stbi_write_png(name.c_str(), img->width(), img->height(), 3, img->data(), img->width() * 3);
	Error::bracketLine<0>("Save " + name + " " + std::to_string(img->width()) + "x"
		+ std::to_string(img->height()));
}

void processKeys()
{
	if (GUI::fileSelector.isOpen())
		return;

	const int Keys[] = { GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D,
		GLFW_KEY_Q, GLFW_KEY_E, GLFW_KEY_R, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_SPACE };

	for (auto key : Keys)
	{
		if (isPressing(key))
		{
			GLContext::scene.previewCamera.move(key);
			if (Config::linkCameras)
			{
				setMainCameraCurrent();
				reset();
			}
		}
	}
}

void renderGUI()
{
	using namespace GLContext;
	using namespace Config;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Import", "Ctrl+O"))
				GUI::fileSelector.isOpen() = true;

			if (ImGui::MenuItem("Export", "Ctrl+S"));

			if (ImGui::MenuItem("Exit", "Esc"));
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Settings"))
		{
			rasterViewer->renderSettingsGUI();
			ImGui::Text("General");
			ImGui::PushItemWidth(200.0f);

			ImGui::SameLine();
			if (ImGui::Checkbox("Vertical sync ", &verticalSync))
				VerticalSyncStatus(verticalSync);

			ImGui::PopItemWidth();
			if (ImGui::SliderAngle("Env rotation", &scene.envRotation, -180.0f, 180.0f))
				reset();

			if (ImGui::Checkbox("Limit time", &limitTime) &&
				ImGui::GetTime() - renderBeginTime >= maxTime)
				reset();
			if (limitTime)
			{
				ImGui::SameLine();
				ImGui::SetNextItemWidth(120.0f);
				if (ImGui::InputDouble("Max time     ", &maxTime, 1.0, 10.0) &&
					ImGui::GetTime() - renderBeginTime >= maxTime)
					reset();
			}

			const char* ToneMappers[] = { "None", "Filmic", "ACES" };
			if (ImGui::Combo("Tone mapper", &GUI::toneIndex, ToneMappers, IM_ARRAYSIZE(ToneMappers)))
				postShader->set1i("uToneMapper", GUI::toneIndex);

			if (ImGui::DragInt2("Render size", glm::value_ptr(renderSize), 10.0, 1, 4096))
				reset();

			const char* IntegNames[] = { "NaivePath", "LightPath", "TriplePath",
				"GlobalQueuePath", "BlockQueuePath", "SharedQueuePath",
				"BVHDisplay" };
			IntegratorPtr integs[] = { naivePathTracer, lightTracer, triplePathTracer,
				globalQueuePathTracer, blockQueuePathTracer, sharedQueuePathTracer,
				bvhDisplayer };
			if (ImGui::Combo("Integrator", &GUI::integIndex, IntegNames, IM_ARRAYSIZE(IntegNames)))
			{
				integrator = integs[GUI::integIndex];
				reset(ResetLevel::ResetFrame);
			}
			ImGui::Separator();

			ImGui::Text("%s settings", IntegNames[GUI::integIndex]);
			integrator->renderSettingsGUI();
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Camera"))
		{
			if (cameraEditor(scene.camera, "Rendering camera"))
				reset();
			if (ImGui::Button("Set to preview camera"))
			{
				setMainCameraCurrent();
				reset();
			}
			if (cameraEditor(scene.previewCamera, "Preview camera"))
				reset();
			if (ImGui::Button("Set to default"))
				resetPreviewCamera();
			if (ImGui::Checkbox("Link cameras", &linkCameras) && linkCameras)
			{
				setMainCameraCurrent();
				reset();
			}
			ImGui::EndMenu();
		}
		
		if (ImGui::BeginMenu("Model"))
		{
			ImGui::SliderInt("Material", &GUI::matIndex, 0, scene.materials.size() - 1);
			
			if (materialEditor(scene, GUI::matIndex))
				reset();
			sceneGeomChanged |= modelEditor(scene, GUI::modelIndex, GUI::meshIndex, GUI::matIndex);
			//sceneGeomChanged |= lightEditor(scene, GUI::lightIndex);

			rasterViewer->setIndex(GUI::modelIndex, GUI::matIndex);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Statistics"))
		{
			ImGui::Text("BVH nodes:    %d", scene.boxCount);
			ImGui::Text("Triangles:    %d", scene.triangleCount);
			ImGui::Text("Vertices:     %d", scene.vertexCount);
			ImGui::Text("");
			ImGui::Text("Window size:  %dx%d", windowSize.x, windowSize.y);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			ImGui::BulletText("F1            Switch view/edit mode");
			ImGui::BulletText("Q/E          Rotate camera");
			ImGui::BulletText("R             Reset camera rotation");
			ImGui::BulletText("Scroll        Zoom in/out");
			ImGui::BulletText("W/A/S/D       Move camera horizonally");
			ImGui::BulletText("Shift/Space   Move camera vertically");
			ImGui::BulletText("Esc           Quit");
			ImGui::Text("");
			ImGui::Text("Modification of cameras and materials will instantly reflect in the renderer. "
				"However, models' and lights' won't because they require complete scene rebuilt, so you have to restart current rendering");
			ImGui::EndMenu();
		}

		if (!renderWindowIsOpen && ImGui::BeginMenu("Render*"))
		{
			renderWindowIsOpen = true;
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	if (renderWindowIsOpen)
	{
		ImGui::Begin("Render", &renderWindowIsOpen, ImGuiWindowFlags_AlwaysAutoResize);
		{
			if (!rendering && ImGui::Button(" Start "))
			{
				rendering = true;
				if (sceneGeomChanged)
					regenerateScene(false);
			}
			if (rendering && ImGui::Button("Restart"))
			{
				paused = false;
				if (sceneGeomChanged)
					regenerateScene(false);
				else
					reset();
			}

			ImGui::SameLine();
			if (!paused && ImGui::Button("Pause "))
				paused = true;
			if (paused && ImGui::Button("Resume"))
				paused = false;

			ImGui::SameLine();
			if (ImGui::Button("Save"))
				captureImage();

			ImGui::SameLine();
			if (ImGui::Checkbox("Preview", &preview))
				reset();
			if (preview)
			{
				ImGui::SetNextItemWidth(80.0f);
				ImGui::SameLine();
				if (ImGui::SliderInt("Scale", &previewScale, 2, 32))
					reset();
			}

			static float displayScale = 0.5f;
			ImGui::SliderFloat("Display scale", &displayScale, 0.0f, 1.0f);

			ImGui::Image((ImTextureID)resultTex->id(), { renderSize.x * displayScale, renderSize.y * displayScale },
				ImVec2(0, 1), ImVec2(1, 0));
			integrator->renderProgressGUI();

			ImGui::End();
		}
	}

	auto [width, height] = getWindowSize(mainWindow);
	ImGui::SetNextWindowPos({ static_cast<float>(width) - 280.0f, 40.0f });
	if (ImGui::Begin("Example: Simple overlay", nullptr,
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav))
	{
		using namespace InputRecord;

		if (limitTime)
			ImGui::ProgressBar((ImGui::GetTime() - renderBeginTime) / maxTime, ImVec2(-1, 0),
				std::to_string(ImGui::GetTime() - renderBeginTime).c_str());
		else
			ImGui::Text("Time: %lf", ImGui::GetTime() - renderBeginTime);

		double curTime = ImGui::GetTime();
		double frameTime = (curTime - lastTime) * 1000.0;
		const double Alpha = 0.25;
		accFrameTime = glm::mix(accFrameTime, frameTime, Alpha);
		ImGui::Text("Render Time: %.3lf ms, FPS: %.3lf", accFrameTime, 1000.0 / accFrameTime);
		lastTime = curTime;
		ImGui::End();
	}

	if (auto path = GUI::fileSelector.show())
	{
		if (scene.load(*path))
		{
			GUI::fileSelector.isOpen() = false;
			renderSize = { scene.filmWidth, scene.filmHeight };
			regenerateScene(true);
			resetPreviewCamera();
			setMainCameraCurrent();
		}
		else
			Error::bracketLine<0>(path->generic_string() + " is not a scene file");
	}

	//ImGui::ShowDemoWindow();

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void integrate()
{
	using namespace GLContext;
	using namespace Config;

	if ((rendering && !paused) && (!limitTime || ImGui::GetTime() - renderBeginTime < maxTime))
		integrator->renderOnePass();

	auto resultTex = integrator->getFrame();
	if (resultTex)
	{
		int numX = (renderSize.x + PostProcBlockX - 1) / PostProcBlockX;
		int numY = (renderSize.y + PostProcBlockY - 1) / PostProcBlockY;
		postShader->set1f("uResultScale", integrator->resultScale());
		postShader->set1i("uPreviewScale", previewScale);
		postShader->set1i("uPreview", preview);
		postShader->setTexture("uIn", integrator->getFrame(), 7);
		Pipeline::dispatchCompute(numX, numY, 1, postShader);
	}
}

int run()
{
	using namespace GLContext;
	using namespace Config;

	glfwShowWindow(mainWindow);
	while (true)
	{
		if (glfwWindowShouldClose(mainWindow))
			return 0;
		processKeys();

		integrate();
		
		pipeline->clear({ 0.0f, 0.0f, 0.0f, 1.0f });
		rasterViewer->renderOnePass();
		renderGUI();

		glfwSwapBuffers(mainWindow);
		glfwPollEvents();
	}
	return -1;
}

NAMESPACE_END(Application)