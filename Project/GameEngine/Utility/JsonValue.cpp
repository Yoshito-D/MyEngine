#include "GameEngine/pch.h"
#include "GameEngine/Utility/JsonValue.h"
#include "GameEngine/Utility/Logger.h"
#include <array>
#include <cmath>
#include <limits>
#include <span>

namespace GameEngine::JsonValue {
namespace {

void ReadComponents(const nlohmann::json& data, std::span<float> output, std::source_location location) {
   if (!data.is_array() || data.size() != output.size()) {
      Logger::Fail("Invalid vector JSON: expected " + std::to_string(output.size()) + " components",
         Logger::LogChannel::Engine, location);
   }

   for (size_t index = 0; index < output.size(); ++index) {
      const auto& component = data[index];

      // JSONの保持値をfloatへ狭める前に、表現範囲をdoubleのまま検証する。
      if (!component.is_number() || !std::isfinite(component.get<double>()) ||
          std::abs(component.get<double>()) > std::numeric_limits<float>::max()) {
         Logger::Fail("Invalid vector JSON: expected a finite number at index " + std::to_string(index),
            Logger::LogChannel::Engine, location);
      }

      output[index] = component.get<float>();
   }
}

} // namespace

nlohmann::json WriteVector3(const Vector3& value) {
   return nlohmann::json::array({value.x, value.y, value.z});
}

Vector2 ReadVector2(const nlohmann::json& data, std::source_location location) {
   std::array<float, 2> values;
   ReadComponents(data, values, location);
   return Vector2(values[0], values[1]);
}

Vector3 ReadVector3(const nlohmann::json& data, std::source_location location) {
   std::array<float, 3> values;
   ReadComponents(data, values, location);
   return Vector3(values[0], values[1], values[2]);
}

Vector4 ReadVector4(const nlohmann::json& data, std::source_location location) {
   std::array<float, 4> values;
   ReadComponents(data, values, location);
   return Vector4(values[0], values[1], values[2], values[3]);
}

float ReadFloat(const nlohmann::json& data, const char* key, float defaultValue) {
   const auto it = data.find(key);

   if (it == data.end()) {
      return defaultValue;
   }

   if (it->is_number()) {
      const double value = it->get<double>();

      if (std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max()) {
         return static_cast<float>(value);
      }
   }

   Logger::Fail(std::string("Invalid JSON number: ") + key);
}

bool ReadBool(const nlohmann::json& data, const char* key, bool defaultValue) {
   const auto it = data.find(key);

   if (it == data.end()) {
      return defaultValue;
   }

   if (!it->is_boolean()) {
      Logger::Fail(std::string("Invalid JSON boolean: ") + key);
   }

   return it->get<bool>();
}

std::string ReadString(const nlohmann::json& data, const char* key, const std::string& defaultValue) {
   const auto it = data.find(key);

   if (it == data.end()) {
      return defaultValue;
   }

   if (!it->is_string()) {
      Logger::Fail(std::string("Invalid JSON string: ") + key);
   }

   return it->get<std::string>();
}

} // namespace GameEngine::JsonValue
