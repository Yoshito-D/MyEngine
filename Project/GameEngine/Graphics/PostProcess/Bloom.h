#pragma once

#include "GameEngine/Graphics/PostProcess/PostProcess.h"
#include <wrl.h>
#include <d3d12.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace GameEngine {

/// @brief ブルーム効果
class Bloom : public PostProcess {
public:
   /// @brief 初期化（パイプラインは外部から設定される）
   /// @param device グラフィックスデバイス
   /// @param renderTarget レンダーターゲット
   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;

   /// @brief エフェクトを適用
   /// @param inputSRV 入力SRV
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   /// @brief ブルームのしきい値・強度・ぼかし半径を編集する。
   void ImGuiEdit() override;
#endif

   /// @brief エフェクトの表示名を取得する。
   const char* GetEffectName() const override { return "Bloom"; }

   /// @brief 輝度しきい値・境界の柔らかさ・合成強度・ぼかし半径をシーン保存用JSONへ変換する。
   /// @return 当該エフェクト固有の設定を含むJSONオブジェクト。
   nlohmann::json SerializeSettings() const override;

   /// @brief 輝度しきい値・境界の柔らかさ・合成強度・ぼかし半径を部分設定JSONから復元し、GPU定数へ反映する。
   /// @param settings エフェクトの設定JSONオブジェクト。未指定・型不正の項目は現在値を使う。
   /// @return JSONオブジェクトを受け付けた場合はtrue。それ以外はfalse。
   bool DeserializeSettings(const nlohmann::json& settings) override;

   /// @brief 発光を抽出する輝度しきい値を設定する。
   void SetThreshold(float threshold);

   /// @brief 周囲へ加算する発光の強度を設定する。
   void SetIntensity(float intensity);

   /// @brief ぼかしの広がりを設定する（ピクセル基準、0～10）。
   void SetBlurRadius(float radius);

   /// @brief 輝度しきい値の境界の柔らかさを設定する（0～1）。
   void SetSoftThreshold(float softThreshold);

private:
   // Bloom.PS.hlslのパス番号・重み配列と一致させる。
   enum class Pass : std::uint32_t {
      Copy,
      Horizontal,
      Composite,
      Count
   };
   static constexpr float kMaxBlurRadius = 10.0f;
   static constexpr float kSigmaPerBlurRadius = 1.5f;
   static constexpr float kKernelSupportSigma = 3.0f;
   static constexpr std::int32_t kMaxKernelRadius =
      static_cast<std::int32_t>(kMaxBlurRadius * kSigmaPerBlurRadius * kKernelSupportSigma);
   static constexpr size_t kWeightsPerVector = 4;
   static constexpr size_t kWeightVectorCount = (kMaxKernelRadius + kWeightsPerVector) / kWeightsPerVector;

   struct BloomCB {
      float threshold;
      float intensity;
      float softThreshold;
      std::int32_t kernelRadius;
      std::uint32_t pass;
      std::array<float, 3> padding{};
      std::array<std::array<float, kWeightsPerVector>, kWeightVectorCount> weights{};
   };

   // D3D12の定数バッファは開始位置を256バイト境界に揃える。
   // 構造体の配置は変えず、パスごとに別領域を持って記録済みコマンドの定数を上書きしない。
   static constexpr size_t kConstantBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
   static constexpr size_t kConstantBufferStride =
      (sizeof(BloomCB) + kConstantBufferAlignment - 1) / kConstantBufferAlignment * kConstantBufferAlignment;

   float threshold_ = 1.0f;     // 通常の白を超えるHDR発光を中心に抽出する
   float softThreshold_ = 0.5f; // 閾値境界だけを滑らかにし、通常色へのにじみを抑える
   float intensity_ = 1.0f;     // HDR輝度差を保ちながら周辺光を加算する
   float blurRadius_ = 4.0f;    // パーティクル輪郭の外側まで発光を広げる

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_; ///< この処理の設定をGPUへ渡す所有定数バッファ。
   std::byte* constantBufferData_ = nullptr; ///< 定数バッファの永続Map先。対応するリソースがメモリを所有する。

   Microsoft::WRL::ComPtr<ID3D12Resource> blurTexture_;         ///< 横方向の発光ぼかしを保存するHDR中間画像。
   Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> blurRtvHeap_;   ///< シーンのRTVを上書きしない専用ヒープ。
   std::shared_ptr<SrvDescriptorAllocation> blurSrvAllocation_; ///< 共有SRVヒープ内の中間画像スロットの所有権。
   D3D12_GPU_DESCRIPTOR_HANDLE blurSrvHandle_{};                ///< 中間画像のGPU参照ハンドル。

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
   bool EnsureBlurTarget();
   void DrawPass(Pass pass, D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
};
}
