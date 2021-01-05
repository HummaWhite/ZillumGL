#include "BVH.h"

BVH::BVH(const std::vector<Mesh>& meshes, BVHSplitMethod splitMethod):
	splitMethod(splitMethod)
{
	std::cout << "[BVH]\t\tCPU Building ...  ";
	uint32_t indicesCount = 0;

	for (uint32_t i = 0; i < meshes.size(); i++)
	{
		vertices.insert(vertices.end(), meshes[i].vertices.begin(), meshes[i].vertices.end());
		normals.insert(normals.end(), meshes[i].normals.begin(), meshes[i].normals.end());

		std::vector<uint32_t> transIndices(meshes[i].indices.size());
		std::transform(meshes[i].indices.begin(), meshes[i].indices.end(), transIndices.begin(),
			[i, indicesCount](uint32_t x) { return (x + indicesCount) | (i << 24); });
		// 8bit meshIndex | 24bit vertexIndex

		indices.insert(indices.end(), transIndices.begin(), transIndices.end());
		indicesCount += meshes[i].indices.size();
	}

	std::vector<HittableInfo> hittableInfo(indicesCount / 3);

	const auto clamp = [](uint32_t x) { return x & 0x00ffffff; };

	AABB bound;
	for (int i = 0; i < hittableInfo.size(); i++)
	{
		HittableInfo hInfo;
		hInfo.bound = AABB(
			vertices[clamp(indices[i * 3 + 0])],
			vertices[clamp(indices[i * 3 + 1])],
			vertices[clamp(indices[i * 3 + 2])]);
		hInfo.centroid = hInfo.bound.centroid();
		hInfo.index = i;
		bound = (i == 0) ? hInfo.bound : AABB(bound, hInfo.bound);
		hittableInfo[i] = hInfo;
	}

	bounds.resize(hittableInfo.size() * 2 - 1);
	sizeIndices.resize(hittableInfo.size() * 2 - 1);
	build(0, hittableInfo, bound, 0, hittableInfo.size() - 1);
	std::cout << "[" << vertices.size() << " vertices, " << indices.size() / 3 << " triangles, " << bounds.size() << " nodes]\n";
}

BVH::~BVH()
{
}

BVHBuffer BVH::genBuffers()
{
	auto vertexBuf = std::make_shared<Buffer>();
	auto normalBuf = std::make_shared<Buffer>();
	auto indexBuf = std::make_shared<Buffer>();
	auto boundBuf = std::make_shared<Buffer>();
	auto sizeIndexBuf = std::make_shared<Buffer>();

	vertexBuf->allocate(vertices.size() * sizeof(glm::vec3), vertices.data(), 0);
	normalBuf->allocate(normals.size() * sizeof(glm::vec3), normals.data(), 0);
	indexBuf->allocate(indices.size() * sizeof(uint32_t), indices.data(), 0);
	boundBuf->allocate(bounds.size() * sizeof(AABB), bounds.data(), 0);
	sizeIndexBuf->allocate(sizeIndices.size() * sizeof(int), sizeIndices.data(), 0);

	return BVHBuffer{ vertexBuf, normalBuf, indexBuf, boundBuf, sizeIndexBuf };
}

void BVH::build(int offset, std::vector<HittableInfo>& hittableInfo, const AABB& nodeBound, int l, int r)
{
	int dim = nodeBound.maxExtent();
	int size = (r - l) * 2 + 1;
	bounds[offset] = nodeBound;
	sizeIndices[offset] = (size == 1) ? -hittableInfo[l].index : size;

	if (l == r) return;

	const auto getDim = [](const glm::vec3& v, int d) { return *(float*)(&v.x + d); };

	auto cmp = [dim, getDim](const HittableInfo& a, const HittableInfo& b)
	{
		return getDim(a.centroid, dim) < getDim(b.centroid, dim);
	};

	std::sort(hittableInfo.begin() + l, hittableInfo.begin() + r, cmp);
	int hittableCount = r - l + 1;

	if (hittableCount == 2)
	{
		build(offset + 1, hittableInfo, hittableInfo[l].bound, l, l);
		build(offset + 2, hittableInfo, hittableInfo[r].bound, r, r);
		return;
	}

	AABB* boundInfo = new AABB[hittableCount];
	AABB* boundInfoRev = new AABB[hittableCount];

	boundInfo[0] = hittableInfo[l].bound;
	boundInfoRev[hittableCount - 1] = hittableInfo[r].bound;

	for (int i = 1; i < hittableCount; i++)
	{
		boundInfo[i] = AABB(boundInfo[i - 1], hittableInfo[l + i].bound);
		boundInfoRev[hittableCount - 1 - i] = AABB(boundInfoRev[hittableCount - i], hittableInfo[r - i].bound);
	}

	int m = l;
	switch (splitMethod)
	{
	case BVHSplitMethod::SAH:
	{
		m = l;
		float cost = boundInfo[0].surfaceArea() + boundInfoRev[1].surfaceArea() * (r - l);

		for (int i = 1; i < hittableCount - 1; i++)
		{
			float c = boundInfo[i].surfaceArea() * (i + 1) + boundInfoRev[i + 1].surfaceArea() * (hittableCount - i - 1);
			if (c < cost)
			{
				cost = c;
				m = l + i;
			}
		}
	} break;

	case BVHSplitMethod::Middle:
	{
		glm::vec3 nodeCentroid = nodeBound.centroid();
		float mid = getDim(nodeCentroid, dim);
		for (m = l; m < r - 1; m++)
		{
			float tmp = getDim(hittableInfo[m].centroid, dim);
			if (tmp >= mid) break;
		}
	} break;

	case BVHSplitMethod::EqualCounts:
		m = (l + r) >> 1;
		break;

	default: break;
	}

	AABB lBound = boundInfo[m - l];
	AABB rBound = boundInfoRev[m + 1 - l];
	delete[] boundInfo;
	delete[] boundInfoRev;

	build(offset + 1, hittableInfo, lBound, l, m);
	build(offset + 2 * (m - l) + 2, hittableInfo, rBound, m + 1, r);
}
