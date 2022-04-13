#include "../core/Integrator.h"

const int WorkgroupSizeX = 48;
const int WorkgroupSizeY = 32;

void BVHDisplayIntegrator::recreateFrameTex(int width, int height)
{
	mFrameTex = Texture2D::createEmpty(width, height, TextureFormat::Col4x32f);
	mFrameTex->setFilter(TextureFilter::Nearest);
}

void BVHDisplayIntegrator::updateUniforms(const Scene& scene, int width, int height)
{
	auto& sceneBuffers = scene.glContext;
	Pipeline::bindTextureToImage(mFrameTex, 0, 0, ImageAccess::ReadWrite, TextureFormat::Col4x32f);
	mShader->setTexture("uVertices", sceneBuffers.vertex, 1);
	mShader->setTexture("uNormals", sceneBuffers.normal, 2);
	mShader->setTexture("uTexCoords", sceneBuffers.texCoord, 3);
	mShader->setTexture("uIndices", sceneBuffers.index, 4);
	mShader->setTexture("uBounds", sceneBuffers.bound, 5);
	mShader->setTexture("uHitTable", sceneBuffers.hitTable, 6);
	mShader->setTexture("uMatTexIndices", sceneBuffers.matTexIndex, 7);
	mShader->set1i("uBvhSize", scene.boxCount);
	mShader->set1f("uBvhDepth", glm::log2(static_cast<float>(scene.boxCount)));
	mShader->set1i("uMatIndex", mMatIndex);

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
}

void BVHDisplayIntegrator::init(const Scene& scene, int width, int height, PipelinePtr ctx)
{
	mShader = Shader::createFromText("bvh_display.glsl", { WorkgroupSizeX, WorkgroupSizeY, 1 });
	recreateFrameTex(width, height);
	updateUniforms(scene, width, height);
}

void BVHDisplayIntegrator::renderOnePass()
{
	mFreeCounter++;
	if (mShouldReset)
	{
		reset(*mResetStatus.scene, mResetStatus.renderSize.x, mResetStatus.renderSize.y);
		mShouldReset = false;
	}
	if (mCurSample > 1)
	{
		mRenderFinished = true;
		return;
	}

	int width = mResetStatus.renderSize.x;
	int height = mResetStatus.renderSize.y;
	int numX = (width + WorkgroupSizeX - 1) / WorkgroupSizeX;
	int numY = (height + WorkgroupSizeY - 1) / WorkgroupSizeY;

	Pipeline::dispatchCompute(numX, numY, 1, mShader);
	Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);

	mCurSample++;
}

void BVHDisplayIntegrator::reset(const Scene& scene, int width, int height)
{
	if (width != mFrameTex->width() && height != mFrameTex->height())
		recreateFrameTex(width, height);
	updateUniforms(scene, width, height);
	mCurSample = 0;
}

void BVHDisplayIntegrator::renderProgressGUI()
{
	ImGui::Text("BVH Display Mode");
}