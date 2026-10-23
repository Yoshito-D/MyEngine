#pragma once

#include <string>
#include <vector>
#include <climits>
#include <d3d12.h>
#include "GameEngine/Graphics/Pipeline/PipelineState.h" // BlendModeの定義を含む

namespace GameEngine {

/// @brief ルートパラメータ定義
struct RootParameterDefinition {
   D3D12_ROOT_PARAMETER_TYPE type = D3D12_ROOT_PARAMETER_TYPE_CBV; ///< 処理対象の種類を区別する識別値。
   /// @brief このリソースを参照できるシェーダーステージ。
   D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
   UINT shaderRegister = UINT_MAX;                                          ///< シェーダー側のレジスタ番号。
   UINT registerSpace = 0;                                                  ///< シェーダーレジスタの名前空間番号。
   UINT descriptorCount = 1;                                                // DESCRIPTOR_TABLE用
   D3D12_DESCRIPTOR_RANGE_TYPE rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // ディスクリプタテーブル用
   std::string semantic;  ///< JSONとC++の間でバインド先を解決する意味名。
   bool required = false; ///< シェーダーが実際に使用するリソースであることをリフレクション結果で示す。
};

/// @brief サンプラー定義
struct SamplerDefinition {
   D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;                 ///< テクスチャの拡大・縮小・ミップ補間方式。
   D3D12_TEXTURE_ADDRESS_MODE addressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; ///< U方向の範囲外テクスチャ座標の扱い。
   D3D12_TEXTURE_ADDRESS_MODE addressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; ///< V方向の範囲外テクスチャ座標の扱い。
   D3D12_TEXTURE_ADDRESS_MODE addressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; ///< W方向の範囲外テクスチャ座標の扱い。
   D3D12_COMPARISON_FUNC comparisonFunc = D3D12_COMPARISON_FUNC_NEVER;    ///< 比較サンプラーで使う比較関数。
   FLOAT maxLOD = D3D12_FLOAT32_MAX; ///< サンプリングで参照可能な最大ミップレベル。
   UINT shaderRegister = 0;          ///< シェーダー側のレジスタ番号。
   /// @brief このリソースを参照できるシェーダーステージ。
   D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_PIXEL;
};

/// @brief ルートシグネチャ定義
struct RootSignatureDefinition {
   std::string name;                                ///< 識別とエディター表示に使う名前。
   std::vector<RootParameterDefinition> parameters; ///< ルートシグネチャへ登録するパラメーターの定義順一覧。
   std::vector<SamplerDefinition> samplers;         ///< ルートシグネチャへ登録する静的サンプラー。
};

/// @brief 入力レイアウト要素定義
struct InputElementDefinition {
   std::string semanticName;                  ///< 頂点入力要素をシェーダーへ対応付けるセマンティクス名。
   UINT semanticIndex;                        ///< 同名セマンティクス内の要素番号。
   DXGI_FORMAT format;                        ///< リソースまたは頂点要素のデータ形式。
   UINT alignedByteOffset;                    ///< 入力要素の先頭からのバイトオフセット。
   UINT inputSlot;                            ///< 頂点入力に使うバッファスロット番号。
   D3D12_INPUT_CLASSIFICATION inputSlotClass; ///< 頂点ごとまたはインスタンスごとの入力方式。
   UINT instanceDataStepRate;                 ///< インスタンス入力の要素を切り替えるインスタンス数。
};

/// @brief HLSLのバイトオフセットを明示したfloatベクトルパラメーター。
struct MaterialParameterDefinition {
   std::string name;                ///< 識別とエディター表示に使う名前。
   UINT offset = 0;                 ///< 定数バッファ先頭からこのパラメーターまでのバイト数。
   std::vector<float> defaultValue; ///< 設定が指定されない場合に採用する既定値。
   bool checkbox = false; ///< 単一floatを0/1のチェックボックスで編集する。
};

/// @brief すべてのPSOバリエーションで共有する、検証済みモデル描画契約。
struct ModelPipelineDefinition {
   std::vector<RootParameterDefinition> bindings;       ///< モデル描画に必要なリソースのバインド契約。
   UINT parameterBufferSize = 0;                        ///< 独自マテリアル定数バッファのサイズ（バイト）。
   std::vector<MaterialParameterDefinition> parameters; ///< ルートシグネチャへ登録するパラメーターの定義順一覧。
};

/// @brief パイプライン定義構造体
struct PipelineDefinition {
   bool modelCompatible = false; ///< 使用前にモデルの頂点およびリソース契約を検証する。
   std::string name;             ///< 識別とエディター表示に使う名前。
   std::string vertexShader;     ///< 使用する頂点シェーダーの登録名。
   std::string pixelShader;      ///< 使用するピクセルシェーダーの登録名。
   std::string rootSignature;    ///< リソースのバインド配置を定めるルートシグネチャ。
   std::vector<InputElementDefinition> inputLayout;        ///< 頂点バッファとシェーダー入力の対応定義。
   ModelPipelineDefinition model;                          ///< モデル描画のバインドと独自マテリアルの契約定義。
   bool supportBlendModes = false;                         ///< ブレンド方式ごとのPSOを生成するかどうか。
   BlendMode defaultBlendMode = BlendMode::kBlendModeNone; ///< 明示指定がない場合に使う既定のブレンド方式。
   D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_BACK;        ///< 描画から除外する面の向き。
   D3D12_FILL_MODE fillMode = D3D12_FILL_MODE_SOLID;       ///< ポリゴンを塗りつぶすかワイヤー表示するかの設定。
   BOOL depthEnable = TRUE;                                ///< 深度テストを有効にするかどうか。
   D3D12_DEPTH_WRITE_MASK depthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; ///< 深度バッファへ書き込むかどうかの設定。
   D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; ///< 深度テストで使う比較方法。
   /// @brief 頂点を点・線・三角形のどれとして組み立てるかの分類。
   D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

   // 中間HDRターゲットと最終LDRターゲットを同一レジストリで安全に使い分けるための任意上書き。
   DXGI_FORMAT rtvFormatOverride = DXGI_FORMAT_UNKNOWN; ///< 描画先形式の上書き。UNKNOWNなら既定形式を使う。
   std::string computeShader;                           ///< 使用するComputeシェーダーの登録名。
};

/// @brief パイプラインレジストリ定義
struct PipelineRegistryDefinition {
   /// @brief 名前ごとに登録するルートシグネチャの定義または所有実体。
   std::vector<RootSignatureDefinition> rootSignatures;
   std::vector<PipelineDefinition> pipelines; ///< 登録対象の描画・Computeパイプライン定義。
};
}
