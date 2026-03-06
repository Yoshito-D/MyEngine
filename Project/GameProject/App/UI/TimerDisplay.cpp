#include "pch.h"
#include "TimerDisplay.h"
#include "Framework/EngineContext.h"
#include "Object/Sprite/Sprite.h"
#include "Core/Graphics/Texture.h"
#include "Core/Graphics/Material.h"
#include "Utility/MathUtils/EasingFunctions.h"
#include <cmath>

using namespace GameEngine;

TimerDisplay::TimerDisplay()
	: position_(Vector2(0.0f, 0.0f))
	, digitSize_(Vector2(32.0f, 48.0f))
	, currentValue_(0.0f)
	, colonTexture_(nullptr)
	, blinkTimer_(0.0f)
	, isVisible_(true)
	, colorState_(ColorState::Normal)
	, currentColor_(0xffffffff)
	, scaleAnimationTimer_(0.0f)
	, currentScale_(1.0f) {

	for (int i = 0; i < 5; ++i) {
		digits_[i] = 0;
	}
}

TimerDisplay::~TimerDisplay() {
}

void TimerDisplay::Initialize(const Vector2& position, const Vector2& digitSize) {
	position_ = position;
	digitSize_ = digitSize;

	LoadTextures();
	CreateSprites();
	SetValue(0.0f);
}

void TimerDisplay::LoadTextures() {
	digitTextures_.clear();
	digitTextures_.resize(10);

	for (int i = 0; i < 10; ++i) {
		std::string textureName = "number" + std::to_string(i);
		auto* texture = GameEngine::EngineContext::GetTexture(textureName);

		if (!texture) {
			std::string texturePath = "resources/textures/" + textureName + ".png";
			GameEngine::EngineContext::LoadTexture(texturePath, textureName);
			texture = GameEngine::EngineContext::GetTexture(textureName);
		}

		digitTextures_[i] = texture;
	}

	colonTexture_ = GameEngine::EngineContext::GetTexture("colon");
	if (!colonTexture_) {
		GameEngine::EngineContext::LoadTexture("resources/textures/colon.png", "colon")
		;
		colonTexture_ = GameEngine::EngineContext::GetTexture("colon");
	}
}

void TimerDisplay::CreateSprites() {
	sprites_.clear();
	sprites_.resize(5);

	for (int i = 0; i < 5; ++i) {
		sprites_[i] = std::make_unique<GameEngine::Sprite>();
		// 各スプライト用の独立したマテリアルを作成
		std::string materialName = "TimerDigit" + std::to_string(i);
		GameEngine::EngineContext::CreateMaterial(materialName, currentColor_, 0);
		auto* material = GameEngine::EngineContext::GetMaterial(materialName);
		// アンカーポイントを中心(0.5f, 0.5f)に設定
		sprites_[i]->Create(digitSize_, material, Vector2(0.5f, 0.5f));

		float xOffset = i * (digitSize_.x + 4.0f);
		Vector2 spritePos = position_ + Vector2(xOffset, 0.0f);
		sprites_[i]->SetPosition(spritePos);
	}
}

void TimerDisplay::SetValue(float seconds) {
	currentValue_ = (seconds < 0.0f) ? 0.0f : seconds;
	UpdateDigits();
	UpdateColorState();
	
	// タイマーが0になったら点滅をリセット
	if (currentValue_ < 1.0f) {
		blinkTimer_ = 0.0f;
		isVisible_ = true;
	}
}

void TimerDisplay::Update(float deltaTime) {
	// タイマーが0の時のみ点滅
	if (currentValue_ < 1.0f) {
		blinkTimer_ += deltaTime;
		if (blinkTimer_ >= kBlinkInterval_) {
			blinkTimer_ = 0.0f;
			isVisible_ = !isVisible_;
		}
	} else {
		isVisible_ = true;
	}
	
	// スケールアニメーションの更新
	UpdateScaleAnimation(deltaTime);
}

void TimerDisplay::UpdateDigits() {
	int totalSec = static_cast<int>(currentValue_);

	int m = totalSec / 60;
	int s = totalSec % 60;

	digits_[0] = m / 10;
	digits_[1] = m % 10;
	digits_[2] = 10;
	digits_[3] = s / 10;
	digits_[4] = s % 10;
}

void TimerDisplay::UpdateColorState() {
	ColorState newState = ColorState::Normal;
	
	if (currentValue_ <= 10.0f) {
		newState = ColorState::Danger;
	} else if (currentValue_ <= 30.0f) {
		newState = ColorState::Warning;
	}
	
	// 状態が変わった場合のみ色を更新
	if (newState != colorState_) {
		colorState_ = newState;
		
		switch (colorState_) {
			case ColorState::Normal:
				currentColor_ = 0xffffffff; // 白
				break;
			case ColorState::Warning:
				currentColor_ = 0xffff00ff; // 黄色 (RGBA)
				break;
			case ColorState::Danger:
				currentColor_ = 0xff0000ff; // 赤 (RGBA)
				break;
		}
		
		// 全スプライトのマテリアルの色を更新
		for (size_t i = 0; i < sprites_.size(); ++i) {
			std::string materialName = "TimerDigit" + std::to_string(i);
			auto* material = GameEngine::EngineContext::GetMaterial(materialName);
			if (material) {
				material->SetColor(currentColor_);
			}
		}
	}
}

void TimerDisplay::UpdateScaleAnimation(float deltaTime) {
	// 点滅中はアニメーションしない
	if (currentValue_ < 1.0f) {
		currentScale_ = 1.0f;
		return;
	}
	
	// 警告または危険状態の場合のみアニメーション
	if (colorState_ == ColorState::Normal) {
		currentScale_ = 1.0f;
		return;
	}
	
	scaleAnimationTimer_ += deltaTime;
	
	// 1秒ごとにアニメーションをトリガー
	if (scaleAnimationTimer_ >= kScaleAnimationInterval_) {
		scaleAnimationTimer_ = 0.0f;
	}
	
	// アニメーション期間内かチェック
	if (scaleAnimationTimer_ < kScaleAnimationDuration_) {
		float progress = scaleAnimationTimer_ / kScaleAnimationDuration_;
		
		// 前半で大きくなり、後半で元に戻る (EaseOut -> EaseIn)
		if (progress < 0.5f) {
			// 前半: 1.0 -> 1.2 (EaseOut)
			float halfProgress = progress * 2.0f;
			currentScale_ = Easing::EaseOut(kMinScale_, kMaxScale_, halfProgress, 2.0f);
		} else {
			// 後半: 1.2 -> 1.0 (EaseIn)
			float halfProgress = (progress - 0.5f) * 2.0f;
			currentScale_ = Easing::EaseIn(kMaxScale_, kMinScale_, halfProgress, 2.0f);
		}
	} else {
		currentScale_ = kMinScale_;
	}
}

void TimerDisplay::Draw() {
	if (sprites_.empty()) return;
	
	// タイマーが0で非表示状態の時は描画しない
	if (!isVisible_) return;

	for (int i = 0; i < 5; ++i) {
		GameEngine::Texture* texture = nullptr;

		if (i == 2) {
			texture = colonTexture_;
		}
		else {
			int digitValue = digits_[i];
			if (digitValue >= 0 && digitValue < 10) {
				texture = digitTextures_[digitValue];
			}
		}

		if (texture && sprites_[i]) {
			// スケールを適用
			sprites_[i]->SetScale(Vector2(currentScale_, currentScale_));
			
			GameEngine::EngineContext::DrawUI(
				sprites_[i].get(),
				texture,
				GameEngine::Sprite::AnchorPoint::TopLeft,
			   BlendMode::kBlendModeNormal
			);
		}
	}
}

void TimerDisplay::SetPosition(const Vector2& position) {
	position_ = position;

	for (int i = 0; i < static_cast<int>(sprites_.size()); ++i) {
		if (sprites_[i]) {
			float xOffset = i * (digitSize_.x + 4.0f);
			Vector2 spritePos = position_ + Vector2(xOffset, 0.0f);
			sprites_[i]->SetPosition(spritePos);
		}
	}
}

void TimerDisplay::SetDigitSize(const Vector2& size) {
	digitSize_ = size;

	for (auto& sprite : sprites_) {
		if (sprite) {
			sprite->SetSize(digitSize_);
		}
	}

	SetPosition(position_);
}
