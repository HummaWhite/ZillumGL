#pragma once

#include <memory>

#include "Texture.h"

class EnvironmentMap;
using EnvironmentMapPtr = std::shared_ptr<EnvironmentMap>;

class EnvironmentMap
{
public:
	EnvironmentMap(const File::path& path);

	Texture2DPtr envMap() { return mEnvMap; }
	Texture2DPtr aliasTable() { return mAliasTable; }
	Texture2DPtr aliasProb() { return mAliasProb; }

	int width() const { return mEnvMap->width(); }
	int height() const { return mEnvMap->height(); }

	int sumPdf() const { return mSumPdf; }

	static EnvironmentMapPtr create(const File::path& path);

private:
	static float setupAliasTable(int* alias, float* pdf, int n, int stride);

private:
	Texture2DPtr mEnvMap;
	Texture2DPtr mAliasTable;
	Texture2DPtr mAliasProb;
	float mSumPdf = 0.0f;
};