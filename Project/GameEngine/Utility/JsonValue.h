#pragma once

#include "GameEngine/Math/VectorMath.h"
#include <nlohmann/json.hpp>
#include <source_location>
#include <string>

namespace GameEngine::JsonValue {
/// @brief 位置・方向ベクトルを数値配列へ保存する。
nlohmann::json WriteVector3(const Vector3& value);

/// @brief 有限な二要素の数値配列を読み、不正な配列はログを記録して例外にする。
/// @param data 読み込む数値配列。
/// @param location 配列を読み込んだ呼び出し箇所。
/// @return 検証済みのベクトル。
Vector2 ReadVector2(const nlohmann::json& data, std::source_location location = std::source_location::current());

/// @brief 有限な三要素の数値配列を読み、不正な配列はログを記録して例外にする。
/// @param data 読み込む数値配列。
/// @param location 配列を読み込んだ呼び出し箇所。
/// @return 検証済みのベクトル。
Vector3 ReadVector3(const nlohmann::json& data, std::source_location location = std::source_location::current());

/// @brief 有限な四要素の数値配列を読み、不正な配列はログを記録して例外にする。
/// @param data 読み込む数値配列。
/// @param location 配列を読み込んだ呼び出し箇所。
/// @return 検証済みのベクトル。
Vector4 ReadVector4(const nlohmann::json& data, std::source_location location = std::source_location::current());

/// @brief 任意の有限な数値設定を読み、省略時は現在値を使い、不正値はログを記録して例外にする。
float ReadFloat(const nlohmann::json& data, const char* key, float defaultValue);

/// @brief 任意の真偽値設定を読み、省略時は現在値を使い、不正値はログを記録して例外にする。
bool ReadBool(const nlohmann::json& data, const char* key, bool defaultValue);

/// @brief 任意の文字列設定を読み、省略時のみ既定値を使う。
/// @param data 設定を含むJSONオブジェクト。
/// @param key 読み込むキー。
/// @param defaultValue キーを省略した場合に返す値。
/// @return 読み込んだ文字列。不正な型にはログを記録して例外を送出する。
std::string ReadString(const nlohmann::json& data, const char* key, const std::string& defaultValue);
} // namespace GameEngine::JsonValue
