#pragma once
#include "PostProcess.h"
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {
/// @brief ボックスフィルター効果
class BoxFilter : public PostProcess {
public:
   /// @brief ボックスフィルター用パラメータ構造体
   struct BoxFilterParams {
      int32_t kernelRadius; // 1=3x3, 2=5x5, 3=7x7
      float padding0;
      float padding1;
      float padding2;
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
   const char* GetEffectName() const override { return "BoxFilter"; }

   /// @brief カーネル半径を設定（1=3x3, 2=5x5, 3=7x7）
   void SetKernelRadius(int radius) { kernelRadius_ = radius; UpdateConstantBuffer(); }

private:
   int kernelRadius_ = 1; // デフォルト3x3

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   BoxFilterParams* constantBufferData_ = nullptr;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}
