#pragma once
#include "ParticleModule.h"

namespace GameEngine {
class TextureSheetAnimationModule : public ParticleModule {
public:
	enum class AnimationMode {
		WholeSheet = 0,
		SingleRow = 1
	};

	TextureSheetAnimationModule();

	void InitializeParticle(Particle& particle) const;
	void UpdateAnimation(Particle& particle, float deltaTime) const;

	void SetTilesX(uint32_t value) { tilesX_ = value; }
	uint32_t GetTilesX() const { return tilesX_; }
	void SetTilesY(uint32_t value) { tilesY_ = value; }
	uint32_t GetTilesY() const { return tilesY_; }
	void SetFrameOverTime(float value) { frameOverTime_ = value; }
	float GetFrameOverTime() const { return frameOverTime_; }
	void SetCycles(uint32_t value) { cycles_ = value; }
	uint32_t GetCycles() const { return cycles_; }
	void SetRandomRow(bool value) { randomRow_ = value; }
	bool GetRandomRow() const { return randomRow_; }
	void SetRowIndex(uint32_t value) { rowIndex_ = value; }
	uint32_t GetRowIndex() const { return rowIndex_; }
	void SetStartFrame(uint32_t value) { startFrame_ = value; }
	uint32_t GetStartFrame() const { return startFrame_; }
	void SetAnimationMode(AnimationMode value) { animationMode_ = value; }
	AnimationMode GetAnimationMode() const { return animationMode_; }

	nlohmann::json ToJson() const override;
	void FromJson(const nlohmann::json& json) override;

private:
	uint32_t ResolveFrame(const Particle& particle) const;

	uint32_t tilesX_ = 1;
	uint32_t tilesY_ = 1;
	float frameOverTime_ = 1.0f;
	uint32_t cycles_ = 1;
	bool randomRow_ = false;
	uint32_t rowIndex_ = 0;
	uint32_t startFrame_ = 0;
	AnimationMode animationMode_ = AnimationMode::WholeSheet;
};
}
