
#include "pch.h"
#include "skeleton/core/application.h"

#include <stdio.h>
#include <chrono>

#include "skeleton/core/debug_tools.h"
#include "skeleton/core/time.h"
#include "skeleton/core/file_system.h"

void Application::Run()
{
  Init();
  MainLoop();
  Cleanup();
}

void Application::CreateObject(const char* _meshDirectory, uint32_t _shaderProgramIndex)
{
  mesh_t m = CreateMesh(_meshDirectory);

  // Creates a renderable from mesh & ShaderProgram
  vulkanContext.renderables.push_back({m, _shaderProgramIndex});

  renderer->CreateDescriptorSet(vulkanContext.shaderPrograms[_shaderProgramIndex],
                                vulkanContext.renderables[vulkanContext.renderables.size() - 1]);
}

mesh_t Application::CreateMesh(const char* _directory)
{
  return LoadMesh(_directory, renderer->bufferManager);
}

void Application::Init()
{
  SKL_PRINT("Application", "Init =================================================");

  // Create SDL window
  //=================================================
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
  {
    SKL_LOG("SDL ERROR", "%s", SDL_GetError());
    throw "SDL failure";
  }

  uint32_t sdlFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_INPUT_FOCUS |
                      SDL_WINDOW_RESIZABLE;
  window = SDL_CreateWindow("Skeleton Application", SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, 800, 600, sdlFlags);
  SDL_SetRelativeMouseMode(SDL_TRUE);
  SDL_SetWindowGrab(window, SDL_TRUE);

  // Get instance extensions required by SDL
  uint32_t sdlExtensionCount;
  SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, nullptr);
  std::vector<const char*> sdlExtensions(sdlExtensionCount);
  SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, sdlExtensions.data());

  // Create Renderer
  //=================================================
  renderer = new Renderer(sdlExtensions, window);
  renderer->CreateRenderer();

  Start();
}

void Application::Cleanup()
{
  SKL_PRINT("Application", "Cleanup =================================================");

  delete(renderer);
  SDL_DestroyWindow(window);
}

void Application::MainLoop()
{
  SKL_PRINT("Application", "MainLoop =================================================");

  SDL_Event e;
  auto startTime = std::chrono::high_resolution_clock::now();
  auto prevTime = startTime;

  float camSpeed = 2.f;
  float mouseSensativity = 0.1f;

  uint32_t FPSCap = -1;
  uint32_t FPSPrintIndex = 0;
  float deltaSum = 0.001f;
  uint32_t deltaCount = 1;

  char titleBuffer[255];
  int titleBufferSize = 0;

  while (!appShouldClose)
  {
    auto curTime = std::chrono::high_resolution_clock::now();
    sklTime.totalTime = std::chrono::duration<float, std::chrono::seconds::period>
        (curTime - startTime).count();
    sklTime.deltaTime = std::chrono::duration<float, std::chrono::seconds::period>
        (curTime - prevTime).count();

    // FPS cap
    while (FPSCap != -1 && sklTime.deltaTime < (1.f / FPSCap))
    {
      curTime = std::chrono::high_resolution_clock::now();
      sklTime.totalTime = std::chrono::duration<float, std::chrono::seconds::period>
          (curTime - startTime).count();
      sklTime.deltaTime = std::chrono::duration<float, std::chrono::seconds::period>
          (curTime - prevTime).count();
    }

    // Poll and handle SDL events
    while (SDL_PollEvent(&e) != 0)
    {
      if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
      {
        appShouldClose = true;
      }
      if (e.type == SDL_MOUSEMOTION)
      {
        renderer->cam.yaw += e.motion.xrel * mouseSensativity;
        renderer->cam.pitch -= e.motion.yrel * mouseSensativity;
        renderer->cam.pitch = glm::clamp(renderer->cam.pitch, -89.f, 89.f);
      }
    }

    int keyCount = 0;
    const Uint8* keyboardState = SDL_GetKeyboardState(&keyCount);

    // TODO : Create a proper input system and move this to Main's Application
    if (keyboardState[SDL_SCANCODE_W])
      renderer->cam.position += renderer->cam.GetForward() * camSpeed * sklTime.deltaTime;
    if (keyboardState[SDL_SCANCODE_S])
      renderer->cam.position -= renderer->cam.GetForward() * camSpeed * sklTime.deltaTime;
    if (keyboardState[SDL_SCANCODE_D])
      renderer->cam.position += renderer->cam.GetRight() * camSpeed * sklTime.deltaTime;
    if (keyboardState[SDL_SCANCODE_A])
      renderer->cam.position -= renderer->cam.GetRight() * camSpeed * sklTime.deltaTime;
    if (keyboardState[SDL_SCANCODE_E] || keyboardState[SDL_SCANCODE_LSHIFT])
      renderer->cam.position += glm::vec3(0.f, 1.f, 0.f) * camSpeed * sklTime.deltaTime;
    if (keyboardState[SDL_SCANCODE_Q] || keyboardState[SDL_SCANCODE_LCTRL])
      renderer->cam.position -= glm::vec3(0.f, 1.f, 0.f) * camSpeed * sklTime.deltaTime;

    mvp.model = glm::rotate(glm::mat4(1.f), sklTime.totalTime, glm::vec3(0.f, 1.f, 0.f));
    mvp.view = renderer->cam.GetViewMatrix();
    mvp.proj = renderer->cam.projectionMatrix;
    mvp.proj[1][1] *= -1;

    renderer->bufferManager->FillBuffer(renderer->mvpMemory, &mvp, sizeof(mvp));

    CoreLoop();
    renderer->RenderFrame();

    // Print FPS information once per second
    if (sklTime.totalTime < FPSPrintIndex)
    {
      deltaSum += sklTime.deltaTime;
      deltaCount++;
    }
    else
    {
      float avgFPS = deltaSum / deltaCount;

      SKL_PRINT_SLIM("%6.0f sec, %4.2f ms average, Frame# %6u, %4.2f FPS", sklTime.totalTime,
                     avgFPS * 1000.f, sklTime.frameCount, 1.f / avgFPS);
      titleBufferSize = sprintf_s(titleBuffer, 255, "Skeleton :==: %4.2f ms :==: %4.2f FPS",
                                  avgFPS * 1000.0f, 1.0f / avgFPS);
      SDL_SetWindowTitle(window, titleBuffer);

      FPSPrintIndex++;
      deltaSum = 0;
      deltaCount = 0;
    }

    prevTime = curTime;
    sklTime.frameCount++;
  }
}

