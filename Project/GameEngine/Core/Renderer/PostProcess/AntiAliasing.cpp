#include "pch.h"
#include "AntiAliasing.h"
#include "ResourceHelper.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

/// @brief 輝度ベースのアンチエイリアシングに必要なGPU定数を初期化する。
/// @details 基底クラスへ描画環境を登録してからUPLOADバッファを作成し、既定パラメーターを転送する。
///          この順序によりCreateConstantBufferは有効なGraphicsDeviceを常に参照できる。
/// @param device 定数バッファの作成と描画コマンドの記録に使用するデバイス
/// @param renderTarget ポストプロセス結果を書き込むオフスクリーンターゲット
void AntiAliasing::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

/// @brief 入力画像の輝度差からエッジ方向を推定し、方向性フィルターを適用する。
/// @details 汎用PostProcessルートシグネチャのCBVと入力SRVを解決済みスロットへ束縛し、
///          頂点入力を持たない1回のフルスクリーンパスでFXAA系の平滑化を行う。
/// @param inputSRV 直前のポストプロセス結果、またはシーンカラーのSRV
void AntiAliasing::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!enabled_) return;
   if (!pipeline_ || !rootSignature_) return;

   UpdateConstantBuffer();

   // 深度を使わない色フィルターなのでDSVを束縛せず、スタックのping-pong出力面だけを更新する。
   renderTarget_->PreDraw(false);

   auto cmdList = device_->GetCommandList();

   cmdList->SetPipelineState(pipeline_->GetPipelineState());
   cmdList->SetGraphicsRootSignature(rootSignature_->GetRootSignature());

   // ルートパラメータの物理番号はPSO定義から解決されるため、b0/t0の並びをC++側で固定しない。
   if (constantBuffer_) {
	  cmdList->SetGraphicsRootConstantBufferView(GetConstantBufferRootSlot(), constantBuffer_->GetGPUVirtualAddress());
   }

   cmdList->SetGraphicsRootDescriptorTable(GetInputTextureRootSlot(), inputSRV);

   // 頂点バッファを持たないフルスクリーントライアングルで画面端の継ぎ目を避ける。
   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);

   // 次のポストプロセスが今回の出力を入力SRVとして使用できる状態へ戻す。
   renderTarget_->PostDraw();
}

/// @brief CPUから直接更新するアンチエイリアシング定数バッファを作成し、永続Mapする。
void AntiAliasing::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(AntiAliasingCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

/// @brief 現在のエッジ判定・平滑化パラメーターをHLSLのAntiAliasingParamsへ転送する。
/// @details メンバーの並びをシェーダー側の4つのfloatと一致させ、1レジスター分を一括して更新する。
void AntiAliasing::UpdateConstantBuffer() {
   if (!constantBufferData_) {
	  return;
   }

   constantBufferData_->contrastThreshold = contrastThreshold_;
   constantBufferData_->relativeThreshold = relativeThreshold_;
   constantBufferData_->subpixelBlending = subpixelBlending_;
   constantBufferData_->edgeSearchSteps = edgeSearchSteps_;
}

#ifdef USE_IMGUI
/// @brief アンチエイリアシングの検出感度と平滑化量をエディターへ表示する。
/// @details UI範囲はシェーダーで有効な正値・正規化値へ制限し、複数項目の変更後に定数を一度だけ転送する。
void AntiAliasing::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_None, "%s",
      LocalizeEditorText("アンチエイリアシングのパラメータ", "Anti Aliasing Parameters"))) {
	  bool changed = false;
	  changed |= ImGui::SliderFloat(LocalizeEditorText("コントラストしきい値", "Contrast Threshold"), &contrastThreshold_, 0.001f, 0.2f, "%.4f");
	  changed |= ImGui::SliderFloat(LocalizeEditorText("相対しきい値", "Relative Threshold"), &relativeThreshold_, 0.0312f, 0.333f, "%.4f");
	  changed |= ImGui::SliderFloat(LocalizeEditorText("サブピクセルブレンド", "Subpixel Blending"), &subpixelBlending_, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("エッジ探索ステップ数", "Edge Search Steps"), &edgeSearchSteps_, 1.0f, 16.0f, "%.0f");

	  if (changed) {
		 UpdateConstantBuffer();
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}
