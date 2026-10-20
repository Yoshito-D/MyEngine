#include "GameEngine/pch.h"
#include <cstdint>
#include "GameEngine/Effects/Particles/Modules/TrailModule.h"

namespace GameEngine {
nlohmann::json TrailModule::ToJson() const {
   return {{"enabled", IsEnabled()}, {"mode", static_cast<std::int32_t>(mode_)}, {"width", width_.ToJson()},
      {"maxPoints", maxPoints_}, {"minDistance", minDistance_}, {"textureName", textureName_},
      {"color", {color_.x, color_.y, color_.z, color_.w}}, {"retractionDuration", retractionDuration_},
      {"tailWidthScale", tailWidthScale_}, {"textureTiling", textureTiling_}};
}

void TrailModule::FromJson(const nlohmann::json& j) {
   if (j.contains("enabled")) {
      SetEnabled(j.at("enabled").get<bool>());
   }

   if (j.contains("mode")) {
      const std::int32_t mode = j.at("mode").get<std::int32_t>();

      if (mode >= static_cast<std::int32_t>(TrailMode::ParticlePath) &&
          mode <= static_cast<std::int32_t>(TrailMode::EmitterToParticle)) {
         SetMode(static_cast<TrailMode>(mode));
      }
   }

   if (j.contains("width")) {
      width_.FromJson(j.at("width"));
   }

   if (j.contains("maxPoints")) {
      SetMaxPoints(j.at("maxPoints"));
   }

   if (j.contains("minDistance")) {
      SetMinDistance(j.at("minDistance"));
   }

   if (j.contains("textureName")) {
      SetTextureName(j.at("textureName").get<std::string>());
   }

   if (j.contains("color") && j.at("color").is_array() && j.at("color").size() == 4) {
      const auto& color = j.at("color");
      SetColor(Vector4(color[0], color[1], color[2], color[3]));
   }

   if (j.contains("retractionDuration")) {
      SetRetractionDuration(j.at("retractionDuration"));
   }

   if (j.contains("tailWidthScale")) {
      SetTailWidthScale(j.at("tailWidthScale"));
   }

   if (j.contains("textureTiling")) {
      SetTextureTiling(j.at("textureTiling"));
   }
}
}
