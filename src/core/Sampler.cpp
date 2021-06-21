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
		float* samples = new float[size];

		uint32_t L = (uint32_t)std::ceil(std::log((double)nSamples) / std::log(2.0));

		// C[i] = index from the right of the first zero bit of i
		uint32_t* C = new uint32_t[nSamples];
		for (uint32_t i = 0; i < nSamples; i++)
		{
			C[i] = 0;
			uint32_t value = i;
			while (value & 1)
			{
				value >>= 1;
				C[i]++;
			}
		}

		for (uint32_t j = 0; j < nDim; j++) samples[0 * nDim + j] = 0.0;

		uint32_t* V = new uint32_t[L];
		for (uint32_t i = 0; i < L; i++) V[i] = 1 << (31 - i);

		uint32_t* X = new uint32_t[nSamples];
		X[0] = 0;
		for (uint32_t i = 1; i < nSamples; i++)
		{
			X[i] = X[i - 1] ^ V[C[i - 1]];
			samples[i * nDim + 0] = (float)X[i] / std::pow(2.0, 32);
		}

		for (uint32_t j = 1; j < nDim; j++)
		{
			auto [a, m] = SOBOL_MATRICES[j - 1];
			uint32_t s = m.size();

			// Compute direction numbers V[1] to V[L], scaled by pow(2,32)
			if (L <= s)
			{
				for (uint32_t i = 0; i < L; i++) V[i] = m[i] << (31 - i);
			}
			else
			{
				for (uint32_t i = 0; i < s; i++) V[i] = m[i] << (31 - i);

				for (uint32_t i = s; i < L; i++)
				{
					V[i] = V[i - s] ^ (V[i - s] >> s);

					for (uint32_t k = 1; k < s; k++)
					{
						V[i] ^= (((a >> (s - 1 - k)) & 1) * V[i - k]);
					}
				}
			}

			X[0] = 0;
			for (uint32_t i = 1; i < nSamples; i++)
			{
				X[i] = X[i - 1] ^ V[C[i - 1]];
				samples[i * nDim + j] = (float)X[i] / std::pow(2.0, 32);
			}
		}
		delete[] V;
		delete[] X;
		delete[] C;

		ret->allocate(size * sizeof(float), samples, GL_R32F);

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