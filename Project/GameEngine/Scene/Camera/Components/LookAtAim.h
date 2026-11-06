#pragma once

#include "GameEngine/Scene/Camera/Core/ICinemachineComponent.h"

namespace GameEngine {

/// @brief ターゲットを注視するAimコンポーネント
class LookAtAim : public ICinemachineComponent {
public:
   /// @brief 既定の注視オフセットと減衰で生成する
   LookAtAim() = default;

   /// @brief カメラコンポーネントを破棄する
   ~LookAtAim() override = default;

   /// @brief 注視対象とオフセットから目標姿勢を作り、指数補間でカメラ回転を更新する。
   /// @param state 更新するカメラ状態。
   /// @param deltaTime フレームの経過時間（s）。
   /// @details 所有カメラまたは注視対象が未設定の場合は、カメラ状態を変更しない。
   void MutateCameraState(CameraState& state, float deltaTime) override;

   /// @brief 仮想カメラ内でこの処理を実行するステージを返す。
   /// @return 姿勢を計算するAimステージ。
   CinemachineStage GetStage() const override { return CinemachineStage::Aim; }

   /// @brief 保存データと仮想カメラのコンポーネント検索に使う名前を返す。
   /// @return 「LookAtAim」を表す固定文字列。
   const char* GetComponentName() const override { return "LookAtAim"; }

   /// @brief ダンピングを設定
   void SetDamping(float damping) { damping_ = damping; }

   /// @brief 注視回転のダンピングを取得する
   float GetDamping() const { return damping_; }

   /// @brief オフセットを設定（注視点のオフセット）
   void SetOffset(const Vector3& offset) { offset_ = offset; }

   /// @brief 注視点のオフセットを取得する
   const Vector3& GetOffset() const { return offset_; }

   /// @brief カメラ注視の保存対象設定をJSONへ変換する。
   /// @return 注視点オフセット（m）と回転追従の減衰係数（1/s）を含む設定JSON。
   nlohmann::json Serialize() const override;

   /// @brief 保存JSONからカメラ注視の設定を復元する。
   /// @param data 注視点オフセット（m）と回転追従の減衰係数（1/s）を含む設定JSON。
   void Deserialize(const nlohmann::json& data) override;

   /// @brief カメラ注視の共有可能な調整値をJSONで取得する。
   /// @return 注視点オフセット（m）と回転追従の減衰係数（1/s）を含む共有設定JSON。
   nlohmann::json GetSharedParameters() const override;

   /// @brief カメラ注視の共有設定を既存コンポーネントへ反映する。
   /// @param data 注視点オフセット（m）と回転追従の減衰係数（1/s）を含む共有設定JSON。
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief カメラ注視の注視点オフセット（m）と回転追従の減衰係数（1/s）をインスペクターで表示・編集する。
   void DrawInspector() override;
#endif

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   float damping_ = 10.0f;               ///< 注視回転のダンピング。
   Vector3 offset_ = {0.0f, 0.0f, 0.0f}; ///< 注視点のオフセット。
};

} // namespace GameEngine
