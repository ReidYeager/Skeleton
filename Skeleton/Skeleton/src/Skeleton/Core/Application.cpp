
#include "pch.h"
#include <stdio.h>
#include <chrono>

#include "Skeleton/Core/Common.h"
#include "Skeleton/Core/Application.h"
#include "Skeleton/Core/FileSystem.h"

void skeleton::Application::Init()
{
	SKL_PRINT("Application", "Init =================================================");
	// Create window
	//=================================================
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		SKL_LOG("SDL ERROR", "%s", SDL_GetError());
		throw "SDL failure";
	}

	uint32_t sdlFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_RESIZABLE;
	window = SDL_CreateWindow(
		"Skeleton Application",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		800,
		600,
		sdlFlags);
	SDL_SetRelativeMouseMode(SDL_TRUE);
	SDL_SetWindowGrab(window, SDL_TRUE);

	// Fill instance extensions & layers
	uint32_t sdlExtensionCount;
	SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, nullptr);
	std::vector<const char*> sdlExtensions(sdlExtensionCount);
	SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, sdlExtensions.data());

	// Create Renderer
	//=================================================
	renderer = new Renderer(sdlExtensions, window);
}

void skeleton::Application::Run()
{
	SKL_PRINT("Application", "Run =================================================");
	MainLoop();
}

void skeleton::Application::Cleanup()
{
	SKL_PRINT("Application", "Cleanup =================================================");
	delete(renderer);
	SDL_DestroyWindow(window);
}

void skeleton::Application::MainLoop()
{
	static SDL_Event e;
	auto startTime = std::chrono::high_resolution_clock::now();
	auto prevTime = startTime;
	float camSpeed = 2.f;
	float mouseSensativity = 0.1f;

	while (stayOpen)
	{
		auto curTime = std::chrono::high_resolution_clock::now();
		sklTime.totalTime = std::chrono::duration<float, std::chrono::seconds::period>(curTime - startTime).count();
		sklTime.deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(curTime - prevTime).count();

		// FPS cap
		uint32_t FPSCap = 300;
		while (sklTime.deltaTime < (1.f / FPSCap))
		{
			curTime = std::chrono::high_resolution_clock::now();
			sklTime.totalTime = std::chrono::duration<float, std::chrono::seconds::period>(curTime - startTime).count();
			sklTime.deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(curTime - prevTime).count();
		}

		while (SDL_PollEvent(&e) != 0)
		{
			if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
			{
				stayOpen = false;
			}
			if (e.type == SDL_MOUSEMOTION)
			{
				renderer->cam.yaw += e.motion.xrel * mouseSensativity;
				renderer->cam.pitch -= e.motion.yrel * mouseSensativity;
				renderer->cam.pitch = glm::clamp(renderer->cam.pitch, -89.f, 89.f);
			}
		}

		int count = 0;
		const Uint8* keys = SDL_GetKeyboardState(&count);
		if (keys[SDL_SCANCODE_W])
			renderer->cam.position += renderer->cam.GetForward() * camSpeed * sklTime.deltaTime;
		if (keys[SDL_SCANCODE_S])
			renderer->cam.position -= renderer->cam.GetForward() * camSpeed * sklTime.deltaTime;
		if (keys[SDL_SCANCODE_D])
			renderer->cam.position += renderer->cam.GetRight() * camSpeed * sklTime.deltaTime;
		if (keys[SDL_SCANCODE_A])
			renderer->cam.position -= renderer->cam.GetRight() * camSpeed * sklTime.deltaTime;
		if (keys[SDL_SCANCODE_E] || keys[SDL_SCANCODE_LSHIFT])
			renderer->cam.position += glm::vec3(0.f, 1.f, 0.f) * camSpeed * sklTime.deltaTime;
		if (keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_LCTRL])
			renderer->cam.position -= glm::vec3(0.f, 1.f, 0.f) * camSpeed * sklTime.deltaTime;

		mvp.model = glm::rotate(glm::mat4(1.f), sklTime.totalTime, glm::vec3(0.f, 1.f, 0.f));
		mvp.view = renderer->cam.GetViewMatrix();
		mvp.proj = renderer->cam.projectionMatrix;
		mvp.proj[1][1] *= -1;

		renderer->bufferManager->FillBuffer(renderer->mvpMemory, &mvp, sizeof(mvp));

		CoreLoop();

		renderer->RenderFrame();

		SKL_PRINT_SLIM("%8f, %8f, %6u, %8f FPS", sklTime.totalTime, sklTime.deltaTime, sklTime.frameCount, 1.f / sklTime.deltaTime);
		prevTime = curTime;
		sklTime.frameCount++;
	}
}

void skeleton::Application::DefineShaders()
{
	
}

void skeleton::Application::CreateObject(
	const char* _meshDirectory,
	uint32_t _parProgIndex)
{
	vulkanContext.renderables.push_back(
		CreateRenderable(CreateMesh(_meshDirectory), _parProgIndex));
}

skeleton::mesh_t skeleton::Application::CreateMesh(
	const char* _directory)
{
	return skeleton::tools::LoadMesh(_directory, renderer->bufferManager);
}

sklRenderable_t skeleton::Application::CreateRenderable(
	mesh_t _mesh,
	uint32_t _parProgIndex)
{
	sklRenderable_t r;
	r.mesh = _mesh;
	r.parProgIndex = _parProgIndex;
	return r;
}

