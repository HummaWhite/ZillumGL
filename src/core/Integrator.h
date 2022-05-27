#pragma once

#include <iostream>
#include <memory>

#include "Texture.h"
#include "Pipeline.h"
#include "Shader.h"
#include "Scene.h"
#include "../util/Timer.h"
#include "../thirdparty/imgui.hpp"

enum class ResetLevel
{
	ResetFrame, ResetUniform, FullReset
};

struct RenderStatus
{
	Scene* scene = nullptr;
	glm::ivec2 renderSize;
	ResetLevel resetLevel;
};

class Integrator
{
public:
	virtual void init(Scene* scene, int width, int height, PipelinePtr ctx) = 0;
	virtual void renderOnePass() = 0;
	virtual void reset(const RenderStatus& status) = 0;
	virtual void renderSettingsGUI() = 0;
	virtual void renderProgressGUI() = 0;

	virtual Texture2DPtr getFrame() = 0;
	virtual float resultScale() const = 0;

	virtual void recreateFrameTex(int width, int height) {}

	void setStatus(const RenderStatus& status) { mStatus = status; }
	void setShouldReset() { mShouldReset = true; }

protected:
	bool mRenderFinished = false;
	bool mPassFinished = false;
	int mCurSample = 0;
	int mFreeCounter = 0;
	RenderStatus mStatus;
	bool mShouldReset = false;

	double mTime;
	Timer mTimer;
};

using IntegratorPtr = std::shared_ptr<Integrator>;

struct PathIntegParam
{
	int maxDepth = 4;
	bool russianRoulette = false;
	bool sampleLight = true;
	bool lightEnvUniformSample = false;
	float lightPortion = 0.5f;
	bool finiteSample = false;
	int maxSample = 64;
	int sampler = 1;
};

class NaivePathIntegrator :
	public Integrator
{
public:
	void init(Scene* scene, int width, int height, PipelinePtr ctx);
	void renderOnePass();
	void reset(const RenderStatus& status);
	void renderSettingsGUI();
	void renderProgressGUI();

	Texture2DPtr getFrame() { return mFrameTex; }
	float resultScale() const { return 1.0f / (mCurSample + 1); }
	void recreateFrameTex(int width, int height);

private:
	void updateUniforms(const RenderStatus& status);

public:
	PathIntegParam mParam;

private:
	Texture2DPtr mFrameTex;
	ShaderPtr mShader;
};

struct LightPathIntegParam
{
	int maxDepth = 4;
	bool russianRoulette = false;
	bool finiteSample = false;
	int maxSample = 64;
	int threadBlocksOnePass = 32;
	float samplePerPixel = 0.0f;
};

class LightPathIntegrator :
	public Integrator
{
public:
	void init(Scene* scene, int width, int height, PipelinePtr ctx);
	void renderOnePass();
	void reset(const RenderStatus& status);
	void renderSettingsGUI();
	void renderProgressGUI();

	Texture2DPtr getFrame() { return mFrameTex; }
	float resultScale() const { return 1.0f / mParam.samplePerPixel; }
	void recreateFrameTex(int width, int height);

private:
	void updateUniforms(const RenderStatus& status);

public:
	LightPathIntegParam mParam;

private:
	Texture2DPtr mFrameTex;
	Texture2DPtr mFilmTex;
	ShaderPtr mShader;
	ShaderPtr mImageCopyShader;
	ShaderPtr mImageClearShader;
};

struct TriplePathIntegParam
{
	int maxDepth = 4;
	bool russianRoulette = false;
	int LPTBlocksOnePass = 64;
	int LPTLoopsPerPass = 1;
	bool finiteSample = false;
	int maxSample = 64;
	float samplePerPixel = 0.0f;
	int PTSampler = 1;
	bool limitTime = true;
	double maxTime = 30.0;
};

class TriplePathIntegrator :
	public Integrator
{
public:
	void init(Scene* scene, int width, int height, PipelinePtr ctx);
	void renderOnePass();
	void reset(const RenderStatus& status);
	void renderSettingsGUI();
	void renderProgressGUI();

	Texture2DPtr getFrame() { return mFrameTex; }
	float resultScale() const { return 1.0f / mParam.samplePerPixel; }
	void recreateFrameTex(int width, int height);

private:
	void updateUniforms(const RenderStatus& status);

public:
	TriplePathIntegParam mParam;

private:
	Texture2DPtr mFrameTex;
	Texture2DPtr mFilmTex;
	ShaderPtr mPTShader;
	ShaderPtr mLPTShader;
	ShaderPtr mImageCopyShader;
	ShaderPtr mImageClearShader;
};

class AOIntegrator :
	public Integrator
{
private:
	Texture2DPtr mFrameTex;
	ShaderPtr mShader;
};

class BDPTIntegrator;

class BVHDisplayIntegrator :
	public Integrator
{
public:
	void init(Scene* scene, int width, int height, PipelinePtr ctx);
	void renderOnePass();
	void reset(const RenderStatus& status);
	void renderSettingsGUI();
	void renderProgressGUI();

	Texture2DPtr getFrame() { return mFrameTex; }
	float resultScale() const { return 1.0f; }
	void recreateFrameTex(int width, int height);

	void setMatIndex(int index) { mMatIndex = index; }

private:
	void updateUniforms(const RenderStatus& status);

private:
	Texture2DPtr mFrameTex;
	ShaderPtr mShader;
	int mMatIndex = 0;
	int mDepthCmp = 10;
	bool mCheckDepth = false;
};

struct QueuedPTParam
{
	int maxDepth = 4;
	bool russianRoulette = false;
	bool sampleLight = true;
	bool lightEnvUniformSample = false;
	float lightPortion = 0.5f;
	bool finiteSample = false;
	int maxSample = 64;
	float samplePerPixel = 0.0f;
	int sampler = 1;
};

class GlobalQueuePathIntegrator :
	public Integrator
{
public:
	void init(Scene* scene, int width, int height, PipelinePtr ctx);
	void renderOnePass();
	void reset(const RenderStatus& status);
	void renderSettingsGUI();
	void renderProgressGUI();

	Texture2DPtr getFrame() { return mFrameTex; }
	float resultScale() const { return 1.0f / mParam.samplePerPixel; }
	void recreateFrameTex(int width, int height);

private:
	void updateUniforms(const RenderStatus& status);

private:
	Texture2DPtr mFrameTex;
	
	TextureBufferedPtr mPosWoxQueue;
	TextureBufferedPtr mTputWoyQueue;
	TextureBufferedPtr mUVWozQueue;
	TextureBufferedPtr mIdQueue;

	ShaderPtr mPrimaryRayShader;
	ShaderPtr mStreamingShader;
	ShaderPtr mImageClearShader;

	QueuedPTParam mParam;
};

class BlockQueuePathIntegrator :
	public Integrator
{
public:
	void init(Scene* scene, int width, int height, PipelinePtr ctx);
	void renderOnePass();
	void reset(const RenderStatus& status);
	void renderSettingsGUI();
	void renderProgressGUI();

	Texture2DPtr getFrame() { return mFrameTex; }
	float resultScale() const { return 1.0f / mParam.samplePerPixel; }
	void recreateFrameTex(int width, int height);

private:
	void updateUniforms(const RenderStatus& status);

private:
	Texture2DPtr mFrameTex;

	TextureBufferedPtr mPosWoxQueue;
	TextureBufferedPtr mTputWoyQueue;
	TextureBufferedPtr mUVWozQueue;
	TextureBufferedPtr mIdQueue;

	ShaderPtr mShader;
	ShaderPtr mImageClearShader;

	QueuedPTParam mParam;
};

class SharedQueuePathIntegrator :
	public Integrator
{
public:
	void init(Scene* scene, int width, int height, PipelinePtr ctx);
	void renderOnePass();
	void reset(const RenderStatus& status);
	void renderSettingsGUI();
	void renderProgressGUI();

	Texture2DPtr getFrame() { return mFrameTex; }
	float resultScale() const { return 1.0f / mParam.samplePerPixel; }
	void recreateFrameTex(int width, int height);

private:
	void updateUniforms(const RenderStatus& status);

private:
	Texture2DPtr mFrameTex;

	ShaderPtr mShader;
	ShaderPtr mImageClearShader;

	QueuedPTParam mParam;
};

class RasterView :
	public Integrator
{
public:
	void init(Scene* scene, int width, int height, PipelinePtr ctx);
	void renderOnePass();
	void reset(const RenderStatus& status);
	void renderSettingsGUI();
	void renderProgressGUI() {}

	Texture2DPtr getFrame() { return mFrameTex; }
	float resultScale() const { return 1.0f; }

	void setIndex(int modelIndex, int matIndex) { mModelIndex = modelIndex, mMatIndex = matIndex; }
	void forceClear() { mShader->clearUniformRecord(); }

private:
	PipelinePtr mCtx;
	ShaderPtr mShader;
	FrameBufferPtr mFrameBuffer;
	Texture2DPtr mFrameTex;

	int mDisplayChannel = 0;
	int mModelIndex = 0;
	int mMatIndex = 0;
};