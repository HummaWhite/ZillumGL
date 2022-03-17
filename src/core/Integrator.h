#pragma once

#include <iostream>
#include <memory>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "Texture.h"
#include "Pipeline.h"
#include "Shader.h"
#include "Scene.h"

class Integrator
{
public:
	struct ResetStatus
	{
		Scene* scene = nullptr;
		glm::ivec2 renderSize;
	};

public:
	virtual void init(const Scene& scene, int width, int height) = 0;
	virtual void renderOnePass() = 0;
	virtual void reset(const Scene& scene, int width, int height) = 0;
	virtual void renderSettingsGUI() = 0;
	virtual void renderProgressGUI() = 0;

	virtual TexturePtr getFrame() = 0;
	virtual float resultScale() const = 0;

	void setResetStatus(const ResetStatus& status) { mResetStatus = status; }
	void setShouldReset() { mShouldReset = true; }

protected:
	bool mRenderFinished = false;
	bool mPassFinished = false;
	int mCurSample = 0;
	int mFreeCounter = 0;
	ResetStatus mResetStatus;
	bool mShouldReset = false;
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
};

class PathIntegrator :
	public Integrator
{
public:
	void init(const Scene& scene, int width, int height);
	void renderOnePass();
	void reset(const Scene& scene, int width, int height);
	void renderSettingsGUI();
	void renderProgressGUI();

	TexturePtr getFrame() { return mFrameTex; }
	float resultScale() const { return 1.0f / (mCurSample + 1); }

private:
	void recreateFrameTex(int width, int height);
	void updateUniforms(const Scene& scene, int width, int height);

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
};

class LightPathIntegrator :
	public Integrator
{
public:
	void init(const Scene& scene, int width, int height);
	void renderOnePass();
	void reset(const Scene& scene, int width, int height);
	void renderSettingsGUI();
	void renderProgressGUI();

	TexturePtr getFrame() { return mFrameTex; }
	float resultScale() const { return 1.0f / (mCurSample + 1); }

private:
	void recreateFrameTex(int width, int height);
	void updateUniforms(const Scene& scene, int width, int height);

public:
	LightPathIntegParam mParam;

private:
	Texture2DPtr mFrameTex;
	ShaderPtr mShader;
};

class TriPathIntegrator :
	public Integrator
{
private:
	Texture2DPtr mFrameTex;
	ShaderPtr mShader;
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
private:
	Texture2DPtr mFrameTex;
	ShaderPtr mShader;
};