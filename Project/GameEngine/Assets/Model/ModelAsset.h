#pragma once

#include <d3d12.h>
#include <string>
#include <vector>
#include <optional>
#include <map>
#include <array>
#include "GameEngine/Graphics/Resources/Mesh.h"
#include "GameEngine/Math/MathUtils.h"
#include "GameEngine/Assets/Animation/Skeleton.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <span>
#include <memory>

namespace GameEngine {
class GraphicsDevice;
class SrvDescriptorAllocation;

/// @brief マテリアルアセットを表す構造体
struct MaterialAsset {
   std::string textureFilePath; // テクスチャファイルのパス
};

/// @brief メッシュデータを表す構造体
struct MeshData {
   std::vector<Mesh::VertexData> vertices; ///< メッシュを構成する頂点データ。
   std::vector<uint32_t> indices;          ///< 三角形を構成する頂点配列への参照番号。
   uint32_t materialIndex; // このメッシュが使用するマテリアルのインデックス
};

struct Node {
   Transform transform;        ///< 位置・回転・拡縮をまとめた変換情報。
   Matrix4x4 localMatrix;      ///< 親の座標系に対するローカル変換行列。
   std::string name;           ///< 識別とエディター表示に使う名前。
   std::vector<Node> children; ///< この要素に属する子ノード。
};

struct VertexWeightData {
   float weight;       // ウェイト値
   uint32_t vertexId;  // 頂点ID
   uint32_t meshIndex; // メッシュID
};

struct JointWeightData {
   Matrix4x4 inverseBindPoseMatrix;             // 逆バインドポーズ行列
   std::vector<VertexWeightData> vertexWeights; // このジョイントの影響を受ける頂点とウェイト。
};

const uint32_t kNumMaxInfluence = 4; // 頂点が影響を受ける最大ジョイント数

struct VertexInfluence {
   std::array<float, kNumMaxInfluence> weights;        ///< 各ジョイントが頂点に与える影響の重み。
   std::array<int32_t, kNumMaxInfluence> jointIndices; ///< 重みに対応するジョイント番号。
};

struct WellForGPU {
   Matrix4x4 skeletonSpaceMatrix;                 ///< ジョイント座標をスケルトン基準へ変換する行列。
   Matrix4x4 skeletonSpaceInverseTransposeMatrix; ///< スキニング後の法線変換に使う逆転置行列。
};

struct SkinningInformationForGPU {
   uint32_t numVertices = 0; ///< 処理対象の頂点数。Computeの範囲外アクセスを防ぐ上限。
   uint32_t padding[3] = {}; ///< GPU転送データの配置境界を揃える未使用領域。
};

class SkinCluster {
   friend class ModelAsset;

   // 不変な入力はコピー間で共有し、Palette/出力を差し替える際には対応する所有権も置換する。
   /// @brief メッシュごとの入力頂点SRVの共有所有権。
   std::vector<std::shared_ptr<SrvDescriptorAllocation>> inputVertexDescriptors;
   /// @brief メッシュごとの頂点ウェイトSRVの共有所有権。
   std::vector<std::shared_ptr<SrvDescriptorAllocation>> influenceDescriptors;
   /// @brief メッシュごとのスキニング出力UAVの共有所有権。
   std::vector<std::shared_ptr<SrvDescriptorAllocation>> skinnedVertexDescriptors;
   std::shared_ptr<SrvDescriptorAllocation> paletteDescriptor;             ///< ジョイント行列パレットSRVの共有所有権。
   std::vector<Matrix4x4> inverseBindPoseMatrices;                         // 逆バインドポーズ行列のリスト
   std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> influenceResources; // メッシュごとの頂点影響リソース
   std::vector<D3D12_VERTEX_BUFFER_VIEW> influenceBufferViews;             // メッシュごとの頂点バッファ
   std::vector<VertexInfluence*> mappedInfluenceData;                      // メッシュごとのマップ済み頂点影響データ
   /// @brief メッシュごとの入力頂点SRVのCPU/GPUハンドル。
   std::vector<std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>> inputVertexSrvHandles;
   /// @brief メッシュごとの頂点ウェイトSRVのCPU/GPUハンドル。
   std::vector<std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>> influenceSrvHandles;
   /// @brief インスタンス固有のスキニング済み頂点を保持するGPUバッファ。
   std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> skinnedVertexResources;
   /// @brief スキニング済み頂点バッファを描画入力に指定するビュー。
   std::vector<D3D12_VERTEX_BUFFER_VIEW> skinnedVertexBufferViews;
   /// @brief スキニング出力先UAVのCPU/GPUハンドル。
   std::vector<std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>> skinnedVertexUavHandles;
   /// @brief メッシュごとの頂点数をComputeへ渡す定数バッファ。
   std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> skinningInformationResources;
   /// @brief 頂点数定数バッファの永続Map先。対応リソースが寿命を管理する。
   std::vector<SkinningInformationForGPU*> mappedSkinningInformationData;
   std::vector<D3D12_RESOURCE_STATES> skinnedVertexResourceStates; ///< スキニング出力バッファごとの現在の遷移状態。
   Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;         ///< ジョイント行列パレットを格納するGPUリソース。
   std::span<WellForGPU> mappedPalette; // マップされたスケルトン行列データへのスパン
   std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE>
      paletteSrvHandle; // パレット用のCPU/GPUディスクリプタハンドル

public:
   /// @brief 未確保のスキニング状態を構築する。
   SkinCluster() = default;

   /// @brief 同じ出力GPUリソースを別の遷移状態で管理するコピーを禁止する。
   SkinCluster(const SkinCluster&) = delete;

   /// @brief コピー代入によるGPU実行状態の別名を禁止する。
   SkinCluster& operator=(const SkinCluster&) = delete;

   /// @brief 確保済みリソースと対応する実行状態の所有権をまとめて移す。
   SkinCluster(SkinCluster&&) noexcept = default;

   /// @brief リソースと実行状態を同じ所有者へ移動代入する。
   SkinCluster& operator=(SkinCluster&&) noexcept = default;

   /// @brief バインド行列と同じ階層の姿勢を、法線用逆転置と共にGPUパレットへ反映する。
   bool ApplyPose(const Skeleton& pose);

   /// @brief GPUと同じウェイト・パレットで1頂点の現在位置を評価する。
   Vector3 EvaluateSkinnedPosition(size_t meshIndex, uint32_t vertexIndex, const Vector3& bindPosition) const;

   /// @brief パレットの逆バインドを戻し、現在のジョイント姿勢を取得する。
   bool TryResolveJointMatrix(size_t jointIndex, Matrix4x4& matrix) const;

   /// @brief Computeの入出力を検証し、遷移・Dispatch・頂点入力への復帰を同じ操作で完了する。
   bool DispatchSkinning(ID3D12GraphicsCommandList* commands, size_t meshIndex, const std::array<UINT, 5>& slots);

   /// @brief パレットのリソースとディスクリプターが利用可能か調べる。
   bool HasPalette() const { return paletteResource && paletteSrvHandle.second.ptr != 0 && !mappedPalette.empty(); }

   /// @brief サブメッシュの頂点数が確保済み出力と一致するか確認する。
   bool CanSkinVertices(size_t meshIndex, size_t count) const;

   /// @brief 指定メッシュのCompute Skinning用リソースがそろっているか
   /// @param meshIndex メッシュ番号
   /// @return 必要なSRV/UAV/CBV/VBVが利用可能ならtrue
   bool HasComputeSkinningResources(size_t meshIndex) const;

   /// @brief 指定メッシュのCompute Skinning済み頂点バッファビューを取得する
   /// @param meshIndex メッシュ番号
   /// @return 利用可能な場合は頂点バッファビュー、範囲外ならnullptr
   const D3D12_VERTEX_BUFFER_VIEW* GetSkinnedVertexBufferView(size_t meshIndex) const;

   const D3D12_VERTEX_BUFFER_VIEW* GetInfluenceBufferView(size_t meshIndex) const;
};

/// @brief モデルデータを表す構造体
struct ModelData {
   std::map<std::string, JointWeightData> skinClusterData; ///< ジョイント名ごとのバインド行列と頂点ウェイト。
   std::vector<MeshData> meshes;         // 複数メッシュ対応
   std::vector<MaterialAsset> materials; // 複数マテリアル対応
   Node rootNode;                        // ルートノード
};

/// @brief モデルアセットクラス
class ModelAsset {
public:
   /// @brief 永続化と参照解決で使用する完全なアセットIDを設定する。
   void SetAssetId(const std::string& assetId) { assetId_ = assetId; }

   /// @brief このモデルの完全なアセットIDを取得する。
   const std::string& GetAssetId() const { return assetId_; }

   /// @brief モデルを検証してからCPUデータとGPUリソースをロードする。
   /// @param device グラフィックスデバイス
   /// @param modelPath モデルファイルのパス
   /// @param modelName モデル名
   /// @return 読込成功時はtrue。無効なモデルやデバイスでは既存データを変更せずfalse。
   bool LoadFile(GraphicsDevice* device, const std::string& modelPath, const std::string& modelName);

   /// @brief モデル単位で利用するスキンクラスタを生成する
   std::optional<SkinCluster> CreateSkinClusterInstance() const;

   /// @brief 指定インデックスの頂点バッファビューを取得する
   /// @param index メッシュ番号（省略時は0）
   /// @return 頂点バッファビュー
   const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView(size_t index = 0) const;

   /// @brief 指定インデックスの頂点バッファを取得する
   /// @param index メッシュ番号（省略時は0）
   /// @return 頂点バッファリソース
   ID3D12Resource* GetVertexBuffer(size_t index = 0) const;

   /// @brief 指定インデックスのインデックスバッファビューを取得する
   /// @param index メッシュ番号（省略時は0）
   /// @return インデックスバッファビュー
   const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView(size_t index = 0) const;

   /// @brief 指定インデックスのインデックスデータを取得する
   /// @param index メッシュ番号（省略時は0）
   /// @return インデックスデータの参照
   const std::vector<uint32_t>& GetIndices(size_t index = 0) const;

   /// @brief 指定インデックスの頂点データを取得する
   /// @param index メッシュ番号（省略時は0）
   /// @return 頂点データの参照
   const std::vector<Mesh::VertexData>& GetVertices(size_t index = 0) const;

   /// @brief 指定インデックスのマテリアルを取得する
   /// @param index マテリアル番号（省略時は0）
   /// @return マテリアルアセット
   const MaterialAsset& GetMaterialAsset(size_t index = 0) const;

   /// @brief メッシュデータ一覧を取得する
   const std::vector<MeshData>& GetMeshData() const { return modelData_.meshes; }

   /// @brief マテリアルアセット一覧を取得する
   const std::vector<MaterialAsset>& GetMaterialAssets() const { return modelData_.materials; }

   /// @brief ルートノードを取得する
   const Node& GetRootNode() const { return modelData_.rootNode; }

   /// @brief バインドポーズのスケルトンを取得する
   const Skeleton* GetBindSkeleton() const { return skeleton_ ? &(*skeleton_) : nullptr; }

   /// @brief スキンクラスタを取得する

   /// @brief スキンクラスタを取得する
   const SkinCluster* GetSkinCluster() const { return skinCluster_ ? &(*skinCluster_) : nullptr; }

   /// @brief スキニングボーン情報を含むモデルか
   bool HasSkinningData() const { return hasSkinningData_; }

private:
   static void CreateInputSkinningResourceViews(GraphicsDevice* device, SkinCluster& skinCluster,
      const std::vector<MeshData>& meshes, const std::vector<ComPtr<ID3D12Resource>>& vertices);
   static void CreateOutputSkinningResources(
      GraphicsDevice* device, SkinCluster& skinCluster, const std::vector<MeshData>& meshes);
   std::string assetId_;                                     ///< このモデルの完全なアセットID。
   ModelData modelData_;                                     ///< 読み込み済みのメッシュ・マテリアル・スキニング情報。
   std::vector<ComPtr<ID3D12Resource>> vertexResources_;     // 複数リソース
   std::vector<D3D12_VERTEX_BUFFER_VIEW> vertexBufferViews_; // 複数ビュー
   std::vector<Mesh::VertexData*> mappedVertexData_;         // 複数マップデータ
   std::vector<ComPtr<ID3D12Resource>> indexResources_;      // 複数インデックスバッファ
   std::vector<D3D12_INDEX_BUFFER_VIEW> indexBufferViews_;   // 複数インデックスビュー
   std::vector<uint32_t*> mappedIndexData_;                  // 複数インデックスマップデータ
   std::optional<Skeleton> skeleton_;                        // スケルトン（オプション）
   std::optional<SkinCluster> skinCluster_;                  ///< 描画インスタンスが持つスキニング用の状態とリソース。
   GraphicsDevice* graphicsDevice_ = nullptr; ///< GPUリソースの生成とディスクリプタ割り当てに使うデバイスへの参照。
   bool hasSkinningData_ = false;             ///< スキニングボーン情報を含むモデルか。

private:
   ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);

   Node ReadNode(aiNode* node);

   Skeleton CreateSkeleton(const Node& rootNode, const ModelData& modelData);

   int32_t CreateJoint(const Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);

   SkinCluster CreateSkinCluster(GraphicsDevice* device, const Skeleton& skeleton, const ModelData& modelData);
};
}
