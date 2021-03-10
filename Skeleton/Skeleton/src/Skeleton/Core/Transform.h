#pragma once

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

struct Transform
{
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;

	glm::mat4 GetRotationMatrix()
	{
		return glm::mat4_cast(glm::radians(eulerAngles));
	}
};

