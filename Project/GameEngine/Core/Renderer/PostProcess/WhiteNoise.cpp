#include "pch.h"
#include "WhiteNoise.h"
#include "ResourceHelper.h"
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace {
constexpr float kTimeMin = 0.0f;
constexpr float kNoiseDensityMin = 1.0f;
constexpr float kNoiseDensityMax = 2048.0f;
constexpr float kSeedChangeRateMin = 0.0f;
constexpr float kSeedChangeRateMax = 120.0f;
constexpr float kNormalizedParameterMin = 0.0f;
constexpr float kNormalizedParameterMax = 1.0f;

// API・シーン復元・エディターのどの経路から値が入っても、有限な入力値をシェーダーが想定する
// 密度／更新頻度／正規化値の範囲へ同じ規則で収める。
GameEngine::WhiteNoiseParams NormalizeWhiteNoiseParams(const GameEngine::WhiteNoiseParams& source) {
   GameEngine::WhiteNoiseParams params = source;
   params.time = std::max(params.time, kTimeMin);
   params.noiseDensity = std::clamp(params.noiseDensity, kNoiseDensityMin, kNoiseDensityMax);
   params.seedChangeRate = std::clamp(params.seedChangeRate, kSeedChangeRateMin, kSeedChangeRateMax);
   params.noiseThreshold = std::clamp(params.noiseThreshold, kNormalizedParameterMin, kNormalizedParameterMax);
   params.noiseIntensity = std::clamp(params.noiseIntensity, kNormalizedParameterMin, kNormalizedParameterMax);
   return params;
}
}

namespace GameEngine {

/// @brief ホワイトノイズ用の共有描画環境とGPU定数を初期化する。
/// @details 基底クラスへデバイスを登録してからUPLOADバッファを作成する。現在時刻を最初の基準点にすることで、
///          初回描画時にエフェクト生成前の経過時間をノイズシードへ加算しない。
/// @param device 定数バッファの作成と描画コマンドの記録に使用するデバイス
/// @param renderTarget ポストプロセス結果を書き込むオフスクリーンターゲット
void WhiteNoise::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   previousTime_ = std::chrono::steady_clock::now();
   hasPreviousTime_ = true;
   UpdateConstantBuffer();
}

/// @brief 入力カラーへ時間変化するセル状ノイズを1パスで適用する。
/// @details 無効時またはPSO未設定時は時間も進めない。描画する場合だけ経過時間と定数を同期し、
///          ポストプロセススタックが選択した反対側のターゲットへフルスクリーン描画する。
/// @param inputSRV 直前のポストプロセス結果、またはシーンカラーのSRV
void WhiteNoise::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!enabled_) return;
   if (!pipeline_ || !rootSignature_) return;

   // シェーダーはfloor(time * seedChangeRate)を乱数シードに使うため、定数転送前に時刻を更新する。
   AdvanceTime();
   UpdateConstantBuffer();

   // 色だけを扱う後処理なのでDSVを束縛せず、深度リソースとの不要な競合を避ける。
   renderTarget_->PreDraw(false);

   auto cmdList = device_->GetCommandList();

   cmdList->SetPipelineState(pipeline_->GetPipelineState());
   cmdList->SetGraphicsRootSignature(rootSignature_->GetRootSignature());

   // 物理スロット番号はルートシグネチャ定義の意味名から解決済みであり、並び順を固定値で仮定しない。
   if (constantBuffer_) {
	  cmdList->SetGraphicsRootConstantBufferView(GetConstantBufferRootSlot(), constantBuffer_->GetGPUVirtualAddress());
   }

   cmdList->SetGraphicsRootDescriptorTable(GetInputTextureRootSlot(), inputSRV);

   // 頂点バッファ不要のフルスクリーントライアングルで、入力画像全体を一度だけ評価する。
   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);

   // 後続エフェクトが今回の出力をSRVとして読める状態へ遷移する。
   renderTarget_->PostDraw();
}

#ifdef USE_IMGUI
/// @brief ホワイトノイズの実行時パラメーターをエディターへ表示する。
/// @details 編集中は一時コピーを更新し、変更があったフレームだけSetParamsを通すことで、
///          範囲正規化とGPU定数の反映を一度にまとめる。
void WhiteNoise::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_None, "%s",
      LocalizeEditorText("ホワイトノイズのパラメータ", "White Noise Parameters"))) {
	  bool changed = false;
	  WhiteNoiseParams params = params_;
	  changed |= ImGui::DragFloat(LocalizeEditorText("時間", "Time"), &params.time, 0.01f, kTimeMin);
	  changed |= ImGui::SliderFloat(
		 LocalizeEditorText("ノイズ密度", "Noise Density"),
		 &params.noiseDensity,
		 kNoiseDensityMin,
		 kNoiseDensityMax);
	  changed |= ImGui::SliderFloat(
		 LocalizeEditorText("シード更新頻度", "Seed Change Rate"),
		 &params.seedChangeRate,
		 kSeedChangeRateMin,
		 kSeedChangeRateMax);
	  changed |= ImGui::SliderFloat(
		 LocalizeEditorText("ノイズしきい値", "Noise Threshold"),
		 &params.noiseThreshold,
		 kNormalizedParameterMin,
		 kNormalizedParameterMax);
	  changed |= ImGui::SliderFloat(
		 LocalizeEditorText("ノイズ強度", "Noise Intensity"),
		 &params.noiseIntensity,
		 kNormalizedParameterMin,
		 kNormalizedParameterMax);

	  if (changed) {
		 SetParams(params);
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

/// @brief ホワイトノイズの全パラメーターを検証して反映する。
/// @details 有限な入力値について、時刻は0以上、密度は1～2048、シード更新頻度は0～120、
///          しきい値と強度は0～1へ制限し、
///          CPU保持値と永続Map済み定数バッファを同時に更新する。
/// @param params 反映する実行時パラメーター
void WhiteNoise::SetParams(const WhiteNoiseParams& params) {
   params_ = NormalizeWhiteNoiseParams(params);
   UpdateConstantBuffer();
}

/// @brief ノイズパターン生成に使用する時刻を設定する。
/// @details SetParamsを経由するため負の値は0へ補正される。次回の自動更新はこの値へ実時間差分を加算する。
/// @param time シェーダーへ渡す時刻（秒）
void WhiteNoise::SetTime(float time) {
   auto params = params_;
   params.time = time;
   SetParams(params);
}

/// @brief CPUから毎フレーム更新できるホワイトノイズ定数バッファを作成する。
/// @details ResourceHelperのUPLOADヒープを一度だけMapし、以後はconstantBufferData_を介して直接更新する。
void WhiteNoise::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(WhiteNoiseCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

/// @brief 現在のCPUパラメーターをHLSLのWhiteNoiseCBレイアウトへ転送する。
/// @details 末尾パディングも明示的に初期化し、定数バッファレジスター境界の未定義値を残さない。
void WhiteNoise::UpdateConstantBuffer() {
   if (!constantBufferData_) {
	  return;
   }

   constantBufferData_->time = params_.time;
   constantBufferData_->noiseDensity = params_.noiseDensity;
   constantBufferData_->seedChangeRate = params_.seedChangeRate;
   constantBufferData_->noiseThreshold = params_.noiseThreshold;
   constantBufferData_->noiseIntensity = params_.noiseIntensity;
   constantBufferData_->padding[0] = 0.0f;
   constantBufferData_->padding[1] = 0.0f;
   constantBufferData_->padding[2] = 0.0f;
}

/// @brief 単調増加時計の差分からノイズ用時刻を進める。
/// @details システム時刻の変更に影響されないsteady_clockを使用する。初回は基準時刻だけを保存し、
///          休止や処理落ち後の大きな差分は0.1秒へ制限してノイズパターンの急変を抑える。
void WhiteNoise::AdvanceTime() {
   const auto now = std::chrono::steady_clock::now();
   if (!hasPreviousTime_) {
	  previousTime_ = now;
	  hasPreviousTime_ = true;
	  return;
   }

   const std::chrono::duration<float> delta = now - previousTime_;
   previousTime_ = now;

   // 負の差分も0へ収め、手動設定したparams_.timeへ安全な増分だけを加える。
   params_.time += std::clamp(delta.count(), 0.0f, 0.1f);
}

}
