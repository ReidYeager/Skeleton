
#include <iostream>

#include "skeleton.h"

// TODO : Define shader files/directory

class sandboxApp : public Application
{
public:
  void Start()
  {
    renderer->cam.yaw = -90.f;
    renderer->cam.position.z = 3;
    renderer->cam.UpdateProjection(
        vulkanContext.renderExtent.width / float(vulkanContext.renderExtent.height));

    renderer->CreateModelBuffers();

    uint32_t i = GetShaderProgram("default", Skl_Shader_Vert_Stage | Skl_Shader_Frag_Stage,
                                  Skl_Cull_Mode_Front);
    uint32_t j = GetShaderProgram("blue", Skl_Shader_Vert_Stage | Skl_Shader_Frag_Stage,
                                  Skl_Cull_Mode_Back);
    CreateObject("./res/models/SphereSmooth.obj", j);
    CreateObject("./res/models/SphereSmooth.obj", i);
    CreateObject("./res/models/Cube.obj", i);

    renderer->RecordCommandBuffers();
  }

  void CoreLoop()
  {
    for (const auto& r : vulkanContext.renderables)
    {
      renderer->bufferManager->CopyBuffer(renderer->mvpBuffer, r.buffers[0]->buffer, sizeof(mvp));
    }
  }

};

int main(int argc, char* argv[])
{
  try
  {
    sandboxApp app;
    app.Run();
  }
  catch (const char* e)
  {
    std::cout << "Caught: " << e << "\n";
  }

  system("PAUSE");
  return 0;
}

