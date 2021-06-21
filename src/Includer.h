#pragma once

#include "core/Buffer.h"
#include "core/BufferLayout.h"
#include "core/Camera.h"
#include "core/CheckError.h"
#include "core/EngineBase.h"
#include "core/FrameBuffer.h"
#include "core/GLSizeofType.h"
#include "core/Inputs.h"
#include "core/Model.h"
#include "core/RenderBuffer.h"
#include "core/Renderer.h"
#include "core/Shader.h"
#include "core/Shape.h"
#include "core/Texture.h"
#include "core/VertexArray.h"
#include "core/VerticalSync.h"
#include "core/Sampler.h"

static char modelPathBuf[128] = { 0 };
static char shaderPathBuf[128] = { 0 };
static char scenePathBuf[128] = { 0 };
static char envMapBuf[128] = { 0 };
static int windowSizeBuf[2] = { 0 };
static const char* defaultModelPath = "res/model/";
static const char* defaultShaderPath = "res/shader/";
static const char* defaultScene = "res/IBL.sc";