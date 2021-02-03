#pragma once

namespace skeleton
{

static struct SKL_ApplicationTimeData
{
	float totalTime; /*In seconds*/
	float deltaTime; /*In seconds*/
	uint32_t frameCount; /*No. frames rendered*/
} sklTime;

} // namespace skeleton

