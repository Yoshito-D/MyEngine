#include "pch.h"
#include "Outline.h"
#include "ResourceHelper.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

/// @brief 深度輪郭抽出に必要な共有描画環境とGPU定数を初期化する。
/// @details 基底初期化でデバイスと出力先を保持した後にUPLOADバッファを作成する。初期値も直ちに転送し、
///          パイプライン設定後の最初のApplyから有効な定数を参照できる状態にする。
/// @param device 定数バッファの作成、深度SRVの取得、描画コマンドの記録に使用するデバイス
/// @param renderTarget ポストプロセス結果を書き込むオフスクリーンターゲット
void Outline::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

/// @brief シーン深度の不連続を検出し、入力カラーへアウトラインを合成する。
/// @details 呼び出し元が事前に深度バッファをSRV状態へ遷移していることを前提とする。色入力、深度入力、
///          定数バッファをそれぞれ意味名から解決済みのスロットへ束縛し、1回のフルスクリーンパスで処理する。
/// @param inputSRV 直前のポストプロセス結果、またはシーンカラーのSRV
void Outline::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!enabled_) return;
   if (!pipeline_ || !rootSignature_) return;

   const D3D12_GPU_DESCRIPTOR_HANDLE depthSRV = device_->GetDepthSRVHandleGPU();
   // Prewittフィルターには深度近傍値が必須なため、無効ハンドルで色だけを描くフォールバックは行わない。
   if (depthSRV.ptr == 0) {
	  return;
   }

   // リサイズで1テクセル幅が変化し得るため、描画直前にrenderTarget_の現在サイズから定数を再計算する。
   UpdateConstantBuffer();

   // 深度はSRVとして読むためDSVを同時束縛せず、色出力だけをping-pong先へ書き込む。
   renderTarget_->PreDraw(false);

   auto cmdList = device_->GetCommandList();

   cmdList->SetPipelineState(pipeline_->GetPipelineState());
   cmdList->SetGraphicsRootSignature(rootSignature_->GetRootSignature());

   // Outline専用ルートシグネチャのCBV・カラーSRV・深度SRVは、物理番号ではなく解決済みスロットを使う。
   if (constantBuffer_) {
	  cmdList->SetGraphicsRootConstantBufferView(GetConstantBufferRootSlot(), constantBuffer_->GetGPUVirtualAddress());
   }

   cmdList->SetGraphicsRootDescriptorTable(GetInputTextureRootSlot(), inputSRV);
   cmdList->SetGraphicsRootDescriptorTable(GetDepthTextureRootSlot(), depthSRV);

   // 頂点入力のないフルスクリーントライアングルで全ピクセルの3x3深度近傍を評価する。
   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);

   renderTarget_->PostDraw();
}

/// @brief 輪郭の合成色を設定してGPU定数へ即時反映する。
/// @details アルファ成分はシェーダー内で輪郭強度へ乗算され、RGBは検出した輪郭との補間色に使われる。
/// @param r 輪郭色の赤成分
/// @param g 輪郭色の緑成分
/// @param b 輪郭色の青成分
/// @param a 輪郭寄与率として使用するアルファ成分
void Outline::SetOutlineColor(float r, float g, float b, float a) {
   outlineColor_[0] = r;
   outlineColor_[1] = g;
   outlineColor_[2] = b;
   outlineColor_[3] = a;
   UpdateConstantBuffer();
}

/// @brief 深度サンプリング間隔の倍率を設定する。
/// @details シェーダーでは1テクセル幅へこの値を乗算する。APIでは値を制限せず、エディターでは0.5～5.0を提示する。
/// @param thickness 深度近傍を採取するピクセル間隔の倍率
void Outline::SetThickness(float thickness) {
   thickness_ = thickness;
   UpdateConstantBuffer();
}

/// @brief 深度勾配を輪郭強度へ変換するしきい値を設定する。
/// @details 小さいほど弱い深度差も強い輪郭になる。シェーダー側は除算下限を設けてゼロ除算を防ぐ。
/// @param threshold 深度勾配の正規化に使うしきい値
void Outline::SetDepthThreshold(float threshold) {
   depthThreshold_ = threshold;
   UpdateConstantBuffer();
}

/// @brief 検出した輪郭全体の合成強度を設定する。
/// @details シェーダーで0～1へ飽和されるが、CPU側では値を変更せず保持する。
/// @param intensity 輪郭の強度係数
void Outline::SetIntensity(float intensity) {
   intensity_ = intensity;
   UpdateConstantBuffer();
}

/// @brief CPUから直接更新するOutline定数バッファを作成し、永続Mapする。
void Outline::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(OutlineCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

/// @brief CPU保持パラメーターと現在の出力解像度をHLSLのOutlineParamsへ転送する。
/// @details 1テクセルのUV幅を解像度から算出することで、thicknessを画面サイズに依存しない
///          ピクセル単位の近傍間隔として扱える。ゼロ寸法では除算せず0を渡す。
void Outline::UpdateConstantBuffer() {
   if (!constantBufferData_ || !renderTarget_) {
	  return;
   }

   constantBufferData_->outlineColor[0] = outlineColor_[0];
   constantBufferData_->outlineColor[1] = outlineColor_[1];
   constantBufferData_->outlineColor[2] = outlineColor_[2];
   constantBufferData_->outlineColor[3] = outlineColor_[3];

   const float width = static_cast<float>(renderTarget_->GetWidth());
   const float height = static_cast<float>(renderTarget_->GetHeight());
   // ピクセル単位の近傍サンプルを解像度に依存させないため、UVの1テクセル幅を渡す。
   constantBufferData_->texelSize[0] = width > 0.0f ? 1.0f / width : 0.0f;
   constantBufferData_->texelSize[1] = height > 0.0f ? 1.0f / height : 0.0f;
   constantBufferData_->thickness = thickness_;
   constantBufferData_->depthThreshold = depthThreshold_;
   constantBufferData_->intensity = intensity_;
   constantBufferData_->padding[0] = 0.0f;
   constantBufferData_->padding[1] = 0.0f;
   constantBufferData_->padding[2] = 0.0f;
}

#ifdef USE_IMGUI
/// @brief アウトラインの色、検出範囲、感度、強度をエディターへ表示する。
/// @details 同一フレームの複数変更をまとめ、値が変わった場合だけ定数バッファを更新する。
void Outline::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_None, "%s",
      LocalizeEditorText("アウトラインのパラメータ", "Outline Parameters"))) {
	  bool changed = false;
	  changed |= ImGui::ColorEdit4(LocalizeEditorText("アウトライン色", "Outline Color"), outlineColor_);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("太さ", "Thickness"), &thickness_, 0.5f, 5.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("深度しきい値", "Depth Threshold"), &depthThreshold_, 0.0001f, 1.0f, "%.4f");
	  changed |= ImGui::SliderFloat(LocalizeEditorText("強度", "Intensity"), &intensity_, 0.0f, 1.0f);

	  if (changed) {
		 UpdateConstantBuffer();
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}
