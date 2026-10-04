#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "GameEngine/Math/VectorMath.h"

namespace GameEngine {
class GraphicsDevice;

/// @brief スポットライトクラス
class SpotLight {
public:
   /// @brief GPUデータを確保する前の状態を構築する。
   SpotLight() = default;
   /// @brief マップ領域の共有による別オブジェクトからの書き換えを禁止する。
   SpotLight(const SpotLight&) = delete;
   /// @brief GPUデータの暗黙な共有を禁止する。
   SpotLight& operator=(const SpotLight&) = delete;

   /// @brief スポットライトデータ構造体
   struct SpotLightData {
	  Vector4 color;
	  Vector3 position;
	  float intensity;
	  Vector3 direction;
	  float distance;
	  float decay;
	  float cosAngle;
	  float cosFalloffStart;
	  float padding;
   };

   /// @brief デバイスを取得して初期化
   /// @param device デバイス
   static void Initialize(GraphicsDevice* device);

   /// @brief スポットライトを作成
   /// @param color 色
   /// @param position 位置 
   /// @param intensity 強度
   /// @param direction 方向
   /// @param distance 到達距離
   /// @param decay 減衰
   /// @param cosAngle スポットライトの角度の余弦
   /// @param cosFalloffStart スポットライトの減衰開始角度の余弦
   void Create(unsigned int color = 0xffffffff, const Vector3& position = { 0.0f,0.0f,0.0f }, float intensity = 1.0f, const Vector3& direction = Vector3(0.0f, -1.0f, 0.0f), float distance = 5.0f, float decay = 0.2f, float cosAngle = 0.0f, float cosFalloffStart = 0.0f);

   /// @brief スポットライトデータへのポインタを取得
   /// @return スポットライトデータへのポインタ
   const SpotLightData* GetSpotLightData() const { return spotLightData_; }

   /// @brief スポットライトリソースを取得
   /// @return スポットライトリソースへのポインタ
   ID3D12Resource* GetSpotLightResource() const { return spotLightResource_.Get(); }

   /// @brief 照明の記述を検証・正規化してからGPUデータへ一括反映する。
   bool ApplyIllumination(const SpotLightData& illumination);

private:
   Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_ = nullptr;
   SpotLightData* spotLightData_ = nullptr;

};
}