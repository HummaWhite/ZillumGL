#include "Material.h"

#include <sstream>

std::optional<Material> loadMaterial(const pugi::xml_node& node)
{
	Material material;
	std::string type(node.attribute("type").as_string());

	auto loadFloat = [&](const char* childName, float& value)
	{
		auto child = node.child(childName);
		if (!child)
			return;
		std::stringstream ss(child.attribute("value").as_string());
		ss >> value;
	};

	auto loadVec3f = [&](const char* childName, glm::vec3& value)
	{
		auto child = node.child(childName);
		if (!child)
			return;
		std::stringstream ss(child.attribute("value").as_string());
		ss >> value.x >> value.y >> value.z;
	};

	if (type == "default")
		return std::nullopt;
	else if (type == "principled")
	{
		loadVec3f("baseColor", material.baseColor);
		loadFloat("subsurface", material.subsurface);
		loadFloat("metallic", material.metallic);
		loadFloat("roughness", material.roughness);
		loadFloat("specular", material.specular);
		loadFloat("specularTint", material.specularTint);
		loadFloat("sheen", material.sheen);
		loadFloat("sheenTint", material.sheenTint);
		loadFloat("clearcoat", material.clearcoat);
		loadFloat("clearcoatGloss", material.clearcoatGloss);
		material.type = Material::Principled;
	}
	else if (type == "metalWorkflow")
	{
		loadVec3f("baseColor", material.baseColor);
		loadFloat("metallic", material.metallic);
		loadFloat("roughness", material.roughness);
		material.type = Material::MetalWorkflow;
	}
	else if (type == "dielectric")
	{
		loadVec3f("baseColor", material.baseColor);
		loadFloat("ior", material.ior);
		loadFloat("roughness", material.roughness);
		material.type = Material::Dielectric;
	}
	else if (type == "thinDielectric")
	{
		loadVec3f("baseColor", material.baseColor);
		loadFloat("ior", material.ior);
		material.type = Material::ThinDielectric;
	}
	return material;
}