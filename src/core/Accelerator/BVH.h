#pragma once

#include <vector>
#include <memory>
#include <stack>

#include "AABB.h"
#include "../Buffer.h"
#include "../Model.h"

enum class BVHSplitMethod { SAH, Middle, EqualCounts };

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
	BVH(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, BVHSplitMethod splitMethod = BVHSplitMethod::SAH) :
		vertices(vertices), indices(indices), method(splitMethod) {}

	PackedBVH build();

private:
	void build(int offset, const AABB& nodeBound, int l, int r);
	void buildHitTable();

private:
	std::vector<glm::vec3> vertices;
	std::vector<uint32_t> indices;
	std::vector<PrimInfo> primInfo;
	std::vector<AABB> bounds;
	std::vector<int> sizeIndices;
	std::vector<int> hitTable;
	BVHSplitMethod method;
	int treeSize = 0;
};

