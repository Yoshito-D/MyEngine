#include "pch.h"
#include "TextureSheetAnimationModule.h"

namespace GameEngine {
TextureSheetAnimationModule::TextureSheetAnimationModule() = default;

void TextureSheetAnimationModule::ClampSettings() {
	tilesX_ = std::max(tilesX_, 1u);
	tilesY_ = std::max(tilesY_, 1u);
	cycles_ = std::max(cycles_, 1u);
	frameOverTime_ = std::max(frameOverTime_, 0.0f);

	const uint32_t totalFrameCount = GetTotalFrameCount();
	if (totalFrameCount > 0) {
		startFrame_ %= totalFrameCount;
	}
	if (frameCount_ > totalFrameCount) {
		frameCount_ = totalFrameCount;
	}
	if (animationMode_ == AnimationMode::WholeSheet) {
		randomRow_ = false;
	}
	rowIndex_ = std::min(rowIndex_, tilesY_ - 1u);
}

uint32_t TextureSheetAnimationModule::GetTotalFrameCount() const {
	const uint32_t safeTilesX = std::max(tilesX_, 1u);
	const uint32_t safeTilesY = std::max(tilesY_, 1u);
	return animationMode_ == AnimationMode::WholeSheet ? safeTilesX * safeTilesY : safeTilesX;
}

uint32_t TextureSheetAnimationModule::GetPlayableFrameCount() const {
	const uint32_t totalFrameCount = GetTotalFrameCount();
	if (totalFrameCount == 0) {
		return 1;
	}

	const uint32_t safeStartFrame = startFrame_ % totalFrameCount;
	const uint32_t availableFrames = std::max(totalFrameCount - safeStartFrame, 1u);
	if (frameCount_ == 0) {
		return availableFrames;
	}
	return std::clamp(frameCount_, 1u, availableFrames);
}

void TextureSheetAnimationModule::InitializeParticle(Particle& particle) const {
	if (!enabled_) return;
	const uint32_t safeTilesY = std::max(tilesY_, 1u);
	particle.sheetFrame = static_cast<int>(startFrame_);
	particle.sheetRow = randomRow_ ? RandomUtils::Random<int>(0, static_cast<int>(safeTilesY - 1u)) : static_cast<int>(std::min(rowIndex_, safeTilesY - 1u));
}

uint32_t TextureSheetAnimationModule::ResolveFrame(const Particle& particle) const {
	const uint32_t safeTilesX = std::max(tilesX_, 1u);
	const uint32_t safeTilesY = std::max(tilesY_, 1u);
	const uint32_t totalFrames = safeTilesX * safeTilesY;
	const uint32_t playableFrames = GetPlayableFrameCount();
	const float progress = particle.GetLifeProgress();
	const float animated = progress * frameOverTime_ * static_cast<float>(std::max(cycles_, 1u));
	uint32_t frame = startFrame_;

	if (animationMode_ == AnimationMode::WholeSheet) {
		frame += static_cast<uint32_t>(animated * static_cast<float>(playableFrames)) % playableFrames;
		frame %= totalFrames;
	} else {
		frame += static_cast<uint32_t>(animated * static_cast<float>(playableFrames)) % playableFrames;
		frame %= safeTilesX;
	}

	return frame;
}

void TextureSheetAnimationModule::UpdateAnimation(Particle& particle, float) const {
	if (!enabled_) return;
	particle.sheetFrame = static_cast<int>(ResolveFrame(particle));
}

nlohmann::json TextureSheetAnimationModule::ToJson() const {
	nlohmann::json j;
	j["enabled"] = enabled_;
	j["tilesX"] = tilesX_;
	j["tilesY"] = tilesY_;
	j["frameOverTime"] = frameOverTime_;
	j["cycles"] = cycles_;
	j["frameCount"] = frameCount_;
	j["randomRow"] = randomRow_;
	j["rowIndex"] = rowIndex_;
	j["startFrame"] = startFrame_;
	j["animationMode"] = static_cast<int>(animationMode_);
	return j;
}

void TextureSheetAnimationModule::FromJson(const nlohmann::json& j) {
	if (j.contains("enabled")) enabled_ = j["enabled"];
	if (j.contains("tilesX")) tilesX_ = j["tilesX"].get<uint32_t>();
	if (j.contains("tilesY")) tilesY_ = j["tilesY"].get<uint32_t>();
	if (j.contains("frameOverTime")) frameOverTime_ = j["frameOverTime"];
	if (j.contains("cycles")) cycles_ = j["cycles"].get<uint32_t>();
	if (j.contains("frameCount")) frameCount_ = j["frameCount"].get<uint32_t>();
	if (j.contains("randomRow")) randomRow_ = j["randomRow"];
	if (j.contains("rowIndex")) rowIndex_ = j["rowIndex"].get<uint32_t>();
	if (j.contains("startFrame")) startFrame_ = j["startFrame"].get<uint32_t>();
	if (j.contains("animationMode")) {
		const int mode = j["animationMode"].get<int>();
		animationMode_ = mode == static_cast<int>(AnimationMode::SingleRow) ? AnimationMode::SingleRow : AnimationMode::WholeSheet;
	}
	ClampSettings();
}
}
