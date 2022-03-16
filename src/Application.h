#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <functional>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <random>
#include <ctime>
#include <map>

#include "core/Buffer.h"
#include "core/Camera.h"
#include "core/CheckError.h"
#include "core/FrameBuffer.h"
#include "core/GLSizeofType.h"
#include "core/Model.h"
#include "core/RenderBuffer.h"
#include "core/Pipeline.h"
#include "core/Scene.h"
#include "core/Texture.h"
#include "core/VertexArray.h"
#include "core/VerticalSync.h"
#include "core/Sampler.h"
#include "core/Integrator.h"
#include "util/NamespaceDecl.h"
#include "util/Error.h"
#include "util/NamespaceDecl.h"

NAMESPACE_BEGIN(Application)

void init(int width, int height, const std::string& title);
int run();

NAMESPACE_END(Application)