#pragma once
#include <string>

namespace GameEngine {
class ParticleSystem;
}

namespace ParticleSystemEdit {
/// @brief ParticleSystem のパラメータを ImGui でリアルタイム編集する
/// @param particleSystem 編集対象の ParticleSystem（nullptr の場合は何もしない）
/// @param name パーティクルシステムの名前（ImGuiのウィンドウ/タブ識別用）
void Edit(GameEngine::ParticleSystem* particleSystem, const std::string& name = "Particle System");
}
