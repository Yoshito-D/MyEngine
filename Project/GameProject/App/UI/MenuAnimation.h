#pragma once

#include "GameEngine/Math/Types/Vector3.h"
#include <cstdint>
#include <span>

namespace GameEngine {
class TransformComponent;
class UITextComponent;
}

namespace App {
/// @brief メニューの切替と脈動だけを管理する。入力と画面遷移は所有画面が処理する。
class MenuSelectionAnimation {
public:
   /// @brief 演出の時間を先頭へ戻し、必要なら選択切替を開始する。
   void Reset(bool switching = false);

   /// @brief 選択項目へ適用する倍率を一フレーム分進める。
   float Evaluate(float deltaTime, float pulseDuration, float pulseScale, float switchDuration, float switchScale);

private:
   float elapsed_ = 0.0f;
   float switchElapsed_ = 0.0f;
   bool switching_ = false;
};

/// @brief 基準倍率を毎フレーム適用し、未選択側へ直前の選択演出を残さない。
void UpdateMenuSelection(std::span<GameEngine::UITextComponent* const> texts,
   std::span<GameEngine::TransformComponent* const> transforms, std::span<const GameEngine::Vector3> scales,
   std::int32_t selected, MenuSelectionAnimation& animation, float deltaTime, float pulseDuration, float pulseScale,
   float switchDuration, float switchScale);

/// @brief 決定時点の倍率から拡大・フェードし、反復補間による誤差を蓄積させない。
void ApplyMenuReaction(GameEngine::UITextComponent& text, GameEngine::TransformComponent& transform,
   const GameEngine::Vector3& baseScale, const GameEngine::Vector3& startScale, float opacity, float elapsed,
   float duration, float endScale);
} // namespace App
