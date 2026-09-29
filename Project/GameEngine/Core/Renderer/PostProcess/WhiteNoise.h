#pragma once
#include "PostProcess.h"
#include <chrono>
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {
/// @brief アニメーションするホワイトノイズの実行時パラメーター。
struct WhiteNoiseParams {
   /// @brief シェーダーの乱数シードに使用する時間。
   float time = 0.0f;

   /// @brief UV空間に配置する手続き的ノイズセルの数。
   float noiseDensity = 320.0f;

   /// @brief 1秒あたりのランダムパターン変更回数。
   float seedChangeRate = 24.0f;

   /// @brief セルをノイズ化するか決める乱数のしきい値。
   float noiseThreshold = 0.94f;

   /// @brief 選択されたノイズセルの強度倍率。
   float noiseIntensity = 0.85f;
};

/// @brief オフスクリーンテクスチャへアニメーションするホワイトノイズを乗算する。
class WhiteNoise : public PostProcess {
public:
   /// @brief ホワイトノイズ用定数バッファーのレイアウト。
   struct WhiteNoiseCB {
	  /// @brief シェーダーの乱数シードに使用する時間。
	  float time;

	  /// @brief UV空間に配置する手続き的ノイズセルの数。
	  float noiseDensity;

	  /// @brief 1秒あたりのランダムパターン変更回数。
	  float seedChangeRate;

	  /// @brief セルをノイズ化するか決める乱数のしきい値。
	  float noiseThreshold;

	  /// @brief 選択されたノイズセルの強度倍率。
	  float noiseIntensity;

	  /// @brief 定数バッファーのレジスターアライメント用パディング。
	  float padding[3];
   };

   /// @brief ホワイトノイズエフェクトで使用するGPUリソースを初期化する。
   /// @param device グラフィックスデバイス。
   /// @param renderTarget オフスクリーンレンダーターゲット。
   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;

   /// @brief 指定された入力テクスチャへエフェクトを適用する。
   /// @param inputSRV 入力テクスチャのSRV。
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   /// @brief エフェクトパラメーターのデバッグ操作を表示する。
   void ImGuiEdit() override;
#endif

   /// @brief エディター・デバッグ表示名を取得する。
   /// @return エフェクト名。
   const char* GetEffectName() const override { return "White Noise"; }

   /// @copydoc PostProcess::SerializeSettings
   nlohmann::json SerializeSettings() const override;

   /// @copydoc PostProcess::DeserializeSettings
   bool DeserializeSettings(const nlohmann::json& settings) override;

   /// @brief ホワイトノイズの全パラメーターを設定する。
   /// @param params 適用するパラメーター。
   void SetParams(const WhiteNoiseParams& params);

   /// @brief 現在のホワイトノイズパラメーターを取得する。
   /// @return 現在のパラメーター。
   const WhiteNoiseParams& GetParams() const { return params_; }

   /// @brief ホワイトノイズシェーダーへ渡す経過時間を設定する。
   /// @param time 秒単位の時間。
   void SetTime(float time);

   /// @brief ホワイトノイズシェーダーへ渡している経過時間を取得する。
   /// @return 秒単位の時間。
   float GetTime() const { return params_.time; }

   /// @brief 手続き的ノイズの密度を設定する。
   /// @param value UV空間に配置するノイズセル数。
   void SetNoiseDensity(float value) { auto params = params_; params.noiseDensity = value; SetParams(params); }

   /// @brief シードの変更頻度を設定する。
   /// @param value 1秒あたりのランダムパターン変更回数。
   void SetSeedChangeRate(float value) { auto params = params_; params.seedChangeRate = value; SetParams(params); }

   /// @brief ノイズセルの選択に使用するしきい値を設定する。
   /// @param value 0.0～1.0のしきい値。
   void SetNoiseThreshold(float value) { auto params = params_; params.noiseThreshold = value; SetParams(params); }

   /// @brief ノイズ強度を設定する。
   /// @param value 0.0～1.0の強度倍率。
   void SetNoiseIntensity(float value) { auto params = params_; params.noiseIntensity = value; SetParams(params); }

private:
   WhiteNoiseParams params_;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   WhiteNoiseCB* constantBufferData_ = nullptr;
   std::chrono::steady_clock::time_point previousTime_;
   bool hasPreviousTime_ = false;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
   void AdvanceTime();
};
}
