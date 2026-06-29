#pragma once
#include "ParticleModule.h"
#include <algorithm>

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

	void SetTilesX(uint32_t value) { tilesX_ = (std::max)(value, 1u); ClampSettings(); }
	uint32_t GetTilesX() const { return tilesX_; }
	void SetTilesY(uint32_t value) { tilesY_ = (std::max)(value, 1u); ClampSettings(); }
	uint32_t GetTilesY() const { return tilesY_; }
	void SetFrameOverTime(float value) { frameOverTime_ = (std::max)(value, 0.0f); }
	float GetFrameOverTime() const { return frameOverTime_; }
	void SetCycles(uint32_t value) { cycles_ = (std::max)(value, 1u); }
	uint32_t GetCycles() const { return cycles_; }
	void SetFrameCount(uint32_t value) { frameCount_ = value; ClampSettings(); }
	uint32_t GetFrameCount() const { return frameCount_; }
	void SetRandomRow(bool value) { randomRow_ = animationMode_ == AnimationMode::SingleRow && value; }
	bool GetRandomRow() const { return randomRow_; }
	void SetRowIndex(uint32_t value) { rowIndex_ = value; ClampSettings(); }
	uint32_t GetRowIndex() const { return rowIndex_; }
	void SetStartFrame(uint32_t value) { startFrame_ = value; ClampSettings(); }
	uint32_t GetStartFrame() const { return startFrame_; }
	void SetAnimationMode(AnimationMode value) { animationMode_ = value == AnimationMode::SingleRow ? AnimationMode::SingleRow : AnimationMode::WholeSheet; ClampSettings(); }
	AnimationMode GetAnimationMode() const { return animationMode_; }

	nlohmann::json ToJson() const override;
	void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
	void DrawInspector() override;
#endif

private:
	void ClampSettings();
	uint32_t GetTotalFrameCount() const;
	uint32_t GetPlayableFrameCount() const;
	uint32_t ResolveFrame(const Particle& particle) const;

	uint32_t tilesX_ = 1;
	uint32_t tilesY_ = 1;
	float frameOverTime_ = 1.0f;
	uint32_t cycles_ = 1;
	uint32_t frameCount_ = 0;
	bool randomRow_ = false;
	uint32_t rowIndex_ = 0;
	uint32_t startFrame_ = 0;
	AnimationMode animationMode_ = AnimationMode::WholeSheet;
};
}
