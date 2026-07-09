#pragma once
#include <d3d12.h>
#include <string>
#include <vector>
#include <optional>
#include <map>
#include <array>
#include "Graphics/Mesh.h"
#include "MathUtils.h"
#include "Skeleton.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <span>

namespace GameEngine {
class GraphicsDevice;

/// @brief マテリアルアセットを表す構造体
struct MaterialAsset {
   std::string textureFilePath; // テクスチャファイルのパス
};

/// @brief メッシュデータを表す構造体
struct MeshData {
   std::vector<Mesh::VertexData> vertices;
   std::vector<uint32_t> indices;
   uint32_t materialIndex; // このメッシュが使用するマテリアルのインデックス
};

struct Node {
   Transform transform;
   Matrix4x4 localMatrix;
   std::string name;
   std::vector<Node> children;
};

struct VertexWeightData {
   float weight;      // ウェイト値
   uint32_t vertexId; // 頂点ID
   uint32_t meshIndex; // メッシュID
};

struct JointWeightData {
   Matrix4x4 inverseBindPoseMatrix; // 逆バインドポーズ行列
   std::vector<VertexWeightData> vertexWeights; // このジョイントに影響を受
};

const uint32_t kNumMaxInfluence = 4; // 頂点が影響を受ける最大ジョイント数
struct VertexInfluence {
   std::array<float, kNumMaxInfluence> weights;
   std::array<int32_t, kNumMaxInfluence> jointIndices;
};

struct WellForGPU {
   Matrix4x4 skeletonSpaceMatrix;
   Matrix4x4 skeletonSpaceInverseTransposeMatrix;
};

struct SkinningInformationForGPU {
   uint32_t numVertices = 0;
   uint32_t padding[3] = {};
};

struct SkinCluster {
   std::vector<Matrix4x4> inverseBindPoseMatrices; // 逆バインドポーズ行列のリスト
   std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> influenceResources; // メッシュごとの頂点影響リソース
   std::vector<D3D12_VERTEX_BUFFER_VIEW> influenceBufferViews; // メッシュごとの頂点バッファ
   std::vector<VertexInfluence*> mappedInfluenceData; // メッシュごとのマップ済み頂点影響データ
   std::vector<std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>> inputVertexSrvHandles;
   std::vector<std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>> influenceSrvHandles;
   std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> skinnedVertexResources;
   std::vector<D3D12_VERTEX_BUFFER_VIEW> skinnedVertexBufferViews;
   std::vector<std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>> skinnedVertexUavHandles;
   std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> skinningInformationResources;
   std::vector<SkinningInformationForGPU*> mappedSkinningInformationData;
   std::vector<D3D12_RESOURCE_STATES> skinnedVertexResourceStates;
   Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
   std::span<WellForGPU> mappedPalette; // マップされたスケルトン行列データへのスパン
   std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle; // パレット用のCPU/GPUディスクリプタハンドル

   /// @brief 指定メッシュのCompute Skinning用リソースがそろっているか
   /// @param meshIndex メッシュ番号
   /// @return 必要なSRV/UAV/CBV/VBVが利用可能ならtrue
   bool HasComputeSkinningResources(size_t meshIndex) const {
	  return meshIndex < inputVertexSrvHandles.size() &&
		 meshIndex < influenceSrvHandles.size() &&
		 meshIndex < skinnedVertexResources.size() &&
		 meshIndex < skinnedVertexBufferViews.size() &&
		 meshIndex < skinnedVertexUavHandles.size() &&
		 meshIndex < skinningInformationResources.size() &&
		 inputVertexSrvHandles[meshIndex].second.ptr != 0 &&
		 influenceSrvHandles[meshIndex].second.ptr != 0 &&
		 skinnedVertexResources[meshIndex] &&
		 skinnedVertexUavHandles[meshIndex].second.ptr != 0 &&
		 skinningInformationResources[meshIndex];
   }

   /// @brief 指定メッシュのCompute Skinning済み頂点バッファビューを取得する
   /// @param meshIndex メッシュ番号
   /// @return 利用可能な場合は頂点バッファビュー、範囲外ならnullptr
   const D3D12_VERTEX_BUFFER_VIEW* GetSkinnedVertexBufferView(size_t meshIndex) const {
	  if (meshIndex >= skinnedVertexBufferViews.size()) {
		 return nullptr;
	  }
	  return &skinnedVertexBufferViews[meshIndex];
   }

   const D3D12_VERTEX_BUFFER_VIEW* GetInfluenceBufferView(size_t meshIndex) const {
	  if (meshIndex >= influenceBufferViews.size()) {
		 return nullptr;
	  }
	  return &influenceBufferViews[meshIndex];
   }
};

/// @brief モデルデータを表す構造体
struct ModelData {
   std::map<std::string, JointWeightData> skinClusterData;
   std::vector<MeshData> meshes;               // 複数メッシュ対応
   std::vector<MaterialAsset> materials;       // 複数マテリアル対応
   Node rootNode;                             // ルートノード
};

/// @brief モデルアセットクラス
class ModelAsset {
public:
   void SetAssetId(const std::string& assetId) { assetId_ = assetId; }
   const std::string& GetAssetId() const { return assetId_; }

   /// @brief objファイルをロードする
   /// @param device グラフィックスデバイス
   /// @param modelPath モデルファイルのパス
   /// @param modelName モデル名
   void LoadFile(GraphicsDevice* device, const std::string& modelPath, const std::string& modelName);

   /// @brief モデル単位で利用するスキンクラスタを生成する
   std::optional<SkinCluster> CreateSkinClusterInstance();

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

   /// @brief 指定インデックスのインデックスバッファビューを取得する
   /// @param index メッシュ番号（省略時は0）
   /// @return インデックスバッファビュー
   const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView(size_t index = 0) const {
	  assert(index < indexBufferViews_.size());
	  return indexBufferViews_[index];
   }

   /// @brief 指定インデックスのインデックスデータを取得する
   /// @param index メッシュ番号（省略時は0）
   /// @return インデックスデータの参照
   const std::vector<uint32_t>& GetIndices(size_t index = 0) const {
	  assert(index < modelData_.meshes.size());
	  return modelData_.meshes[index].indices;
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

   /// @brief バインドポーズのスケルトンを取得する
   const Skeleton* GetBindSkeleton() const {
	  return skeleton_ ? &(*skeleton_) : nullptr;
   }

   /// @brief スキンクラスタを取得する
   SkinCluster* GetSkinCluster() {
	  return skinCluster_ ? &(*skinCluster_) : nullptr;
   }

   /// @brief スキンクラスタを取得する
   const SkinCluster* GetSkinCluster() const {
	  return skinCluster_ ? &(*skinCluster_) : nullptr;
   }

   /// @brief スキニングボーン情報を含むモデルか
   bool HasSkinningData() const {
	  return hasSkinningData_;
   }

private:
   std::string assetId_;
   ModelData modelData_;
   std::vector<ComPtr<ID3D12Resource>> vertexResources_;            // 複数リソース
   std::vector<D3D12_VERTEX_BUFFER_VIEW> vertexBufferViews_;        // 複数ビュー
   std::vector<Mesh::VertexData*> mappedVertexData_;                // 複数マップデータ
   std::vector<ComPtr<ID3D12Resource>> indexResources_;             // 複数インデックスバッファ
   std::vector<D3D12_INDEX_BUFFER_VIEW> indexBufferViews_;          // 複数インデックスビュー
   std::vector<uint32_t*> mappedIndexData_;                         // 複数インデックスマップデータ
   std::optional<Skeleton> skeleton_; // スケルトン（オプション）
   std::optional<SkinCluster> skinCluster_;
   GraphicsDevice* graphicsDevice_ = nullptr;
   bool hasSkinningData_ = false;
private:
   ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);

   Node ReadNode(aiNode* node);

   Skeleton CreateSkeleton(const Node& rootNode, const ModelData& modelData);

   int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);

   SkinCluster CreateSkinCluster(GraphicsDevice* device, const Skeleton& skeleton, const ModelData& modelData);
};
}
