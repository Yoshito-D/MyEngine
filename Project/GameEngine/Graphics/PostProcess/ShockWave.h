#pragma once

#include "GameEngine/Graphics/PostProcess/PostProcess.h"
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {

/// @brief ショックウェーブ効果
class ShockWave : public PostProcess {
public:
   /// @brief ショックウェーブ用定数バッファ構造体
   struct ShockWaveCB {
      float centerX;            ///< 画面上の効果中心のX座標（正規化UV）。
      float centerY;            ///< 画面上の効果中心のY座標（正規化UV）。
      float aspectRatioX;       ///< 波の距離計算で横方向へ掛けるアスペクト補正倍率。
      float aspectRatioY;       ///< 波の距離計算で縦方向へ掛けるアスペクト補正倍率。
      float waveRadius;         ///< 衝撃波の中心からの現在の半径。
      float waveThickness;      ///< 衝撃波として歪ませるリングの幅。
      float distortionStrength; ///< 背景のサンプリング位置を歪ませる強さ。
      float fadeOutRadius;      ///< 衝撃波が薄くなる距離の基準半径。
      float highlightIntensity; ///< 衝撃波の輪郭を明るくする強さ。
      float padding[3];         ///< GPU転送データの配置境界を揃える未使用領域。
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
   const char* GetEffectName() const override { return "Shock Wave"; }

   /// @copydoc PostProcess::SerializeSettings
   nlohmann::json SerializeSettings() const override;

   /// @copydoc PostProcess::DeserializeSettings
   bool DeserializeSettings(const nlohmann::json& settings) override;

   // パラメータ設定
   void SetCenter(float x, float y);

   void SetStrength(float strength);

   void SetWaveRadius(float radius);

   void SetWaveThickness(float thickness);

private:
   float centerX_ = 0.5f;            ///< 画面上の効果中心のX座標（正規化UV）。
   float centerY_ = 0.5f;            ///< 画面上の効果中心のY座標（正規化UV）。
   float aspectRatioX_ = 1.6f;       ///< 波の距離計算で横方向へ掛けるアスペクト補正倍率。
   float aspectRatioY_ = 0.9f;       ///< 波の距離計算で縦方向へ掛けるアスペクト補正倍率。
   float waveRadius_ = 0.3f;         ///< 衝撃波の中心からの現在の半径。
   float waveThickness_ = 0.1f;      ///< 衝撃波として歪ませるリングの幅。
   float distortionStrength_ = 0.1f; ///< 背景のサンプリング位置を歪ませる強さ。
   float fadeOutRadius_ = 1.0f;      ///< 衝撃波が薄くなる距離の基準半径。
   float highlightIntensity_ = 0.5f; ///< 衝撃波の輪郭を明るくする強さ。

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_; ///< この処理の設定をGPUへ渡す所有定数バッファ。
   ShockWaveCB* constantBufferData_ = nullptr; ///< 定数バッファの永続Map先。対応するリソースがメモリを所有する。

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}
