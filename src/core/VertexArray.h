#pragma once

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Buffer.h"
#include "GLStateObject.h"

class VertexArray;
using VertexArrayPtr = std::shared_ptr<VertexArray>;

struct VertexArrayAttrib
{
	uint32_t index;
	int count;
	DataType type;
	uint32_t offset;
	uint32_t binding = 0;
};

struct VertexArrayLayout
{
	std::vector<VertexArrayAttrib> attribs;
	int64_t stride;
};

class VertexArray :
	public GLStateObject
{
public:
	VertexArray(const VertexArrayLayout& layout);
	~VertexArray();

	void attribute(uint32_t index, int count, DataType type, uint32_t offset, uint32_t binding = 0);
	void attribute(const VertexArrayAttrib& attrib);

	void bind();
	void unbind();

	int64_t stride() const { return mLayout.stride; }
	// Create another VA object with same layouts
	VertexArrayPtr deviceCopy() { return create(mLayout); }
	
	static VertexArrayPtr create(const VertexArrayLayout& layout);

	static VertexArray* currentBinding() { return curBinding; }
	static VertexArrayPtr layoutPos2() { return VALayoutPos2; }
	static VertexArrayPtr layoutPos3Tex2Norm3Interw() { return VALayoutPos3Tex2Norm3Interw; }
	static VertexArrayPtr createNewLayoutPos2();
	static VertexArrayPtr createNewLayoutPos3Tex2Norm3Interw();

	static void initDefaultLayouts();

private:
	static VertexArray* curBinding;
	static VertexArrayPtr VALayoutPos2;
	static VertexArrayPtr VALayoutPos3Tex2Norm3Interw;

private:
	VertexArrayLayout mLayout;
};