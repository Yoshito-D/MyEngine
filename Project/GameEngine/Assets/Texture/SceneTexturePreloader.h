#pragma once

#include <nlohmann/json_fwd.hpp>

namespace GameEngine {
class TextureManager;
/// @brief シーンと参照先Particle設定のテクスチャを構築前に同期準備する。
/// @return 参照ファイル・必須テクスチャをすべて準備できた場合true。
bool PreloadSceneTextures(TextureManager& textures, const nlohmann::json& scene);
}
