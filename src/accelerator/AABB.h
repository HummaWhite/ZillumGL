#pragma once

#include <iostream>
#include <sstream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class AABB
{
public:
	AABB() : pMin(1e8f), pMax(-1e8f) {}

	AABB(const glm::vec3& p) : pMin(p), pMax(p) {}

	AABB(const glm::vec3& pMin, const glm::vec3& pMax) :
		pMin(pMin), pMax(pMax) {}

	AABB(float xMin, float yMin, float zMin, float xMax, float yMax, float zMax) :
		pMin(xMin, yMin, zMin), pMax(xMax, yMax, zMax) {}

	AABB(const glm::vec3& center, float radius) :
		pMin(center - glm::vec3(radius)), pMax(center + glm::vec3(radius)) {}

	AABB(const glm::vec3& va, const glm::vec3& vb, const glm::vec3& vc);
	AABB(const AABB& boundA, const AABB& boundB);
	void expand(const AABB& rhs);
	glm::vec3 centroid() const;
	float surfaceArea() const;
	int maxExtent() const;

	std::string toString();

public:
	glm::vec3 pMin;
	glm::vec3 pMax;
};

