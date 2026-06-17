#include "pch.h"
#include "MainModule.h"

namespace GameEngine {
    // ============================================================
    // RandomFloat, RandomVector3, RandomColor Implementation
    // ============================================================
    
    float RandomFloat::GetValue() const {
        if (!randomize || minValue == maxValue) return minValue;
        return RandomUtils::Random(minValue, maxValue);
    }

    Vector2 RandomVector2::GetValue() const {
        if (!randomize) return minValue;
        return Vector2{RandomUtils::Random(minValue.x, maxValue.x), RandomUtils::Random(minValue.y, maxValue.y)};
    }

    Vector3 RandomVector3::GetValue() const {
        if (!randomize) return minValue;
        return Vector3(RandomUtils::Random(minValue.x, maxValue.x), RandomUtils::Random(minValue.y, maxValue.y), RandomUtils::Random(minValue.z, maxValue.z));
    }

    uint32_t RandomColor::GetValue() const {
        if (!randomize || minValue == maxValue) return minValue;
        return RandomLerpRGBAColor(minValue, maxValue);
    }
    
    nlohmann::json RandomFloat::ToJson() const {
        return {{"min", minValue}, {"max", maxValue}, {"randomize", randomize}};
    }

    void RandomFloat::FromJson(const nlohmann::json& json) {
        if (json.contains("min")) minValue = json["min"];
        if (json.contains("max")) maxValue = json["max"];
        if (json.contains("randomize")) randomize = json["randomize"];
    }

    nlohmann::json RandomVector2::ToJson() const {
        return {
            {"min", {minValue.x, minValue.y}},
            {"max", {maxValue.x, maxValue.y}},
            {"randomize", randomize}
        };
    }

    void RandomVector2::FromJson(const nlohmann::json& json) {
        if (json.contains("min") && json["min"].is_array() && json["min"].size() >= 2) {
            minValue.x = json["min"][0]; minValue.y = json["min"][1];
        }
        if (json.contains("max") && json["max"].is_array() && json["max"].size() >= 2) {
            maxValue.x = json["max"][0]; maxValue.y = json["max"][1];
        }
        if (json.contains("randomize")) randomize = json["randomize"];
    }

    nlohmann::json RandomVector3::ToJson() const {
        return {
            {"min", {minValue.x, minValue.y, minValue.z}},
            {"max", {maxValue.x, maxValue.y, maxValue.z}},
            {"randomize", randomize}
        };
    }

    void RandomVector3::FromJson(const nlohmann::json& json) {
        if (json.contains("min") && json["min"].is_array() && json["min"].size() >= 3) {
            minValue.x = json["min"][0]; minValue.y = json["min"][1]; minValue.z = json["min"][2];
        }
        if (json.contains("max") && json["max"].is_array() && json["max"].size() >= 3) {
            maxValue.x = json["max"][0]; maxValue.y = json["max"][1]; maxValue.z = json["max"][2];
        }
        if (json.contains("randomize")) randomize = json["randomize"];
    }

    nlohmann::json RandomColor::ToJson() const {
        return {{"min", minValue}, {"max", maxValue}, {"randomize", randomize}};
    }

    void RandomColor::FromJson(const nlohmann::json& json) {
        if (json.contains("min")) minValue = json["min"];
        if (json.contains("max")) maxValue = json["max"];
        if (json.contains("randomize")) randomize = json["randomize"];
    }

    // ============================================================
    // MainModule
    // ============================================================
    
    MainModule::MainModule() = default;

    nlohmann::json MainModule::ToJson() const {
        nlohmann::json j;
        
        j["duration"] = duration_;
        j["looping"] = looping_;
        j["startLifetime"] = startLifetime_.ToJson();
        j["startSpeed"] = startSpeed_.ToJson();
        j["startSpeedMode"] = static_cast<int>(startSpeedMode_);
        j["startVelocity"] = startVelocity_.ToJson();
        j["startSize"] = startSize_.ToJson();
        j["startRotation"] = startRotation_.ToJson();
        j["startColor"] = startColor_.ToJson();
        
        j["gravityModifier"] = gravityModifier_;
        j["simulationSpace"] = static_cast<int>(simulationSpace_);
        j["playOnAwake"] = playOnAwake_;
        j["scalingMode"] = static_cast<int>(scalingMode_);
        j["maxParticles"] = maxParticles_;
        j["emissionRate"] = emissionRate_;
        
        return j;
    }

    void MainModule::FromJson(const nlohmann::json& j) {
        if (j.contains("duration")) duration_ = j["duration"];
        if (j.contains("looping")) looping_ = j["looping"];
        
        if (j.contains("startLifetime")) {
            if (j["startLifetime"].is_object()) {
                startLifetime_.FromJson(j["startLifetime"]);
            } else {
                float value = j["startLifetime"];
                startLifetime_ = RandomFloat(value, value, false);
            }
        }
        
        const bool hasStartSpeedMode = j.contains("startSpeedMode");
        if (hasStartSpeedMode) {
            startSpeedMode_ = static_cast<StartSpeedMode>(j["startSpeedMode"].get<int>());
        }

        if (j.contains("startSpeed")) {
            const auto& startSpeedJson = j["startSpeed"];
            if (startSpeedJson.is_object()) {
                const bool isVectorRange =
                    startSpeedJson.contains("min") && startSpeedJson["min"].is_array();
                if (isVectorRange) {
                    startVelocity_.FromJson(startSpeedJson);
                    if (!hasStartSpeedMode) {
                        startSpeedMode_ = StartSpeedMode::Vector3;
                    }
                } else {
                    startSpeed_.FromJson(startSpeedJson);
                }
            } else if (startSpeedJson.is_array() && startSpeedJson.size() >= 3) {
                Vector3 value{startSpeedJson[0], startSpeedJson[1], startSpeedJson[2]};
                startVelocity_ = RandomVector3(value, value, false);
                if (!hasStartSpeedMode) {
                    startSpeedMode_ = StartSpeedMode::Vector3;
                }
            } else {
                float value = startSpeedJson;
                startSpeed_ = RandomFloat(value, value, false);
            }
        }

        if (j.contains("startVelocity")) {
            if (j["startVelocity"].is_object()) {
                startVelocity_.FromJson(j["startVelocity"]);
            } else if (j["startVelocity"].is_array() && j["startVelocity"].size() >= 3) {
                auto arr = j["startVelocity"];
                Vector3 value{arr[0], arr[1], arr[2]};
                startVelocity_ = RandomVector3(value, value, false);
            }
            if (!hasStartSpeedMode) {
                startSpeedMode_ = StartSpeedMode::Vector3;
            }
        }
        
        if (j.contains("startSize")) {
            if (j["startSize"].is_object()) {
                startSize_.FromJson(j["startSize"]);
            } else {
                float value = j["startSize"];
                Vector3 v(value, value, value);
                startSize_ = RandomVector3(v, v, false);
            }
        }
        
        if (j.contains("startRotation")) {
            if (j["startRotation"].is_object()) {
                startRotation_.FromJson(j["startRotation"]);
            } else if (j["startRotation"].is_array()) {
                auto arr = j["startRotation"];
                Vector3 value{arr[0], arr[1], arr[2]};
                startRotation_ = RandomVector3(value, value, false);
            }
        }
        
        if (j.contains("startColor")) {
            if (j["startColor"].is_object()) {
                startColor_.FromJson(j["startColor"]);
            } else if (j["startColor"].is_number()) {
                uint32_t value = j["startColor"];
                startColor_ = RandomColor(value, value, false);
            }
        }
        
        if (j.contains("gravityModifier")) gravityModifier_ = j["gravityModifier"];
        if (j.contains("simulationSpace")) simulationSpace_ = static_cast<SimulationSpace>(j["simulationSpace"].get<int>());
        if (j.contains("playOnAwake")) playOnAwake_ = j["playOnAwake"];
        if (j.contains("scalingMode")) scalingMode_ = static_cast<ScalingMode>(j["scalingMode"].get<int>());
        if (j.contains("maxParticles")) maxParticles_ = j["maxParticles"];
        if (j.contains("emissionRate")) emissionRate_ = j["emissionRate"];
    }
}
