#pragma once

#include <vector>
#include <memory>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "AABB.h"
#include "../Buffer.h"
#include "../Model.h"

enum class BVHSplitMethod { SAH, Middle, EqualCounts };

struct PackedBVH
{
	std::vector<AABB> bounds;
	std::vector<int> sizeIndices;
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
	BVH(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, const std::vector<uint8_t>& matIndices, BVHSplitMethod splitMethod = BVHSplitMethod::SAH) :
		vertices(vertices), indices(indices), matIndices(matIndices), method(splitMethod) {}

	PackedBVH build();

private:
	void build(int offset, const AABB& nodeBound, int l, int r);

private:
	std::vector<glm::vec3> vertices;
	std::vector<uint32_t> indices;
	std::vector<PrimInfo> primInfo;
	const std::vector<uint8_t> matIndices;
	std::vector<AABB> bounds;
	std::vector<int> sizeIndices;
	BVHSplitMethod method;
};

