#pragma once

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

namespace skeleton
{
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
} // namespace skeleton

