#include "BVH.h"

typedef std::pair<int, int> RadixSortElement;

void radixSortLH(std::pair<int, int>* a, int count)
{
	RadixSortElement* b = new RadixSortElement[count];
	int mIndex[4][256];
	memset(mIndex, 0, sizeof(mIndex));

	for (int i = 0; i < count; i++)
	{
		int u = a[i].first;
		mIndex[0][uint8_t(u)]++; u >>= 8;
		mIndex[1][uint8_t(u)]++; u >>= 8;
		mIndex[2][uint8_t(u)]++; u >>= 8;
		mIndex[3][uint8_t(u)]++; u >>= 8;
	}

	int m[4] = { 0, 0, 0, 0 };
	for (int i = 0; i < 256; i++)
	{
		int n[4] = { mIndex[0][i], mIndex[1][i], mIndex[2][i], mIndex[3][i] };
		mIndex[0][i] = m[0];
		mIndex[1][i] = m[1];
		mIndex[2][i] = m[2];
		mIndex[3][i] = m[3];
		m[0] += n[0];
		m[1] += n[1];
		m[2] += n[2];
		m[3] += n[3];
	}

	for (int j = 0; j < 4; j++)
	{             // radix sort
		for (int i = 0; i < count; i++)
		{     //  sort by current lsb
			int u = a[i].first;
			b[mIndex[j][uint8_t(u >> (j << 3))]++] = a[i];
		}
		std::swap(a, b);                //  swap ptrs
	}
	delete[] b;
}

void radixSort16(PrimInfo* a, int count, int dim)
{
	auto getDim = [&](const PrimInfo& p) -> int
	{
		return *(int*)(&p.centroid.x + dim);
	};

	RadixSortElement* b = new RadixSortElement[count];

	int l = 0, r = count;
	for (int i = 0; i < count; i++)
	{
		int u = getDim(a[i]);
		if (u & 0x80000000) b[l++] = RadixSortElement(~u, i);
		else b[--r] = RadixSortElement(u, i);
	}
	radixSortLH(b, l);
	radixSortLH(b + l, count - l);

	PrimInfo* c = new PrimInfo[count];
	memcpy(c, a, count * sizeof(PrimInfo));

	for (int i = 0; i < count; i++)
	{
		a[i] = c[b[i].second];
	}
	delete[] b;
	delete[] c;
}

PackedBVH BVH::build()
{
	std::cout << "[BVH]\t\tCPU Building ...  ";
	primInfo.resize(indices.size() / 3);
	treeSize = primInfo.size() * 2 - 1;
	bounds.resize(treeSize);
	sizeIndices.resize(treeSize);

	AABB bound;
	for (int i = 0; i < primInfo.size(); i++)
	{
		PrimInfo hInfo;
		hInfo.bound = AABB(vertices[indices[i * 3 + 0]], vertices[indices[i * 3 + 1]], vertices[indices[i * 3 + 2]]);
		hInfo.centroid = hInfo.bound.centroid();
		hInfo.index = i;
		bound = (i == 0) ? hInfo.bound : AABB(bound, hInfo.bound);
		primInfo[i] = hInfo;
	}

	build(0, bound, 0, primInfo.size() - 1);
	std::cout << "[" << vertices.size() << " vertices, " << primInfo.size() << " triangles, " << bounds.size() << " nodes]\n";

	buildHitTable();
	return PackedBVH{ bounds, hitTable };
}

void BVH::build(int offset, const AABB& nodeBound, int l, int r)
{
	int dim = nodeBound.maxExtent();
	int size = (r - l) * 2 + 1;
	bounds[offset] = nodeBound;
	sizeIndices[offset] = (size == 1) ? (primInfo[l].index | BVH_LEAF_MASK) : size;

	if (l == r) return;

	const auto getDim = [](const glm::vec3& v, int d) { return *(float*)(&v.x + d); };

	auto cmp = [dim, getDim](const PrimInfo& a, const PrimInfo& b)
	{
		return getDim(a.centroid, dim) < getDim(b.centroid, dim);
	};

	//std::sort(primInfo.begin() + l, primInfo.begin() + r, cmp);
	int hittableCount = r - l + 1;
	radixSort16(primInfo.data() + l, hittableCount, dim);

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

void BVH::buildHitTable()
{
	bool (*cmpFuncs[6])(const glm::vec3 & a, const glm::vec3 & b) =
	{
		[](const glm::vec3& a, const glm::vec3& b) { return a.x > b.x; },	// X+
		[](const glm::vec3& a, const glm::vec3& b) { return a.x < b.x; },	// X-
		[](const glm::vec3& a, const glm::vec3& b) { return a.y > b.y; },	// Y+
		[](const glm::vec3& a, const glm::vec3& b) { return a.y < b.y; },	// Y-
		[](const glm::vec3& a, const glm::vec3& b) { return a.z > b.z; },	// Z+
		[](const glm::vec3& a, const glm::vec3& b) { return a.z < b.z; }		// Z-
	};

	hitTable.resize(18 * treeSize);

	int* st = new int[treeSize];

	for (int i = 0; i < 6; i++)
	{
		int tableOffset = treeSize * 3 * i;
		int top = 0, index = 0;
		st[top++] = 0;
		while (top)
		{
			int k = st[--top];

			bool isLeaf = sizeIndices[k] & BVH_LEAF_MASK;
			int nodeSize = isLeaf ? 1 : sizeIndices[k];

			hitTable[tableOffset + index * 3 + 0] = k;			// nodeIndex
			hitTable[tableOffset + index * 3 + 1] = isLeaf ? sizeIndices[k] ^ BVH_LEAF_MASK : -1; // primIndex
			hitTable[tableOffset + index * 3 + 2] = index + nodeSize;	// missIndex
			index++;
			if (isLeaf) continue;

			int lSize = sizeIndices[k + 1];
			if (lSize & BVH_LEAF_MASK) lSize = 1;

			int lch = k + 1, rch = k + 1 + lSize;

			if (!cmpFuncs[i](bounds[lch].centroid(), bounds[rch].centroid())) std::swap(lch, rch);
			st[top++] = rch;
			st[top++] = lch;
		}
	}
	delete[] st;
}
