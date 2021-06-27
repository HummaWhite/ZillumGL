#include "Sampler.h"

namespace Sampler
{
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

	std::shared_ptr<BufferTexture> genHaltonSeqTexture(int nSamples, int nDim)
	{
		auto ret = std::make_shared<BufferTexture>();
		size_t size = nSamples * nDim;
		float* samples = new float[size];

		for (int i = 0; i < nSamples; i++)
		{
			for (int j = 0; j < nDim; j++)
			{
				samples[i * nDim + j] = radicalInverse(i, PRIMES[j]);
			}
		}

		ret->allocate(size * sizeof(float), samples, GL_R32F);

		delete[] samples;
		return ret;
	}

	std::shared_ptr<BufferTexture> genSobolSeqTexture(int nSamples, int nDim)
	{
		auto ret = std::make_shared<BufferTexture>();
		size_t size = nSamples * nDim;
		uint32_t* samples = new uint32_t[size];

		for (int i = 0; i < nSamples; i++)
		{
			for (int j = 0; j < nDim; j++)
			{
				samples[i * nDim + j] = sobolSample(i, j);
			}
		}
		ret->allocate(size * sizeof(uint32_t), samples, GL_R32UI);

		delete[] samples;
		return ret;
	}

	std::shared_ptr<Texture> genNoiseTexture(int width, int height)
	{
		auto ret = std::make_shared<Texture>();
		size_t size = width * height * 2;
		float* data = new float[size];

		std::default_random_engine rg;
		for (int i = 0; i < size; i++)
		{
			data[i] = std::uniform_real_distribution<float>(0.0f, 1.0f)(rg);
		}

		ret->loadData(GL_RG32F, width, height, GL_RG, GL_FLOAT, data);
		delete[] data;
		return ret;
	}
}