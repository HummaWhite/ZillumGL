#include "AABB.h"

AABB::AABB(const glm::vec3& va, const glm::vec3& vb, const glm::vec3& vc)
{
	pMin = glm::min(glm::min(va, vb), vc);
	pMax = glm::max(glm::max(va, vb), vc);
}

AABB::AABB(const AABB& boundA, const AABB& boundB)
{
	pMin = glm::min(boundA.pMin, boundB.pMin);
	pMax = glm::max(boundA.pMax, boundB.pMax);
}

glm::vec3 AABB::centroid() const
{
	return (pMin + pMax) * 0.5f;
}

float AABB::surfaceArea() const
{
	glm::vec3 vol = pMax - pMin;
	return 2.0f * (vol.x * vol.y + vol.y * vol.z + vol.z * vol.x);
}

int AABB::maxExtent() const
{
	glm::vec3 v = pMax - pMin;
	if (v.x > v.y) return v.x > v.z ? 0 : 2;
	return v.y > v.z ? 1 : 2;
}

std::string AABB::toString()
{
	auto vec3ToString = [](const glm::vec3 & v)
	{
		std::stringstream ss;
		ss << "vec3{ x:" << v.x << ", y:" << v.y << ", z:" << v.z << " }";
		return ss.str();
	};

	std::stringstream ss;
	ss << "AABB{ pMin:" << vec3ToString(pMin) << ", pMax:" << vec3ToString(pMax) << " }";
	return ss.str();
}