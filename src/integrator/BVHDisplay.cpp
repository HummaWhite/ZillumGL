#include "../core/Integrator.h"

const int WorkgroupSizeX = 48;
const int WorkgroupSizeY = 32;

void BVHDisplayIntegrator::recreateFrameTex(int width, int height)
{
	mFrameTex = Texture2D::createEmpty(width, height, TextureFormat::Col4x32f);
	mFrameTex->setFilter(TextureFilter::Nearest);
}

void BVHDisplayIntegrator::updateUniforms(const RenderStatus& status)
{
	auto [scene, size, level] = status;
	
	auto& sceneBuffers = scene->glContext;
	Pipeline::bindTextureToImage(mFrameTex, 0, 0, ImageAccess::ReadWrite, TextureFormat::Col4x32f);
	mShader->setTexture("uVertices", sceneBuffers.vertex, 1);
	mShader->setTexture("uNormals", sceneBuffers.normal, 2);
	mShader->setTexture("uTexCoords", sceneBuffers.texCoord, 3);
	mShader->setTexture("uIndices", sceneBuffers.index, 4);
	mShader->setTexture("uBounds", sceneBuffers.bound, 5);
	mShader->setTexture("uHitTable", sceneBuffers.hitTable, 6);
	mShader->setTexture("uMatTexIndices", sceneBuffers.matTexIndex, 7);
	mShader->set1i("uBvhSize", scene->boxCount);
	mShader->set1f("uBvhDepth", glm::log2(static_cast<float>(scene->boxCount)));
	mShader->set1i("uMatIndex", mMatIndex);

	const auto& camera = scene->camera;
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
	mShader->setVec2i("uFilmSize", size);

	mShader->set1i("uCheckDepth", mCheckDepth);
	mShader->set1i("uDepthCmp", mDepthCmp);
}

void BVHDisplayIntegrator::init(Scene* scene, int width, int height, PipelinePtr ctx)
{
	mShader = Shader::createFromText("bvh_display.glsl", { WorkgroupSizeX, WorkgroupSizeY, 1 });
	recreateFrameTex(width, height);
	updateUniforms({ scene, { width, height } });
}

void BVHDisplayIntegrator::renderOnePass()
{
	mFreeCounter++;
	if (mShouldReset)
	{
		reset(mStatus);
		mShouldReset = false;
	}
	if (mCurSample > 1)
	{
		mRenderFinished = true;
		return;
	}

	int width = mStatus.renderSize.x;
	int height = mStatus.renderSize.y;
	int numX = (width + WorkgroupSizeX - 1) / WorkgroupSizeX;
	int numY = (height + WorkgroupSizeY - 1) / WorkgroupSizeY;

	Pipeline::dispatchCompute(numX, numY, 1, mShader);
	Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);

	mCurSample++;
}

void BVHDisplayIntegrator::reset(const RenderStatus& status)
{
	if (mFrameTex->size() != status.renderSize)
		recreateFrameTex(status.renderSize.x, status.renderSize.y);
	updateUniforms(status);
	mCurSample = 0;
}

void BVHDisplayIntegrator::renderSettingsGUI()
{
	ImGui::Checkbox("Check depth", &mCheckDepth);
	if (mCheckDepth)
	{
		if (ImGui::InputInt("Compare depth", &mDepthCmp, 1, 10));
			setShouldReset();
	}
}

void BVHDisplayIntegrator::renderProgressGUI()
{
	ImGui::Text("BVH Display Mode");
}