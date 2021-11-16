#pragma once

#include <iostream>
#include <memory>
#include <vector>
#include <map>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Texture.h"
#include "Buffer.h"
#include "GLStateObject.h"

class Shader;
using ShaderPtr = std::shared_ptr<Shader>;

enum class ShaderType
{
	Graphics, Compute
};

class Shader :
	public GLStateObject
{
public:
	Shader(const File::path& path, const glm::ivec3& computeSize, const std::string& extensionStr);
	~Shader();

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

	void setTexture(const char* name, TexturePtr tex, uint32_t slot);

	int getUniformLocation(const char* name) const;
	static GLint getUniformLocation(GLuint programID, const char* name);

	std::string name() const { return mName; }
	ShaderType type() const { return mType; }

	static ShaderPtr create(const File::path& path, const glm::ivec3& computeSize = glm::ivec3(1),
		const std::string& extensionStr = "");

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
	void loadShader(std::fstream& file, ShaderText& text, ShaderLoadStat stat, std::map<File::path, bool>& inclRec);
	void compileShader(const ShaderText& text);
	void checkShaderCompileInfo(uint32_t shaderId, const std::string& name);
	void checkShaderLinkInfo();

private:
	std::string mName;
	ShaderType mType;

	std::string mExtensionStr;
	glm::ivec3 mComputeGroupSize;
};