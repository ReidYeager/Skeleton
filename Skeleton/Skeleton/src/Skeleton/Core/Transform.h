
#ifndef SKELETON_CORE_TRANSFORM_H
#define SKELETON_CORE_TRANSFORM_H 1

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

struct Transform
{
  glm::vec3 position;  // In Meters
  glm::vec3 rotation;  // In degrees
  glm::vec3 scale;     // Relative scale multiplier

  // Converts the rotation Euler angles to a quaternion
  glm::mat4 GetRotationMatrix()
  {
    return glm::mat4_cast(glm::radians(eulerAngles));
  }
};

#endif // !SKELETON_CORE_TRANSFORM_H

