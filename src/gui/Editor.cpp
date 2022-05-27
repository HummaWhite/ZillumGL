#include "Editor.h"

bool cameraEditor(Camera& camera, const std::string& name)
{
	ImGui::Text("%s", name.c_str());
	auto pos = camera.pos();
	auto fov = camera.FOV();
	auto angle = camera.angle();
	auto lensRadius = camera.lensRadius();
	auto focalDist = camera.focalDist();
	if (ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.1f) ||
		ImGui::DragFloat3("Angle", glm::value_ptr(angle), 0.05f) ||
		ImGui::SliderFloat("FOV", &fov, 0.0f, 90.0f) ||
		ImGui::DragFloat("Lens radius", &lensRadius, 0.001f, 0.0f, 10.0f) ||
		ImGui::DragFloat("Focal distance", &focalDist, 0.01f, 0.004f, 100.0f))
	{
		camera.setPos(pos);
		camera.setAngle(angle);
		camera.setFOV(fov);
		camera.setLensRadius(lensRadius);
		camera.setFocalDist(focalDist);
		return true;
	}
	return false;
}

bool materialEditor(Material& m)
{
	const char* matTypes[] = { "Lambertian", "Principled", "MetalWorkflow", "Dielectric", "ThinDielectric" };
	if (ImGui::Combo("Type", &m.type, matTypes, IM_ARRAYSIZE(matTypes)))
		return true;

	if (m.type == Material::Lambertian)
		return ImGui::ColorEdit3("BaseColor", glm::value_ptr(m.baseColor));
	else if (m.type == Material::MetalWorkflow)
	{
		return ImGui::ColorEdit3("BaseColor", glm::value_ptr(m.baseColor)) |
			ImGui::SliderFloat("Metallic", &m.metallic, 0.0f, 1.0f) |
			ImGui::SliderFloat("Roughness", &m.roughness, 0.0f, 1.0f);
	}
	else if (m.type == Material::Principled)
	{
		return ImGui::ColorEdit3("BaseColor", glm::value_ptr(m.baseColor)) |
			ImGui::SliderFloat("Subsurface", &m.subsurface, 0.0f, 1.0f) |
			ImGui::SliderFloat("Metallic", &m.metallic, 0.0f, 1.0f) |
			ImGui::SliderFloat("Roughness", &m.roughness, 0.0f, 1.0f) |
			ImGui::SliderFloat("Specular", &m.specular, 0.0f, 1.0f) |
			ImGui::SliderFloat("Specular tint", &m.specularTint, 0.0f, 1.0f) |
			ImGui::SliderFloat("Sheen", &m.sheen, 0.0f, 1.0f) |
			ImGui::SliderFloat("Sheen tint", &m.sheenTint, 0.0f, 1.0f) |
			ImGui::SliderFloat("Clearcoat", &m.clearcoat, 0.0f, 1.0f) |
			ImGui::SliderFloat("Clearcoat gloss", &m.clearcoatGloss, 0.0f, 1.0f);
	}
	else if (m.type == Material::Dielectric)
	{
		return ImGui::ColorEdit3("BaseColor", glm::value_ptr(m.baseColor)) |
			ImGui::SliderFloat("IOR", &m.ior, 0.01f, 3.5f) |
			ImGui::SliderFloat("Roughness", &m.roughness, 0.0f, 1.0f);
	}
	else if (m.type == Material::ThinDielectric)
	{
		return ImGui::ColorEdit3("BaseColor", glm::value_ptr(m.baseColor)) |
			ImGui::SliderFloat("IOR", &m.ior, 0.01f, 3.5f);
	}
}

bool materialEditor(Scene& scene, int& matIndex)
{
	matIndex = std::min(static_cast<size_t>(matIndex), scene.materials.size() - 1);
	auto& m = scene.materials[matIndex];
	bool writeMat = materialEditor(m);
	if (writeMat)
		scene.glContext.material->write(sizeof(Material) * matIndex, sizeof(Material), &m);
	return writeMat;
}

bool modelEditor(Scene& scene, int& modelIndex, int& meshIndex, int& matIndex)
{
	bool geometryChange = false;

	modelIndex = std::min(static_cast<size_t>(modelIndex), scene.objects.size() - 1);
	ImGui::SliderInt("Select model", &modelIndex, 0, scene.objects.size() - 1);
	auto& obj = scene.objects[modelIndex];
	auto name = obj->name();
	auto pos = obj->pos();
	auto scale = obj->scale();
	auto rotation = obj->rotation();

	static char nameBuf[256];
	strcpy(nameBuf, name.c_str());
	ImGui::InputText("Name", nameBuf, 256);
	obj->setName(nameBuf);

	ImGui::Text("%s", obj->path().generic_string().c_str());

	geometryChange |= ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.01f);
	obj->setPos(pos);
	geometryChange |= ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.01f);
	obj->setScale(scale);
	geometryChange |= ImGui::DragFloat3("Rotation", glm::value_ptr(rotation), 0.01f);
	obj->setRotation(rotation);

	return geometryChange;
}

bool lightEditor(Scene& scene, int& lightIndex)
{
	bool change = false;
	lightIndex = std::min(static_cast<size_t>(lightIndex), scene.lights.size() - 1);
	ImGui::SliderInt("Select light", &lightIndex, 0, scene.lights.size() - 1);
	auto& [obj, power] = scene.lights[lightIndex];

	auto pos = obj->pos();
	auto scale = obj->scale();
	auto rotation = obj->rotation();

	change |= ImGui::DragFloat("Power", glm::value_ptr(power), 0.01f, 0.0f);
	change |= ImGui::DragFloat3("Position", glm::value_ptr(pos), 0.01f);
	obj->setPos(pos);
	change |= ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.01f);
	obj->setScale(scale);
	change |= ImGui::DragFloat3("Rotation", glm::value_ptr(rotation), 0.01f);
	obj->setRotation(rotation);
	return change;
}