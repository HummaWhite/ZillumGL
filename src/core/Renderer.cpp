#include "Renderer.h"

void Renderer::clear(float v0, float v1, float v2, float v3) const
{
	glClearColor(v0, v1, v2, v3);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::clearFrameBuffer(FrameBuffer& frameBuffer, float v0, float v1, float v2, float v3)
{
	frameBuffer.bind();
	glClearColor(v0, v1, v2, v3);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	frameBuffer.unbind();
}

void Renderer::draw(const VertexArray& va, const Buffer& eb, const Shader& shader, GLuint renderMode) const
{
	shader.enable();
	va.bind();
	glVertexArrayElementBuffer(va.ID(), eb.ID());
	glPolygonMode(GL_FRONT_AND_BACK, renderMode);
	glDrawElements(GL_TRIANGLES, eb.elementsCount(), GL_UNSIGNED_INT, 0);
	glVertexArrayElementBuffer(va.ID(), 0);
}

void Renderer::draw(const VertexArray& va, const Shader& shader, GLuint renderMode) const
{
	shader.enable();
	va.bind();
	glPolygonMode(GL_FRONT_AND_BACK, renderMode);
	glDrawArrays(GL_TRIANGLES, 0, va.verticesCount());
}

void Renderer::drawToFrameBuffer(FrameBuffer& frameBuffer, const VertexArray& va, const Buffer& eb, const Shader& shader, GLuint renderMode) const
{
	frameBuffer.bind();

	shader.enable();
	va.bind();
	glVertexArrayElementBuffer(va.ID(), eb.ID());
	glPolygonMode(GL_FRONT_AND_BACK, renderMode);
	glDrawElements(GL_TRIANGLES, eb.elementsCount(), GL_UNSIGNED_INT, 0);
	glVertexArrayElementBuffer(va.ID(), 0);

	frameBuffer.unbind();
}

void Renderer::drawToFrameBuffer(FrameBuffer& frameBuffer, const VertexArray& va, const Shader& shader, GLuint renderMode) const
{
	frameBuffer.bind();

	shader.enable();
	va.bind();
	glPolygonMode(GL_FRONT_AND_BACK, renderMode);
	glDrawArrays(GL_TRIANGLES, 0, va.verticesCount());

	frameBuffer.unbind();
}
