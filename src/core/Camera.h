#pragma once

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const float CAMERA_ROTATE_SENSITIVITY = 0.4f;
const float CAMERA_MOVE_SENSITIVITY = 0.1f;
const float CAMERA_ROLL_SENSITIVITY = 0.05f;
const float CAMERA_FOV_SENSITIVITY = 150.0f;
const float CAMERA_PITCH_LIMIT = 88.0f;

class Camera
{
public:
	Camera(glm::vec3 pos = glm::vec3(0, 0, 0), glm::vec3 angle = glm::vec3(90.0f, 0.0f, 0.0f));

	void move(glm::vec3 vect);
	void move(GLuint key);
	void roll(float rolAngle);
	void rotate(glm::vec3 rotAngle);
	void changeFOV(float offset);
	void setFOV(float fov);
	void lookAt(glm::vec3 focus);
	void setDir(glm::vec3 dir);
	void setPos(glm::vec3 p);
	void setAngle(glm::vec3 angle);
	void setPlanes(float nearZ, float farZ);
	void setAspect(float asp);

	glm::vec3 pos() const;
	glm::vec3 front() const;
	glm::vec3 right() const;
	glm::vec3 up() const;
	float FOV() const;

	glm::mat4 viewMatrix() const;
	glm::mat4 viewMatrix(glm::vec3 focus) const;
	glm::mat4 projMatrix() const;

private:
	void update();

private:
	glm::vec3 m_Pos;
	glm::vec3 m_Angle;
	glm::vec3 m_Front;
	glm::vec3 m_Right;
	glm::vec3 m_Up = glm::vec3(0.0f, 0.0f, 1.0f);
	float m_FOV = 45.0f;
	float m_Near = 0.1f;
	float m_Far = 100.0f;
	float m_Aspect = 1.0f;
};