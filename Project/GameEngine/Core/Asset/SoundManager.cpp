#include "pch.h"
#include "SoundManager.h"
#include "Audio.h"
#include <cassert>

namespace GameEngine {
void SoundManager::Initialize(Audio* audio) {
   assert(audio != nullptr);
   audio_ = audio;
}

void SoundManager::LoadSound(const std::string& filePath, const std::string& name) {
   // 既にロード済みの場合は何もしない
   if (sounds_.find(name) != sounds_.end()) {
	  return;
   }

   // 新しいサウンドを作成
   auto sound = std::make_unique<Sound>();

   // ワイド文字列に変換
   std::wstring wFilePath(filePath.begin(), filePath.end());

   // サウンドファイルをロード
   sound->Load(wFilePath);

   // マップに追加
   sounds_[name] = std::move(sound);
}

Sound* SoundManager::GetSound(const std::string& name) {
   auto it = sounds_.find(name);
   if (it != sounds_.end()) {
	  return it->second.get();
   }
   return nullptr;
}

void SoundManager::Clear() {
   sounds_.clear();
}
}
