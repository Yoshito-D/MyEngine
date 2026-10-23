#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include "GameEngine/Math/VectorMath.h"

using namespace Microsoft::WRL;

namespace GameEngine {
class GraphicsDevice;

/// @brief ライト用データバッファクラス（構造化バッファ管理）
class LightDataBuffer {
public:
   /// @brief ディレクショナルライトデータ構造体
   struct DirectionalLightData {
      Vector4 color;     ///< 表示色のRGBA成分。
      Vector3 direction; ///< 光が進むワールド方向。
      float intensity;   ///< 効果または光の強さ。
   };

   /// @brief ポイントライトデータ構造体
   struct PointLightData {
      Vector4 color;    ///< 表示色のRGBA成分。
      Vector3 position; ///< 光源中心のワールド位置。
      float intensity;  ///< 効果または光の強さ。
      float radius;     ///< 形状または効果範囲の半径。
      float decay;      ///< 距離減衰。
      float padding[2]; ///< GPU転送データの配置境界を揃える未使用領域。
   };

   /// @brief スポットライトデータ構造体
   struct SpotLightData {
      Vector4 color;         ///< 表示色のRGBA成分。
      Vector3 position;      ///< 光源中心のワールド位置。
      float intensity;       ///< 効果または光の強さ。
      Vector3 direction;     ///< 光が進むワールド方向。
      float distance;        ///< 基準位置からの距離。
      float decay;           ///< 距離減衰。
      float cosAngle;        ///< スポット光の外側コーン角の余弦。
      float cosFalloffStart; ///< スポット光の減衰開始コーン角の余弦。
      float padding;         ///< GPU転送データの配置境界を揃える未使用領域。
   };

   /// @brief エリアライトデータ構造体
   struct AreaLightData {
      Vector4 color;    ///< 表示色のRGBA成分。
      Vector3 position; ///< 光源中心のワールド位置。
      float intensity;  ///< 効果または光の強さ。
      Vector3 normal;   ///< 面の向きを表す法線ベクトル。
      float width;      ///< 対象領域の横幅。
      Vector3 tangent;  ///< ライトの右方向ベクトル。
      float height;     ///< 対象領域の高さ。
      Vector3 padding;  ///< GPU転送データの配置境界を揃える未使用領域。
      float padding2;   ///< GPU転送データの配置境界を揃える未使用領域。
   };

   /// @brief ライトカウント構造体
   struct LightCountData {
      uint32_t directionalLightCount; ///< GPUへ渡す有効な平行光源の数。
      uint32_t pointLightCount;       ///< GPUへ渡す有効な点光源の数。
      uint32_t spotLightCount;        ///< GPUへ渡す有効なスポット光源の数。
      uint32_t areaLightCount;        ///< GPUへ渡す有効な矩形光源の数。
   };

   /// @brief デバイスを取得して初期化
   /// @param device デバイス
   static void Initialize(GraphicsDevice* device);

   /// @brief 構造化バッファを作成
   /// @param maxDirectionalLights ディレクショナルライトの最大数
   /// @param maxPointLights ポイントライトの最大数
   /// @param maxSpotLights スポットライトの最大数
   /// @param maxAreaLights エリアライトの最大数
   void Create(uint32_t maxDirectionalLights = 1, uint32_t maxPointLights = 32, uint32_t maxSpotLights = 32,
      uint32_t maxAreaLights = 16);

   /// @brief ディレクショナルライトデータを更新
   /// @param lights ライトデータの配列
   void UpdateDirectionalLights(const std::vector<DirectionalLightData>& lights);

   /// @brief ポイントライトデータを更新
   /// @param lights ライトデータの配列
   void UpdatePointLights(const std::vector<PointLightData>& lights);

   /// @brief スポットライトデータを更新
   /// @param lights ライトデータの配列
   void UpdateSpotLights(const std::vector<SpotLightData>& lights);

   /// @brief エリアライトデータを更新
   /// @param lights ライトデータの配列
   void UpdateAreaLights(const std::vector<AreaLightData>& lights);

   /// @brief ライトカウントを更新
   void UpdateLightCount();

   /// @brief ディレクショナルライトSRVハンドルを取得
   /// @return SRVのGPUハンドル
   D3D12_GPU_DESCRIPTOR_HANDLE GetDirectionalLightSRV() const { return directionalLightSRVHandle_; }

   /// @brief ポイントライトSRVハンドルを取得
   /// @return SRVのGPUハンドル
   D3D12_GPU_DESCRIPTOR_HANDLE GetPointLightSRV() const { return pointLightSRVHandle_; }

   /// @brief スポットライトSRVハンドルを取得
   /// @return SRVのGPUハンドル
   D3D12_GPU_DESCRIPTOR_HANDLE GetSpotLightSRV() const { return spotLightSRVHandle_; }

   /// @brief エリアライトSRVハンドルを取得
   /// @return SRVのGPUハンドル
   D3D12_GPU_DESCRIPTOR_HANDLE GetAreaLightSRV() const { return areaLightSRVHandle_; }

   /// @brief ライトカウントリソースを取得
   /// @return ライトカウントリソース
   ID3D12Resource* GetLightCountResource() const { return lightCountResource_.Get(); }

private:
   // 構造化バッファリソース
   ComPtr<ID3D12Resource> directionalLightBuffer_ = nullptr; ///< 平行光源のパラメーターを保持するバッファ。
   ComPtr<ID3D12Resource> pointLightBuffer_ = nullptr;       ///< 点光源のパラメーターを保持するバッファ。
   ComPtr<ID3D12Resource> spotLightBuffer_ = nullptr;        ///< スポット光源のパラメーターを保持するバッファ。
   ComPtr<ID3D12Resource> areaLightBuffer_ = nullptr;        ///< 矩形光源のパラメーターを保持するバッファ。
   ComPtr<ID3D12Resource> lightCountResource_ = nullptr;     ///< 種類別の有効ライト数を保持する所有GPUリソース。

   // マップ済みポインタ
   /// @brief 平行光源のパラメーターを参照するCPU側の永続Map先。対応リソースがメモリを所有する。
   DirectionalLightData* directionalLightData_ = nullptr;
   /// @brief 点光源のパラメーターを参照するCPU側の永続Map先。対応リソースがメモリを所有する。
   PointLightData* pointLightData_ = nullptr;
   /// @brief スポット光源のパラメーターを参照するCPU側の永続Map先。対応リソースがメモリを所有する。
   SpotLightData* spotLightData_ = nullptr;
   /// @brief 矩形光源のパラメーターを参照するCPU側の永続Map先。対応リソースがメモリを所有する。
   AreaLightData* areaLightData_ = nullptr;
   /// @brief 種類別の有効ライト数を参照するCPU側の永続Map先。対応リソースがメモリを所有する。
   LightCountData* lightCountData_ = nullptr;

   // SRVハンドル
   D3D12_GPU_DESCRIPTOR_HANDLE directionalLightSRVHandle_{}; ///< ディレクショナルライトSRVハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE pointLightSRVHandle_{};       ///< ポイントライトSRVハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE spotLightSRVHandle_{};        ///< スポットライトSRVハンドル。
   D3D12_GPU_DESCRIPTOR_HANDLE areaLightSRVHandle_{};        ///< エリアライトSRVハンドル。

   // 最大ライト数
   uint32_t maxDirectionalLights_ = 0; ///< 確保した平行光源バッファの要素数上限。
   uint32_t maxPointLights_ = 0;       ///< 確保した点光源バッファの要素数上限。
   uint32_t maxSpotLights_ = 0;        ///< 確保したスポット光源バッファの要素数上限。
   uint32_t maxAreaLights_ = 0;        ///< 確保した矩形光源バッファの要素数上限。

   // 現在のライト数
   uint32_t currentDirectionalLightCount_ = 0; ///< 現在GPUバッファへ転送している平行光源の数。
   uint32_t currentPointLightCount_ = 0;       ///< 現在GPUバッファへ転送している点光源の数。
   uint32_t currentSpotLightCount_ = 0;        ///< 現在GPUバッファへ転送しているスポット光源の数。
   uint32_t currentAreaLightCount_ = 0;        ///< 現在GPUバッファへ転送している矩形光源の数。

   /// @brief 構造化バッファとSRVを作成
   /// @param buffer バッファのポインタ
   /// @param srvHandle SRVハンドルの参照
   /// @param elementSize 要素のサイズ
   /// @param elementCount 要素数
   void CreateStructuredBuffer(ComPtr<ID3D12Resource>& buffer, D3D12_GPU_DESCRIPTOR_HANDLE& srvHandle,
      uint32_t elementSize, uint32_t elementCount);
};
}
