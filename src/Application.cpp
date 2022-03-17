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
	VertexBufferPtr screenVB;
	Scene scene;
	ShaderPtr postShader;
	glm::ivec2 frameSize;
}

namespace Config
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

	int toneMapping = 2;
}

void reset()
{
	using namespace GLContext;
	using namespace Config;
	auto& integrator = integrators[uiIntegIndex];
	glm::ivec2 resetFrameSize = preview ? frameSize / previewScale : frameSize;
	integrator->setResetStatus({ &scene, resetFrameSize });
	integrator->setShouldReset();
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
		else if (key == GLFW_KEY_B)
		{
			Config::showBVH ^= 1;
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
	reset();
}

void scrollCallback(GLFWwindow* window, double offsetX, double offsetY)
{
	if (!InputRecord::cursorDisabled)
		return;
	GLContext::scene.camera.changeFOV(offsetY);
	reset();
}

void resizeCallback(GLFWwindow* window, int width, int height)
{
	Error::bracketLine<0>("Resize to " + std::to_string(width) + "x" + std::to_string(height));
	using namespace GLContext;
	pipeline->setViewport(0, 0, width, height);
	scene.camera.setAspect(static_cast<float>(width) / height);
	frameSize = { width, height };
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

	glfwSetKeyCallback(mainWindow, keyCallback);
	glfwSetCursorPosCallback(mainWindow, cursorCallback);
	glfwSetScrollCallback(mainWindow, scrollCallback);
	glfwSetWindowSizeCallback(mainWindow, resizeCallback);

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

	integrators[0] = std::make_shared<PathIntegrator>();
	auto& integrator = integrators[Config::uiIntegIndex];
	integrator->init(scene, width, height);

	postShader = Shader::create("post_proc.glsl");
	postShader->set1i("uToneMapper", Config::toneMapping);
	postShader->setTexture("uFrame", integrator->getFrame(), 0);

	VerticalSyncStatus(Config::verticalSync);
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

void processKeys()
{
	const int Keys[] = { GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D,
		GLFW_KEY_Q, GLFW_KEY_E, GLFW_KEY_R, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_SPACE };

	bool updated = false;
	for (auto key : Keys)
	{
		if (isPressing(key))
		{
			GLContext::scene.camera.move(key);
			updated = true;
		}
	}

	if (updated)
		reset();
}

void materialEditor(Scene& scene, int matIndex)
{
	auto& m = scene.materials[matIndex];
	const char* matTypes[] = { "Principled", "MetalWorkflow", "Dielectric", "ThinDielectric" };
	if (ImGui::Combo("Type", &m.type, matTypes, IM_ARRAYSIZE(matTypes)))
	{
		scene.glContext.material->write(sizeof(Material) * matIndex, sizeof(Material), &m);
		reset();
	}

	if (m.type == Material::Principled)
	{
		if (ImGui::ColorEdit3("BaseColor", glm::value_ptr(m.baseColor)) ||
			ImGui::SliderFloat("Subsurface", &m.subsurface, 0.0f, 1.0f) ||
			ImGui::SliderFloat("Metallic", &m.metallic, 0.0f, 1.0f) ||
			ImGui::SliderFloat("Roughness", &m.roughness, 0.0f, 1.0f) ||
			ImGui::SliderFloat("Specular", &m.specular, 0.0f, 1.0f) ||
			ImGui::SliderFloat("Specular tint", &m.specularTint, 0.0f, 1.0f) ||
			ImGui::SliderFloat("Sheen", &m.sheen, 0.0f, 1.0f) ||
			ImGui::SliderFloat("Sheen tint", &m.sheenTint, 0.0f, 1.0f) ||
			ImGui::SliderFloat("Clearcoat", &m.clearcoat, 0.0f, 1.0f) ||
			ImGui::SliderFloat("Clearcoat gloss", &m.clearcoatGloss, 0.0f, 1.0f))
		{
			scene.glContext.material->write(sizeof(Material) * matIndex, sizeof(Material), &m);
			reset();
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

			ImGui::PopItemWidth();
			if (ImGui::SliderAngle("Env rotation", &scene.envRotation, -180.0f, 180.0f))
				reset();
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

		if (ImGui::BeginMenu("Material"))
		{
			ImGui::SliderInt("Index", &uiMatIndex, 0, scene.materials.size() - 1);
			materialEditor(scene, uiMatIndex);
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
		integrators[uiIntegIndex]->renderProgressGUI();
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
	using namespace Config;

	glfwShowWindow(mainWindow);
	while (true)
	{
		if (glfwWindowShouldClose(mainWindow))
			return 0;
		processKeys();

		auto& integrator = integrators[uiIntegIndex];
		integrator->renderOnePass();

		pipeline->clear({ 0.0f, 0.0f, 0.0f, 1.0f });
		postShader->setTexture("uFrame", integrator->getFrame(), 0);
		postShader->set1f("uResultScale", integrator->resultScale());
		pipeline->draw(screenVB, VertexArray::layoutPos2(), postShader);

		renderGUI();

		glfwSwapBuffers(mainWindow);
		glfwPollEvents();
	}
	return -1;
}

NAMESPACE_END(Application)