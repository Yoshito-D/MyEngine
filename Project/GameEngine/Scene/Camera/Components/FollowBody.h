#pragma once

#include "GameEngine/Scene/Camera/Core/ICinemachineComponent.h"

namespace GameEngine {

/// @brief ターゲットを追従するBodyコンポーネント
class FollowBody : public ICinemachineComponent {
public:
   /// @brief 既定の追従オフセットと減衰で生成する
   FollowBody() = default;

   /// @brief カメラコンポーネントを破棄する
   ~FollowBody() override = default;

   /// @brief 追従対象のワールド位置へオフセットを加え、各軸の指数減衰でカメラ位置を追従させる。
   /// @param state 更新するカメラ状態。
   /// @param deltaTime フレームの経過時間（s）。
   /// @details 所有カメラまたは追従対象が未設定の場合は、カメラ状態を変更しない。
   void MutateCameraState(CameraState& state, float deltaTime) override;

   /// @brief 仮想カメラ内でこの処理を実行するステージを返す。
   /// @return 位置を計算するBodyステージ。
   CinemachineStage GetStage() const override { return CinemachineStage::Body; }

   /// @brief 保存データと仮想カメラのコンポーネント検索に使う名前を返す。
   /// @return 「FollowBody」を表す固定文字列。
   const char* GetComponentName() const override { return "FollowBody"; }

   /// @brief オフセットを設定
   void SetOffset(const Vector3& offset) { offset_ = offset; }

   /// @brief 追従対象からのオフセットを取得する
   const Vector3& GetOffset() const { return offset_; }

   /// @brief ダンピングを設定（各軸ごと）
   void SetDamping(const Vector3& damping) { damping_ = damping; }

   /// @brief 軸ごとの追従ダンピングを取得する
   const Vector3& GetDamping() const { return damping_; }

   /// @brief カメラ位置追従の保存対象設定をJSONへ変換する。
   /// @return 追従オフセット（m）と各軸の減衰係数（1/s）を含む設定JSON。
   nlohmann::json Serialize() const override;

   /// @brief 保存JSONからカメラ位置追従の設定を復元する。
   /// @param data 追従オフセット（m）と各軸の減衰係数（1/s）を含む設定JSON。
   void Deserialize(const nlohmann::json& data) override;

   /// @brief カメラ位置追従の共有可能な調整値をJSONで取得する。
   /// @return 追従オフセット（m）と各軸の減衰係数（1/s）を含む共有設定JSON。
   nlohmann::json GetSharedParameters() const override;

   /// @brief カメラ位置追従の共有設定を既存コンポーネントへ反映する。
   /// @param data 追従オフセット（m）と各軸の減衰係数（1/s）を含む共有設定JSON。
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief カメラ位置追従の追従オフセット（m）と各軸の減衰係数（1/s）をインスペクターで表示・編集する。
   void DrawInspector() override;
#endif

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   Vector3 offset_ = {0.0f, 5.0f, -10.0f}; ///< 追従対象からのオフセット。
   Vector3 damping_ = {1.0f, 1.0f, 1.0f};  ///< 軸ごとの追従ダンピング。
};

} // namespace GameEngine
