#pragma once

#include "GameEngine/Object/Component/Rendering/MeshComponent.h"
#include <memory>
#include <span>
#include <string_view>

namespace GameEngine {

/// @brief 一つの形状の識別子・生成Strategy・編集Strategyを対応付ける不変の登録情報。
struct PrimitiveMeshDefinition {
   MeshComponent::PrimitiveType type;          ///< 公開APIで使う形状の識別子。
   const char* name;                           ///< 保存JSONとInspectorで共有する安定名。
   bool supportsPlaneOrientation;              ///< 生成平面を変更できるか。
   bool supportsOriginY;                       ///< Y方向原点を変更できるか。
   void (*build)(Mesh&, const MeshComponent&); ///< 設定から新しいMeshを構築するStrategy。
#ifdef USE_IMGUI
   bool (*drawInspector)(MeshComponent&); ///< 形状固有の入力欄を描画し、設定変更時にtrueを返すStrategy。
#endif
};

/// @brief 登録されたStrategyを選び、形状ごとのGPUメッシュを所有権付きで生成するFactory。
class PrimitiveMeshFactory {
public:
   /// @brief 表示順を含む標準形状の登録表を返す。要素はプログラム終了まで有効。
   static std::span<const PrimitiveMeshDefinition> GetDefinitions();

   /// @brief 形状識別子から登録情報を取得する。未登録ならnullptr。
   static const PrimitiveMeshDefinition* Find(MeshComponent::PrimitiveType type);

   /// @brief 保存用の形状名から登録情報を取得する。未登録ならnullptr。
   static const PrimitiveMeshDefinition* Find(std::string_view name);

   /// @brief 現在の形状設定から新しいメッシュを構築する。未登録形状なら生成せずnullptr。
   static std::unique_ptr<Mesh> Create(const MeshComponent& component);
};
} // namespace GameEngine
