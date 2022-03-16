#pragma once

#include <vector>
#include <memory>
#include <stack>

#include "AABB.h"
#include "../core/Buffer.h"
#include "../core/Model.h"

const int BVH_LEAF_MASK = 0x80000000;

struct PackedBVH
{
	std::vector<AABB> bounds;
	std::vector<int> hitTable;
};

struct PrimInfo
{
	AABB bound;
	glm::vec3 centroid;
	int index;
};

class BVH
{
public:
	BVH(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices) :
		vertices(vertices), indices(indices) {}

	PackedBVH build();

private:
	void standardBuild(const AABB& rootExtent);
	void quickBuild(const AABB& rootExtent);
	void buildHitTable();

private:
	std::vector<glm::vec3> vertices;
	std::vector<uint32_t> indices;
	std::vector<PrimInfo> primInfo;
	std::vector<AABB> bounds;
	std::vector<int> sizeIndices;
	std::vector<int> hitTable;
	size_t treeSize = 0;
};

