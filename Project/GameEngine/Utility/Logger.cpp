#include "Logger.h"

Logger& Logger::GetInstance() {
   static Logger instance;
   return instance;
}

void Logger::Initialize() {
   std::filesystem::create_directory("logs");
   std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
   std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
	  nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
   std::chrono::zoned_time localTime{ std::chrono::current_zone(),nowSeconds };
   std::string dateString = std::format("{:%Y-%m-%d_%H-%M-%S}", localTime);
   std::string logFilePath = std::string("logs/") + dateString + ".log";

   logStream_.open(logFilePath, std::ios::out | std::ios::app);
   if (!logStream_.is_open()) {
	  throw std::runtime_error("ログファイルを開けません: " + logFilePath);
   }

   CleanOldLogFiles("logs");

   isInitialized_ = true;
}

void Logger::Log(const std::string& message, LogLevel level, [[maybe_unused]] std::source_location location) {
   if (!isInitialized_) {
	  throw std::runtime_error("Logger::Initialize() が呼び出されていません。");
   }

   std::lock_guard<std::mutex> lock(logMutex_);

   auto now = std::chrono::system_clock::now();
   auto nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
   auto localTime = std::chrono::zoned_time{ std::chrono::current_zone(), nowSeconds };
   std::string timestamp = std::format("{:%Y-%m-%d %H:%M:%S}", localTime);

   std::wstring logMessage = L"[" + ConvertString(timestamp) + L"] [" + GetLogLevelString(level) + L"] " + ConvertString(message) + L"\n";

   OutputDebugStringW(logMessage.c_str());
   WriteToFile(logStream_, ConvertString(logMessage));
}

void Logger::Log(const std::wstring& message, LogLevel level, [[maybe_unused]] std::source_location location) {
   if (!isInitialized_) {
	  throw std::runtime_error("Logger::Initialize() が呼び出されていません。");
   }

   std::lock_guard<std::mutex> lock(logMutex_);

   auto now = std::chrono::system_clock::now();
   auto nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
   auto localTime = std::chrono::zoned_time{ std::chrono::current_zone(), nowSeconds };
   std::string timestamp = std::format("{:%Y-%m-%d %H:%M:%S}", localTime);

   std::wstring logMessage = L"[" + ConvertString(timestamp) + L"] [" + GetLogLevelString(level) + L"] " + message + L"\n";

   OutputDebugStringW(logMessage.c_str());
   WriteToFile(logStream_, ConvertString(logMessage));
}

std::wstring Logger::ConvertString(const std::string& str) {
   if (str.empty()) {
	  return std::wstring();
   }

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
   os << logMessage << std::endl;
}

std::wstring Logger::GetLogLevelString(LogLevel level) {
   switch (level) {
	  case LogLevel::Info:    return L"INFO   ";
	  case LogLevel::Warning: return L"WARNING";
	  case LogLevel::Error:   return L"ERROR  ";
	  default:                return L"UNKNOWN";
   }
}

void Logger::CleanOldLogFiles(const std::string& logDir) {
   std::vector<std::filesystem::path> logFiles;

   // ディレクトリ内のログファイルを取得
   for (const auto& entry : std::filesystem::directory_iterator(logDir)) {
	  if (entry.is_regular_file() && entry.path().extension() == ".log") {
		 logFiles.push_back(entry.path());
	  }
   }

   // ファイルを作成日時でソート（古い順）
   std::sort(logFiles.begin(), logFiles.end(), [](const std::filesystem::path& a, const std::filesystem::path& b) {
	  return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
	  }
   );

   // ログファイルが指定数を超えている場合、古いファイルを削除
   while (logFiles.size() > kMaxLogFiles) {
	  std::wcout << L"Deleting old log file: " << logFiles.front() << std::endl;
	  std::filesystem::remove(logFiles.front());  // 古いファイル削除
	  logFiles.erase(logFiles.begin());  // 削除したファイルをリストから削除
   }
}
