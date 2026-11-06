#pragma once

#include "GameEngine/Effects/Particles/Modules/ParticleModule.h"
#include "GameEngine/Math/VectorMath.h"
#include "GameEngine/Effects/Particles/Modules/MainModule.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
// ============================================================
// 生存期間中のサイズモジュール
// ============================================================
class SizeOverLifetimeModule : public ParticleModule {
public:
   /// @brief 生存期間サイズ設定を既定値で初期化する
   SizeOverLifetimeModule();

   /// @brief パーティクルのサイズを更新
   void UpdateSize(Particle& particle) const;

   /// @brief 粒子ごとのサイズ倍率を固定値で設定する
   void SetSizeMultiplier(float multiplier) { sizeMultiplier_ = RandomFloat(multiplier); }

   /// @brief サイズ倍率の代表値を取得する
   float GetSizeMultiplier() const { return sizeMultiplier_.Minimum(); }

   /// @brief 粒子ごとのサイズ倍率を乱数範囲で設定する
   void SetSizeMultiplierRange(const RandomFloat& multiplier) { sizeMultiplier_ = multiplier; }

   /// @brief サイズ倍率の乱数範囲を取得する
   const RandomFloat& GetSizeMultiplierRange() const { return sizeMultiplier_; }

   /// @brief 粒子固有のサイズ倍率を初期化する
   void InitializeParticle(Particle& particle) const;

   /// @brief 生存期間の開始時サイズを設定する
   void SetStartSize(const Vector3& size) { startSize_ = size; }

   /// @brief 生存期間の開始時サイズを取得する
   const Vector3& GetStartSize() const { return startSize_; }

   /// @brief 生存期間の終了時サイズを設定する
   void SetEndSize(const Vector3& size) { endSize_ = size; }

   /// @brief 生存期間の終了時サイズを取得する
   const Vector3& GetEndSize() const { return endSize_; }

   /// @brief 寿命に応じたサイズ変化とサイズ倍率の設定をJSONへ保存する。
   /// @return モジュールの有効状態と固有パラメータを含む設定JSON。
   nlohmann::json ToJson() const override;

   /// @brief 寿命に応じたサイズ変化とサイズ倍率の設定をJSONから読み込む。
   /// @param json モジュールの有効状態と固有パラメータを含む設定JSON。
   void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI

   /// @brief 寿命に応じたサイズ変化とサイズ倍率の設定とモジュールの有効状態を表示・編集する。
   void DrawInspector() override;
#endif

private:
   RandomFloat sizeMultiplier_{1.0f, 1.0f, false}; ///< サイズ倍率の乱数範囲。
   Vector3 startSize_{1.0f, 1.0f, 1.0f};           ///< 生存期間の開始時サイズ。
   Vector3 endSize_{0.0f, 0.0f, 0.0f};             ///< 生存期間の終了時サイズ。
};
}
