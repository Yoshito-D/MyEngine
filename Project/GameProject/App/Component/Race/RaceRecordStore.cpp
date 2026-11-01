#include "GameProject/App/Component/Race/RaceRecordStore.h"
#include "GameEngine/Utility/Logger.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#include <Windows.h>
#endif
namespace App {
namespace {
constexpr std::int32_t kRecordFormatVersion = 1;
constexpr std::int32_t kJsonIndentSize = 3;
}

void RaceRecordStore::AddBestTime(float timeSeconds) {
   if (!std::isfinite(timeSeconds) || timeSeconds <= 0.0f) return;

   bestTimes_.push_back(timeSeconds);

   // 保存・表示の双方が先頭から最速順を仮定するため、追加時点で固定件数へ正規化する。
   std::sort(bestTimes_.begin(), bestTimes_.end());

   if (bestTimes_.size() > kBestTimeCount) {
      bestTimes_.resize(kBestTimeCount);
   }
}

bool RaceRecordStore::LoadBestTimes(const std::string& file, const std::string& key) {
   bestTimes_.clear();

   if (file.empty() || key.empty()) {
      return true;
   }

   const std::filesystem::path recordPath(file);

   if (!std::filesystem::exists(recordPath)) {
      // 初回起動は記録なしが正常なので、空ランキングのまま成功として扱う。
      return true;
   }

   try {
      std::ifstream input(recordPath);

      if (!input.is_open()) {
         LogRecordWarning(file, "could not open record file");
         return false;
      }

      nlohmann::json root;
      input >> root;

      if (!root.is_object() || !root.contains("races") || !root.at("races").is_object()) {
         LogRecordWarning(file, "record file schema is invalid");
         return false;
      }

      const auto& races = root.at("races");

      // 1ファイルを複数コースで共有するため、現在のrecordKeyだけを取り出す。
      if (!races.contains(key) || !races.at(key).is_object()) {
         return true;
      }

      const auto& raceData = races.at(key);

      if (!raceData.contains("bestTimes") || !raceData.at("bestTimes").is_array()) {
         return true;
      }

      for (const auto& value : raceData.at("bestTimes")) {
         if (value.is_number()) {
            AddBestTime(value.get<float>());
         }
      }

      return true;
   } catch (const std::exception& exception) {
      LogRecordWarning(file, std::string("failed to load records: ") + exception.what());
      bestTimes_.clear();
      return false;
   }
}

bool RaceRecordStore::SaveBestTimes(const std::string& file, const std::string& key) const {
   if (file.empty() || key.empty()) {
      LogRecordWarning(file, "recordFile or recordKey is empty");
      return false;
   }

   const std::filesystem::path recordPath(file);
   nlohmann::json root = nlohmann::json::object();

   if (std::filesystem::exists(recordPath)) {
      // 他コースの記録を保持するため、既存JSONを土台に現在キーだけを更新する。
      try {
         std::ifstream input(recordPath);

         if (input.is_open()) {
            input >> root;
         }
      } catch (const std::exception& exception) {
         LogRecordWarning(file, std::string("existing record file will be replaced: ") + exception.what());
         root = nlohmann::json::object();
      }
   }

   if (!root.is_object()) {
      root = nlohmann::json::object();
   }

   root["version"] = kRecordFormatVersion;

   if (!root.contains("races") || !root.at("races").is_object()) {
      root["races"] = nlohmann::json::object();
   }

   root["races"][key]["bestTimes"] = bestTimes_;

   std::error_code error;

   if (!recordPath.parent_path().empty()) {
      std::filesystem::create_directories(recordPath.parent_path(), error);

      if (error) {
         LogRecordWarning(file, "could not create record directory: " + error.message());
         return false;
      }
   }

   std::filesystem::path temporaryPath = recordPath;
   temporaryPath += ".tmp";

   // 書き込み途中の本番ファイルを残さないよう、一時ファイルを完成させてから置換する。
   try {
      std::ofstream output(temporaryPath, std::ios::out | std::ios::trunc);

      if (!output.is_open()) {
         LogRecordWarning(file, "could not open temporary record file");
         return false;
      }

      output << root.dump(kJsonIndentSize) << '\n';
      output.close();

      if (!output) {
         LogRecordWarning(file, "could not finish writing temporary record file");
         return false;
      }
   } catch (const std::exception& exception) {
      LogRecordWarning(file, std::string("failed to write records: ") + exception.what());
      return false;
   }

#ifdef _WIN32
   // Windowsでは置換とディスク反映を同じAPIで要求し、既存ファイルも安全に更新する。
   if (!MoveFileExW(temporaryPath.c_str(), recordPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      LogRecordWarning(file, "could not replace record file (Win32 error " + std::to_string(GetLastError()) + ")");
      std::filesystem::remove(temporaryPath, error);
      return false;
   }
#else
   std::filesystem::rename(temporaryPath, recordPath, error);

   if (error) {
      LogRecordWarning(file, "could not replace record file: " + error.message());
      std::filesystem::remove(temporaryPath, error);
      return false;
   }
#endif
   return true;
}

void RaceRecordStore::LogRecordWarning(const std::string& file, const std::string& message) {
   Logger::Warning("Race record: " + message + " [" + file + "]", Logger::LogChannel::Game);
}
}
