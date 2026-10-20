#include "GameEngine/pch.h"
#include <cstdint>
#include "GameEngine/Effects/Particles/Modules/MainModule.h"

namespace GameEngine {
// ============================================================
// RandomFloat、RandomVector3、RandomColorの実装
// ============================================================

float RandomFloat::GetValue() const {
   if (!randomized_ || minimum_ == maximum_) {
      return minimum_;
   }

   return RandomUtils::Random(minimum_, maximum_);
}

Vector2 RandomVector2::GetValue() const {
   if (!randomized_) {
      return minimum_;
   }

   // 各成分を独立抽選し、矩形範囲内の任意の組み合わせを生成できるようにする。
   return Vector2{RandomUtils::Random(minimum_.x, maximum_.x), RandomUtils::Random(minimum_.y, maximum_.y)};
}

Vector3 RandomVector3::GetValue() const {
   if (!randomized_) {
      return minimum_;
   }

   // 一つの補間率を共有せず、軸ごとの最小・最大で直方体の乱数分布を作る。
   return Vector3(RandomUtils::Random(minimum_.x, maximum_.x), RandomUtils::Random(minimum_.y, maximum_.y),
      RandomUtils::Random(minimum_.z, maximum_.z));
}

uint32_t RandomColor::GetValue() const {
   if (!randomize || minValue == maxValue) {
      return minValue;
   }

   return RandomLerpRGBAColor(minValue, maxValue);
}

nlohmann::json RandomFloat::ToJson() const {
   return {{"min", minimum_}, {"max", maximum_}, {"randomize", randomized_}};
}

void RandomFloat::FromJson(const nlohmann::json& json) {
   *this = RandomFloat(json.value("min", minimum_), json.value("max", maximum_), json.value("randomize", randomized_));
}

nlohmann::json RandomVector2::ToJson() const {
   return {{"min", {minimum_.x, minimum_.y}}, {"max", {maximum_.x, maximum_.y}}, {"randomize", randomized_}};
}

void RandomVector2::FromJson(const nlohmann::json& json) {
   auto low = minimum_;
   auto high = maximum_;

   if (json.contains("min") && json["min"].is_array() && json["min"].size() >= 2) {
      low.x = json["min"][0].get<float>();
      low.y = json["min"][1].get<float>();
   }

   if (json.contains("max") && json["max"].is_array() && json["max"].size() >= 2) {
      high.x = json["max"][0].get<float>();
      high.y = json["max"][1].get<float>();
   }

   *this = RandomVector2(low, high, json.value("randomize", randomized_));
}

nlohmann::json RandomVector3::ToJson() const {
   return {{"min", {minimum_.x, minimum_.y, minimum_.z}}, {"max", {maximum_.x, maximum_.y, maximum_.z}},
      {"randomize", randomized_}};
}

void RandomVector3::FromJson(const nlohmann::json& json) {
   auto low = minimum_;
   auto high = maximum_;

   if (json.contains("min") && json["min"].is_array() && json["min"].size() >= 3) {
      low.x = json["min"][0].get<float>();
      low.y = json["min"][1].get<float>();
      low.z = json["min"][2].get<float>();
   }

   if (json.contains("max") && json["max"].is_array() && json["max"].size() >= 3) {
      high.x = json["max"][0].get<float>();
      high.y = json["max"][1].get<float>();
      high.z = json["max"][2].get<float>();
   }

   *this = RandomVector3(low, high, json.value("randomize", randomized_));
}

nlohmann::json RandomColor::ToJson() const {
   return {{"min", minValue}, {"max", maxValue}, {"randomize", randomize}};
}

void RandomColor::FromJson(const nlohmann::json& json) {
   if (json.contains("min")) {
      minValue = json["min"];
   }

   if (json.contains("max")) {
      maxValue = json["max"];
   }

   if (json.contains("randomize")) {
      randomize = json["randomize"];
   }
}

// ============================================================
// メインモジュール
// ============================================================

MainModule::MainModule() = default;

nlohmann::json MainModule::ToJson() const {
   nlohmann::json j;

   j["duration"] = duration_;
   j["looping"] = looping_;
   j["startLifetime"] = startLifetime_.ToJson();
   j["startSpeed"] = startSpeed_.ToJson();
   j["startSpeedMode"] = static_cast<std::int32_t>(startSpeedMode_);
   j["startVelocity"] = startVelocity_.ToJson();
   j["startSize"] = startSize_.ToJson();
   j["startRotation"] = startRotation_.ToJson();
   j["startColor"] = startColor_.ToJson();

   j["gravityModifier"] = gravityModifier_.ToJson();
   j["timeScale"] = timeScale_;
   j["simulationSpace"] = static_cast<std::int32_t>(simulationSpace_);
   j["playOnAwake"] = playOnAwake_;
   j["maxParticles"] = maxParticles_;

   return j;
}

void MainModule::FromJson(const nlohmann::json& j) {
   auto readRange = [&j](const char* key, auto& value) {
      if (j.contains(key)) {
         value.FromJson(j.at(key));
      }
   };

   readRange("startLifetime", startLifetime_);
   readRange("startSpeed", startSpeed_);
   readRange("startVelocity", startVelocity_);
   readRange("startSize", startSize_);
   readRange("startRotation", startRotation_);
   readRange("startColor", startColor_);
   readRange("gravityModifier", gravityModifier_);

   if (j.contains("duration")) {
      duration_ = j.at("duration");
   }

   if (j.contains("looping")) {
      looping_ = j.at("looping");
   }

   if (j.contains("startSpeedMode")) {
      startSpeedMode_ = static_cast<StartSpeedMode>(j.at("startSpeedMode").get<std::int32_t>());
   }

   if (j.contains("timeScale")) {
      SetTimeScale(j.at("timeScale"));
   }

   if (j.contains("simulationSpace")) {
      simulationSpace_ = static_cast<SimulationSpace>(j.at("simulationSpace").get<std::int32_t>());
   }

   if (j.contains("playOnAwake")) {
      playOnAwake_ = j.at("playOnAwake");
   }

   if (j.contains("maxParticles")) {
      maxParticles_ = j.at("maxParticles");
   }
}
}

namespace GameEngine {

RandomFloat::RandomFloat(float low, float high, bool randomize) : randomized_(randomize) {
   minimum_ = std::isfinite(low) ? low : 0.0f;

   // 固定値では未使用の上限を破棄し、範囲の並べ替えによる指定値の変化を防ぐ。
   maximum_ = randomized_ && std::isfinite(high) ? high : minimum_;

   if (maximum_ < minimum_) {
      std::swap(minimum_, maximum_);
   }
}

RandomVector2::RandomVector2(const Vector2& low, const Vector2& high, bool randomize) : randomized_(randomize) {
   minimum_.x = std::isfinite(low.x) ? low.x : 0.0f;
   maximum_.x = randomized_ && std::isfinite(high.x) ? high.x : minimum_.x;

   if (maximum_.x < minimum_.x) {
      std::swap(minimum_.x, maximum_.x);
   }

   minimum_.y = std::isfinite(low.y) ? low.y : 0.0f;
   maximum_.y = randomized_ && std::isfinite(high.y) ? high.y : minimum_.y;

   if (maximum_.y < minimum_.y) {
      std::swap(minimum_.y, maximum_.y);
   }
}

RandomVector3::RandomVector3(const Vector3& low, const Vector3& high, bool randomize) : randomized_(randomize) {
   minimum_.x = std::isfinite(low.x) ? low.x : 0.0f;
   maximum_.x = randomized_ && std::isfinite(high.x) ? high.x : minimum_.x;

   if (maximum_.x < minimum_.x) {
      std::swap(minimum_.x, maximum_.x);
   }

   minimum_.y = std::isfinite(low.y) ? low.y : 0.0f;
   maximum_.y = randomized_ && std::isfinite(high.y) ? high.y : minimum_.y;

   if (maximum_.y < minimum_.y) {
      std::swap(minimum_.y, maximum_.y);
   }

   minimum_.z = std::isfinite(low.z) ? low.z : 0.0f;
   maximum_.z = randomized_ && std::isfinite(high.z) ? high.z : minimum_.z;

   if (maximum_.z < minimum_.z) {
      std::swap(minimum_.z, maximum_.z);
   }
}

RandomColor::RandomColor(uint32_t minVal, uint32_t maxVal, bool enableRandom)
   : minValue(minVal), maxValue(maxVal), randomize(enableRandom) {
}

void MainModule::SetStartVelocity(const RandomVector3& velocity) {
   startVelocity_ = velocity;
   startSpeedMode_ = StartSpeedMode::Vector3;
}

} // namespace GameEngine
