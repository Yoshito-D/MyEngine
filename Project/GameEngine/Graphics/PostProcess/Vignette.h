#pragma once

#include "GameEngine/Graphics/PostProcess/PostProcess.h"
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {

/// @brief ビネット効果
class Vignette : public PostProcess {
public:
   /// @brief ビネット効果用パラメータ構造体
   struct VignetteCB {
      float centerX;        ///< 画面上の効果中心のX座標（正規化UV）。
      float centerY;        ///< 画面上の効果中心のY座標（正規化UV）。
      float radius;         ///< 形状または効果範囲の半径。
      float softness;       ///< ビネット境界が滑らかに変化する幅。
      float vignetteColorR; ///< 画面周辺へ重ねるビネット色の赤成分。
      float vignetteColorG; ///< 画面周辺へ重ねるビネット色の緑成分。
      float vignetteColorB; ///< 画面周辺へ重ねるビネット色の青成分。
      float intensity;      ///< 効果または光の強さ。
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
   const char* GetEffectName() const override { return "Vignette"; }

   /// @copydoc PostProcess::SerializeSettings
   nlohmann::json SerializeSettings() const override;

   /// @copydoc PostProcess::DeserializeSettings
   bool DeserializeSettings(const nlohmann::json& settings) override;

   // パラメータ設定
   void SetIntensity(float intensity);

   void SetSoftness(float softness);

   void SetRadius(float radius);

private:
   float intensity_ = 0.5f;      ///< 効果または光の強さ。
   float softness_ = 0.5f;       ///< ビネット境界が滑らかに変化する幅。
   float radius_ = 0.8f;         ///< 形状または効果範囲の半径。
   float centerX_ = 0.5f;        ///< 画面上の効果中心のX座標（正規化UV）。
   float centerY_ = 0.5f;        ///< 画面上の効果中心のY座標（正規化UV）。
   float vignetteColorR_ = 0.0f; ///< 画面周辺へ重ねるビネット色の赤成分。
   float vignetteColorG_ = 0.0f; ///< 画面周辺へ重ねるビネット色の緑成分。
   float vignetteColorB_ = 0.0f; ///< 画面周辺へ重ねるビネット色の青成分。

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_; ///< この処理の設定をGPUへ渡す所有定数バッファ。
   VignetteCB* constantBufferData_ = nullptr; ///< 定数バッファの永続Map先。対応するリソースがメモリを所有する。

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}
