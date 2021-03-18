
#include "sdl/SDL.h"
#include "sdl/SDL_vulkan.h"

#include "skeleton/renderer/renderer.h"

#ifndef SKELETON_CORE_APPLICATION_H
#define SKELETON_CORE_APPLICATION_H 1

// Abstract class to handle project-independent boilerplate
// Bridge for all Game/Engine communication
class Application
{
  //////////////////////////////////////////////////////////////////////////
  // Variables
  //////////////////////////////////////////////////////////////////////////
protected:
  SDL_Window* window;
  bool appShouldClose = false;

  Renderer* renderer;
  // InputManager
  // AudioManager

  // TODO : Remove from Application.h
  // Model-View-Projection matrices for model-to-screenspace transformations
  struct MVPMatrices {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
  } mvp;

  //////////////////////////////////////////////////////////////////////////
  // Functions
  //////////////////////////////////////////////////////////////////////////
public:
  // Calls the application's Init, MainLoop, and Cleanup
  void Run();
  // NOTE : Add a force quit/restart?

protected:
  // (Pure) User defined function called once at the end of Init
  virtual void Start() = 0;
  // (Pure) User defined function called once per frame before rendering
  virtual void CoreLoop() = 0;

  // Creates and binds a renderable in the renderer
  void CreateObject(const char* _meshDirectory, uint32_t _shaderProgramIndex);
  // Loads an obj file and creates a renderable mesh
  mesh_t CreateMesh(const char* _directory);
  // Binds a renderable in the renderer
  //sklRenderable_t CreateRenderable(mesh_t _mesh, uint32_t _shaderIndex);

private:
  // Creates an SDL window and creates and initializes the renderer
  void Init();
  // Destroys all member components, frees all allocated memory
  void Cleanup();
  // Handles user input, time, and calls user defined CoreLoop function
  void MainLoop();

}; // class Application

#endif // !SKELETON_CORE_APPLICATION_H

