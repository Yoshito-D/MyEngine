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
static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
   // ログ初期化前のクラッシュでも一意に近い出力先を作れるよう、ローカル日時をファイル名へ埋め込む。
   SYSTEMTIME time;
   GetLocalTime(&time);
   wchar_t filePath[MAX_PATH] = { 0 };
   CreateDirectory(L"./dumps", nullptr);
   StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
   // デバッガー等が同時に読み取れるよう共有アクセスを許可し、同名の場合は最新ダンプで置き換える。
   HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
   // processId（このexeのId）とクラッシュ（例外）の発生したthreadIdを取得
   DWORD processId = GetCurrentProcessId();
   DWORD threadId = GetCurrentThreadId();
   // 設定情報を入力
   MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
   minidumpInformation.ThreadId = threadId;
   minidumpInformation.ExceptionPointers = exception;
   // 例外情報はダンプ対象（このプロセス）のアドレス空間にあるため、対象側ポインターとして解釈させる。
   minidumpInformation.ClientPointers = TRUE;
   // Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
   MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
   // 他に関連づけられているSEH例外ハンドラがあれば実行。通常はプロセスを終了する
   return EXCEPTION_EXECUTE_HANDLER;
}
}