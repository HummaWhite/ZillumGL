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

void updateCameraUniforms()
{
	using namespace GLContext;
	const auto& cam = scene.camera;
	rayTraceShader->setVec3("uCamF", cam.front());
	rayTraceShader->setVec3("uCamR", cam.right());
	rayTraceShader->setVec3("uCamU", cam.up());
	glm::mat3 camMatrix(cam.right(), cam.up(), cam.front());
	rayTraceShader->setMat3("uCamMatInv", glm::inverse(camMatrix));
	rayTraceShader->setVec3("uCamPos", cam.pos());
	rayTraceShader->set1f("uTanFOV", glm::tan(glm::radians(cam.FOV() * 0.5f)));
	rayTraceShader->set1f("uCamAsp", cam.aspect());
	rayTraceShader->set1f("uLensRadius", scene.lensRadius);
	rayTraceShader->set1f("uFocalDist", scene.focalDist);
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
	{
		updateCameraUniforms();
		resetCounter();
	}
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

void init(int width, int height, GLFWwindow* window)
{
	auto& sceneBuffers = scene.glContext;

	Count::boxCount = sceneBuffers.bound->size() / sizeof(AABB);
	Count::vertexCount = sceneBuffers.vertex->size() / sizeof(glm::vec3);
	Count::triangleCount = sceneBuffers.index->size() / sizeof(uint32_t) / 3;

	frameTex = Texture2D::createEmpty(width, height, TextureFormat::Col4x32f);
	
	rayTraceShader = Shader::create("main.glsl", { WorkgroupSizeX, WorkgroupSizeY, 1 },
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

	postShader = Shader::create("post_proc.glsl");
	postShader->set1i("uToneMapper", scene.toneMapping);
	postShader->setTexture("uFrame", frameTex, 0);

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
	using namespace Settings;
	
	if (curSpp < maxSpp || !limitSpp)
	{
		int width = preview ? frameTex->width() / previewScale : frameTex->width();
		int height = preview ? frameTex->height() / previewScale : frameTex->height();
		int numX = (width + WorkgroupSizeX - 1) / WorkgroupSizeX;
		int numY = (height + WorkgroupSizeY - 1) / WorkgroupSizeY;

		rayTraceShader->set1i("uShowBVH", Settings::showBVH);
		rayTraceShader->set1i("uSpp", curSpp);
		rayTraceShader->set1i("uFreeCounter", freeCounter);
		rayTraceShader->set2i("uFilmSize", width, height);

		Pipeline::dispatchCompute(numX, numY, 1, rayTraceShader);
		Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);
	}

	pipeline->clear({ 0.0f, 0.0f, 0.0f, 1.0f });
	postShader->set1i("uPreview", preview);
	postShader->set1i("uPreviewScale", previewScale);
	postShader->set1f("uResultScale", 1.0f / (curSpp + 1));
	pipeline->draw(screenVB, VertexArray::layoutPos2(), postShader);
}

void materialEditor(Scene& scene, int matIndex)
{
	auto& m = scene.materials[matIndex];
	const char* matTypes[] = { "Principled", "MetalWorkflow", "Dielectric", "ThinDielectric" };
	if (ImGui::Combo("Type", &m.type, matTypes, IM_ARRAYSIZE(matTypes)))
	{
		scene.glContext.material->write(sizeof(Material) * matIndex, sizeof(Material), &m);
		resetCounter();
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
			resetCounter();
		}
	}
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
			if (ImGui::MenuItem("Save", "Ctrl+S"))
			{
			}

			if (ImGui::MenuItem("Save image", nullptr))
				captureImage();

			if (ImGui::MenuItem("Exit", "Esc"));
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Settings"))
		{
			if (ImGui::MenuItem("Display BVH", "Ctrl+B", &showBVH))
				resetCounter();

			if (ImGui::MenuItem("Preview mode", "Ctrl+P", &preview))
				resetCounter();
			if (preview)
			{
				if (ImGui::SliderInt("Preview scale", &previewScale, 2, 32))
					resetCounter();
			}

			if (ImGui::MenuItem("Vertical sync", nullptr, &verticalSync))
				VerticalSyncStatus(verticalSync);

			if (ImGui::MenuItem("Russian roulette", nullptr, &russianRoulette))
			{
				rayTraceShader->set1i("uRussianRoulette", russianRoulette);
				resetCounter();
			}

			if (ImGui::MenuItem("Limit spp", nullptr, &limitSpp) && curSpp > maxSpp)
				resetCounter();
			if (limitSpp)
			{
				if (ImGui::DragInt("Max spp", &maxSpp, 1.0f, 0, 131072) && curSpp > maxSpp)
					resetCounter();
			}

			if (ImGui::MenuItem("Ambient occlusion", nullptr, &scene.aoMode))
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

			if (ImGui::SliderInt("Max bounces", &scene.maxBounce, 0, 60))
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

			if (ImGui::Checkbox("Sample direct light", &scene.sampleLight))
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
					rayTraceShader->set1i("uMatIndex", uiMatIndex);
					if (showBVH)
						resetCounter();
				}
				materialEditor(scene, uiMatIndex);
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

			if (ImGui::SliderAngle("EnvMap rotation", &scene.envRotation, -180.0f, 180.0f))
			{
				rayTraceShader->set1f("uEnvRotation", scene.envRotation);
				resetCounter();
			}

			if (scene.nLightTriangles > 0 &&
				ImGui::Checkbox("Manual sample portion", &scene.lightEnvUniformSample))
			{
				rayTraceShader->set1i("uLightEnvUniformSample", scene.lightEnvUniformSample);
				resetCounter();
			}

			if (scene.lightEnvUniformSample &&
				ImGui::SliderFloat("Light portion", &scene.lightPortion, 1e-4f, 1.0f - 1e-4f))
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
				ImGui::DragFloat("Focal distance", &scene.focalDist, 0.01f, 0.004f, 100.0f) ||
				ImGui::DragFloat("Lens radius", &scene.lensRadius, 0.001f, 0.0f, 10.0f))
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
			ImGui::Text("BVH nodes:    %d", boxCount);
			ImGui::Text("Triangles:    %d", triangleCount);
			ImGui::Text("Vertices:     %d", vertexCount);
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

NAMESPACE_END(PathTracer)