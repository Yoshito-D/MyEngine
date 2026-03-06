#pragma once
#include <d3d12.h>
#include <memory>
#include "Graphics/GraphicsDevice.h"
#include "Graphics/OffscreenRenderTarget.h"
#include "Graphics/PipelineState.h"
#include "Graphics/RootSignature.h"

namespace GameEngine {
/// @brief ポストプロセスクラス
class PostProcess {
public:
   /// @brief デストラクタ
   virtual ~PostProcess() = default;

   /// @brief 初期化（パイプラインは外部から設定される）
   /// @param device グラフィックスデバイス
   /// @param renderTarget レンダーターゲット
   virtual void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget);

   /// @brief エフェクトを適用
   /// @param inputSRV 入力SRV
   virtual void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) = 0;

   /// @brief パイプラインとルートシグネチャを設定（外部から）
   /// @param pipeline パイプラインステート
   /// @param rootSignature ルートシグネチャ
   void SetPipeline(PipelineState* pipeline, RootSignature* rootSignature);

#ifdef USE_IMGUI
   // ImGui編集用メソッド（派生クラスでオーバーライド可能）
   virtual void ImGuiEdit() {}
#endif

   // エフェクトの有効/無効設定
   virtual void SetEnabled(bool enabled) { enabled_ = enabled; }
   virtual bool IsEnabled() const { return enabled_; }

   // エフェクト名の取得（派生クラスでオーバーライド推奨）
   virtual const char* GetEffectName() const { return "Unknown"; }

protected:
   GraphicsDevice* device_ = nullptr;
   OffscreenRenderTarget* renderTarget_ = nullptr;

   // パイプラインは外部から設定される
   PipelineState* pipeline_ = nullptr;
   RootSignature* rootSignature_ = nullptr;

   bool enabled_ = true;

   // ImGuiの固有ID管理
#ifdef USE_IMGUI
		// 各エフェクトインスタンス用の固有IDを生成
   const void* GetImGuiID() const { return static_cast<const void*>(this); }
#endif
};
}