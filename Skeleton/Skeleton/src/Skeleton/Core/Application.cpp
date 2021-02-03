
#include "pch.h"
#include <stdio.h>
#include <chrono>

#include "Skeleton/Core/Common.h"
#include "Application.h"

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
	while (stayOpen)
	{
		CoreLoop();
	}
}

void skeleton::Application::CoreLoop()
{
	SKL_PRINT_SLIM("%f, %f, %6u, %8f FPS", sklTime.totalTime, sklTime.deltaTime, sklTime.frameCount, 1.f/sklTime.deltaTime);
	static SDL_Event e;
	static auto startTime = std::chrono::high_resolution_clock::now();
	static auto prevTime = startTime;

	float camSpeed = 2.f;
	float mouseSensativity = 0.1f;

	auto curTime = std::chrono::high_resolution_clock::now();
	sklTime.totalTime = std::chrono::duration<float, std::chrono::seconds::period>(curTime - startTime).count();
	sklTime.deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(curTime - prevTime).count();

	while (SDL_PollEvent(&e) != 0)
	{
		//if (e.type == SDL_WINDOWEVENT)
		//{
		//	#define sdls(x) case(x): SKL_PRINT("SDL WindowEvent", #x); break;
		//	switch (e.window.event)
		//	{
		//	sdls(SDL_WINDOWEVENT_CLOSE);
		//	sdls(SDL_WINDOWEVENT_ENTER);
		//	sdls(SDL_WINDOWEVENT_EXPOSED);
		//	sdls(SDL_WINDOWEVENT_RESIZED);
		//	sdls(SDL_WINDOWEVENT_FOCUS_GAINED);
		//	sdls(SDL_WINDOWEVENT_FOCUS_LOST);
		//	sdls(SDL_WINDOWEVENT_HIDDEN);
		//	sdls(SDL_WINDOWEVENT_HIT_TEST);
		//	sdls(SDL_WINDOWEVENT_LEAVE);
		//	sdls(SDL_WINDOWEVENT_MAXIMIZED);
		//	sdls(SDL_WINDOWEVENT_MINIMIZED);
		//	sdls(SDL_WINDOWEVENT_MOVED);
		//	sdls(SDL_WINDOWEVENT_NONE);
		//	sdls(SDL_WINDOWEVENT_RESTORED);
		//	sdls(SDL_WINDOWEVENT_SHOWN);
		//	sdls(SDL_WINDOWEVENT_SIZE_CHANGED);
		//	sdls(SDL_WINDOWEVENT_TAKE_FOCUS);
		//	default: SKL_PRINT("Not an SDL windowEvent",""); break;
		//	}
		//	#undef sdls
		//}
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
	if (keys[SDL_SCANCODE_E])
		renderer->cam.position += glm::vec3(0.f, 1.f, 0.f) * camSpeed * sklTime.deltaTime;
	if (keys[SDL_SCANCODE_Q])
		renderer->cam.position -= glm::vec3(0.f, 1.f, 0.f) * camSpeed * sklTime.deltaTime;

	//mvp.model = glm::mat4(1.0f);
	mvp.model = glm::rotate(glm::mat4(1.f), sklTime.totalTime, glm::vec3(0.f, 1.f, 0.f));
	mvp.view = renderer->cam.GetViewMatrix();
	mvp.proj = renderer->cam.projectionMatrix;
	mvp.proj[1][1] *= -1;

	renderer->bufferManager->FillBuffer(renderer->mvpMemory, &mvp, sizeof(mvp));

	renderer->RenderFrame();

	prevTime = curTime;
	sklTime.frameCount++;
}
