#include "pch.h"
#include "Dissolve.h"
#include "Framework/EngineContext.h"
#include "Graphics/Texture.h"
#include "ResourceHelper.h"
#include <array>

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace {
// 公開API・シーン復元・エディターの入力を一か所へ集約し、有限な値についてsmoothstepの幅や
// UV倍率がシェーダー内で不正にならない範囲へ揃える。maskOffsetだけはスクロール用途のため制限しない。
GameEngine::DissolveParams NormalizeDissolveParams(const GameEngine::DissolveParams& source) {
   GameEngine::DissolveParams params = source;
   // シェーダーの除算・補間が破綻しない範囲へ、APIとJSON由来の値を一括で正規化する。
   params.threshold = std::clamp(params.threshold, 0.0f, 1.0f);
   params.edgeWidth = std::clamp(params.edgeWidth, 0.0001f, 0.5f);
   params.edgeIntensity = std::clamp(params.edgeIntensity, 0.0f, 4.0f);
   params.maskContrast = std::clamp(params.maskContrast, 0.0f, 4.0f);
   params.maskTiling.x = std::clamp(params.maskTiling.x, 0.01f, 32.0f);
   params.maskTiling.y = std::clamp(params.maskTiling.y, 0.01f, 32.0f);

   for (float& value : params.edgeColor) {
	  value = std::clamp(value, 0.0f, 1.0f);
   }
   for (float& value : params.dissolveColor) {
	  value = std::clamp(value, 0.0f, 1.0f);
   }

   return params;
}
}

namespace GameEngine {

/// @brief ディゾルブ用の共有描画環境とGPU定数を初期化する。
/// @details 基底クラスへデバイスを登録した後にUPLOADバッファを作成し、既定パラメーターを転送する。
///          マスクテクスチャはテクスチャ管理の初期化順へ依存しないよう、この時点では解決しない。
/// @param device 定数バッファの作成、テクスチャ参照、描画コマンドの記録に使用するデバイス
/// @param renderTarget ポストプロセス結果を書き込むオフスクリーンターゲット
void Dissolve::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

/// @brief マスク値としきい値から可視領域・エッジ領域を求め、入力カラーをディゾルブ合成する。
/// @details カラーSRVとマスクSRVを専用ルートシグネチャの別テーブルへ束縛し、Wrapサンプラーによる
///          タイリングを含めて1回のフルスクリーンパスで処理する。
/// @param inputSRV 直前のポストプロセス結果、またはシーンカラーのSRV
void Dissolve::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!enabled_) return;
   if (!pipeline_ || !rootSignature_) return;

   D3D12_GPU_DESCRIPTOR_HANDLE maskSRV = inputSRV;
   // マスク未解決時も有効なSRVを束縛し、ルートテーブルの未設定によるGPUエラーを避ける。
   if (Texture* maskTexture = ResolveMaskTexture()) {
	  maskSRV = maskTexture->GetTextureSrvHandleGPU();
   }

   UpdateConstantBuffer();

   // 深度を使わない色フィルターなのでDSVを束縛せず、スタックのping-pong出力面だけを更新する。
   renderTarget_->PreDraw(false);

   auto cmdList = device_->GetCommandList();

   cmdList->SetPipelineState(pipeline_->GetPipelineState());
   cmdList->SetGraphicsRootSignature(rootSignature_->GetRootSignature());

   // constantbuffer・inputtexture・masktextureの意味名から解決済みの物理スロットへ各リソースを設定する。
   if (constantBuffer_) {
	  cmdList->SetGraphicsRootConstantBufferView(GetConstantBufferRootSlot(), constantBuffer_->GetGPUVirtualAddress());
   }

   cmdList->SetGraphicsRootDescriptorTable(GetInputTextureRootSlot(), inputSRV);
   cmdList->SetGraphicsRootDescriptorTable(GetMaskTextureRootSlot(), maskSRV);

   // 頂点入力不要のフルスクリーントライアングルで、各ピクセルのマスク判定と色補間を一度だけ行う。
   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);

   renderTarget_->PostDraw();
}

/// @brief ディゾルブの全数値パラメーターを正規化して反映する。
/// @details 有限な入力値について、しきい値と色は0～1、エッジ幅は0.0001～0.5、強度とコントラストは0～4、
///          タイリング各軸は0.01～32へ制限し、CPU保持値とGPU定数を同時に更新する。
/// @param params 反映するディゾルブパラメーター
void Dissolve::SetParams(const DissolveParams& params) {
   params_ = NormalizeDissolveParams(params);
   UpdateConstantBuffer();
}

/// @brief ディゾルブの進行しきい値を設定する。
/// @details SetParamsを経由するため0～1へ制限され、0で入力色を全面表示し、1で入力色の可視率を0にする。
/// @param threshold 可視領域と消失領域を分けるマスクしきい値
void Dissolve::SetThreshold(float threshold) {
   DissolveParams params = params_;
   params.threshold = threshold;
   SetParams(params);
}

/// @brief しきい値周辺に生成する遷移エッジの幅を設定する。
/// @details SetParamsを経由して正の下限を保証し、シェーダー内の幅除算がゼロにならないようにする。
/// @param edgeWidth マスク値空間でのエッジ幅
void Dissolve::SetEdgeWidth(float edgeWidth) {
   DissolveParams params = params_;
   params.edgeWidth = edgeWidth;
   SetParams(params);
}

/// @brief 使用する2Dマスクテクスチャの登録名を変更する。
/// @details 非所有キャッシュを破棄して遅延再検索を予約する。同じ名前ではキャッシュを維持し、
///          空名または未登録名の場合はResolveMaskTextureが既定ノイズ名を順に検索する。
/// @param textureName EngineContextへ登録されたテクスチャ名
void Dissolve::SetMaskTextureName(const std::string& textureName) {
   if (maskTextureName_ == textureName) {
	  return;
   }

   maskTextureName_ = textureName;
   maskTexture_ = nullptr;
   maskTextureLookupDirty_ = true;
}

/// @brief CPUから直接更新するディゾルブ定数バッファを作成し、永続Mapする。
void Dissolve::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(DissolveCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

/// @brief 現在のパラメーターをHLSLのDissolveCBと同じ並びでGPUバッファへ転送する。
/// @details Vector2と2組のfloat4を含むレイアウトをフィールド単位でコピーし、SetParams後の値を
///          次の描画から確実に参照できるようにする。
void Dissolve::UpdateConstantBuffer() {
   if (!constantBufferData_) {
	  return;
   }

   constantBufferData_->threshold = params_.threshold;
   constantBufferData_->edgeWidth = params_.edgeWidth;
   constantBufferData_->edgeIntensity = params_.edgeIntensity;
   constantBufferData_->maskContrast = params_.maskContrast;
   constantBufferData_->maskTiling = params_.maskTiling;
   constantBufferData_->maskOffset = params_.maskOffset;

   for (size_t i = 0; i < 4; ++i) {
	  constantBufferData_->edgeColor[i] = params_.edgeColor[i];
	  constantBufferData_->dissolveColor[i] = params_.dissolveColor[i];
   }
}

/// @brief マスク名に対応する2Dテクスチャを必要時だけ検索する。
/// @details 名前変更までは検索結果を再利用する。指定名が見つからない場合は旧データとの互換性を保つため、
///          短縮名、ファイル名、正規リソースIDの順で既定ノイズを探索する。
/// @return 解決したテクスチャへの非所有ポインタ。候補がすべて未登録ならnullptr
Texture* Dissolve::ResolveMaskTexture() {
   if (!maskTextureLookupDirty_) {
	  // 毎フレームの名前検索を避け、名前変更時だけ再解決する。
	  return maskTexture_;
   }

   maskTextureLookupDirty_ = false;
   maskTexture_ = EngineContext::GetTexture(maskTextureName_);
   if (maskTexture_) {
	  return maskTexture_;
   }

   static constexpr std::array<const char*, 3> kDefaultMaskNames = {
	  "noise0",
	  "noise0.png",
	  "engine/textures/postprocess/noise0.png"
   };

   // 旧プロジェクトで使われた短縮名・ファイル名・正規IDの順に既定ノイズを探す。
   for (const char* textureName : kDefaultMaskNames) {
	  maskTexture_ = EngineContext::GetTexture(textureName);
	  if (maskTexture_) {
		 return maskTexture_;
	  }
   }

   return nullptr;
}

#ifdef USE_IMGUI
/// @brief ディゾルブの数値、色、マスクテクスチャをエディターへ表示する。
/// @details 数値編集は一時コピーへまとめてSetParamsで正規化し、マスク候補からTextureCubeを除外する。
///          選択中の2Dテクスチャは縦横比を保った最大128ピクセルのプレビューとして表示する。
void Dissolve::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_None, "%s",
      LocalizeEditorText("ディゾルブのパラメータ", "Dissolve Parameters"))) {
	  bool changed = false;
	  DissolveParams params = params_;

	  changed |= ImGui::SliderFloat(LocalizeEditorText("しきい値", "Threshold"), &params.threshold, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("エッジ幅", "Edge Width"), &params.edgeWidth, 0.0001f, 0.5f, "%.4f");
	  changed |= ImGui::SliderFloat(LocalizeEditorText("エッジ強度", "Edge Intensity"), &params.edgeIntensity, 0.0f, 4.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("マスクコントラスト", "Mask Contrast"), &params.maskContrast, 0.0f, 4.0f);
	  changed |= ImGui::SliderFloat2(LocalizeEditorText("マスクタイリング", "Mask Tiling"), &params.maskTiling.x, 0.01f, 8.0f);
	  changed |= ImGui::DragFloat2(LocalizeEditorText("マスクオフセット", "Mask Offset"), &params.maskOffset.x, 0.005f);
	  changed |= ImGui::ColorEdit4(LocalizeEditorText("エッジ色", "Edge Color"), params.edgeColor);
	  changed |= ImGui::ColorEdit4(LocalizeEditorText("ディゾルブ色", "Dissolve Color"), params.dissolveColor);

      // HLSL側はTexture2Dとして束縛するため、同じSRV一覧に含まれるキューブマップは候補から除外する。
	  const auto textureNames = EngineContext::GetTextureNames();
	  const char* texturePreview = maskTextureName_.empty()
		 ? LocalizeEditorText("なし", "<none>")
		 : maskTextureName_.c_str();
	  if (ImGui::BeginCombo(LocalizeEditorText("マスクテクスチャ", "Mask Texture"), texturePreview)) {
		 for (const auto& textureName : textureNames) {
			Texture* candidate = EngineContext::GetTexture(textureName);
			if (candidate && candidate->GetMetadata().IsCubemap()) {
			   continue;
			}

			const bool selected = (textureName == maskTextureName_);
			if (ImGui::Selectable(textureName.c_str(), selected)) {
			   SetMaskTextureName(textureName);
			}
			if (selected) {
			   ImGui::SetItemDefaultFocus();
			}
		 }
		 ImGui::EndCombo();
	  }

	  if (Texture* previewTexture = ResolveMaskTexture()) {
		 if (previewTexture->GetMetadata().IsCubemap()) {
			ImGui::TextDisabled("%s", LocalizeEditorText(
			   "TextureCubeはディゾルブマスクとしてプレビューできません",
			   "TextureCube cannot be previewed as a dissolve mask"));
		 } else {
			ImGui::Text("%s: %s (%ux%u)", LocalizeEditorText("プレビュー", "Preview"), previewTexture->GetName().c_str(),
			   previewTexture->GetWidth(), previewTexture->GetHeight());

			constexpr float kPreviewMax = 128.0f;
			const float width = static_cast<float>(previewTexture->GetWidth());
			const float height = static_cast<float>(previewTexture->GetHeight());
			if (width > 0.0f && height > 0.0f) {
               // 長辺だけを上限へ合わせ、非正方形マスクも歪めずに確認できるようにする。
			   const float scale = (width > height) ? (kPreviewMax / width) : (kPreviewMax / height);
			   const ImVec2 previewSize(width * scale, height * scale);
			   const ImTextureID texId = static_cast<ImTextureID>(previewTexture->GetTextureSrvHandleGPU().ptr);
			   ImGui::Image(ImTextureRef(texId), previewSize);
			}
		 }
	  } else {
		 ImGui::TextDisabled("%s", LocalizeEditorText(
			"マスクテクスチャが見つかりません", "Mask texture not found"));
	  }

	  if (changed) {
		 SetParams(params);
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}
