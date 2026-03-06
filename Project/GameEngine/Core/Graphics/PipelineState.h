#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <d3dcompiler.h>
#include <vector>
#include <string>
#include <memory>
#include "RootSignature.h"

using namespace Microsoft::WRL;


/// @brief ブレンドモードを定義する列挙型
enum class BlendMode {
   kBlendModeNone,// ブレンドなし
   kBlendModeNormal,// 通常ブレンド
   kBlendModeAdd,// 加算ブレンド
   kBlendModeSubtract,// 減算ブレンド
   kBlendModeMultiply,// 乗算ブレンド
   kBlendModeScreen,// スクリーンブレンド
   kCount// カウント
};

namespace GameEngine {
/// @brief パイプラインステートクラス
class PipelineState {
public:
   /// @brief パイプラインステートを作成する
   /// @param device デバイス
   void CreatePipelineState(ID3D12Device* device);

   /// @brief ルートシグネチャを設定する
   /// @param rootSignature ルートシグネチャ
   void SetRootSignature(RootSignature* rootSignature);

   /// @brief 入力レイアウトを設定する
   /// @param semanticName セマンティック名
   /// @param semanticIndex セマンティックインデックス
   /// @param format フォーマット
   /// @param alignedByteOffset アライメントされたバイトオフセット
   /// @param inputSlot 入力スロット
   /// @param inputSlotClass 入力スロットクラス
   /// @param instanceDataStepRate インスタンスデータステップレート
   void SetInputLayOut(
	  const char* semanticName,
	  UINT semanticIndex,
	  DXGI_FORMAT format,
	  UINT alignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
	  UINT inputSlot = 0,
	  D3D12_INPUT_CLASSIFICATION inputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
	  UINT instanceDataStepRate = 0
   );

   /// @brief ブレンドステートを設定する
   /// @param blendMode ブレンドモード
   void SetBlendState(BlendMode blendMode = BlendMode::kBlendModeNormal);

   /// @brief ラスタライザーステートを設定する
   /// @param cullMode カリングモード
   /// @param fillMode フィルモード
   /// @param frontCounterClockwise 前面カウンタークロックワイズ
   /// @param depthBias デプスバイアス
   /// @param depthBiasClamp デプスバイアスクランプ
   /// @param slopeScaledDepthBias スロープスケールドデプスバイアス
   /// @param depthClipEnable デプスクリップ有効
   /// @param multiSampleEnable マルチサンプル有効
   /// @param antialiasedLineEnable アンチエイリアスライン有効
   /// @param forcedSampleCount 強制サンプル数
   /// @param conservativeRasterizationMode 保守的ラスタライズモード
   void SetRasterizer(
	  D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_BACK,
	  D3D12_FILL_MODE fillMode = D3D12_FILL_MODE_SOLID,
	  BOOL frontCounterClockwise = FALSE,
	  INT depthBias = D3D12_DEFAULT_DEPTH_BIAS,
	  FLOAT depthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
	  FLOAT slopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
	  BOOL depthClipEnable = TRUE,
	  BOOL multisampleEnable = FALSE,
	  BOOL antialiasedLineEnable = FALSE,
	  UINT forcedSampleCount = 0,
	  D3D12_CONSERVATIVE_RASTERIZATION_MODE conservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
   );

   /// @brief 深度ステンシルステートを設定する
   /// @param depthEnable 深度テストを有効にするか
   /// @param writeMask 深度書き込みマスク
   /// @param depthFunc 深度比較関数
   void SetDepthStencil(BOOL depthEnable = TRUE, D3D12_DEPTH_WRITE_MASK writeMask = D3D12_DEPTH_WRITE_MASK_ALL, D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL);

   /// @brief シェーダーを設定する
   /// @param vertexShader 頂点シェーダー
   /// @param pixelShader ピクセルシェーダー
   void SetShaders(IDxcBlob* vertexShader, IDxcBlob* pixelShader);

   /// @brief レンダーターゲットビューのフォーマットを設定する
   /// @param format フォーマット
   void SetRTVFormat(DXGI_FORMAT format);

   /// @brief プリミティブトポロジタイプを設定する
   /// @param topologyType プリミティブトポロジタイプ
   void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType);

   /// @brief ルートシグネチャを取得する
   /// @return ルートシグネチャ
   ID3D12RootSignature* GetRootSignature() const { return rootSignature_->GetRootSignature(); }

   /// @brief パイプラインステートを取得する
   /// @return パイプラインステート
   ID3D12PipelineState* GetPipelineState() const { return graphicsPipelineState_.Get(); }
private:
   ComPtr<ID3D12PipelineState> graphicsPipelineState_ = nullptr;
   ComPtr<IDxcBlob> vertexShaderBlob_ = nullptr;
   ComPtr<IDxcBlob> pixelShaderBlob_ = nullptr;
   RootSignature* rootSignature_ = nullptr;
   std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs_{};
   D3D12_BLEND_DESC blendDesc_{};
   D3D12_RASTERIZER_DESC rasterizerDesc_{};
   D3D12_DEPTH_STENCIL_DESC depthStencilDesc_{};
   DXGI_FORMAT rtvFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
   D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
};
}