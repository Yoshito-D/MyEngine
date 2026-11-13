#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <functional>
#include <initializer_list>
#include <string>

namespace GameEngine {
class Audio;
}

namespace App {
/// @brief チュートリアルとゲームで共通のポーズ画面を接続し、ユーザー音量設定を読み書きする。
class PauseMenuComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "PauseMenuComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"ポーズメニュー", "Pause Menu"};

   /// @brief 全体・BGM・SE音量を読み込む。ファイルがない場合や不正な場合は現在の音量を保つ。
   /// @details ポーズ画面を持たないタイトルの音声にも反映するため、シーン読込前に呼び出す。
   /// @param audio 読み込んだ0～1の音量を適用する音声サービス。
   /// @return 読み込みに成功した場合、または初回起動でファイルがない場合はtrue。
   static bool LoadAudioSettings(GameEngine::Audio& audio);

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

   /// @brief タイトルへの遷移先だけを保存する。実行中の画面IDと音量は保存しない。
   nlohmann::json Serialize() const override;

   /// @brief タイトルへの遷移先を復元する。
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief タイトルへの遷移先をシーン参照として編集する。
   void DrawInspector() override;
#endif

private:
   /// @brief 全体・BGM・SEの設定音量を一時ファイル経由で保存する。BGMの一時的な減衰率は保存しない。
   /// @param audio 設定した0～1の音量を取得する音声サービス。
   /// @return 保存先の置換まで成功した場合はtrue。失敗時は既存のファイルを保持する。
   static bool SaveAudioSettings(const GameEngine::Audio& audio);

   bool IsPresentationActive() const;
   bool CreatePauseScreen();
   void OpenSettings();
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
};
}
