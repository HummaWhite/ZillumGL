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

struct BVHBuffer
{
	std::shared_ptr<Buffer> vertex;
	std::shared_ptr<Buffer> normal;
	std::shared_ptr<Buffer> index;
	std::shared_ptr<Buffer> bound;
	std::shared_ptr<Buffer> sizeIndex;
};

class BVH
{
public:
	BVH(const std::vector<Mesh>& meshes, BVHSplitMethod splitMethod = BVHSplitMethod::SAH);
	~BVH();
	BVHBuffer genBuffers();

private:
	struct HittableInfo
	{
		AABB bound;
		glm::vec3 centroid;
		int index;
	};

private:
	void build(int offset, std::vector<HittableInfo>& hittableInfo, const AABB& nodeBound, int l, int r);

private:
	BVHSplitMethod splitMethod;
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec3> normals;
	std::vector<uint32_t> indices;
	std::vector<AABB> bounds;
	std::vector<int> sizeIndices;
};

