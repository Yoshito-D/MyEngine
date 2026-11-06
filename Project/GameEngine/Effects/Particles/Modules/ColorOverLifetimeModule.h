#pragma once

#include "GameEngine/Effects/Particles/Modules/ParticleModule.h"
#include "GameEngine/Math/VectorMath.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
// ============================================================
// 生存期間中の色モジュール
// ============================================================
class ColorOverLifetimeModule : public ParticleModule {
public:
   /// @brief 生存期間カラー設定を既定値で初期化する
   ColorOverLifetimeModule();

   /// @brief パーティクルの色を更新
   void UpdateColor(Particle& particle) const;

   /// @brief 生存期間の開始色を設定する
   void SetStartColor(const Vector4& color) { startColor_ = color; }

   /// @brief 生存期間の開始色を取得する
   const Vector4& GetStartColor() const { return startColor_; }

   /// @brief 生存期間の終了色を設定する
   void SetEndColor(const Vector4& color) { endColor_ = color; }

   /// @brief 生存期間の終了色を取得する
   const Vector4& GetEndColor() const { return endColor_; }

   /// @brief 寿命に応じた開始色・終了色の設定をJSONへ保存する。
   /// @return モジュールの有効状態と固有パラメータを含む設定JSON。
   nlohmann::json ToJson() const override;

   /// @brief 寿命に応じた開始色・終了色の設定をJSONから読み込む。
   /// @param json モジュールの有効状態と固有パラメータを含む設定JSON。
   void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI

   /// @brief 寿命に応じた開始色・終了色の設定とモジュールの有効状態を表示・編集する。
   void DrawInspector() override;
#endif

private:
   Vector4 startColor_{1.0f, 1.0f, 1.0f, 1.0f}; ///< 生存期間の開始色。
   Vector4 endColor_{1.0f, 1.0f, 1.0f, 0.0f};   ///< 生存期間の終了色。
};
}
