#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "Graphics/Material.h"

namespace GameEngine {
/// @brief マテリアルマネージャークラス
class MaterialManager {
public:
   /// @brief マテリアルマネージャーの初期化
   /// @param device グラフィックスデバイス
   void Initialize(ID3D12Device* device);

   /// @brief マテリアルを作成
   /// @param name 作成するマテリアルの名前
   /// @param color マテリアルの色。デフォルトは0xffffffff（白、不透明）
   /// @param enableLighting ライティングを有効にするかどうか。デフォルトはtrue（有効）
   /// @param uvTransform UV座標に適用する変換行列。デフォルトは単位行列
   void* CreateMaterial(const std::string& name, uint32_t color = 0xffffffff,
	  int32_t lightingMode = Material::LightingMode::HALFLAMBERT, const Matrix4x4& uvTransform = MakeIdentity4x4());

   /// @brief マテリアルを取得
   /// @param name 取得するマテリアルの名前
   Material* GetMaterial(const std::string& name) const;

   /// @brief マテリアルを全削除
   void Clear();

private:
   ID3D12Device* device_ = nullptr;
   std::unordered_map<std::string, std::unique_ptr<Material>> materials_;
};
}