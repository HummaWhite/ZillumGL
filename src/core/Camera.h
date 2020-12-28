#pragma once

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const float CAMERA_ROTATE_SENSITIVITY = 0.05f;
const float CAMERA_MOVE_SENSITIVITY = 0.1f;
const float CAMERA_ROLL_SENSITIVITY = 0.05f;
const float CAMERA_FOV_SENSITIVITY = 150.0f;
const float CAMERA_PITCH_LIMIT = 88.0f;
const float CAMERA_DEFAULT_FOV = 45.0f;
const float CAMERA_DEFAULT_NEAR = 0.1f;
const float CAMERA_DEFAULT_FAR = 100.0f;
const glm::vec3 CAMERA_VEC_UP = glm::vec3(0.0f, 0.0f, 1.0f);

class Camera
{
public:
	Camera(glm::vec3 pos = glm::vec3(0, 0, 0), glm::vec3 angle = glm::vec3(90.0f, 0.0f, 0.0f))
		:m_Pos(pos), m_Angle(glm::radians(angle)), m_FOV(CAMERA_DEFAULT_FOV), m_CameraUp(CAMERA_VEC_UP),
		m_Near(CAMERA_DEFAULT_NEAR), m_Far(CAMERA_DEFAULT_FAR) {}

	void move(glm::vec3 vect);
	void move(GLuint key);
	void roll(float angle);
	void rotate(glm::vec3 angle);
	void changeFOV(float offset);
	void setFOV(float fov);
	void lookAt(glm::vec3 pos);
	void setPos(glm::vec3 pos) { m_Pos = pos; }
	void setAngle(glm::vec3 angle) { m_Angle = glm::radians(angle); }
	void setDir(glm::vec3 dir);
	void setPlanes(float near, float far) { m_Near = near, m_Far = far; }

	float FOV() const { return m_FOV; }
	float nearPlane() const { return m_Near; }
	float farPlane() const { return m_Far; }
	glm::vec3 pos() const { return m_Pos; }
	glm::vec3 angle() const { return m_Angle; }
	glm::vec3 pointing() const;
	glm::mat4 viewMatrix() const;
	glm::mat4 viewMatrix(glm::vec3 focus) const;
	glm::mat4 projMatrix(int width, int height) const;

private:
	glm::vec3 m_Pos;
	glm::vec3 m_Angle;
	glm::vec3 m_Pointing;
	glm::vec3 m_CameraUp;
	float m_FOV;
	float m_Near;
	float m_Far;
};