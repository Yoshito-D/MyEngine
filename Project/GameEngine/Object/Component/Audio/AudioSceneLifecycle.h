#pragma once
namespace GameEngine {
/// @brief 読み込んだシーンに有効な準備済み起動BGMがあるかどうか。
bool HasSceneBgmRequest();
/// @brief シーンの設定と参照がすべて準備できた後、オブジェクト音声を1回開始する。
void BeginSceneAudio();
}
