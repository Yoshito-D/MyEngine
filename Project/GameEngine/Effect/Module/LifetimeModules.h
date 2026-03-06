#pragma once
#include "ParticleModule.h"
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
    // Forward declare RandomVector3 from MainModule.h
    struct RandomVector3;
    
    // ============================================================
    // Velocity over Lifetime Module
    // ============================================================
    class VelocityOverLifetimeModule : public ParticleModule {
    public:
        VelocityOverLifetimeModule();

        /// @brief パーティクルの速度を更新
        void ApplyVelocity(Particle& particle, float deltaTime) const;

        void SetLinearVelocity(const Vector3& velocity) { linearVelocity_ = velocity; }
        const Vector3& GetLinearVelocity() const { return linearVelocity_; }

        void SetSpeedModifier(float modifier) { speedModifier_ = modifier; }
        float GetSpeedModifier() const { return speedModifier_; }

        nlohmann::json ToJson() const override;
        void FromJson(const nlohmann::json& json) override;

    private:
        Vector3 linearVelocity_{0.0f, 0.0f, 0.0f};
        float speedModifier_ = 1.0f;
    };

    // ============================================================
    // Limit Velocity over Lifetime Module
    // ============================================================
    class LimitVelocityOverLifetimeModule : public ParticleModule {
    public:
        LimitVelocityOverLifetimeModule();

        /// @brief パーティクルの速度を制限
        void LimitVelocity(Particle& particle) const;

        void SetSpeedLimit(float limit) { speedLimit_ = limit; }
        float GetSpeedLimit() const { return speedLimit_; }

        void SetDampen(float dampen) { dampen_ = dampen; }
        float GetDampen() const { return dampen_; }

        nlohmann::json ToJson() const override;
        void FromJson(const nlohmann::json& json) override;

    private:
        float speedLimit_ = 10.0f;
        float dampen_ = 0.5f;
    };

    // ============================================================
    // Force over Lifetime Module
    // ============================================================
    class ForceOverLifetimeModule : public ParticleModule {
    public:
        ForceOverLifetimeModule();

        /// @brief パーティクルに力を適用
        void ApplyForce(Particle& particle) const;

        void SetForce(const Vector3& force) { force_ = force; }
        const Vector3& GetForce() const { return force_; }

        nlohmann::json ToJson() const override;
        void FromJson(const nlohmann::json& json) override;

    private:
        Vector3 force_{0.0f, 0.0f, 0.0f};
    };

    // ============================================================
    // Color over Lifetime Module
    // ============================================================
    class ColorOverLifetimeModule : public ParticleModule {
    public:
        ColorOverLifetimeModule();

        /// @brief パーティクルの色を更新
        void UpdateColor(Particle& particle) const;

        void SetStartColor(const Vector4& color) { startColor_ = color; }
        const Vector4& GetStartColor() const { return startColor_; }

        void SetEndColor(const Vector4& color) { endColor_ = color; }
        const Vector4& GetEndColor() const { return endColor_; }

        nlohmann::json ToJson() const override;
        void FromJson(const nlohmann::json& json) override;

    private:
        Vector4 startColor_{1.0f, 1.0f, 1.0f, 1.0f};
        Vector4 endColor_{1.0f, 1.0f, 1.0f, 0.0f};
    };

    // ============================================================
    // Size over Lifetime Module
    // ============================================================
    class SizeOverLifetimeModule : public ParticleModule {
    public:
        SizeOverLifetimeModule();

        /// @brief パーティクルのサイズを更新
        void UpdateSize(Particle& particle) const;

        void SetSizeMultiplier(float multiplier) { sizeMultiplier_ = multiplier; }
        float GetSizeMultiplier() const { return sizeMultiplier_; }

        void SetStartSize(float size) { startSize_ = size; }
        float GetStartSize() const { return startSize_; }

        void SetEndSize(float size) { endSize_ = size; }
        float GetEndSize() const { return endSize_; }

        nlohmann::json ToJson() const override;
        void FromJson(const nlohmann::json& json) override;

    private:
        float sizeMultiplier_ = 1.0f;
        float startSize_ = 1.0f;
        float endSize_ = 0.0f;
    };

    // ============================================================
    // Rotation over Lifetime Module (ランダム対応)
    // ============================================================
    class RotationOverLifetimeModule : public ParticleModule {
    public:
        RotationOverLifetimeModule();

        /// @brief パーティクルの回転を更新
        void UpdateRotation(Particle& particle, float deltaTime) const;

        // Angular Velocity (ランダム対応)
        void SetAngularVelocityMin(const Vector3& minVal) { angularVelocityMin_ = minVal; }
        const Vector3& GetAngularVelocityMin() const { return angularVelocityMin_; }
        
        void SetAngularVelocityMax(const Vector3& maxVal) { angularVelocityMax_ = maxVal; }
        const Vector3& GetAngularVelocityMax() const { return angularVelocityMax_; }
        
        void SetAngularVelocityRandomize(bool randomize) { angularVelocityRandomize_ = randomize; }
        bool GetAngularVelocityRandomize() const { return angularVelocityRandomize_; }
        
        /// @brief ランダムな角速度を取得
        Vector3 GetRandomAngularVelocity() const;

        nlohmann::json ToJson() const override;
        void FromJson(const nlohmann::json& json) override;

    private:
        Vector3 angularVelocityMin_{0.0f, 0.0f, 0.0f};
        Vector3 angularVelocityMax_{0.0f, 0.0f, 0.0f};
        bool angularVelocityRandomize_ = false;
    };

    // ============================================================
    // Noise Module
    // ============================================================
    class NoiseModule : public ParticleModule {
    public:
        NoiseModule();

        /// @brief パーティクルにノイズを適用
        void ApplyNoise(Particle& particle, float deltaTime);

        void SetStrength(float strength) { strength_ = strength; }
        float GetStrength() const { return strength_; }

        void SetFrequency(float frequency) { frequency_ = frequency; }
        float GetFrequency() const { return frequency_; }

        void SetScrollSpeed(float speed) { scrollSpeed_ = speed; }
        float GetScrollSpeed() const { return scrollSpeed_; }

        nlohmann::json ToJson() const override;
        void FromJson(const nlohmann::json& json) override;

    private:
        float strength_ = 1.0f;
        float frequency_ = 0.5f;
        float scrollSpeed_ = 1.0f;
        float noiseTime_ = 0.0f;
    };
}
