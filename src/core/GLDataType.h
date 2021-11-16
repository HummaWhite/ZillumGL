#pragma once

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

enum class DataType
{
	I8 = GL_BYTE, U8 = GL_UNSIGNED_BYTE,
	I16 = GL_SHORT, U16 = GL_UNSIGNED_SHORT,
	I32 = GL_INT, U32 = GL_UNSIGNED_INT,
	F16 = GL_HALF_FLOAT, F32 = GL_FLOAT, F64 = GL_DOUBLE
};

static int sizeofGLType(GLenum type)
{
	switch (type)
	{
	case GL_FLOAT:			return sizeof(GLfloat);
	case GL_DOUBLE:			return sizeof(GLdouble);
	case GL_SHORT:			return sizeof(GLshort);
	case GL_UNSIGNED_SHORT:	return sizeof(GLushort);
	case GL_INT:			return sizeof(GLint);
	case GL_UNSIGNED_INT:	return sizeof(GLuint);
	case GL_BYTE:			return sizeof(GLbyte);
	case GL_UNSIGNED_BYTE:	return sizeof(GLubyte);
	}
	return 0;
}

static int sizeofType(DataType type)
{
	return sizeofGLType(static_cast<GLenum>(type));
}