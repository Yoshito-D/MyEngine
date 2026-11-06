#pragma once

#include "GameEngine/Math/Types/Vector3.h"

namespace GameEngine {
class ICinemachineComponent;
}

namespace App {

/// @brief 車両カメラが共通で受け取る追従入力。位置はm、速度はm/s。
struct VehicleCameraFollowInput {
   GameEngine::Vector3 gravityUp{0.0f, 1.0f, 0.0f}; ///< 重力方向と逆向きの単位ベクトル。
   GameEngine::Vector3 target{};                    ///< 追従対象のワールド位置（m）。
   GameEngine::Vector3 planetCenter{};              ///< 追従の基準となる惑星の中心（m）。
   GameEngine::Vector3 landingPlanetCenter{};       ///< 空中時に予測対象となる惑星の中心（m）。
   float speed = 0.0f;                              ///< 現在速度（m/s）。
   float autoSpeed = 13.0f;                         ///< 通常走行速度（m/s）。
};

/// @brief 保存されたカメラ設定とは別に適用する、一フレーム分の演出入力。
struct VehicleCameraEffectInput {
   float distanceScale = 1.0f;          ///< 基準距離へ掛ける倍率。
   float cameraHeightOffset = 0.0f;     ///< カメラ高さへ加える量（m）。
   float lookTargetHeightOffset = 0.0f; ///< 注視点高さへ加える量（m）。
};

/// @brief 追従と車両演出の共通能力。着地予測など各カメラ固有の機能は含めない。
class IVehicleCamera {
public:
   /// @brief 派生カメラを安全に破棄する。
   virtual ~IVehicleCamera() = default;

   /// @brief 共通追従入力を各カメラの内部表現へ変換する。
   virtual void ApplyVehicleFollowInput(const VehicleCameraFollowInput& input) = 0;

   /// @brief 距離と高さの一時演出を各カメラの内部表現へ変換する。
   virtual void ApplyVehicleEffectInput(const VehicleCameraEffectInput& input) = 0;

   /// @brief 選択確認とシェイク通知に使う、生存中のカメラ部品を返す。
   virtual GameEngine::ICinemachineComponent& GetVehicleCameraComponent() = 0;

   /// @brief 直近の計算結果のカメラUp方向を返す。
   virtual GameEngine::Vector3 GetVehicleCameraUp() const = 0;
};

} // namespace App
