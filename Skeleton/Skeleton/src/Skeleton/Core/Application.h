#pragma once

#include "sdl/SDL.h"
#include "sdl/SDL_vulkan.h"

#include "Skeleton/Renderer/Renderer.h"

namespace skeleton
{
class Application
{
//////////////////////////////////////////////////////////////////////////
// Variables
//////////////////////////////////////////////////////////////////////////
protected:
	SDL_Window* window;
	skeleton::Renderer* renderer;

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

private:
	void CoreLoop();

}; // Application
} // namespace skeleton

