#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "GameEngine/Math/VectorMath.h"

using namespace Microsoft::WRL;

namespace GameEngine {
class GraphicsDevice;

/// @brief ディレクショナルライトクラス
class DirectionalLight {
public:
   /// @brief GPUデータを確保する前の状態を構築する。
   DirectionalLight() = default;
   /// @brief マップ領域の共有による別オブジェクトからの書き換えを禁止する。
   DirectionalLight(const DirectionalLight&) = delete;
   /// @brief GPUデータの暗黙な共有を禁止する。
   DirectionalLight& operator=(const DirectionalLight&) = delete;

   /// @brief ディレクショナルライトデータ構造体
   struct DirectionalLightData {
	  Vector4 color; // ライトの色
	  Vector3 direction; // ライトの向き
	  float intensity; // 輝度
   };

   /// @brief デバイスを取得して初期化
   /// @param device デバイス
   static void Initialize(GraphicsDevice* device);

   /// @brief ディレクショナルライトの作成
   /// @param device デバイス
   /// @param color ライトの色。デフォルトは白（0xffffffff）
   /// @param direction ライトの向き。デフォルトは下方向（0.0f, -1.0f, 0.0f）
   /// @param intensity 輝度。デフォルトは1.0f
   /// @details ディレクショナルライトは、無限遠からの平行光源を表現する。
   void Create(unsigned int color = 0xffffffff, const Vector3& direction = { 0.0f,-1.0f,0.0f }, float intensity = 1.0f);

   /// @brief ディレクショナルライトデータへのポインタを取得
   /// @return directionalLightData_ メンバーへのポインタ
   const DirectionalLightData* GetDirectionalLightData() const { return directionalLightData_; }

   /// @brief ディレクショナルライトリソースを取得
   /// @return directionalLightResource_ メンバーへのポインタ
   ID3D12Resource* GetDirectionalLightResource() const { return directionalLightResource_.Get(); }

   /// @brief 照明の記述を検証・正規化してからGPUデータへ一括反映する。
   bool ApplyIllumination(const DirectionalLightData& illumination);

private:
   ComPtr<ID3D12Resource> directionalLightResource_ = nullptr;
   DirectionalLightData* directionalLightData_ = nullptr;
};
}