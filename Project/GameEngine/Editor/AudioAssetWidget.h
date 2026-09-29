#pragma once
#ifdef USE_IMGUI
#include <string>
namespace GameEngine {
/// @brief 共通の音声選択欄とドロップ先を描画する。アセットIDが変更された場合だけtrueを返す。
bool DrawAudioAssetWidget(const char* label, std::string& assetId);
}
#endif
