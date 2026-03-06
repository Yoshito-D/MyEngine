#pragma once
#include "Utility/VectorMath.h"
#include <nlohmann/json.hpp>
#include "ParticleModule.h"

namespace GameEngine {
    // ============================================================
    // Shape Module (形状モジュール)
    // パーティクルが発生する形状を決定
    // ============================================================
    class ShapeModule {
    public:
        enum class ShapeType {
            Sphere,      // 球
            Hemisphere,  // 半球
            Cone,        // 円錐
            Box,         // 箱
            Circle,      // 円
            Edge,        // エッジ
            Point        // 点
        };

        enum class EmitFrom {
            Volume,      // 体積全体
            Shell,       // 表面のみ
            Edge         // エッジのみ
        };

        ShapeModule();

        void SetEnabled(bool enabled) { enabled_ = enabled; }
        bool IsEnabled() const { return enabled_; }

        void SetShapeType(ShapeType type) { shapeType_ = type; }
        ShapeType GetShapeType() const { return shapeType_; }

        void SetEmitFrom(EmitFrom from) { emitFrom_ = from; }
        EmitFrom GetEmitFrom() const { return emitFrom_; }

        // Sphere / Hemisphere
        void SetRadius(float radius) { radius_ = radius; }
        float GetRadius() const { return radius_; }

        // Cone
        void SetAngle(float angle) { angle_ = angle; }
        float GetAngle() const { return angle_; }
        void SetLength(float length) { length_ = length; }
        float GetLength() const { return length_; }

        // Box
        void SetBoxSize(const Vector3& size) { boxSize_ = size; }
        const Vector3& GetBoxSize() const { return boxSize_; }

        // Circle
        void SetArc(float arc) { arc_ = arc; }
        float GetArc() const { return arc_; }

        // Position
        void SetPosition(const Vector3& position) { position_ = position; }
        const Vector3& GetPosition() const { return position_; }

        // Rotation
        void SetRotation(const Vector3& rotation) { rotation_ = rotation; }
        const Vector3& GetRotation() const { return rotation_; }

        // Scale
        void SetScale(const Vector3& scale) { scale_ = scale; }
        const Vector3& GetScale() const { return scale_; }

        /// @brief 形状に基づいてランダムな放出位置を取得
        Vector3 GetRandomEmissionPosition() const;

        /// @brief 形状に基づいてランダムな初期速度方向を取得
        Vector3 GetRandomEmissionDirection() const;

        // JSON Serialization
        nlohmann::json ToJson() const;
        void FromJson(const nlohmann::json& json);

    private:
        bool enabled_ = true;
        ShapeType shapeType_ = ShapeType::Cone;
        EmitFrom emitFrom_ = EmitFrom::Volume;
        
        float radius_ = 1.0f;
        float angle_ = 25.0f;
        float length_ = 5.0f;
        Vector3 boxSize_{1.0f, 1.0f, 1.0f};
        float arc_ = 360.0f;
        
        Vector3 position_{0.0f, 0.0f, 0.0f};
        Vector3 rotation_{0.0f, 0.0f, 0.0f};
        Vector3 scale_{1.0f, 1.0f, 1.0f};
    };
}
