#pragma once
#include "ParticleModule.h"
#include "Utility/VectorMath.h"
#include <nlohmann/json.hpp>

namespace GameEngine {
    /// @brief パーティクルのレンダリング設定モジュール
    class RendererModule : public ParticleModule {
    public:
        /// @brief ビルボード描画タイプ
        enum class BillboardType {
            Billboard = 0,              // 常にカメラを向く
            StretchedBillboard = 1,     // 速度方向に引き伸ばされたビルボード
            HorizontalBillboard = 2,    // 水平方向のビルボード（Y軸が常に上）
            VerticalBillboard = 3,      // 垂直方向のビルボード（XZ平面内で回転）
            Mesh = 4                    // メッシュとして表示（回転が適用される）
        };

        RendererModule();

        // ビルボードタイプ
        void SetBillboardType(BillboardType type) { billboardType_ = type; }
        BillboardType GetBillboardType() const { return billboardType_; }

        // Stretch 設定（StretchedBillboard用）
        void SetSpeedScale(float scale) { speedScale_ = scale; }
        float GetSpeedScale() const { return speedScale_; }

        void SetLengthScale(float scale) { lengthScale_ = scale; }
        float GetLengthScale() const { return lengthScale_; }

        // JSON Serialization
        nlohmann::json ToJson() const override;
        void FromJson(const nlohmann::json& json) override;

    private:
        BillboardType billboardType_ = BillboardType::Billboard;
        float speedScale_ = 1.0f;    // 速度に基づくスケール
        float lengthScale_ = 2.0f;   // 長さのスケール
    };
}
