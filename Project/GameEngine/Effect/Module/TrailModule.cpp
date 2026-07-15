#include "pch.h"
#include "TrailModule.h"

namespace GameEngine {
nlohmann::json TrailModule::ToJson() const {
	return {
	  { "enabled", enabled_ },
	  { "mode", static_cast<int>(mode_) },
	  { "width", width_.ToJson() },
	  { "maxPoints", maxPoints_ },
	  { "minDistance", minDistance_ },
	  { "textureName", textureName_ },
	  { "color", { color_.x, color_.y, color_.z, color_.w } },
	  { "retractionDuration", retractionDuration_ },
	  { "tailWidthScale", tailWidthScale_ },
	  { "textureTiling", textureTiling_ }
   };
}

void TrailModule::FromJson(const nlohmann::json& j) {
   // 旧RendererModuleでは有効フラグだけがribbonEnabledだったため、こちらを優先する。
	if (j.contains("ribbonEnabled")) enabled_ = j["ribbonEnabled"];
	else if (j.contains("enabled")) enabled_ = j["enabled"];
	if (j.contains("mode")) {
	  const int mode = j["mode"].get<int>();
	  if (mode >= static_cast<int>(TrailMode::ParticlePath) &&
		 mode <= static_cast<int>(TrailMode::EmitterToParticle)) {
		 SetMode(static_cast<TrailMode>(mode));
	  }
	} else {
	  // モード項目が存在しない旧設定は、従来と同じ移動軌跡として読み込む。
	  SetMode(TrailMode::ParticlePath);
	}

   const char* widthKey = j.contains("width") ? "width" : "ribbonWidth";
   if (j.contains(widthKey)) {
	  if (j[widthKey].is_object()) width_.FromJson(j[widthKey]);
	  else width_ = RandomFloat(j[widthKey].get<float>());
   }
   if (j.contains("maxPoints")) SetMaxPoints(j["maxPoints"]);
   else if (j.contains("ribbonMaxPoints")) SetMaxPoints(j["ribbonMaxPoints"]);
   if (j.contains("minDistance")) SetMinDistance(j["minDistance"]);
   else if (j.contains("ribbonMinDistance")) SetMinDistance(j["ribbonMinDistance"]);
   if (j.contains("textureName")) SetTextureName(j["textureName"].get<std::string>());
   else if (j.contains("ribbonTextureName")) SetTextureName(j["ribbonTextureName"].get<std::string>());

   const char* colorKey = j.contains("color") ? "color" : "ribbonColor";
   if (j.contains(colorKey) && j[colorKey].is_array() && j[colorKey].size() >= 4) {
	  const auto& color = j[colorKey];
	  SetColor(Vector4(color[0], color[1], color[2], color[3]));
   }
   if (j.contains("retractionDuration")) SetRetractionDuration(j["retractionDuration"]);
   else if (j.contains("ribbonFadeDuration")) SetRetractionDuration(j["ribbonFadeDuration"]);
   if (j.contains("tailWidthScale")) SetTailWidthScale(j["tailWidthScale"]);
   else if (j.contains("ribbonTailWidthScale")) SetTailWidthScale(j["ribbonTailWidthScale"]);
   if (j.contains("textureTiling")) SetTextureTiling(j["textureTiling"]);
   else if (j.contains("ribbonTextureTiling")) SetTextureTiling(j["ribbonTextureTiling"]);
}
}
