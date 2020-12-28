#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "Lighting.h"
#include "Material.h"
#include "Model.h"

class SceneLoader
{
public:
	static bool loadScene(std::vector<std::shared_ptr<Model>>& models, std::vector<MaterialPBR>& materials, std::vector<std::shared_ptr<Light>>& lights, const char* filePath);
	static bool saveScene(std::vector<std::shared_ptr<Model>>& models, std::vector<MaterialPBR>& materials, std::vector<std::shared_ptr<Light>>& lights, const char* filePath);
};