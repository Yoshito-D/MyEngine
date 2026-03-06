#pragma once
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"
#include <nlohmann/json.hpp>
#include "ParticleModule.h"

namespace GameEngine {
    // ============================================================
    // Random Range Structures
    // ============================================================
    
    /// @brief ランダム範囲を表す構造体（浮動小数点数）
    struct RandomFloat {
        float minValue = 0.0f;
        float maxValue = 0.0f;
        bool randomize = false;

        RandomFloat() = default;
        RandomFloat(float value) : minValue(value), maxValue(value), randomize(false) {}
        RandomFloat(float minVal, float maxVal, bool enableRandom = true) 
            : minValue(minVal), maxValue(maxVal), randomize(enableRandom) {}

        float GetValue() const;  // .cppで実装

        nlohmann::json ToJson() const;
        void FromJson(const nlohmann::json& json);
    };

    /// @brief ランダム範囲を表す構造体（Vector3）
    struct RandomVector3 {
        Vector3 minValue = Vector3(0.0f, 0.0f, 0.0f);
        Vector3 maxValue = Vector3(0.0f, 0.0f, 0.0f);
        bool randomize = false;

        RandomVector3() = default;
        RandomVector3(const Vector3& value) : minValue(value), maxValue(value), randomize(false) {}
        RandomVector3(const Vector3& minVal, const Vector3& maxVal, bool enableRandom = true)
            : minValue(minVal), maxValue(maxVal), randomize(enableRandom) {}

        Vector3 GetValue() const;  // .cppで実装

        nlohmann::json ToJson() const;
        void FromJson(const nlohmann::json& json);
    };

    /// @brief ランダム範囲を表す構造体（色）
    struct RandomColor {
        uint32_t minValue = 0xFFFFFFFF;
        uint32_t maxValue = 0xFFFFFFFF;
        bool randomize = false;

        RandomColor() = default;
        RandomColor(uint32_t value) : minValue(value), maxValue(value), randomize(false) {}
        RandomColor(uint32_t minVal, uint32_t maxVal, bool enableRandom = true)
            : minValue(minVal), maxValue(maxVal), randomize(enableRandom) {}

        uint32_t GetValue() const;  // .cppで実装

        nlohmann::json ToJson() const;
        void FromJson(const nlohmann::json& json);
    };

    // ============================================================
    // Main Module (メインモジュール)
    // パーティクル全体の基本設定
    // ============================================================
    class MainModule {
    public:
        enum class SimulationSpace {
            World,  // ワールド空間
            Local   // ローカル空間
        };

        enum class ScalingMode {
            Hierarchy,  // 階層的スケール
            Local,      // ローカルスケールのみ
            Shape       // 形状のみスケール
        };

        MainModule();

        // Duration & Loop
        void SetDuration(float duration) { duration_ = duration; }
        float GetDuration() const { return duration_; }
        void SetLooping(bool loop) { looping_ = loop; }
        bool IsLooping() const { return looping_; }

        // Start Lifetime (ランダム対応)
        void SetStartLifetime(const RandomFloat& lifetime) { startLifetime_ = lifetime; }
        const RandomFloat& GetStartLifetime() const { return startLifetime_; }
        void SetStartLifetimeMin(float min) { startLifetime_.minValue = min; }
        void SetStartLifetimeMax(float max) { startLifetime_.maxValue = max; }
        void SetStartLifetimeRandomize(bool randomize) { startLifetime_.randomize = randomize; }

        // Start Speed (ランダム対応)
        void SetStartSpeed(const RandomFloat& speed) { startSpeed_ = speed; }
        const RandomFloat& GetStartSpeed() const { return startSpeed_; }
        void SetStartSpeedMin(float min) { startSpeed_.minValue = min; }
        void SetStartSpeedMax(float max) { startSpeed_.maxValue = max; }
        void SetStartSpeedRandomize(bool randomize) { startSpeed_.randomize = randomize; }

        // Start Size (ランダム対応)
        void SetStartSize(const RandomFloat& size) { startSize_ = size; }
        const RandomFloat& GetStartSize() const { return startSize_; }
        void SetStartSizeMin(float min) { startSize_.minValue = min; }
        void SetStartSizeMax(float max) { startSize_.maxValue = max; }
        void SetStartSizeRandomize(bool randomize) { startSize_.randomize = randomize; }

        // Start Rotation (ランダム対応)
        void SetStartRotation(const RandomVector3& rotation) { startRotation_ = rotation; }
        const RandomVector3& GetStartRotation() const { return startRotation_; }
        void SetStartRotationMin(const Vector3& min) { startRotation_.minValue = min; }
        void SetStartRotationMax(const Vector3& max) { startRotation_.maxValue = max; }
        void SetStartRotationRandomize(bool randomize) { startRotation_.randomize = randomize; }

        // Start Color (ランダム対応)
        void SetStartColor(const RandomColor& color) { startColor_ = color; }
        const RandomColor& GetStartColor() const { return startColor_; }
        void SetStartColorMin(uint32_t min) { startColor_.minValue = min; }
        void SetStartColorMax(uint32_t max) { startColor_.maxValue = max; }
        void SetStartColorRandomize(bool randomize) { startColor_.randomize = randomize; }

        // Gravity Modifier
        void SetGravityModifier(float modifier) { gravityModifier_ = modifier; }
        float GetGravityModifier() const { return gravityModifier_; }

        // Simulation Space
        void SetSimulationSpace(SimulationSpace space) { simulationSpace_ = space; }
        SimulationSpace GetSimulationSpace() const { return simulationSpace_; }

        // Play On Awake
        void SetPlayOnAwake(bool play) { playOnAwake_ = play; }
        bool GetPlayOnAwake() const { return playOnAwake_; }

        // Scaling Mode
        void SetScalingMode(ScalingMode mode) { scalingMode_ = mode; }
        ScalingMode GetScalingMode() const { return scalingMode_; }

        // Max Particles
        void SetMaxParticles(uint32_t max) { maxParticles_ = max; }
        uint32_t GetMaxParticles() const { return maxParticles_; }

        // Emission Rate (1秒間に放出するパーティクル数)
        void SetEmissionRate(float rate) { emissionRate_ = rate; }
        float GetEmissionRate() const { return emissionRate_; }

        // JSON Serialization
        nlohmann::json ToJson() const;
        void FromJson(const nlohmann::json& json);

    private:
        float duration_ = 5.0f;
        bool looping_ = true;
        
        RandomFloat startLifetime_{2.0f, 2.5f, false};
        RandomFloat startSpeed_{5.0f, 6.0f, false};
        RandomFloat startSize_{1.0f, 1.0f, false};
        RandomVector3 startRotation_{Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), false};
        RandomColor startColor_{0xFFFFFFFF, 0xFFFFFFFF, false};
        
        float gravityModifier_ = 0.0f;
        SimulationSpace simulationSpace_ = SimulationSpace::World;
        bool playOnAwake_ = true;
        ScalingMode scalingMode_ = ScalingMode::Hierarchy;
        uint32_t maxParticles_ = 1000;
        float emissionRate_ = 10.0f;  // 1秒間に放出するパーティクル数
    };
}
