#include "EnvironmentMap.h"

#include <fstream>
#include <thread>
#include <queue>

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
		return i * (width + 1) + j;
	};

	auto brightness = [](float* p)
	{
		return (0.299f * (*p) + 0.587f * (*(p + 1)) + 0.114f * (*(p + 2)));
	};

	std::cout << "[EnvMap]\tGenerating importance sampling table ...\n";

	float* pdf = new float[(width + 1) * height];
	int* alias = new int[(width + 1) * height];

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			pdf[offset(i, j)] = brightness(data + 3 * (i * width + j)) * std::sin((float)(i + 0.5f) / height * 3.141592653589793f);
		}
	}
	stbi_image_free(data);

	for (int i = 0; i < height; i++)
	{
		pdf[offset(i, width)] = setupAliasTable(alias + offset(i, 0), pdf + offset(i, 0), width, 1);
	}
	sumPdf = setupAliasTable(alias + width, pdf + width, height, width + 1);

	aliasTable.loadData(GL_R32I, width + 1, height, GL_RED_INTEGER, GL_INT, alias);
	aliasTable.setFilterAndWrapping(GL_NEAREST, GL_CLAMP_TO_EDGE);
	aliasProb.loadData(GL_R32F, width + 1, height, GL_RED, GL_FLOAT, pdf);
	delete[] pdf;
	delete[] alias;
}

float EnvironmentMap::setupAliasTable(int* alias, float* pdf, int n, int stride)
{
	typedef std::pair<int, float> Element;
	float sum = 0.0f;

	for (int i = 0, offset = 0; i < n; i++, offset += stride) sum += pdf[offset];
	float sumInv = n / sum;

	// 奈何STL太慢，开了O2还是比数组方法慢一倍
	Element* greater = new Element[n * 2], * lesser = new Element[n * 2];
	int gTop = 0, lTop = 0;

	for (int i = 0, offset = 0; i < n; i++, offset += stride)
	{
		pdf[offset] *= sumInv;
		(pdf[offset] < 1.0f ? lesser[lTop++] : greater[gTop++]) = Element(i, pdf[offset]);
	}

	while (gTop != 0 && lTop != 0)
	{
		auto [l, pl] = lesser[--lTop];
		auto [g, pg] = greater[--gTop];

		alias[l * stride] = g;
		pdf[l * stride] = pl;

		pg += pl - 1.0f;
		(pg < 1.0f ? lesser[lTop++] : greater[gTop++]) = Element(g, pg);
	}

	while (gTop != 0)
	{
		auto [g, pg] = greater[--gTop];
		alias[g * stride] = g;
		pdf[g * stride] = 1.0f;
	}

	while (lTop != 0)
	{
		auto [l, pl] = lesser[--lTop];
		alias[l * stride] = l;
		pdf[l * stride] = 1.0f;
	}

	delete[] greater;
	delete[] lesser;

	return sum;
}
