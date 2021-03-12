
#include "glm/glm.hpp"

#ifndef SKELETON_CORE_CAMERA_H
#define SKELETON_CORE_CAMERA_H

// Contains all information required for rendering within a worldspace
class Camera
{
  //=================================================
  // Variables
  //=================================================
public:
  float pitch = 0, yaw = 0;  // Pitch & Yaw in degrees
  glm::vec3 position;        // World position
  glm::mat4 projectionMatrix;

private:
  float minRenderDist = 0.1f;   // View frustum near cull distance
  float maxRenderDist = 10.0f;  // View frustum far cull distance
  float VFOV = 45.0f;           // Vertical Field Of View (degrees)
  glm::vec3 forward;

  //=================================================
  // Functions
  //=================================================
public:
  glm::mat4 GetViewMatrix()
  {
    forward.x = (float)(glm::cos(glm::radians(yaw)) * glm::cos(glm::radians(pitch)));
    forward.y = (float)glm::sin(glm::radians(pitch));
    forward.z = (float)(glm::sin(glm::radians(yaw)) * glm::cos(glm::radians(pitch)));

    return glm::lookAt(position, position + forward, { 0.f, 1.f, 0.f });
  }

  glm::mat4 UpdateProjection(float _screenRatio)
  {
    projectionMatrix = glm::perspective(glm::radians(VFOV), _screenRatio,
                                        minRenderDist, maxRenderDist);
    return projectionMatrix;
  }

  glm::vec3 GetForward() { return forward; }
  glm::vec3 GetRight() { return glm::normalize(glm::cross(forward, { 0.f, 1.f, 0.f })); }

}; // class Camera

#endif // !SKELETON_CORE_CAMERA_H

