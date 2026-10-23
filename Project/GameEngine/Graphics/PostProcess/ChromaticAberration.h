#pragma once

#include "GameEngine/Graphics/PostProcess/PostProcess.h"
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {

/// @brief 色収差効果
class ChromaticAberration : public PostProcess {
public:
   /// @brief 色収差効果のパラメータ構造体
   struct ParamsCB {
      float centerX;             ///< 画面上の効果中心のX座標（正規化UV）。
      float centerY;             ///< 画面上の効果中心のY座標（正規化UV）。
      float pixelShift;          ///< 色収差でRGBの参照位置をずらす量（ピクセル）。
      int32_t useFixedDirection; ///< 中心からの放射方向ではなく指定方向へ色をずらすかどうか。
      float fixedDirectionX;     ///< 色収差の固定方向のX成分。
      float fixedDirectionY;     ///< 色収差の固定方向のY成分。
      float texSizeX;            ///< 入力テクスチャの横幅（ピクセル）。
      float texSizeY;            ///< 入力テクスチャの高さ（ピクセル）。
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
   const char* GetEffectName() const override { return "Chromatic Aberration"; }

   /// @copydoc PostProcess::SerializeSettings
   nlohmann::json SerializeSettings() const override;

   /// @copydoc PostProcess::DeserializeSettings
   bool DeserializeSettings(const nlohmann::json& settings) override;

   // パラメータ設定
   void SetStrength(float strength);

   void SetCenter(float x, float y);

private:
   float centerX_ = 0.5f;          ///< 画面上の効果中心のX座標（正規化UV）。
   float centerY_ = 0.5f;          ///< 画面上の効果中心のY座標（正規化UV）。
   float pixelShift_ = 5.0f;       ///< 色収差でRGBの参照位置をずらす量（ピクセル）。
   int32_t useFixedDirection_ = 0; ///< 中心からの放射方向ではなく指定方向へ色をずらすかどうか。
   float fixedDirectionX_ = 1.0f;  ///< 色収差の固定方向のX成分。
   float fixedDirectionY_ = 0.0f;  ///< 色収差の固定方向のY成分。

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_; ///< この処理の設定をGPUへ渡す所有定数バッファ。
   ParamsCB* constantBufferData_ = nullptr; ///< 定数バッファの永続Map先。対応するリソースがメモリを所有する。

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}
