#pragma once

#include "Texture.h"

class EnvironmentMap
{
public:
	EnvironmentMap() {};

	void load(const std::string& path);

	Texture& getEnvMap() { return envMap; }
	Texture& getCdfTable() { return cdfTable; }

	int width() const { return envMap.width(); }
	int height() const { return envMap.height(); }

private:
	Texture envMap;
	Texture cdfTable;
};