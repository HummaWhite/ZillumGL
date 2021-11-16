#include "VertexArray.h"

const VertexArrayLayout LayoutPos2 =
{
	{ { 0, 2, DataType::F32, 0, 0 } }, 8
};

const VertexArrayLayout LayoutPos3Tex2Norm3Interw =
{
	{
		{ 0, 3, DataType::F32, 0, 0 },
		{ 1, 2, DataType::F32, 12, 0 },
		{ 2, 3, DataType::F32, 20, 0 }
	}, 32
};

VertexArray* VertexArray::curBinding = nullptr;
VertexArrayPtr VertexArray::VALayoutPos2;
VertexArrayPtr VertexArray::VALayoutPos3Tex2Norm3Interw;

VertexArray::VertexArray(const VertexArrayLayout& layout) :
	mLayout(layout), GLStateObject(GLStateObjectType::VertexArray)
{
	glCreateVertexArrays(1, &mId);
	for (const auto& i : layout.attribs)
		attribute(i);
}

VertexArray::~VertexArray()
{
	glDeleteVertexArrays(1, &mId);
}

void VertexArray::attribute(uint32_t index, int count, DataType type, uint32_t offset, uint32_t binding)
{
	glEnableVertexArrayAttrib(mId, index);
	glVertexArrayAttribFormat(mId, index, count, static_cast<GLenum>(type), false, offset);
	glVertexArrayAttribBinding(mId, index, binding);
}

void VertexArray::attribute(const VertexArrayAttrib& attrib)
{
	attribute(attrib.index, attrib.count, attrib.type, attrib.offset, attrib.binding);
}

void VertexArray::bind()
{
	glBindVertexArray(mId);
	curBinding = this;
}

void VertexArray::unbind()
{
	glBindVertexArray(0);
	curBinding = nullptr;
}

VertexArrayPtr VertexArray::create(const VertexArrayLayout& layout)
{
	return std::make_shared<VertexArray>(layout);
}

VertexArrayPtr VertexArray::createNewLayoutPos2()
{
	return create(LayoutPos2);
}

VertexArrayPtr VertexArray::createNewLayoutPos3Tex2Norm3Interw()
{
	return create(LayoutPos3Tex2Norm3Interw);
}

void VertexArray::initDefaultLayouts()
{
	VALayoutPos2 = VertexArray::create(LayoutPos2);
	VALayoutPos3Tex2Norm3Interw = VertexArray::create(LayoutPos3Tex2Norm3Interw);
}
