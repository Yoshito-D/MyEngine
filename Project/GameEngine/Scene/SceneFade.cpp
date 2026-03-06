#include "pch.h"
#include "SceneFade.h"
#include "Sprite.h"
#include "Camera.h"
#include "EngineContext.h"
#include "Window.h"
#include "Utility/MathUtils.h"

namespace GameEngine {

void SceneFade::Initialize(float fadeDuration, uint32_t fadeColor) {
   fadeDuration_ = fadeDuration;
   fadeColor_ = fadeColor;
   easingType_ = EasingType::EaseInOut;  // デフォルトはEaseInOut
   easingPower_ = 2.0f;

   // 既存のテクスチャを使用（white1x1が無い場合のフォールバック）
   whiteTexture_ = EngineContext::GetTexture("white1x1");

   if (!whiteTexture_) {
	  // white1x1.pngをロード試行
	  EngineContext::LoadTexture("resources/white1x1.png", "white1x1");
	  whiteTexture_ = EngineContext::GetTexture("white1x1");
   }

   // それでも取得できない場合は既存のテクスチャを使用
   if (!whiteTexture_) {
	  whiteTexture_ = EngineContext::GetTexture("uvChecker");
   }

   // マテリアルを作成
   EngineContext::CreateMaterial("FadeMaterial", fadeColor_);
   fadeMaterial_ = EngineContext::GetMaterial("FadeMaterial");

   // フェード用スプライトを作成
   fadeSprite_ = std::make_unique<Sprite>();
   Vector2 screenSize = { static_cast<float>(Window::kResolutionWidth), static_cast<float>(Window::kResolutionHeight) };
   fadeSprite_->Create(screenSize, fadeMaterial_, Vector2(0.5f, 0.5f));
   fadeSprite_->SetPosition(Vector2(0.0f, 0.0f));
}

void SceneFade::Update() {
   if (fadeState_ == FadeState::None) {
	  return;
   }

   // デルタタイムを内部で取得
   float deltaTime = EngineContext::GetDeltaTime();

   fadeTimer_ += deltaTime;
   float t = std::min(fadeTimer_ / fadeDuration_, 1.0f);

   // イージングを適用
   float easedT = t;
   switch (easingType_) {
	  case EasingType::Linear:
		 easedT = t;
		 break;
	  case EasingType::EaseIn:
		 easedT = Easing::EaseIn(0.0f, 1.0f, t, easingPower_);
		 break;
	  case EasingType::EaseOut:
		 easedT = Easing::EaseOut(0.0f, 1.0f, t, easingPower_);
		 break;
	  case EasingType::EaseInOut:
		 easedT = Easing::EaseInOut(0.0f, 1.0f, t, easingPower_);
		 break;
   }

   if (fadeState_ == FadeState::FadeIn) {
	  // フェードイン: 1.0 -> 0.0
	  fadeAlpha_ = 1.0f - easedT;

	  if (t >= 1.0f) {
		 fadeState_ = FadeState::None;
		 fadeAlpha_ = 0.0f;
		 fadeTimer_ = 0.0f;
	  }
   } else if (fadeState_ == FadeState::FadeOut) {
	  // フェードアウト: 0.0 -> 1.0
	  fadeAlpha_ = easedT;

	  if (t >= 1.0f) {
		 fadeState_ = FadeState::None;
		 fadeAlpha_ = 1.0f;
		 fadeTimer_ = 0.0f;

		 // フェードアウト完了を1フレーム遅らせて設定
		 if (!isFadeOutCompleted_) {
			isFadeOutCompleted_ = true;
		 }
	  }
   }

   // アルファ値を反映
   uint32_t alpha = static_cast<uint32_t>(fadeAlpha_ * 255.0f);
   uint32_t color = (fadeColor_ & 0xFFFFFF00) | alpha;
   fadeMaterial_->SetColor(color);
}

void SceneFade::Draw() {
   // フェード中、またはフェードアウト完了状態の場合は描画
   // （fadeAlpha_ が 0 より大きい場合、またはフェードアウト完了している場合）
   if ((fadeAlpha_ > 0.0001f || fadeState_ != FadeState::None) &&
	  fadeSprite_ && fadeMaterial_ && whiteTexture_) {
	  // DrawUIを呼び出し
	  EngineContext::DrawUI(fadeSprite_.get(), whiteTexture_,
		 Sprite::AnchorPoint::MiddleCenter,
		 BlendMode::kBlendModeNormal,
		 false,
		 Window::kResolutionWidth,
		 Window::kResolutionHeight);
   }

#ifdef USE_IMGUI
   // デバッグ情報を表示
   ImGui::Begin("SceneFade Debug");
   ImGui::Text("Fade State: %d", static_cast<int>(fadeState_));
   ImGui::Text("Fade Alpha: %.3f", fadeAlpha_);
   ImGui::Text("Fade Timer: %.3f / %.3f", fadeTimer_, fadeDuration_);
   ImGui::Text("Fade Color: 0x%08X", fadeColor_);
   ImGui::Text("Fade Out Completed: %s", isFadeOutCompleted_ ? "true" : "false");
   ImGui::Text("Has Sprite: %s", fadeSprite_ ? "true" : "false");
   ImGui::Text("Has Material: %s", fadeMaterial_ ? "true" : "false");
   ImGui::Text("Has Texture: %s", whiteTexture_ ? "true" : "false");
   ImGui::Text("Should Draw: %s", ((fadeAlpha_ > 0.0001f || isFadeOutCompleted_) && fadeSprite_ && fadeMaterial_ && whiteTexture_) ? "YES" : "NO");
   ImGui::End();
#endif
}

void SceneFade::StartFadeIn() {
   // フェードイン開始時は常にアルファ値を1.0に設定
   fadeAlpha_ = 1.0f;

   fadeState_ = FadeState::FadeIn;
   fadeTimer_ = 0.0f;
   isFadeOutCompleted_ = false;

   // マテリアルの色も即座に更新
   if (fadeMaterial_) {
	  uint32_t alpha = static_cast<uint32_t>(fadeAlpha_ * 255.0f);
	  uint32_t color = (fadeColor_ & 0xFFFFFF00) | alpha;
	  fadeMaterial_->SetColor(color);
   }
}

void SceneFade::StartFadeOut() {
#ifdef USE_IMGUI
   OutputDebugStringA("=== StartFadeOut Called ===\n");
   char debugBuffer[256];
   sprintf_s(debugBuffer, "Current fadeAlpha_: %.3f\n", fadeAlpha_);
   OutputDebugStringA(debugBuffer);
   sprintf_s(debugBuffer, "fadeDuration_: %.3f\n", fadeDuration_);
   OutputDebugStringA(debugBuffer);
   sprintf_s(debugBuffer, "fadeColor_: 0x%08X\n", fadeColor_);
   OutputDebugStringA(debugBuffer);
#endif

   // フェードアウト開始時は常にアルファ値を0.0に設定
   fadeAlpha_ = 0.0f;

   fadeState_ = FadeState::FadeOut;
   fadeTimer_ = 0.0f;
   isFadeOutCompleted_ = false;  // フェードアウト開始時はリセット

   // マテリアルの色も即座に更新
   if (fadeMaterial_) {
	  uint32_t alpha = static_cast<uint32_t>(fadeAlpha_ * 255.0f);
	  uint32_t color = (fadeColor_ & 0xFFFFFF00) | alpha;
	  fadeMaterial_->SetColor(color);

#ifdef USE_IMGUI
	  sprintf_s(debugBuffer, "Material color set to: 0x%08X\n", color);
	  OutputDebugStringA(debugBuffer);
#endif
   } else {
#ifdef USE_IMGUI
	  OutputDebugStringA("ERROR: fadeMaterial_ is nullptr!\n");
#endif
   }
}

void SceneFade::SetFadeColor(uint32_t color) {
   fadeColor_ = color;
   if (fadeMaterial_) {
	  uint32_t alpha = static_cast<uint32_t>(fadeAlpha_ * 255.0f);
	  uint32_t colorWithAlpha = (color & 0xFFFFFF00) | alpha;
	  fadeMaterial_->SetColor(colorWithAlpha);
   }
}

}
