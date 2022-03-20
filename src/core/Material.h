#pragma once

#include <iostream>
#include <variant>
#include <optional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../thirdparty/pugixml/pugixml.hpp"

struct MetalWorkflowBSDF
{
	glm::vec3 baseColor;
	float metallic;
	float roughness;
};

struct DielectricBSDF
{
	glm::vec3 baseColor;
	float ior;
	float roughness;
};

struct ThinDielectricBSDF
{
	glm::vec3 baseColor;
	float ior;
};

struct Material
{
	enum { Lambertian = 0, Principled, MetalWorkflow, Dielectric, ThinDielectric };

	glm::vec3 baseColor = glm::vec3(1.0f);
	float roughness = 1.0f;

	float subsurface = 0.0f;
	float metallic = 0.0f;
	float specular = 1.0f;
	float specularTint = 1.0f;

	float sheen = 0.0f;
	float sheenTint = 1.0f;
	float clearcoat = 0.0f;
	float clearcoatGloss = 0.0f;

	float ior = 1.5f;
	int type = Lambertian;
	float padding2;
	float padding3;
};

std::optional<Material> loadMaterial(const pugi::xml_node& node);