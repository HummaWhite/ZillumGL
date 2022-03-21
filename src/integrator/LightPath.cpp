#include "../core/Integrator.h"

const int WorkgroupSize = 1536;
const int ImageOpSizeX = 48;
const int ImageOpSizeY = 32;

const ShaderSource FilmClearShader =
{
	"", "", "",
	"#version 450\n"
	"layout(local_size_x = 48, local_size_y = 32, local_size_z = 1) in;\n"
	"layout(r32f, binding = 0) uniform image2D uImage;\n"
	"uniform ivec2 uTexSize;\n"
	"\n"
	"void main()\n"
	"{\n"
	"    ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);\n"
	"    if (iuv.x >= uTexSize.x || iuv.y >= uTexSize.y)\n"
	"        return;\n"
	"    imageStore(uImage, ivec2(iuv.x * 3 + 0, iuv.y), vec4(0.0, 0.0, 0.0, 1.0));\n"
	"    imageStore(uImage, ivec2(iuv.x * 3 + 1, iuv.y), vec4(0.0, 0.0, 0.0, 1.0));\n"
	"    imageStore(uImage, ivec2(iuv.x * 3 + 2, iuv.y), vec4(0.0, 0.0, 0.0, 1.0));\n"
	"}\n"
};

const ShaderSource ImageCopyShader =
{
	"", "", "",
	"#version 450\n"
	"layout(local_size_x = 48, local_size_y = 32, local_size_z = 1) in;\n"
	"layout(r32f, binding = 0) uniform readonly image2D uInImage;\n"
	"layout(rgba32f, binding = 1) uniform writeonly image2D uOutImage;\n"
	"uniform ivec2 uTexSize;\n"
	"\n"
	"void main()"
	"{\n"
	"    ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);\n"
	"    if (iuv.x >= uTexSize.x || iuv.y >= uTexSize.y)\n"
	"        return;\n"
	"    float r = imageLoad(uInImage, ivec2(iuv.x * 3 + 0, iuv.y)).r;\n"
	"    float g = imageLoad(uInImage, ivec2(iuv.x * 3 + 1, iuv.y)).r;\n"
	"    float b = imageLoad(uInImage, ivec2(iuv.x * 3 + 2, iuv.y)).r;\n"
	"    imageStore(uOutImage, iuv, vec4(r, g, b, 1.0));\n"
	"}\n"
};

void LightPathIntegrator::recreateFrameTex(int width, int height)
{
	mFilmTex = Texture2D::createEmpty(width * 3, height, TextureFormat::Col1x32f);
	mFilmTex->setFilter(TextureFilter::Nearest);
	mFrameTex = Texture2D::createEmpty(width, height, TextureFormat::Col4x32f);
	mFrameTex->setFilter(TextureFilter::Nearest);
	Pipeline::bindTextureToImage(mFilmTex, 0, 0, ImageAccess::ReadWrite, TextureFormat::Col1x32f);
	Pipeline::bindTextureToImage(mFrameTex, 1, 0, ImageAccess::WriteOnly, TextureFormat::Col4x32f);
}

void LightPathIntegrator::updateUniforms(const Scene& scene, int width, int height)
{
	auto& sceneBuffers = scene.glContext;
	mShader->setTexture("uVertices", sceneBuffers.vertex, 2);
	mShader->setTexture("uNormals", sceneBuffers.normal, 3);
	mShader->setTexture("uTexCoords", sceneBuffers.texCoord, 4);
	mShader->setTexture("uIndices", sceneBuffers.index, 5);
	mShader->setTexture("uBounds", sceneBuffers.bound, 6);
	mShader->setTexture("uHitTable", sceneBuffers.hitTable, 7);
	mShader->setTexture("uMatTexIndices", sceneBuffers.matTexIndex, 8);
	mShader->setTexture("uEnvMap", scene.envMap->envMap(), 9);
	mShader->setTexture("uEnvAliasTable", scene.envMap->aliasTable(), 10);
	mShader->setTexture("uEnvAliasProb", scene.envMap->aliasProb(), 11);
	mShader->setTexture("uMaterials", sceneBuffers.material, 12);
	mShader->setTexture("uMatTypes", sceneBuffers.material, 12);
	mShader->setTexture("uLightPower", sceneBuffers.lightPower, 13);
	mShader->setTexture("uLightAlias", sceneBuffers.lightAlias, 14);
	mShader->setTexture("uLightProb", sceneBuffers.lightProb, 15);
	mShader->setTexture("uTextures", sceneBuffers.textures, 16);
	mShader->setTexture("uTexUVScale", sceneBuffers.texUVScale, 17);
	mShader->setTexture("uSobolSeq", scene.sobolTex, 18);
	mShader->setTexture("uNoiseTex", scene.noiseTex, 19);
	mShader->set1i("uNumLightTriangles", scene.nLightTriangles);
	mShader->set1f("uLightSum", scene.lightSumPdf);
	mShader->set1f("uEnvSum", scene.envMap->sumPdf());
	mShader->set1i("uObjPrimCount", scene.objPrimCount);
	mShader->set1i("uBvhSize", scene.boxCount);
	mShader->set1i("uSampleDim", scene.SampleDim);
	mShader->set1i("uSampleNum", scene.SampleNum);
	mShader->set1f("uEnvRotation", scene.envRotation);
	//mShader->set1i("uSampler", scene.sampler);
	mShader->set1i("uSampler", 0);

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
	mShader->set1i("uMaxDepth", mParam.maxDepth);

	mImageCopyShader->set2i("uTexSize", width, height);
	mImageClearShader->set2i("uTexSize", width, height);
}

void LightPathIntegrator::init(const Scene& scene, int width, int height)
{
	mShader = Shader::create("light_path_integ.glsl", { WorkgroupSize, 1, 1 },
		"#extension GL_EXT_texture_array : enable\n" \
		"#extension GL_NV_shader_atomic_float : enable\n");
	mImageCopyShader = Shader::create("LightPathIntegrator:: image_copy.glsl", ImageCopyShader);
	mImageClearShader = Shader::create("LightPathIntegrator:: film_clear.glsl", FilmClearShader);
	recreateFrameTex(width, height);
	updateUniforms(scene, width, height);
}

void LightPathIntegrator::renderOnePass()
{
	mFreeCounter++;
	int width = mResetStatus.renderSize.x;
	int height = mResetStatus.renderSize.y;

	if (mShouldReset)
	{
		reset(*mResetStatus.scene, width, height);
		mShouldReset = false;
	}
	if (mParam.finiteSample && static_cast<float>(mParam.samplePerPixel) > mParam.maxSample)
	{
		mRenderFinished = true;
		return;
	}

	mShader->set1i("uSpp", mCurSample);
	mShader->set1i("uFreeCounter", mFreeCounter);

	Pipeline::dispatchCompute(mParam.threadBlocksOnePass, 1, 1, mShader);
	Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);

	int numX = (width + ImageOpSizeX - 1) / ImageOpSizeX;
	int numY = (height + ImageOpSizeY - 1) / ImageOpSizeY;
	Pipeline::dispatchCompute(numX, numY, 1, mImageCopyShader);
	Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);

	mParam.samplePerPixel += static_cast<float>(mParam.threadBlocksOnePass) * WorkgroupSize /
		(width * height);
	mCurSample++;
}

void LightPathIntegrator::reset(const Scene& scene, int width, int height)
{
	if (width != mFrameTex->width() && height != mFrameTex->height())
		recreateFrameTex(width, height);
	else
	{
		int numX = (width + ImageOpSizeX - 1) / ImageOpSizeX;
		int numY = (height + ImageOpSizeY - 1) / ImageOpSizeY;
		Pipeline::dispatchCompute(numX, numY, 1, mImageClearShader);
		Pipeline::memoryBarrier(MemoryBarrierBit::ShaderImageAccess);
	}
	updateUniforms(scene, width, height);
	mCurSample = 0;
	mParam.samplePerPixel = static_cast<float>(mParam.threadBlocksOnePass) * WorkgroupSize /
		(width * height);
}

void LightPathIntegrator::renderSettingsGUI()
{
	ImGui::SetNextItemWidth(80.0f);
	if (ImGui::SliderInt("Max depth", &mParam.maxDepth, 0, 32))
		setShouldReset();

	ImGui::SameLine();
	if (ImGui::Checkbox("Russian rolette", &mParam.russianRoulette))
		setShouldReset();

	if (ImGui::InputInt("Thread blocks one pass", &mParam.threadBlocksOnePass, 1, 10))
		setShouldReset();

	int curSample = static_cast<int>(mParam.samplePerPixel);
	if (ImGui::Checkbox("Limit spp", &mParam.finiteSample) && curSample > mParam.maxSample)
		setShouldReset();
	if (mParam.finiteSample)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::InputInt("Max spp      ", &mParam.maxSample, 1, 10) &&
			curSample > mParam.maxSample)
			setShouldReset();
	}
}

void LightPathIntegrator::renderProgressGUI()
{
	if (mParam.finiteSample)
		ImGui::ProgressBar(mParam.samplePerPixel / mParam.maxSample);
	else
		ImGui::Text("Spp: %f", mParam.samplePerPixel);
}