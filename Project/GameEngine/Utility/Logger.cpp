#include "Logger.h"
#include <algorithm>
#include <format>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
const char* const kLogRootDirectory = "Logs";
const char* const kAllLogFileName = "all.log";

// C++20 timezone APIは環境によって起動時に例外を投げるため、Win32のローカル時刻を使う。
std::string FormatLocalTimeForDirectory() {
   SYSTEMTIME localTime{};
   GetLocalTime(&localTime);

   return std::format(
      "{:04}-{:02}-{:02}_{:02}-{:02}-{:02}",
      localTime.wYear,
      localTime.wMonth,
      localTime.wDay,
      localTime.wHour,
      localTime.wMinute,
      localTime.wSecond);
}

std::string FormatLocalTimeForLogLine() {
   SYSTEMTIME localTime{};
   GetLocalTime(&localTime);

   return std::format(
      "{:04}-{:02}-{:02} {:02}:{:02}:{:02}",
      localTime.wYear,
      localTime.wMonth,
      localTime.wDay,
      localTime.wHour,
      localTime.wMinute,
      localTime.wSecond);
}

std::string ExtractSourceFileName(const char* sourceFilePath) {
   if (!sourceFilePath) {
	  return {};
   }

   std::string_view path(sourceFilePath);
   const size_t separatorPosition = path.find_last_of("/\\");
   if (separatorPosition == std::string_view::npos) {
	  return std::string(path);
   }

   return std::string(path.substr(separatorPosition + 1));
}
}

Logger& Logger::GetInstance() {
   static Logger instance;
   return instance;
}

void Logger::Initialize() {
   GetInstance().InitializeInternal();
}

void Logger::WriteLogEntry(const std::string& message, LogLevel level, LogChannel channel, std::source_location location) {
   Logger& instance = GetInstance();
   instance.Log(message, level, channel, location);
}

void Logger::WriteLogEntry(const std::wstring& message, LogLevel level, LogChannel channel, std::source_location location) {
   Logger& instance = GetInstance();
   instance.Log(message, level, channel, location);
}

void Logger::InitializeInternal() {
   // 出力中にストリームを差し替えないよう、再初期化全体を通常の書き込みと同じMutexで保護する。
   std::lock_guard<std::mutex> lock(logMutex_);

   // 新しい出力先がすべて開くまでは未初期化扱いにし、部分的なファイル群へ書き込ませない。
   isInitialized_ = false;
   CloseStreams();

   std::filesystem::create_directories(kLogRootDirectory);
   std::string dateString = FormatLocalTimeForDirectory();

   currentLogDirectory_ = std::filesystem::path(kLogRootDirectory) / dateString;
   std::filesystem::create_directories(currentLogDirectory_);

   OpenChannelLogFile(LogChannel::Engine, currentLogDirectory_ / "engine.log");
   OpenChannelLogFile(LogChannel::Game, currentLogDirectory_ / "game.log");
   OpenChannelLogFile(LogChannel::Editor, currentLogDirectory_ / "editor.log");

   // チャンネル別ログに加えて時系列を横断できる集約ログへも同じ行を出力する。
   const std::filesystem::path allLogFilePath = currentLogDirectory_ / kAllLogFileName;
   allLogStream_.open(allLogFilePath, std::ios::out | std::ios::app);
   if (!allLogStream_.is_open()) {
	  throw std::runtime_error("ログファイルを開けません: " + allLogFilePath.generic_string());
   }

   // 現在のストリーム確立後に世代整理し、起動中のディレクトリを削除候補から除外する。
   CleanOldLogFiles(kLogRootDirectory);

   isInitialized_ = true;
}

void Logger::Log(const std::string& message, LogLevel level, std::source_location location) {
   Log(message, level, LogChannel::Engine, location);
}

void Logger::Log(const std::wstring& message, LogLevel level, std::source_location location) {
   Log(message, level, LogChannel::Engine, location);
}

void Logger::Log(const std::string& message, LogLevel level, LogChannel channel, std::source_location location) {
   Log(ConvertString(message), level, channel, location);
}

void Logger::Log(const std::wstring& message, LogLevel level, LogChannel channel, std::source_location location) {
   // 1行の整形と複数出力先への書き込みを不可分にし、スレッド間で行が混ざるのを防ぐ。
   std::lock_guard<std::mutex> lock(logMutex_);
   WriteLog(message, level, channel, location);
}

std::wstring Logger::ConvertString(const std::string& str) {
   if (str.empty()) {
	  return std::wstring();
   }

   // Win32変換をサイズ照会と実変換の2段階で行い、可変長UTF-8でも切り捨てない。
   auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
   if (sizeNeeded == 0) {
	  return std::wstring();
   }
   std::wstring result(sizeNeeded, 0);
   MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
   return result;
}

std::string Logger::ConvertString(const std::wstring& str) {
   if (str.empty()) {
	  return std::string();
   }

   // デバッグ出力用UTF-16から、ログファイル共通のUTF-8バイト列へ必要量だけ確保して変換する。
   auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
   if (sizeNeeded == 0) {
	  return std::string();
   }
   std::string result(sizeNeeded, 0);
   WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
   return result;
}

std::wstring Logger::ConvertStringToWString(const std::string& str) {
   std::wstringstream wss;
   wss << str.c_str();
   return wss.str();
}

void Logger::WriteToFile(std::ostream& os, const std::string& logMessage) {
   os << logMessage;
   os.flush();
}

void Logger::CloseStreams() {
   for (std::ofstream& stream : channelStreams_) {
	  if (stream.is_open()) {
		 stream.close();
	  }
   }

   if (allLogStream_.is_open()) {
	  allLogStream_.close();
   }
}

void Logger::OpenChannelLogFile(LogChannel channel, const std::filesystem::path& filePath) {
   std::ofstream& stream = GetLogChannelStream(channel);
   stream.open(filePath, std::ios::out | std::ios::app);
   if (!stream.is_open()) {
	  throw std::runtime_error("ログファイルを開けません: " + filePath.generic_string());
   }
}

void Logger::WriteLog(const std::wstring& message, LogLevel level, LogChannel channel, std::source_location location) {
   if (!isInitialized_) {
	  throw std::runtime_error("Logger::Initialize() が呼び出されていません。");
   }

   // 表示とファイルで時刻・重大度・呼び出し元がずれないよう、整形結果を一度だけ作って分配する。
   std::wstring logMessage = FormatLogMessage(message, level, channel, location);
   std::string narrowLogMessage = ConvertString(logMessage);

   OutputDebugStringW(logMessage.c_str());
   WriteToFile(GetLogChannelStream(channel), narrowLogMessage);
   WriteToFile(allLogStream_, narrowLogMessage);
}

size_t Logger::GetLogChannelIndex(LogChannel channel) const {
   switch (channel) {
	  case LogChannel::Engine: return 0;
	  case LogChannel::Game:   return 1;
	  case LogChannel::Editor: return 2;
	  default:                 return 0;
   }
}

std::ofstream& Logger::GetLogChannelStream(LogChannel channel) {
   return channelStreams_[GetLogChannelIndex(channel)];
}

std::wstring Logger::FormatLogMessage(const std::wstring& message, LogLevel level, LogChannel channel, std::source_location location) {
   std::string timestamp = FormatLocalTimeForLogLine();

   std::wstring logMessage =
      L"[" + ConvertString(timestamp) + L"] " +
      L"[" + GetLogLevelString(level) + L"] " +
      L"[" + GetLogChannelString(channel) + L"] " +
      L"[" + ConvertString(GetSourceLocationString(location)) + L"] " +
      message + L"\n";

   return logMessage;
}

std::string Logger::GetSourceLocationString(std::source_location location) const {
   // ビルド環境固有の絶対パスを落とし、ログを短く保ちながらファイル・行・関数は残す。
   std::string fileName = ExtractSourceFileName(location.file_name());
   if (fileName.empty()) {
	  fileName = "unknown";
   }

   return std::format("{}:{} {}", fileName, location.line(), location.function_name());
}

std::wstring Logger::GetLogLevelString(LogLevel level) {
   switch (level) {
	  case LogLevel::Trace:    return L"TRACE   ";
	  case LogLevel::Debug:    return L"DEBUG   ";
	  case LogLevel::Info:     return L"INFO    ";
	  case LogLevel::Warning:  return L"WARNING ";
	  case LogLevel::Error:    return L"ERROR   ";
	  case LogLevel::Critical: return L"CRITICAL";
	  default:                 return L"UNKNOWN ";
   }
}

std::wstring Logger::GetLogChannelString(LogChannel channel) {
   switch (channel) {
	  case LogChannel::Engine: return L"ENGINE";
	  case LogChannel::Game:   return L"GAME  ";
	  case LogChannel::Editor: return L"EDITOR";
	  default:                 return L"UNKNOWN";
   }
}

void Logger::CleanOldLogFiles(const std::string& logDir) {
   std::vector<std::filesystem::path> logFiles;
   const std::filesystem::path currentLogDirectory = currentLogDirectory_.empty() ? std::filesystem::path() : std::filesystem::absolute(currentLogDirectory_);

   // 現行の日時ディレクトリと旧形式の直下.logを同じ世代数の対象として列挙する。
   // ディレクトリ内のログファイルを取得
   for (const auto& entry : std::filesystem::directory_iterator(logDir)) {
	  if (!currentLogDirectory.empty() && std::filesystem::absolute(entry.path()) == currentLogDirectory) {
		 continue;
	  }

	  if ((entry.is_regular_file() && entry.path().extension() == ".log") || entry.is_directory()) {
		 logFiles.push_back(entry.path());
	  }
   }

   // ファイルを作成日時でソート（古い順）
   std::sort(logFiles.begin(), logFiles.end(), [](const std::filesystem::path& a, const std::filesystem::path& b) {
	  return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
	  }
   );

   // ログファイルが指定数を超えている場合、古いファイルを削除
   // 現在のディレクトリ1件を上限へ含めるため、古い世代へ割り当てる枠を1つ減らす。
   const size_t maxOldLogFiles = currentLogDirectory_.empty() ? kMaxLogFiles : kMaxLogFiles - 1;
   while (logFiles.size() > maxOldLogFiles) {
	  std::wcout << L"Deleting old log file: " << logFiles.front() << std::endl;
	  if (std::filesystem::is_directory(logFiles.front())) {
		 std::filesystem::remove_all(logFiles.front());  // 古いログフォルダ削除
	  } else {
		 std::filesystem::remove(logFiles.front());  // 古いファイル削除
	  }
	  logFiles.erase(logFiles.begin());  // 削除したファイルをリストから削除
   }
}
