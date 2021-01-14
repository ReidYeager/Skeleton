
#include "pch.h"
#include <stdio.h>

#include "Application.h"

void skeleton::Application::Init()
{
	renderer = new Renderer();
	delete(renderer);
}
