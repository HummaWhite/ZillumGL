#define _CRT_SECURE_NO_WARNINGS

#include "Shader.h"
#include "../util/Error.h"

#include <fstream>
#include <sstream>
#include <cstring>
#include <string>

const File::path ShaderDefaultDir = File::absolute(File::path("res/shader"));

Shader::Shader(const File::path& path, const glm::ivec3& computeSize, const std::string& extensionStr) :
	mType(ShaderType::Graphics), mName(path.generic_string()), mExtensionStr(extensionStr),
	mComputeGroupSize(computeSize), GLStateObject(GLStateObjectType::Shader)
{
	ShaderText text;
	std::fstream file;

	std::map<File::path, bool> inclRec;
	auto fullPath = ShaderDefaultDir / path;
	inclRec[fullPath] = true;

	file.open(fullPath);
	Error::check(file.is_open(), "[Shader]\tunable to load file");
	Error::log("Shader", "loading [" + fullPath.generic_string() + "] ...");

	loadShader(file, text, ShaderLoadStat::None, inclRec);

	compileShader(text);
}

Shader::~Shader()
{
	glDeleteProgram(mId);
}

void Shader::enable() const
{
	glUseProgram(mId);
}

void Shader::disable() const
{
	glUseProgram(0);
}

void Shader::set1i(const char* name, int v) const
{
	glProgramUniform1i(mId, getUniformLocation(name), v);
}

void Shader::set1f(const char* name, float v0) const
{
	glProgramUniform1f(mId, getUniformLocation(name), v0);
}

void Shader::set2i(const char* name, int v0, int v1) const
{
	glProgramUniform2i(mId, getUniformLocation(name), v0, v1);
}

void Shader::set2f(const char* name, float v0, float v1) const
{
	glProgramUniform2f(mId, getUniformLocation(name), v0, v1);
}

void Shader::set3f(const char* name, float v0, float v1, float v2) const
{
	glProgramUniform3f(mId, getUniformLocation(name), v0, v1, v2);
}

void Shader::set4f(const char* name, float v0, float v1, float v2, float v3) const
{
	glProgramUniform4f(mId, getUniformLocation(name), v0, v1, v2, v3);
}

void Shader::setVec2(const char* name, const glm::vec2& vec) const
{
	glProgramUniform2f(mId, getUniformLocation(name), vec.x, vec.y);
}

void Shader::setVec3(const char* name, const glm::vec3& vec) const
{
	glProgramUniform3f(mId, getUniformLocation(name), vec.x, vec.y, vec.z);
}

void Shader::setVec4(const char* name, const glm::vec4& vec) const
{
	glProgramUniform4f(mId, getUniformLocation(name), vec.x, vec.y, vec.z, vec.w);
}

void Shader::setMat3(const char* name, const glm::mat3& mat) const
{
	glProgramUniformMatrix3fv(mId, getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::setMat4(const char* name, const glm::mat4& mat) const
{
	glProgramUniformMatrix4fv(mId, getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::setTexture(const char* name, TexturePtr tex, uint32_t slot)
{
	glBindTextureUnit(slot, tex->id());
	set1i(name, slot);
}

int Shader::getUniformLocation(const char* name) const
{
	GLint location = glGetUniformLocation(mId, name);
	if (location == -1)
		std::cout << "[Error]\t\tUnable to locate the uniform [" << name << "] in shader: " << mName << std::endl;
	return location;
}

GLint Shader::getUniformLocation(GLuint programID, const char* name)
{
	GLint location = glGetUniformLocation(programID, name);
	if (location == -1)
		std::cout << "[Error]\t\tUnable to locate uniform [" << name << "] in shader numbered: " << programID << std::endl;
	return location;
}

ShaderPtr Shader::create(const File::path& path, const glm::ivec3& computeSize, const std::string& extensionStr)
{
	return std::make_shared<Shader>(path, computeSize, extensionStr);
}

void Shader::loadShader(std::fstream& file, ShaderText& text, ShaderLoadStat stat,
	std::map<File::path, bool>& inclRec)
{
	std::string line;
	while (std::getline(file, line))
	{
		if (line[0] == '@')
		{
			std::string macro, arg;
			std::stringstream ssline(line);
			ssline >> macro;
			if (macro == "@type")
			{
				ssline >> arg;
				if (arg == "vertex")
				{
					Error::check(stat != ShaderLoadStat::Vertex, "[Shader] redef");
					stat = ShaderLoadStat::Vertex;
					text.vertex += "\n#version 450\n" + mExtensionStr;
				}
				else if (arg == "fragment")
				{
					Error::check(stat != ShaderLoadStat::Fragment, "[Shader] redef");
					stat = ShaderLoadStat::Fragment;
					text.fragment += "\n#version 450\n" + mExtensionStr;
				}
				else if (arg == "geometry")
				{
					Error::check(stat != ShaderLoadStat::Geometry, "[Shader] redef");
					stat = ShaderLoadStat::Geometry;
					text.geometry += "\n#version 450\n" + mExtensionStr;
				}
				else if (arg == "compute")
				{
					Error::check(stat != ShaderLoadStat::Compute, "[Shader] redef");
					stat = ShaderLoadStat::Compute;
					mType = ShaderType::Compute;
					text.compute += "\n#version 450\n" + mExtensionStr +
						"layout(local_size_x = " + std::to_string(mComputeGroupSize.x) +
						", local_size_y = " + std::to_string(mComputeGroupSize.y) +
						", local_size_z = " + std::to_string(mComputeGroupSize.z) +
						") in;\n";
				}
				else if (arg != "lib") throw "included file is not a lib";
				continue;
			}
			else if (macro == "@include")
			{
				ssline >> arg;
				File::path incPath = ShaderDefaultDir / File::path(arg);
				if (inclRec.find(incPath) != inclRec.end()) continue;

				std::fstream file;
				file.open(incPath);
				Error::check(file.is_open(), "[Shader] included file not found");

				loadShader(file, text, stat, inclRec);
				inclRec[incPath] = true;
				continue;
			}
		}

		switch (stat)
		{
		case ShaderLoadStat::Vertex:
			text.vertex += line + '\n';
			break;
		case ShaderLoadStat::Fragment:
			text.fragment += line + '\n';
			break;
		case ShaderLoadStat::Geometry:
			text.geometry += line + '\n';
			break;
		case ShaderLoadStat::Compute:
			text.compute += line + '\n';
			break;
		}
	}
}

void Shader::compileShader(const ShaderText& text)
{
	int shaderType[] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER, GL_COMPUTE_SHADER };
	std::string shaderName[] = { "Vertex", "Fragment", "Geometry", "Compute" };
	const std::string* shaders[] = { &text.vertex, &text.fragment, &text.geometry, &text.compute };
	const char* shaderText[] = { text.vertex.c_str(), text.fragment.c_str(), text.geometry.c_str(), text.compute.c_str() };

	mId = glCreateProgram();

	for (int i = 0; i < 4; i++)
	{
		if (shaders[i]->length() == 0) continue;
		auto shader = glCreateShader(shaderType[i]);
		glShaderSource(shader, 1, &shaderText[i], nullptr);
		glCompileShader(shader);
		checkShaderCompileInfo(shader, shaderName[i]);
		glAttachShader(mId, shader);
		glDeleteShader(shader);
	}

	glLinkProgram(mId);
	checkShaderLinkInfo();
}

void Shader::checkShaderCompileInfo(uint32_t shaderId, const std::string& name)
{
	int param;
	glGetShaderiv(shaderId, GL_COMPILE_STATUS, &param);
	if (param != GL_TRUE)
	{
		const int length = 8192;
		char info[length];
		glGetShaderInfoLog(shaderId, length, nullptr, info);
		Error::log("Shader", std::string(info));
	}
}

void Shader::checkShaderLinkInfo()
{
	int linkSuccess;
	glGetProgramiv(mId, GL_LINK_STATUS, &linkSuccess);
	if (linkSuccess != GL_TRUE)
	{
		const int length = 8192;
		char info[length];
		glGetProgramInfoLog(mId, length, nullptr, info);
		Error::log("Shader", std::string(info));
	}
}
