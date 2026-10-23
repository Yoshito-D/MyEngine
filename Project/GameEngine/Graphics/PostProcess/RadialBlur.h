#pragma once

#include "GameEngine/Graphics/PostProcess/PostProcess.h"
#include "GameEngine/Math/Types/Vector2.h"
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>

namespace GameEngine {
struct RadialBlurParams {
   Vector2 center = {0.5f, 0.5f}; ///< 中心点。
   float strength = 0.0f;         ///< 効果を適用する強さ。
   int32_t sampleCount = 15;      ///< 効果の評価に使うテクスチャサンプル数。
};

/// @brief ラジアルブラー効果
class RadialBlur : public PostProcess {
public:
   /// @brief ラジアルブラー用定数バッファ構造体
   struct RadialBlurCB {
      float centerX;       ///< 画面上の効果中心のX座標（正規化UV）。
      float centerY;       ///< 画面上の効果中心のY座標（正規化UV）。
      float strength;      ///< 効果を適用する強さ。
      int32_t sampleCount; ///< 効果の評価に使うテクスチャサンプル数。
   };

   /// @brief 初期化（パイプラインは外部から設定される）
   /// @param device グラフィックスデバイス
   /// @param renderTarget レンダーターゲット
   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;

   /// @brief エフェクトを適用
   /// @param inputSRV 入力SRV
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   void ImGuiEdit() override;
#endif
   const char* GetEffectName() const override { return "Radial Blur"; }

   /// @copydoc PostProcess::SerializeSettings
   nlohmann::json SerializeSettings() const override;

   /// @copydoc PostProcess::DeserializeSettings
   bool DeserializeSettings(const nlohmann::json& settings) override;

   void SetParams(const RadialBlurParams& params);

   const RadialBlurParams& GetParams() const { return params_; }

   // パラメータ設定
   void SetBlurStrength(float strength);

   void SetCenter(float x, float y);

   void SetSampleCount(int32_t count);

private:
   RadialBlurParams params_; ///< CPU側で保持する効果の設定値。

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_; ///< この処理の設定をGPUへ渡す所有定数バッファ。
   RadialBlurCB* constantBufferData_ = nullptr; ///< 定数バッファの永続Map先。対応するリソースがメモリを所有する。

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}
