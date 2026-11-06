#pragma once

#include <cstdint>

namespace GameEngine {

/// @brief コンポーネント互換性の判定に使用するオブジェクト種別
enum class ObjectType : uint32_t {
   Generic = 1u << 0,
   Model = 1u << 1,
   Sprite = 1u << 2,
   UIText = 1u << 3,
   Skybox = 1u << 4
};

/// @brief オブジェクト種別をシーン保存用の安定した名前へ変換する。
/// @param objectType 保存するオブジェクト種別。
/// @return 対応する種別名。未定義の値はUnknownを返す。
constexpr const char* ToObjectTypeName(ObjectType objectType) {
   switch (objectType) {
      case ObjectType::Generic:
         return "Generic";
      case ObjectType::Model:
         return "Model";
      case ObjectType::Sprite:
         return "Sprite";
      case ObjectType::UIText:
         return "UIText";
      case ObjectType::Skybox:
         return "Skybox";
   }

   return "Unknown";
}

/// @brief 複数のオブジェクト種別を表すビットマスク
using ObjectTypeMask = uint32_t;

/// @brief オブジェクト種別をビットマスクへ変換する
/// @param objectType 変換対象の種別
/// @return 対応するビットマスク
constexpr ObjectTypeMask ToObjectTypeMask(ObjectType objectType) {
   return static_cast<ObjectTypeMask>(objectType);
}

/// @brief 2つのオブジェクト種別からビットマスクを作成する
constexpr ObjectTypeMask operator|(ObjectType lhs, ObjectType rhs) {
   return ToObjectTypeMask(lhs) | ToObjectTypeMask(rhs);
}

/// @brief オブジェクト種別を既存のビットマスクへ追加する
constexpr ObjectTypeMask operator|(ObjectTypeMask lhs, ObjectType rhs) {
   return lhs | ToObjectTypeMask(rhs);
}

/// @brief すべてのオブジェクト種別を含むビットマスク
constexpr ObjectTypeMask kAllObjectTypes =
   ObjectType::Generic | ObjectType::Model | ObjectType::Sprite | ObjectType::UIText | ObjectType::Skybox;

} // namespace GameEngine
