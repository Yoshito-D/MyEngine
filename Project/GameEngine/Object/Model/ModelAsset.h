#pragma once
#include <d3d12.h>
#include <string>
#include <vector>
#include "Graphics/Mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace GameEngine {
/// @brief マテリアルアセットを表す構造体
struct MaterialAsset {
   std::string textureFilePath; // テクスチャファイルのパス
};

/// @brief メッシュデータを表す構造体
struct MeshData {
   std::vector<Mesh::VertexData> vertices;
   uint32_t materialIndex; // このメッシュが使用するマテリアルのインデックス
};

struct Node {
   Matrix4x4 localMatrix;
   std::string name;
   std::vector<Node> children;
};

/// @brief モデルデータを表す構造体
struct ModelData {
   std::vector<MeshData> meshes;               // 複数メッシュ対応
   std::vector<MaterialAsset> materials;       // 複数マテリアル対応
   Node rootNode;                             // ルートノード
};


/// @brief モデルアセットクラス
class ModelAsset {
public:
   /// @brief objファイルをロードする
   /// @param device デバイス
   /// @param modelPath モデルファイルのパス
   /// @param modelName モデル名
   void LoadFile(ID3D12Device* device, const std::string& modelPath, const std::string& modelName);

   /// @brief 指定インデックスの頂点バッファビューを取得する 
   /// @param index メッシュ番号（省略時は0）
   /// @return 頂点バッファビュー
   const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView(size_t index = 0) const {
	  assert(index < vertexBufferViews_.size());
	  return vertexBufferViews_[index];
   }


   /// @brief 指定インデックスの頂点バッファを取得する
   /// @param index メッシュ番号（省略時は0）
   /// @return 頂点バッファリソース
   ID3D12Resource* GetVertexBuffer(size_t index = 0) const {
	  assert(index < vertexResources_.size());
	  return vertexResources_[index].Get();
   }


   /// @brief 指定インデックスの頂点データを取得する
   /// @param index メッシュ番号（省略時は0）
   /// @return 頂点データの参照
   const std::vector<Mesh::VertexData>& GetVertices(size_t index = 0) const {
	  assert(index < modelData_.meshes.size());
	  return modelData_.meshes[index].vertices;
   }

   /// @brief 指定インデックスのマテリアルを取得する
   /// @param index マテリアル番号（省略時は0）
   /// @return マテリアルアセット
   const MaterialAsset& GetMaterialAsset(size_t index = 0) const {
	  assert(index < modelData_.materials.size());
	  return modelData_.materials[index];
   }


   /// @brief メッシュデータ一覧を取得する
   const std::vector<MeshData>& GetMeshData() const {
	  return modelData_.meshes;
   }

   /// @brief マテリアルアセット一覧を取得する
   const std::vector<MaterialAsset>& GetMaterialAssets() const {
	  return modelData_.materials;
   }

   /// @brief ルートノードを取得する
   const Node& GetRootNode() const {
	  return modelData_.rootNode;
   }


private:
   ModelData modelData_;
   std::vector<ComPtr<ID3D12Resource>> vertexResources_;            // 複数リソース
   std::vector<D3D12_VERTEX_BUFFER_VIEW> vertexBufferViews_;        // 複数ビュー
   std::vector<Mesh::VertexData*> mappedVertexData_;                // 複数マップデータ
private:
   ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);

   Node ReadNode(aiNode* node);
};
}