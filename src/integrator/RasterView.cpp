#include "../core/Integrator.h"

void RasterView::init(const Scene& scene, int width, int height, PipelinePtr ctx)
{
	mCtx = ctx;
	mShader = Shader::createFromText("raster_view.glsl", { 1, 1, 1 },
		"#extension GL_EXT_texture_array : enable\n");
	reset(scene, width, height);
}

void RasterView::renderOnePass()
{
	Scene* scene = mResetStatus.scene;
	if (mShouldReset)
	{
		reset(*scene, mResetStatus.renderSize.x, mResetStatus.renderSize.y);
		mShouldReset = false;
	}

	mCtx->clear({ 0.0f, 0.0f, 0.0f, 1.0f });
	glm::mat4 view = scene->camera.viewMatrix();
	glm::mat4 proj = scene->camera.projMatrix();
	mShader->set1i("uChannel", mDisplayChannel);
	mShader->set1i("uIndexUI", mMatIndex);
	for (const auto& modelInstance : scene->objects)
	{
		glm::mat4 modelMat = modelInstance->modelMatrix();
		glm::mat3 modelInv(glm::transpose(glm::inverse(modelMat)));
		mShader->setMat4("uMVPMatrix", proj * view * modelMat);
		mShader->setMat4("uModelMat", modelMat);
		mShader->setMat3("uModelInv", modelInv);
		for (const auto& mesh : modelInstance->meshInstances())
		{
			int matIndex = mesh->globalMatIndex;
			if (matIndex >= 0)
				mShader->setVec3("uBaseColor", (matIndex == mMatIndex) ? glm::vec3(1.0f, 0.6f, 0.2f) :
					scene->materials[matIndex].baseColor);
			mShader->set1i("uTexIndex", mesh->texIndex);
			if (mesh->texIndex != -1)
				mShader->setVec2("uTexScale", scene->glContext.textures->getTexScale(mesh->texIndex));
			mesh->meshData->render(mCtx, mShader);
		}
	}
}

void RasterView::reset(const Scene& scene, int width, int height)
{
	mShader->setTexture("uTextures", scene.glContext.textures, 0);
	mShader->setVec3("uCamPos", scene.camera.pos());
}

void RasterView::renderSettingsGUI()
{
	const char* channels[] = { "Grey", "Position", "Normal", "UV" };
	ImGui::Combo("Channel", &mDisplayChannel, channels, IM_ARRAYSIZE(channels));
}