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

   RendererModule();

   void SetRotationSpace(RotationSpace space) { rotationSpace_ = space; }
   RotationSpace GetRotationSpace() const { return rotationSpace_; }

   void SetBillboardType(BillboardType type) { billboardType_ = type; }
   BillboardType GetBillboardType() const { return billboardType_; }

   void SetSpeedScale(float scale) { speedScale_ = scale; }
   float GetSpeedScale() const { return speedScale_; }

   void SetLengthScale(float scale) { lengthScale_ = scale; }
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

   nlohmann::json ToJson() const override;
   void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
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
