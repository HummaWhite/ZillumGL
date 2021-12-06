#pragma once

#include "core/Buffer.h"
#include "core/Camera.h"
#include "core/CheckError.h"
#include "core/FrameBuffer.h"
#include "core/GLSizeofType.h"
#include "core/Model.h"
#include "core/RenderBuffer.h"
#include "core/Pipeline.h"
#include "core/Shader.h"
#include "core/Scene.h"
#include "core/Texture.h"
#include "core/VertexArray.h"
#include "core/VerticalSync.h"
#include "core/Sampler.h"
#include "util/NamespaceDecl.h"

NAMESPACE_BEGIN(PathTracer)

void init(int width, int height, GLFWwindow* window);
void mainLoop();

void trace();
void renderGUI();

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void cursorCallback(GLFWwindow* window, double posX, double posY);
void scrollCallback(GLFWwindow* window, double offsetX, double offsetY);
void resizeCallback(GLFWwindow* window, int width, int height);

void processKeys();

NAMESPACE_END(PathTracer)