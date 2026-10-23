#pragma once

#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include "GameEngine/Math/VectorMath.h"

namespace GameEngine {
class GraphicsDevice;

/// @brief ポイントライトクラス
class PointLight {
public:
   /// @brief GPUデータを確保する前の状態を構築する。
   PointLight() = default;

   /// @brief マップ領域の共有による別オブジェクトからの書き換えを禁止する。
   PointLight(const PointLight&) = delete;

   /// @brief GPUデータの暗黙な共有を禁止する。
   PointLight& operator=(const PointLight&) = delete;

   /// @brief ポイントライトデータ構造体
   struct PointLightData {
      Vector4 color;    ///< 表示色のRGBA成分。
      Vector3 position; ///< 光源中心のワールド位置。
      float intensity;  ///< 効果または光の強さ。
      float radius;     ///< 形状または効果範囲の半径。
      float decay;      ///< 距離減衰。
      float padding[2]; ///< GPU転送データの配置境界を揃える未使用領域。
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
   void Create(std::uint32_t color = 0xffffffff, const Vector3& position = {0.0f, 0.0f, 0.0f}, float intensity = 1.0f,
      float radius = 2.0f, float decay = 0.2f);

   /// @brief ポイントライトデータを取得
   /// @return ポイントライトデータへのポインタ
   const PointLightData* GetPointLightData() const { return pointLightData_; }

   /// @brief ポイントライトリソースを取得
   /// @return ポイントライトリソースへのポインタ
   ID3D12Resource* GetPointLightResource() const { return pointLightResource_.Get(); }

   /// @brief 照明の記述を検証・正規化してからGPUデータへ一括反映する。
   bool ApplyIllumination(const PointLightData& illumination);

private:
   /// @brief 点光源のパラメーターを保持する所有GPUリソース。
   Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_ = nullptr;
   PointLightData* pointLightData_ = nullptr; ///< ポイントライトデータ。
};
}
