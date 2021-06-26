#pragma once

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <map>

#include "Texture.h"
#include "Buffer.h"

const std::string SHADER_DEFAULT_DIR = "res/shader/";

enum class ShaderType
{
	Graphics, Compute
};

class Shader
{
public:
	Shader() : m_ID(0) {}
	Shader(const char* filePath);
	~Shader();

	GLuint ID() const { return m_ID; }

	bool load(const char* filePath);
	void enable() const;
	void disable() const;

	void set1i(const char* name, int v) const;
	void set1f(const char* name, float v0) const;
	void set2i(const char* name, int v0, int v1) const;
	void set2f(const char* name, float v0, float v1) const;
	void set3f(const char* name, float v0, float v1, float v2) const;
	void set4f(const char* name, float v0, float v1, float v2, float v3) const;

	void setVec2(const char* name, const glm::vec2& vec) const;
	void setVec3(const char* name, const glm::vec3& vec) const;
	void setVec4(const char* name, const glm::vec4& vec) const;
	void setMat3(const char* name, const glm::mat3& mat) const;
	void setMat4(const char* name, const glm::mat4& mat) const;

	void setTexture(const char* name, const Texture& tex);
	void setTexture(const char* name, const Texture& tex, uint32_t slot);

	void setUniformBlock(const Buffer& buffer, int binding);

	int getUniformLocation(const char* name) const;
	static GLint getUniformLocation(GLuint programID, const char* name);

	std::string name() const { return m_Name; }
	ShaderType type() const { return m_Type; }

	void resetTextureMap();

	void setComputeShaderSizeHint(int x, int y, int z);

private:
	enum class ShaderLoadStat
	{
		None, Vertex, Fragment, Geometry, Compute
	};

	struct ShaderText
	{
		std::string vertex;
		std::string fragment;
		std::string geometry;
		std::string compute;
	};

private:
	void loadShader(std::fstream& file, ShaderText& text, ShaderLoadStat stat, std::map<std::string, bool>& inclRec);
	void compileShader(const ShaderText& text);
	void checkShaderCompileInfo(uint32_t shaderId, const std::string& name);
	void checkShaderLinkInfo();

private:
	GLuint m_ID;
	std::string m_Name;
	ShaderType m_Type;

	std::map<std::string, int> m_TextureMap;
};