#include "EnvironmentMap.h"

void EnvironmentMap::load(const std::string& path)
{
	std::cout << "[EnvMap]\tLoading ...\n";
	int width, height, channels;
	float* data = stbi_loadf(path.c_str(), &width, &height, &channels, 0);
	if (data == nullptr)
	{
		std::cout << "[Error]\tUnable to load envMap: " << path << std::endl;
		return;
	}

	envMap.loadData(GL_RGB16F, width, height, GL_RGB, GL_FLOAT, data);

	auto offset = [width](int i, int j)
	{
		return j * (width + 1) + i;
	};

	auto brightness = [](float* p)
	{
		return (0.299f * (*p) + 0.587f * (*(p + 1)) + 0.114f * (*(p + 2)));
	};

	std::cout << "[EnvMap]\tGenerating importance sampling table ...\n";

	float* cdf = new float[(width + 1) * height];
	for (int i = 0; i < width; i++)
	{
		for (int j = 0; j < height; j++)
		{
			cdf[offset(i, j)] = brightness(data + 3 * (width * j + i)) * std::sin((float)(j + 0.5f) / height * 3.141592653589793f);
		}
	}
	stbi_image_free(data);

	for (int i = 0; i < height; i++)
	{
		for (int j = 1; j < width; j++)
		{
			cdf[offset(j, i)] += cdf[offset(j - 1, i)];
		}
	}

	cdf[offset(width, 0)] = cdf[offset(width - 1, 0)];
	for (int i = 1; i < height; i++)
	{
		cdf[offset(width, i)] = cdf[offset(width, i - 1)] + cdf[offset(width - 1, i)];
	}

	cdfTable.loadData(GL_R32F, width + 1, height, GL_RED, GL_FLOAT, cdf);
	delete[] cdf;
}
