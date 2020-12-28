#pragma once

#include <iostream>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const int NORMAL_ATTENUATION_LEVEL = 4;
const glm::vec3 ATTENUATION[] =
{
	glm::vec3(1, 0.7, 1.8),        //7
	glm::vec3(1, 0.35, 0.44),      //13
	glm::vec3(1, 0.22, 0.2),       //20
	glm::vec3(1, 0.14, 0.07),      //32
	glm::vec3(1, 0.09, 0.032),     //50
	glm::vec3(1, 0.07, 0.017),     //65
	glm::vec3(1, 0.045, 0.0075),   //100
	glm::vec3(1, 0.027, 0.0028),   //160
	glm::vec3(1, 0.022, 0.0019),   //200
	glm::vec3(1, 0.014, 0.0007),   //325
	glm::vec3(1, 0.007, 0.0002),   //600
	glm::vec3(1, 0.0014, 7e-006)   //3250
};
const glm::vec3 PHYSICAL_ATTEN(0.0f, 0.0f, 1.0f);

struct Light
{
public:
	enum
	{
		DIRECTIONAL,
		POINT,
	};
	glm::vec3 pos;
	glm::vec3 dir;
	glm::vec3 color;
	glm::vec3 attenuation;
	float cutoff;
	float outerCutoff;
	float strength;
	float size;
	GLuint type;
	Light(int _type, glm::vec3 _dir, glm::vec3 _color) :
		type(DIRECTIONAL), dir(_dir), color(_color) {}
	Light(glm::vec3 _pos, glm::vec3 _color, glm::vec3 _dir = glm::vec3(0.0f, 0.0f, -1.0f), float _cutOff = 180.0f, float _outerCutOff = 180.0f, GLuint attenLevel = -1) :
		type(POINT), pos(_pos), dir(_dir), color(_color), cutoff(_cutOff),
		outerCutoff(_outerCutOff), strength(4.0f), size(0.3f)
	{
		setAttenuationLevel(attenLevel);
	}
	void setAttenuationLevel(int level)
	{
		if (level > 11) return;
		if (level == -1)
		{
			attenuation = PHYSICAL_ATTEN;
			return;
		}
		attenuation = ATTENUATION[level];
	}
};