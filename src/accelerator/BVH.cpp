#include "BVH.h"
#include "../util/Error.h"

struct BoxRec
{
	AABB vertBox;
	AABB centExtent;
};

struct Bucket
{
	Bucket() : count(0) {}
	Bucket(const Bucket& a, const Bucket& b) :
		count(a.count + b.count), box(a.box, b.box) {}
	int count;
	AABB box;
};

struct BuildRec
{
	int offset;
	AABB nodeExtent;
	int splitDim;
	int lRange;
	int rRange;
};

using RadixSortElement = std::pair<int, int>;

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
	{
		for (int i = 0; i < count; i++)
		{
			int u = a[i].first;
			b[mIndex[j][uint8_t(u >> (j << 3))]++] = a[i];
		}
		std::swap(a, b);
	}
	delete[] b;
}

void radixSort16(PrimInfo* a, int count, int dim)
{
	RadixSortElement* b = new RadixSortElement[count];

	int l = 0, r = count;
	for (int i = 0; i < count; i++)
	{
		int u = *reinterpret_cast<int*>(&a[i].centroid[dim]);
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

template<int NumBuckets>
PrimInfo* partition(PrimInfo* a, int size, float axisMin, float axisMax, int splitDim, int splitPoint)
{
	auto tmp = new PrimInfo[size];
	std::copy(a, a + size, tmp);
	int l = 0;
	int r = size;
	for (int i = 0; i < size; i++)
	{
		int b = NumBuckets * (tmp[i].centroid[splitDim] - axisMin) / (axisMax - axisMin);
		b = std::max(std::min(b, NumBuckets - 1), 0);
		(b <= splitPoint ? a[l++] : a[--r]) = tmp[i];
	}
	delete[] tmp;
	if (r == size)
		r--;
	return a + r - 1;
}

PackedBVH BVH::build()
{
	Error::bracketLine<0>("BVH building by CPU");
	primInfo.resize(indices.size() / 3);
	treeSize = primInfo.size() * 2 - 1;
	bounds.resize(treeSize);
	sizeIndices.resize(treeSize);

	AABB rootCentExtent;
	AABB rootBox;
	for (int i = 0; i < primInfo.size(); i++)
	{
		PrimInfo hInfo;
		hInfo.bound = AABB(vertices[indices[i * 3 + 0]], vertices[indices[i * 3 + 1]], vertices[indices[i * 3 + 2]]);
		hInfo.centroid = hInfo.bound.centroid();
		hInfo.index = i;
		rootCentExtent.expand(hInfo.centroid);
		rootBox.expand(hInfo.bound);
		primInfo[i] = hInfo;
	}
	//standardBuild(rootBox);
	quickBuild(rootCentExtent);
	std::cout << "\t[" << vertices.size() << " vertices, " << primInfo.size() << " triangles, " << bounds.size() << " nodes]\n";

	buildHitTable();
	return PackedBVH{ bounds, hitTable };
}

void BVH::standardBuild(const AABB& rootExtent)
{
	std::stack<BuildRec> stack;
	stack.push({ 0, rootExtent, rootExtent.maxExtent(), 0, static_cast<int>(primInfo.size()) - 1 });

	auto prefixes = new BoxRec[primInfo.size()];
	auto suffixes = new BoxRec[primInfo.size()];

	while (!stack.empty())
	{
		auto [offset, nodeExtent, splitDim, l, r] = stack.top();
		stack.pop();
		int size = (r - l) * 2 + 1;
		sizeIndices[offset] = (size == 1) ? (primInfo[l].index | BVH_LEAF_MASK) : size;

		if (l == r)
		{
			bounds[offset] = primInfo[l].bound;
			continue;
		}
		int nBoxes = r - l + 1;
		radixSort16(primInfo.data() + l, nBoxes, splitDim);
		if (nBoxes == 2)
		{
			bounds[offset] = AABB(primInfo[l].bound, primInfo[r].bound);
			stack.push({ offset + 2, primInfo[r].bound, 0, r, r });
			stack.push({ offset + 1, primInfo[l].bound, 0, l, l });
			continue;
		}
		
		auto prefix = prefixes + l;
		auto suffix = suffixes + l;
		prefix[0] = { primInfo[l].bound, primInfo[l].centroid };
		suffix[nBoxes - 1] = { primInfo[r].bound, primInfo[r].centroid };

		for (int i = 1; i < nBoxes; i++)
		{
			prefix[i] = { AABB(prefix[i - 1].vertBox, primInfo[l + i].bound),
				AABB(prefix[i - 1].centExtent, primInfo[l + i].centroid) };
			suffix[nBoxes - i - 1] = { AABB(suffix[nBoxes - i].vertBox, primInfo[r - i].bound),
				AABB(suffix[nBoxes - i].centExtent, primInfo[r - i].centroid) };
		}
		bounds[offset] = prefix[nBoxes - 1].vertBox;

		auto getCost = [&](int i) -> float
		{
			return prefix[i].vertBox.surfaceArea() * (i + 1) +
				suffix[i + 1].vertBox.surfaceArea() * (nBoxes - i - 1);
		};

		int splitPoint = l;
		float minCost = getCost(0);
		for (int i = 1; i < nBoxes - 1; i++)
		{
			float cost = getCost(i);
			if (cost < minCost)
			{
				minCost = cost;
				splitPoint = l + i;
			}
		}
		AABB lchCentBox = prefix[splitPoint - l].vertBox;
		AABB rchCentBox = suffix[splitPoint - l + 1].vertBox;

		stack.push({ offset + 2 * (splitPoint - l) + 2, rchCentBox, rchCentBox.maxExtent(), splitPoint + 1, r });
		stack.push({ offset + 1, lchCentBox, lchCentBox.maxExtent(), l, splitPoint });
	}
	delete[] prefixes;
	delete[] suffixes;
}

void BVH::quickBuild(const AABB& rootExtent)
{
	std::stack<BuildRec> stack;
	stack.push({ 0, rootExtent, rootExtent.maxExtent(), 0, static_cast<int>(primInfo.size()) - 1 });

	constexpr int NumBuckets = 16;

	while (!stack.empty())
	{
		auto [offset, nodeExtent, splitDim, l, r] = stack.top();
		stack.pop();
		int size = (r - l) * 2 + 1;
		sizeIndices[offset] = (size == 1) ? (primInfo[l].index | BVH_LEAF_MASK) : size;

		if (l == r)
		{
			bounds[offset] = primInfo[l].bound;
			continue;
		}
		int nBoxes = r - l + 1;
		if (nBoxes == 2)
		{
			bounds[offset] = AABB(primInfo[l].bound, primInfo[r].bound);
			if (primInfo[l].centroid[splitDim] > primInfo[r].centroid[splitDim])
				std::swap(primInfo[l], primInfo[r]);
			stack.push({ offset + 2, primInfo[r].centroid, 0, r, r });
			stack.push({ offset + 1, primInfo[l].centroid, 0, l, l });
			continue;
		}

		float axisMin = nodeExtent.pMin[splitDim];
		float axisMax = nodeExtent.pMax[splitDim];

		Bucket buckets[NumBuckets];
		Bucket prefix[NumBuckets];
		Bucket suffix[NumBuckets];

		for (int i = l; i <= r; i++)
		{
			int b = NumBuckets * (primInfo[i].centroid[splitDim] - axisMin) / (axisMax - axisMin);
			b = std::max(std::min(b, NumBuckets - 1), 0);
			buckets[b].count++;
			buckets[b].box.expand(primInfo[i].bound);
		}

		prefix[0] = buckets[0];
		suffix[NumBuckets - 1] = buckets[NumBuckets - 1];
		for (int i = 1; i < NumBuckets; i++)
		{
			prefix[i] = Bucket(prefix[i - 1], buckets[i]);
			suffix[NumBuckets - 1 - i] = Bucket(suffix[NumBuckets - i], buckets[NumBuckets - i - 1]);
		}
		bounds[offset] = prefix[NumBuckets - 1].box;

		int splitPoint = 0;
		float minCost = prefix[0].count * prefix[0].box.surfaceArea() +
			suffix[1].count * suffix[1].box.surfaceArea();
		for (int i = 1; i < NumBuckets - 1; i++)
		{
			float cost = prefix[i].count * prefix[i].box.surfaceArea() +
				suffix[i + 1].count * suffix[i + 1].box.surfaceArea();
			if (cost < minCost)
			{
				minCost = cost;
				splitPoint = i;
			}
		}
		auto itr = partition<NumBuckets>(&primInfo[l], nBoxes, axisMin, axisMax, splitDim, splitPoint);
		splitPoint = itr - &primInfo[0];

		AABB lchCentBox, rchCentBox;
		for (int i = l; i <= splitPoint; i++)
			lchCentBox.expand(primInfo[i].centroid);
		for (int i = splitPoint + 1; i <= r; i++)
			rchCentBox.expand(primInfo[i].centroid);

		stack.push({ offset + 2 * (splitPoint - l) + 2, rchCentBox, rchCentBox.maxExtent(), splitPoint + 1, r });
		stack.push({ offset + 1, lchCentBox, lchCentBox.maxExtent(), l, splitPoint });
	}
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

	hitTable.resize(treeSize * 18);

	auto stack = new size_t[treeSize];

	for (int i = 0; i < 6; i++)
	{
		size_t tableOffset = treeSize * 3 * i;
		size_t top = 0, index = 0;
		stack[top++] = 0;
		while (top)
		{
			auto k = stack[--top];

			bool isLeaf = sizeIndices[k] & BVH_LEAF_MASK;
			int nodeSize = isLeaf ? 1 : sizeIndices[k];

			hitTable[tableOffset + index * 3 + 0] = k;			// nodeIndex
			hitTable[tableOffset + index * 3 + 1] = isLeaf ? sizeIndices[k] ^ BVH_LEAF_MASK : -1; // primIndex
			hitTable[tableOffset + index * 3 + 2] = index + nodeSize;	// missIndex
			index++;
			if (isLeaf)
				continue;

			int lSize = sizeIndices[k + 1];
			if (lSize & BVH_LEAF_MASK)
				lSize = 1;

			size_t lch = k + 1, rch = k + 1 + lSize;

			if (!cmpFuncs[i](bounds[lch].centroid(), bounds[rch].centroid()))
				std::swap(lch, rch);
			stack[top++] = rch;
			stack[top++] = lch;
		}
	}
	delete[] stack;
}