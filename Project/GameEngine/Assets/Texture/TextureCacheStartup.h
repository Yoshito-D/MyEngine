#pragma once

namespace GameEngine {
/// @brief ビルド用引数がある場合にDDSを差分生成して終了する。通常起動時は何もしない。
/// @note ゲーム初期化前に呼ぶ。生成の成否はプロセスの終了コードでMSBuildへ伝える。
void RunTextureCacheBuildCommandIfRequested();

/// @brief Resources内のDDS不足分を生成し、生成・再利用件数をログへ出力する。
/// @throws std::runtime_error DDSの準備に失敗した場合。
/// @note COMとLoggerの初期化後、TextureManagerの初期化前に呼び出す。
void PrepareStartupTextureCache();
}
