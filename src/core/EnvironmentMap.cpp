#include "EnvironmentMap.h"
#include "../util/Error.h"

#include <fstream>
#include <thread>
#include <queue>

EnvironmentMap::EnvironmentMap(const File::path& path)
{
	Error::bracketLine<0>("EnvMap loading " + path.generic_string());

	auto image = Image::createFromFile(path, ImageDataType::Float32);
	mEnvMap = Texture2D::createFromImage(image, TextureFormat::Col3x16f);

	int width = image->width();
	int height = image->height();
	auto data = reinterpret_cast<float*>(image->data());

	auto offset = [width](int i, int j)
	{
		return i * (width + 1) + j;
	};

	auto luminance = [](float* p)
	{
		return (0.2126f * (*p) + 0.7152f * (*(p + 1)) + 0.0722f * (*(p + 2)));
	};

	Error::bracketLine<0>("EnvMap generating sample table");

	size_t size = size_t(size_t(width) + 1) * height;

	float* pdf = new float[size];
	int* alias = new int[size];

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			pdf[offset(i, j)] = luminance(data + 3 * (i * width + j)) *
				std::sin((float)(i + 0.5f) / height * 3.141592653589793f);
		}
	}

	for (int i = 0; i < height; i++)
	{
		pdf[offset(i, width)] = setupAliasTable(alias + offset(i, 0), pdf + offset(i, 0), width, 1);
	}
	mSumPdf = setupAliasTable(alias + width, pdf + width, height, width + 1);

	mAliasTable = Texture2D::createFromMemory(TextureFormat::Col1x32i, width + 1, height,
		TextureSourceFormat::Col1i, DataType::I32, alias);
	mAliasTable->setFilterWrapping(TextureFilter::Nearest, TextureWrapping::ClampToEdge);
	mAliasProb = Texture2D::createFromMemory(TextureFormat::Col1x32f, width + 1, height,
		TextureSourceFormat::Col1f, DataType::F32, pdf);
	
	delete[] pdf;
	delete[] alias;
}

EnvironmentMapPtr EnvironmentMap::create(const File::path& path)
{
	return std::make_shared<EnvironmentMap>(path);
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
