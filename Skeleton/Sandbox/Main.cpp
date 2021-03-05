
#include <iostream>

#include "Skeleton.h"

// TODO : Implement shader layout file handling
// TODO : Define shader files/directory

class sandboxApp : public skeleton::Application
{
public:
	void Init()
	{
		__super::Init();

		renderer->CreateRenderer();
		renderer->CreateModelBuffers();

		uint32_t i = skeleton::GetProgram("default", SKL_SHADER_VERT_STAGE | SKL_SHADER_FRAG_STAGE, SKL_CULL_MODE_FRONT);
		uint32_t j = skeleton::GetProgram("blue", SKL_SHADER_VERT_STAGE | SKL_SHADER_FRAG_STAGE, SKL_CULL_MODE_BACK);
		CreateObject("./res/models/SphereSmooth.obj", j);
		CreateObject("./res/models/SphereSmooth.obj", i);
		CreateObject("./res/models/Cube.obj", i);


		//renderer->CreateDescriptorSet(vulkanContext.parProgs[i], );
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
		app.Init();
		app.Run();
		app.Cleanup();
	}
	catch (const char* e)
	{
		std::cout << "Caught: " << e << "\n";
	}

	return 0;
}

