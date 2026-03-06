#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "Audio/Sound.h"

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

private:
   Audio* audio_ = nullptr;
   std::unordered_map<std::string, std::unique_ptr<Sound>> sounds_;
};
}
