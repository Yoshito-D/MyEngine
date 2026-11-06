#pragma once

namespace GameEngine {
/// @brief ビルド用引数がある場合にsounds/CookedのPCMキャッシュを差分生成して終了する。
/// @note ゲーム初期化前に呼ぶ。COMとMedia Foundationを生成処理中だけ初期化し、成否を終了コードで伝える。
void RunAudioCacheBuildCommandIfRequested();
}
