#include "pch.h"
#include "TextureSheetAnimationModule.h"

namespace GameEngine {
TextureSheetAnimationModule::TextureSheetAnimationModule() = default;

void TextureSheetAnimationModule::InitializeParticle(Particle& particle) const {
	if (!enabled_) return;
	particle.sheetFrame = startFrame_;
	particle.sheetRow = randomRow_ ? RandomUtils::Random<int>(0, static_cast<int>(tilesY_ > 0 ? tilesY_ - 1 : 0)) : static_cast<int>(rowIndex_);
}

uint32_t TextureSheetAnimationModule::ResolveFrame(const Particle& particle) const {
	const uint32_t safeTilesX = (std::max)(tilesX_, 1u);
	const uint32_t safeTilesY = (std::max)(tilesY_, 1u);
	const uint32_t totalFrames = safeTilesX * safeTilesY;
	const float progress = particle.GetLifeProgress();
	const float animated = progress * frameOverTime_ * static_cast<float>((std::max)(cycles_, 1u));
	uint32_t frame = startFrame_;

	if (animationMode_ == AnimationMode::WholeSheet) {
		frame += static_cast<uint32_t>(animated * static_cast<float>(totalFrames));
		frame %= totalFrames;
	} else {
		frame += static_cast<uint32_t>(animated * static_cast<float>(safeTilesX));
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
	if (j.contains("randomRow")) randomRow_ = j["randomRow"];
	if (j.contains("rowIndex")) rowIndex_ = j["rowIndex"].get<uint32_t>();
	if (j.contains("startFrame")) startFrame_ = j["startFrame"].get<uint32_t>();
	if (j.contains("animationMode")) animationMode_ = static_cast<AnimationMode>(j["animationMode"].get<int>());
}
}
