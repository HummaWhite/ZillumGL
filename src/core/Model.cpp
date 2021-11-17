#include "Model.h"
#include "../util/Error.h"

#include <filesystem>

const glm::mat4 ConstRot = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

ModelInstance::~ModelInstance()
{
}

void ModelInstance::setPos(glm::vec3 pos)
{
	mPos = pos;
}

void ModelInstance::setPos(float x, float y, float z)
{
	setPos(glm::vec3(x, y, z));
}

void ModelInstance::move(glm::vec3 vec)
{
	mPos += vec;
}

void ModelInstance::rotateObjectSpace(float angle, glm::vec3 axis)
{
	mRotMatrix = glm::rotate(mRotMatrix, glm::radians(angle), axis);
}

void ModelInstance::rotateWorldSpace(float angle, glm::vec3 axis)
{
	mRotMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(angle), axis) * mRotMatrix;
}

void ModelInstance::setScale(glm::vec3 scale)
{
	mScale = scale;
}

void ModelInstance::setScale(float xScale, float yScale, float zScale)
{
	mScale = glm::vec3(xScale, yScale, zScale);
}

void ModelInstance::setRotation(glm::vec3 angle)
{
	mRotation = angle;
}

void ModelInstance::setRotation(float yaw, float pitch, float roll)
{
	mRotation = glm::vec3(yaw, pitch, roll);
}

void ModelInstance::setSize(float size)
{
	mScale = glm::vec3(size);
}

glm::mat4 ModelInstance::modelMatrix() const
{
	glm::mat4 model(1.0f);
	model = glm::translate(model, mPos);
	model = glm::scale(model, mScale);
	model = model * ConstRot;
	model = glm::rotate(model, glm::radians(mRotation.x), glm::vec3(0.0f, 0.0f, 1.0f));
	model = glm::rotate(model, glm::radians(mRotation.y), glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, glm::radians(mRotation.z), glm::vec3(0.0f, 1.0f, 0.0f));
	return model;
}

ModelInstancePtr ModelInstance::copy() const
{
	auto model = std::make_shared<ModelInstance>();
	*model = *this;
	model->mName = mName + "\'";
	return model;
}
