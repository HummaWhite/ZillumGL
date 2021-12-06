#pragma once

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Camera
{
public:
	constexpr static float SensitivityRotate = 0.4f;
	constexpr static float SensitivityMove = 0.1f;
	constexpr static float SensitivityRoll = 0.2f;
	constexpr static float SensitivityFOV = 150.0f;
	constexpr static float PitchLimit = 88.0f;

public:
	friend class SVGF;
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

	glm::vec3 pos() const { return mPos; }
	glm::vec3 front() const { return mFront; }
	glm::vec3 right() const { return mRight; }
	glm::vec3 up() const { return mUp; }
	float FOV() const { return mFOV; }
	float aspect() const { return mAspect; }

	glm::mat4 viewMatrix() const;
	glm::mat4 viewMatrix(glm::vec3 focus) const;
	glm::mat4 projMatrix() const;

private:
	void update();

private:
	glm::vec3 mPos;
	glm::vec3 mAngle;
	glm::vec3 mFront;
	glm::vec3 mRight;
	glm::vec3 mUp = glm::vec3(0.0f, 0.0f, 1.0f);
	float mFOV = 45.0f;
	float mNear = 0.1f;
	float mFar = 100.0f;
	float mAspect = 1.0f;
};
