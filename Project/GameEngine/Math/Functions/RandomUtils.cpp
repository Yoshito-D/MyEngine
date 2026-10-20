#include "GameEngine/pch.h"
#include "GameEngine/Math/Functions/RandomUtils.h"

namespace GameEngine::RandomUtils {

std::mt19937& RandomManager::Engine() {
   static std::random_device rd;
   static std::mt19937 engine(rd());
   return engine;
}

} // namespace GameEngine::RandomUtils
