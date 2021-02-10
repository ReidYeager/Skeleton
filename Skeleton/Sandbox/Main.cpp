
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

		skeleton::CreateShader("default", SKL_SHADER_VERT_STAGE | SKL_SHADER_FRAG_STAGE);

		renderer->descriptorSetLayout = vulkanContext.parProgs[0].descriptorSetLayout;

		renderer->CreateRenderer();

		renderer->CreateModelBuffers();
		renderer->CreateDescriptorSet();
		renderer->RecordCommandBuffers();
	}

	void Run()
	{
		__super::Run();
	}

	void Cleanup()
	{
		__super::Cleanup();
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

