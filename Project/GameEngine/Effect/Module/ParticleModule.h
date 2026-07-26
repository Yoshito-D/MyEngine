#pragma once
#include <nlohmann/json.hpp>
#include "Effect/Particle.h"
#include "Utility/VectorMath.h"

namespace GameEngine {
/// @brief パーティクルモジュールの基底クラス
/// 各モジュールはパラメータの保存・読み込みとJSON変換のみを担当
class ParticleModule {
public:
   /// @brief 派生モジュールを基底ポインター経由で安全に破棄する
   virtual ~ParticleModule() = default;

   /// @brief モジュールの処理を有効または無効にする
   void SetEnabled(bool enabled) { enabled_ = enabled; }
   /// @brief モジュールが有効かを取得する
   bool IsEnabled() const { return enabled_; }

   /// @brief JSON形式でパラメータを取得
   virtual nlohmann::json ToJson() const = 0;

   /// @brief JSON形式からパラメータを設定
   virtual void FromJson(const nlohmann::json& json) = 0;

#ifdef USE_IMGUI
   /// @brief ImGui inspector for editor builds.
   virtual void DrawInspector() = 0;
#endif

protected:
   bool enabled_ = true;
};
}
