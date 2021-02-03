#pragma once

#include "glm/glm.hpp"

namespace skeleton
{
class Camera
{
//=================================================
// Variables
//=================================================
private:
	float minRenderDist = 0.1f;
	float maxRenderDist = 10.f;
	float VFOV = 45.0f; // Vertical Field Of View
	glm::vec3 forward;

public:
	float pitch = 0, yaw = 0;
	glm::vec3 position;
	glm::mat4 projectionMatrix;

//=================================================
// Functions
//=================================================
public:
	glm::mat4 GetViewMatrix()
	{
		forward.x = (float)(glm::cos(glm::radians(yaw)) * glm::cos(glm::radians(pitch)));
		forward.y = (float)glm::sin(glm::radians(pitch));
		forward.z = (float)(glm::sin(glm::radians(yaw)) * glm::cos(glm::radians(pitch)));

		return glm::lookAt(position, position + forward, {0.f, 1.f, 0.f});
	}

	glm::mat4 UpdateProjection(float _screenRatio)
	{
		projectionMatrix = glm::perspective(glm::radians(VFOV), _screenRatio, minRenderDist, maxRenderDist);
		return projectionMatrix;
	}

	glm::vec3 GetForward() { return forward; }
	glm::vec3 GetRight() { return glm::normalize(glm::cross(forward, {0.f, 1.f, 0.f})); }

}; // Camera
} // namespace Skeleton

