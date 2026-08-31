#include "pch.h"
#include "PostProcess.h"
#ifdef USE_IMGUI
#include "Utility/ImGuiHelper.h"
#endif
#include <nlohmann/json.hpp>

namespace GameEngine {

/// @brief 派生エフェクトが共通利用する描画環境を関連付ける。
/// @details 引数の所有権は移動せず、ポストプロセスの生存中は呼び出し元が両オブジェクトを
///          有効に保つ必要がある。派生クラスは追加リソースを作成する前に本実装を呼び出す。
/// @param device GPUリソースやコマンドリストへアクセスするグラフィックスデバイス
/// @param renderTarget エフェクトの出力先として共有するオフスクリーンレンダーターゲット
void PostProcess::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   device_ = device;
   renderTarget_ = renderTarget;
}

/// @brief エフェクト適用時にバインドするパイプライン一式を設定する。
/// @details パイプライン構築と所有はPSO管理側へ集約されているため、本クラスは非所有ポインタのみを保持する。
/// @param pipeline 描画に使用するパイプラインステート
/// @param rootSignature パイプラインと対応するルートシグネチャ
void PostProcess::SetPipeline(PipelineState* pipeline, RootSignature* rootSignature) {
   pipeline_ = pipeline;
   rootSignature_ = rootSignature;
}

/// @brief 定数バッファと入力カラーSRVのルートパラメータ番号を設定する。
/// @details PSO定義の意味名から解決済みの番号を受け取ることで、派生エフェクトを個々の
///          ルートシグネチャの物理的な並び順から独立させる。
/// @param constantBufferSlot エフェクト固有定数をバインドするルートパラメータ番号
/// @param inputTextureSlot 前段の描画結果をバインドするルートパラメータ番号
void PostProcess::SetBindingSlots(UINT constantBufferSlot, UINT inputTextureSlot) {
   constantBufferRootSlot_ = constantBufferSlot;
   inputTextureRootSlot_ = inputTextureSlot;
}

/// @brief 深度バッファSRVのルートパラメータ番号を設定する。
/// @details 深度を参照するエフェクトだけが利用する任意バインディングであり、番号はPSO定義から解決される。
/// @param depthTextureSlot 深度SRVをバインドするルートパラメータ番号
void PostProcess::SetDepthTextureRootSlot(UINT depthTextureSlot) {
   depthTextureRootSlot_ = depthTextureSlot;
}

/// @brief マスクテクスチャSRVのルートパラメータ番号を設定する。
/// @details マスクを参照するエフェクトだけが利用する任意バインディングであり、番号はPSO定義から解決される。
/// @param maskTextureSlot マスクSRVをバインドするルートパラメータ番号
void PostProcess::SetMaskTextureRootSlot(UINT maskTextureSlot) {
   maskTextureRootSlot_ = maskTextureSlot;
}

/// @brief 状態を持たないエフェクト向けの既定設定を生成する。
/// @details 空のオブジェクトを返すことで、管理側は派生クラスが固有設定を持つかどうかに関係なく
///          同一のJSON形式で設定を保存できる。
/// @return 空のJSONオブジェクト
nlohmann::json PostProcess::SerializeSettings() const {
   return nlohmann::json::object();
}

/// @brief 状態を持たないエフェクト向けに設定コンテナの形式を検証する。
/// @details 個別フィールドは持たないが、配列やプリミティブ値を受理しないことで派生クラスと
///          共通の「設定はJSONオブジェクト」という契約を維持する。
/// @param settings 復元対象の設定JSON
/// @return JSONオブジェクトならtrue、それ以外ならfalse
bool PostProcess::DeserializeSettings(const nlohmann::json& settings) {
   return settings.is_object();
}

#ifdef USE_IMGUI
/// @brief エディターの現在言語に対応するラベルを選択する。
/// @param japanese 日本語表示時に使用する文字列
/// @param english 英語表示時に使用する文字列
/// @return 現在の言語設定に対応する入力文字列
const char* PostProcess::LocalizeEditorText(const char* japanese, const char* english) {
   return ImGuiHelper::Localize({ japanese, english });
}
#endif

}
