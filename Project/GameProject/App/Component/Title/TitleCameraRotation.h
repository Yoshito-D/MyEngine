#pragma once

#include "GameEngine/Scene/Camera/Core/ICinemachineComponent.h"
#include "GameEngine/Math/Functions/MathConstants.h"

namespace App {

/// @brief タイトル背景のカメラを、位置を保ったままワールドY軸周りに回転させる。
class TitleCameraRotation final : public GameEngine::ICinemachineComponent {
public:
   /// @brief 経過時間に応じてカメラの回転を進める。
   /// @param state 回転を反映するカメラ状態。
   /// @param deltaTime フレームの経過秒数。非有限値と0以下の値では更新しない。
   void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;

   /// @brief 姿勢を計算するAimステージを返す。
   GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Aim; }

   /// @brief カメラコンポーネントの登録名を返す。
   const char* GetComponentName() const override { return "TitleCameraRotation"; }

   /// @brief 回転速度をJSONへ保存する。
   nlohmann::json Serialize() const override;

   /// @brief JSONから回転速度を読み込み、不正な値では現在値を維持する。
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief 回転速度をインスペクターで編集する。
   void DrawInspector() override;
#endif

private:
   float rotationSpeed_ = 3.0f * GameEngine::MathConstants::kDegreesToRadians; ///< ワールドY軸周りの回転速度（rad/s）。
};

} // namespace App
