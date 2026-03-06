#pragma once
#include <iostream>
#include <filesystem>
#include <fstream>
#include <Windows.h>
#include <mutex>
#include <chrono>
#include <source_location>

class Logger final {
public:
   const size_t kMaxLogFiles = 10;

public:
   enum class LogLevel {
	  Info,
	  Warning,
	  Error
   };

   /// @brief インスタンスの生成
   /// @return インスタンス
   static Logger& GetInstance();

   void Initialize();

   /// @brief ログをログファイルに書き出す
   /// @param メッセージ
   /// @param ログレベル 
   void Log(const std::string& message, LogLevel level = LogLevel::Info, std::source_location location = std::source_location::current());

   /// @brief ログをログファイルに書き出す
   /// @param メッセージ
   /// @param ログレベル 
   void Log(const std::wstring& message, LogLevel level = LogLevel::Info, std::source_location location = std::source_location::current());

   // ↓ Utilityクラスなどをつくって移動させる //

   /// @brief 文字列変換
   /// @param 文字列 
   /// @return wstring
   std::wstring ConvertString(const std::string& str);

   /// @brief 文字列変換
   /// @param 文字列 
   /// @return string
   std::string ConvertString(const std::wstring& str);

   std::wstring ConvertStringToWString(const std::string& str);

private:
   std::ofstream logStream_;
   std::mutex logMutex_;
   bool isInitialized_ = false;
private:
   Logger() = default;
   ~Logger() = default;
   Logger(const Logger&) = delete;
   const Logger& operator=(const Logger&) = delete;

   /// @brief ファイルに書き出す
   /// @param ファイル 
   /// @param メッセージ 
   void WriteToFile(std::ostream& os, const std::string& logMessage);

   /// @brief ログレベルを文字列に変換 
   /// @param ログレベル 
   /// @return ログレベル
   std::wstring GetLogLevelString(LogLevel level);

   /// @brief 古いログファイルを削除
   /// @param 日時
   void CleanOldLogFiles(const std::string& logDir);
};

