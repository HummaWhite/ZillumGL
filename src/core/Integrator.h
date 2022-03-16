#pragma once

#include <iostream>
#include <memory>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "Texture.h"
#include "Pipeline.h"
#include "Shader.h"

class Integrator
{
public:
	virtual void renderOnePassTo(TexturePtr frame) = 0;
	virtual void reset() = 0;
	virtual void renderSettingsGUI() = 0;

protected:
	bool mRenderFinished = false;
	bool mPassFinished = false;
};

using IntegratorPtr = std::shared_ptr<Integrator>;

struct PathIntegParam
{
	int maxDepth = 4;
	bool russianRoulette = false;
	bool sampleLight = true;
	bool lightEnvUniformSample = false;
	float lightPortion = 0.5f;
};

class PathIntegrator :
	public Integrator
{
public:
	void renderOnePassTo(TexturePtr frame) {}
	void reset() {}
	void renderSettingsGUI();

public:
	PathIntegParam mParam;

private:
	ShaderPtr mShader;
};