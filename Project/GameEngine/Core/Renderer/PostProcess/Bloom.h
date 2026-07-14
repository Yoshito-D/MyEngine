#pragma once
#include "PostProcess.h"
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {
/// @brief ブルーム効果
class Bloom : public PostProcess {
public:
	/// @brief ブルーム用パラメータ構造体
	struct BloomCB {
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
	float threshold_ = 1.0f;        // 通常の白を超えるHDR発光を中心に抽出する
	float softThreshold_ = 0.5f;    // 閾値境界だけを滑らかにし、通常色へのにじみを抑える
	float intensity_ = 1.0f;        // HDR輝度差を保ちながら周辺光を加算する
	float blurRadius_ = 4.0f;       // パーティクル輪郭の外側まで発光を広げる

	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	BloomCB* constantBufferData_ = nullptr;

	void CreateConstantBuffer();
	void UpdateConstantBuffer();
};
}
