#include "Application.h"

std::pair<int, int> getWindowSize(GLFWwindow* window)
{
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	return { width, height };
}

NAMESPACE_BEGIN(Application)

GLFWwindow* mainWindow = nullptr;
IntegratorPtr integrators[4];

namespace InputRecord
{
	int lastCursorX = 0;
	int lastCursorY = 0;
	bool firstCursorMove = true;
	bool cursorDisabled = false;

	std::set<int> pressedKeys;
}

namespace GLContext
{
	PipelinePtr pipeline;
	Texture2DPtr frameTex;
	VertexBufferPtr screenVB;
	Scene scene;
	ShaderPtr postShader;
}

namespace SampleCounter
{
	int curSpp = 0;
	int freeCounter = 0;
}

namespace Settings
{
	int uiMeshIndex = 0;
	int uiMatIndex = 0;
	int uiEnvIndex;
	int uiIntegIndex = 0;

	bool showBVH = false;
	bool cursorDisabled = false;

	bool pause = false;
	bool verticalSync = false;

	bool preview = true;
	int previewScale = 8;

	bool limitSpp = false;
	int maxSpp = 64;

	int toneMapping = 2;
}

bool count()
{
	using namespace SampleCounter;
	using namespace Settings;
	freeCounter++;
	if (curSpp < maxSpp || !limitSpp)
	{
		curSpp++;
		return true;
	}
	return false;
}

void reset()
{
	SampleCounter::curSpp = 0;
	//GLContext::rayTraceShader->set1i("uSpp", SampleCounter::curSpp);
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
			Settings::preview ^= 1;
			reset();
		}
		else if (key == GLFW_KEY_B)
		{
			Settings::showBVH ^= 1;
			reset();
		}
		else if (key == GLFW_KEY_V)
		{
			Settings::verticalSync ^= 1;
			VerticalSyncStatus(Settings::verticalSync);
		}
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
	//updateCameraUniforms();
	reset();
}

void scrollCallback(GLFWwindow* window, double offsetX, double offsetY)
{
	if (!InputRecord::cursorDisabled)
		return;
	GLContext::scene.camera.changeFOV(offsetY);
	//updateCameraUniforms();
	reset();
}

void resizeCallback(GLFWwindow* window, int width, int height)
{
	Error::bracketLine<0>("Resize to " + std::to_string(width) + "x" + std::to_string(height));

	using namespace GLContext;
	frameTex = Texture2D::createEmpty(width, height, TextureFormat::Col4x32f);
	frameTex->setFilter(TextureFilter::Nearest);
	Pipeline::bindTextureToImage(frameTex, 0, 0, ImageAccess::ReadWrite, TextureFormat::Col4x32f);
	pipeline->setViewport(0, 0, width, height);
	scene.camera.setAspect(static_cast<float>(width) / height);

	//rayTraceShader->set2i("uFilmSize", width, height);
	//rayTraceShader->set1f("uCamAsp", scene.camera.aspect());
	//postShader->setTexture("uFrame", frameTex, 0);

	reset();
}

void init(int width, int height, const std::string& title)
{
	if (glfwInit() != GLFW_TRUE)
		Error::exit("failed to init GLFW");

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_DECORATED, true);
	glfwWindowHint(GLFW_RESIZABLE, true);
	glfwWindowHint(GLFW_VISIBLE, false);

	mainWindow = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
	glfwMakeContextCurrent(mainWindow);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		Error::exit("failed to init GLAD");

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
	pipeline->setViewport(0, 0, width, height);

	const float ScreenCoord[] =
	{
		0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
	};
	screenVB = VertexBuffer::createTyped<glm::vec2>(ScreenCoord, 6);

	scene.load();
	scene.createGLContext();

	glfwSetKeyCallback(mainWindow, keyCallback);
	glfwSetCursorPosCallback(mainWindow, cursorCallback);
	glfwSetScrollCallback(mainWindow, scrollCallback);
	glfwSetWindowSizeCallback(mainWindow, resizeCallback);

	frameTex = Texture2D::createEmpty(width, height, TextureFormat::Col4x32f);
	postShader = Shader::create("post_proc.glsl");
	postShader->set1i("uToneMapper", Settings::toneMapping);
	postShader->setTexture("uFrame", frameTex, 0);

	integrators[0] = std::make_shared<PathIntegrator>();
}

void captureImage()
{
	std::string name = "screenshots/save" + std::to_string((long long)time(0)) + ".png";
	auto img = GLContext::pipeline->readFramePixels();
	stbi_flip_vertically_on_write(true);
	stbi_write_png(name.c_str(), img->width(), img->height(), 3, img->data(), img->width() * 3);
	Error::bracketLine<0>("Save " + name + " " + std::to_string(img->width()) + "x"
		+ std::to_string(img->height()));
}

void renderGUI()
{
	using namespace GLContext;
	using namespace Settings;
	using namespace SampleCounter;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::ShowDemoWindow();

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open", "Ctrl+O"));
			if (ImGui::MenuItem("Save", "Ctrl+S"));

			if (ImGui::MenuItem("Save image", nullptr))
				captureImage();

			if (ImGui::MenuItem("Exit", "Esc"));
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Settings"))
		{
			ImGui::Text("General");
			ImGui::PushItemWidth(200.0f);
			if (ImGui::Checkbox("Display BVH   ", &showBVH))
				reset();

			ImGui::SameLine();
			if (ImGui::Checkbox("Vertical sync ", &verticalSync))
				VerticalSyncStatus(verticalSync);

			if (ImGui::Checkbox("Preview mode  ", &preview))
				reset();
			if (preview)
			{
				ImGui::SetNextItemWidth(80.0f);
				ImGui::SameLine();
				if (ImGui::SliderInt("Preview scale", &previewScale, 2, 32))
					reset();
			}

			if (ImGui::Checkbox("Limit spp     ", &limitSpp) && curSpp > maxSpp)
				reset();
			if (limitSpp)
			{
				ImGui::SetNextItemWidth(80.0f);
				ImGui::SameLine();
				if (ImGui::InputInt("Max spp      ", &maxSpp, 1, 10) && curSpp > maxSpp)
					reset();
			}
			ImGui::PopItemWidth();
			ImGui::SliderAngle("Env rotation", &scene.envRotation, -180.0f, 180.0f);
			ImGui::Separator();

			const char* IntegNames[] = { "Path", "LightPath", "TriPath", "BDPT" };
			if (ImGui::Combo("Integrator", &uiIntegIndex, IntegNames, IM_ARRAYSIZE(IntegNames)))
			{
				//integrators[uiIntegIndex]->reset();
			}
			integrators[uiIntegIndex]->renderSettingsGUI();

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Camera"))
		{
			auto& camera = scene.camera;
			auto pos = camera.pos();
			auto fov = camera.FOV();
			auto angle = camera.angle();
			auto lensRadius = camera.lensRadius();
			auto focalDist = camera.focalDist();
			if (ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.1f) ||
				ImGui::DragFloat3("Angle", glm::value_ptr(angle), 0.05f) ||
				ImGui::SliderFloat("FOV", &fov, 0.0f, 90.0f) ||
				ImGui::DragFloat("Lens radius", &lensRadius, 0.001f, 0.0f, 10.0f) ||
				ImGui::DragFloat("Focal distance", &focalDist, 0.01f, 0.004f, 100.0f))
			{
				camera.setPos(pos);
				camera.setAngle(angle);
				camera.setFOV(fov);
				camera.setLensRadius(lensRadius);
				camera.setFocalDist(focalDist);
				reset();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Statistics"))
		{
			ImGui::Text("BVH nodes:    %d", scene.boxCount);
			ImGui::Text("Triangles:    %d", scene.triangleCount);
			ImGui::Text("Vertices:     %d", scene.vertexCount);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			ImGui::BulletText("F1            Switch view/edit mode");
			ImGui::BulletText("Q/E			Rotate camera");
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
		if (limitSpp)
			ImGui::Text("Spp: %d/%d", curSpp, maxSpp);
		else
			ImGui::Text("Spp: %d", curSpp);
		ImGui::Text("Render Time: %.3f ms, FPS: %.3f", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}

	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

int run()
{
	using namespace GLContext;
	using namespace Settings;

	glfwShowWindow(mainWindow);
	while (true)
	{
		if (glfwWindowShouldClose(mainWindow))
			return 0;

		// integrator->renderTo(frameTex);

		pipeline->clear({ 0.0f, 0.0f, 0.0f, 1.0f });
		postShader->set1i("uPreview", preview);
		postShader->set1i("uPreviewScale", previewScale);
		postShader->set1f("uResultScale", 1.0f / (SampleCounter::curSpp + 1));
		pipeline->draw(screenVB, VertexArray::layoutPos2(), postShader);

		renderGUI();

		glfwSwapBuffers(mainWindow);
		glfwPollEvents();
	}
	return -1;
}

NAMESPACE_END(Application)