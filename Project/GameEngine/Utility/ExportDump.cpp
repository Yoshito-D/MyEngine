#include "GameEngine/pch.h"
#include "GameEngine/Utility/ExportDump.h"

namespace GameEngine {

LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
   // ログ初期化前のクラッシュでも一意に近い出力先を作れるよう、ローカル日時をファイル名へ埋め込む。
   SYSTEMTIME time;
   GetLocalTime(&time);
   wchar_t filePath[MAX_PATH] = {0};
   CreateDirectory(L"./dumps", nullptr);
   StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay,
      time.wHour, time.wMinute);

   // デバッガー等が同時に読み取れるよう共有アクセスを許可し、同名の場合は最新ダンプで置き換える。
   HANDLE dumpFileHandle =
      CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

   // processId（このexeのId）とクラッシュ（例外）の発生したthreadIdを取得
   DWORD processId = GetCurrentProcessId();
   DWORD threadId = GetCurrentThreadId();

   // 設定情報を入力
   MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{0};
   minidumpInformation.ThreadId = threadId;
   minidumpInformation.ExceptionPointers = exception;

   // 例外情報はダンプ対象（このプロセス）のアドレス空間にあるため、対象側ポインターとして解釈させる。
   minidumpInformation.ClientPointers = TRUE;

   // Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
   MiniDumpWriteDump(
      GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);

   // 他に関連づけられているSEH例外ハンドラがあれば実行。通常はプロセスを終了する
   return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace GameEngine
