#pragma once
#include "PostProcess.h"
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {
/// @brief ブルーム効果
class Bloom : public PostProcess {
public:
	/// @brief ブルーム用パラメータ構造体
	struct BloomParams {
		float threshold;      // 輝度閾値
		float intensity;      // ブルーム強度
		float blurRadius;     // ブラー半径
		float softThreshold;  // ソフト閾値（閾値付近の滑らかさ）
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
	const char* GetEffectName() const override { return "Bloom"; }

	// パラメータ設定
	void SetThreshold(float threshold) { threshold_ = threshold; UpdateConstantBuffer(); }
	void SetIntensity(float intensity) { intensity_ = intensity; UpdateConstantBuffer(); }
	void SetBlurRadius(float radius) { blurRadius_ = radius; UpdateConstantBuffer(); }
	void SetSoftThreshold(float softThreshold) { softThreshold_ = softThreshold; UpdateConstantBuffer(); }

private:
	float threshold_ = 0.6f;        // 輝度閾値（この値以上の明るい部分がブルームする）
	float softThreshold_ = 1.0f;    // ソフト閾値（0.0 = 急激、1.0 = 非常に滑らか）
	float intensity_ = 2.0f;        // ブルーム強度
	float blurRadius_ = 3.0f;       // ブラー半径

	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	BloomParams* constantBufferData_ = nullptr;

	void CreateConstantBuffer();
	void UpdateConstantBuffer();
};
}
