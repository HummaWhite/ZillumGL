#pragma once

#include <iostream>
#include <vector>

#include "../thirdparty/imgui.hpp"
#include "../core/Scene.h"

bool cameraEditor(Camera& camera, const std::string& name);
bool materialEditor(Scene& scene, int& matIndex);
bool modelEditor(Scene& scene, int& modelIndex, int& meshIndex, int& matIndex);
bool lightEditor(Scene& scene, int& lightIndex);