#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <functional>
#include <initializer_list>
#include <string>

namespace GameEngine {
class Audio;
}

namespace App {
/// @brief ポーズ画面と、各シーンで共通の音量設定画面を接続する。
class PauseMenuComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "PauseMenuComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"ポーズメニュー", "Pause Menu"};

   /// @brief 全体・BGM・SE音量を読み込む。ファイルがない場合や不正な場合は現在の音量を保つ。
   /// @details ポーズ画面を持たないタイトルの音声にも反映するため、シーン読込前に呼び出す。
   /// @param audio 読み込んだ0～1の音量を適用する音声サービス。
   /// @note 初回起動では既定値を使い、既存ファイルの読み込みに失敗した場合はログを記録して例外にする。
   static void LoadAudioSettings(GameEngine::Audio& audio);

   /// @brief 共通の音量設定を開く。表示要求が受理された場合はtrueを返す。
   bool OpenSettings();

   /// @brief 音量設定が表示中かを返す。閉じる演出中もtrueを返す。
   bool IsSettingsVisible() const;

   /// @brief 保存と検索に使う型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief ポーズ中も操作を接続できるUI更新区分を返す。
   GameEngine::ObjectUpdateDomain GetUpdateDomain() const override { return GameEngine::ObjectUpdateDomain::UI; }

   /// @brief 所有ワールドを接続し、実行時だけ生成する画面の参照を初期化する。
   void OnSceneLoaded(GameEngine::SceneWorld& world) override;

   /// @brief ポーズ入力を受け付け、共通UIへ画面の表示を要求する。
   void Update(float deltaTime) override;

   /// @brief 生成した画面の削除を要求し、コールバックの参照先を解除する。
   void OnDetach() override;

   /// @brief ポーズ時の遷移先と、設定画面だけを使うかどうかを保存する。
   nlohmann::json Serialize() const override;

   /// @brief ポーズ時の遷移先と、設定画面だけを使うかどうかを復元する。
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief ポーズメニューの使用有無とタイトルへの遷移先を編集する。
   void DrawInspector() override;
#endif

private:
   /// @brief 全体・BGM・SEの設定音量を一時ファイル経由で保存する。BGMの一時的な減衰率は保存しない。
   /// @param audio 設定した0～1の音量を取得する音声サービス。
   /// @return 保存先の置換まで成功した場合はtrue。失敗時は既存のファイルを保持する。
   static bool SaveAudioSettings(const GameEngine::Audio& audio);

   bool IsPresentationActive() const;
   bool CreatePauseScreen();
   void CloseTopScreen();
   void RequestSceneChange(bool restart);
   void BindButton(const std::string& rootId, const char* key, std::function<void()> callback);
   void BindVolumeSlider(const char* key, float volume, std::function<void(float)> callback);
   bool UpdateVolumeText(const std::string& key, float volume);
   void SetMenuContentVisible(bool visible);
   void SetDefaultFocus(const char* key);
   void UpdateSelectionVisuals(const std::string& screenId, std::initializer_list<const char*> keys, float deltaTime);
   void UpdateSelectionSound(const std::string& screenId);
   void PlayUISound(const char* clipAssetId);

   GameEngine::SceneWorld* world_ = nullptr;
   std::string pauseScreenId_;
   std::string settingsScreenId_;
   std::string selectionScreenId_;
   std::string selectedElementId_;
   std::string titleScene_ = "Title";
   bool settingsOnly_ = false; ///< タイトルではポーズ入力を受け付けず、音量設定だけを表示する。
};
}
