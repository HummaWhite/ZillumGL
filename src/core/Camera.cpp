#include "Camera.h"

Camera::Camera(glm::vec3 pos, glm::vec3 angle) :
	mPos(pos), mAngle(angle)
{
	update();
}

void Camera::move(glm::vec3 vect)
{
	mPos += vect;
}

void Camera::move(GLuint key)
{
	switch (key)
	{
	case GLFW_KEY_W:
		mPos.x += SensitivityMove * glm::sin(glm::radians(mAngle.x));
		mPos.y += SensitivityMove * glm::cos(glm::radians(mAngle.x));
		break;
	case GLFW_KEY_S:
		mPos.x -= SensitivityMove * glm::sin(glm::radians(mAngle.x));
		mPos.y -= SensitivityMove * glm::cos(glm::radians(mAngle.x));
		break;
	case GLFW_KEY_A:
		mPos.y += SensitivityMove * glm::sin(glm::radians(mAngle.x));
		mPos.x -= SensitivityMove * glm::cos(glm::radians(mAngle.x));
		break;
	case GLFW_KEY_D:
		mPos.y -= SensitivityMove * glm::sin(glm::radians(mAngle.x));
		mPos.x += SensitivityMove * glm::cos(glm::radians(mAngle.x));
		break;
	case GLFW_KEY_Q:
		roll(-SensitivityRoll);
		break;
	case GLFW_KEY_E:
		roll(SensitivityRoll);
		break;
	case GLFW_KEY_R:
		mAngle.z = 0.0f;
		break;
	case GLFW_KEY_SPACE:
		mPos.z += SensitivityMove;
		break;
	case GLFW_KEY_LEFT_SHIFT:
		mPos.z -= SensitivityMove;
		break;
	}
	update();
}

void Camera::roll(float angle)
{
	mAngle.z += angle * SensitivityRoll;
	update();
}

void Camera::rotate(glm::vec3 angle)
{
	mAngle += angle * SensitivityRotate;
	if (mAngle.y > PitchLimit) mAngle.y = PitchLimit;
	if (mAngle.y < -PitchLimit) mAngle.y = -PitchLimit;
	update();
}

void Camera::changeFOV(float offset)
{
	mFOV -= glm::radians(offset) * SensitivityFOV;
	if (mFOV > 90.0f) mFOV = 90.0f;
	if (mFOV < 0.1f) mFOV = 0.1f;
}

void Camera::setFOV(float fov)
{
	mFOV = fov;
	if (mFOV > 90.0f) mFOV = 90.0f;
	if (mFOV < 0.1f) mFOV = 0.1f;
}

void Camera::lookAt(glm::vec3 pos)
{
	setDir(pos - mPos);
}

void Camera::setDir(glm::vec3 dir)
{
	dir = glm::normalize(dir);
	mAngle.y = glm::degrees(asin(dir.z / length(dir)));
	glm::vec2 dxy(dir);
	mAngle.x = glm::degrees(asin(dir.y / length(dxy)));
	if (dir.x < 0) mAngle.x = 180.0f - mAngle.x;
	update();
}

void Camera::setPos(glm::vec3 p)
{
	mPos = p;
}

void Camera::setAngle(glm::vec3 angle)
{
	mAngle = angle;
	update();
}

void Camera::setPlanes(float nearZ, float farZ)
{
	mNear = nearZ, mFar = farZ;
}

void Camera::setAspect(float asp)
{
	mAspect = asp;
}

glm::mat4 Camera::viewMatrix() const
{
	float x = glm::sin(glm::radians(mAngle.x)) * glm::cos(glm::radians(mAngle.y));
	float y = glm::cos(glm::radians(mAngle.x)) * glm::cos(glm::radians(mAngle.y));
	float z = glm::sin(glm::radians(mAngle.y));
	glm::vec3 lookingAt = mPos + glm::vec3(x, y, z);
	glm::mat4 view = glm::lookAt(mPos, lookingAt, mUp);
	return view;
}

glm::mat4 Camera::viewMatrix(glm::vec3 focus) const
{
	glm::mat4 view = glm::lookAt(mPos, focus, mUp);
	return view;
}

glm::mat4 Camera::projMatrix() const
{
	glm::mat4 proj = glm::perspective(glm::radians(mFOV), mAspect, mNear, mFar);
	return proj;
}

void Camera::update()
{
	float x = glm::sin(glm::radians(mAngle.x)) * glm::cos(glm::radians(mAngle.y));
	float y = glm::cos(glm::radians(mAngle.x)) * glm::cos(glm::radians(mAngle.y));
	float z = glm::sin(glm::radians(mAngle.y));

	mFront = glm::normalize(glm::vec3(x, y, z));
	const glm::vec3 u(0.0f, 0.0f, 1.0f);
	mRight = glm::normalize(glm::cross(mFront, u));

	auto rotMatrix = glm::rotate(glm::mat4(1.0f), mAngle.z, mFront);
	mRight = glm::normalize(glm::vec3(rotMatrix * glm::vec4(mRight, 1.0f)));
	mUp = glm::normalize(glm::cross(mRight, mFront));
}
