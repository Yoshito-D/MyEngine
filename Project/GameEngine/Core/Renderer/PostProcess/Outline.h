#pragma once
#include "PostProcess.h"
#include <d3d12.h>
#include <wrl.h>

namespace GameEngine {
/// @brief 深度バッファを使ったアウトライン効果
class Outline : public PostProcess {
public:
   /// @brief HLSLのOutlineParamsと同じ定数バッファレイアウト
   struct OutlineCB {
	  float outlineColor[4]; ///< 輪郭のRGB色と寄与率として使うアルファ
	  float texelSize[2];    ///< 出力解像度における1テクセルのUV幅
	  float thickness;       ///< 深度近傍のサンプリング間隔倍率
	  float depthThreshold;  ///< 深度勾配を輪郭強度へ正規化するしきい値
	  float intensity;       ///< 輪郭全体の合成強度
	  float padding[3];      ///< 定数バッファレジスター境界へ合わせるパディング
   };

   /// @brief 共有描画環境とアウトライン用定数バッファを初期化する。
   /// @param device グラフィックスデバイス
   /// @param renderTarget ポストプロセスの出力先
   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;

   /// @brief 入力カラーとシーン深度から輪郭を抽出して合成する。
   /// @param inputSRV 入力カラーのSRV
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   /// @brief アウトライン設定のエディターUIを描画する。
   void ImGuiEdit() override;
#endif

   /// @brief エディター表示と保存識別に使用するエフェクト名を取得する。
   /// @return エフェクト名
   const char* GetEffectName() const override { return "Outline"; }

   /// @copydoc PostProcess::SerializeSettings
   nlohmann::json SerializeSettings() const override;
   /// @copydoc PostProcess::DeserializeSettings
   bool DeserializeSettings(const nlohmann::json& settings) override;

   /// @brief 輪郭色と強度へ乗算されるアルファを設定する。
   /// @param r 赤成分
   /// @param g 緑成分
   /// @param b 青成分
   /// @param a 輪郭の寄与率
   void SetOutlineColor(float r, float g, float b, float a = 1.0f);

   /// @brief 深度近傍のサンプリング間隔倍率を設定する。
   /// @param thickness 1テクセル幅へ乗算する倍率
   void SetThickness(float thickness);

   /// @brief 深度勾配を輪郭強度へ変換するしきい値を設定する。
   /// @param threshold 深度勾配のしきい値
   void SetDepthThreshold(float threshold);

   /// @brief 輪郭全体の合成強度を設定する。
   /// @param intensity 強度係数。シェーダー側で0～1へ飽和される
   void SetIntensity(float intensity);

private:
   float outlineColor_[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
   float thickness_ = 1.4f;
   float depthThreshold_ = 0.6f;
   float intensity_ = 0.7f;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   OutlineCB* constantBufferData_ = nullptr;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}
