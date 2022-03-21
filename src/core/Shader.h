#pragma once

#include <iostream>
#include <memory>
#include <vector>
#include <map>
#include <set>
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

struct ShaderSource
{
	std::string vertex;
	std::string fragment;
	std::string geometry;
	std::string compute;
};

class ShaderUniform
{
public:
	template<typename T>
	ShaderUniform(const T& val)
	{
		*this = val;
	}

	ShaderUniform(const ShaderUniform& rhs)
	{
		if (ptr)
			delete[] ptr;
		size = rhs.size;
		ptr = new uint8_t[size];
		memcpy(ptr, rhs.ptr, size);
	}

	~ShaderUniform()
	{
		if (ptr)
			delete[] ptr;
	}

	template<typename T>
	void operator = (const T& val)
	{
		if (ptr)
			delete[] ptr;
		ptr = new uint8_t[sizeof(T)];
		memcpy(ptr, &val, sizeof(T));
		size = sizeof(T);
	}

	bool operator == (const ShaderUniform& rhs) const
	{
		if (size != rhs.size)
			return false;
		return memcmp(ptr, rhs.ptr, size) == 0;
	}

	template<typename T>
	T value() const
	{
		T ret;
		memcpy(&ret, ptr, size);
		return ret;
	}

private:
	uint8_t* ptr = nullptr;
	uint8_t size = 0;
};

class Shader :
	public GLStateObject
{
public:
	Shader(const File::path& path, const glm::ivec3& computeSize, const std::string& extensionStr);
	Shader(const std::string& name, const ShaderSource& source);
	~Shader();

	void enable();
	void disable();

	void set1i(const std::string& name, int v);
	void set1f(const std::string& name, float v0);
	void set2i(const std::string& name, int v0, int v1);
	void set2f(const std::string& name, float v0, float v1);
	void set3f(const std::string& name, float v0, float v1, float v2);
	void set4f(const std::string& name, float v0, float v1, float v2, float v3);

	void setVec2(const std::string& name, const glm::vec2& vec);
	void setVec3(const std::string& name, const glm::vec3& vec);
	void setVec4(const std::string& name, const glm::vec4& vec);
	void setMat3(const std::string& name, const glm::mat3& mat);
	void setMat4(const std::string& name, const glm::mat4& mat);

	void setTexture(const std::string& name, TexturePtr tex, uint32_t slot);

	int getUniformLocation(const std::string& name);
	static int getUniformLocation(GLuint programID, const std::string& name);

	std::string name() const { return mName; }

	static ShaderPtr create(const File::path& path, const glm::ivec3& computeSize = glm::ivec3(1),
		const std::string& extensionStr = "");
	static ShaderPtr create(const std::string& name, const ShaderSource& source);

private:
	enum class ShaderLoadStat
	{
		None, Vertex, Fragment, Geometry, Compute
	};

private:
	void loadShader(std::fstream& file, ShaderSource& text, ShaderLoadStat stat, std::map<File::path, bool>& inclRec);
	void compileShader(const ShaderSource& text);
	void checkShaderCompileInfo(uint32_t shaderId, const std::string& name);
	void checkShaderLinkInfo();

	template<typename T>
	bool canAssignUniform(const std::string& name, const T& val)
	{
		auto itr = mUniformRec.find(name);
		if (itr == mUniformRec.end())
		{
			mUniformRec.insert({ name, ShaderUniform(val) });
			return true;
		}
		auto unif = itr->second;
		if (!(unif == val))
		{
			itr->second = val;
			return true;
		}
		return false;
	}

private:
	std::string mName;

	std::string mExtensionStr;
	glm::ivec3 mComputeGroupSize;

	std::map<std::string, ShaderUniform> mUniformRec;
	std::set<std::string> mMissingUniforms;
};