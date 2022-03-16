#include "../core/Integrator.h"

void PathIntegrator::renderSettingsGUI()
{
	ImGui::SetNextItemWidth(80.0f);
	ImGui::SliderInt("Max depth", &mParam.maxDepth, 0, 32);
	ImGui::SameLine();
	ImGui::Checkbox("Russian rolette", &mParam.russianRoulette);
	ImGui::Checkbox("Direct sample", &mParam.sampleLight);
	if (mParam.sampleLight)
	{
		ImGui::Checkbox("Light and env uniform", &mParam.lightEnvUniformSample);
		if (!mParam.lightEnvUniformSample)
			ImGui::SliderFloat("Light sample portion", &mParam.lightPortion, 0.001f, 0.999f);
	}
}