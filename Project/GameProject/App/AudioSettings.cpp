#include "GameProject/App/AudioSettings.h"
#include "GameEngine/Audio/Audio.h"
#include "GameEngine/Utility/JsonFile.h"
#include "GameEngine/Utility/Logger.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {
constexpr std::int32_t kAudioSettingsVersion = 1;

float ReadVolume(const nlohmann::json& settings, const char* key) {
   const auto& value = settings.at(key);

   if (!value.is_number()) {
      throw std::invalid_argument(std::string("volume is not a number: ") + key);
   }

   const float volume = value.get<float>();

   if (!std::isfinite(volume) || volume < 0.0f || volume > 1.0f) {
      throw std::invalid_argument(std::string("volume is outside 0..1: ") + key);
   }

   return volume;
}
}

namespace App {
bool LoadAudioSettings(GameEngine::Audio& audio, const std::filesystem::path& file) {
   try {
      // 初回起動では既定の音量を使い、設定を変更するまでは保存ファイルを作らない。
      if (!std::filesystem::exists(file)) {
         return true;
      }

      std::ifstream input(file);

      if (!input.is_open()) {
         throw std::runtime_error("could not open settings file");
      }

      nlohmann::json settings;
      input >> settings;

      if (!settings.is_object() || !settings.contains("version") || !settings.at("version").is_number_integer() ||
          settings.at("version") != kAudioSettingsVersion) {
         throw std::invalid_argument("invalid settings format");
      }

      // 全項目を検証してから適用し、一部だけ読み込まれた音量で起動することを防ぐ。
      const float master = ReadVolume(settings, "master");
      const float bgm = ReadVolume(settings, "bgm");
      const float se = ReadVolume(settings, "se");
      audio.SetMasterVolume(master);
      audio.SetBusVolume(GameEngine::AudioBus::Bgm, bgm);
      audio.SetBusVolume(GameEngine::AudioBus::Se, se);
      return true;
   } catch (const std::exception& exception) {
      Logger::Warning("Audio settings could not be loaded: " + file.generic_string() + " (" + exception.what() + ")",
         Logger::LogChannel::Game);
      return false;
   }
}

bool SaveAudioSettings(const GameEngine::Audio& audio, const std::filesystem::path& file) {
   const nlohmann::json settings{{"version", kAudioSettingsVersion}, {"master", audio.GetMasterVolume()},
      {"bgm", audio.GetBusVolume(GameEngine::AudioBus::Bgm)}, {"se", audio.GetBusVolume(GameEngine::AudioBus::Se)}};

   if (GameEngine::SaveJsonFileAtomically(file, settings)) {
      return true;
   }

   Logger::Warning("Audio settings could not be saved: " + file.generic_string(), Logger::LogChannel::Game);
   return false;
}
}
