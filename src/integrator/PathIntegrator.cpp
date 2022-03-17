#include "../core/Integrator.h"

const int WorkgroupSizeX = 48;
const int WorkgroupSizeY = 32;

void PathIntegrator::recreateFrameTex(int width, int height)
{
	mFrameTex = Texture2D::createEmpty(width, height, TextureFormat::Col4x32f);
	mFrameTex->setFilter(TextureFilter::Nearest);
	Pipeline::bindTextureToImage(mFrameTex, 0, 0, ImageAccess::ReadWrite, TextureFormat::Col4x32f);
}

void PathIntegrator::updateUniforms(const Scene& scene, int width, int height)
{
	auto& sceneBuffers = scene.glContext;
	mShader->setTexture("uVertices", sceneBuffers.vertex, 1);
	mShader->setTexture("uNormals", sceneBuffers.normal, 2);
	mShader->setTexture("uTexCoords", sceneBuffers.texCoord, 3);
	mShader->setTexture("uIndices", sceneBuffers.index, 4);
	mShader->setTexture("uBounds", sceneBuffers.bound, 5);
	mShader->setTexture("uHitTable", sceneBuffers.hitTable, 6);
	mShader->setTexture("uMatTexIndices", sceneBuffers.matTexIndex, 7);
	mShader->setTexture("uEnvMap", scene.envMap->envMap(), 8);
	mShader->setTexture("uEnvAliasTable", scene.envMap->aliasTable(), 9);
	mShader->setTexture("uEnvAliasProb", scene.envMap->aliasProb(), 10);
	mShader->setTexture("uMaterials", sceneBuffers.material, 11);
	mShader->setTexture("uMatTypes", sceneBuffers.material, 11);
	mShader->setTexture("uLightPower", sceneBuffers.lightPower, 12);
	mShader->setTexture("uLightAlias", sceneBuffers.lightAlias, 13);
	mShader->setTexture("uLightProb", sceneBuffers.lightProb, 14);
	mShader->setTexture("uTextures", sceneBuffers.textures, 15);
	mShader->setTexture("uTexUVScale", sceneBuffers.texUVScale, 16);
	mShader->setTexture("uSobolSeq", scene.sobolTex, 17);
	mShader->setTexture("uNoiseTex", scene.noiseTex, 18);
	mShader->set1i("uNumLightTriangles", scene.nLightTriangles);
	mShader->set1f("uLightSum", scene.lightSumPdf);
	mShader->set1f("uEnvSum", scene.envMap->sumPdf());
	mShader->set1i("uObjPrimCount", scene.objPrimCount);
	mShader->set1i("uBvhSize", scene.boxCount);
	mShader->set1i("uSampleDim", scene.SampleDim);
	mShader->set1i("uSampleNum", scene.SampleNum);
	mShader->set1f("uEnvRotation", scene.envRotation);
	mShader->set1i("uSampler", scene.sampler);

	const auto& camera = scene.camera;
	mShader->setVec3("uCamF", camera.front());
	mShader->setVec3("uCamR", camera.right());
	mShader->setVec3("uCamU", camera.up());
	glm::mat3 camMatrix(camera.right(), camera.up(), camera.front());
	mShader->setMat3("uCamMatInv", glm::inverse(camMatrix));
	mShader->setVec3("uCamPos", camera.pos());
	mShader->set1f("uTanFOV", glm::tan(glm::radians(camera.FOV() * 0.5f)));
	mShader->set1f("uCamAsp", camera.aspect());
	mShader->set1f("uLensRadius", camera.lensRadius());
	mShader->set1f("uFocalDist", camera.focalDist());
	mShader->set2i("uFilmSize", width, height);

	mShader->set1i("uRussianRoulette", mParam.russianRoulette);
	mShader->set1i("uMaxBounce", mParam.maxDepth);
	mShader->set1i("uSampleLight", mParam.sampleLight);
	mShader->set1i("uLightEnvUniformSample", mParam.lightEnvUniformSample);
	mShader->set1f("uLightSamplePortion", mParam.lightPortion);
}

void PathIntegrator::init(const Scene& scene, int width, int height)
{
	mShader = Shader::create("path_integ_naive.glsl", { WorkgroupSizeX, WorkgroupSizeY, 1 },
		"#extension GL_EXT_texture_array : enable\n");
	recreateFrameTex(width, height);
	updateUniforms(scene, width, height);
}

void PathIntegrator::renderOnePass()
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
	int numX = (width + WorkgroupSizeX - 1) / WorkgroupSizeX;
	int numY = (height + WorkgroupSizeY - 1) / WorkgroupSizeY;

	mShader->set1i("uSpp", mCurSample);
	mShader->set1i("uFreeCounter", mFreeCounter);

	Pipeline::dispatchCompute(numX, numY, 1, mShader);
	Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);

	mCurSample++;
}

void PathIntegrator::reset(const Scene& scene, int width, int height)
{
	if (width != mFrameTex->width() && height != mFrameTex->height())
		recreateFrameTex(width, height);
	updateUniforms(scene, width, height);
	mCurSample = 0;
}

void PathIntegrator::renderSettingsGUI()
{
	ImGui::SetNextItemWidth(80.0f);
	if (ImGui::SliderInt("Max depth", &mParam.maxDepth, 0, 32))
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

void PathIntegrator::renderProgressGUI()
{
	if (mParam.finiteSample)
		ImGui::ProgressBar(static_cast<float>(mCurSample) / mParam.maxSample);
	else
		ImGui::Text("Spp: %d", mCurSample);
}