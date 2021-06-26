#define _CRT_SECURE_NO_WARNINGS

#include "Shader.h"

#include <fstream>
#include <sstream>
#include <cstring>
#include <string>

bool Shader::load(const char* filePath)
{
	m_Type = ShaderType::Graphics;
	ShaderText text;
	std::fstream file;
	
	std::map<std::string, bool> inclRec;
	inclRec[SHADER_DEFAULT_DIR + filePath] = true;
	file.open(SHADER_DEFAULT_DIR + filePath);
	if (!file.is_open()) return false;

	std::cout << "[Shader]\tLoading [" << SHADER_DEFAULT_DIR + filePath << "] ...\n";

	try
	{
		loadShader(file, text, ShaderLoadStat::None, inclRec);
	}
	catch (const char* e)
	{
		std::cout << "\n[Error] " << e << std::endl;
		return false;
	}

	compileShader(text);
	m_Name = std::string(filePath);
	return true;
}

Shader::Shader(const char* filePath)
{
	load(filePath);
}

Shader::~Shader()
{
	glDeleteProgram(m_ID);
}

void Shader::enable() const
{
	glUseProgram(m_ID);
}

void Shader::disable() const
{
	glUseProgram(0);
}

void Shader::set1i(const char* name, int v) const
{
	glProgramUniform1i(m_ID, getUniformLocation(name), v);
}

void Shader::set1f(const char* name, float v0) const
{
	glProgramUniform1f(m_ID, getUniformLocation(name), v0);
}

void Shader::set2i(const char* name, int v0, int v1) const
{
	glProgramUniform2i(m_ID, getUniformLocation(name), v0, v1);
}

void Shader::set2f(const char* name, float v0, float v1) const
{
	glProgramUniform2f(m_ID, getUniformLocation(name), v0, v1);
}

void Shader::set3f(const char* name, float v0, float v1, float v2) const
{
	glProgramUniform3f(m_ID, getUniformLocation(name), v0, v1, v2);
}

void Shader::set4f(const char* name, float v0, float v1, float v2, float v3) const
{
	glProgramUniform4f(m_ID, getUniformLocation(name), v0, v1, v2, v3);
}

void Shader::setVec3(const char* name, const glm::vec3& vec) const
{
	glProgramUniform3f(m_ID, getUniformLocation(name), vec.x, vec.y, vec.z);
}

void Shader::setVec4(const char* name, const glm::vec4& vec) const
{
	glProgramUniform4f(m_ID, getUniformLocation(name), vec.x, vec.y, vec.z, vec.w);
}

void Shader::setMat3(const char* name, const glm::mat3& mat) const
{
	glProgramUniformMatrix3fv(m_ID, getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::setMat4(const char* name, const glm::mat4& mat) const
{
	glProgramUniformMatrix4fv(m_ID, getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::setTexture(const char* name, const Texture& tex)
{
	std::string texName(name);
	std::map<std::string, int>::iterator it = m_TextureMap.find(texName);

	int texUnit = 0;
	if (it == m_TextureMap.end())
	{
		texUnit = m_TextureMap.size();
		m_TextureMap[texName] = texUnit;
	}
	
	glBindTextureUnit(texUnit, tex.ID());
	set1i(name, texUnit);
}

void Shader::setTexture(const char* name, const Texture& tex, uint32_t slot)
{
	glBindTextureUnit(slot, tex.ID());
	set1i(name, slot);
}

void Shader::setUniformBlock(const Buffer& buffer, int binding)
{
	glUniformBufferEXT(m_ID, binding, buffer.ID());
}

int Shader::getUniformLocation(const char* name) const
{
	GLint location = glGetUniformLocation(m_ID, name);
	if (location == -1)
		std::cout << "[Error]\t\tUnable to locate the uniform [" << name << "] in shader: " << m_Name << std::endl;
	return location;
}

GLint Shader::getUniformLocation(GLuint programID, const char* name)
{
	GLint location = glGetUniformLocation(programID, name);
	if (location == -1)
		std::cout << "[Error]\t\tUnable to locate uniform [" << name << "] in shader numbered: " << programID << std::endl;
	return location;
}

void Shader::resetTextureMap()
{
	m_TextureMap.clear();
}

void Shader::loadShader(std::fstream& file, ShaderText& text, ShaderLoadStat stat, std::map<std::string, bool>& inclRec)
{
	std::string line;
	while (std::getline(file, line))
	{
		if (line[0] == '=')
		{
			std::string macro, arg;
			std::stringstream ssline(line);
			ssline >> macro;
			if (macro == "=type")
			{
				ssline >> arg;
				if (arg == "vertex")
				{
					if (stat == ShaderLoadStat::Vertex) throw "redefinition";
					stat = ShaderLoadStat::Vertex;
				}
				else if (arg == "fragment")
				{
					if (stat == ShaderLoadStat::Fragment) throw "redefinition";
					stat = ShaderLoadStat::Fragment;
				}
				else if (arg == "geometry")
				{
					if (stat == ShaderLoadStat::Geometry) throw "redefinition";
				}
				else if (arg == "compute")
				{
					if (stat == ShaderLoadStat::Compute) throw "redefinition";
					stat = ShaderLoadStat::Compute;
					m_Type = ShaderType::Compute;
				}
				else if (arg != "lib") throw "included file is not a lib";
				continue;
			}
			else if (macro == "=include")
			{
				ssline >> arg;
				arg = SHADER_DEFAULT_DIR + arg;
				if (inclRec.find(arg) != inclRec.end()) continue;

				std::fstream file;
				file.open(arg);
				if (!file.is_open()) throw "included file not found";

				loadShader(file, text, stat, inclRec);
				inclRec[arg] = true;
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
	const char *shaderText[] = { text.vertex.c_str(), text.fragment.c_str(), text.geometry.c_str(), text.compute.c_str() };

	m_ID = glCreateProgram();

	for (int i = 0; i < 4; i++)
	{
		if (shaders[i]->length() == 0) continue;
		auto shader = glCreateShader(shaderType[i]);
		glShaderSource(shader, 1, &shaderText[i], nullptr);
		glCompileShader(shader);
		checkShaderCompileInfo(shader, shaderName[i]);
		glAttachShader(m_ID, shader);
		glDeleteShader(shader);
	}

	glLinkProgram(m_ID);
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
		std::cout << "\n[Shader]\t\t" << name << " compilation error\n" << info << "\n";
	}
}

void Shader::checkShaderLinkInfo()
{
	int linkSuccess;
	glGetProgramiv(m_ID, GL_LINK_STATUS, &linkSuccess);
	if (linkSuccess != GL_TRUE)
	{
		const int length = 8192;
		char info[length];
		glGetProgramInfoLog(m_ID, length, nullptr, info);
		std::cout << "\n[Shader]\t\tlink error\n" << info << "\n";
	}
}
