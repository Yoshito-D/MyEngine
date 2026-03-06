#pragma once
#include <string>

namespace GameEngine {
#ifdef USE_IMGUI
class Sprite;
class Model;

namespace ObjectEdit {
/// @brief Sprite のパラメータを ImGui でリアルタイム編集する
/// @param name ImGui ウィンドウ名
/// @param sprite 編集対象の Sprite（nullptr の場合は何もしない）
void SpriteEdit(const std::string& name, GameEngine::Sprite* sprite);

/// @brief Model のパラメータを ImGui でリアルタイム編集する
/// @param name ImGui ウィンドウ名
/// @param model 編集対象の Model（nullptr の場合は何もしない）
void ModelEdit(const std::string& name, GameEngine::Model* model);
}
#endif // USE_IMGUI
}