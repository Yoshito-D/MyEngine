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
        
        if (j.contains("startSpeed")) {
            if (j["startSpeed"].is_object()) {
                startSpeed_.FromJson(j["startSpeed"]);
            } else {
                float value = j["startSpeed"];
                startSpeed_ = RandomFloat(value, value, false);
            }
        }
        
        if (j.contains("startSize")) {
            if (j["startSize"].is_object()) {
                startSize_.FromJson(j["startSize"]);
            } else {
                float value = j["startSize"];
                startSize_ = RandomFloat(value, value, false);
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
