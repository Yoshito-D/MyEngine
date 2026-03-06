#pragma once
#include <nlohmann/json.hpp>
#include "Effect/Particle.h"
#include "Utility/VectorMath.h"

namespace GameEngine {
/// @brief パーティクルモジュールの基底クラス
/// 各モジュールはパラメータの保存・読み込みとJSON変換のみを担当
class ParticleModule {
public:
   virtual ~ParticleModule() = default;

   void SetEnabled(bool enabled) { enabled_ = enabled; }
   bool IsEnabled() const { return enabled_; }

   /// @brief JSON形式でパラメータを取得
   virtual nlohmann::json ToJson() const = 0;

   /// @brief JSON形式からパラメータを設定
   virtual void FromJson(const nlohmann::json& json) = 0;

protected:
   bool enabled_ = true;
};
}