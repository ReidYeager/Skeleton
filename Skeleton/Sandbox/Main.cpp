
#include <iostream>

#include "Skeleton.h"

int main(int argc, char* argv[])
{
	try
	{
		skeleton::Application a;
		a.Init();
	}
	catch (const char* e)
	{
		std::cout << "Failure: " << e << "\n";
	}

	return 0;
}

