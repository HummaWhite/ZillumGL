#pragma once

#include <iostream>
#include <sstream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class AABB
{
public:
	AABB() {}
	AABB(const glm::vec3 _pMin, const glm::vec3 _pMax) :
		pMin(_pMin), pMax(_pMax) {}

	AABB(float xMin, float yMin, float zMin, float xMax, float yMax, float zMax) :
		pMin(xMin, yMin, zMin), pMax(xMax, yMax, zMax) {}

	AABB(const glm::vec3& center, float radius) :
		pMin(center - glm::vec3(radius)), pMax(center + glm::vec3(radius)) {}

	AABB(const glm::vec3& va, const glm::vec3& vb, const glm::vec3& vc);
	AABB(const AABB& boundA, const AABB& boundB);
	glm::vec3 centroid() const;
	float surfaceArea() const;
	int maxExtent() const;

	std::string toString();

public:
	glm::vec3 pMin;
	glm::vec3 pMax;
};

