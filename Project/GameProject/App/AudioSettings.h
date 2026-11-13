#pragma once

#include <filesystem>

namespace GameEngine {
class Audio;
}

namespace App {
/// @brief タイム記録と同じ保存フォルダーに置く、ユーザー音量設定の保存先。
inline constexpr const char* kAudioSettingsFile = "Saved/audio_settings.json";

/// @brief 全体・BGM・SE音量を読み込む。ファイルがない場合や不正な場合は現在の音量を保つ。
/// @param audio 読み込んだ0～1の音量を適用する音声サービス。
/// @param file 読み込むJSONのパス。
/// @return 読み込みに成功した場合、または初回起動でファイルがない場合はtrue。
bool LoadAudioSettings(GameEngine::Audio& audio, const std::filesystem::path& file = kAudioSettingsFile);

/// @brief 全体・BGM・SEの設定音量を一時ファイル経由で保存する。BGMの一時的な減衰率は保存しない。
/// @param audio 設定した0～1の音量を取得する音声サービス。
/// @param file 保存するJSONのパス。親フォルダーは必要に応じて作成する。
/// @return 保存先の置換まで成功した場合はtrue。失敗時は既存のファイルを保持する。
bool SaveAudioSettings(const GameEngine::Audio& audio, const std::filesystem::path& file = kAudioSettingsFile);
}
