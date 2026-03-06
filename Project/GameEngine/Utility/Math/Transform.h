#pragma once
#include "Vector3.h"

namespace GameEngine {

struct Transform {
   Vector3 scale;
   Vector3 rotation;
   Vector3 translation;

   Transform() :scale(1.0f, 1.0f, 1.0f), rotation(0.0f, 0.0f, 0.0f), translation(0.0f, 0.0f, 0.0f) {}
};

} // namespace GameEngine