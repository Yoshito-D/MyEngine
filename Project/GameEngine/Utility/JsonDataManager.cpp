#include "JsonDataManager.h"

#include <Windows.h>
#include <algorithm>
#include <atomic>

namespace GameEngine {

bool SaveJsonFileAtomically(const std::filesystem::path& filePath, const json& data, int indent) {
   // シリアライズの例外で既存の保存データが切り詰められないよう、I/Oより先に完了させる。
   std::string serialized;
   try {
      const std::string formatted = data.dump(indent);
      serialized.reserve(formatted.size());
      for (const char character : formatted) {
         // 従来のWindowsテキスト保存と同じCRLFを維持する。
         if (character == '\n') serialized += '\r';
         serialized += character;
      }
   } catch (const std::exception&) {
      return false;
   }

   std::error_code error;
   if (!filePath.parent_path().empty()) {
      std::filesystem::create_directories(filePath.parent_path(), error);
      if (error) return false;
   }

   // 同一ディレクトリでの置換を使う。CREATE_NEWにより他の保存処理や既存の一時ファイルを上書きしない。
   static std::atomic<unsigned long long> sequence{ 0 };
   std::filesystem::path temporaryPath;
   HANDLE file = INVALID_HANDLE_VALUE;
   for (int attempt = 0; attempt < 32; ++attempt) {
      temporaryPath = filePath;
      temporaryPath += L".save." + std::to_wstring(GetCurrentProcessId()) +
         L"." + std::to_wstring(sequence.fetch_add(1)) + L".tmp";
      file = CreateFileW(temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (file != INVALID_HANDLE_VALUE) break;
      if (GetLastError() != ERROR_FILE_EXISTS) return false;
   }
   if (file == INVALID_HANDLE_VALUE) return false;

   bool succeeded = true;
   size_t offset = 0;
   while (offset < serialized.size()) {
      const DWORD size = static_cast<DWORD>(std::min<size_t>(serialized.size() - offset, MAXDWORD));
      DWORD written = 0;
      if (!WriteFile(file, serialized.data() + offset, size, &written, nullptr) || written == 0) {
         succeeded = false;
         break;
      }
      offset += written;
   }
   if (succeeded && !FlushFileBuffers(file)) succeeded = false;
   if (!CloseHandle(file)) succeeded = false;

   if (succeeded) {
      succeeded = MoveFileExW(temporaryPath.c_str(), filePath.c_str(),
         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
   }
   if (!succeeded) {
      // この呼び出しがCREATE_NEWで作成したファイルだけを片付ける。
      DeleteFileW(temporaryPath.c_str());
   }
   return succeeded;
}

} // namespace GameEngine
