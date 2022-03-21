#define _CRT_SECURE_NO_WARNINGS

#include "Shader.h"
#include "../util/Error.h"

#include <fstream>
#include <sstream>
#include <cstring>
#include <string>

const File::path ShaderDefaultDir = File::absolute(File::path("src/shader"));

Shader::Shader(const File::path& path, const glm::ivec3& computeSize, const std::string& extensionStr) :
	mName(path.generic_string()), mExtensionStr(extensionStr),
	mComputeGroupSize(computeSize), GLStateObject(GLStateObjectType::Shader)
{
	ShaderSource source;
	std::fstream file;

	std::map<File::path, bool> inclRec;
	auto fullPath = ShaderDefaultDir / path;
	inclRec[fullPath] = true;

	file.open(fullPath);
	Error::line("[Shader " + fullPath.generic_string() + "]");
	Error::check(file.is_open(), "\t[Error unable to load file]");

	loadShader(file, source, ShaderLoadStat::None, inclRec);

	compileShader(source);
}

Shader::Shader(const std::string& name, const ShaderSource& source) :
	mName(name), GLStateObject(GLStateObjectType::Shader)
{
	Error::line("[Shader " + name + "]");
	compileShader(source);
}

Shader::~Shader()
{
	glDeleteProgram(mId);
}

void Shader::enable()
{
	glUseProgram(mId);
}

void Shader::disable()
{
	glUseProgram(0);
}

void Shader::set1i(const std::string& name, int v)
{
	if (canAssignUniform(name, v))
		glProgramUniform1i(mId, getUniformLocation(name), v);
}

void Shader::set1f(const std::string& name, float v0)
{
	if (canAssignUniform(name, v0))
		glProgramUniform1f(mId, getUniformLocation(name), v0);
}

void Shader::set2i(const std::string& name, int v0, int v1)
{
	if (canAssignUniform(name, glm::ivec2(v0, v1)))
		glProgramUniform2i(mId, getUniformLocation(name), v0, v1);
}

void Shader::set2f(const std::string& name, float v0, float v1)
{
	if (canAssignUniform(name, glm::vec2(v0, v1)))
		glProgramUniform2f(mId, getUniformLocation(name), v0, v1);
}

void Shader::set3f(const std::string& name, float v0, float v1, float v2)
{
	if (canAssignUniform(name, glm::vec3(v0, v1, v2)))
		glProgramUniform3f(mId, getUniformLocation(name), v0, v1, v2);
}

void Shader::set4f(const std::string& name, float v0, float v1, float v2, float v3)
{
	if (canAssignUniform(name, glm::vec4(v0, v1, v2, v3)))
		glProgramUniform4f(mId, getUniformLocation(name), v0, v1, v2, v3);
}

void Shader::setVec2(const std::string& name, const glm::vec2& vec)
{
	if (canAssignUniform(name, vec))
		glProgramUniform2f(mId, getUniformLocation(name), vec.x, vec.y);
}

void Shader::setVec3(const std::string& name, const glm::vec3& vec)
{
	if (canAssignUniform(name, vec))
		glProgramUniform3f(mId, getUniformLocation(name), vec.x, vec.y, vec.z);
}

void Shader::setVec4(const std::string& name, const glm::vec4& vec)
{
	if (canAssignUniform(name, vec))
		glProgramUniform4f(mId, getUniformLocation(name), vec.x, vec.y, vec.z, vec.w);
}

void Shader::setMat3(const std::string& name, const glm::mat3& mat)
{
	if (canAssignUniform(name, mat))
		glProgramUniformMatrix3fv(mId, getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat)
{
	if (canAssignUniform(name, mat))
		glProgramUniformMatrix4fv(mId, getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::setTexture(const std::string& name, TexturePtr tex, uint32_t slot)
{
	if (canAssignUniform(name, std::tuple<uint32_t, uint32_t>(tex->id(), slot)))
	{
		glBindTextureUnit(slot, tex->id());
		set1i(name, slot);
	}
}

int Shader::getUniformLocation(const std::string& name)
{
	GLint location = glGetUniformLocation(mId, name.c_str());
	if (location == -1)
	{
		if (mMissingUniforms.find(name) == mMissingUniforms.end())
		{
			std::cout << "[Unable to locate the uniform \"" << name << "\" in shader: " <<
				mName << "]" << std::endl;
			mMissingUniforms.insert(name);
		}
	}
	return location;
}

int Shader::getUniformLocation(GLuint programID, const std::string& name)
{
	return glGetUniformLocation(programID, name.c_str());
}

ShaderPtr Shader::create(const File::path& path, const glm::ivec3& computeSize, const std::string& extensionStr)
{
	return std::make_shared<Shader>(path, computeSize, extensionStr);
}

ShaderPtr Shader::create(const std::string& name, const ShaderSource& source)
{
	return std::make_shared<Shader>(name, source);
}

void Shader::loadShader(std::fstream& file, ShaderSource& text, ShaderLoadStat stat,
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
					Error::check(stat != ShaderLoadStat::Vertex, "Redefinition");
					stat = ShaderLoadStat::Vertex;
					text.vertex += "\n#version 450\n" + mExtensionStr;
				}
				else if (arg == "fragment")
				{
					Error::check(stat != ShaderLoadStat::Fragment, "Redefinition");
					stat = ShaderLoadStat::Fragment;
					text.fragment += "\n#version 450\n" + mExtensionStr;
				}
				else if (arg == "geometry")
				{
					Error::check(stat != ShaderLoadStat::Geometry, "Redefinition");
					stat = ShaderLoadStat::Geometry;
					text.geometry += "\n#version 450\n" + mExtensionStr;
				}
				else if (arg == "compute")
				{
					Error::check(stat != ShaderLoadStat::Compute, "Redefinition");
					stat = ShaderLoadStat::Compute;
					text.compute += "\n#version 450\n" + mExtensionStr +
						"layout(local_size_x = " + std::to_string(mComputeGroupSize.x) +
						", local_size_y = " + std::to_string(mComputeGroupSize.y) +
						", local_size_z = " + std::to_string(mComputeGroupSize.z) +
						") in;\n";
				}
				else
					Error::check(arg == "lib", "Included file is not a lib");
				continue;
			}
			else if (macro == "@include")
			{
				ssline >> arg;
				File::path incPath = ShaderDefaultDir / File::path(arg);
				if (inclRec.find(incPath) != inclRec.end()) continue;

				Error::bracketLine<1>("Included " + incPath.generic_string());

				std::fstream file;
				file.open(incPath);
				Error::check(file.is_open(), "Included file not found " + incPath.generic_string());
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

void Shader::compileShader(const ShaderSource& text)
{
	int shaderType[] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER, GL_COMPUTE_SHADER };
	std::string shaderName[] = { "Vertex", "Fragment", "Geometry", "Compute" };
	const std::string* shaders[] = { &text.vertex, &text.fragment, &text.geometry, &text.compute };
	const char* shaderText[] = { text.vertex.c_str(), text.fragment.c_str(), text.geometry.c_str(), text.compute.c_str() };

	mId = glCreateProgram();

	std::string tempPath = "temp/" + mName;
	if (text.vertex != "")
	{
		std::ofstream txt(ShaderDefaultDir / (tempPath + "_vs.glsl"));
		txt << text.vertex;
	}
	if (text.fragment != "")
	{
		std::ofstream txt(ShaderDefaultDir / (tempPath + "_fs.glsl"));
		txt << text.fragment;
	}
	if (text.geometry != "")
	{
		std::ofstream txt(ShaderDefaultDir / (tempPath + "_gs.glsl"));
		txt << text.geometry;
	}
	if (text.compute != "")
	{
		std::ofstream txt(ShaderDefaultDir / (tempPath + "_cs.glsl"));
		txt << text.compute;
	}

	for (int i = 0; i < 4; i++)
	{
		if (shaders[i]->length() == 0) continue;
		auto shader = glCreateShader(shaderType[i]);
		glShaderSource(shader, 1, &shaderText[i], nullptr);
		Error::line("\t[Compiling " + shaderName[i] + " shader]");
		glCompileShader(shader);
		checkShaderCompileInfo(shader, shaderName[i]);
		glAttachShader(mId, shader);
		glDeleteShader(shader);
	}
	Error::line("\t[Linking shaders]");
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
		Error::bracketLine<0>("Shader " + std::string(info));
		Error::exit("failed to compile shader");
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
		Error::bracketLine<0>("Shader " + std::string(info));
		Error::exit("failed to compile shader");
	}
}
