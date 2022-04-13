#include "../core/Integrator.h"

const int BlockSizeX = 48;
const int BlockSizeY = 32;
const int LPTBlockSize = 1536;

void TriplePathIntegrator::recreateFrameTex(int width, int height)
{
	mFilmTex = Texture2D::createEmpty(width * 3, height, TextureFormat::Col1x32f);
	mFilmTex->setFilter(TextureFilter::Nearest);
	mFrameTex = Texture2D::createEmpty(width, height, TextureFormat::Col4x32f);
	mFrameTex->setFilter(TextureFilter::Nearest);

	Pipeline::bindTextureToImage(mFilmTex, 0, 0, ImageAccess::ReadWrite, TextureFormat::Col1x32f);
	Pipeline::bindTextureToImage(mFrameTex, 1, 0, ImageAccess::WriteOnly, TextureFormat::Col4x32f);
}

void TriplePathIntegrator::updateUniforms(const Scene& scene, int width, int height)
{
	Shader* shaders[] = { mPTShader.get(), mLPTShader.get() };
	auto& sceneBuffers = scene.glContext;
	const auto& camera = scene.camera;
	glm::mat3 camMatrix(camera.right(), camera.up(), camera.front());

	for (auto shader : shaders)
	{
		shader->setTexture("uVertices", sceneBuffers.vertex, 2);
		shader->setTexture("uNormals", sceneBuffers.normal, 3);
		shader->setTexture("uTexCoords", sceneBuffers.texCoord, 4);
		shader->setTexture("uIndices", sceneBuffers.index, 5);
		shader->setTexture("uBounds", sceneBuffers.bound, 6);
		shader->setTexture("uHitTable", sceneBuffers.hitTable, 7);
		shader->setTexture("uMatTexIndices", sceneBuffers.matTexIndex, 8);
		shader->setTexture("uEnvMap", scene.envMap->envMap(), 9);
		shader->setTexture("uEnvAliasTable", scene.envMap->aliasTable(), 10);
		shader->setTexture("uEnvAliasProb", scene.envMap->aliasProb(), 11);
		shader->setTexture("uMaterials", sceneBuffers.material, 12);
		shader->setTexture("uMatTypes", sceneBuffers.material, 12);
		shader->setTexture("uLightPower", sceneBuffers.lightPower, 13);
		shader->setTexture("uLightAlias", sceneBuffers.lightAlias, 14);
		shader->setTexture("uLightProb", sceneBuffers.lightProb, 15);
		shader->setTexture("uTextures", sceneBuffers.textures, 16);
		shader->setTexture("uTexUVScale", sceneBuffers.texUVScale, 17);
		shader->setTexture("uSobolSeq", scene.sobolTex, 18);
		shader->setTexture("uNoiseTex", scene.noiseTex, 19);
		shader->set1i("uNumLightTriangles", scene.nLightTriangles);
		shader->set1f("uLightSum", scene.lightSumPdf);
		shader->set1f("uEnvSum", scene.envMap->sumPdf());
		shader->set1i("uObjPrimCount", scene.objPrimCount);
		shader->set1i("uBvhSize", scene.boxCount);
		shader->set1i("uSampleDim", scene.SampleDim);
		shader->set1i("uSampleNum", scene.SampleNum);
		shader->set1f("uEnvRotation", scene.envRotation);

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
	}

	mPTShader->set1i("uSampler", scene.sampler);
	mLPTShader->set1i("uSampler", 0);

	mLPTShader->set1i("uBlocksOnePass", mParam.LPTBlocksOnePass);
	mLPTShader->set1i("uLoopsPerPass", mParam.LPTLoopsPerPass);
	mLPTShader->set1f("uScale", static_cast<float>(width * height) /
		(mParam.LPTBlocksOnePass * mParam.LPTLoopsPerPass * LPTBlockSize));

	mImageCopyShader->set2i("uTexSize", width, height);
	mImageClearShader->set2i("uTexSize", width, height);
}

void TriplePathIntegrator::init(const Scene& scene, int width, int height, PipelinePtr ctx)
{
	mPTShader = Shader::createFromText("triple_path_pass_pt.glsl", { BlockSizeX, BlockSizeY, 1 },
		"#extension GL_EXT_texture_array : enable\n"
		"#extension GL_NV_shader_atomic_float : enable\n");
	mLPTShader = Shader::createFromText("triple_path_pass_lpt.glsl", { LPTBlockSize, 1, 1 },
		"#extension GL_EXT_texture_array : enable\n"
		"#extension GL_NV_shader_atomic_float : enable\n");
	mImageCopyShader = Shader::createFromText("util/img_copy_1x32f_4x32f.glsl", { BlockSizeX, BlockSizeY, 1 });
	mImageClearShader = Shader::createFromText("util/img_clear_1x32f.glsl", { BlockSizeX, BlockSizeY, 1 });
	recreateFrameTex(width, height);
	updateUniforms(scene, width, height);
}

void TriplePathIntegrator::renderOnePass()
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

	int width = mResetStatus.renderSize.x;
	int height = mResetStatus.renderSize.y;
	int numX = (width + BlockSizeX - 1) / BlockSizeX;
	int numY = (height + BlockSizeY - 1) / BlockSizeY;

	mPTShader->set1i("uSpp", mCurSample);
	mPTShader->set1i("uFreeCounter", mFreeCounter);

	Pipeline::dispatchCompute(numX, numY, 1, mPTShader);
	Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);

	mLPTShader->set1i("uSpp", mCurSample);
	mLPTShader->set1i("uFreeCounter", mFreeCounter);

	Pipeline::dispatchCompute(mParam.LPTBlocksOnePass, 1, 1, mLPTShader);
	Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);

	Pipeline::dispatchCompute(numX, numY, 1, mImageCopyShader);
	Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);

	mParam.samplePerPixel += 1.0f;
	mCurSample++;
}

void TriplePathIntegrator::reset(const Scene& scene, int width, int height)
{
	if (width != mFrameTex->width() && height != mFrameTex->height())
		recreateFrameTex(width, height);
	else
	{
		int numX = (width + BlockSizeX - 1) / BlockSizeX;
		int numY = (height + BlockSizeY - 1) / BlockSizeY;
		Pipeline::dispatchCompute(numX, numY, 1, mImageClearShader);
		Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);
	}
	updateUniforms(scene, width, height);

	mParam.samplePerPixel = 1.0f;
	mCurSample = 0;
}

void TriplePathIntegrator::renderSettingsGUI()
{
	ImGui::SetNextItemWidth(80.0f);
	if (ImGui::InputInt("Max depth", &mParam.maxDepth, 1, 1))
		setShouldReset();

	ImGui::SameLine();
	if (ImGui::Checkbox("Russian rolette", &mParam.russianRoulette))
		setShouldReset();

	if (ImGui::InputInt("LPT blocks per pass", &mParam.LPTBlocksOnePass, 1, 10))
		setShouldReset();

	if (ImGui::InputInt("LPT loops in kernel", &mParam.LPTLoopsPerPass, 1, 1))
		setShouldReset();

	if (ImGui::Checkbox("Limit spp", &mParam.finiteSample) && mCurSample > mParam.maxSample)
		setShouldReset();
	if (mParam.finiteSample)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::InputInt("Max passes   ", &mParam.maxSample, 1, 10) &&
			mCurSample > mParam.maxSample)
			setShouldReset();
	}
}

void TriplePathIntegrator::renderProgressGUI()
{
	if (mParam.finiteSample)
		ImGui::ProgressBar(static_cast<float>(mCurSample) / mParam.maxSample);
	else
		ImGui::Text("Spp: %f", mParam.samplePerPixel);
}