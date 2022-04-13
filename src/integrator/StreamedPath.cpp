#include "../core/Integrator.h"

const int PrimaryBlockSizeX = 48;
const int PrimaryBlockSizeY = 32;
const int StreamingBlockSize = 1536;

void StreamedPathIntegrator::recreateFrameTex(int width, int height)
{
	mFrameTex = Texture2D::createEmpty(width, height, TextureFormat::Col4x32f);
	mFrameTex->setFilter(TextureFilter::Nearest);
	mFilmTex = Texture2D::createEmpty(width * 3, height, TextureFormat::Col1x32f);
	mFilmTex->setFilter(TextureFilter::Nearest);

	size_t queueSize = width * height;

	mPositionQueue = TextureBuffered::createTyped<glm::vec4>(nullptr, queueSize, TextureFormat::Col4x32f);
	mThroughputQueue = TextureBuffered::createTyped<glm::vec4>(nullptr, queueSize, TextureFormat::Col4x32f);
	mWoQueue = TextureBuffered::createTyped<glm::vec4>(nullptr, queueSize, TextureFormat::Col4x32f);
	mObjIdQueue = TextureBuffered::createTyped<int>(nullptr, queueSize + 2, TextureFormat::Col1x32i);

	Pipeline::bindTextureToImage(mFilmTex, 0, 0, ImageAccess::ReadWrite, TextureFormat::Col1x32f);
	Pipeline::bindTextureToImage(mFrameTex, 1, 0, ImageAccess::WriteOnly, TextureFormat::Col4x32f);
	Pipeline::bindTextureToImage(mPositionQueue, 2, 0, ImageAccess::ReadWrite, TextureFormat::Col4x32f);
	Pipeline::bindTextureToImage(mThroughputQueue, 3, 0, ImageAccess::ReadWrite, TextureFormat::Col4x32f);
	Pipeline::bindTextureToImage(mWoQueue, 4, 0, ImageAccess::ReadWrite, TextureFormat::Col4x32f);
	Pipeline::bindTextureToImage(mObjIdQueue, 5, 0, ImageAccess::ReadWrite, TextureFormat::Col1x32i);
}

void StreamedPathIntegrator::updateUniforms(const Scene& scene, int width, int height)
{
	auto& sceneBuffers = scene.glContext;
	const auto& camera = scene.camera;
	glm::mat3 camMatrix(camera.right(), camera.up(), camera.front());

	Shader* shaders[] = { mPrimaryRayShader.get(), mStreamingShader.get() };
	for (auto shader : shaders)
	{
		shader->setTexture("uVertices", sceneBuffers.vertex, 6);
		shader->setTexture("uNormals", sceneBuffers.normal, 7);
		shader->setTexture("uTexCoords", sceneBuffers.texCoord, 8);
		shader->setTexture("uIndices", sceneBuffers.index, 9);
		shader->setTexture("uBounds", sceneBuffers.bound, 10);
		shader->setTexture("uHitTable", sceneBuffers.hitTable, 11);
		shader->setTexture("uMatTexIndices", sceneBuffers.matTexIndex, 12);
		shader->setTexture("uEnvMap", scene.envMap->envMap(), 13);
		shader->setTexture("uEnvAliasTable", scene.envMap->aliasTable(), 14);
		shader->setTexture("uEnvAliasProb", scene.envMap->aliasProb(), 15);
		shader->setTexture("uMaterials", sceneBuffers.material, 16);
		shader->setTexture("uMatTypes", sceneBuffers.material, 16);
		shader->setTexture("uLightPower", sceneBuffers.lightPower, 17);
		shader->setTexture("uLightAlias", sceneBuffers.lightAlias, 18);
		shader->setTexture("uLightProb", sceneBuffers.lightProb, 19);
		shader->setTexture("uTextures", sceneBuffers.textures, 20);
		shader->setTexture("uTexUVScale", sceneBuffers.texUVScale, 21);
		shader->setTexture("uSobolSeq", scene.sobolTex, 22);
		shader->setTexture("uNoiseTex", scene.noiseTex, 23);
		shader->set1i("uNumLightTriangles", scene.nLightTriangles);
		shader->set1i("uObjPrimCount", scene.objPrimCount);

		shader->set1f("uLightSum", scene.lightSumPdf);
		shader->set1f("uEnvSum", scene.envMap->sumPdf());
		shader->set1i("uBvhSize", scene.boxCount);
		shader->set1i("uSampleDim", scene.SampleDim);
		shader->set1i("uSampleNum", scene.SampleNum);
		shader->set1f("uEnvRotation", scene.envRotation);
		shader->set1i("uSampler", scene.sampler);

		shader->setVec3("uCamF", camera.front());
		shader->setVec3("uCamR", camera.right());
		shader->setVec3("uCamU", camera.up());
		shader->setMat3("uCamMatInv", glm::inverse(camMatrix));
		shader->setVec3("uCamPos", camera.pos());
		shader->set1f("uTanFOV", glm::tan(glm::radians(camera.FOV() * 0.5f)));
		shader->set1f("uCamAsp", camera.aspect());
		shader->set1f("uLensRadius", camera.lensRadius());
		shader->set1f("uFocalDist", camera.focalDist());
		shader->set2i("uFilmSize", width, height);

		shader->set1i("uRussianRoulette", mParam.russianRoulette);
		shader->set1i("uMaxDepth", mParam.maxDepth);
		shader->set1i("uSampleLight", mParam.sampleLight);
		shader->set1i("uLightEnvUniformSample", mParam.lightEnvUniformSample);
		shader->set1f("uLightSamplePortion", mParam.lightPortion);
	}
}

void StreamedPathIntegrator::init(const Scene& scene, int width, int height, PipelinePtr ctx)
{
	mPrimaryRayShader = Shader::createFromText("pt_streamed_primary.glsl", { PrimaryBlockSizeX, PrimaryBlockSizeY, 1 },
		"#extension GL_EXT_texture_array : enable\n"
		"#extension GL_NV_shader_atomic_float : enable\n");
	mStreamingShader = Shader::createFromText("pt_streamed_streaming.glsl", { PrimaryBlockSizeX, PrimaryBlockSizeY, 1 },
		"#extension GL_EXT_texture_array : enable\n"
		"#extension GL_NV_shader_atomic_float : enable\n");
	recreateFrameTex(width, height);
	updateUniforms(scene, width, height);
}

void StreamedPathIntegrator::renderOnePass()
{
	mFreeCounter++;
	if (mShouldReset)
	{
		reset(*mResetStatus.scene, mResetStatus.renderSize.x, mResetStatus.renderSize.y);
		mShouldReset = false;
	}
	if (mParam.finiteSample && mCurSample > mParam.maxSample)
	{
		mRenderFinished = true;
		return;
	}

	uint64_t index = 0;
	mObjIdQueue->write(0, 8, &index);

	int width = mResetStatus.renderSize.x;
	int height = mResetStatus.renderSize.y;

	int numX = (width + PrimaryBlockSizeX - 1) / PrimaryBlockSizeX;
	int numY = (height + PrimaryBlockSizeY - 1) / PrimaryBlockSizeY;

	mPrimaryRayShader->set1i("uSpp", mCurSample);
	mPrimaryRayShader->set1i("uFreeCounter", mFreeCounter);

	Pipeline::dispatchCompute(numX, numY, 1, mPrimaryRayShader);
	Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);

	mCurSample++;
}

void StreamedPathIntegrator::reset(const Scene& scene, int width, int height)
{
	if (width != mFrameTex->width() && height != mFrameTex->height())
		recreateFrameTex(width, height);
	updateUniforms(scene, width, height);
	mCurSample = 0;
}

void StreamedPathIntegrator::renderSettingsGUI()
{
	ImGui::SetNextItemWidth(80.0f);
	if (ImGui::InputInt("Max depth", &mParam.maxDepth, 1, 1))
		setShouldReset();

	ImGui::SameLine();
	if (ImGui::Checkbox("Russian rolette", &mParam.russianRoulette))
		setShouldReset();

	if (ImGui::Checkbox("Direct sample", &mParam.sampleLight))
		setShouldReset();
	if (mParam.sampleLight)
	{
		ImGui::Text("  ");
		ImGui::SameLine();
		ImGui::Checkbox("Uniform", &mParam.lightEnvUniformSample);
		if (!mParam.lightEnvUniformSample)
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(80.0f);
			if (ImGui::SliderFloat("Light portion", &mParam.lightPortion, 0.001f, 0.999f))
				setShouldReset();
		}
	}
	if (ImGui::Checkbox("Limit spp", &mParam.finiteSample) && mCurSample > mParam.maxSample)
		setShouldReset();
	if (mParam.finiteSample)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::InputInt("Max spp      ", &mParam.maxSample, 1, 10) &&
			mCurSample > mParam.maxSample)
			setShouldReset();
	}
}

void StreamedPathIntegrator::renderProgressGUI()
{
	if (mParam.finiteSample)
		ImGui::ProgressBar(static_cast<float>(mCurSample) / mParam.maxSample);
	else
		ImGui::Text("Spp: %d", mCurSample);
}