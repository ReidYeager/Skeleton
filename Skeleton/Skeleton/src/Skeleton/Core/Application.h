#pragma once

#include "sdl/SDL.h"
#include "sdl/SDL_vulkan.h"

#include "Skeleton/Renderer/Renderer.h"

class Application
{
//////////////////////////////////////////////////////////////////////////
// Variables
//////////////////////////////////////////////////////////////////////////
protected:
	SDL_Window* window;
	Renderer* renderer;

	struct MVPMatrices {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	} mvp;
	bool stayOpen = true;


//////////////////////////////////////////////////////////////////////////
// Functions
//////////////////////////////////////////////////////////////////////////
public:
	void Init();
	void Run();
	void Cleanup();

protected:
	void MainLoop();
	void DefineShaders();
	virtual void CoreLoop() = 0;

	void CreateObject(
		const char* _meshDirectory,
		uint32_t _shaderIndex);
	mesh_t CreateMesh(
		const char* _directory);
	sklRenderable_t CreateRenderable(
		mesh_t _mesh,
		uint32_t _shaderIndex);

}; // Application

