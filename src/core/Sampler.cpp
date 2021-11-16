#include "Sampler.h"

NAMESPACE_BEGIN(Sampler)

float radicalInverse(int i, int base)
{
	int numPoints = 1;
	int inv = 0;

	while (i > 0)
	{
		inv = inv * base + (i % base);
		numPoints = numPoints * base;
		i /= base;
	}
	return static_cast<float>(inv) / numPoints;
}

uint32_t sobolSample(uint32_t index, int dim, uint32_t scramble)
{
	uint32_t r = scramble;

	for (int i = dim * SobolMatricesSize; index != 0; index >>= 1, i++)
	{
		if (index & 1) r ^= SobolMatrices[i];
	}
	return r;
}

TextureBufferedPtr genHaltonSeqTexture(int nSamples, int nDim)
{
	size_t size = nSamples * nDim;
	float* samples = new float[size];

	for (int i = 0; i < nSamples; i++)
	{
		for (int j = 0; j < nDim; j++)
		{
			samples[i * nDim + j] = radicalInverse(i, PRIMES[j]);
		}
	}
	auto ret = TextureBuffered::createTyped<float>(samples, size, TextureFormat::Col1x32f);

	delete[] samples;
	return ret;
}

TextureBufferedPtr genSobolSeqTexture(int nSamples, int nDim)
{
	size_t size = nSamples * nDim;
	uint32_t* samples = new uint32_t[size];

	for (int i = 0; i < nSamples; i++)
	{
		for (int j = 0; j < nDim; j++)
		{
			samples[i * nDim + j] = sobolSample(i, j);
		}
	}
	auto ret = TextureBuffered::createTyped<uint32_t>(samples, size, TextureFormat::Col1x32u);

	delete[] samples;
	return ret;
}

Texture2DPtr genNoiseTexture(int width, int height)
{
	size_t size = static_cast<size_t>(width) * height * 2;
	float* data = new float[size];

	std::default_random_engine rg;
	for (int i = 0; i < size; i++)
		data[i] = std::uniform_real_distribution<float>(0.0f, 1.0f)(rg);

	auto tex = Texture2D::createFromMemory(TextureFormat::Col2x32f,
		width, height, TextureSourceFormat::Col2f, DataType::F32, data);

	delete[] data;
	return tex;
}

NAMESPACE_END(Sampler)