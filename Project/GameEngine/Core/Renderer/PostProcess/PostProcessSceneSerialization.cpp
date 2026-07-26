#include "pch.h"
#include "AntiAliasing.h"
#include "Bloom.h"
#include "BoxFilter.h"
#include "ChromaticAberration.h"
#include "Dissolve.h"
#include "GaussFilter.h"
#include "Outline.h"
#include "Pixelation.h"
#include "RadialBlur.h"
#include "ShockWave.h"
#include "SpeedLine.h"
#include "Vignette.h"
#include "WhiteNoise.h"
#include <cmath>
#include <nlohmann/json.hpp>

namespace {
using json = nlohmann::json;

float ReadFloat(const json& settings, const char* key, float fallback) {
   const auto it = settings.find(key);
   if (it == settings.end() || !it->is_number()) {
      return fallback;
   }

   try {
      const float value = it->get<float>();
      return std::isfinite(value) ? value : fallback;
   } catch (const json::exception&) {
      return fallback;
   }
}

int32_t ReadInt32(const json& settings, const char* key, int32_t fallback) {
   const auto it = settings.find(key);
   if (it == settings.end() || !it->is_number_integer()) {
      return fallback;
   }

   try {
      return it->get<int32_t>();
   } catch (const json::exception&) {
      return fallback;
   }
}

bool ReadBool(const json& settings, const char* key, bool fallback) {
   const auto it = settings.find(key);
   return (it != settings.end() && it->is_boolean()) ? it->get<bool>() : fallback;
}

std::string ReadString(const json& settings, const char* key, const std::string& fallback) {
   const auto it = settings.find(key);
   return (it != settings.end() && it->is_string()) ? it->get<std::string>() : fallback;
}

GameEngine::Vector2 ReadVector2(const json& settings, const char* key, const GameEngine::Vector2& fallback) {
   const auto it = settings.find(key);
   if (it == settings.end() || !it->is_array() || it->size() != 2 ||
      !(*it)[0].is_number() || !(*it)[1].is_number()) {
      return fallback;
   }

   try {
      const GameEngine::Vector2 value((*it)[0].get<float>(), (*it)[1].get<float>());
      return (std::isfinite(value.x) && std::isfinite(value.y)) ? value : fallback;
   } catch (const json::exception&) {
      return fallback;
   }
}

void ReadFloat4(const json& settings, const char* key, float (&values)[4]) {
   const auto it = settings.find(key);
   if (it == settings.end() || !it->is_array() || it->size() != 4) {
      return;
   }
   for (size_t index = 0; index < 4; ++index) {
      if (!(*it)[index].is_number()) {
         return;
      }
   }

   try {
      float parsedValues[4]{};
      for (size_t index = 0; index < 4; ++index) {
         parsedValues[index] = (*it)[index].get<float>();
         if (!std::isfinite(parsedValues[index])) {
            return;
         }
      }
      for (size_t index = 0; index < 4; ++index) {
         values[index] = parsedValues[index];
      }
   } catch (const json::exception&) {
      return;
   }
}
} // namespace

namespace GameEngine {

nlohmann::json AntiAliasing::SerializeSettings() const {
   return {
      { "contrastThreshold", contrastThreshold_ },
      { "relativeThreshold", relativeThreshold_ },
      { "subpixelBlending", subpixelBlending_ },
      { "edgeSearchSteps", edgeSearchSteps_ }
   };
}

bool AntiAliasing::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }
   contrastThreshold_ = ReadFloat(settings, "contrastThreshold", contrastThreshold_);
   relativeThreshold_ = ReadFloat(settings, "relativeThreshold", relativeThreshold_);
   subpixelBlending_ = ReadFloat(settings, "subpixelBlending", subpixelBlending_);
   edgeSearchSteps_ = ReadFloat(settings, "edgeSearchSteps", edgeSearchSteps_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json Bloom::SerializeSettings() const {
   return {
      { "threshold", threshold_ },
      { "softThreshold", softThreshold_ },
      { "intensity", intensity_ },
      { "blurRadius", blurRadius_ }
   };
}

bool Bloom::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }
   threshold_ = ReadFloat(settings, "threshold", threshold_);
   softThreshold_ = ReadFloat(settings, "softThreshold", softThreshold_);
   intensity_ = ReadFloat(settings, "intensity", intensity_);
   blurRadius_ = ReadFloat(settings, "blurRadius", blurRadius_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json BoxFilter::SerializeSettings() const {
   return { { "kernelRadius", kernelRadius_ } };
}

bool BoxFilter::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }
   kernelRadius_ = ReadInt32(settings, "kernelRadius", kernelRadius_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json ChromaticAberration::SerializeSettings() const {
   return {
      { "center", { centerX_, centerY_ } },
      { "pixelShift", pixelShift_ },
      { "useFixedDirection", useFixedDirection_ != 0 },
      { "fixedDirection", { fixedDirectionX_, fixedDirectionY_ } }
   };
}

bool ChromaticAberration::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }
   const Vector2 center = ReadVector2(settings, "center", Vector2(centerX_, centerY_));
   const Vector2 fixedDirection = ReadVector2(settings, "fixedDirection", Vector2(fixedDirectionX_, fixedDirectionY_));
   centerX_ = center.x;
   centerY_ = center.y;
   pixelShift_ = ReadFloat(settings, "pixelShift", pixelShift_);
   useFixedDirection_ = ReadBool(settings, "useFixedDirection", useFixedDirection_ != 0) ? 1 : 0;
   fixedDirectionX_ = fixedDirection.x;
   fixedDirectionY_ = fixedDirection.y;
   UpdateConstantBuffer();
   return true;
}

nlohmann::json Dissolve::SerializeSettings() const {
   return {
      { "threshold", params_.threshold },
      { "edgeWidth", params_.edgeWidth },
      { "edgeIntensity", params_.edgeIntensity },
      { "maskContrast", params_.maskContrast },
      { "maskTiling", { params_.maskTiling.x, params_.maskTiling.y } },
      { "maskOffset", { params_.maskOffset.x, params_.maskOffset.y } },
      { "edgeColor", { params_.edgeColor[0], params_.edgeColor[1], params_.edgeColor[2], params_.edgeColor[3] } },
      { "dissolveColor", { params_.dissolveColor[0], params_.dissolveColor[1], params_.dissolveColor[2], params_.dissolveColor[3] } },
      { "maskTexture", maskTextureName_ }
   };
}

bool Dissolve::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }
   DissolveParams params = params_;
   params.threshold = ReadFloat(settings, "threshold", params.threshold);
   params.edgeWidth = ReadFloat(settings, "edgeWidth", params.edgeWidth);
   params.edgeIntensity = ReadFloat(settings, "edgeIntensity", params.edgeIntensity);
   params.maskContrast = ReadFloat(settings, "maskContrast", params.maskContrast);
   params.maskTiling = ReadVector2(settings, "maskTiling", params.maskTiling);
   params.maskOffset = ReadVector2(settings, "maskOffset", params.maskOffset);
   ReadFloat4(settings, "edgeColor", params.edgeColor);
   ReadFloat4(settings, "dissolveColor", params.dissolveColor);
   SetParams(params);
   SetMaskTextureName(ReadString(settings, "maskTexture", maskTextureName_));
   return true;
}

nlohmann::json GaussFilter::SerializeSettings() const {
   return {
      { "intensity", intensity_ },
      { "kernelSize", kernelSize_ },
      { "sigma", sigma_ }
   };
}

bool GaussFilter::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }
   intensity_ = ReadFloat(settings, "intensity", intensity_);
   kernelSize_ = ReadInt32(settings, "kernelSize", kernelSize_);
   sigma_ = ReadFloat(settings, "sigma", sigma_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json Outline::SerializeSettings() const {
   return {
      { "outlineColor", { outlineColor_[0], outlineColor_[1], outlineColor_[2], outlineColor_[3] } },
      { "thickness", thickness_ },
      { "depthThreshold", depthThreshold_ },
      { "intensity", intensity_ }
   };
}

bool Outline::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }
   ReadFloat4(settings, "outlineColor", outlineColor_);
   thickness_ = ReadFloat(settings, "thickness", thickness_);
   depthThreshold_ = ReadFloat(settings, "depthThreshold", depthThreshold_);
   intensity_ = ReadFloat(settings, "intensity", intensity_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json Pixelation::SerializeSettings() const {
   return { { "pixelSize", pixelSize_ } };
}

bool Pixelation::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }
   pixelSize_ = ReadFloat(settings, "pixelSize", pixelSize_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json RadialBlur::SerializeSettings() const {
   return {
      { "center", { params_.center.x, params_.center.y } },
      { "strength", params_.strength },
      { "sampleCount", params_.sampleCount }
   };
}

bool RadialBlur::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }
   RadialBlurParams params = params_;
   params.center = ReadVector2(settings, "center", params.center);
   params.strength = ReadFloat(settings, "strength", params.strength);
   params.sampleCount = ReadInt32(settings, "sampleCount", params.sampleCount);
   SetParams(params);
   return true;
}

nlohmann::json ShockWave::SerializeSettings() const {
   return {
      { "center", { centerX_, centerY_ } },
      { "aspectRatio", { aspectRatioX_, aspectRatioY_ } },
      { "waveRadius", waveRadius_ },
      { "waveThickness", waveThickness_ },
      { "distortionStrength", distortionStrength_ },
      { "fadeOutRadius", fadeOutRadius_ },
      { "highlightIntensity", highlightIntensity_ }
   };
}

bool ShockWave::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }
   const Vector2 center = ReadVector2(settings, "center", Vector2(centerX_, centerY_));
   const Vector2 aspectRatio = ReadVector2(settings, "aspectRatio", Vector2(aspectRatioX_, aspectRatioY_));
   centerX_ = center.x;
   centerY_ = center.y;
   aspectRatioX_ = aspectRatio.x;
   aspectRatioY_ = aspectRatio.y;
   waveRadius_ = ReadFloat(settings, "waveRadius", waveRadius_);
   waveThickness_ = ReadFloat(settings, "waveThickness", waveThickness_);
   distortionStrength_ = ReadFloat(settings, "distortionStrength", distortionStrength_);
   fadeOutRadius_ = ReadFloat(settings, "fadeOutRadius", fadeOutRadius_);
   highlightIntensity_ = ReadFloat(settings, "highlightIntensity", highlightIntensity_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json SpeedLine::SerializeSettings() const {
   return {
      { "center", { params_.center.x, params_.center.y } },
      { "intensity", params_.intensity },
      { "lineDensity", params_.lineDensity },
      { "thickness", params_.thickness },
      { "innerRadius", params_.innerRadius },
      { "outerRadius", params_.outerRadius },
      { "time", params_.time },
      { "randomSeed", params_.randomSeed },
      { "flowSpeed", params_.flowSpeed }
   };
}

bool SpeedLine::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }
   SpeedLineParams params = params_;
   params.center = ReadVector2(settings, "center", params.center);
   params.intensity = ReadFloat(settings, "intensity", params.intensity);
   params.lineDensity = ReadFloat(settings, "lineDensity", params.lineDensity);
   params.thickness = ReadFloat(settings, "thickness", params.thickness);
   params.innerRadius = ReadFloat(settings, "innerRadius", params.innerRadius);
   params.outerRadius = ReadFloat(settings, "outerRadius", params.outerRadius);
   params.time = ReadFloat(settings, "time", params.time);
   params.randomSeed = ReadFloat(settings, "randomSeed", params.randomSeed);
   params.flowSpeed = ReadFloat(settings, "flowSpeed", params.flowSpeed);
   SetParams(params);
   return true;
}

nlohmann::json Vignette::SerializeSettings() const {
   return {
      { "center", { centerX_, centerY_ } },
      { "radius", radius_ },
      { "softness", softness_ },
      { "color", { vignetteColorR_, vignetteColorG_, vignetteColorB_ } },
      { "intensity", intensity_ }
   };
}

bool Vignette::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }
   const Vector2 center = ReadVector2(settings, "center", Vector2(centerX_, centerY_));
   centerX_ = center.x;
   centerY_ = center.y;
   radius_ = ReadFloat(settings, "radius", radius_);
   softness_ = ReadFloat(settings, "softness", softness_);
   const auto color = settings.find("color");
   if (color != settings.end() && color->is_array() && color->size() == 3 &&
      (*color)[0].is_number() && (*color)[1].is_number() && (*color)[2].is_number()) {
      try {
         const float red = (*color)[0].get<float>();
         const float green = (*color)[1].get<float>();
         const float blue = (*color)[2].get<float>();
         if (std::isfinite(red) && std::isfinite(green) && std::isfinite(blue)) {
            vignetteColorR_ = red;
            vignetteColorG_ = green;
            vignetteColorB_ = blue;
         }
      } catch (const nlohmann::json::exception&) {
         // Keep the previous color when a numeric conversion is out of range.
      }
   }
   intensity_ = ReadFloat(settings, "intensity", intensity_);
   UpdateConstantBuffer();
   return true;
}

nlohmann::json WhiteNoise::SerializeSettings() const {
   // Time is advanced by rendering and is intentionally excluded from authored scene state.
   return {
      { "noiseDensity", params_.noiseDensity },
      { "seedChangeRate", params_.seedChangeRate },
      { "noiseThreshold", params_.noiseThreshold },
      { "noiseIntensity", params_.noiseIntensity }
   };
}

bool WhiteNoise::DeserializeSettings(const nlohmann::json& settings) {
   if (!settings.is_object()) {
      return false;
   }
   WhiteNoiseParams params = params_;
   params.time = 0.0f;
   params.noiseDensity = ReadFloat(settings, "noiseDensity", params.noiseDensity);
   params.seedChangeRate = ReadFloat(settings, "seedChangeRate", params.seedChangeRate);
   params.noiseThreshold = ReadFloat(settings, "noiseThreshold", params.noiseThreshold);
   params.noiseIntensity = ReadFloat(settings, "noiseIntensity", params.noiseIntensity);
   SetParams(params);
   previousTime_ = std::chrono::steady_clock::now();
   hasPreviousTime_ = true;
   return true;
}

} // namespace GameEngine
