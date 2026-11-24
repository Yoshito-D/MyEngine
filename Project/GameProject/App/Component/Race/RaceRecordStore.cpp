#include "GameProject/App/Component/Race/RaceRecordStore.h"
#include "GameEngine/Utility/JsonFile.h"
#include "GameEngine/Utility/Logger.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
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
         LogRecordError(file, "could not open record file");
         return false;
      }

      nlohmann::json root;
      input >> root;

      if (!root.is_object() || !root.contains("races") || !root.at("races").is_object()) {
         LogRecordError(file, "record file schema is invalid");
         return false;
      }

      const auto& races = root.at("races");

      // 1ファイルを複数コースで共有するため、現在のrecordKeyだけを取り出す。
      if (!races.contains(key)) {
         return true;
      }

      const auto& raceData = races.at(key);

      if (!raceData.is_object() || !raceData.contains("bestTimes") || !raceData.at("bestTimes").is_array()) {
         throw std::runtime_error("invalid bestTimes for " + key);
      }

      for (const auto& value : raceData.at("bestTimes")) {
         if (!value.is_number() || !std::isfinite(value.get<float>()) || value.get<float>() <= 0.0f) {
            throw std::runtime_error("invalid race time for " + key);
         }

         AddBestTime(value.get<float>());
      }

      return true;
   } catch (const std::exception& exception) {
      LogRecordError(file, std::string("failed to load records: ") + exception.what());
      bestTimes_.clear();
      return false;
   }
}

bool RaceRecordStore::SaveBestTimes(const std::string& file, const std::string& key) const {
   if (file.empty() || key.empty()) {
      LogRecordError(file, "recordFile or recordKey is empty");
      return false;
   }

   const std::filesystem::path recordPath(file);
   nlohmann::json root = nlohmann::json::object();

   if (std::filesystem::exists(recordPath)) {
      // 他コースの記録を保持するため、既存JSONを土台に現在キーだけを更新する。
      try {
         std::ifstream input(recordPath);

         if (!input.is_open()) {
            throw std::runtime_error("could not open existing record file");
         }

         input >> root;

         if (!root.is_object() || !root.contains("races") || !root.at("races").is_object()) {
            throw std::runtime_error("invalid existing record schema");
         }
      } catch (const std::exception& exception) {
         LogRecordError(file, std::string("could not update existing records: ") + exception.what());
         return false;
      }
   }

   root["version"] = kRecordFormatVersion;

   if (!root.contains("races")) {
      root["races"] = nlohmann::json::object();
   }

   root["races"][key]["bestTimes"] = bestTimes_;

   // 書き込み途中の本番ファイルを残さないよう、一時ファイルを完成させてから置換する。
   if (!GameEngine::SaveJsonFileAtomically(recordPath, root, kJsonIndentSize)) {
      LogRecordError(file, "could not save records");
      return false;
   }

   return true;
}

void RaceRecordStore::LogRecordError(const std::string& file, const std::string& message) {
   Logger::Error("Race record: " + message + " [" + file + "]", Logger::LogChannel::Game);
}
}
