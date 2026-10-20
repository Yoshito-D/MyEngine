#pragma once

#include "GameEngine/Math/VectorMath.h"
#include <nlohmann/json.hpp>

namespace GameEngine::JsonValue {
/// @brief 位置・方向ベクトルを数値配列へ保存する。
nlohmann::json WriteVector3(const Vector3& value);

/// @brief 有限な二要素の数値配列を読み、不正値では現在値を維持する。
Vector2 ReadVector2(const nlohmann::json& data, const Vector2& fallback);

/// @brief 有限な三要素の数値配列を読み、不正値では現在値を維持する。
Vector3 ReadVector3(const nlohmann::json& data, const Vector3& fallback);

/// @brief 有限な四要素の数値配列を読み、不正値では現在値を維持する。
Vector4 ReadVector4(const nlohmann::json& data, const Vector4& fallback);

/// @brief 任意の有限な数値設定を読み、欠落・型違反時は現在値を維持する。
float ReadFloat(const nlohmann::json& data, const char* key, float fallback);

/// @brief 任意の真偽値設定を読み、欠落・型違反時は現在値を維持する。
bool ReadBool(const nlohmann::json& data, const char* key, bool fallback);
} // namespace GameEngine::JsonValue
