#pragma once

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <memory>

#include "Shader.h"
#include "Buffer.h"
#include "VertexArray.h"
#include "FrameBuffer.h"
#include "../util/NamespaceDecl.h"
#include "../util/EnumBitField.h"

class Pipeline;
using PipelinePtr = std::shared_ptr<Pipeline>;

enum class PrimitiveType
{
	Points = GL_POINTS,
	Lines = GL_LINES, LineLoop = GL_LINE_LOOP, LineStrip = GL_LINE_STRIP,
	Triangles = GL_TRIANGLES, TriangleStrip = GL_TRIANGLE_STRIP, TriangleFan = GL_TRIANGLE_FAN
};

enum class PolygonMode
{
	Point = GL_POINT, Line = GL_LINE, Fill = GL_FILL
};

enum class DrawFace
{
	Front = GL_FRONT, Back = GL_BACK, FrontBack = GL_FRONT_AND_BACK
};

enum class BufferBit
{
	Color = GL_COLOR_BUFFER_BIT, Depth = GL_DEPTH_BUFFER_BIT, Stencil = GL_STENCIL_BUFFER_BIT
};

template<>
struct EnableEnumBitMask<BufferBit> { static constexpr bool enable = true; };

enum class MemoryBarrierBit
{
	VertexAttribArray = GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT,
	ElementArray = GL_ELEMENT_ARRAY_BARRIER_BIT,
	Uniform = GL_UNIFORM_BARRIER_BIT,
	TextureFetch = GL_TEXTURE_FETCH_BARRIER_BIT,
	ShaderImageAccess = GL_SHADER_IMAGE_ACCESS_BARRIER_BIT,
	Command = GL_COMMAND_BARRIER_BIT,
	PixelBuffer = GL_PIXEL_BUFFER_BARRIER_BIT,
	TextureUpdate = GL_TEXTURE_UPDATE_BARRIER_BIT,
	BufferUpdate = GL_BUFFER_UPDATE_BARRIER_BIT,
	FrameBuffer = GL_FRAMEBUFFER_BARRIER_BIT,
	TransformFeedback = GL_TRANSFORM_FEEDBACK_BARRIER_BIT,
	AtomicCounter = GL_ATOMIC_COUNTER_BARRIER_BIT,
	ShaderStorage = GL_SHADER_STORAGE_BARRIER_BIT
};

template<>
struct EnableEnumBitMask<MemoryBarrierBit> { static constexpr bool enable = true; };

struct PipelineCreateInfo
{
	PrimitiveType primType;
	PolygonMode polyMode;
	DrawFace faceToDraw;
	BufferBit clearBuffer;
};

class Pipeline
{
public:
	Pipeline(const PipelineCreateInfo& createInfo) :
		mPrimType(createInfo.primType), mPolyMode(createInfo.polyMode), mFaceToDraw(createInfo.faceToDraw),
		mClearBuffer(createInfo.clearBuffer) {}

	void setFaceToDraw(DrawFace faceToDraw) { mFaceToDraw = faceToDraw; }
	void setPolygonMode(PolygonMode polyMode) { mPolyMode = polyMode; }
	void setClearBuffer(BufferBit bits) { mClearBuffer = bits; }
	void setViewport(int x, int y, int width, int height);

	void clear(const glm::vec4& color);
	void draw(VertexBufferPtr vertices, VertexArrayPtr interpretor, ShaderPtr shader);
	void drawIndexed(VertexBufferPtr vertices, VertexArrayPtr interpretor, VertexBufferPtr indices, ShaderPtr shader);

	std::vector<uint8_t> readFramePixels();

	glm::ivec4 viewport() const { return mViewport; }

	static PipelinePtr create(const PipelineCreateInfo& createInfo);

	static void dispatchCompute(int xNum, int yNum, int zNum, ShaderPtr shader);
	static void memoryBarrier(MemoryBarrierBit bits);

	static void bindTextureToImage(TexturePtr tex, uint32_t unit, int level, ImageAccess access, TextureFormat format);

private:
	PrimitiveType mPrimType;
	PolygonMode mPolyMode;
	DrawFace mFaceToDraw;
	BufferBit mClearBuffer;

	FrameBufferPtr mFrameBuffer;

	glm::ivec4 mViewport;
};