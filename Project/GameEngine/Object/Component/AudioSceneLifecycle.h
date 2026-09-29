#pragma once
namespace GameEngine {
/// @brief Whether the loaded scene has an enabled and prepared startup BGM.
bool HasSceneBgmRequest();
/// @brief Start object audio once after all scene settings and references are ready.
void BeginSceneAudio();
}
