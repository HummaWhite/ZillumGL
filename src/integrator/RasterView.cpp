#include "../core/Integrator.h"

void RasterView::init(const Scene& scene, int width, int height, PipelinePtr ctx)
{
	mCtx = ctx;
	mShader = Shader::create("raster_view.glsl", { 1, 1, 1 },
		"#extension GL_EXT_texture_array : enable\n");
	reset(scene, width, height);
}

void RasterView::renderOnePass()
{
	if (mShouldReset)
	{
		reset(*mResetStatus.scene, mResetStatus.renderSize.x, mResetStatus.renderSize.y);
		mShouldReset = false;
	}

	mCtx->clear({ 0.0f, 0.0f, 0.0f, 1.0f });
	glm::mat4 view = mCamera.viewMatrix();
	glm::mat4 proj = mCamera.projMatrix();
	mShader->set1i("uChannel", mDisplayChannel);
	for (const auto& modelInstance : mObjects)
	{
		glm::mat4 modelMat = modelInstance->modelMatrix();
		glm::mat3 modelInv(glm::transpose(glm::inverse(modelMat)));
		for (const auto& mesh : modelInstance->meshInstances())
		{
			mShader->setMat4("uMVPMatrix", proj * view * modelMat);
			mShader->setMat4("uModelMat", modelMat);
			mShader->setMat3("uModelInv", modelInv);
			mesh->meshData->render(mCtx, mShader);
		}
	}
}

void RasterView::reset(const Scene& scene, int width, int height)
{
	mCamera = scene.camera;
	mShader->setTexture("uTextures", scene.glContext.textures, 0);
	mShader->setVec3("uCamPos", scene.camera.pos());
	mObjects = scene.objects;
	mLights = scene.lights;
}

void RasterView::renderSettingsGUI()
{
	const char* channels[] = { "Grey", "Position", "Normal" };
	ImGui::Combo("Channel", &mDisplayChannel, channels, IM_ARRAYSIZE(channels));
}