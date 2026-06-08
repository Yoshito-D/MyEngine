#pragma once
#include <string>

namespace GameEngine {
class ParticleSystem;
}

namespace ParticleSystemEdit {
/// @brief ParticleSystem のパラメータを ImGui でリアルタイム編集する
/// @param particleSystem 編集対象の ParticleSystem（nullptr の場合は何もしない）
void Edit(GameEngine::ParticleSystem* particleSystem);
}
