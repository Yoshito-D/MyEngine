#include "GameProject/App/Component/Race/RaceRecordStore.h"

#include "GameProject/App/Component/Race/RaceTime.h"

#include "GameEngine/Utility/JsonFile.h"
#include "GameEngine/Utility/Logger.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace App {
namespace {

constexpr std::int32_t kRecordFormatVersion = 1;
constexpr std::int32_t kJsonIndentSize = 3;

void InsertRankedTime(std::vector<float>& bestTimes, float timeSeconds) {
   bestTimes.push_back(timeSeconds);

   // 保存・表示の双方が先頭から最速順を仮定するため、追加時点で固定件数へ正規化する。
   std::sort(bestTimes.begin(), bestTimes.end());

   if (bestTimes.size() > RaceRecordStore::kBestTimeCount) {
      bestTimes.resize(RaceRecordStore::kBestTimeCount);
   }
}

nlohmann::json ReadRecordFile(const std::filesystem::path& path) {
   if (path.empty()) {
      throw std::invalid_argument("recordFile is empty");
   }

   if (!std::filesystem::exists(path)) {
      // 初回起動は記録なしが正常なので、空ランキングのまま成功として扱う。
      return {{"version", kRecordFormatVersion}, {"races", nlohmann::json::object()}};
   }

   std::ifstream input(path);

   if (!input.is_open()) {
      throw std::runtime_error("could not open record file");
   }

   auto root = nlohmann::json::parse(input);

   if (!root.is_object() || !root.contains("version") || !root.at("version").is_number_integer() ||
       root.at("version") != kRecordFormatVersion || !root.contains("races") || !root.at("races").is_object()) {
      throw std::runtime_error("invalid record format version or races object");
   }

   return root;
}

std::vector<float> ReadCourseTimes(const nlohmann::json& races, const std::string& key) {
   // 1ファイルを複数コースで共有するため、現在のrecordKeyだけを取り出す。
   const auto race = races.find(key);

   if (race == races.end()) {
      return {};
   }

   if (!race->is_object() || !race->contains("bestTimes") || !race->at("bestTimes").is_array()) {
      throw std::runtime_error("invalid bestTimes");
   }

   std::vector<float> times;

   for (const auto& value : race->at("bestTimes")) {
      if (!value.is_number()) {
         throw std::runtime_error("invalid race time");
      }

      // 狭める前に、順位表示のミリ秒へ変換できる範囲を確認する。
      const double seconds = value.get<double>();

      if (seconds <= 0.0 || !IsRaceTimeRepresentable(seconds)) {
         throw std::runtime_error("invalid race time");
      }

      const float time = static_cast<float>(seconds);

      if (time <= 0.0f || !IsRaceTimeRepresentable(time)) {
         throw std::runtime_error("race time is outside the supported range after conversion");
      }

      InsertRankedTime(times, time);
   }

   return times;
}

}

bool RaceRecordStore::AddBestTime(float timeSeconds, const std::string& file, const std::string& key) {
   if (timeSeconds <= 0.0f || !IsRaceTimeRepresentable(timeSeconds)) {
      LogRecordError(file, "race time is outside the supported range for " + key);
      return false;
   }

   if (key.empty()) {
      InsertRankedTime(bestTimes_, timeSeconds);
      return true;
   }

   try {
      const std::filesystem::path path(file);
      // 他コースの記録を保持し、ロード後に保存済みの順位が変わっていても最新の内容に追加する。
      auto root = ReadRecordFile(path);
      auto& races = root.at("races");
      auto times = ReadCourseTimes(races, key);
      InsertRankedTime(times, timeSeconds);
      races[key]["bestTimes"] = times;

      // 一時ファイルの置換に成功した順位だけをリザルトへ反映する。
      if (!GameEngine::SaveJsonFileAtomically(path, root, kJsonIndentSize)) {
         LogRecordError(file, "could not save records for " + key);
         return false;
      }

      bestTimes_ = std::move(times);
      return true;
   } catch (const std::exception& exception) {
      LogRecordError(file, "could not update records for " + key + ": " + exception.what());
      return false;
   }
}

bool RaceRecordStore::LoadBestTimes(const std::string& file, const std::string& key) {
   if (key.empty()) {
      bestTimes_.clear();
      return true;
   }

   try {
      const auto root = ReadRecordFile(std::filesystem::path(file));
      bestTimes_ = ReadCourseTimes(root.at("races"), key);
      return true;
   } catch (const std::exception& exception) {
      LogRecordError(file, "failed to load records for " + key + ": " + exception.what());
      return false;
   }
}

void RaceRecordStore::LogRecordError(const std::string& file, const std::string& message) {
   Logger::Error("Race record: " + message + " [" + file + "]", Logger::LogChannel::Game);
}

}
