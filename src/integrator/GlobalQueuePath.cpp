#include "../core/Integrator.h"

const int PrimaryBlockSizeX = 48;
const int PrimaryBlockSizeY = 32;
const int StreamingBlockSize = 64;

const int ImageOpSizeX = 48;
const int ImageOpSizeY = 32;

void GlobalQueuePathIntegrator::recreateFrameTex(int width, int height)
{
	mFrameTex = Texture2D::createEmpty(width, height, TextureFormat::Col4x32f);
	mFrameTex->setFilter(TextureFilter::Nearest);
	size_t queueSize = static_cast<size_t>(width) * height;

	mPosWoxQueue = TextureBuffered::createTyped<glm::vec4>(nullptr, queueSize + 1, TextureFormat::Col4x32f);
	mTputWoyQueue = TextureBuffered::createTyped<glm::vec4>(nullptr, queueSize + 1, TextureFormat::Col4x32f);
	mUVWozQueue = TextureBuffered::createTyped<glm::ivec4>(nullptr, queueSize + 1, TextureFormat::Col4x32i);
	mIdQueue = TextureBuffered::createTyped<int>(nullptr, queueSize + 3, TextureFormat::Col1x32i);

	Pipeline::bindTextureToImage(mFrameTex, 0, 0, ImageAccess::WriteOnly, TextureFormat::Col4x32f);
	Pipeline::bindTextureToImage(mPosWoxQueue, 1, 0, ImageAccess::ReadWrite, TextureFormat::Col4x32f);
	Pipeline::bindTextureToImage(mTputWoyQueue, 2, 0, ImageAccess::ReadWrite, TextureFormat::Col4x32f);
	Pipeline::bindTextureToImage(mUVWozQueue, 3, 0, ImageAccess::ReadWrite, TextureFormat::Col4x32i);
	Pipeline::bindTextureToImage(mIdQueue, 4, 0, ImageAccess::ReadWrite, TextureFormat::Col1x32i);
}

void GlobalQueuePathIntegrator::updateUniforms(const RenderStatus& status)
{
	auto [scene, size, level] = status;
	auto& sceneBuffers = scene->glContext;
	const auto& camera = scene->camera;
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
		shader->setTexture("uEnvMap", scene->envMap->envMap(), 13);
		shader->setTexture("uEnvAliasTable", scene->envMap->aliasTable(), 14);
		shader->setTexture("uEnvAliasProb", scene->envMap->aliasProb(), 15);
		shader->setTexture("uMaterials", sceneBuffers.material, 16);
		shader->setTexture("uMatTypes", sceneBuffers.material, 16);
		shader->setTexture("uLightPower", sceneBuffers.lightPower, 17);
		shader->setTexture("uLightAlias", sceneBuffers.lightAlias, 18);
		shader->setTexture("uLightProb", sceneBuffers.lightProb, 19);
		shader->setTexture("uTextures", sceneBuffers.textures, 20);
		shader->setTexture("uTexUVScale", sceneBuffers.texUVScale, 21);
		shader->setTexture("uSobolSeq", scene->sobolTex, 22);
		shader->setTexture("uNoiseTex", scene->noiseTex, 23);
		shader->set1i("uNumLightTriangles", scene->nLightTriangles);
		shader->set1i("uObjPrimCount", scene->objPrimCount);

		shader->set1f("uLightSum", scene->lightSumPdf);
		shader->set1f("uEnvSum", scene->envMap->sumPdf());
		shader->set1i("uBvhSize", scene->boxCount);
		shader->set1i("uSampleDim", scene->SampleDim);
		shader->set1i("uSampleNum", scene->SampleNum);
		shader->set1f("uEnvRotation", scene->envRotation);
		shader->set1i("uLightEnvUniformSample", mParam.lightEnvUniformSample);
		shader->set1f("uLightSamplePortion", mParam.lightPortion);
		shader->set1i("uSampler", scene->sampler);
		shader->setVec2i("uFilmSize", size);
		shader->set1i("uQueueCapacity", size.x * size.y + 1);
	}

	mPrimaryRayShader->setVec3("uCamF", camera.front());
	mPrimaryRayShader->setVec3("uCamR", camera.right());
	mPrimaryRayShader->setVec3("uCamU", camera.up());
	mPrimaryRayShader->setMat3("uCamMatInv", glm::inverse(camMatrix));
	mPrimaryRayShader->setVec3("uCamPos", camera.pos());
	mPrimaryRayShader->set1f("uTanFOV", glm::tan(glm::radians(camera.FOV() * 0.5f)));
	mPrimaryRayShader->set1f("uCamAsp", camera.aspect());
	mPrimaryRayShader->set1f("uLensRadius", camera.lensRadius());
	mPrimaryRayShader->set1f("uFocalDist", camera.focalDist());

	mStreamingShader->set1i("uRussianRoulette", mParam.russianRoulette);
	mStreamingShader->set1i("uSampleLight", mParam.sampleLight);

	mImageClearShader->setVec2i("uTexSize", size);
}

void GlobalQueuePathIntegrator::init(Scene* scene, int width, int height, PipelinePtr ctx)
{
	mPrimaryRayShader = Shader::createFromText("pt_global_queue_primary.glsl", { PrimaryBlockSizeX, PrimaryBlockSizeY, 1 },
		"#extension GL_EXT_texture_array : enable\n");
	mStreamingShader = Shader::createFromText("pt_global_queue_streaming.glsl", { StreamingBlockSize, 1, 1 },
		"#extension GL_EXT_texture_array : enable\n");
	mImageClearShader = Shader::createFromText("util/img_clear_4x32f.glsl", { ImageOpSizeX, ImageOpSizeY, 1 });
	recreateFrameTex(width, height);
	updateUniforms({ scene, { width, height } });
}

void GlobalQueuePathIntegrator::renderOnePass()
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

	mIdQueue->buffer()->write<int64_t>(0, 0);

	mPrimaryRayShader->set1i("uSpp", mCurSample);
	mPrimaryRayShader->set1i("uFreeCounter", mFreeCounter);

	Pipeline::dispatchCompute(numX, numY, 1, mPrimaryRayShader);
	Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);

	mStreamingShader->set1i("uSpp", mCurSample);
	mStreamingShader->set1i("uFreeCounter", mFreeCounter);

	const int QueueCapacity = width * height + 1;
	
	for (int i = 1; i <= mParam.maxDepth; i++)
	{
		int headPtr = mIdQueue->buffer()->read<int>(0);
		int tailPtr = mIdQueue->buffer()->read<int>(4);
		headPtr = ((headPtr % QueueCapacity) + QueueCapacity) % QueueCapacity;
		tailPtr = ((tailPtr % QueueCapacity) + QueueCapacity) % QueueCapacity;
		int queueSize = (tailPtr - headPtr + QueueCapacity) % QueueCapacity;

		//std::cout << headPtr << "\t" << tailPtr << "\t" << queueSize << "\n";
		if (queueSize == 0)
			break;
		
		int blockNum = (queueSize + StreamingBlockSize - 1) / StreamingBlockSize;
		mStreamingShader->set1i("uCurDepth", i);
		mStreamingShader->set1i("uQueueSize", queueSize);
		Pipeline::dispatchCompute(blockNum, 1, 1, mStreamingShader);
		Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);
	}
	//std::cout << "\n";

	mCurSample++;
	mParam.samplePerPixel += 1.0f;
}

void GlobalQueuePathIntegrator::reset(const RenderStatus& status)
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

void GlobalQueuePathIntegrator::renderSettingsGUI()
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

void GlobalQueuePathIntegrator::renderProgressGUI()
{
	if (mParam.finiteSample)
		ImGui::ProgressBar(static_cast<float>(mCurSample) / mParam.maxSample);
	else
		ImGui::Text("Spp: %d", mCurSample);
}