#pragma once

#include "Texture.h"

class EnvironmentMap
{
public:
	EnvironmentMap() {};

	void load(const std::string& path);

	Texture& getEnvMap() { return envMap; }
	Texture& getAliasTable() { return aliasTable; }
	Texture& getAliasProb() { return aliasProb; }

	int width() const { return envMap.width(); }
	int height() const { return envMap.height(); }

	int sum() const { return sumPdf; }

private:
	static float setupAliasTable(int* alias, float* pdf, int n, int stride);

private:
	Texture envMap;
	Texture aliasTable;
	Texture aliasProb;
	float sumPdf = 0.0f;
};