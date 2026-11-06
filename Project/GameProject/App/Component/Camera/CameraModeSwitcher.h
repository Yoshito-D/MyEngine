#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <cstddef>
#include <string>
#include <vector>

namespace GameEngine {
class VirtualCamera;
}

namespace App {

/// @brief 入力に応じてJSON定義された仮想カメラの優先度を切り替える
class CameraModeSwitcher final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "CameraModeSwitcher"; ///< コンポーネント型名。
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"カメラモード切替", "Camera Mode Switcher"};

   /// @brief コンポーネント型名を取得する
   /// @return CameraModeSwitcher
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief JSONのカメラIDを実体へ解決して初期モードを適用する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 保存されたID順に仮想カメラ参照を再解決し、選択中カメラと車両制御の接続を更新する。
   /// @param sceneWorld 参照の解決先となる現在のシーンワールド。
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 非ReleaseビルドではTab入力で次のカメラへ切り替える
   void Update(float) override;

   /// @brief JSONで登録されたIDを指定してカメラを切り替える
   /// @param cameraId 切り替え先の仮想カメラIDまたは名前
   /// @return カメラが見つかり切り替えられた場合はtrue
   bool SwitchToCamera(const std::string& cameraId);

   /// @brief 選択中の仮想カメラを返す。未解決の場合はnullptr。
   GameEngine::VirtualCamera* GetSelectedCamera();

   /// @brief constコンポーネントの選択結果を読み取り専用で参照する。
   const GameEngine::VirtualCamera* GetSelectedCamera() const;

   /// @brief カメラ順序と初期モードをJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONからカメラ順序と初期モードを読み込む
   /// @param data カメラ切り替え設定
   void Deserialize(const nlohmann::json& data) override;

   /// @brief カメラ切り替えの共有可能な調整値をJSONで取得する。
   /// @return 参照カメラIDを除いた切り替え条件を含む共有設定JSON。
   nlohmann::json GetSharedParameters() const override;

   /// @brief カメラ切り替えの共有設定を既存コンポーネントへ反映する。
   /// @param data 参照カメラIDを除いた切り替え条件を含む共有設定JSON。
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief 現在のカメラモードを表示する
   void DrawInspector() override;
#endif

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   void ApplyMode();

   std::vector<std::string> cameraIds_;              ///< 切り替え候補の仮想カメラIDを順番に保持する一覧。
   std::vector<GameEngine::VirtualCamera*> cameras_; ///< 安定IDから解決した切り替え候補の非所有カメラ参照。
   size_t initialIndex_ = 0;                         ///< 開始時に選択するカメラの候補番号。
   size_t currentIndex_ = 0;                         ///< 現在選択中インデックス。
};

} // namespace App
