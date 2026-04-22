#pragma once

#include "GravityAttractor.h"

namespace GameEngine {

/// @brief 球状重力の発生源コンポーネント
/// 球体の中心から対象オブジェクトへの逆方向をUpVectorとして提供する
/// （マリオギャラクシーの惑星重力に相当）
class SphericalGravityAttractor final : public GravityAttractor {
public:
    static constexpr const char* kTypeName = "SphericalGravityAttractor";
    const char* GetTypeName() const override { return kTypeName; }

    void Update(float deltaTime) override {
        (void)deltaTime;
    }

    /// @brief 指定した座標が影響半径内にあるか判定する
    /// @param objectPosition 対象の座標
    /// @return 影響半径内であればtrue（0以下は無限大）
    bool IsInRange(const Vector3& objectPosition) const override {
        // 影響半径が0以下の場合は無限大として扱う
        if (influenceRadius <= 0.0f) {
            return true;
        }

        Vector3 center = GetCenter();
        Vector3 diff = objectPosition - center;
        return diff.LengthSquared() <= influenceRadius * influenceRadius;
    }

    /// @brief 球体の中心→対象の逆方向（=足元の「上」方向）を計算する
    ///
    /// アルゴリズム:
    ///   1. 球体の中心座標と対象座標の差分ベクトルを計算
    ///      dirToObject = objectPosition - center
    ///   2. 差分ベクトルを正規化してUpVectorとして返す
    ///      UpVector = normalize(dirToObject)
    ///
    /// @param objectPosition 対象の座標
    /// @return 重力に対するUpVector（正規化済み）
    Vector3 GetUpVectorFor(const Vector3& objectPosition) const override {
        Vector3 center = GetCenter();

        // 中心から対象への方向ベクトル
        Vector3 dirToObject = objectPosition - center;

        // 対象が中心と完全に重なっている場合はデフォルトのUpを返す
        if (dirToObject.LengthSquared() < 1e-8f) {
            return Vector3{ 0.0f, 1.0f, 0.0f };
        }

        // 正規化してUpVectorとして返す
        return dirToObject.Normalize();
    }

    nlohmann::json Serialize() const override {
        nlohmann::json json;
        json["influenceRadius"] = influenceRadius;
        return json;
    }

    void Deserialize(const nlohmann::json& data) override {
        if (data.contains("influenceRadius")) {
            influenceRadius = data["influenceRadius"];
        }
    }

#ifdef USE_IMGUI
    void DrawInspector() override;
#endif

public:
    float influenceRadius = 0.0f;  ///< 重力の影響半径（0以下で無限大）

private:
    /// @brief オーナーのTransformから中心座標を取得する
    Vector3 GetCenter() const {
        if (!HasOwner()) {
            return Vector3{ 0.0f, 0.0f, 0.0f };
        }
        auto* transform = GetOwner().GetComponent<TransformComponent>();
        if (!transform) {
            return Vector3{ 0.0f, 0.0f, 0.0f };
        }
        return transform->transform.translation;
    }
};

} // namespace GameEngine
