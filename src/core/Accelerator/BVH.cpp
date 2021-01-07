#include "BVH.h"

PackedBVH BVH::build()
{
	std::cout << "[BVH]\t\tCPU Building ...  ";

	primInfo.resize(indices.size() / 3);
	bounds.resize(primInfo.size() * 2 - 1);
	sizeIndices.resize(primInfo.size() * 2 - 1);

	AABB bound;
	for (int i = 0; i < primInfo.size(); i++)
	{
		PrimInfo hInfo;
		hInfo.bound = AABB(
			vertices[indices[i * 3 + 0]],
			vertices[indices[i * 3 + 1]],
			vertices[indices[i * 3 + 2]]);
		hInfo.centroid = hInfo.bound.centroid();
		hInfo.index = i | ((uint32_t)matIndices[i] << 24);
		bound = (i == 0) ? hInfo.bound : AABB(bound, hInfo.bound);
		primInfo[i] = hInfo;
	}

	build(0, bound, 0, primInfo.size() - 1);
	std::cout << "[" << vertices.size() << " vertices, " << primInfo.size() << " triangles, " << bounds.size() << " nodes]\n";

	return PackedBVH{ bounds, sizeIndices };
}

void BVH::build(int offset, const AABB& nodeBound, int l, int r)
{
	int dim = nodeBound.maxExtent();
	int size = (r - l) * 2 + 1;
	bounds[offset] = nodeBound;
	sizeIndices[offset] = (size == 1) ? (primInfo[l].index | 0x80000000) : size;

	if (l == r) return;

	const auto getDim = [](const glm::vec3& v, int d) { return *(float*)(&v.x + d); };

	auto cmp = [dim, getDim](const PrimInfo& a, const PrimInfo& b)
	{
		return getDim(a.centroid, dim) < getDim(b.centroid, dim);
	};

	std::sort(primInfo.begin() + l, primInfo.begin() + r, cmp);
	int hittableCount = r - l + 1;

	if (hittableCount == 2)
	{
		build(offset + 1, primInfo[l].bound, l, l);
		build(offset + 2, primInfo[r].bound, r, r);
		return;
	}

	AABB* boundInfo = new AABB[hittableCount];
	AABB* boundInfoRev = new AABB[hittableCount];

	boundInfo[0] = primInfo[l].bound;
	boundInfoRev[hittableCount - 1] = primInfo[r].bound;

	for (int i = 1; i < hittableCount; i++)
	{
		boundInfo[i] = AABB(boundInfo[i - 1], primInfo[l + i].bound);
		boundInfoRev[hittableCount - 1 - i] = AABB(boundInfoRev[hittableCount - i], primInfo[r - i].bound);
	}

	int m = l;
	switch (method)
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
			float tmp = getDim(primInfo[m].centroid, dim);
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

	build(offset + 1, lBound, l, m);
	build(offset + 2 * (m - l) + 2, rBound, m + 1, r);
}
