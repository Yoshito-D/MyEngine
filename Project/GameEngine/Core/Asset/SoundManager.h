#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>
#include <filesystem>
#include "Audio/Sound.h"
#include "Audio/SoundClip.h"

namespace GameEngine {
class Audio;

/// @brief サウンドマネージャークラス
class SoundManager {
public:
   /// @brief サウンドマネージャーの初期化
   /// @param audio オーディオシステム
   void Initialize(Audio* audio);

   /// @brief サウンドをロード
   /// @param filePath サウンドファイルのパス
   /// @param name サウンドの名前
   void LoadSound(const std::string& filePath, const std::string& name);

   /// @brief サウンドを取得
   /// @param name 取得するサウンドの名前
   /// @return サウンドへのポインタ（存在しない場合は nullptr）
   Sound* GetSound(const std::string& name);

   /// @brief サウンドを全削除
   void Clear();

   /// @brief Resolve and decode a Resources-relative UTF-8 asset ID once.
   /// @return Shared PCM clip, or nullptr for an invalid or unreadable asset.
   std::shared_ptr<const SoundClip> GetClip(const std::string& assetId);

   /// @brief Normalize a UTF-8 Resources-relative path, rejecting traversal.
   static std::string NormalizeAssetId(const std::string& assetId);

private:
   Audio* audio_ = nullptr;
   std::unordered_map<std::string, std::unique_ptr<Sound>> sounds_;
   std::unordered_map<std::string, std::shared_ptr<const SoundClip>> clips_;
   std::unordered_map<std::string, std::optional<std::filesystem::file_time_type>> failedClips_;
};
}
