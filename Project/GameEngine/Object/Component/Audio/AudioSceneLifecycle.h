#pragma once

namespace GameEngine {
class SceneWorld;

/// @brief 読み込んだシーンに有効な準備済み起動BGMがあるかどうか。
bool HasSceneBgmRequest(SceneWorld& world);

/// @brief シーンの設定と参照がすべて準備できた後、オブジェクト音声を1回開始する。
void BeginSceneAudio(SceneWorld& world);
}
