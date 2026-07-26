#pragma once
#include "ParticleModule.h"
#include <algorithm>
#include <nlohmann/json.hpp>

namespace GameEngine {
class RendererModule : public ParticleModule {
public:
   enum class RotationSpace {
	  World = 0,
	  Local = 1
   };

   enum class BillboardType {
	  None = 0,
	  View,
	  Horizontal,
	  Vertical,
	  Velocity
   };

   /// @brief 透明パーティクルの描画順序
   enum class SortMode {
	  Auto = 0,
	  None,
	  BackToFront,
	  FrontToBack
   };

   /// @brief パーティクル描画設定を既定値で初期化する
   RendererModule();

   /// @brief 粒子回転を解釈する座標空間を設定する
   void SetRotationSpace(RotationSpace space) { rotationSpace_ = space; }
   /// @brief 粒子回転を解釈する座標空間を取得する
   RotationSpace GetRotationSpace() const { return rotationSpace_; }

   /// @brief ビルボードの向き合わせ方式を設定する
   void SetBillboardType(BillboardType type) { billboardType_ = type; }
   /// @brief ビルボードの向き合わせ方式を取得する
   BillboardType GetBillboardType() const { return billboardType_; }

   /// @brief 速度ストレッチへ使用する速度倍率を設定する
   void SetSpeedScale(float scale) { speedScale_ = scale; }
   /// @brief 速度ストレッチへ使用する速度倍率を取得する
   float GetSpeedScale() const { return speedScale_; }

   /// @brief 速度ストレッチの長さ倍率を設定する
   void SetLengthScale(float scale) { lengthScale_ = scale; }
   /// @brief 速度ストレッチの長さ倍率を取得する
   float GetLengthScale() const { return lengthScale_; }

   /// @brief 速度に応じて粒子本体を長軸方向へ引き延ばすか設定する
   /// @param enabled trueの場合、トレイルとは独立して速度ストレッチを適用する
   void SetVelocityStretchEnabled(bool enabled) { velocityStretchEnabled_ = enabled; }

   /// @brief 速度ストレッチが有効か取得する
   /// @return 有効な場合true
   bool IsVelocityStretchEnabled() const { return velocityStretchEnabled_; }

   /// @brief 描画順序を設定する
   void SetSortMode(SortMode mode) { sortMode_ = mode; }

   /// @brief 描画順序を取得する
   SortMode GetSortMode() const { return sortMode_; }

   /// @brief カメラ近接フェードを有効化する
   void SetCameraFadeEnabled(bool enabled) { cameraFadeEnabled_ = enabled; }

   /// @brief カメラ近接フェードが有効か取得する
   bool IsCameraFadeEnabled() const { return cameraFadeEnabled_; }

   /// @brief 完全に透明になるカメラ距離を設定する
   void SetCameraFadeNear(float distance) { cameraFadeNear_ = std::max(distance, 0.0f); }

   /// @brief 完全に透明になるカメラ距離を取得する
   float GetCameraFadeNear() const { return cameraFadeNear_; }

   /// @brief 完全に表示されるカメラ距離を設定する
   void SetCameraFadeFar(float distance) { cameraFadeFar_ = std::max(distance, 0.0f); }

   /// @brief 完全に表示されるカメラ距離を取得する
   float GetCameraFadeFar() const { return cameraFadeFar_; }

   /// @copydoc ParticleModule::ToJson
   nlohmann::json ToJson() const override;
   /// @copydoc ParticleModule::FromJson
   void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
   /// @copydoc ParticleModule::DrawInspector
   void DrawInspector() override;
#endif

private:
   RotationSpace rotationSpace_ = RotationSpace::Local;
   BillboardType billboardType_ = BillboardType::View;
   float speedScale_ = 1.0f;
   float lengthScale_ = 2.0f;
   bool velocityStretchEnabled_ = false;
   SortMode sortMode_ = SortMode::Auto;
   bool cameraFadeEnabled_ = false;
   float cameraFadeNear_ = 0.25f;
   float cameraFadeFar_ = 1.0f;
};
}
