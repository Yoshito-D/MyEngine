#pragma once

#include "GameEngine/Graphics/PostProcess/PostProcess.h"
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {

/// @brief ピクセル化効果
class Pixelation : public PostProcess {
public:
   /// @brief ピクセル化エフェクト用パラメータ構造体
   struct PixelationCB {
      float pixelSize;   ///< ピクセル化で同じ色にまとめるブロックの辺長。
      float screenSizeX; ///< 効果を適用する画面の横幅（ピクセル）。
      float screenSizeY; ///< 効果を適用する画面の高さ（ピクセル）。
      float padding;     ///< GPU転送データの配置境界を揃える未使用領域。
   };

   /// @brief 初期化
   /// @param device グラフィックスデバイス
   /// @param renderTarget レンダーターゲット
   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;

   /// @brief エフェクトを適用
   /// @param inputSRV 入力SRV
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   void ImGuiEdit() override;
#endif
   const char* GetEffectName() const override { return "Pixelation"; }

   /// @copydoc PostProcess::SerializeSettings
   nlohmann::json SerializeSettings() const override;

   /// @copydoc PostProcess::DeserializeSettings
   bool DeserializeSettings(const nlohmann::json& settings) override;

   // パラメータ設定
   void SetPixelSize(float size);

   void SetScreenSize(float width, float height);

private:
   float pixelSize_ = 4.0f;      ///< ピクセル化で同じ色にまとめるブロックの辺長。
   float screenSizeX_ = 1280.0f; ///< 効果を適用する画面の横幅（ピクセル）。
   float screenSizeY_ = 720.0f;  ///< 効果を適用する画面の高さ（ピクセル）。

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_; ///< この処理の設定をGPUへ渡す所有定数バッファ。
   PixelationCB* constantBufferData_ = nullptr; ///< 定数バッファの永続Map先。対応するリソースがメモリを所有する。

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}
