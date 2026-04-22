#pragma once

#include "GravityAttractor.h"
#include "Object/Model/ModelAsset.h"
#include <memory>

namespace GameEngine {

/// @brief メッシュ法線ベースの重力発生源コンポーネント
/// 対象オブジェクトの真下にあるポリゴンの法線を取得してUpVectorとして返す
///
/// 現在は基底実装（スタブ）として機能し、サブクラスが
/// ActualFindSurfaceNormal() をオーバーライドして具体的な
/// レイキャスト実装を提供する
class MeshNormalGravityAttractor : public GravityAttractor {
public:
    static constexpr const char* kTypeName = "MeshNormalGravityAttractor";
    const char* GetTypeName() const override { return kTypeName; }

    void Update(float deltaTime) override {
        (void)deltaTime;
    }

    /// @brief 常に影響範囲内として判定する
    bool IsInRange(const Vector3& objectPosition) const override {
        (void)objectPosition;
        return true;
    }

    /// @brief 対象の真下のメッシュ法線をUpVectorとして返す
    ///
    /// アルゴリズム（フルメッシュレイキャスト実装時の設計）:
    ///   1. objectPosition から -upHint 方向にレイを飛ばす
    ///   2. メッシュとの交差点を検出する
    ///   3. 交差したポリゴンの頂点法線を重心補間で取得する
    ///   4. 取得した法線を正規化してUpVectorとして返す
    ///
    /// @param objectPosition 対象の座標
    /// @return メッシュ法線に基づくUpVector
    Vector3 GetUpVectorFor(const Vector3& objectPosition) const override {
        // 実装サブクラスに委譲
        Vector3 normal = FindSurfaceNormal(objectPosition);

        // 有効な法線が返ってきた場合はそれを使用
        if (normal.LengthSquared() > 1e-8f) {
            return normal.Normalize();
        }

        // フォールバック：デフォルトのUp方向
        return fallbackUpVector_;
    }

    /// @brief フォールバックのUpVectorを設定（メッシュが見つからない場合に使用）
    void SetFallbackUpVector(const Vector3& up) {
        if (up.LengthSquared() > 1e-8f) {
            fallbackUpVector_ = up.Normalize();
        }
    }

    nlohmann::json Serialize() const override {
        nlohmann::json json;
        json["fallbackUp"] = { fallbackUpVector_.x, fallbackUpVector_.y, fallbackUpVector_.z };
        return json;
    }

    void Deserialize(const nlohmann::json& data) override {
        if (data.contains("fallbackUp")) {
            auto up = data["fallbackUp"];
            fallbackUpVector_ = Vector3{ up[0], up[1], up[2] };
        }
    }

#ifdef USE_IMGUI
    void DrawInspector() override;
#endif

protected:
    /// @brief サブクラスでオーバーライドしてメッシュ法線を取得する
    /// @param objectPosition 対象の座標
    /// @return メッシュの法線（見つからない場合は0ベクトル）
    virtual Vector3 FindSurfaceNormal(const Vector3& objectPosition) const {
        (void)objectPosition;
        // 基底実装: 常にフォールバック値を返す
        return Vector3{ 0.0f, 0.0f, 0.0f };
    }

protected:
    Vector3 fallbackUpVector_ = { 0.0f, 1.0f, 0.0f };  ///< メッシュが見つからない場合のUpVector
};

} // namespace GameEngine
