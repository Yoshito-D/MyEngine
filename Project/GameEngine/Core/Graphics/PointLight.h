#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "Utility/VectorMath.h"

namespace GameEngine {
class GraphicsDevice;

/// @brief ポイントライトクラス
class PointLight {
public:
   /// @brief ポイントライトデータ構造体
   struct PointLightData {
	  Vector4 color;
	  Vector3 position;
	  float intensity;
	  float radius;
	  float decay;
	  float padding[2];
   };

   /// @brief デバイスを取得して初期化
   /// @param device デバイス
   static void Initialize(GraphicsDevice* device);

   /// @brief ポイントライトを作成
   /// @param color 色
   /// @param position 位置 
   /// @param intensity 強度
   /// @param radius 半径
   /// @param decay 減衰
   void Create(unsigned int color = 0xffffffff, const Vector3& position = { 0.0f,0.0f,0.0f }, float intensity = 1.0f, float radius = 2.0f, float decay = 0.2f);

   /// @brief ポイントライトデータを取得
   /// @return ポイントライトデータへのポインタ
   PointLightData* GetPointLightData() const { return pointLightData_; }

   /// @brief ポイントライトリソースを取得
   /// @return ポイントライトリソースへのポインタ
   ID3D12Resource* GetPointLightResource() const { return pointLightResource_.Get(); }

private:
   Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_ = nullptr;
   PointLightData* pointLightData_ = nullptr;

};
}