#include "GameEngine/pch.h"
#include "GameEngine/Utility/JsonValue.h"
#include "GameEngine/Utility/Logger.h"
#include <array>
#include <cmath>
#include <limits>
#include <span>

namespace GameEngine::JsonValue {

nlohmann::json WriteVector3(const Vector3& value) {
   return nlohmann::json::array({value.x, value.y, value.z});
}

} // namespace GameEngine::JsonValue

namespace GameEngine::JsonValue {
namespace {

bool ReadComponents(const nlohmann::json& data, std::span<float> output) {
   if (data.is_array() && data.size() == output.size()) {
      // JSONの保持値をfloatへ狭める前に、表現範囲をdoubleのまま検証する。
      for (const auto& value : data) {
         if (!value.is_number() || !std::isfinite(value.get<double>()) ||
             std::abs(value.get<double>()) > std::numeric_limits<float>::max()) {
            Logger::Warning("Invalid vector JSON: expected finite numbers");
            return false;
         }
      }

      for (size_t index = 0; index < output.size(); ++index) {
         output[index] = data[index].get<float>();
      }

      return true;
   }

   Logger::Warning("Invalid vector JSON: incorrect component count");
   return false;
}

} // namespace
} // namespace GameEngine::JsonValue

namespace GameEngine::JsonValue {

Vector2 ReadVector2(const nlohmann::json& data, const Vector2& fallback) {
   std::array<float, 2> values;
   return ReadComponents(data, values) ? Vector2(values[0], values[1]) : fallback;
}

Vector3 ReadVector3(const nlohmann::json& data, const Vector3& fallback) {
   std::array<float, 3> values;
   return ReadComponents(data, values) ? Vector3(values[0], values[1], values[2]) : fallback;
}

Vector4 ReadVector4(const nlohmann::json& data, const Vector4& fallback) {
   std::array<float, 4> values;
   return ReadComponents(data, values) ? Vector4(values[0], values[1], values[2], values[3]) : fallback;
}

float ReadFloat(const nlohmann::json& data, const char* key, float fallback) {
   const auto it = data.find(key);

   if (it != data.end() && it->is_number()) {
      // 範囲外の値をfloatへ変換してから検証すると、オーバーフローを防げない。
      const double value = it->get<double>();

      if (std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max()) {
         return static_cast<float>(value);
      }
   }

   return fallback;
}

bool ReadBool(const nlohmann::json& data, const char* key, bool fallback) {
   const auto it = data.find(key);
   return it != data.end() && it->is_boolean() ? it->get<bool>() : fallback;
}

} // namespace GameEngine::JsonValue
