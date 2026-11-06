#pragma once

#include <cstdint>
#include "GameEngine/Graphics/PostProcess/PostProcess.h"
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {

/// @brief ボックスフィルター効果
class BoxFilter : public PostProcess {
public:
   /// @brief ボックスフィルター用パラメータ構造体
   struct BoxFilterCB {
      int32_t kernelRadius; // 1=3x3, 2=5x5, 3=7x7
      float padding[3];     ///< GPU転送データの配置境界を揃える未使用領域。
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

   /// @brief ボックスフィルターのカーネル半径をシーン保存用JSONへ変換する。
   /// @return 当該エフェクト固有の設定を含むJSONオブジェクト。
   nlohmann::json SerializeSettings() const override;

   /// @brief ボックスフィルターのカーネル半径を部分設定JSONから復元し、GPU定数へ反映する。
   /// @param settings エフェクトの設定JSONオブジェクト。未指定・型不正の項目は現在値を使う。
   /// @return JSONオブジェクトを受け付けた場合はtrue。それ以外はfalse。
   bool DeserializeSettings(const nlohmann::json& settings) override;

   /// @brief カーネル半径を設定（1=3x3, 2=5x5, 3=7x7）
   void SetKernelRadius(std::int32_t radius);

private:
   int32_t kernelRadius_ = 1; // デフォルト3x3

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_; ///< この処理の設定をGPUへ渡す所有定数バッファ。
   BoxFilterCB* constantBufferData_ = nullptr; ///< 定数バッファの永続Map先。対応するリソースがメモリを所有する。

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}
