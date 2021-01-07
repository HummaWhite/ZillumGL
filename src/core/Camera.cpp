#include "Camera.h"

Camera::Camera(glm::vec3 pos, glm::vec3 angle):
	m_Pos(pos), m_Angle(angle)
{
	update();
}

void Camera::move(glm::vec3 vect)
{
	m_Pos += vect;
}

void Camera::move(GLuint key)
{
	switch (key)
	{
	case GLFW_KEY_W:
		m_Pos.x += CAMERA_MOVE_SENSITIVITY * glm::cos(glm::radians(m_Angle.x));
		m_Pos.y += CAMERA_MOVE_SENSITIVITY * glm::sin(glm::radians(m_Angle.x));
		break;
	case GLFW_KEY_S:
		m_Pos.x -= CAMERA_MOVE_SENSITIVITY * glm::cos(glm::radians(m_Angle.x));
		m_Pos.y -= CAMERA_MOVE_SENSITIVITY * glm::sin(glm::radians(m_Angle.x));
		break;
	case GLFW_KEY_A:
		m_Pos.y += CAMERA_MOVE_SENSITIVITY * glm::cos(glm::radians(m_Angle.x));
		m_Pos.x -= CAMERA_MOVE_SENSITIVITY * glm::sin(glm::radians(m_Angle.x));
		break;
	case GLFW_KEY_D:
		m_Pos.y -= CAMERA_MOVE_SENSITIVITY * glm::cos(glm::radians(m_Angle.x));
		m_Pos.x += CAMERA_MOVE_SENSITIVITY * glm::sin(glm::radians(m_Angle.x));
		break;
	case GLFW_KEY_Q:
		roll(-CAMERA_ROLL_SENSITIVITY);
		break;
	case GLFW_KEY_E:
		roll(CAMERA_ROLL_SENSITIVITY);
		break;
	case GLFW_KEY_R:
		m_Up = glm::vec3(0.0f, 0.0f, 1.0f);
		break;
	case GLFW_KEY_SPACE:
		m_Pos.z += CAMERA_MOVE_SENSITIVITY;
		break;
	case GLFW_KEY_LEFT_SHIFT:
		m_Pos.z -= CAMERA_MOVE_SENSITIVITY;
		break;
	}
	update();
}

void Camera::roll(float angle)
{
	glm::mat4 mul(1.0f);
	mul = glm::rotate(mul, glm::radians(angle), m_Front);
	glm::vec4 tmp(m_Up.x, m_Up.y, m_Up.z, 1.0f);
	tmp = mul * tmp;
	m_Up = glm::vec3(tmp);
	update();
}

void Camera::rotate(glm::vec3 angle)
{
	m_Angle += angle * CAMERA_ROTATE_SENSITIVITY;
	if (m_Angle.y > CAMERA_PITCH_LIMIT) m_Angle.y = CAMERA_PITCH_LIMIT;
	if (m_Angle.y < -CAMERA_PITCH_LIMIT) m_Angle.y = -CAMERA_PITCH_LIMIT;
	update();
}

void Camera::changeFOV(float offset)
{
	m_FOV -= glm::radians(offset) * CAMERA_FOV_SENSITIVITY;
	if (m_FOV > 90.0) m_FOV = 90.0;
	if (m_FOV < 15.0) m_FOV = 15.0;
}

void Camera::setFOV(float fov)
{
	m_FOV = fov;
	if (m_FOV > 90.0f) m_FOV = 90.0f;
	if (m_FOV < 15.0f) m_FOV = 15.0f;
}

void Camera::lookAt(glm::vec3 pos)
{
	setDir(pos - m_Pos);
}

void Camera::setDir(glm::vec3 dir)
{
	dir = glm::normalize(dir);
	m_Angle.y = glm::degrees(asin(dir.z / length(dir)));
	glm::vec2 dxy(dir);
	m_Angle.x = glm::degrees(asin(dir.y / length(dxy)));
	if (dir.x < 0) m_Angle.x = 180.0f - m_Angle.x;
	update();
}

void Camera::setPos(glm::vec3 p)
{
	m_Pos = p;
}

void Camera::setAngle(glm::vec3 angle)
{
	m_Angle = angle;
	update();
}

void Camera::setPlanes(float nearZ, float farZ)
{
	m_Near = nearZ, m_Far = farZ;
}

void Camera::setAspect(float asp)
{
	m_Aspect = asp;
}

glm::vec3 Camera::pos() const
{
	return m_Pos;
}

glm::vec3 Camera::front() const
{
	return m_Front;
}

glm::vec3 Camera::right() const
{
	return m_Right;
}

glm::vec3 Camera::up() const
{
	return m_Up;
}

float Camera::FOV() const
{
	return m_FOV;
}

glm::mat4 Camera::viewMatrix() const
{
	float aX = cos(glm::radians(m_Angle.y)) * cos(glm::radians(m_Angle.x));
	float aY = cos(glm::radians(m_Angle.y)) * sin(glm::radians(m_Angle.x));
	float aZ = sin(glm::radians(m_Angle.y));
	glm::vec3 lookingAt = m_Pos + glm::vec3(aX, aY, aZ);
	glm::mat4 view = glm::lookAt(m_Pos, lookingAt, m_Up);
	return view;
}

glm::mat4 Camera::viewMatrix(glm::vec3 focus) const
{
	glm::mat4 view = glm::lookAt(m_Pos, focus, m_Up);
	return view;
}

glm::mat4 Camera::projMatrix() const
{
	glm::mat4 proj = glm::perspective(glm::radians(m_FOV), m_Aspect, m_Near, m_Far);
	return proj;
}

void Camera::update()
{
	float aX = glm::cos(glm::radians(m_Angle.y)) * glm::cos(glm::radians(m_Angle.x));
	float aY = glm::cos(glm::radians(m_Angle.y)) * glm::sin(glm::radians(m_Angle.x));
	float aZ = glm::sin(glm::radians(m_Angle.y));
	m_Front = glm::normalize(glm::vec3(aX, aY, aZ));
	glm::vec3 u(0.0f, 0.0f, 1.0f);
	m_Right = glm::normalize(glm::cross(m_Front, u));
	m_Up = glm::normalize(glm::cross(m_Right, m_Front));
}
