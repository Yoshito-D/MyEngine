#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <ostream>
#include <source_location>
#include <string>
#include <Windows.h>

class Logger final {
public:
   /// @brief 保持するログ実行フォルダ数
   const size_t kMaxLogFiles = 10;

public:
   /// @brief ログの重要度
   enum class LogLevel {
      Trace,
      Debug,
      Info,
      Warning,
      Error,
      Critical
   };

   /// @brief ログの出力チャンネル
   enum class LogChannel {
      Engine,
      Game,
      Editor
   };

   /// @brief インスタンスの生成
   /// @return インスタンス
   static Logger& GetInstance();

   /// @brief ログファイル出力を初期化する
   static void Initialize();

   /// @brief Traceログを出力する
   /// @param message メッセージ
   /// @param channel 出力チャンネル
   /// @param location 呼び出し元情報
   template <class Message>
   static void Trace(const Message& message, LogChannel channel = LogChannel::Engine, std::source_location location = std::source_location::current()) {
      WriteLogEntry(message, LogLevel::Trace, channel, location);
   }

   /// @brief Debugログを出力する
   /// @param message メッセージ
   /// @param channel 出力チャンネル
   /// @param location 呼び出し元情報
   template <class Message>
   static void Debug(const Message& message, LogChannel channel = LogChannel::Engine, std::source_location location = std::source_location::current()) {
      WriteLogEntry(message, LogLevel::Debug, channel, location);
   }

   /// @brief Infoログを出力する
   /// @param message メッセージ
   /// @param channel 出力チャンネル
   /// @param location 呼び出し元情報
   template <class Message>
   static void Info(const Message& message, LogChannel channel = LogChannel::Engine, std::source_location location = std::source_location::current()) {
      WriteLogEntry(message, LogLevel::Info, channel, location);
   }

   /// @brief Warningログを出力する
   /// @param message メッセージ
   /// @param channel 出力チャンネル
   /// @param location 呼び出し元情報
   template <class Message>
   static void Warning(const Message& message, LogChannel channel = LogChannel::Engine, std::source_location location = std::source_location::current()) {
      WriteLogEntry(message, LogLevel::Warning, channel, location);
   }

   /// @brief Errorログを出力する
   /// @param message メッセージ
   /// @param channel 出力チャンネル
   /// @param location 呼び出し元情報
   template <class Message>
   static void Error(const Message& message, LogChannel channel = LogChannel::Engine, std::source_location location = std::source_location::current()) {
      WriteLogEntry(message, LogLevel::Error, channel, location);
   }

   /// @brief Criticalログを出力する
   /// @param message メッセージ
   /// @param channel 出力チャンネル
   /// @param location 呼び出し元情報
   template <class Message>
   static void Critical(const Message& message, LogChannel channel = LogChannel::Engine, std::source_location location = std::source_location::current()) {
      WriteLogEntry(message, LogLevel::Critical, channel, location);
   }

   /// @brief GameチャンネルへInfoログを出力する
   /// @param message メッセージ
   /// @param location 呼び出し元情報
   template <class Message>
   static void GameInfo(const Message& message, std::source_location location = std::source_location::current()) {
      Info(message, LogChannel::Game, location);
   }

   /// @brief EngineチャンネルへWarningログを出力する
   /// @param message メッセージ
   /// @param location 呼び出し元情報
   template <class Message>
   static void EngineWarning(const Message& message, std::source_location location = std::source_location::current()) {
      Warning(message, LogChannel::Engine, location);
   }

   /// @brief ログをログファイルに書き出す
   /// @param message メッセージ
   /// @param level ログレベル
   /// @param location 呼び出し元情報
   void Log(const std::string& message, LogLevel level = LogLevel::Info, std::source_location location = std::source_location::current());

   /// @brief ログをログファイルに書き出す
   /// @param message メッセージ
   /// @param level ログレベル
   /// @param location 呼び出し元情報
   void Log(const std::wstring& message, LogLevel level = LogLevel::Info, std::source_location location = std::source_location::current());

   /// @brief 指定チャンネルのログファイルにログを書き出す
   /// @param message メッセージ
   /// @param level ログレベル
   /// @param channel 出力チャンネル
   /// @param location 呼び出し元情報
   void Log(const std::string& message, LogLevel level, LogChannel channel, std::source_location location = std::source_location::current());

   /// @brief 指定チャンネルのログファイルにログを書き出す
   /// @param message メッセージ
   /// @param level ログレベル
   /// @param channel 出力チャンネル
   /// @param location 呼び出し元情報
   void Log(const std::wstring& message, LogLevel level, LogChannel channel, std::source_location location = std::source_location::current());

   // ↓ Utilityクラスなどをつくって移動させる //

   /// @brief 文字列変換
   /// @param str 文字列
   /// @return wstring
   static std::wstring ConvertString(const std::string& str);

   /// @brief 文字列変換
   /// @param str 文字列
   /// @return string
   static std::string ConvertString(const std::wstring& str);

   /// @brief stringをwstringに変換する
   /// @param str 文字列
   /// @return wstring
   static std::wstring ConvertStringToWString(const std::string& str);

private:
   static constexpr size_t kLogChannelCount = 3;

   std::array<std::ofstream, kLogChannelCount> channelStreams_;
   std::ofstream allLogStream_;
   std::filesystem::path currentLogDirectory_;
   std::mutex logMutex_;
   bool isInitialized_ = false;
private:
   Logger() = default;
   ~Logger() = default;
   Logger(const Logger&) = delete;
   const Logger& operator=(const Logger&) = delete;

   static void WriteLogEntry(const std::string& message, LogLevel level, LogChannel channel, std::source_location location);
   static void WriteLogEntry(const std::wstring& message, LogLevel level, LogChannel channel, std::source_location location);

   void InitializeInternal();

   /// @brief ファイルに書き出す
   /// @param ファイル 
   /// @param メッセージ 
   void WriteToFile(std::ostream& os, const std::string& logMessage);

   void CloseStreams();
   void OpenChannelLogFile(LogChannel channel, const std::filesystem::path& filePath);
   void WriteLog(const std::wstring& message, LogLevel level, LogChannel channel, std::source_location location);
   size_t GetLogChannelIndex(LogChannel channel) const;
   std::ofstream& GetLogChannelStream(LogChannel channel);
   std::wstring FormatLogMessage(const std::wstring& message, LogLevel level, LogChannel channel, std::source_location location);
   std::string GetSourceLocationString(std::source_location location) const;

   /// @brief ログレベルを文字列に変換 
   /// @param ログレベル 
   /// @return ログレベル
   std::wstring GetLogLevelString(LogLevel level);

   std::wstring GetLogChannelString(LogChannel channel);

   /// @brief 古いログファイルを削除
   /// @param 日時
   void CleanOldLogFiles(const std::string& logDir);
};
