#include "../core/Integrator.h"

const int PrimaryBlockSizeX = 8;
const int PrimaryBlockSizeY = 4;

const int ImageOpSizeX = 48;
const int ImageOpSizeY = 32;

void SharedQueuePathIntegrator::recreateFrameTex(int width, int height)
{
	mFrameTex = Texture2D::createEmpty(width, height, TextureFormat::Col4x32f);
	mFrameTex->setFilter(TextureFilter::Nearest);
	size_t queueSize = static_cast<size_t>(width) * height;

	Pipeline::bindTextureToImage(mFrameTex, 0, 0, ImageAccess::WriteOnly, TextureFormat::Col4x32f);
}

void SharedQueuePathIntegrator::updateUniforms(const RenderStatus& status)
{
	auto [scene, size, level] = status;

	auto& sceneBuffers = scene->glContext;
	const auto& camera = scene->camera;
	glm::mat3 camMatrix(camera.right(), camera.up(), camera.front());

	mShader->setTexture("uVertices", sceneBuffers.vertex, 6);
	mShader->setTexture("uNormals", sceneBuffers.normal, 7);
	mShader->setTexture("uTexCoords", sceneBuffers.texCoord, 8);
	mShader->setTexture("uIndices", sceneBuffers.index, 9);
	mShader->setTexture("uBounds", sceneBuffers.bound, 10);
	mShader->setTexture("uHitTable", sceneBuffers.hitTable, 11);
	mShader->setTexture("uMatTexIndices", sceneBuffers.matTexIndex, 12);
	mShader->setTexture("uEnvMap", scene->envMap->envMap(), 13);
	mShader->setTexture("uEnvAliasTable", scene->envMap->aliasTable(), 14);
	mShader->setTexture("uEnvAliasProb", scene->envMap->aliasProb(), 15);
	mShader->setTexture("uMaterials", sceneBuffers.material, 16);
	mShader->setTexture("uMatTypes", sceneBuffers.material, 16);
	mShader->setTexture("uLightPower", sceneBuffers.lightPower, 17);
	mShader->setTexture("uLightAlias", sceneBuffers.lightAlias, 18);
	mShader->setTexture("uLightProb", sceneBuffers.lightProb, 19);
	mShader->setTexture("uTextures", sceneBuffers.textures, 20);
	mShader->setTexture("uTexUVScale", sceneBuffers.texUVScale, 21);
	mShader->setTexture("uSobolSeq", scene->sobolTex, 22);
	mShader->setTexture("uNoiseTex", scene->noiseTex, 23);
	mShader->set1i("uNumLightTriangles", scene->nLightTriangles);
	mShader->set1i("uObjPrimCount", scene->objPrimCount);

	mShader->set1f("uLightSum", scene->lightSumPdf);
	mShader->set1f("uEnvSum", scene->envMap->sumPdf());
	mShader->set1i("uBvhSize", scene->boxCount);
	mShader->set1i("uSampleDim", scene->SampleDim);
	mShader->set1i("uSampleNum", scene->SampleNum);
	mShader->set1f("uEnvRotation", scene->envRotation);
	mShader->set1i("uLightEnvUniformSample", mParam.lightEnvUniformSample);
	mShader->set1f("uLightSamplePortion", mParam.lightPortion);
	mShader->set1i("uSampler", scene->sampler);
	mShader->setVec2i("uFilmSize", size);
	mShader->set1i("uMaxDepth", mParam.maxDepth);

	mShader->setVec3("uCamF", camera.front());
	mShader->setVec3("uCamR", camera.right());
	mShader->setVec3("uCamU", camera.up());
	mShader->setMat3("uCamMatInv", glm::inverse(camMatrix));
	mShader->setVec3("uCamPos", camera.pos());
	mShader->set1f("uTanFOV", glm::tan(glm::radians(camera.FOV() * 0.5f)));
	mShader->set1f("uCamAsp", camera.aspect());
	mShader->set1f("uLensRadius", camera.lensRadius());
	mShader->set1f("uFocalDist", camera.focalDist());

	mShader->set1i("uRussianRoulette", mParam.russianRoulette);
	mShader->set1i("uSampleLight", mParam.sampleLight);

	mImageClearShader->setVec2i("uTexSize", size);
}

void SharedQueuePathIntegrator::init(Scene* scene, int width, int height, PipelinePtr ctx)
{
	mShader = Shader::createFromText("pt_shared_queue.glsl", { PrimaryBlockSizeX, PrimaryBlockSizeY, 1 },
		"#extension GL_EXT_texture_array : enable\n");
	mImageClearShader = Shader::createFromText("util/img_clear_4x32f.glsl", { ImageOpSizeX, ImageOpSizeY, 1 });
	recreateFrameTex(width, height);
	updateUniforms({ scene, { width, height } });
}

void SharedQueuePathIntegrator::renderOnePass()
{
	mFreeCounter++;
	if (mShouldReset)
	{
		reset(mStatus);
		mShouldReset = false;
	}
	if (mParam.finiteSample && mCurSample > mParam.maxSample)
	{
		mRenderFinished = true;
		return;
	}

	int width = mStatus.renderSize.x;
	int height = mStatus.renderSize.y;

	int numX = (width + PrimaryBlockSizeX - 1) / PrimaryBlockSizeX;
	int numY = (height + PrimaryBlockSizeY - 1) / PrimaryBlockSizeY;

	mShader->set1i("uSpp", mCurSample);
	mShader->set1i("uFreeCounter", mFreeCounter);

	Pipeline::dispatchCompute(numX, numY, 1, mShader);
	Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);

	mCurSample++;
	mParam.samplePerPixel += 1.0f;
}

void SharedQueuePathIntegrator::reset(const RenderStatus& status)
{
	int width = status.renderSize.x;
	int height = status.renderSize.y;
	if (mFrameTex->size() != status.renderSize)
		recreateFrameTex(width, height);
	else
	{
		int numX = (width + ImageOpSizeX - 1) / ImageOpSizeX;
		int numY = (height + ImageOpSizeY - 1) / ImageOpSizeY;
		Pipeline::dispatchCompute(numX, numY, 1, mImageClearShader);
		Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);
	}
	updateUniforms(status);
	mCurSample = 0;
	mParam.samplePerPixel = 1.0f;
}

void SharedQueuePathIntegrator::renderSettingsGUI()
{
	ImGui::SetNextItemWidth(80.0f);
	if (ImGui::InputInt("Max depth", &mParam.maxDepth, 1, 1))
		setShouldReset();

	ImGui::SameLine();
	if (ImGui::Checkbox("Russian rolette", &mParam.russianRoulette))
		setShouldReset();

	const char* samplerNames[] = { "Independent", "Sobol" };
	if (ImGui::Combo("Sampler", &mParam.sampler, samplerNames, IM_ARRAYSIZE(samplerNames)))
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

void SharedQueuePathIntegrator::renderProgressGUI()
{
	if (mParam.finiteSample)
		ImGui::ProgressBar(static_cast<float>(mCurSample) / mParam.maxSample);
	else
		ImGui::Text("Spp: %d", mCurSample);
}