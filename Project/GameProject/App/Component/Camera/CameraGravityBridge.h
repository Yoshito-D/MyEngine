#pragma once
#include "GameProject/App/Component/Camera/CameraPresentationCapture.h"
#include "GameProject/App/Component/Camera/VehicleCameraEffects.h"

#include <cmath>
#include <algorithm>

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"
#include "GameEngine/Math/Types/Vector4.h"
#include "GameEngine/Math/Types/Quaternion.h"
#include "GameProject/App/Component/Camera/RearFollow/PlayerRearFollowCamera.h"
#include <cstdint>
#include <vector>

namespace App {
class GravityBody;

/// @brief 自身の位置・重力Up方向をカメラ系コンポーネントへ橋渡しするコンポーネント
class CameraGravityBridge final : public GameEngine::IObjectComponent {
public:
   /// @brief 依存する状態の更新後に処理するためのフェーズを返す。
   GameEngine::ObjectUpdatePhase GetUpdatePhase() const override { return GameEngine::ObjectUpdatePhase::Presentation; }

   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "CameraGravityBridge";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"カメラ重力ブリッジ", "Camera Gravity Bridge"};

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief オーナー位置から重力Upを算出し、接続先カメラへ反映する
   void Update(float) override;

   /// @brief JSONに保存されたカメラ名から通知先を解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 参照設定の変更後に、指定ワールドから依存先を解決し直す。
   /// @param sceneWorld 参照先を検索するシーン。
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 惑星中心座標を設定する
   void SetPlanetCenter(const GameEngine::Vector3& center) { planetCenter_ = center; }

#ifdef USE_IMGUI

   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief シリアライズ
   nlohmann::json Serialize() const override;

   /// @brief デシリアライズ
   void Deserialize(const nlohmann::json& data) override;

   /// @brief シーン固有の参照と実行履歴を除く、共有可能な設定JSONを返す。
   nlohmann::json GetSharedParameters() const override;

   /// @brief 実行履歴を維持したまま共有設定を適用する。
   /// @param data 適用する共有設定JSON。
   void ApplySharedParameters(const nlohmann::json& data) override;

   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings : VehicleCameraEffectsSettings, CameraPresentationCaptureSettings {};

   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const;

   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested);

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   Settings settings_;
   VehicleCameraEffects effects_;
   CameraPresentationCapture presentation_;

   /// @brief 重力方向を算出するための惑星中心
   GameEngine::Vector3 planetCenter_ = {0.0f, 0.0f, 0.0f};

   /// @brief プレイヤー後方追従カメラへの通知先
   PlayerRearFollowCamera* playerRearFollowCamera_ = nullptr;
   std::vector<IVehicleCamera*> cameras_; ///< 共通能力を持つ追従カメラへの非所有参照。参照変更時に再解決する。
   /// @brief 通知先の安定カメラID。具象型を設定形式へ持ち込まない。
   std::vector<std::string> cameraIds_{"GravityFollowCamera", "PlayerRearFollowCamera", "PlanetLeashCamera"};
};

} // namespace App
