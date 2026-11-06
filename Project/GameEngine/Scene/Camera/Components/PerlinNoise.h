#pragma once

#include "GameEngine/Scene/Camera/Core/ICinemachineComponent.h"
#include "GameEngine/Math/Types/Vector3.h"

namespace GameEngine {

/// @brief パーリンノイズベースのカメラシェイク
class PerlinNoise : public ICinemachineComponent {
public:
   /// @brief 停止状態のノイズコンポーネントを生成する
   PerlinNoise() = default;

   /// @brief カメラコンポーネントを破棄する
   ~PerlinNoise() override = default;

   /// @brief 経過時間に応じたシェイクをカメラ位置へ加え、残り時間とビュー行列を同期する。
   /// @param state 更新するカメラ状態。
   /// @param deltaTime フレームの経過時間（s）。
   /// @details 方向指定・画面基準・三軸の設定に応じて揺れを適用し、有限時間のシェイクは残り時間を減らす。
   void MutateCameraState(CameraState& state, float deltaTime) override;

   /// @brief 仮想カメラ内でこの処理を実行するステージを返す。
   /// @return 位置へ揺れを加えるNoiseステージ。
   CinemachineStage GetStage() const override { return CinemachineStage::Noise; }

   /// @brief 保存データと仮想カメラのコンポーネント検索に使う名前を返す。
   /// @return 「PerlinNoise」を表す固定文字列。
   const char* GetComponentName() const override { return "PerlinNoise"; }

   /// @brief シェイクを開始
   /// @param amplitude 振幅
   /// @param frequency 周波数
   /// @param duration 持続時間
   void Shake(float amplitude, float frequency, float duration);

   /// @brief 指定方向だけにシェイクを開始
   /// @param direction シェイク方向
   /// @param amplitude 振幅
   /// @param frequency 周波数
   /// @param duration 持続時間
   void ShakeDirectional(const Vector3& direction, float amplitude, float frequency, float duration);

   /// @brief 更新時のカメラの縦・横方向へ独立したシェイクを開始する
   /// @param horizontalAmplitude 横揺れの振幅
   /// @param verticalAmplitude 縦揺れの振幅
   /// @param frequency 周波数
   /// @param duration 持続時間（0以下なら停止）
   void ShakeScreenSpace(float horizontalAmplitude, float verticalAmplitude, float frequency, float duration);

   /// @brief 振幅を設定
   void SetAmplitude(float amplitude) { amplitudeGain_ = amplitude; }

   /// @brief 周波数を設定
   void SetFrequency(float frequency) { frequencyGain_ = frequency; }

   /// @brief シェイク中かどうか
   bool IsShaking() const { return remainingTime_ > 0.0f; }

   /// @brief カメラシェイクの保存対象設定をJSONへ変換する。
   /// @return 振幅・周波数とシェイクの実行状態を含む設定JSON。
   nlohmann::json Serialize() const override;

   /// @brief 保存JSONからカメラシェイクの設定を復元する。
   /// @param data 振幅・周波数とシェイクの実行状態を含む設定JSON。
   void Deserialize(const nlohmann::json& data) override;

   /// @brief カメラシェイクの共有可能な調整値をJSONで取得する。
   /// @return 経過位相・残り時間・一時振幅・方向などの実行状態を除いたシェイク設定を含む共有設定JSON。
   nlohmann::json GetSharedParameters() const override;

   /// @brief カメラシェイクの共有設定を既存コンポーネントへ反映する。
   /// @param data 経過位相・残り時間・一時振幅・方向などの実行状態を除いたシェイク設定を含む共有設定JSON。
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief カメラシェイクの振幅・周波数・再生状態を表示し、テストシェイクを操作する。
   void DrawInspector() override;
#endif

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   float Noise(float t) const;

   float amplitudeGain_ = 0.0f;                  ///< カメラ揺れに掛ける振幅倍率。
   float frequencyGain_ = 1.0f;                  ///< カメラ揺れの時間周波数へ掛ける倍率。
   float shakeFrequency_ = 1.0f;                 ///< 一時シェイク専用の周波数。共通設定へ保存しない。
   float time_ = 0.0f;                           ///< クリップ先頭からの時刻（秒）。
   float remainingTime_ = 0.0f;                  ///< 処理が終了するまでの残り時間（秒）。
   float initialAmplitude_ = 0.0f;               ///< 減衰開始時のカメラ揺れの振幅。
   Vector3 shakeDirection_ = {0.0f, 1.0f, 0.0f}; ///< 方向指定のカメラ揺れに使うワールド方向。
   bool useDirectionalShake_ = false;            ///< 揺れを指定方向に制限するかどうか。
   Vector2 screenSpaceWeights_ = {};             ///< 画面の横・縦方向それぞれに加える揺れの重み。
   bool useScreenSpaceShake_ = false;            ///< 画面基準の横・縦方向へ揺れを適用するかどうか。
};

} // namespace GameEngine
