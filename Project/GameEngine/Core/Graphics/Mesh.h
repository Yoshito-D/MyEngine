#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <vector>
#include <string>
#include "Utility/VectorMath.h"

using namespace Microsoft::WRL;

namespace GameEngine {
class GraphicsDevice;

/// @brief メッシュクラス
class Mesh {
public:
   enum class PlaneOrientation {
	  XY,
	  XZ,
	  YZ,
   };

   /// @brief 頂点データ構造体
   struct VertexData {
	  Vector4 position; // 頂点位置
	  Vector2 texCoord; // テクスチャ座標
	  Vector3 normal; // 法線ベクトル
   };

   /// @brief デバイスを取得して初期化
   /// @param device デバイス
   static void Initialize(GraphicsDevice* device);

   /// @brief メッシュの初期化
   /// @param width 幅
   /// @param height 高さ
   /// @param orientation 生成平面（デフォルトはXY）
   void CreateSprite(float width, float height, PlaneOrientation orientation = PlaneOrientation::XY);

   /// @brief パーティクル用のクワッドメッシュを作成
   /// @param width 幅
   /// @param height 高さ
   /// @param orientation 生成平面（デフォルトはXY）
   /// @param originY 縦方向の原点位置（0.0=下端, 0.5=中央, 1.0=上端）
   void CreateParticleQuad(float width, float height, PlaneOrientation orientation = PlaneOrientation::XY, float originY = 0.5f);

   /// @brief スカイボックス用のキューブメッシュを作成
   void CreateSkybox();

   /// @brief リングメッシュを作成
   /// @param innerRadius 内半径
   /// @param outerRadius 外半径
   /// @param segmentCount セグメント数（リングの滑らかさを決定、デフォルトは32）
   /// @param orientation 生成平面（デフォルトはXY）
   void CreateRing(float innerRadius = 0.4f, float outerRadius = 0.5f, uint32_t segmentCount = 32, PlaneOrientation orientation = PlaneOrientation::XY);

   /// @brief 平面メッシュを作成
   /// @param width 幅
   /// @param depth 奥行き
   /// @param widthSegments 幅方向の分割数（デフォルトは1）
   /// @param depthSegments 奥行き方向の分割数（デフォルトは1）
   /// @param orientation 生成平面（デフォルトはXZ）
   void CreatePlane(float width = 1.0f, float depth = 1.0f, uint32_t widthSegments = 1, uint32_t depthSegments = 1, PlaneOrientation orientation = PlaneOrientation::XZ);

   /// @brief 円形メッシュを作成
   /// @param radius 半径
   /// @param segmentCount セグメント数（デフォルトは32）
   /// @param orientation 生成平面（デフォルトはXY）
   void CreateCircle(float radius = 0.5f, uint32_t segmentCount = 32, PlaneOrientation orientation = PlaneOrientation::XY);

   /// @brief ボックスメッシュを作成
   /// @param width 幅
   /// @param height 高さ
   /// @param depth 奥行き
   /// @param originY 縦方向の原点位置（0.0=下端, 0.5=中央, 1.0=上端）
   void CreateBox(float width = 1.0f, float height = 1.0f, float depth = 1.0f, float originY = 0.5f);

   /// @brief 球体メッシュを作成
   /// @param radius 半径
   /// @param stackCount 縦方向の分割数（デフォルトは16）
   /// @param sliceCount 横方向の分割数（デフォルトは32）
   /// @param originY 縦方向の原点位置（0.0=下端, 0.5=中央, 1.0=上端）
   void CreateSphere(float radius = 0.5f, uint32_t stackCount = 16, uint32_t sliceCount = 32, float originY = 0.5f);

   /// @brief トーラスメッシュを作成
   /// @param majorRadius チューブ中心の円半径
   /// @param minorRadius チューブ断面の半径
   /// @param majorSegments 主方向の分割数（デフォルトは32）
   /// @param minorSegments チューブ断面の分割数（デフォルトは16）
   /// @param originY 縦方向の原点位置（0.0=下端, 0.5=中央, 1.0=上端）
   void CreateTorus(float majorRadius = 0.5f, float minorRadius = 0.2f, uint32_t majorSegments = 32, uint32_t minorSegments = 16, float originY = 0.5f);

   /// @brief シリンダーメッシュを作成
   /// @param topRadius 上面の半径
   /// @param bottomRadius 底面の半径
   /// @param height 高さ
   /// @param segmentCount 側面の分割数（デフォルトは32）
   /// @param originY 縦方向の原点位置（0.0=底面, 0.5=中央, 1.0=上面）
   void CreateCylinder(float topRadius = 0.5f, float bottomRadius = 0.5f, float height = 1.0f, uint32_t segmentCount = 32, float originY = 0.5f);

   /// @brief 底面と上面のないシリンダーメッシュを作成
   /// @param topRadius 上面の半径
   /// @param bottomRadius 底面の半径
   /// @param height 高さ
   /// @param segmentCount 側面の分割数（デフォルトは32）
   /// @param originY 縦方向の原点位置（0.0=底面, 0.5=中央, 1.0=上面）
   void CreateCylinderWithoutCaps(float topRadius = 0.5f, float bottomRadius = 0.5f, float height = 1.0f, uint32_t segmentCount = 32, float originY = 0.5f);

   /// @brief コーンメッシュを作成
   /// @param radius 底面の半径
   /// @param height 高さ
   /// @param segmentCount 側面の分割数（デフォルトは32）
   /// @param originY 縦方向の原点位置（0.0=底面, 0.5=中央, 1.0=上面）
   void CreateCone(float radius = 0.5f, float height = 1.0f, uint32_t segmentCount = 32, float originY = 0.5f);

   /// @brief 三角形メッシュを作成
   /// @param v0 頂点0の位置
   /// @param v1 頂点1の位置
   /// @param v2 頂点2の位置
   /// @param orientation 生成平面（デフォルトはXY）
   void CreateTriangle(const Vector3& v0 = {-0.5f, 0.0f, 0.0f}, const Vector3& v1 = {0.0f, 1.0f, 0.0f}, const Vector3& v2 = {0.5f, 0.0f, 0.0f}, PlaneOrientation orientation = PlaneOrientation::XY);

   /// @brief 毎フレーム更新可能な動的メッシュ領域を確保する
   /// @param maxVertexCount 最大頂点数
   /// @param maxIndexCount 最大インデックス数
   void CreateDynamic(uint32_t maxVertexCount, uint32_t maxIndexCount);

   /// @brief 動的メッシュの頂点とインデックスを更新する
   /// @param vertices 新しい頂点列
   /// @param indices 新しいインデックス列
   void UpdateDynamic(const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices);

   const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return vertexBufferView_; }
   const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return indexBufferView_; }
   UINT GetIndexCount() const { return indexCount_; }

   VertexData* GetVertexData() const;
private:
   ComPtr<ID3D12Resource> vertexResource_ = nullptr;
   ComPtr<ID3D12Resource> indexResource_ = nullptr;
   D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
   D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
   VertexData* vertexData_ = nullptr;
   uint32_t* dynamicIndexData_ = nullptr;
   uint32_t dynamicVertexCapacity_ = 0;
   uint32_t dynamicIndexCapacity_ = 0;
   UINT indexCount_ = 0;
};
}
