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
	return vol.x * vol.y * vol.z;
}

int AABB::maxExtent() const
{
	glm::vec3 vol = pMax - pMin;

	float maxVal = vol.x;
	int maxComponent = 0;
	for (int i = 1; i < 3; i++)
	{
		float component = *((float*)&vol + i);
		if (component > maxVal)
		{
			maxVal = component;
			maxComponent = i;
		}
	}

	return maxComponent;
}
