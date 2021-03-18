
#ifndef SKELETON_CORE_TIME_H
#define SKELETON_CORE_TIME_H 1

// TODO : Move to a managed class/struct
// Stores all application time information
static struct SKL_ApplicationTimeData
{
  float totalTime;      // In seconds
  float deltaTime;      // In seconds
  uint32_t frameCount;  //Number of frames rendered
} sklTime;

#endif // !SKELETON_CORE_TIME_H

