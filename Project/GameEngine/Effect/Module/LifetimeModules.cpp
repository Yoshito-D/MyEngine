#include "pch.h"
#include "LifetimeModules.h"
#include <cmath>

namespace GameEngine {
    // ============================================================
    // VelocityOverLifetimeModule
    // ============================================================
    VelocityOverLifetimeModule::VelocityOverLifetimeModule() = default;

    void VelocityOverLifetimeModule::ApplyVelocity(Particle& particle, float deltaTime) const {
        if (!enabled_) return;
        
        particle.velocity += linearVelocity_ * deltaTime;
        
        if (speedModifier_ != 1.0f) {
            particle.velocity = particle.velocity * speedModifier_;
        }
    }

    // ============================================================
    // LimitVelocityOverLifetimeModule
    // ============================================================
    LimitVelocityOverLifetimeModule::LimitVelocityOverLifetimeModule() = default;

    void LimitVelocityOverLifetimeModule::LimitVelocity(Particle& particle) const {
        if (!enabled_) return;

        float speed = particle.velocity.Length();
        if (speed > speedLimit_) {
            float excess = speed - speedLimit_;
            particle.velocity = particle.velocity * (1.0f - dampen_ * excess / speed);
        }
    }

    // ============================================================
    // ForceOverLifetimeModule
    // ============================================================
    ForceOverLifetimeModule::ForceOverLifetimeModule() = default;

    void ForceOverLifetimeModule::ApplyForce(Particle& particle) const {
        if (!enabled_) return;
        particle.acceleration += force_;
    }

    // ============================================================
    // ColorOverLifetimeModule
    // ============================================================
    ColorOverLifetimeModule::ColorOverLifetimeModule() = default;

    void ColorOverLifetimeModule::UpdateColor(Particle& particle) const {
        if (!enabled_) return;

        float t = particle.GetLifeProgress();
        particle.color.x = startColor_.x + (endColor_.x - startColor_.x) * t;
        particle.color.y = startColor_.y + (endColor_.y - startColor_.y) * t;
        particle.color.z = startColor_.z + (endColor_.z - startColor_.z) * t;
        particle.color.w = startColor_.w + (endColor_.w - startColor_.w) * t;
    }

    // ============================================================
    // SizeOverLifetimeModule
    // ============================================================
    SizeOverLifetimeModule::SizeOverLifetimeModule() = default;

    void SizeOverLifetimeModule::UpdateSize(Particle& particle) const {
        if (!enabled_) return;

        float t = particle.GetLifeProgress();
        float size = (startSize_ + (endSize_ - startSize_) * t) * sizeMultiplier_;
        particle.currentSize = size;
        particle.transform.scale = Vector3(size, size, size);
    }

    // ============================================================
    // RotationOverLifetimeModule
    // ============================================================
    RotationOverLifetimeModule::RotationOverLifetimeModule() = default;

    void RotationOverLifetimeModule::UpdateRotation(Particle& particle, float deltaTime) const {
        // モジュールが無効でも、パーティクルに角速度が設定されていれば回転を適用
        // これにより、生成時に設定された角速度が常に機能する
        particle.transform.rotation.x += particle.angularVelocity.x * deltaTime;
        particle.transform.rotation.y += particle.angularVelocity.y * deltaTime;
        particle.transform.rotation.z += particle.angularVelocity.z * deltaTime;
    }

    Vector3 RotationOverLifetimeModule::GetRandomAngularVelocity() const {
        if (!angularVelocityRandomize_) return angularVelocityMin_;
        return Vector3(
            RandomUtils::Random(angularVelocityMin_.x, angularVelocityMax_.x),
            RandomUtils::Random(angularVelocityMin_.y, angularVelocityMax_.y),
            RandomUtils::Random(angularVelocityMin_.z, angularVelocityMax_.z)
        );
    }

    // ============================================================
    // NoiseModule
    // ============================================================
    NoiseModule::NoiseModule() = default;

    void NoiseModule::ApplyNoise(Particle& particle, float deltaTime) {
        if (!enabled_) return;

        noiseTime_ += scrollSpeed_ * deltaTime;

        float noiseX = std::sin(particle.transform.translation.x * frequency_ + noiseTime_) * strength_;
        float noiseY = std::sin(particle.transform.translation.y * frequency_ + noiseTime_ * 1.3f) * strength_;
        float noiseZ = std::sin(particle.transform.translation.z * frequency_ + noiseTime_ * 0.7f) * strength_;

        particle.velocity.x += noiseX * deltaTime;
        particle.velocity.y += noiseY * deltaTime;
        particle.velocity.z += noiseZ * deltaTime;
    }

    // ============================================================
    // JSON Serialization Implementations
    // ============================================================

    nlohmann::json VelocityOverLifetimeModule::ToJson() const {
        nlohmann::json j;
        j["enabled"] = enabled_;
        j["linearVelocity"] = {linearVelocity_.x, linearVelocity_.y, linearVelocity_.z};
        j["speedModifier"] = speedModifier_;
        return j;
    }

    void VelocityOverLifetimeModule::FromJson(const nlohmann::json& j) {
        if (j.contains("enabled")) enabled_ = j["enabled"];
        if (j.contains("linearVelocity")) {
            auto arr = j["linearVelocity"];
            linearVelocity_ = Vector3{arr[0], arr[1], arr[2]};
        }
        if (j.contains("speedModifier")) speedModifier_ = j["speedModifier"];
    }

    nlohmann::json LimitVelocityOverLifetimeModule::ToJson() const {
        nlohmann::json j;
        j["enabled"] = enabled_;
        j["speedLimit"] = speedLimit_;
        j["dampen"] = dampen_;
        return j;
    }

    void LimitVelocityOverLifetimeModule::FromJson(const nlohmann::json& j) {
        if (j.contains("enabled")) enabled_ = j["enabled"];
        if (j.contains("speedLimit")) speedLimit_ = j["speedLimit"];
        if (j.contains("dampen")) dampen_ = j["dampen"];
    }

    nlohmann::json ForceOverLifetimeModule::ToJson() const {
        nlohmann::json j;
        j["enabled"] = enabled_;
        j["force"] = {force_.x, force_.y, force_.z};
        return j;
    }

    void ForceOverLifetimeModule::FromJson(const nlohmann::json& j) {
        if (j.contains("enabled")) enabled_ = j["enabled"];
        if (j.contains("force")) {
            auto arr = j["force"];
            force_ = Vector3{arr[0], arr[1], arr[2]};
        }
    }

    nlohmann::json ColorOverLifetimeModule::ToJson() const {
        nlohmann::json j;
        j["enabled"] = enabled_;
        j["startColor"] = {startColor_.x, startColor_.y, startColor_.z, startColor_.w};
        j["endColor"] = {endColor_.x, endColor_.y, endColor_.z, endColor_.w};
        return j;
    }

    void ColorOverLifetimeModule::FromJson(const nlohmann::json& j) {
        if (j.contains("enabled")) enabled_ = j["enabled"];
        if (j.contains("startColor")) {
            auto arr = j["startColor"];
            startColor_ = Vector4{arr[0], arr[1], arr[2], arr[3]};
        }
        if (j.contains("endColor")) {
            auto arr = j["endColor"];
            endColor_ = Vector4{arr[0], arr[1], arr[2], arr[3]};
        }
    }

    nlohmann::json SizeOverLifetimeModule::ToJson() const {
        nlohmann::json j;
        j["enabled"] = enabled_;
        j["sizeMultiplier"] = sizeMultiplier_;
        j["startSize"] = startSize_;
        j["endSize"] = endSize_;
        return j;
    }

    void SizeOverLifetimeModule::FromJson(const nlohmann::json& j) {
        if (j.contains("enabled")) enabled_ = j["enabled"];
        if (j.contains("sizeMultiplier")) sizeMultiplier_ = j["sizeMultiplier"];
        if (j.contains("startSize")) startSize_ = j["startSize"];
        if (j.contains("endSize")) endSize_ = j["endSize"];
    }

    nlohmann::json RotationOverLifetimeModule::ToJson() const {
        nlohmann::json j;
        j["enabled"] = enabled_;
        j["angularVelocityMin"] = {angularVelocityMin_.x, angularVelocityMin_.y, angularVelocityMin_.z};
        j["angularVelocityMax"] = {angularVelocityMax_.x, angularVelocityMax_.y, angularVelocityMax_.z};
        j["angularVelocityRandomize"] = angularVelocityRandomize_;
        return j;
    }

    void RotationOverLifetimeModule::FromJson(const nlohmann::json& j) {
        if (j.contains("enabled")) enabled_ = j["enabled"];
        
        // 後方互換性: 古い"angularVelocity"フォーマット
        if (j.contains("angularVelocity") && j["angularVelocity"].is_array()) {
            auto arr = j["angularVelocity"];
            angularVelocityMin_ = Vector3{arr[0], arr[1], arr[2]};
            angularVelocityMax_ = angularVelocityMin_;
            angularVelocityRandomize_ = false;
        }
        
        // 新しいランダム対応フォーマット
        if (j.contains("angularVelocityMin") && j["angularVelocityMin"].is_array()) {
            auto arr = j["angularVelocityMin"];
            angularVelocityMin_ = Vector3{arr[0], arr[1], arr[2]};
        }
        if (j.contains("angularVelocityMax") && j["angularVelocityMax"].is_array()) {
            auto arr = j["angularVelocityMax"];
            angularVelocityMax_ = Vector3{arr[0], arr[1], arr[2]};
        }
        if (j.contains("angularVelocityRandomize")) {
            angularVelocityRandomize_ = j["angularVelocityRandomize"];
        }
    }

    nlohmann::json NoiseModule::ToJson() const {
        nlohmann::json j;
        j["enabled"] = enabled_;
        j["strength"] = strength_;
        j["frequency"] = frequency_;
        j["scrollSpeed"] = scrollSpeed_;
        return j;
    }

    void NoiseModule::FromJson(const nlohmann::json& j) {
        if (j.contains("enabled")) enabled_ = j["enabled"];
        if (j.contains("strength")) strength_ = j["strength"];
        if (j.contains("frequency")) frequency_ = j["frequency"];
        if (j.contains("scrollSpeed")) scrollSpeed_ = j["scrollSpeed"];
    }
}
