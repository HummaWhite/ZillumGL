#pragma once

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

enum class GLStateObjectType
{
	Buffer, VertexArray, FrameBuffer, RenderBuffer,
	Shader, Texture
};

class GLStateObject
{
public:
	GLStateObject(GLStateObjectType type) : mType(type) {}

	GLStateObjectType type() const { return mType; }
	GLuint id() const { return mId; }
	bool hasGLHandle() const { return !mId; }

protected:
	GLuint mId = 0;

private:
	GLStateObjectType mType;
};