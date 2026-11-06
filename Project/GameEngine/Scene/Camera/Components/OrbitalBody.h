#pragma once

#include "GameEngine/Scene/Camera/Core/ICinemachineComponent.h"
#include "GameEngine/Math/Functions/MathConstants.h"
#include <algorithm>

namespace GameEngine {

/// @brief ターゲットの周りを周回するBodyコンポーネント
/// DebugCameraの動作を再現
class OrbitalBody : public ICinemachineComponent {
public:
   /// @brief 既定の旋回角度と距離で生成する
   OrbitalBody() = default;

   /// @brief カメラコンポーネントを破棄する
   ~OrbitalBody() override = default;

   /// @brief Yaw・Pitch（rad）とピボットから、カメラ位置とビュー行列を再構築する。
   /// @param state 更新するカメラ状態。
   /// @details フレーム時間の引数は使用しない。入力で更新済みの角度と距離から状態を計算する。
   void MutateCameraState(CameraState& state, float) override;

   /// @brief 仮想カメラ内でこの処理を実行するステージを返す。
   /// @return 位置とビュー行列を計算するBodyステージ。
   CinemachineStage GetStage() const override { return CinemachineStage::Body; }

   /// @brief 入力処理
   /// @param mouseDelta マウス移動量
   /// @param wheelDelta マウスホイール量
   /// @param isDragging ドラッグ中かどうか
   /// @param isShiftPressed シフトキーが押されているか
   void ProcessInput(const Vector2& mouseDelta, int32_t wheelDelta, bool isDragging, bool isShiftPressed);

   /// @brief 距離を設定
   void SetDistance(float distance) { distance_ = std::max(0.5f, distance); }

   /// @brief ピボットからの距離を取得する
   float GetDistance() const { return distance_; }

   /// @brief ピボットターゲットを設定
   void SetPivotTarget(const Vector3& target) { pivotTarget_ = target; }

   /// @brief ピボットターゲットを取得する
   const Vector3& GetPivotTarget() const { return pivotTarget_; }

   /// @brief 回転速度を設定
   void SetRotateSpeed(float speed) { rotateSpeed_ = speed; }

   /// @brief スクロール速度を設定
   void SetScrollSpeed(float speed) { scrollSpeed_ = speed; }

   /// @brief 移動速度を設定
   void SetMoveSpeed(float speed) { moveSpeed_ = speed; }

   /// @brief Yaw角度を取得
   float GetYaw() const { return yaw_; }

   /// @brief Pitch角度を取得
   float GetPitch() const { return pitch_; }

   /// @brief Yaw角度を設定
   void SetYaw(float yaw) { yaw_ = yaw; }

   /// @brief Pitch角度を設定
   void SetPitch(float pitch) { pitch_ = pitch; }

   /// @brief OrbitalBodyのYaw/Pitchから計算したカメラの前方ベクトルを返す
   Vector3 GetCameraForward() const;

   /// @brief OrbitalBodyのYaw/Pitchから計算したカメラの上ベクトルを返す（スクリーン上方向）
   Vector3 GetCameraUp() const;

   /// @brief OrbitalBodyのYaw/Pitchから計算したカメラの右ベクトルを返す（スクリーン右方向）
   Vector3 GetCameraRight() const;

   /// @brief 保存データと仮想カメラのコンポーネント検索に使う名前を返す。
   /// @return 「OrbitalBody」を表す固定文字列。
   const char* GetComponentName() const override { return "OrbitalBody"; }

   /// @brief 設定値をプロジェクト共通の単位でJSONへ保存する。
   nlohmann::json Serialize() const override;

   /// @brief プロジェクト共通の単位で保存された設定JSONを読み込む。
   void Deserialize(const nlohmann::json& data) override;

   /// @brief シーン固有の参照を除き、共有可能な設定をJSONで返す。
   nlohmann::json GetSharedParameters() const override;

   /// @brief 実行状態を維持したまま、統一単位の共有設定JSONを適用する。
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief 設定と実行状態を単位付きで表示・編集する。
   void DrawInspector() override;
#endif

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   float yaw_ = MathConstants::kPi;           ///< Yaw角度。
   float pitch_ = -0.785f;                    // -45度
   float distance_ = 75.0f;                   ///< ピボットからの距離。
   Vector3 pivotTarget_ = {0.0f, 0.0f, 0.0f}; ///< ピボットターゲット。

   float rotateSpeed_ = 0.01f;        ///< 入力回転感度（rad/pixel）。
   float scrollSpeed_ = 1.0f / 20.0f; ///< ホイールズーム感度（m/wheel入力値）。
   float moveSpeed_ = 0.0008f; ///< 距離に対するパン感度（1/pixel）。実移動量はdistance × mouseDelta × moveSpeed。。

   Matrix4x4 rotationMatrix_ = MakeIdentity4x4(); ///< 現在の旋回姿勢を表す回転行列。
};

} // namespace GameEngine
