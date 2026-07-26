#pragma once
#include <d3d12.h>
#include <memory>
#include "Graphics/GraphicsDevice.h"
#include "Graphics/OffscreenRenderTarget.h"
#include "Graphics/PipelineState.h"
#include "Graphics/RootSignature.h"
#include <nlohmann/json_fwd.hpp>

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

   /// @brief 入力SRV/定数バッファのルートパラメータスロットを設定
   void SetBindingSlots(UINT constantBufferSlot, UINT inputTextureSlot);

   /// @brief 深度SRVのルートパラメータスロットを設定
   void SetDepthTextureRootSlot(UINT depthTextureSlot);

   /// @brief マスクSRVのルートパラメータスロットを設定
   void SetMaskTextureRootSlot(UINT maskTextureSlot);

#ifdef USE_IMGUI
   /// @brief エフェクト固有のImGui編集項目を描画する
   virtual void ImGuiEdit() {}
#endif

   /// @brief エフェクトの有効状態を設定する
   virtual void SetEnabled(bool enabled) { enabled_ = enabled; }
   /// @brief エフェクトが有効か取得する
   virtual bool IsEnabled() const { return enabled_; }

   /// @brief エディター表示と保存識別に使用するエフェクト名を取得する
   virtual const char* GetEffectName() const { return "Unknown"; }

   /// @brief シーンへ保存するエフェクト固有設定を取得する
   /// @return エフェクト固有設定を格納したJSONオブジェクト
   virtual nlohmann::json SerializeSettings() const;

   /// @brief シーンからエフェクト固有設定を復元する
   /// @param settings エフェクト固有設定を格納したJSONオブジェクト
   /// @return 設定を適用できた場合はtrue
   virtual bool DeserializeSettings(const nlohmann::json& settings);

protected:
   GraphicsDevice* device_ = nullptr;
   OffscreenRenderTarget* renderTarget_ = nullptr;

   // パイプラインは外部から設定される
   PipelineState* pipeline_ = nullptr;
   RootSignature* rootSignature_ = nullptr;

   bool enabled_ = true;
   UINT constantBufferRootSlot_ = 0;
   UINT inputTextureRootSlot_ = 1;
   UINT depthTextureRootSlot_ = 2;
   UINT maskTextureRootSlot_ = 2;

   UINT GetConstantBufferRootSlot() const { return constantBufferRootSlot_; }
   UINT GetInputTextureRootSlot() const { return inputTextureRootSlot_; }
   UINT GetDepthTextureRootSlot() const { return depthTextureRootSlot_; }
   UINT GetMaskTextureRootSlot() const { return maskTextureRootSlot_; }

   // ImGuiの固有ID管理
#ifdef USE_IMGUI
		// 各エフェクトインスタンス用の固有IDを生成
   const void* GetImGuiID() const { return static_cast<const void*>(this); }

   /// @brief エディタの現在言語に対応する表示文字列を取得する
   /// @param japanese 日本語表示
   /// @param english 英語表示
   /// @return 現在のエディタ言語に対応する文字列
   static const char* LocalizeEditorText(const char* japanese, const char* english);
#endif
};
}
