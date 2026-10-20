#pragma once

#include <cstdlib>
#include <Windows.h>
#include <dbghelp.h>
#include <strsafe.h>

#pragma comment(lib, "Shcore.lib")
#pragma comment(lib, "Dbghelp.lib")

namespace GameEngine {

/// @brief 未処理の Windows 構造化例外から最小クラッシュダンプを出力する。
/// @param exception 例外コード、CPU コンテキスト、発生位置を含む SEH 情報。
/// @return ダンプ出力後に通常の未処理例外処理を続行させる EXCEPTION_EXECUTE_HANDLER。
/// @details 実行ディレクトリ直下の Dumps フォルダーへ発生日時を含むファイル名で保存する。
///          例外フィルターから呼ばれるため、追加のゲームエンジン機能に依存せず Win32 API だけで完結させる。
LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception);
}
