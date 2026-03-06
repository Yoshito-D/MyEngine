#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "Utility/VectorMath.h"

namespace GameEngine {
class GraphicsDevice;

/// @brief エリアライトクラス
class AreaLight {
public:
   /// @brief エリアライトデータ構造体
   struct AreaLightData {
	  Vector4 color;           // ライトの色
	  Vector3 position;        // 中心座標
	  float intensity;         // 強度
	  Vector3 normal;          // 照射方向
	  float width;             // 幅
	  Vector3 tangent;         // ライトの右方向ベクトル
	  float height;            // 高さ
	  Vector3 padding;         // パディング
	  float padding2;          // パディング
   };

   /// @brief デバイスを取得して初期化
   /// @param device デバイス
   static void Initialize(GraphicsDevice* device);

   /// @brief エリアライトを作成
   /// @param position 中心座標
   /// @param normal 照射方向
   /// @param tangent ライトの右方向ベクトル（回転制御用）
   /// @param size 幅と高さ
   /// @param color 光の色
   /// @param intensity 強度
   void Create(const Vector3& position = { 0.0f, 0.0f, 0.0f },
              const Vector3& normal = { 0.0f, 0.0f, -1.0f },
              const Vector3& tangent = { 1.0f, 0.0f, 0.0f },
              const Vector2& size = { 1.0f, 1.0f },
              const Vector3& color = { 1.0f, 1.0f, 1.0f },
              float intensity = 1.0f);

   /// @brief エリアライトデータを取得
   /// @return エリアライトデータへのポインタ
   AreaLightData* GetAreaLightData() const { return areaLightData_; }

   /// @brief エリアライトリソースを取得
   /// @return エリアライトリソースへのポインタ
   ID3D12Resource* GetAreaLightResource() const { return areaLightResource_.Get(); }

private:
   Microsoft::WRL::ComPtr<ID3D12Resource> areaLightResource_ = nullptr;
   AreaLightData* areaLightData_ = nullptr;
};
}
